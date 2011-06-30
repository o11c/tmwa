#include "magic-stmt.hpp"

#include "../common/timer.hpp"
#include "../common/utils.hpp"

#include "battle.hpp"
#include "clif.hpp"
#include "map.hpp"
#include "mob.hpp"
#include "npc.hpp"
#include "pc.hpp"
#include "script.hpp"
#include "skill.hpp"

#include "magic.structs.hpp"
#include "magic-base.hpp"
#include "magic-expr.hpp"

/// Used for local spell effects
#define INVISIBLE_NPC 127

static int heading_x[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
static int heading_y[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

static void clear_activation_record(cont_activation_record_t *ar)
{
    switch (ar->ty)
    {
    case ContStackType::FOREACH:
        free(ar->c_foreach.entities);
        break;
    case ContStackType::PROC:
        free(ar->c_proc.old_actuals);
        break;
    }
}

static void invocation_timer_callback(timer_id, tick_t, uint32_t id)
{
    invocation_t *invocation = static_cast<invocation_t *>(map_id2bl(id));

    if (invocation)
    {
        invocation->timer = 0;
        spell_execute(invocation);
    }
}

static void clear_stack(invocation_t *invocation)
{
    for (int i = 0; i < invocation->stack_size; i++)
        clear_activation_record(&invocation->stack[i]);

    invocation->stack_size = 0;
}

void spell_free_invocation(invocation_t *invocation)
{
    if (invocation->status_change_refs)
    {
        free(invocation->status_change_refs);
        // The following cleanup shouldn't be necessary,
        // but Fate added it to help tracking a certain bug
        // in commit aa760452
        invocation->status_change_refs = NULL;
        invocation->status_change_refs_nr = 0;
    }

    if (invocation->flags & InvocationFlag::BOUND)
    {
        BlockList *e = map_id2bl(invocation->subject);
        if (e && e->type == BL_PC)
            spell_unbind(static_cast<MapSessionData *>(e), invocation);
    }

    clear_stack(invocation);

    if (invocation->timer)
        delete_timer(invocation->timer);

    magic_free_env(invocation->env);

    map_delblock(invocation);
    // also frees the object
    map_delobject(invocation->id, BL_SPELL);
}

static void char_set_weapon_icon(MapSessionData *subject, int count, int icon, int look)
{
    const int old_icon = subject->attack_spell_icon_override;

    subject->attack_spell_icon_override = icon;
    subject->attack_spell_look_override = look;

    if (old_icon && old_icon != icon)
        clif_status_change(subject, old_icon, 0);

    clif_fixpcpos(subject);
    if (count)
    {
        clif_changelook(subject, LOOK_WEAPON, look);
        if (icon)
            clif_status_change(subject, icon, 1);
    }
    else
    {
        // Set it to `normal'
        clif_changelook(subject, LOOK_WEAPON, subject->status.weapon);
    }
}

static void char_set_attack_info(MapSessionData *subject, int speed, int range)
{
    subject->attack_spell_delay = speed;
    subject->attack_spell_range = range;

    if (speed == 0)
    {
        pc_calcstatus(subject, 1);
        clif_updatestatus(subject, SP_ASPD);
        clif_updatestatus(subject, SP_ATTACKRANGE);
    }
    else
    {
        subject->aspd = speed;
        clif_updatestatus(subject, SP_ASPD);
        clif_updatestatus(subject, SP_ATTACKRANGE);
    }
}

// on player death
void magic_stop_completely(MapSessionData *c)
{
    // Zap all status change references to spells
    for (int i = 0; i < MAX_STATUSCHANGE; i++)
        c->sc_data[i].spell_invocation = 0;

    while (c->active_spells)
        spell_free_invocation(c->active_spells);

    if (c->attack_spell_override)
    {
        invocation_t *attack_spell = static_cast<invocation_t *>(map_id2bl(c->attack_spell_override));
        if (attack_spell)
            spell_free_invocation(attack_spell);
        c->attack_spell_override = 0;
        char_set_weapon_icon(c, 0, 0, 0);
        char_set_attack_info(c, 0, 0);
    }
}

// Spell execution has finished normally or we have been notified by a finished skill timer
static void try_to_finish_invocation(invocation_t *invocation)
{
    if (invocation->status_change_refs_nr == 0 && !invocation->current_effect)
    {
        if (invocation->end_effect)
        {
            clear_stack(invocation);
            invocation->current_effect = invocation->end_effect;
            invocation->end_effect = NULL;
            spell_execute(invocation);
        }
        else
            spell_free_invocation(invocation);
    }
}

static int trigger_spell(int subject, int spell)
{
    invocation_t *invocation = static_cast<invocation_t *>(map_id2bl(spell));

    if (!invocation)
        return 0;

    invocation = spell_clone_effect(invocation);

    spell_bind(static_cast<MapSessionData *>(map_id2bl(subject)), invocation);
    magic_clear_var(&invocation->env->vars[Var::CASTER]);
    invocation->env->vars[Var::CASTER].ty = TY::ENTITY;
    invocation->env->vars[Var::CASTER].v_int = subject;

    return invocation->id;
}

static void entity_warp(BlockList *target, int destm, int destx, int desty);

static void char_update(MapSessionData *character)
{
    entity_warp(character, character->m, character->x, character->y);
}

static void timer_callback_effect(timer_id, tick_t, uint32_t id, int data)
{
    BlockList *target = map_id2bl(id);
    if (target)
        clif_misceffect(target, data);
}

static void entity_effect(BlockList *entity, int effect_nr, int delay)
{
    add_timer(gettick() + delay, timer_callback_effect, entity->id, effect_nr);
}

void magic_unshroud(MapSessionData *other_char)
{
    other_char->state.shroud_active = 0;
    // Now warp the caster out of and back into here to refresh everyone's display
    char_update(other_char);
    clif_displaymessage(other_char->fd, "Your shroud has been dispelled!");
//        entity_effect(other_char, MAGIC_EffectType::REVEAL);
}

static void timer_callback_effect_npc_delete(timer_id, tick_t, uint32_t npc_id)
{
    delete map_id2bl(npc_id);
}

static struct npc_data *local_spell_effect(int m, int x, int y, int effect, int tdelay)
{
    // 1 minute should be enough for all interesting spell effects, I hope
    // unit = 2 ticks?
    int delay = 30000;
    // the client can't handle effects directly on a tile
    npc_data *effect_npc = npc_spawn_text(m, x, y, INVISIBLE_NPC, "", "?");
    uint32_t effect_npc_id = effect_npc->id;

    entity_effect(effect_npc, effect, tdelay);
    add_timer(gettick() + delay, timer_callback_effect_npc_delete, effect_npc_id);

    return effect_npc;
}

static int op_sfx(env_t *, int, val_t *args)
{
    int delay = ARGINT(2);

    if (TY(0) == TY::ENTITY)
    {
        entity_effect(ARGENTITY(0), ARGINT(1), delay);
    }
    else if (TY(0) == TY::LOCATION)
    {
        local_spell_effect(ARGLOCATION(0).m,
                            ARGLOCATION(0).x,
                            ARGLOCATION(0).y, ARGINT(1), delay);
    }
    else
        return 1;

    return 0;
}

static int op_instaheal(env_t *env, int, val_t *args)
{
    BlockList *caster = (VAR(Var::CASTER).ty == TY::ENTITY)
        ? map_id2bl(VAR(Var::CASTER).v_int) : NULL;
    BlockList *subject = ARGENTITY(0);
    if (!caster)
        caster = subject;

    if (caster->type == BL_PC && subject->type == BL_PC)
    {
        MapSessionData *caster_pc = static_cast<MapSessionData *>(caster);
        MapSessionData *subject_pc = static_cast<MapSessionData *>(subject);
        MAP_LOG_PC(caster_pc, "SPELLHEAL-INSTA PC%d FOR %d",
                   subject_pc->status.char_id, ARGINT(1));
    }

    battle_heal(caster, subject, ARGINT(1), ARGINT(2));
    return 0;
}

static int op_itemheal(env_t *env, int args_nr, val_t *args)
{
    BlockList *subject = ARGENTITY(0);
    if (subject->type == BL_PC)
    {
        pc_itemheal(static_cast<MapSessionData *>(subject),
                     ARGINT(1), ARGINT(2));
    }
    else
        return op_instaheal(env, args_nr, args);

    return 0;
}

#define SHROUD_HIDE_NAME_TALKING_FLAG   (1 << 0)
#define SHROUD_DISAPPEAR_ON_PICKUP_FLAG (1 << 1)
#define SHROUD_DISAPPEAR_ON_TALK_FLAG   (1 << 2)

#define ARGCHAR(n) (ARGENTITY(n)->type == BL_PC ? ARGPC(n) : NULL)

static int op_shroud(env_t *, int, val_t *args)
{
    MapSessionData *subject = ARGCHAR(0);
    int arg = ARGINT(1);

    if (!subject)
        return 0;

    subject->state.shroud_active = 1;
    subject->state.shroud_hides_name_talking =
        (arg & SHROUD_HIDE_NAME_TALKING_FLAG) != 0;
    subject->state.shroud_disappears_on_pickup =
        (arg & SHROUD_DISAPPEAR_ON_PICKUP_FLAG) != 0;
    subject->state.shroud_disappears_on_talk =
        (arg & SHROUD_DISAPPEAR_ON_TALK_FLAG) != 0;
    return 0;
}

static int op_reveal(env_t *, int, val_t *args)
{
    MapSessionData *subject = ARGCHAR(0);

    if (subject && subject->state.shroud_active)
        magic_unshroud(subject);

    return 0;
}

static int op_message(env_t *, int, val_t *args)
{
    MapSessionData *subject = ARGCHAR(0);

    if (subject)
        clif_displaymessage(subject->fd, ARGSTR(1).c_str());

    return 0;
}

static void timer_callback_kill_npc(timer_id, tick_t, uint32_t npc_id)
{
    delete map_id2bl(npc_id);
}

static int op_messenger_npc(env_t *, int, val_t *args)
{
    struct npc_data *npc;
    location_t *loc = &ARGLOCATION(0);

    npc = npc_spawn_text(loc->m, loc->x, loc->y,
                          ARGINT(1), ARGSTR(2).c_str(), ARGSTR(3).c_str());

    add_timer(gettick() + ARGINT(4), timer_callback_kill_npc, npc->id);

    return 0;
}

static void entity_warp(BlockList *target, int destm, int destx, int desty)
{
    if (target->type == BL_PC || target->type == BL_MOB)
    {

        switch (target->type)
        {
            case BL_PC:
            {
                MapSessionData *character = static_cast<MapSessionData *>(target);
                clif_being_remove(character, BeingRemoveType::WARP);
                map_delblock(character);
                character->x = destx;
                character->y = desty;
                character->m = destm;

                pc_touch_all_relevant_npcs(character);

                // Note that touching NPCs may have triggered warping and thereby updated x and y:
                fixed_string<16>& map_name = maps[character->m].name;

                pc_setpos(character, Point{map_name, character->x, character->y}, BeingRemoveType::ZERO);
                break;
            }
            case BL_MOB:
                target->x = destx;
                target->y = desty;
                target->m = destm;
                clif_fixmobpos(static_cast<struct mob_data *>(target));
                break;
        }
    }
}

static int op_move(env_t *, int, val_t *args)
{
    BlockList *subject = ARGENTITY(0);
    Direction dir = ARGDIR(1);

    int newx = subject->x + heading_x[static_cast<int>(dir)];
    int newy = subject->y + heading_y[static_cast<int>(dir)];

    if (!map_is_solid(subject->m, newx, newy))
        entity_warp(subject, subject->m, newx, newy);

    return 0;
}

static int op_warp(env_t *, int, val_t *args)
{
    BlockList *subject = ARGENTITY(0);
    location_t *loc = &ARGLOCATION(1);

    entity_warp(subject, loc->m, loc->x, loc->y);

    return 0;
}

static int op_banish(env_t *, int, val_t *args)
{
    BlockList *subject = ARGENTITY(0);

    if (subject->type == BL_MOB)
    {
        struct mob_data *mob = static_cast<struct mob_data *>(subject);

        if (mob->mode & MOB_MODE_SUMMONED)
            mob_catch_delete(mob);
    }

    return 0;
}

static void record_status_change(invocation_t *invocation, int bl_id, int sc_id)
{
    int idx = invocation->status_change_refs_nr++;
    status_change_ref_t *cr;

    RECREATE(invocation->status_change_refs, status_change_ref_t, invocation->status_change_refs_nr);

    cr = &invocation->status_change_refs[idx];

    cr->sc_type = sc_id;
    cr->bl_id = bl_id;
}

static int op_status_change(env_t *env, int, val_t *args)
{
    BlockList *subject = ARGENTITY(0);
    int invocation_id = VAR(Var::INVOCATION).ty == TY::INVOCATION
        ? VAR(Var::INVOCATION).v_int : 0;
    invocation_t *invocation = static_cast<invocation_t *>(map_id2bl(invocation_id));

    skill_status_effect(subject, ARGINT(1), ARGINT(2),
                        /* ARGINT(3), ARGINT(4), ARGINT(5), */
                        ARGINT(6), invocation_id);

    if (invocation && subject->type == BL_PC)
        record_status_change(invocation, subject->id, ARGINT(1));

    return 0;
}

static int op_stop_status_change(env_t *, int, val_t *args)
{
    BlockList *subject = ARGENTITY(0);

    skill_status_change_end(subject, ARGINT(1), NULL);

    return 0;
}

static int op_override_attack(env_t *env, int, val_t *args)
{
    BlockList *psubject = ARGENTITY(0);
    int charges = ARGINT(1);
    int attack_delay = ARGINT(2);
    int attack_range = ARGINT(3);
    int icon = ARGINT(4);
    int look = ARGINT(5);
    int stopattack = ARGINT(6);
    MapSessionData *subject;

    if (psubject->type != BL_PC)
        return 0;

    subject = static_cast<MapSessionData *>(psubject);

    if (subject->attack_spell_override)
    {
        invocation_t *old_invocation =
                static_cast<invocation_t *>(map_id2bl(subject->attack_spell_override));
        if (old_invocation)
            spell_free_invocation(old_invocation);
    }

    subject->attack_spell_override =
        trigger_spell(subject->id, VAR(Var::INVOCATION).v_int);
    subject->attack_spell_charges = charges;

    if (subject->attack_spell_override)
    {
        invocation_t *attack_spell =
                static_cast<invocation_t *>(map_id2bl(subject->attack_spell_override));
        if (attack_spell && stopattack)
            attack_spell->flags |= InvocationFlag::STOPATTACK;

        char_set_weapon_icon(subject, charges, icon, look);
        char_set_attack_info(subject, attack_delay, attack_range);
    }

    return 0;
}

static int op_create_item(env_t *, int, val_t *args)
{
    struct item item;
    BlockList *entity = ARGENTITY(0);
    MapSessionData *subject;
    int stackable;
    int count = ARGINT(2);
    if (count <= 0)
        return 0;

    if (entity->type == BL_PC)
        subject = static_cast<MapSessionData *>(entity);
    else
        return 0;

    GET_ARG_ITEM(1, item, stackable);

    if (!stackable)
        while (count--)
            pc_additem(subject, &item, 1);
    else
        pc_additem(subject, &item, count);

    return 0;
}

#define AGGRAVATION_MODE_ATTACKS_CASTER(n)      ((n) == 0 || (n) == 2)
#define AGGRAVATION_MODE_MAKES_AGGRESSIVE(n)    ((n) > 0)

static int op_aggravate(env_t *, int, val_t *args)
{
    BlockList *victim = ARGENTITY(2);
    int mode = ARGINT(1);
    BlockList *target = ARGENTITY(0);
    struct mob_data *other;

    if (target->type != BL_MOB)
        return 0;

    other = static_cast<struct mob_data *>(target);

    mob_target(other, victim, battle_get_range(victim));

    if (AGGRAVATION_MODE_MAKES_AGGRESSIVE(mode))
        other->mode = 0x85 | (other->mode & MOB_SENSIBLE_MASK); /* war */

    if (AGGRAVATION_MODE_ATTACKS_CASTER(mode))
    {
        other->target_id = victim->id;
        other->attacked_id = victim->id;
    }

    return 0;
}

#define MONSTER_ATTITUDE_HOSTILE        0
#define MONSTER_ATTITUDE_FRIENDLY       1
#define MONSTER_ATTITUDE_SERVANT        2
#define MONSTER_ATTITUDE_FROZEN         3

static int op_spawn(env_t *, int, val_t *args)
{
    area_t *area = ARGAREA(0);
    BlockList *owner_e = ARGENTITY(1);
    int monster_id = ARGINT(2);
    int monster_attitude = ARGINT(3);
    int monster_count = ARGINT(4);
    int monster_lifetime = ARGINT(5);
    int i;

    MapSessionData *owner = NULL;
    if (monster_attitude == MONSTER_ATTITUDE_SERVANT && owner_e->type == BL_PC)
        owner = static_cast<MapSessionData *>(owner_e);

    for (i = 0; i < monster_count; i++)
    {
        location_t loc;
        magic_random_location(&loc, area);

        int mob_id;
        struct mob_data *mob;

        mob_id = mob_once_spawn(owner, maps[loc.m].name, loc.x, loc.y, "--ja--",    // Is that needed?
                                 monster_id, 1, "");

        mob = static_cast<struct mob_data *>(map_id2bl(mob_id));

        if (mob)
        {
            mob->mode = mob_db[monster_id].mode;

            switch (monster_attitude)
            {

                case MONSTER_ATTITUDE_SERVANT:
                    mob->state.special_mob_ai = 1;
                    mob->mode |= 0x04;
                    break;

                case MONSTER_ATTITUDE_FRIENDLY:
                    mob->mode = 0x80 | (mob->mode & 1);
                    break;

                case MONSTER_ATTITUDE_HOSTILE:
                    mob->mode = 0x84 | (mob->mode & 1);
                    if (owner)
                    {
                        mob->target_id = owner->id;
                        mob->attacked_id = owner->id;
                    }
                    break;

                case MONSTER_ATTITUDE_FROZEN:
                    mob->mode = 0;
                    break;
            }

            mob->mode |=
                MOB_MODE_SUMMONED | MOB_MODE_TURNS_AGAINST_BAD_MASTER;

            mob->deletetimer = add_timer(gettick() + monster_lifetime,
                                          mob_timer_delete, mob_id);

            if (owner)
            {
                mob->master_id = owner->id;
                mob->master_dist = 6;
            }
        }
    }

    return 0;
}

static POD_string get_invocation_name(env_t *env)
{
    if (VAR(Var::INVOCATION).ty != TY::INVOCATION)
    {
        POD_string out;
        out.init();
        out.assign("?");
        return out;
    }
    invocation_t *invocation = static_cast<invocation_t *>(map_id2bl(VAR(Var::INVOCATION).v_int));

    if (invocation)
        return invocation->spell->name.clone();
    POD_string out;
    out.init();
    out.assign("??");
    return out;
}

static int op_injure(env_t *env, int, val_t *args)
{
    BlockList *caster = ARGENTITY(0);
    BlockList *target = ARGENTITY(1);
    int damage_caused = ARGINT(2);
    int mp_damage = ARGINT(3);
    int target_hp = battle_get_hp(target);
    int mdef = battle_get_mdef(target);

    if (target->type == BL_PC
        && !maps[target->m].flag.pvp
        && !static_cast<MapSessionData *>(target)->special_state.killable
        && (caster->type != BL_PC
            || !static_cast<MapSessionData *>(caster)->special_state.killer))
        return 0;               /* Cannot damage other players outside of pvp */

    if (target != caster)
    {
        /* Not protected against own spells */
        damage_caused = (damage_caused * (100 - mdef)) / 100;
        mp_damage = (mp_damage * (100 - mdef)) / 100;
    }

    damage_caused = (damage_caused > target_hp) ? target_hp : damage_caused;

    if (damage_caused < 0)
        damage_caused = 0;

    // display damage first, because dealing damage may deallocate the target.
    clif_damage(caster, target, gettick(), 0, 0, damage_caused, 0, 0, 0);

    if (caster->type == BL_PC)
    {
        MapSessionData *caster_pc = static_cast<MapSessionData *>(caster);
        if (target->type == BL_MOB)
        {
            struct mob_data *mob = static_cast<struct mob_data *>(target);

            MAP_LOG_PC(caster_pc, "SPELLDMG MOB%d %d FOR %d BY %s",
                       mob->id, mob->mob_class, damage_caused,
                       get_invocation_name(env).c_str());
        }
    }
    battle_damage(caster, target, damage_caused);

    return 0;
}

static int op_emote(env_t *, int, val_t *args)
{
    BlockList *victim = ARGENTITY(0);
    int emotion = ARGINT(1);
    clif_emotion(victim, emotion);

    return 0;
}

static int op_set_script_variable(env_t *, int, val_t *args)
{
    MapSessionData *c = (ETY(0) == BL_PC) ? ARGPC(0) : NULL;

    if (!c)
        return 1;

    pc_setglobalreg(c, ARGSTR(1).c_str(), ARGINT(2));

    return 0;
}

static int op_set_hair_colour(env_t *, int, val_t *args)
{
    MapSessionData *c = (ETY(0) == BL_PC) ? ARGPC(0) : NULL;

    if (!c)
        return 1;

    pc_changelook(c, LOOK_HAIR_COLOR, ARGINT(1));

    return 0;
}

static int op_set_hair_style(env_t *, int, val_t *args)
{
    MapSessionData *c = (ETY(0) == BL_PC) ? ARGPC(0) : NULL;

    if (!c)
        return 1;

    pc_changelook(c, LOOK_HAIR, ARGINT(1));

    return 0;
}

static int op_drop_item_for(env_t *, int args_nr, val_t *args)
{
    struct item item;
    int stackable;
    location_t *loc = &ARGLOCATION(0);
    int count = ARGINT(2);
    int duration = ARGINT(3);
    MapSessionData *c = ((args_nr > 4) && (ETY(4) == BL_PC)) ? ARGPC(4) : NULL;
    int delay = (args_nr > 5) ? ARGINT(5) : 0;
    int delaytime[3] = { delay, delay, delay };
    MapSessionData *owners[3] = { c, NULL, NULL };

    GET_ARG_ITEM(1, item, stackable);

    if (stackable)
        map_addflooritem_any(&item, count, loc->m, loc->x, loc->y,
                              owners, delaytime, duration, 0);
    else
        while (count-- > 0)
            map_addflooritem_any(&item, 1, loc->m, loc->x, loc->y,
                                  owners, delaytime, duration, 0);

    return 0;
}

static int op_gain_exp(env_t *, int, val_t *args)
{
    MapSessionData *c = (ETY(0) == BL_PC) ? ARGPC(0) : NULL;

    if (!c)
        return 1;

    pc_gainexp_reason(c, ARGINT(1), ARGINT(2), ARGINT(3));
    return 0;
}

static op_t operations[] =
{
    {"sfx", ".ii", op_sfx},
    {"instaheal", "eii", op_instaheal},
    {"itemheal", "eii", op_itemheal},
    {"shroud", "ei", op_shroud},
    {"unshroud", "e", op_reveal},
    {"message", "es", op_message},
    {"messenger_npc", "lissi", op_messenger_npc},
    {"move", "ed", op_move},
    {"warp", "el", op_warp},
    {"banish", "e", op_banish},
    {"status_change", "eiiiiii", op_status_change},
    {"stop_status_change", "ei", op_stop_status_change},
    {"override_attack", "eiiiiii", op_override_attack},
    {"create_item", "e.i", op_create_item},
    {"aggravate", "eie", op_aggravate},
    {"spawn", "aeiiii", op_spawn},
    {"injure", "eeii", op_injure},
    {"emote", "ei", op_emote},
    {"set_script_variable", "esi", op_set_script_variable},
    {"set_hair_colour", "ei", op_set_hair_colour},
    {"set_hair_style", "ei", op_set_hair_style},
    {"drop_item", "l.ii", op_drop_item_for},
    {"drop_item_for", "l.iiei", op_drop_item_for},
    {"gain_experience", "eiii", op_gain_exp},
    {NULL, NULL, NULL}
};

static int operations_sorted = 0;
static int operation_count;

static int compare_operations(const void *lhs, const void *rhs)
{
    return strcmp(static_cast<const op_t *>(lhs)->name,
                  static_cast<const op_t *>(rhs)->name);
}

const op_t *magic_get_op(const char *name, int *idx)
{
    op_t key;

    if (!operations_sorted)
    {
        op_t *opc = operations;

        while (opc->name)
            ++opc;

        operation_count = opc - operations;

        qsort(operations, operation_count, sizeof(op_t), compare_operations);
        operations_sorted = 1;
    }

    key.name = name;
    op_t *op =
            static_cast<op_t *>(
                    bsearch(&key, operations, operation_count, sizeof(op_t),
                            compare_operations));

    if (op && idx)
        *idx = op - operations;

    return op;
}

void spell_effect_report_termination(int invocation_id, int bl_id, int sc_id, int)
{
    int i;
    int idx = -1;
    invocation_t *invocation = static_cast<invocation_t *>(map_id2bl(invocation_id));

    if (!invocation || invocation->type != BL_SPELL)
        return;

    for (i = 0; i < invocation->status_change_refs_nr; i++)
    {
        status_change_ref_t *cr = &invocation->status_change_refs[i];
        if (cr->sc_type == sc_id && cr->bl_id == bl_id)
        {
            idx = i;
            break;
        }
    }

    if (idx == -1)
    {
        BlockList *entity = map_id2bl(bl_id);
        if (entity->type == BL_PC)
            fprintf(stderr,
                     "[magic] INTERNAL ERROR: spell-effect-report-termination:  tried to terminate on unexpected bl %d, sc %d\n",
                     bl_id, sc_id);
        return;
    }

    if (idx == invocation->status_change_refs_nr - 1)
        invocation->status_change_refs_nr--;
    else                        /* Copy last change ref to the one we are deleting */
        invocation->status_change_refs[idx] =
            invocation->
            status_change_refs[--invocation->status_change_refs_nr];

    try_to_finish_invocation(invocation);
}

static effect_t *return_to_stack(invocation_t *invocation)
{
    if (!invocation->stack_size)
        return NULL;
    else
    {
        cont_activation_record_t *ar =
            invocation->stack + (invocation->stack_size - 1);
        switch (ar->ty)
        {

            case ContStackType::PROC:
            {
                effect_t *ret = ar->return_location;
                int i;

                for (i = 0; i < ar->c_proc.args_nr; i++)
                {
                    val_t *var =
                        &invocation->env->vars[ar->c_proc.formals[i]];
                    magic_clear_var(var);
                    *var = ar->c_proc.old_actuals[i];
                }

                clear_activation_record(ar);
                --invocation->stack_size;

                return ret;
            }

            case ContStackType::FOREACH:
            {
                int entity_id;
                val_t *var = &invocation->env->vars[ar->c_foreach.id];

                do
                {
                    if (ar->c_foreach.idx >= ar->c_foreach.entities_nr)
                    {
                        effect_t *ret = ar->return_location;
                        clear_activation_record(ar);
                        --invocation->stack_size;
                        return ret;
                    }

                    entity_id =
                        ar->c_foreach.entities[ar->c_foreach.idx++];
                }
                while (!entity_id || !map_id2bl(entity_id));

                magic_clear_var(var);
                var->ty = ar->c_foreach.ty;
                var->v_int = entity_id;

                return ar->c_foreach.body;
            }

            case ContStackType::FOR:
                if (ar->c_for.current > ar->c_for.stop)
                {
                    effect_t *ret = ar->return_location;
                    clear_activation_record(ar);
                    --invocation->stack_size;
                    return ret;
                }

                magic_clear_var(&invocation->env->vars[ar->c_for.id]);
                invocation->env->vars[ar->c_for.id].ty = TY::INT;
                invocation->env->vars[ar->c_for.id].v_int =
                    ar->c_for.current++;

                return ar->c_for.body;

            default:
                fprintf(stderr,
                        "[magic] INTERNAL ERROR: While executing spell `%s':  stack corruption\n",
                        invocation->spell->name.c_str());
                return NULL;
        }
    }
}

static cont_activation_record_t *add_stack_entry(invocation_t *invocation,
                                                  ContStackType ty,
                                                  effect_t *return_location)
{
    cont_activation_record_t *ar =
        invocation->stack + invocation->stack_size++;
    if (invocation->stack_size >= MAX_STACK_SIZE)
    {
        fprintf(stderr,
                "[magic] Execution stack size exceeded in spell `%s'; truncating effect\n",
                invocation->spell->name.c_str());
        invocation->stack_size--;
        return NULL;
    }

    ar->ty = ty;
    ar->return_location = return_location;
    return ar;
}

static void find_entities_in_area_c(BlockList *target,
                                    int *entities_allocd_p,
                                    int *entities_nr_p,
                                    int **entities_p,
                                    ForEach_FilterType filter)
{
/* The following macro adds an entity to the result list: */
#define ADD_ENTITY(e)                                                   \
        if (*entities_nr_p == *entities_allocd_p) {                     \
                /* Need more space */                                   \
                (*entities_allocd_p) += 32;                             \
                RECREATE(*entities_p, int, *entities_allocd_p); \
        }                                                               \
        (*entities_p)[(*entities_nr_p)++] = e;

    switch (target->type)
    {

        case BL_PC:
            if (filter == ForEach_FilterType::PC
                || filter == ForEach_FilterType::ENTITY
                || (filter == ForEach_FilterType::TARGET
                    && maps[target->m].flag.pvp))
                break;
            else if (filter == ForEach_FilterType::SPELL)
            {                   /* Check all spells bound to the caster */
                invocation_t *invoc = static_cast<MapSessionData *>(target)->active_spells;
                /* Add all spells locked onto thie PC */

                while (invoc)
                {
                    ADD_ENTITY(invoc->id);
                    invoc = invoc->next_invocation;
                }
            }
            return;

        case BL_MOB:
            if (filter == ForEach_FilterType::MOB
                || filter == ForEach_FilterType::ENTITY
                || filter == ForEach_FilterType::TARGET)
                break;
            else
                return;

        case BL_SPELL:
            if (filter == ForEach_FilterType::SPELL)
            {
                invocation_t *invocation = static_cast<invocation_t *>(target);

                /* Check whether the spell is `bound'-- if so, we'll consider it iff we see the caster (case BL_PC). */
                if (invocation->flags & InvocationFlag::BOUND)
                    return;
                else
                    break;      /* Add the spell */
            }
            else
                return;

        case BL_NPC:
            if (filter == ForEach_FilterType::NPC)
                break;
            else
                return;

        default:
            return;
    }

    ADD_ENTITY(target->id);
#undef ADD_ENTITY
}

static void find_entities_in_area(area_t *area, int *entities_allocd_p,
                       int *entities_nr_p, int **entities_p, ForEach_FilterType filter)
{
    switch (area->ty)
    {
        case AreaType::UNION:
            find_entities_in_area(area->a_union[0], entities_allocd_p,
                                   entities_nr_p, entities_p, filter);
            find_entities_in_area(area->a_union[1], entities_allocd_p,
                                   entities_nr_p, entities_p, filter);
            break;

        default:
        {
            int m, x, y, width, height;
            magic_area_rect(&m, &x, &y, &width, &height, area);
            map_foreachinarea(find_entities_in_area_c,
                              m, x, y, x + width, y + height,
                              BL_NUL /* filter elsewhere */ ,
                              entities_allocd_p, entities_nr_p, entities_p,
                              filter);
        }
    }
}

static effect_t *run_foreach(invocation_t *invocation, effect_t *foreach,
                              effect_t *return_location)
{
    val_t area;
    ForEach_FilterType filter = foreach->e_foreach.filter;
    int id = foreach->e_foreach.id;
    effect_t *body = foreach->e_foreach.body;

    magic_eval(invocation->env, &area, foreach->e_foreach.area);

    if (area.ty != TY::AREA)
    {
        magic_clear_var(&area);
        fprintf(stderr,
                "[magic] Error in spell `%s':  FOREACH loop over non-area\n",
                invocation->spell->name.c_str());
        return return_location;
    }
    else
    {
        cont_activation_record_t *ar =
            add_stack_entry(invocation, ContStackType::FOREACH, return_location);
        int entities_allocd = 64;
        int *entities_collect;
        int *entities;
        int *shuffle_board;
        int entities_nr = 0;
        int i;

        if (!ar)
            return return_location;

        CREATE(entities_collect, int, entities_allocd);

        find_entities_in_area(area.v_area, &entities_allocd, &entities_nr,
                               &entities_collect, filter);

        /* Now shuffle */
        CREATE(shuffle_board, int, entities_nr);
        CREATE(entities, int, entities_nr);
        for (i = 0; i < entities_nr; i++)
            shuffle_board[i] = i;

        for (i = entities_nr - 1; i >= 0; i--)
        {
            int random_index = rand() % (i + 1);
            entities[i] = entities_collect[shuffle_board[random_index]];
            shuffle_board[random_index] = shuffle_board[i]; // thus, we are guaranteed only to use unused indices
        }

        free(entities_collect);
        free(shuffle_board);
        /* Done shuffling */

        ar->c_foreach.id = id;
        ar->c_foreach.body = body;
        ar->c_foreach.idx = 0;
        ar->c_foreach.entities_nr = entities_nr;
        ar->c_foreach.entities = entities;
        ar->c_foreach.ty =
            (filter == ForEach_FilterType::SPELL) ? TY::INVOCATION : TY::ENTITY;

        magic_clear_var(&area);

        return return_to_stack(invocation);
    }
}

static effect_t *run_for(invocation_t *invocation, effect_t *for_,
                          effect_t *return_location)
{
    cont_activation_record_t *ar;
    int id = for_->e_for.id;
    val_t start;
    val_t stop;

    magic_eval(invocation->env, &start, for_->e_for.start);
    magic_eval(invocation->env, &stop, for_->e_for.stop);

    if (start.ty != TY::INT || stop.ty != TY::INT)
    {
        magic_clear_var(&start);
        magic_clear_var(&stop);
        fprintf(stderr,
                "[magic] Error in spell `%s':  FOR loop start or stop point is not an integer\n",
                invocation->spell->name.c_str());
        return return_location;
    }

    ar = add_stack_entry(invocation, ContStackType::FOR, return_location);

    if (!ar)
        return return_location;

    ar->c_for.id = id;
    ar->c_for.current = start.v_int;
    ar->c_for.stop = stop.v_int;
    ar->c_for.body = for_->e_for.body;

    return return_to_stack(invocation);
}

static effect_t *run_call(invocation_t *invocation,
                           effect_t *return_location)
{
    effect_t *current = invocation->current_effect;
    cont_activation_record_t *ar;
    int args_nr = current->e_call.args_nr;
    int *formals = current->e_call.formals;
    val_t *old_actuals;
    CREATE(old_actuals, val_t, args_nr);
    int i;

    ar = add_stack_entry(invocation, ContStackType::PROC, return_location);
    ar->c_proc.args_nr = args_nr;
    ar->c_proc.formals = formals;
    ar->c_proc.old_actuals = old_actuals;
    for (i = 0; i < args_nr; i++)
    {
        val_t *env_val = &invocation->env->vars[formals[i]];
        val_t result;
        magic_copy_var(&old_actuals[i], env_val);
        magic_eval(invocation->env, &result, current->e_call.actuals[i]);
        *env_val = result;
    }

    return current->e_call.body;
}

#ifdef DEBUG
static void print_cfg(int i, effect_t *e)
{
    int j;
    for (j = 0; j < i; j++)
        printf("    ");

    printf("%p: ", e);

    if (!e)
    {
        puts(" -- end --");
        return;
    }

    switch (e->ty)
    {
        case EffectType::SKIP:
            puts("SKIP");
            break;
        case EffectType::END:
            puts("END");
            break;
        case EffectType::ABORT:
            puts("ABORT");
            break;
        case EffectType::ASSIGN:
            puts("ASSIGN");
            break;
        case EffectType::FOREACH:
            puts("FOREACH");
            print_cfg(i + 1, e->e_foreach.body);
            break;
        case EffectType::FOR:
            puts("FOR");
            print_cfg(i + 1, e->e_for.body);
            break;
        case EffectType::IF:
            puts("IF");
            for (j = 0; j < i; j++)
                printf("    ");
            puts("THEN");
            print_cfg(i + 1, e->e_if.true_branch);
            for (j = 0; j < i; j++)
                printf("    ");
            puts("ELSE");
            print_cfg(i + 1, e->e_if.false_branch);
            break;
        case EffectType::SLEEP:
            puts("SLEEP");
            break;
        case EffectType::SCRIPT:
            puts("SCRIPT");
            break;
        case EffectType::BREAK:
            puts("BREAK");
            break;
        case EffectType::OP:
            puts("OP");
            break;
    }
    print_cfg(i, e->next);
}
#endif

/**
 * Execute a spell invocation until we abort, finish, or hit the next `sleep'.
 *
 * Use spell_execute() to automate handling of timers
 *
 * Returns: 0 if finished(all memory is freed implicitly)
 *          >1 if we hit `sleep'; the result is the number of ticks we should sleep for.
 *          -1 if we paused to wait for a user action(via script interaction)
 */
static int spell_run(invocation_t *invocation, int allow_delete)
{
    const int invocation_id = invocation->id;
#define REFRESH_INVOCATION() do {invocation = static_cast<invocation_t *>(map_id2bl(invocation_id)); if (!invocation) return 0;} while(0)

#ifdef DEBUG
    fprintf(stderr, "Resuming execution:  invocation of `%s'\n",
             invocation->spell->name);
    print_cfg(1, invocation->current_effect);
#endif
    while (invocation->current_effect)
    {
        effect_t *e = invocation->current_effect;
        effect_t *next = e->next;
        int i;

#ifdef DEBUG
        fprintf(stderr, "Next step of type %d\n", e->ty);
        dump_env(invocation->env);
#endif

        switch (e->ty)
        {
            case EffectType::SKIP:
                break;

            case EffectType::ABORT:
                invocation->flags |= InvocationFlag::ABORTED;
                invocation->end_effect = NULL;
            case EffectType::END:
                clear_stack(invocation);
                next = NULL;
                break;

            case EffectType::ASSIGN:
                magic_eval(invocation->env,
                            &invocation->env->vars[e->e_assign.id],
                            e->e_assign.expr);
                break;

            case EffectType::FOREACH:
                next = run_foreach(invocation, e, next);
                break;

            case EffectType::FOR:
                next = run_for(invocation, e, next);
                break;

            case EffectType::IF:
                if (magic_eval_int(invocation->env, e->e_if.cond))
                    next = e->e_if.true_branch;
                else
                    next = e->e_if.false_branch;
                break;

            case EffectType::SLEEP:
            {
                int sleeptime =
                    magic_eval_int(invocation->env, e->e_sleep);
                invocation->current_effect = next;
                if (sleeptime > 0)
                    return sleeptime;
                break;
            }

            case EffectType::SCRIPT:
            {
                MapSessionData *caster = static_cast<MapSessionData *>(map_id2bl(invocation->caster));
                if (caster)
                {
                    env_t *env = invocation->env;
                    argrec_t arg[] =
                    {
                        {
                            "@target",
                            v: { i: VAR(Var::TARGET).ty ==TY::ENTITY ? 0 : VAR(Var::TARGET).v_int }
                        },
                        {
                            "@caster",
                            v: { i: invocation->caster }
                        },
                        {
                            "@caster_name$",
                            v: { s: caster ? caster->status.name : "" }
                        }
                    };
                    int message_recipient =
                        VAR(Var::SCRIPTTARGET).ty ==
                        TY::ENTITY ? VAR(Var::SCRIPTTARGET).v_int : invocation->caster;
                    MapSessionData *recipient = static_cast<MapSessionData *>(map_id2bl(message_recipient));

                    if (recipient->npc_id
                        && recipient->npc_id != invocation->id)
                        break;  /* Don't send multiple message boxes at once */

                    if (!invocation->script_pos)    // first time running this script?
                        clif_spawn_fake_npc_for_player(recipient,
                                                        invocation->id);
                    // We have to do this or otherwise the client won't think that it's
                    // dealing with an NPC

                    int newpos = run_script_l(e->e_script,
                                                invocation->script_pos,
                                                message_recipient,
                                                invocation->id,
                                                3, arg);
                    /* Returns the new script position, or -1 once the script is finished */
                    if (newpos != -1)
                    {
                        /* Must set up for continuation */
                        recipient->npc_id = invocation->id;
                        recipient->npc_pos = invocation->script_pos = newpos;
                        return -1;  /* Signal `wait for script' */
                    }
                    else
                        invocation->script_pos = 0;
                    clif_being_remove_id(invocation->id, BeingRemoveType::DEAD, caster->fd);
                }
                REFRESH_INVOCATION(); // Script may have killed the caster
                break;
            }

            case EffectType::BREAK:
                next = return_to_stack(invocation);
                break;

            case EffectType::OP:
            {
                op_t *op = &operations[e->e_op.id];
                val_t args[MAX_ARGS];

                for (i = 0; i < e->e_op.args_nr; i++)
                    magic_eval(invocation->env, &args[i], e->e_op.args[i]);

                if (!magic_signature_check("effect", op->name, op->signature,
                                            e->e_op.args_nr, args,
                                            e->e_op.line_nr,
                                            e->e_op.column))
                    op->op(invocation->env, e->e_op.args_nr, args);

                for (i = 0; i < e->e_op.args_nr; i++)
                    magic_clear_var(&args[i]);

                REFRESH_INVOCATION(); // EffectType may have killed the caster
                break;
            }

            case EffectType::CALL:
                next = run_call(invocation, next);
                break;

            default:
                fprintf(stderr, "[magic] INTERNAL ERROR: Unknown effect %d\n",
                        static_cast<int>(e->ty));
        }

        if (!next)
            next = return_to_stack(invocation);

        invocation->current_effect = next;
    }

    if (allow_delete)
        try_to_finish_invocation(invocation);
    return 0;
#undef REFRESH_INVOCATION
}

static void spell_execute_d(invocation_t *invocation, int allow_deletion)
{
    spell_update_location(invocation);
    int delta = spell_run(invocation, allow_deletion);

    if (delta > 0)
    {
        if (invocation->timer)
        {
            fprintf(stderr,
                    "[magic] FATAL ERROR: Trying to add multiple timers to the same spell! Already had timer: %p\n",
                    invocation->timer);
            abort();
        }
        invocation->timer = add_timer(gettick() + delta, invocation_timer_callback, invocation->id);
    }

    /* If 0, the script cleaned itself.  If -1 (wait-for-script), we must wait for the user. */
}

void spell_execute(invocation_t *invocation)
{
    spell_execute_d(invocation, 1);
}

void spell_execute_script(invocation_t *invocation)
{
    if (invocation->script_pos)
        spell_execute_d(invocation, 1);
    /* Otherwise the script-within-the-spell has been terminated by some other means.
     * In practice this happens when the script doesn't wait for user input: the client
     * may still notify the server that it's done.  Without the above check, we'd be
     * running the same spell twice! */
}

int spell_attack(int caster_id, int target_id)
{
    MapSessionData *caster = static_cast<MapSessionData *>(map_id2bl(caster_id));
    invocation_t *invocation;
    int stop_attack = 0;

    if (!caster)
        return 0;

    invocation = static_cast<invocation_t *>(map_id2bl(caster->attack_spell_override));

    if (invocation && invocation->flags & InvocationFlag::STOPATTACK)
        stop_attack = 1;

    if (invocation && caster->attack_spell_charges > 0)
    {
        magic_clear_var(&invocation->env->vars[Var::TARGET]);
        invocation->env->vars[Var::TARGET].ty = TY::ENTITY;
        invocation->env->vars[Var::TARGET].v_int = target_id;

        invocation->current_effect = invocation->trigger_effect;
        invocation->flags &= ~InvocationFlag::ABORTED;
        spell_execute_d(invocation,
                         0 /* don't delete the invocation if done */ );

        // If the caster died, we need to refresh here:
        invocation = static_cast<invocation_t *>(map_id2bl(caster->attack_spell_override));

        if (invocation && !(invocation->flags & InvocationFlag::ABORTED))   // If we didn't abort:
            caster->attack_spell_charges--;
    }

    if (invocation && caster->attack_spell_override != invocation->id)
    {
        /* Attack spell changed / was refreshed */
        // spell_free_invocation(invocation); // [Fate] This would be a double free.
    }
    else if (!invocation || caster->attack_spell_charges <= 0)
    {
        caster->attack_spell_override = 0;
        char_set_weapon_icon(caster, 0, 0, 0);
        char_set_attack_info(caster, 0, 0);

        if (stop_attack)
            pc_stopattack(caster);

        if (invocation)
            spell_free_invocation(invocation);
    }

    return 1;
}
