#include "magic-stmt.hpp"
//    magic-stmt.cpp - Imperative commands for the magic backend.
//
//    Copyright © 2004-2011 The Mana World Development Team
//    Copyright © 2011-2014 Ben Longbons <b.r.longbons@gmail.com>
//
//    This file is part of The Mana World (Athena server)
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <cassert>

#include "../compat/attr.hpp"
#include "../compat/fun.hpp"

#include "../strings/zstring.hpp"

#include "../generic/random2.hpp"

#include "../io/cxxstdio.hpp"

#include "../net/timer.hpp"

#include "battle.hpp"
#include "clif.hpp"
#include "magic.hpp"
#include "magic-expr.hpp"
#include "magic-expr-eval.hpp"
#include "magic-interpreter.hpp"
#include "magic-interpreter-base.hpp"
#include "mob.hpp"
#include "npc.hpp"
#include "pc.hpp"
#include "skill.hpp"

#include "../poison.hpp"


namespace tmwa
{
/* used for local spell effects */
constexpr Species INVISIBLE_NPC = wrap<Species>(127);

//#define DEBUG

#ifdef DEBUG
static
void print_val(val_t *v)
{
    switch (v->ty)
    {
        case TYPE::UNDEF:
            FPRINTF(stderr, "UNDEF"_fmt);
            break;
        case TYPE::INT:
            FPRINTF(stderr, "%d"_fmt, v->v.v_int);
            break;
        case TYPE::DIR:
            FPRINTF(stderr, "dir%d"_fmt, v->v.v_int);
            break;
        case TYPE::STRING:
            FPRINTF(stderr, "`%s'"_fmt, v->v.v_string);
            break;
        default:
            FPRINTF(stderr, "ty%d"_fmt, v->ty);
            break;
    }
}

static
void dump_env(env_t *env)
{
    int i;
    for (i = 0; i < env->base_env->vars_nr; i++)
    {
        val_t *v = &env->vars[i];
        val_t *bv = &env->base_env->vars[i];

        FPRINTF(stderr, "%02x %30s "_fmt, i, env->base_env->var_name[i]);
        print_val(v);
        FPRINTF(stderr, "\t("_fmt);
        print_val(bv);
        FPRINTF(stderr, ")\n"_fmt);
    }
}
#endif

static
void clear_activation_record(cont_activation_record_t *ar)
{
    switch (ar->ty)
    {
        case CONT_STACK::FOREACH:
            ar->c.c_foreach.entities_vp.delete_();
            break;
        case CONT_STACK::PROC:
            ar->c.c_proc.old_actualpa.delete_();
            break;
    }
}

static
void invocation_timer_callback(TimerData *, tick_t, BlockId id)
{
    dumb_ptr<invocation> invocation = map_id_is_spell(id);

    assert (invocation);
    {
        spell_execute(invocation);
    }
}

static
void clear_stack(dumb_ptr<invocation> invocation_)
{
    int i;

    for (i = 0; i < invocation_->stack_size; i++)
        clear_activation_record(&invocation_->stack[i]);

    invocation_->stack_size = 0;
}

void spell_free_invocation(dumb_ptr<invocation> invocation_)
{
    invocation_->status_change_refv.clear();

    if (bool(invocation_->flags & INVOCATION_FLAG::BOUND))
    {
        dumb_ptr<map_session_data> e = map_id_is_player(invocation_->subject);
        if (e)
            spell_unbind(e, invocation_);
    }

    clear_stack(invocation_);

    invocation_->timer.cancel();

    magic_free_env(invocation_->env);

    map_delblock(invocation_);
    map_delobject(invocation_->bl_id, BL::SPELL);    // also frees the object
//        free(invocation_);
}

static
void char_set_weapon_icon(dumb_ptr<map_session_data> subject, int count,
        StatusChange icon, ItemNameId look)
{
    const StatusChange old_icon = subject->attack_spell_icon_override;

    subject->attack_spell_icon_override = icon;
    subject->attack_spell_look_override = look;

    if (old_icon != StatusChange::ZERO && old_icon != icon)
        clif_status_change(subject, old_icon, 0);

    clif_fixpcpos(subject);
    if (count)
    {
        clif_changelook(subject, LOOK::WEAPON, unwrap<ItemNameId>(look));
        if (icon != StatusChange::ZERO)
            clif_status_change(subject, icon, 1);
    }
    else
    {
        /* Set it to `normal' */
        clif_changelook(subject, LOOK::WEAPON,
                static_cast<uint16_t>(subject->status.weapon));
    }
}

static
void char_set_attack_info(dumb_ptr<map_session_data> subject, interval_t speed, int range)
{
    subject->attack_spell_delay = speed;
    subject->attack_spell_range = range;

    if (speed == interval_t::zero())
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

void magic_stop_completely(dumb_ptr<map_session_data> c)
{
    // Zap all status change references to spells
    for (StatusChange i : erange(StatusChange(), StatusChange::MAX_STATUSCHANGE))
        c->sc_data[i].spell_invocation = BlockId();

    while (c->active_spells)
        spell_free_invocation(c->active_spells);

    if (c->attack_spell_override)
    {
        dumb_ptr<invocation> attack_spell = map_id_is_spell(c->attack_spell_override);
        if (attack_spell)
            spell_free_invocation(attack_spell);
        c->attack_spell_override = BlockId();
        char_set_weapon_icon(c, 0, StatusChange::ZERO, ItemNameId());
        char_set_attack_info(c, interval_t::zero(), 0);
    }
}

/* Spell execution has finished normally or we have been notified by a finished skill timer */
static
void try_to_finish_invocation(dumb_ptr<invocation> invocation)
{
    if (invocation->status_change_refv.empty() && !invocation->current_effect)
    {
        if (invocation->end_effect)
        {
            clear_stack(invocation);
            invocation->current_effect = invocation->end_effect;
            invocation->end_effect = nullptr;
            spell_execute(invocation);
        }
        else
            spell_free_invocation(invocation);
    }
}

static
BlockId trigger_spell(BlockId subject, BlockId spell)
{
    dumb_ptr<invocation> invocation_ = map_id_is_spell(spell);

    if (!invocation_)
        return BlockId();

    invocation_ = spell_clone_effect(invocation_);

    spell_bind(map_id_is_player(subject), invocation_);
    magic_clear_var(&invocation_->env->varu[VAR_CASTER]);
    invocation_->env->varu[VAR_CASTER].ty = TYPE::ENTITY;
    invocation_->env->varu[VAR_CASTER].v.v_int = static_cast<int32_t>(unwrap<BlockId>(subject));

    return invocation_->bl_id;
}

static
void entity_warp(dumb_ptr<block_list> target, map_local *destm, int destx, int desty);

static
void char_update(dumb_ptr<map_session_data> character)
{
    entity_warp(character, character->bl_m, character->bl_x,
                 character->bl_y);
}

static
void timer_callback_effect(TimerData *, tick_t, BlockId id, int data)
{
    dumb_ptr<block_list> target = map_id2bl(id);
    if (target)
        clif_misceffect(target, data);
}

static
void entity_effect(dumb_ptr<block_list> entity, int effect_nr, interval_t delay)
{
    Timer(gettick() + delay,
            std::bind(&timer_callback_effect, ph::_1, ph::_2,
                entity->bl_id, effect_nr)
    ).detach();
}

void magic_unshroud(dumb_ptr<map_session_data> other_char)
{
    other_char->state.shroud_active = 0;
    // Now warp the caster out of and back into here to refresh everyone's display
    char_update(other_char);
    clif_displaymessage(other_char->sess, "Your shroud has been dispelled!"_s);
//        entity_effect(other_char, MAGIC_EFFECT_REVEAL);
}

static
void timer_callback_effect_npc_delete(TimerData *, tick_t, BlockId npc_id)
{
    dumb_ptr<npc_data> effect_npc = map_id_is_npc(npc_id);
    npc_free(effect_npc);
}

static
dumb_ptr<npc_data> local_spell_effect(map_local *m, int x, int y, int effect,
        interval_t tdelay)
{
    /* 1 minute should be enough for all interesting spell effects, I hope */
    std::chrono::seconds delay = 30_s;
    dumb_ptr<npc_data> effect_npc = npc_spawn_text(m, x, y,
            INVISIBLE_NPC, NpcName(), "?"_s);
    BlockId effect_npc_id = effect_npc->bl_id;

    entity_effect(effect_npc, effect, tdelay);
    Timer(gettick() + delay,
            std::bind(timer_callback_effect_npc_delete, ph::_1, ph::_2,
                effect_npc_id)
    ).detach();

    return effect_npc;
}

static
int op_sfx(dumb_ptr<env_t>, Slice<val_t> args)
{
    interval_t delay = static_cast<interval_t>(ARGINT(2));

    if (ARG_TYPE(0) == TYPE::ENTITY)
    {
        entity_effect(ARGENTITY(0), ARGINT(1), delay);
    }
    else if (ARG_TYPE(0) == TYPE::LOCATION)
    {
        local_spell_effect(ARGLOCATION(0).m,
                            ARGLOCATION(0).x,
                            ARGLOCATION(0).y, ARGINT(1), delay);
    }
    else
        return 1;

    return 0;
}

static
int op_instaheal(dumb_ptr<env_t> env, Slice<val_t> args)
{
    dumb_ptr<block_list> caster = (env->VAR(VAR_CASTER).ty == TYPE::ENTITY)
        ? map_id2bl(wrap<BlockId>(static_cast<uint32_t>(env->VAR(VAR_CASTER).v.v_int))) : nullptr;
    dumb_ptr<block_list> subject = ARGENTITY(0);
    if (!caster)
        caster = subject;

    if (caster->bl_type == BL::PC && subject->bl_type == BL::PC)
    {
        dumb_ptr<map_session_data> caster_pc = caster->is_player();
        dumb_ptr<map_session_data> subject_pc = subject->is_player();
        MAP_LOG_PC(caster_pc, "SPELLHEAL-INSTA PC%d FOR %d"_fmt,
                subject_pc->status_key.char_id, ARGINT(1));
    }

    battle_heal(caster, subject, ARGINT(1), ARGINT(2), 0);
    return 0;
}

static
int op_itemheal(dumb_ptr<env_t> env, Slice<val_t> args)
{
    dumb_ptr<block_list> subject = ARGENTITY(0);
    if (subject->bl_type == BL::PC)
    {
        pc_itemheal(subject->is_player(),
                     ARGINT(1), ARGINT(2));
    }
    else
        return op_instaheal(env, args);

    return 0;
}

namespace e
{
enum class Shroud
{
    HIDE_NAME_TALKING_FLAG      = 1 << 0,
    DISAPPEAR_ON_PICKUP_FLAG    = 1 << 1,
    DISAPPEAR_ON_TALK_FLAG      = 1 << 2,
};
ENUM_BITWISE_OPERATORS(Shroud)
}
using e::Shroud;

// differs from ARGPC by checking
#define ARGCHAR(n) (ARGENTITY(n)->is_player())

static
int op_shroud(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<map_session_data> subject = ARGCHAR(0);
    Shroud arg = static_cast<Shroud>(ARGINT(1));

    if (!subject)
        return 0;

    subject->state.shroud_active = 1;
    subject->state.shroud_hides_name_talking =
        bool(arg & Shroud::HIDE_NAME_TALKING_FLAG);
    subject->state.shroud_disappears_on_pickup =
        bool(arg & Shroud::DISAPPEAR_ON_PICKUP_FLAG);
    subject->state.shroud_disappears_on_talk =
        bool(arg & Shroud::DISAPPEAR_ON_TALK_FLAG);
    return 0;
}

static
int op_reveal(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<map_session_data> subject = ARGCHAR(0);

    if (subject && subject->state.shroud_active)
        magic_unshroud(subject);

    return 0;
}

static
int op_message(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<map_session_data> subject = ARGCHAR(0);

    if (subject)
        clif_displaymessage(subject->sess, ARGSTR(1));

    return 0;
}

static
void timer_callback_kill_npc(TimerData *, tick_t, BlockId npc_id)
{
    dumb_ptr<npc_data> npc = map_id_is_npc(npc_id);
    if (npc)
        npc_free(npc);
}

static
int op_messenger_npc(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<npc_data> npc;
    location_t *loc = &ARGLOCATION(0);

    NpcName npcname = stringish<NpcName>(ARGSTR(2));
    npc = npc_spawn_text(loc->m, loc->x, loc->y,
            wrap<Species>(static_cast<uint16_t>(ARGINT(1))), npcname, ARGSTR(3));

    Timer(gettick() + static_cast<interval_t>(ARGINT(4)),
            std::bind(timer_callback_kill_npc, ph::_1, ph::_2,
                npc->bl_id)
    ).detach();

    return 0;
}

static
void entity_warp(dumb_ptr<block_list> target, map_local *destm, int destx, int desty)
{
    if (target->bl_type == BL::PC || target->bl_type == BL::MOB)
    {

        switch (target->bl_type)
        {
            case BL::PC:
            {
                dumb_ptr<map_session_data> character = target->is_player();
                clif_clearchar(character, BeingRemoveWhy::WARPED);
                map_delblock(character);
                character->bl_x = destx;
                character->bl_y = desty;
                character->bl_m = destm;

                pc_touch_all_relevant_npcs(character);

                // Note that touching NPCs may have triggered warping and thereby updated x and y:
                MapName map_name = character->bl_m->name_;

                // Warp part #1: update relevant data, interrupt trading etc.:
                pc_setpos(character, map_name, character->bl_x, character->bl_y, BeingRemoveWhy::GONE);
                // Warp part #2: now notify the client
                clif_changemap(character, map_name,
                        character->bl_x, character->bl_y);
                break;
            }
            case BL::MOB:
                target->bl_x = destx;
                target->bl_y = desty;
                target->bl_m = destm;
                clif_fixmobpos(target->is_mob());
                break;
        }
    }
}

static
int op_move(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<block_list> subject = ARGENTITY(0);
    DIR dir = ARGDIR(1);

    int newx = subject->bl_x + dirx[dir];
    int newy = subject->bl_y + diry[dir];

    if (!bool(map_getcell(subject->bl_m, newx, newy) & MapCell::UNWALKABLE))
        entity_warp(subject, subject->bl_m, newx, newy);

    return 0;
}

static
int op_warp(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<block_list> subject = ARGENTITY(0);
    location_t *loc = &ARGLOCATION(1);

    entity_warp(subject, loc->m, loc->x, loc->y);

    return 0;
}

static
int op_banish(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<block_list> subject = ARGENTITY(0);

    if (subject->bl_type == BL::MOB)
    {
        dumb_ptr<mob_data> mob = subject->is_mob();

        if (bool(mob->mode & MobMode::SUMMONED))
            mob_catch_delete(mob, BeingRemoveWhy::WARPED);
    }

    return 0;
}

static
void record_status_change(dumb_ptr<invocation> invocation_, BlockId bl_id,
        StatusChange sc_id)
{
    status_change_ref_t cr {};
    cr.sc_type = sc_id;
    cr.bl_id = bl_id;

    invocation_->status_change_refv.push_back(cr);
}

static
int op_status_change(dumb_ptr<env_t> env, Slice<val_t> args)
{
    dumb_ptr<block_list> subject = ARGENTITY(0);
    BlockId invocation_id = env->VAR(VAR_INVOCATION).ty == TYPE::INVOCATION
        ? wrap<BlockId>(static_cast<uint32_t>(env->VAR(VAR_INVOCATION).v.v_int)) : BlockId();
    dumb_ptr<invocation> invocation_ = map_id_is_spell(invocation_id);

    assert (!ARGINT(3));
    assert (!ARGINT(4));
    assert (!ARGINT(5));
    skill_status_effect(subject, static_cast<StatusChange>(ARGINT(1)),
            ARGINT(2),
            static_cast<interval_t>(ARGINT(6)), invocation_id);

    if (invocation_ && subject->bl_type == BL::PC)
        record_status_change(invocation_, subject->bl_id, static_cast<StatusChange>(ARGINT(1)));

    return 0;
}

static
int op_stop_status_change(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<block_list> subject = ARGENTITY(0);

    StatusChange sc = static_cast<StatusChange>(ARGINT(1));
    skill_status_change_end(subject, sc, nullptr);

    return 0;
}

static
int op_override_attack(dumb_ptr<env_t> env, Slice<val_t> args)
{
    dumb_ptr<block_list> psubject = ARGENTITY(0);
    int charges = ARGINT(1);
    interval_t attack_delay = static_cast<interval_t>(ARGINT(2));
    int attack_range = ARGINT(3);
    StatusChange icon = StatusChange(ARGINT(4));
    ItemNameId look = wrap<ItemNameId>(static_cast<uint16_t>(ARGINT(5)));
    int stopattack = ARGINT(6);
    dumb_ptr<map_session_data> subject;

    if (psubject->bl_type != BL::PC)
        return 0;

    subject = psubject->is_player();

    if (subject->attack_spell_override)
    {
        dumb_ptr<invocation> old_invocation = map_id_is_spell(subject->attack_spell_override);
        if (old_invocation)
            spell_free_invocation(old_invocation);
    }

    subject->attack_spell_override =
        trigger_spell(subject->bl_id, wrap<BlockId>(static_cast<uint32_t>(env->VAR(VAR_INVOCATION).v.v_int)));
    subject->attack_spell_charges = charges;

    if (subject->attack_spell_override)
    {
        dumb_ptr<invocation> attack_spell = map_id_is_spell(subject->attack_spell_override);
        if (attack_spell && stopattack)
            attack_spell->flags |= INVOCATION_FLAG::STOPATTACK;

        char_set_weapon_icon(subject, charges, icon, look);
        char_set_attack_info(subject, attack_delay, attack_range);
    }

    return 0;
}

static
int op_create_item(dumb_ptr<env_t>, Slice<val_t> args)
{
    Item item;
    dumb_ptr<block_list> entity = ARGENTITY(0);
    dumb_ptr<map_session_data> subject;
    int stackable;
    int count = ARGINT(2);
    if (count <= 0)
        return 0;

    if (entity->bl_type == BL::PC)
        subject = entity->is_player();
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

inline
bool AGGRAVATION_MODE_ATTACKS_CASTER(int n)
{
    return n == 0 || n == 2;
}
inline
bool AGGRAVATION_MODE_MAKES_AGGRESSIVE(int n)
{
    return n > 0;
}

static
int op_aggravate(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<block_list> victim = ARGENTITY(2);
    int mode = ARGINT(1);
    dumb_ptr<block_list> target = ARGENTITY(0);
    dumb_ptr<mob_data> other;

    if (target->bl_type == BL::MOB)
        other = target->is_mob();
    else
        return 0;

    mob_target(other, victim, battle_get_range(victim));

    if (AGGRAVATION_MODE_MAKES_AGGRESSIVE(mode))
        other->mode = MobMode::war | (other->mode & MobMode::SENSIBLE_MASK);

    if (AGGRAVATION_MODE_ATTACKS_CASTER(mode))
    {
        other->target_id = victim->bl_id;
        other->attacked_id = victim->bl_id;
    }

    return 0;
}

enum class MonsterAttitude
{
    HOSTILE     = 0,
    FRIENDLY    = 1,
    SERVANT     = 2,
    FROZEN      = 3,
};

static
int op_spawn(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<area_t> area = ARGAREA(0);
    dumb_ptr<block_list> owner_e = ARGENTITY(1);
    Species monster_id = wrap<Species>(ARGINT(2));
    MonsterAttitude monster_attitude = static_cast<MonsterAttitude>(ARGINT(3));
    int monster_count = ARGINT(4);
    interval_t monster_lifetime = static_cast<interval_t>(ARGINT(5));
    int i;

    dumb_ptr<map_session_data> owner = nullptr;
    if (monster_attitude == MonsterAttitude::SERVANT
        && owner_e->bl_type == BL::PC)
        owner = owner_e->is_player();

    for (i = 0; i < monster_count; i++)
    {
        location_t loc;
        magic_random_location(&loc, area);

        BlockId mob_id;
        dumb_ptr<mob_data> mob;

        mob_id = mob_once_spawn(owner, loc.m->name_, loc.x, loc.y, JAPANESE_NAME,    // Is that needed?
                monster_id, 1, NpcEvent());

        mob = map_id_is_mob(mob_id);

        if (mob)
        {
            mob->mode = get_mob_db(monster_id).mode;

            switch (monster_attitude)
            {
                case MonsterAttitude::SERVANT:
                    mob->state.special_mob_ai = 1;
                    mob->mode |= MobMode::AGGRESSIVE;
                    break;

                case MonsterAttitude::FRIENDLY:
                    mob->mode = MobMode::CAN_ATTACK | (mob->mode & MobMode::CAN_MOVE);
                    break;

                case MonsterAttitude::HOSTILE:
                    mob->mode = MobMode::CAN_ATTACK | MobMode::AGGRESSIVE | (mob->mode & MobMode::CAN_MOVE);
                    if (owner)
                    {
                        mob->target_id = owner->bl_id;
                        mob->attacked_id = owner->bl_id;
                    }
                    break;

                case MonsterAttitude::FROZEN:
                    mob->mode = MobMode::ZERO;
                    break;
            }

            mob->mode |=
                MobMode::SUMMONED | MobMode::TURNS_AGAINST_BAD_MASTER;

            mob->deletetimer = Timer(gettick() + monster_lifetime,
                    std::bind(mob_timer_delete, ph::_1, ph::_2,
                        mob_id));

            if (owner)
            {
                mob->master_id = owner->bl_id;
                mob->master_dist = 6;
            }
        }
    }

    return 0;
}

static
ZString get_invocation_name(dumb_ptr<env_t> env)
{
    dumb_ptr<invocation> invocation_;

    if (env->VAR(VAR_INVOCATION).ty != TYPE::INVOCATION)
        return "?"_s;
    invocation_ = map_id_is_spell(wrap<BlockId>(static_cast<uint32_t>(env->VAR(VAR_INVOCATION).v.v_int)));

    if (invocation_)
        return invocation_->spell->name;
    else
        return "??"_s;
}

static
int op_injure(dumb_ptr<env_t> env, Slice<val_t> args)
{
    dumb_ptr<block_list> caster = ARGENTITY(0);
    dumb_ptr<block_list> target = ARGENTITY(1);
    int damage_caused = ARGINT(2);
    int mp_damage = ARGINT(3);
    int target_hp = battle_get_hp(target);
    int mdef = battle_get_mdef(target);

    if (target->bl_type == BL::PC
        && !target->bl_m->flag.get(MapFlag::PVP)
        && !target->is_player()->special_state.killable
        && (caster->bl_type != BL::PC || !caster->is_player()->special_state.killer))
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
    clif_damage(caster, target,
            gettick(), interval_t::zero(), interval_t::zero(),
            damage_caused, 0, DamageType::NORMAL, 0);

    if (caster->bl_type == BL::PC)
    {
        dumb_ptr<map_session_data> caster_pc = caster->is_player();
        if (target->bl_type == BL::MOB)
        {
            dumb_ptr<mob_data> mob = target->is_mob();

            MAP_LOG_PC(caster_pc, "SPELLDMG MOB%d %d FOR %d BY %s"_fmt,
                    mob->bl_id, mob->mob_class, damage_caused,
                    get_invocation_name(env));
        }
    }
    battle_damage(caster, target, damage_caused, mp_damage);

    return 0;
}

static
int op_emote(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<block_list> victim = ARGENTITY(0);
    int emotion = ARGINT(1);
    clif_emotion(victim, emotion);

    return 0;
}

static
int op_set_script_variable(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<map_session_data> c = (ENTITY_TYPE(0) == BL::PC) ? ARGPC(0) : nullptr;
    VarName varname = stringish<VarName>(ARGSTR(1));
    int array_index = 0;

    if (!c)
        return 1;

    set_script_var_i(c, varname, array_index, ARGINT(2));

    return 0;
}

static
int op_set_script_str(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<map_session_data> c = (ENTITY_TYPE(0) == BL::PC) ? ARGPC(0) : nullptr;
    VarName varname = stringish<VarName>(ARGSTR(1));
    int array_index = 0;

    if (!c)
        return 1;

    set_script_var_s(c, varname, array_index, ARGSTR(2));

    return 0;
}

static
int op_set_hair_colour(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<map_session_data> c = (ENTITY_TYPE(0) == BL::PC) ? ARGPC(0) : nullptr;

    if (!c)
        return 1;

    pc_changelook(c, LOOK::HAIR_COLOR, ARGINT(1));

    return 0;
}

static
int op_set_hair_style(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<map_session_data> c = (ENTITY_TYPE(0) == BL::PC) ? ARGPC(0) : nullptr;

    if (!c)
        return 1;

    pc_changelook(c, LOOK::HAIR, ARGINT(1));

    return 0;
}

static
int op_drop_item_for (dumb_ptr<env_t>, Slice<val_t> args)
{
    Item item;
    int stackable;
    location_t *loc = &ARGLOCATION(0);
    int count = ARGINT(2);
    interval_t interval = static_cast<interval_t>(ARGINT(3));
    dumb_ptr<map_session_data> c = ((args.size() > 4) && (ENTITY_TYPE(4) == BL::PC)) ? ARGPC(4) : nullptr;
    interval_t delay = (args.size() > 5) ? static_cast<interval_t>(ARGINT(5)) : interval_t::zero();
    interval_t delaytime[3] = { delay, delay, delay };
    dumb_ptr<map_session_data> owners[3] = { c, nullptr, nullptr };

    GET_ARG_ITEM(1, item, stackable);

    if (stackable)
    {
        map_addflooritem_any(&item, count, loc->m, loc->x, loc->y,
                owners, delaytime, interval, 0);
    }
    else
    {
        while (count-- > 0)
            map_addflooritem_any(&item, 1, loc->m, loc->x, loc->y,
                    owners, delaytime, interval, 0);
    }

    return 0;
}

static
int op_gain_exp(dumb_ptr<env_t>, Slice<val_t> args)
{
    dumb_ptr<map_session_data> c = (ENTITY_TYPE(0) == BL::PC) ? ARGPC(0) : nullptr;

    if (!c)
        return 1;

    pc_gainexp_reason(c, ARGINT(1), ARGINT(2),
            PC_GAINEXP_REASON(ARGINT(3)));
    return 0;
}

#define MAGIC_OPERATION(name, args, impl) {{name}, {{name}, {args}, impl}}
#define MAGIC_OPERATION1(name, args) MAGIC_OPERATION(#name##_s, args, op_##name)
static
std::map<ZString, op_t> operations =
{
    MAGIC_OPERATION1(sfx, ".ii"_s),
    MAGIC_OPERATION1(instaheal, "eii"_s),
    MAGIC_OPERATION1(itemheal, "eii"_s),
    MAGIC_OPERATION1(shroud, "ei"_s),
    MAGIC_OPERATION("unshroud"_s, "e"_s, op_reveal),
    MAGIC_OPERATION1(message, "es"_s),
    MAGIC_OPERATION1(messenger_npc, "lissi"_s),
    MAGIC_OPERATION1(move, "ed"_s),
    MAGIC_OPERATION1(warp, "el"_s),
    MAGIC_OPERATION1(banish, "e"_s),
    MAGIC_OPERATION1(status_change, "eiiiiii"_s),
    MAGIC_OPERATION1(stop_status_change, "ei"_s),
    MAGIC_OPERATION1(override_attack, "eiiiiii"_s),
    MAGIC_OPERATION1(create_item, "e.i"_s),
    MAGIC_OPERATION1(aggravate, "eie"_s),
    MAGIC_OPERATION1(spawn, "aeiiii"_s),
    MAGIC_OPERATION1(injure, "eeii"_s),
    MAGIC_OPERATION1(emote, "ei"_s),
    MAGIC_OPERATION1(set_script_variable, "esi"_s),
    MAGIC_OPERATION1(set_script_str, "ess"_s),
    MAGIC_OPERATION1(set_hair_colour, "ei"_s),
    MAGIC_OPERATION1(set_hair_style, "ei"_s),
    MAGIC_OPERATION("drop_item"_s, "l.ii"_s, op_drop_item_for),
    MAGIC_OPERATION1(drop_item_for, "l.iiei"_s),
    MAGIC_OPERATION("gain_experience"_s, "eiii"_s, op_gain_exp),
};

op_t *magic_get_op(ZString name)
{
    auto it = operations.find(name);
    if (it == operations.end())
        return nullptr;
    return &it->second;
}

void spell_effect_report_termination(BlockId invocation_id, BlockId bl_id,
        StatusChange sc_id, int)
{
    dumb_ptr<invocation> invocation_ = map_id_is_spell(invocation_id);

    if (!invocation_ || invocation_->bl_type != BL::SPELL)
        return;

    for (status_change_ref_t& cr : invocation_->status_change_refv)
    {
        if (cr.sc_type == sc_id && cr.bl_id == bl_id)
        {
            if (&cr != &invocation_->status_change_refv.back())
                std::swap(cr, invocation_->status_change_refv.back());
            invocation_->status_change_refv.pop_back();

            try_to_finish_invocation(invocation_);
            return;
        }
    }

    {
        dumb_ptr<block_list> entity = map_id2bl(bl_id);
        if (entity->bl_type == BL::PC)
            FPRINTF(stderr,
                    "[magic] INTERNAL ERROR: spell-effect-report-termination:  tried to terminate on unexpected bl %d, sc %d\n"_fmt,
                    bl_id, sc_id);
        return;
    }

}

static
dumb_ptr<effect_t> return_to_stack(dumb_ptr<invocation> invocation_)
{
    if (!invocation_->stack_size)
        return nullptr;
    else
    {
        cont_activation_record_t *ar =
            invocation_->stack + (invocation_->stack_size - 1);
        switch (ar->ty)
        {
            case CONT_STACK::PROC:
            {
                dumb_ptr<effect_t> ret = ar->return_location;
                for (int i = 0; i < ar->c.c_proc.args_nr; i++)
                {
                    val_t *var =
                        &invocation_->env->varu[ar->c.c_proc.formalap[i]];
                    magic_clear_var(var);
                    *var = ar->c.c_proc.old_actualpa[i];
                }

                // pop the stack
                clear_activation_record(ar);
                --invocation_->stack_size;

                return ret;
            }

            case CONT_STACK::FOREACH:
            {
                BlockId entity_id;
                val_t *var = &invocation_->env->varu[ar->c.c_foreach.id];

                do
                {
                    // This >= is really supposed to be a ==, but
                    // I have no clue if it's actually safe to change it.
                    if (ar->c.c_foreach.index >= ar->c.c_foreach.entities_vp->size())
                    {
                        // pop the stack
                        dumb_ptr<effect_t> ret = ar->return_location;
                        clear_activation_record(ar);
                        --invocation_->stack_size;
                        return ret;
                    }

                    entity_id =
                        (*ar->c.c_foreach.entities_vp)[ar->c.c_foreach.index++];
                }
                while (!entity_id || !map_id2bl(entity_id));

                magic_clear_var(var);
                var->ty = ar->c.c_foreach.ty;
                var->v.v_int = static_cast<int32_t>(unwrap<BlockId>(entity_id));

                return ar->c.c_foreach.body;
            }

            case CONT_STACK::FOR:
                if (ar->c.c_for.current > ar->c.c_for.stop)
                {
                    dumb_ptr<effect_t> ret = ar->return_location;
                    // pop the stack
                    clear_activation_record(ar);
                    --invocation_->stack_size;
                    return ret;
                }

                magic_clear_var(&invocation_->env->varu[ar->c.c_for.id]);
                invocation_->env->varu[ar->c.c_for.id].ty = TYPE::INT;
                invocation_->env->varu[ar->c.c_for.id].v.v_int =
                    ar->c.c_for.current++;

                return ar->c.c_for.body;

            default:
                FPRINTF(stderr,
                        "[magic] INTERNAL ERROR: While executing spell `%s':  stack corruption\n"_fmt,
                        invocation_->spell->name);
                return nullptr;
        }
    }
}

static
cont_activation_record_t *add_stack_entry(dumb_ptr<invocation> invocation_,
        CONT_STACK ty, dumb_ptr<effect_t> return_location)
{
    cont_activation_record_t *ar =
        invocation_->stack + invocation_->stack_size++;
    if (invocation_->stack_size >= MAX_STACK_SIZE)
    {
        FPRINTF(stderr,
                "[magic] Execution stack size exceeded in spell `%s'; truncating effect\n"_fmt,
                invocation_->spell->name);
        invocation_->stack_size--;
        return nullptr;
    }

    ar->ty = ty;
    ar->return_location = return_location;
    return ar;
}

static
void find_entities_in_area_c(dumb_ptr<block_list> target,
        std::vector<BlockId> *entities_vp,
        FOREACH_FILTER filter)
{
    switch (target->bl_type)
    {

        case BL::PC:
            if (filter == FOREACH_FILTER::PC
                || filter == FOREACH_FILTER::ENTITY
                || (filter == FOREACH_FILTER::TARGET
                    && target->bl_m->flag.get(MapFlag::PVP)))
                break;
            else if (filter == FOREACH_FILTER::SPELL)
            {                   /* Check all spells bound to the caster */
                dumb_ptr<invocation> invoc = target->is_player()->active_spells;
                /* Add all spells locked onto thie PC */

                while (invoc)
                {
                    entities_vp->push_back(invoc->bl_id);
                    invoc = invoc->next_invocation;
                }
            }
            return;

        case BL::MOB:
            if (filter == FOREACH_FILTER::MOB
                || filter == FOREACH_FILTER::ENTITY
                || filter == FOREACH_FILTER::TARGET)
                break;
            else
                return;

        case BL::SPELL:
            if (filter == FOREACH_FILTER::SPELL)
            {
                dumb_ptr<invocation> invocation = target->is_spell();

                /* Check whether the spell is `bound'-- if so, we'll consider it iff we see the caster(case BL::PC). */
                if (bool(invocation->flags & INVOCATION_FLAG::BOUND))
                    return;
                else
                    break;      /* Add the spell */
            }
            else
                return;

        case BL::NPC:
            if (filter == FOREACH_FILTER::NPC)
                break;
            else
                return;

        default:
            return;
    }

    entities_vp->push_back(target->bl_id);
}

static
void find_entities_in_area(area_t& area_,
        std::vector<BlockId> *entities_vp,
        FOREACH_FILTER filter)
{
    area_t *area = &area_; // temporary hack to "keep diff small". Heh.
    switch (area->ty)
    {
        case AREA::UNION:
            find_entities_in_area(*area->a.a_union[0], entities_vp, filter);
            find_entities_in_area(*area->a.a_union[1], entities_vp, filter);
            break;

        default:
        {
            map_local *m;
            int x, y, width, height;
            magic_area_rect(&m, &x, &y, &width, &height, *area);
            map_foreachinarea(std::bind(find_entities_in_area_c, ph::_1, entities_vp, filter),
                    m,
                    x, y,
                    x + width, y + height,
                    BL::NUL /* filter elsewhere */);
        }
    }
}

static
dumb_ptr<effect_t> run_foreach(dumb_ptr<invocation> invocation,
        dumb_ptr<effect_t> foreach,
        dumb_ptr<effect_t> return_location)
{
    val_t area;
    FOREACH_FILTER filter = foreach->e.e_foreach.filter;
    int id = foreach->e.e_foreach.id;
    dumb_ptr<effect_t> body = foreach->e.e_foreach.body;

    magic_eval(invocation->env, &area, foreach->e.e_foreach.area);

    if (area.ty != TYPE::AREA)
    {
        magic_clear_var(&area);
        FPRINTF(stderr,
                "[magic] Error in spell `%s':  FOREACH loop over non-area\n"_fmt,
                invocation->spell->name);
        return return_location;
    }
    else
    {
        cont_activation_record_t *ar =
            add_stack_entry(invocation, CONT_STACK::FOREACH, return_location);

        if (!ar)
            return return_location;

        std::vector<BlockId> entities_v;
        find_entities_in_area(*area.v.v_area,
                &entities_v, filter);
        entities_v.shrink_to_fit();
        // iterator_pair will go away when this gets properly containerized.
        random_::shuffle(entities_v);

        ar->c.c_foreach.id = id;
        ar->c.c_foreach.body = body;
        ar->c.c_foreach.index = 0;
        ar->c.c_foreach.entities_vp.new_(std::move(entities_v));
        ar->c.c_foreach.ty =
            (filter == FOREACH_FILTER::SPELL) ? TYPE::INVOCATION : TYPE::ENTITY;

        magic_clear_var(&area);

        return return_to_stack(invocation);
    }
}

static
dumb_ptr<effect_t> run_for (dumb_ptr<invocation> invocation,
        dumb_ptr<effect_t> for_,
        dumb_ptr<effect_t> return_location)
{
    cont_activation_record_t *ar;
    int id = for_->e.e_for.id;
    val_t start;
    val_t stop;

    magic_eval(invocation->env, &start, for_->e.e_for.start);
    magic_eval(invocation->env, &stop, for_->e.e_for.stop);

    if (start.ty != TYPE::INT || stop.ty != TYPE::INT)
    {
        magic_clear_var(&start);
        magic_clear_var(&stop);
        FPRINTF(stderr,
                "[magic] Error in spell `%s':  FOR loop start or stop point is not an integer\n"_fmt,
                invocation->spell->name);
        return return_location;
    }

    ar = add_stack_entry(invocation, CONT_STACK::FOR, return_location);

    if (!ar)
        return return_location;

    ar->c.c_for.id = id;
    ar->c.c_for.current = start.v.v_int;
    ar->c.c_for.stop = stop.v.v_int;
    ar->c.c_for.body = for_->e.e_for.body;

    return return_to_stack(invocation);
}

static
dumb_ptr<effect_t> run_call(dumb_ptr<invocation> invocation,
        dumb_ptr<effect_t> return_location)
{
    dumb_ptr<effect_t> current = invocation->current_effect;
    cont_activation_record_t *ar;
    int args_nr = current->e.e_call.formalv->size();
    int *formals = current->e.e_call.formalv->data();
    auto old_actuals = dumb_ptr<val_t[]>::make(args_nr);

    ar = add_stack_entry(invocation, CONT_STACK::PROC, return_location);
    ar->c.c_proc.args_nr = args_nr;
    ar->c.c_proc.formalap = formals;
    ar->c.c_proc.old_actualpa = old_actuals;
    for (int i = 0; i < args_nr; i++)
    {
        val_t *env_val = &invocation->env->varu[formals[i]];
        magic_copy_var(&old_actuals[i], env_val);
        magic_eval(invocation->env, env_val, (*current->e.e_call.actualvp)[i]);
    }

    return current->e.e_call.body;
}

#ifdef DEBUG
static
void print_cfg(int i, effect_t *e)
{
    int j;
    for (j = 0; j < i; j++)
        PRINTF("    "_fmt);

    PRINTF("%p: "_fmt, e);

    if (!e)
    {
        puts(" -- end --");
        return;
    }

    switch (e->ty)
    {
        case EFFECT::SKIP:
            puts("SKIP");
            break;
        case EFFECT::END:
            puts("END");
            break;
        case EFFECT::ABORT:
            puts("ABORT");
            break;
        case EFFECT::ASSIGN:
            puts("ASSIGN");
            break;
        case EFFECT::FOREACH:
            puts("FOREACH");
            print_cfg(i + 1, e->e.e_foreach.body);
            break;
        case EFFECT::FOR:
            puts("FOR");
            print_cfg(i + 1, e->e.e_for.body);
            break;
        case EFFECT::IF:
            puts("IF");
            for (j = 0; j < i; j++)
                PRINTF("    "_fmt);
            puts("THEN");
            print_cfg(i + 1, e->e.e_if.true_branch);
            for (j = 0; j < i; j++)
                PRINTF("    "_fmt);
            puts("ELSE");
            print_cfg(i + 1, e->e.e_if.false_branch);
            break;
        case EFFECT::SLEEP:
            puts("SLEEP");
            break;
        case EFFECT::SCRIPT:
            puts("NpcSubtype::SCRIPT");
            break;
        case EFFECT::BREAK:
            puts("BREAK");
            break;
        case EFFECT::OP:
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
 * Returns: 0 if finished (all memory is freed implicitly)
 *          >1 if we hit `sleep'; the result is the number of ticks we should sleep for.
 *          -1 if we paused to wait for a user action (via script interaction)
 */
static
interval_t spell_run(dumb_ptr<invocation> invocation_, int allow_delete)
{
    const BlockId invocation_id = invocation_->bl_id;
#define REFRESH_INVOCATION invocation_ = map_id_is_spell(invocation_id); if (!invocation_) return interval_t::zero();

#ifdef DEBUG
    FPRINTF(stderr, "Resuming execution:  invocation of `%s'\n"_fmt,
            invocation_->spell->name);
    print_cfg(1, invocation_->current_effect);
#endif
    while (invocation_->current_effect)
    {
        dumb_ptr<effect_t> e = invocation_->current_effect;
        dumb_ptr<effect_t> next = e->next;
        int i;

#ifdef DEBUG
        FPRINTF(stderr, "Next step of type %d\n"_fmt, e->ty);
        dump_env(invocation_->env);
#endif

        switch (e->ty)
        {
            case EFFECT::SKIP:
                break;

            case EFFECT::ABORT:
                invocation_->flags |= INVOCATION_FLAG::ABORTED;
                invocation_->end_effect = nullptr;
                FALLTHROUGH;
            case EFFECT::END:
                clear_stack(invocation_);
                next = nullptr;
                break;

            case EFFECT::ASSIGN:
                magic_eval(invocation_->env,
                            &invocation_->env->varu[e->e.e_assign.id],
                            e->e.e_assign.expr);
                break;

            case EFFECT::FOREACH:
                next = run_foreach(invocation_, e, next);
                break;

            case EFFECT::FOR:
                next = run_for (invocation_, e, next);
                break;

            case EFFECT::IF:
                if (magic_eval_int(invocation_->env, e->e.e_if.cond))
                    next = e->e.e_if.true_branch;
                else
                    next = e->e.e_if.false_branch;
                break;

            case EFFECT::SLEEP:
            {
                interval_t sleeptime = static_cast<interval_t>(
                        magic_eval_int(invocation_->env, e->e.e_sleep));
                invocation_->current_effect = next;
                if (sleeptime > interval_t::zero())
                    return sleeptime;
                break;
            }

            case EFFECT::SCRIPT:
            {
                dumb_ptr<map_session_data> caster = map_id_is_player(invocation_->caster);
                if (caster)
                {
                    dumb_ptr<env_t> env = invocation_->env;
                    ZString caster_name = (caster ? caster->status_key.name : CharName()).to__actual();
                    argrec_t arg[3] =
                    {
                        {"@target"_s, env->VAR(VAR_TARGET).ty == TYPE::ENTITY ? 0 : env->VAR(VAR_TARGET).v.v_int},
                        {"@caster"_s, static_cast<int32_t>(unwrap<BlockId>(invocation_->caster))},
                        {"@caster_name$"_s, caster_name},
                    };
                    BlockId message_recipient =
                        env->VAR(VAR_SCRIPTTARGET).ty == TYPE::ENTITY
                        ? wrap<BlockId>(static_cast<uint32_t>(env->VAR(VAR_SCRIPTTARGET).v.v_int))
                        : invocation_->caster;
                    dumb_ptr<map_session_data> recipient = map_id_is_player(message_recipient);

                    if (recipient->npc_id
                        && recipient->npc_id != invocation_->bl_id)
                        break;  /* Don't send multiple message boxes at once */

                    if (!invocation_->script_pos)    // first time running this script?
                        clif_spawn_fake_npc_for_player(recipient,
                                invocation_->bl_id);
                    // We have to do this or otherwise the client won't think that it's
                    // dealing with an NPC

                    int newpos = run_script_l(
                            ScriptPointer(&*e->e.e_script, invocation_->script_pos),
                            message_recipient, invocation_->bl_id,
                            arg);
                    /* Returns the new script position, or -1 once the script is finished */
                    if (newpos != -1)
                    {
                        /* Must set up for continuation */
                        recipient->npc_id = invocation_->bl_id;
                        recipient->npc_pos = invocation_->script_pos = newpos;
                        return static_cast<interval_t>(-1);  /* Signal `wait for script' */
                    }
                    else
                        invocation_->script_pos = 0;
                    clif_clearchar_id(invocation_->bl_id, BeingRemoveWhy::DEAD, caster->sess);
                }
                REFRESH_INVOCATION; // Script may have killed the caster
                break;
            }

            case EFFECT::BREAK:
                next = return_to_stack(invocation_);
                break;

            case EFFECT::OP:
            {
                op_t *op = e->e.e_op.opp;
                val_t args[MAX_ARGS];

                for (i = 0; i < e->e.e_op.args_nr; i++)
                    magic_eval(invocation_->env, &args[i], e->e.e_op.args[i]);

                if (!magic_signature_check("effect"_s, op->name, op->signature,
                                            Slice<val_t>(args, e->e.e_op.args_nr),
                                            e->e.e_op.line_nr,
                                            e->e.e_op.column))
                    op->op(invocation_->env, Slice<val_t>(args, e->e.e_op.args_nr));

                for (i = 0; i < e->e.e_op.args_nr; i++)
                    magic_clear_var(&args[i]);

                REFRESH_INVOCATION; // Effect may have killed the caster
                break;
            }

            case EFFECT::CALL:
                next = run_call(invocation_, next);
                break;

            default:
                FPRINTF(stderr,
                        "[magic] INTERNAL ERROR: Unknown effect %d\n"_fmt,
                        e->ty);
        }

        if (!next)
            next = return_to_stack(invocation_);

        invocation_->current_effect = next;
    }

    if (allow_delete)
        try_to_finish_invocation(invocation_);
    return interval_t::zero();
#undef REFRESH_INVOCATION
}

static
void spell_execute_d(dumb_ptr<invocation> invocation, int allow_deletion)
{
    spell_update_location(invocation);
    interval_t delta = spell_run(invocation, allow_deletion);

    if (delta > interval_t::zero())
    {
        assert (!invocation->timer);
        invocation->timer = Timer(gettick() + delta,
                std::bind(invocation_timer_callback, ph::_1, ph::_2,
                    invocation->bl_id));
    }

    /* If 0, the script cleaned itself.  If -1(wait-for-script), we must wait for the user. */
}

void spell_execute(dumb_ptr<invocation> invocation)
{
    spell_execute_d(invocation, 1);
}

void spell_execute_script(dumb_ptr<invocation> invocation)
{
    if (invocation->script_pos)
        spell_execute_d(invocation, 1);
    /* Otherwise the script-within-the-spell has been terminated by some other means.
     * In practice this happens when the script doesn't wait for user input: the client
     * may still notify the server that it's done.  Without the above check, we'd be
     * running the same spell twice! */
}

int spell_attack(BlockId caster_id, BlockId target_id)
{
    dumb_ptr<map_session_data> caster = map_id_is_player(caster_id);
    dumb_ptr<invocation> invocation_;
    int stop_attack = 0;

    if (!caster)
        return 0;

    invocation_ = map_id_is_spell(caster->attack_spell_override);

    if (invocation_ && bool(invocation_->flags & INVOCATION_FLAG::STOPATTACK))
        stop_attack = 1;

    if (invocation_ && caster->attack_spell_charges > 0)
    {
        magic_clear_var(&invocation_->env->varu[VAR_TARGET]);
        invocation_->env->varu[VAR_TARGET].ty = TYPE::ENTITY;
        invocation_->env->varu[VAR_TARGET].v.v_int = static_cast<int32_t>(unwrap<BlockId>(target_id));

        invocation_->current_effect = invocation_->trigger_effect;
        invocation_->flags &= ~INVOCATION_FLAG::ABORTED;
        spell_execute_d(invocation_,
                         0 /* don't delete the invocation if done */ );

        // If the caster died, we need to refresh here:
        invocation_ = map_id_is_spell(caster->attack_spell_override);

        if (invocation_ && !bool(invocation_->flags & INVOCATION_FLAG::ABORTED))   // If we didn't abort:
            caster->attack_spell_charges--;
    }

    if (invocation_ && caster->attack_spell_override != invocation_->bl_id)
    {
        /* Attack spell changed / was refreshed */
        // spell_free_invocation(invocation); // [Fate] This would be a double free.
    }
    else if (!invocation_ || caster->attack_spell_charges <= 0)
    {
        caster->attack_spell_override = BlockId();
        char_set_weapon_icon(caster, 0, StatusChange::ZERO, ItemNameId());
        char_set_attack_info(caster, interval_t::zero(), 0);

        if (stop_attack)
            pc_stopattack(caster);

        if (invocation_)
            spell_free_invocation(invocation_);
    }

    return 1;
}
} // namespace tmwa
