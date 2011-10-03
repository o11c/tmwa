#include "magic-stmt.hpp"

#include <algorithm>

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

static const int32_t heading_x[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
static const int32_t heading_y[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

static void invocation_timer_callback(timer_id, tick_t, uint32_t id)
{
    invocation_t *invocation = static_cast<invocation_t *>(map_id2bl(id));

    if (invocation)
    {
        invocation->timer = 0;
        spell_execute(invocation);
    }
}

// can we change this to ~invocation_t()?
// The problem is that there's that funny map_del* stuff
// Why do I say "we" when I'm the only committer?
void spell_free_invocation(invocation_t *invocation)
{
    invocation->status_change_refs.clear();

    if (invocation->subject)
    {
        BlockList *e = map_id2bl(invocation->subject);
        if (e && e->type == BL_PC)
            spell_unbind(static_cast<MapSessionData *>(e), invocation);
    }

    invocation->stack.clear();

    if (invocation->timer)
        delete_timer(invocation->timer);

    delete invocation->env;

    map_delblock(invocation);
    // also frees the object
    map_delobject(invocation->id, BL_SPELL);
}

static void char_set_weapon_icon(MapSessionData *subject, int32_t count, int32_t icon, int32_t look)
{
    const int32_t old_icon = subject->attack_spell_icon_override;

    subject->attack_spell_icon_override = icon;
    subject->attack_spell_look_override = look;

    if (old_icon && old_icon != icon)
        clif_status_change(subject, old_icon, 0);

    clif_fixpcpos(subject);
    if (count)
    {
        clif_changelook(subject, LOOK::WEAPON, look);
        if (icon)
            clif_status_change(subject, icon, 1);
    }
    else
    {
        // Set it to `normal'
        clif_changelook(subject, LOOK::WEAPON, subject->status.weapon);
    }
}

static void char_set_attack_info(MapSessionData *subject, int32_t speed, int32_t range)
{
    subject->attack_spell_delay = speed;
    subject->attack_spell_range = range;

    if (speed == 0)
    {
        pc_calcstatus(subject, 1);
        clif_updatestatus(subject, SP::ASPD);
        clif_updatestatus(subject, SP::ATTACKRANGE);
    }
    else
    {
        subject->aspd = speed;
        clif_updatestatus(subject, SP::ASPD);
        clif_updatestatus(subject, SP::ATTACKRANGE);
    }
}

// on player death
void magic_stop_completely(MapSessionData *c)
{
    // Zap all status change references to spells
    for (int32_t i = 0; i < MAX_STATUSCHANGE; i++)
        c->sc_data[i].spell_invocation = 0;

    while (!c->active_spells.empty())
        spell_free_invocation(*c->active_spells.begin());

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
    if (!invocation->status_change_refs.empty())
        return;
    if (invocation->current_effect)
        return;

    // if nothing is happening or can happen, just free it
    if (!invocation->end_effect)
    {
        spell_free_invocation(invocation);
        return;
    }

    // if nothing is still happening, replace with end effect
    invocation->stack.clear();
    invocation->current_effect = invocation->end_effect;
    invocation->end_effect = NULL;
    spell_execute(invocation);
}

static int32_t trigger_spell(int32_t subject, int32_t spell)
{
    invocation_t *invocation = static_cast<invocation_t *>(map_id2bl(spell));

    if (!invocation)
        return 0;

    invocation = new invocation_t(invocation);

    spell_bind(map_id2sd(subject), invocation);
    magic_clear_var(&invocation->env->vars[Var::CASTER]);
    invocation->env->vars[Var::CASTER].ty = TY::ENTITY;
    invocation->env->vars[Var::CASTER].v_int = subject;

    return invocation->id;
}

static void timer_callback_effect(timer_id, tick_t, uint32_t id, int32_t effect_nr)
{
    BlockList *target = map_id2bl(id);
    if (target)
        clif_misceffect(target, effect_nr);
}

static void entity_effect(BlockList *entity, int32_t effect_nr, int32_t delay)
{
    add_timer(gettick() + delay, timer_callback_effect, entity->id, effect_nr);
}

static void timer_callback_effect_npc_delete(timer_id, tick_t, uint32_t npc_id)
{
    delete map_id2bl(npc_id);
}

static struct npc_data *local_spell_effect(location_t loc, int32_t effect, int32_t tdelay)
{
    // 1 minute should be enough for all interesting spell effects, I hope
    // unit = 2 ticks?
    int32_t npc_delay = 30000;
    // the client can't handle effects directly on a tile
    // "" means the name won't show, NULL means no dialog will be opened
    npc_data *effect_npc = npc_spawn_text(loc, INVISIBLE_NPC, "", NULL);
    uint32_t effect_npc_id = effect_npc->id;

    // tdelay is how long until the effect starts
    // npc_delay is how long until the npc is deleted
    // ideally npc_delay would be tdelay + effect_time, but the length of
    // the effects is only in the client data
    entity_effect(effect_npc, effect, tdelay);
    add_timer(gettick() + npc_delay, timer_callback_effect_npc_delete, effect_npc_id);

    return effect_npc;
}

// operations
static bool op_sfx(env_t *, val_t *args)
{
    int32_t delay = ARG_INT(2);

    switch(args[0].ty)
    {
    case TY::ENTITY:
        entity_effect(ARG_ENTITY(0), ARG_INT(1), delay);
        return 0;
    case TY::LOCATION:
        local_spell_effect(ARG_LOCATION(0), ARG_INT(1), delay);
        return 0;
    default:
        return 1;
    }
}

static bool op_instaheal(env_t *env, val_t *args)
{
    BlockList *caster = NULL;
    if (env->VAR(Var::CASTER).ty == TY::ENTITY)
        caster = map_id2bl(env->VAR(Var::CASTER).v_int);
    BlockList *subject = ARG_ENTITY(0);
    if (!caster)
        caster = subject;

    if (caster->type == BL_PC && subject->type == BL_PC)
    {
        MapSessionData *caster_pc = static_cast<MapSessionData *>(caster);
        MapSessionData *subject_pc = static_cast<MapSessionData *>(subject);
        MAP_LOG_PC(caster_pc, "SPELLHEAL-INSTA PC%d FOR %d",
                   subject_pc->status.char_id, ARG_INT(1));
    }

    battle_heal(caster, subject, ARG_INT(1), ARG_INT(2));
    return 0;
}

static bool op_itemheal(env_t *env, val_t *args)
{
    BlockList *subject = ARG_ENTITY(0);
    if (subject->type != BL_PC)
        return op_instaheal(env, args);

    pc_itemheal(static_cast<MapSessionData *>(subject), ARG_INT(1), ARG_INT(2));

    return 0;
}

BIT_ENUM(Shroud, uint8_t)
{
    NONE = 0,

    HIDE_NAME_TALKING_FLAG      = 1 << 0,
    DISAPPEAR_ON_PICKUP_FLAG    = 1 << 1,
    DISAPPEAR_ON_TALK_FLAG      = 1 << 2,

    ALL = HIDE_NAME_TALKING_FLAG | DISAPPEAR_ON_PICKUP_FLAG | DISAPPEAR_ON_TALK_FLAG
};

static bool op_shroud(env_t *, val_t *args)
{
    MapSessionData *subject = ARG_PC(0);

    if (!subject)
        return 0;

    Shroud arg = static_cast<Shroud>(ARG_INT(1));

    subject->state.shroud_active = 1;
    subject->state.shroud_hides_name_talking = bool(arg & Shroud::HIDE_NAME_TALKING_FLAG);
    subject->state.shroud_disappears_on_pickup = bool(arg & Shroud::DISAPPEAR_ON_PICKUP_FLAG);
    subject->state.shroud_disappears_on_talk = bool(arg & Shroud::DISAPPEAR_ON_TALK_FLAG);
    return 0;
}

static bool op_unshroud(env_t *, val_t *args)
{
    MapSessionData *subject = ARG_PC(0);

    if (subject && subject->state.shroud_active)
        magic_unshroud(subject);

    return 0;
}

static bool op_message(env_t *, val_t *args)
{
    MapSessionData *subject = ARG_PC(0);

    if (subject)
        clif_displaymessage(subject->fd, ARG_STR(1).c_str());

    return 0;
}

static void timer_callback_kill_npc(timer_id, tick_t, BlockList *npc)
{
    delete npc;
}

static bool op_messenger_npc(env_t *, val_t *args)
{
    struct npc_data *npc = npc_spawn_text(ARG_LOCATION(0), ARG_INT(1), ARG_STR(2).c_str(), ARG_STR(3).c_str());

    add_timer(gettick() + ARG_INT(4), timer_callback_kill_npc, static_cast<BlockList *>(npc));

    return 0;
}

static void entity_warp(BlockList *target, location_t dest)
{
    switch (target->type)
    {
    case BL_PC:
    {
        MapSessionData *character = static_cast<MapSessionData *>(target);
        clif_being_remove(character, BeingRemoveType::WARP);
        map_delblock(character);
        character->x = dest.x;
        character->y = dest.y;
        character->m = dest.m;

        pc_touch_all_relevant_npcs(character);

        // Note that touching NPCs may have triggered warping and thereby updated x and y:
        pc_setpos(character, Point{maps[character->m].name, character->x, character->y}, BeingRemoveType::ZERO);
        break;
    }
    case BL_MOB:
        target->x = dest.x;
        target->y = dest.y;
        target->m = dest.m;
        clif_fixmobpos(static_cast<struct mob_data *>(target));
        break;
    default:
        break;
    }
}

static void char_update(MapSessionData *character)
{
    entity_warp(character, {character->m, character->x, character->y});
}

void magic_unshroud(MapSessionData *other_char)
{
    other_char->state.shroud_active = 0;
    // Now warp the caster out of and back into here to refresh everyone's display
    char_update(other_char);
    clif_displaymessage(other_char->fd, "Your shroud has been dispelled!");
    //        entity_effect(other_char, MAGIC_EffectType::REVEAL);
}

static bool op_move(env_t *, val_t *args)
{
    BlockList *subject = ARG_ENTITY(0);
    Direction dir = ARG_DIR(1);

    uint16_t newx = subject->x + heading_x[static_cast<int32_t>(dir)];
    uint16_t newy = subject->y + heading_y[static_cast<int32_t>(dir)];

    if (!map_is_solid(subject->m, newx, newy))
        entity_warp(subject, {subject->m, newx, newy});

    return 0;
}

static bool op_warp(env_t *, val_t *args)
{
    entity_warp(ARG_ENTITY(0), ARG_LOCATION(1));

    return 0;
}

static bool op_banish(env_t *, val_t *args)
{
    BlockList *subject = ARG_ENTITY(0);

    if (subject->type == BL_MOB)
    {
        struct mob_data *mob = static_cast<struct mob_data *>(subject);

        if (mob->mode & MOB_MODE_SUMMONED)
            mob_catch_delete(mob);
    }

    return 0;
}

static void record_status_change(invocation_t *invocation, int32_t bl_id, int32_t sc_id)
{
    // TODO reduce this once things get strict types
    status_change_ref_t cr;
    cr.sc_type = sc_id;
    cr.bl_id = bl_id;
    invocation->status_change_refs.push_back(cr);
}

static bool op_status_change(env_t *env, val_t *args)
{
    BlockList *subject = ARG_ENTITY(0);
    int32_t invocation_id = env->VAR(Var::INVOCATION).ty == TY::INVOCATION
        ? env->VAR(Var::INVOCATION).v_int : 0;
    invocation_t *invocation = static_cast<invocation_t *>(map_id2bl(invocation_id));

    skill_status_effect(subject, ARG_INT(1), ARG_INT(2),
                        /* ARG_INT(3), ARG_INT(4), ARG_INT(5), */
                        ARG_INT(6), invocation_id);

    if (invocation && subject->type == BL_PC)
        record_status_change(invocation, subject->id, ARG_INT(1));

    return 0;
}

static bool op_stop_status_change(env_t *, val_t *args)
{
    BlockList *subject = ARG_ENTITY(0);

    skill_status_change_end(subject, ARG_INT(1), NULL);

    return 0;
}

static bool op_override_attack(env_t *env, val_t *args)
{
    MapSessionData *subject = ARG_PC(0);
    if (!subject)
        return 0;

    int32_t charges = ARG_INT(1);
    int32_t attack_delay = ARG_INT(2);
    int32_t attack_range = ARG_INT(3);
    int32_t icon = ARG_INT(4);
    int32_t look = ARG_INT(5);
    int32_t stopattack = ARG_INT(6);

    if (subject->attack_spell_override)
    {
        invocation_t *old_invocation =
                static_cast<invocation_t *>(map_id2bl(subject->attack_spell_override));
        if (old_invocation)
            spell_free_invocation(old_invocation);
    }

    subject->attack_spell_override =
        trigger_spell(subject->id, env->VAR(Var::INVOCATION).v_int);
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

static bool op_create_item(env_t *, val_t *args)
{
    MapSessionData *subject = ARG_PC(0);
    if (!subject)
        return 0;

    int32_t count = ARG_INT(2);
    if (count <= 0)
        return 0;

    struct item item;
    bool stackable;
    GET_ARG_ITEM(1, item, stackable);

    if (!stackable)
        while (count--)
            pc_additem(subject, &item, 1);
    else
        pc_additem(subject, &item, count);

    return 0;
}

static bool AGGRAVATION_MODE_ATTACKS_CASTER(int32_t n)
{
    return n == 0 || n == 2;
}
static bool AGGRAVATION_MODE_MAKES_AGGRESSIVE(int32_t n)
{
    return n > 0;
}

static bool op_aggravate(env_t *, val_t *args)
{
    mob_data *other = ARG_MOB(0);

    if (!other)
        return 0;

    // note: in current magic builds, mode is always 0
    int32_t mode = ARG_INT(1);
    BlockList *victim = ARG_ENTITY(2);

    mob_target(other, victim, battle_get_range(victim));

    if (AGGRAVATION_MODE_MAKES_AGGRESSIVE(mode))
        other->mode = 0x85 | (other->mode & MOB_SENSIBLE_MASK);

    if (AGGRAVATION_MODE_ATTACKS_CASTER(mode))
    {
        other->target_id = victim->id;
        other->attacked_id = victim->id;
    }

    return 0;
}

enum class MonsterAttitude
{
    HOSTILE,
    FRIENDLY,
    SERVANT,
    FROZEN
};

static bool op_spawn(env_t *, val_t *args)
{
    area_t *area = ARG_AREA(0);
    MapSessionData *owner = ARG_PC(1);
    int32_t monster_id = ARG_INT(2);
    MonsterAttitude monster_attitude = static_cast<MonsterAttitude>(ARG_INT(3));
    int32_t monster_count = ARG_INT(4);
    int32_t monster_lifetime = ARG_INT(5);

    if (monster_attitude != MonsterAttitude::SERVANT)
        owner = NULL;

    for (int32_t i = 0; i < monster_count; i++)
    {
        location_t loc = area->random_location();

        int32_t mob_id = mob_once_spawn(owner, Point{maps[loc.m].name, loc.x, loc.y}, "--ja--",    // Is that needed?
                                    monster_id, 1, "");

        mob_data *mob = static_cast<struct mob_data *>(map_id2bl(mob_id));

        if (mob)
        {
            mob->mode = mob_db[monster_id].mode;

            switch (monster_attitude)
            {

                case MonsterAttitude::SERVANT:
                    mob->state.special_mob_ai = 1;
                    mob->mode |= 0x04;
                    break;

                case MonsterAttitude::FRIENDLY:
                    mob->mode = 0x80 | (mob->mode & 1);
                    break;

                case MonsterAttitude::HOSTILE:
                    mob->mode = 0x84 | (mob->mode & 1);
                    if (owner)
                    {
                        mob->target_id = owner->id;
                        mob->attacked_id = owner->id;
                    }
                    break;

                case MonsterAttitude::FROZEN:
                    mob->mode = 0;
                    break;
            }

            mob->mode |= MOB_MODE_SUMMONED | MOB_MODE_TURNS_AGAINST_BAD_MASTER;

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
    if (env->VAR(Var::INVOCATION).ty != TY::INVOCATION)
    {
        POD_string out = NULL;
        out.assign("?");
        return out;
    }
    invocation_t *invocation = static_cast<invocation_t *>(map_id2bl(env->VAR(Var::INVOCATION).v_int));

    if (invocation)
        return invocation->spell->name.clone();
    POD_string out = NULL;
    out.assign("??");
    return out;
}

static bool op_injure(env_t *env, val_t *args)
{
    BlockList *caster = ARG_ENTITY(0);
    BlockList *target = ARG_ENTITY(1);
    int32_t damage_caused = ARG_INT(2);
    int32_t mp_damage = ARG_INT(3);
    int32_t target_hp = battle_get_hp(target);
    int32_t mdef = battle_get_mdef(target);

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

static bool op_emote(env_t *, val_t *args)
{
    BlockList *victim = ARG_ENTITY(0);
    int32_t emotion = ARG_INT(1);
    clif_emotion(victim, emotion);

    return 0;
}

static bool op_set_script_variable(env_t *, val_t *args)
{
    MapSessionData *c = (ARG_ENTITY(0)->type == BL_PC) ? ARG_PC(0) : NULL;

    if (!c)
        return 1;

    pc_setglobalreg(c, std::string(ARG_STR(1).c_str()), ARG_INT(2));

    return 0;
}

static bool op_set_hair_colour(env_t *, val_t *args)
{
    MapSessionData *c = (ARG_ENTITY(0)->type == BL_PC) ? ARG_PC(0) : NULL;

    if (!c)
        return 1;

    pc_changelook(c, LOOK::HAIR_COLOR, ARG_INT(1));

    return 0;
}

static bool op_set_hair_style(env_t *, val_t *args)
{
    MapSessionData *c = (ARG_ENTITY(0)->type == BL_PC) ? ARG_PC(0) : NULL;

    if (!c)
        return 1;

    pc_changelook(c, LOOK::HAIR, ARG_INT(1));

    return 0;
}

static bool op_drop_item_for(env_t *, val_t *args)
{
    struct item item;
    bool stackable;
    location_t *loc = &ARG_LOCATION(0);
    int32_t count = ARG_INT(2);
    int32_t duration = ARG_INT(3);
    MapSessionData *c = ARG_PC(4);
    int32_t delay = ARG_INT(5);
    int32_t delaytime[3] = { delay, delay, delay };
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

static bool op_gain_experience(env_t *, val_t *args)
{
    MapSessionData *c = (ARG_ENTITY(0)->type == BL_PC) ? ARG_PC(0) : NULL;

    if (!c)
        return 1;

    pc_gainexp_reason(c, ARG_INT(1), ARG_INT(2), static_cast<PC_GAINEXP_REASON>(ARG_INT(3)));
    return 0;
}

#define OP_SIG(func, args) {#func, {args, op_##func}}
static const std::map<std::string, op_t> operations =
{
    OP_SIG(sfx, ".ii"),
    OP_SIG(instaheal, "eii"),
    OP_SIG(itemheal, "eii"),
    OP_SIG(shroud, "ei"),
    OP_SIG(unshroud, "e"),
    OP_SIG(message, "es"),
    OP_SIG(messenger_npc, "lissi"),
    OP_SIG(move, "ed"),
    OP_SIG(warp, "el"),
    OP_SIG(banish, "e"),
    OP_SIG(status_change, "eiiiiii"),
    OP_SIG(stop_status_change, "ei"),
    OP_SIG(override_attack, "eiiiiii"),
    OP_SIG(create_item, "e.i"),
    OP_SIG(aggravate, "eie"),
    OP_SIG(spawn, "aeiiii"),
    OP_SIG(injure, "eeii"),
    OP_SIG(emote, "ei"),
    OP_SIG(set_script_variable, "esi"),
    OP_SIG(set_hair_colour, "ei"),
    OP_SIG(set_hair_style, "ei"),
    OP_SIG(drop_item_for, "l.iiei"),
    OP_SIG(gain_experience, "eiii"),
};

const std::pair<const std::string, op_t> *magic_get_op(const char *name)
{
    auto it = operations.find(name);
    if (it != operations.end())
        // it's a const map so iterators will remain valid
        return &*it;

    return NULL;
}

void spell_effect_report_termination(int32_t invocation_id, int32_t bl_id, int32_t sc_id, int32_t)
{
    invocation_t *invocation = static_cast<invocation_t *>(map_id2bl(invocation_id));

    if (!invocation || invocation->type != BL_SPELL)
        return;

    status_change_ref_t target_scr;
    target_scr.sc_type = sc_id;
    target_scr.bl_id = bl_id;

    // refs is a vector - std::find is an inefficient linear search
    auto& refs = invocation->status_change_refs;
    auto actual_scr = std::find(refs.begin(), refs.end(), target_scr);

    if (actual_scr == refs.end())
    {
        BlockList *entity = map_id2bl(bl_id);
        if (entity->type == BL_PC)
            fprintf(stderr,
                    "[magic] INTERNAL ERROR: %s:  tried to terminate on unexpected bl %d, sc %d\n",
                    __func__, bl_id, sc_id);
        abort();
    }

    refs.erase(actual_scr);

    try_to_finish_invocation(invocation);
}

static effect_t *return_to_stack(invocation_t *invocation)
{
    if (invocation->stack.empty())
        return NULL;

    cont_activation_record_t *ar = &invocation->stack.back();
    switch (ar->ty)
    {
    case ContStackType::PROC:
    {
        // clean up after a function returns, by restoring variables hidden by arguments
        effect_t *ret = ar->return_location;
        for (int32_t i = 0; i < ar->c_proc.args_nr; i++)
        {
            val_t *var = &invocation->env->vars[ar->c_proc.formals[i]];
            magic_clear_var(var);
            // clear the argument, then overwrite it with the raw backup value
            *var = ar->c_proc.old_actuals[i];
        }

        invocation->stack.pop_back();
        return ret;
    }

    case ContStackType::FOREACH:
    {
        val_t *var = &invocation->env->vars[ar->c_foreach.var_id];

        int32_t entity_id;
        do
        {
            if (ar->c_foreach.entities.empty())
            {
                effect_t *ret = ar->return_location;
                invocation->stack.pop_back();
                return ret;
            }

            entity_id = ar->c_foreach.entities.back();
            ar->c_foreach.entities.pop_back();
        }
        while (!entity_id || !map_id2bl(entity_id));

        magic_clear_var(var);
        // not always TY::ENTITY, sometimes TY::INVOCATION
        var->ty = ar->c_foreach.ty;
        var->v_int = entity_id;

        return ar->c_foreach.body;
    }

    case ContStackType::FOR:
        if (ar->c_for.current > ar->c_for.stop)
        {
            effect_t *ret = ar->return_location;
            invocation->stack.pop_back();
            return ret;
        }

        magic_clear_var(&invocation->env->vars[ar->c_for.var_id]);
        invocation->env->vars[ar->c_for.var_id].ty = TY::INT;
        invocation->env->vars[ar->c_for.var_id].v_int = ar->c_for.current++;

        return ar->c_for.body;

    default:
        fprintf(stderr,
                "[magic] INTERNAL ERROR: While executing spell `%s':  stack corruption\n",
                invocation->spell->name.c_str());
        abort();
    }
}

static cont_activation_record_t *add_stack_entry(invocation_t *invocation,
                                                 ContStackType ty,
                                                 effect_t *return_location)
{
    if (invocation->stack.full())
    {
        fprintf(stderr,
                "[magic] Execution stack size exceeded in spell `%s'; truncating effect\n",
                invocation->spell->name.c_str());
        abort();
    }

    invocation->stack.push_back(cont_activation_record_t(ty, return_location));

    return &invocation->stack.back();
}

static void find_entities_in_area_c(BlockList *target,
                                    std::vector<int32_t> *entities,
                                    ForEach_FilterType filter)
{
    // break in the switch adds "target" to *entities, return does not add it
    switch (target->type)
    {
    case BL_PC:
        if (filter == ForEach_FilterType::PC
            || filter == ForEach_FilterType::ENTITY
            || (filter == ForEach_FilterType::TARGET && maps[target->m].flag.pvp))
            break;

        if (filter == ForEach_FilterType::SPELL)
        {
            // Check all spells bound to the caster
            auto invocs = static_cast<MapSessionData *>(target)->active_spells;
            // Add all spells locked onto thie PC
            for(auto invoc : invocs)
                entities->push_back(invoc->id);
        }
        return;

    case BL_MOB:
        if (filter == ForEach_FilterType::MOB
            || filter == ForEach_FilterType::ENTITY
            || filter == ForEach_FilterType::TARGET)
            break;

        return;

    case BL_SPELL:
        if (filter != ForEach_FilterType::SPELL)
            return;

        if (static_cast<invocation_t *>(target)->subject)
            // in this case it will be added as attached to case BL_PC
            return;

        break;

    case BL_NPC:
        if (filter != ForEach_FilterType::NPC)
            return;

        break;

    default:
        return;
    }

    entities->push_back(target->id);
}

static void find_entities_in_area(area_t *area, std::vector<int32_t> *entities_p, ForEach_FilterType filter)
{
    switch (area->ty)
    {
    case AreaType::UNION:
        find_entities_in_area(area->a_union[0], entities_p, filter);
        find_entities_in_area(area->a_union[1], entities_p, filter);
        break;

    default:
    {
        uint32_t width, height;
        location_t loc = area->rect(width, height);
        map_foreachinarea(find_entities_in_area_c,
                          loc.m, loc.x, loc.y, loc.x + width, loc.y + height,
                          BL_NUL /* filter elsewhere */ ,
                          entities_p,
                          filter);
    }
    }
}

/// Start running a FOREACH
static effect_t *run_foreach(invocation_t *invocation, effect_t *foreach,
                             effect_t *return_location)
{
    ForEach_FilterType filter = foreach->e_foreach.filter;
    int32_t id = foreach->e_foreach.var_id;
    effect_t *body = foreach->e_foreach.body;

    val_t area = invocation->env->magic_eval(foreach->e_foreach.area);

    if (area.ty != TY::AREA)
    {
        magic_clear_var(&area);
        fprintf(stderr,
                "[magic] Error in spell `%s':  FOREACH loop over non-area\n",
                invocation->spell->name.c_str());
        return return_location;
    }

    cont_activation_record_t *ar = add_stack_entry(invocation, ContStackType::FOREACH, return_location);
    if (!ar)
        return return_location;

    std::vector<int32_t> entities;
    find_entities_in_area(area.v_area, &entities, filter);

    // is this worthwhile?
    std::random_shuffle(entities.begin(), entities.end());

    ar->c_foreach.var_id = id;
    ar->c_foreach.body = body;
    ar->c_foreach.entities = std::move(entities);
    ar->c_foreach.ty = (filter == ForEach_FilterType::SPELL) ? TY::INVOCATION : TY::ENTITY;

    magic_clear_var(&area);

    return return_to_stack(invocation);
}

// start running a FOR
static effect_t *run_for(invocation_t *invocation, effect_t *for_,
                         effect_t *return_location)
{
    int32_t id = for_->e_for.var_id;

    val_t start = invocation->env->magic_eval(for_->e_for.start);
    val_t stop = invocation->env->magic_eval(for_->e_for.stop);

    if (start.ty != TY::INT || stop.ty != TY::INT)
    {
        // in the normal case these are integers so don't need cleared
        magic_clear_var(&start);
        magic_clear_var(&stop);
        fprintf(stderr,
                "[magic] Error in spell `%s':  FOR loop start or stop point is not an integer\n",
                invocation->spell->name.c_str());
        return return_location;
    }

    cont_activation_record_t *ar = add_stack_entry(invocation, ContStackType::FOR, return_location);

    ar->c_for.var_id = id;
    ar->c_for.current = start.v_int;
    ar->c_for.stop = stop.v_int;
    ar->c_for.body = for_->e_for.body;

    return return_to_stack(invocation);
}

// set up/backup arguments to run a CALL
static effect_t *run_call(invocation_t *invocation, effect_t *return_location)
{
    effect_t *current = invocation->current_effect;
    int32_t args_nr = current->e_call.args_nr;
    DArray<int32_t> formals = current->e_call.formals;
    DArray<val_t> old_actuals;
    old_actuals.resize(args_nr);

    cont_activation_record_t *ar = add_stack_entry(invocation, ContStackType::PROC, return_location);
    ar->c_proc.args_nr = args_nr;
    ar->c_proc.formals = formals;
    // class DArray is such that modifying old_actuals keeps things distinct
    ar->c_proc.old_actuals = old_actuals;

    // backup the function arguments
    for (int32_t i = 0; i < args_nr; i++)
    {
        // just overwrite the raw data, don't use magic_copy_var
        val_t& env_val = invocation->env->vars[formals[i]];
        old_actuals[i] = env_val;
        env_val = invocation->env->magic_eval(current->e_call.actuals[i]);
    }

    return current->e_call.body;
}

/**
 * Execute a spell invocation until we abort, finish, or hit the next `sleep'.
 *
 * Use spell_execute() to automate handling of timers
 *
 * Returns: 0 if finished(all memory is freed implicitly)
 *          >1 if we hit `sleep'; the result is the number of ticks we should sleep for.
 *          -1 if we paused to wait for a user action(via script interaction)
 */
static int32_t spell_run(invocation_t *invocation, bool allow_delete)
{
    const int32_t invocation_id = invocation->id;
#define REFRESH_INVOCATION() \
    do \
    { \
        invocation = static_cast<invocation_t *>(map_id2bl(invocation_id)); \
        if (!invocation) \
            return 0; \
    } while(0)

    while (invocation->current_effect)
    {
        effect_t *e = invocation->current_effect;
        effect_t *next = e->next;

        switch (e->ty)
        {
        case EffectType::SKIP:
            break;

        case EffectType::ABORT:
            invocation->flags |= InvocationFlag::ABORTED;
            invocation->end_effect = NULL;
            // fall through
        case EffectType::END:
            invocation->stack.clear();
            next = NULL;
            break;

        case EffectType::ASSIGN:
            invocation->env->vars[e->e_assign.id] = invocation->env->magic_eval(e->e_assign.expr);
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
            int32_t sleeptime = magic_eval_int(invocation->env, e->e_sleep);
            invocation->current_effect = next;
            if (sleeptime > 0)
                return sleeptime;
            break;
        }

        case EffectType::SCRIPT:
        {
            MapSessionData *caster = map_id2sd(invocation->caster);
            if (!caster)
                break;
            env_t *env = invocation->env;
            ArgRec arg[] =
            {
                { "@target", env->VAR(Var::TARGET).ty == TY::ENTITY ? 0 : env->VAR(Var::TARGET).v_int },
                { "@caster", invocation->caster },
                { "@caster_name$", caster ? caster->status.name : "" }
            };
            int32_t message_recipient = env->VAR(Var::SCRIPTTARGET).ty == TY::ENTITY
                ? env->VAR(Var::SCRIPTTARGET).v_int
                : invocation->caster;
            MapSessionData *recipient = map_id2sd(message_recipient);

            if (recipient->npc_id && recipient->npc_id != invocation->id)
                // Don't send multiple message boxes at once
                break;

            if (!invocation->script_pos)
                // first time running this script?
                // We have to do this or otherwise the client won't think that it's
                // dealing with an NPC
                clif_spawn_fake_npc_for_player(recipient, invocation->id);

            // Returns the new script position, or -1 once the script is finished
            int32_t newpos = run_script_l(*e->e_script, invocation->script_pos,
                                          message_recipient, invocation->id,
                                          ARRAY_SIZEOF(arg), arg);
            if (newpos != -1)
            {
                // Must set up for continuation
                recipient->npc_id = invocation->id;
                recipient->npc_pos = invocation->script_pos = newpos;
                // Signal "wait for script"
                return -1;
            }
            else
                invocation->script_pos = 0;
            clif_being_remove_id(invocation->id, BeingRemoveType::DEAD, caster->fd);
            REFRESH_INVOCATION(); // Script may have killed the caster
            break;
        }

        case EffectType::BREAK:
            next = return_to_stack(invocation);
            break;

        case EffectType::OP:
        {
            const std::pair<const std::string, op_t> *op = e->e_op.op;

            val_t args[MAX_ARGS];

            for (int32_t i = 0; i < e->e_op.args_nr; i++)
                args[i] = invocation->env->magic_eval(e->e_op.args[i]);

            if (!magic_signature_check("effect", op->first.c_str(),
                                       op->second.signature,
                                       e->e_op.args_nr, args,
                                       e->e_op.line_nr, e->e_op.column))
                op->second.op(invocation->env, args);

            for (int32_t i = 0; i < e->e_op.args_nr; i++)
                magic_clear_var(&args[i]);

            REFRESH_INVOCATION(); // EffectType may have killed the caster
            break;
        }

        case EffectType::CALL:
            next = run_call(invocation, next);
            break;

        default:
            fprintf(stderr, "[magic] INTERNAL ERROR: Unknown effect %d\n",
                    static_cast<int32_t>(e->ty));
            abort();
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

static void spell_execute_d(invocation_t *invocation, bool allow_deletion)
{
    spell_update_location(invocation);
    int32_t delta = spell_run(invocation, allow_deletion);

    if (delta <= 0)
        // Finished, or pending user input from script
        return;
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

}

void spell_execute(invocation_t *invocation)
{
    spell_execute_d(invocation, true);
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

bool spell_attack(int32_t caster_id, int32_t target_id)
{
    MapSessionData *caster = map_id2sd(caster_id);
    invocation_t *invocation;
    int32_t stop_attack = 0;

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
        // don't delete the invocation if done
        spell_execute_d(invocation, false);

        // If the caster died, we need to refresh here:
        invocation = static_cast<invocation_t *>(map_id2bl(caster->attack_spell_override));

        if (invocation && !(invocation->flags & InvocationFlag::ABORTED))   // If we didn't abort:
            caster->attack_spell_charges--;
    }

    if (invocation && caster->attack_spell_override != invocation->id)
    {
        // Attack spell changed or was refreshed
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
