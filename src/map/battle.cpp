#include "battle.hpp"

#include "../common/mt_rand.hpp"
#include "../common/nullpo.hpp"
#include "../common/utils.hpp"

#include "clif.hpp"
#include "main.hpp"
#include "mob.hpp"
#include "path.hpp"
#include "pc.hpp"
#include "skill.hpp"

static struct Damage battle_calc_weapon_attack(BlockList *bl, BlockList *target);
static sint32 battle_calc_damage(BlockList *target, sint32 damage, sint32 div_, BattleFlags flag);

static party_t battle_get_party_id(BlockList *bl);
static sint32 battle_get_race(BlockList *bl);
static MobMode battle_get_mode(BlockList *bl);

static sint32 battle_attr_fix(sint32 damage, sint32 atk_elem, sint32 def_elem);
static sint32 battle_stopattack(BlockList *bl);
static sint32 battle_get_hit(BlockList *bl);
static sint32 battle_get_flee(BlockList *bl);
static sint32 battle_get_flee2(BlockList *bl);
static sint32 battle_get_def2(BlockList *bl);
static sint32 battle_get_baseatk(BlockList *bl);
static sint32 battle_get_atk(BlockList *bl);
static sint32 battle_get_atk2(BlockList *bl);
static sint32 battle_get_attack_element(BlockList *bl);
static sint32 battle_get_attack_element2(BlockList *bl);

/// Table of elemental damage modifiers
sint32 attr_fix_table[4][10][10];

struct Battle_Config battle_config;

// There are lots of battle_get_foo(BlockList *) entries
// to hide the differences between PCs and MOBs.
// In addition, there are some effects

/// Count the number of beings attacking this being, that are at least the given level
static sint32 battle_counttargeted(BlockList *bl, BlockList *src, AttackResult target_lv)
{
    nullpo_ret(bl);

    if (bl->type == BL_PC)
        return pc_counttargeted(static_cast<MapSessionData *>(bl), src, target_lv);
    if (bl->type == BL_MOB)
        return mob_counttargeted(static_cast<struct mob_data *>(bl), src, target_lv);
    return 0;
}

/// which way the object is facing
Direction battle_get_dir(BlockList *bl)
{
    nullpo_retr(Direction::S, bl);

    if (bl->type == BL_MOB)
        return static_cast<struct mob_data *>(bl)->dir;
    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->dir;
    return Direction::S;
}

/// Get the (base) level of this being
level_t battle_get_level(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_MOB)
        return level_t(static_cast<struct mob_data *>(bl)->stats[MOB_LV]);
    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->status.base_level;
    return DEFAULT;
}

/// Get the maximum attack distance of this being
sint32 battle_get_range(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_MOB)
        return mob_db[static_cast<struct mob_data *>(bl)->mob_class].range;
    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->attackrange;
    return 0;
}

/// Get current HP of this being
sint32 battle_get_hp(BlockList *bl)
{
    nullpo_retr(1, bl);

    if (bl->type == BL_MOB)
        return static_cast<struct mob_data *>(bl)->hp;
    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->status.hp;
    return 1;
}

/// Get maximum HP of this being
sint32 battle_get_max_hp(BlockList *bl)
{
    nullpo_retr(1, bl);

    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->status.max_hp;
    if (bl->type != BL_MOB)
        return 1;

    sint32 max_hp = static_cast<struct mob_data *>(bl)->stats[MOB_MAX_HP];
    percent_adjust(max_hp, battle_config.monster_hp_rate);
    return max(1, max_hp);
}

/// Get strength of a being
sint32 battle_get_str(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_MOB)
        return max(0, static_cast<struct mob_data *>(bl)->stats[MOB_STR]);
    if (bl->type == BL_PC)
        return max(0, static_cast<MapSessionData *>(bl)->paramc[ATTR::STR]);
    return 0;
}

/// Get agility of a being
sint32 battle_get_agi(BlockList *bl)
{
    nullpo_ret(bl);
    if (bl->type == BL_MOB)
        return max(0, static_cast<struct mob_data *>(bl)->stats[MOB_AGI]);
    if (bl->type == BL_PC)
        return max(0, static_cast<MapSessionData *>(bl)->paramc[ATTR::AGI]);
    return 0;
}

/// Get vitality of a being
sint32 battle_get_vit(BlockList *bl)
{
    nullpo_ret(bl);
    if (bl->type == BL_MOB)
        return max(0, static_cast<struct mob_data *>(bl)->stats[MOB_VIT]);
    if (bl->type == BL_PC)
        return max(0, static_cast<MapSessionData *>(bl)->paramc[ATTR::VIT]);
    return 0;
}

/// Get intelligence of a being
sint32 battle_get_int(BlockList *bl)
{
    nullpo_ret(bl);
    if (bl->type == BL_MOB)
        return max(0, static_cast<struct mob_data *>(bl)->stats[MOB_INT]);
    if (bl->type == BL_PC)
        return max(0, static_cast<MapSessionData *>(bl)->paramc[ATTR::INT]);
    return 0;
}

/// Get dexterity of a being
sint32 battle_get_dex(BlockList *bl)
{
    nullpo_ret(bl);
    if (bl->type == BL_MOB)
        return max(0, static_cast<struct mob_data *>(bl)->stats[MOB_DEX]);
    if (bl->type == BL_PC)
        return max(0, static_cast<MapSessionData *>(bl)->paramc[ATTR::DEX]);
    return 0;
}

/// Get luck of a being
sint32 battle_get_luk(BlockList *bl)
{
    nullpo_ret(bl);
    if (bl->type == BL_MOB)
        return max(0, static_cast<struct mob_data *>(bl)->stats[MOB_LUK]);
    if (bl->type == BL_PC)
        return max(0, static_cast<MapSessionData *>(bl)->paramc[ATTR::LUK]);
    return 0;
}

/// Calculate a being's ability to not be hit
sint32 battle_get_flee(BlockList *bl)
{
    nullpo_retr(1, bl);

    sint32 flee;
    if (bl->type == BL_PC)
        flee = static_cast<MapSessionData *>(bl)->flee;
    else
        flee = battle_get_agi(bl) + uint8(battle_get_level(bl));

    if (battle_is_unarmed(bl))
        flee += (skill_power_bl(bl, TMW_BRAWLING) >> 3);
    // +25 for 200
    flee += skill_power_bl(bl, TMW_SPEED) >> 3;

    return max(1, flee);
}

/// Calculate a being's ability to hit something
sint32 battle_get_hit(BlockList *bl)
{
    nullpo_retr(1, bl);

    sint32 hit;
    if (bl->type == BL_PC)
        hit = static_cast<MapSessionData *>(bl)->hit;
    else
        hit = battle_get_dex(bl) + uint8(battle_get_level(bl));

    if (battle_is_unarmed(bl))
        // +12 for 200
        hit += (skill_power_bl(bl, TMW_BRAWLING) >> 4);

    return max(1, hit);
}

/// Calculate a being's luck at not getting hit
sint32 battle_get_flee2(BlockList *bl)
{
    nullpo_retr(1, bl);

    sint32 flee2;
    if (bl->type == BL_PC)
        flee2 = static_cast<MapSessionData *>(bl)->flee2;
    else
        flee2 = battle_get_luk(bl) + 1;

    if (battle_is_unarmed(bl))
        flee2 += (skill_power_bl(bl, TMW_BRAWLING) >> 3);
    // +25 for 200
    flee2 += skill_power_bl(bl, TMW_SPEED) >> 3;

    return max(1, flee2);
}

/// Calculate being's ability to make a critical hit
static sint32 battle_get_critical(BlockList *bl)
{
    nullpo_retr(1, bl);

    if (bl->type == BL_PC)
        // FIXME was this intended?
        return max(1, static_cast<MapSessionData *>(bl)->critical - battle_get_luk(bl));
    return battle_get_luk(bl) * 3 + 1;
}

/// Get a being's base attack strength
sint32 battle_get_baseatk(BlockList *bl)
{
    nullpo_retr(1, bl);

    if (bl->type == BL_PC)
        return max(1, static_cast<sint32>(static_cast<MapSessionData *>(bl)->base_atk));

    sint32 str = battle_get_str(bl);
    sint32 dstr = str / 10;
    return dstr * dstr + str;
}

/// Get minimum attack strength of a PC's main weapon
sint32 battle_get_atk(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_PC)
        return max(0, static_cast<sint32>(static_cast<MapSessionData *>(bl)->watk));
    if (bl->type == BL_MOB)
        return max(0, static_cast<sint32>(static_cast<struct mob_data *>(bl)->stats[MOB_ATK1]));
    return 0;
}

/// Get minimum attack strength of a PC's second weapon
static sint32 battle_get_atk_(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->watk_;
    return 0;
}

/// Get maximum attack strength of a PC's main weapon
sint32 battle_get_atk2(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->watk2;
    if (bl->type != BL_MOB)
        return 0;
    return max(0, static_cast<sint32>(static_cast<struct mob_data *>(bl)->stats[MOB_ATK2]));
}

/// Get maximum attack strength of a PC's second weapon
static sint32 battle_get_atk_2(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->watk_2;
    return 0;
}

/// Get a being's defense
sint32 battle_get_def(BlockList *bl)
{
    nullpo_ret(bl);

    sint32 def = 0;
    if (bl->type == BL_PC)
    {
        def = static_cast<MapSessionData *>(bl)->def;
    }
    if (bl->type == BL_MOB)
    {
        def = static_cast<struct mob_data *>(bl)->stats[MOB_DEF];
    }

    struct status_change *sc_data = battle_get_sc_data(bl);
    if (sc_data)
    {
        if (sc_data[SC_POISON].timer && bl->type != BL_PC)
            percent_adjust(def, 75);
    }
    return max(0, def);
}

/// Get a being's magical defense
sint32 battle_get_mdef(BlockList *bl)
{
    nullpo_ret(bl);

    sint32 mdef = 0;
    if (bl->type == BL_PC)
        mdef = static_cast<MapSessionData *>(bl)->mdef;
    if (bl->type == BL_MOB)
        mdef = static_cast<struct mob_data *>(bl)->stats[MOB_MDEF];

    struct status_change *sc_data = battle_get_sc_data(bl);
    if (sc_data)
    {
        if (mdef < 90 && sc_data[SC_MBARRIER].timer)
        {
            mdef += sc_data[SC_MBARRIER].val1;
            if (mdef > 90)
                mdef = 90;
        }
    }
    return max(0, mdef);
}

/// Get a being's secondary defense
sint32 battle_get_def2(BlockList *bl)
{
    nullpo_retr(1, bl);

    sint32 def2 = 1;
    if (bl->type == BL_PC)
        def2 = static_cast<MapSessionData *>(bl)->def2;
    if (bl->type == BL_MOB)
        def2 = static_cast<struct mob_data *>(bl)->stats[MOB_VIT];

    struct status_change *sc_data = battle_get_sc_data(bl);
    if (sc_data)
    {
        if (sc_data[SC_POISON].timer && bl->type != BL_PC)
            percent_adjust(def2, 75);
    }
    return max(1, def2);
}

/// Get a being's walk delay
interval_t battle_get_speed(BlockList *bl)
{
    nullpo_retr(std::chrono::seconds(1), bl);

    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->speed;

    if (bl->type == BL_MOB)
        return std::chrono::milliseconds(static_cast<struct mob_data *>(bl)->stats[MOB_SPEED]);
    return std::chrono::seconds(1);
}

/// Get a being's attack delay
interval_t battle_get_adelay(BlockList *bl)
{
    nullpo_retr(std::chrono::seconds(4), bl);

    if (bl->type == BL_PC)
        // this code is not actually executed
        return static_cast<MapSessionData *>(bl)->aspd * 2;

    // TODO: simplify this
    struct status_change *sc_data = battle_get_sc_data(bl);
    interval_t adelay = std::chrono::seconds(4);
    sint32 aspd_rate = 100;
    if (bl->type == BL_MOB)
        adelay = std::chrono::milliseconds(static_cast<struct mob_data *>(bl)->stats[MOB_ADELAY]);

    if (sc_data)
    {
        if (sc_data[SC_SPEEDPOTION0].timer)
            aspd_rate -= sc_data[SC_SPEEDPOTION0].val1;
        // Fate's `haste' spell works the same as the above
        if (sc_data[SC_HASTE].timer)
            aspd_rate -= sc_data[SC_HASTE].val1;
    }

    percent_adjust(adelay, aspd_rate);
    if (adelay < std::chrono::milliseconds(battle_config.monster_max_aspd * 2))
        adelay = std::chrono::milliseconds(battle_config.monster_max_aspd * 2);
    return adelay;
}

/// Being's attack motion rate?
interval_t battle_get_amotion(BlockList *bl)
{
    nullpo_retr(std::chrono::seconds(2), bl);

    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->amotion;
    struct status_change *sc_data = battle_get_sc_data(bl);
    interval_t amotion = std::chrono::seconds(2);
    sint32 aspd_rate = 100;
    if (bl->type == BL_MOB && static_cast<struct mob_data *>(bl))
        amotion = mob_db[static_cast<struct mob_data *>(bl)->mob_class].amotion;

    if (sc_data)
    {
        if (sc_data[SC_SPEEDPOTION0].timer)
            aspd_rate -= sc_data[SC_SPEEDPOTION0].val1;
        if (sc_data[SC_HASTE].timer)
            aspd_rate -= sc_data[SC_HASTE].val1;
    }

    percent_adjust(amotion, aspd_rate);
    if (amotion < std::chrono::milliseconds(battle_config.monster_max_aspd))
        amotion = std::chrono::milliseconds(battle_config.monster_max_aspd);
    return amotion;
}

/// Being's defense motion rate?
interval_t battle_get_dmotion(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_MOB)
    {
        interval_t ret = mob_db[static_cast<struct mob_data *>(bl)->mob_class].dmotion;
        percent_adjust(ret, battle_config.monster_damage_delay_rate);
        return ret;
    }
    if (bl->type == BL_PC)
    {
        interval_t ret = static_cast<MapSessionData *>(bl)->dmotion;
        percent_adjust(ret, battle_config.pc_damage_delay_rate);
        return ret;
    }
    return std::chrono::seconds(2);
}

/// Get a being's (encoded) defense element
sint32 battle_get_element(BlockList *bl)
{
    nullpo_retr(20, bl);

    if (bl->type == BL_MOB)
        return static_cast<struct mob_data *>(bl)->def_ele;
    if (bl->type == BL_PC)
        // This adds 1 level ...
        return 20 + static_cast<MapSessionData *>(bl)->def_ele;
    // 20 = level 1 neutral
    return 20;
}

/// Get a PC's (secondary?) attack element (TODO rewrite element system)
sint32 battle_get_attack_element(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->atk_ele;
    return 0;
}

/// Get a PC's (secondary?) attack element
sint32 battle_get_attack_element2(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->atk_ele_;
    return 0;
}

/// Return a party ID (or fake one) for a being
party_t battle_get_party_id(BlockList *bl)
{
    nullpo_ret(bl);
    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->status.party_id;
    if (bl->type == BL_MOB)
    {
        struct mob_data *md = static_cast<struct mob_data *>(bl);
        if (md->master_id)
            // slave mobs
            return party_t(-uint32(md->master_id));
        // else, it is its own party
        return party_t(-uint32(md->id));
    }
    return DEFAULT;
}

/// Get a being's race
sint32 battle_get_race(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_MOB)
        return mob_db[static_cast<struct mob_data *>(bl)->mob_class].race;
    if (bl->type == BL_PC)
        return 7;
    return 0;
}

/// Return a bitmask of a being's mode.
/// Not all of these are known to be meaningful
// 0x01: can move
// 0x02: looter
// 0x04: aggresive
// 0x08: assist
// 0x10: castsensor
// 0x20: Boss
// 0x40: plant
// 0x80: can attack
// 0x100: detector
// 0x200: changetarget
MobMode battle_get_mode(BlockList *bl)
{
    nullpo_retr(MobMode::CAN_MOVE, bl);

    if (bl->type == BL_MOB)
        return mob_db[static_cast<struct mob_data *>(bl)->mob_class].mode;
    return MobMode::CAN_MOVE;
}

/// stat_id: like SP::VIT
// this is only used by the skill pool system
sint32 battle_get_stat(SP stat_id, BlockList *bl)
{
    switch (stat_id)
    {
    case SP::STR:
        return battle_get_str(bl);
    case SP::AGI:
        return battle_get_agi(bl);
    case SP::DEX:
        return battle_get_dex(bl);
    case SP::VIT:
        return battle_get_vit(bl);
    case SP::INT:
        return battle_get_int(bl);
    case SP::LUK:
        return battle_get_luk(bl);
    default:
        return 0;
    }
}

/// Get the list of status effects on a being
struct status_change *battle_get_sc_data(BlockList *bl)
{
    nullpo_retr(NULL, bl);
    if (bl->type == BL_MOB)
        return static_cast<struct mob_data *>(bl)->sc_data;
    if (bl->type == BL_PC)
        return static_cast<MapSessionData *>(bl)->sc_data;
    return NULL;
}

/// Get pointer to the number of status effects on a being?
sint16 *battle_get_sc_count(BlockList *bl)
{
    nullpo_retr(NULL, bl);

    if (bl->type == BL_MOB)
        return &static_cast<struct mob_data *>(bl)->sc_count;
    if (bl->type == BL_PC)
        return &static_cast<MapSessionData *>(bl)->sc_count;
    return NULL;
}

/// Get a pointer to a being's "opt1" field
sint16 *battle_get_opt1(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_MOB)
        return &static_cast<struct mob_data *>(bl)->opt1;
    if (bl->type == BL_PC && static_cast<MapSessionData *>(bl))
        return &static_cast<MapSessionData *>(bl)->opt1;
    if (bl->type == BL_NPC && static_cast<struct npc_data *>(bl))
        return &static_cast<struct npc_data *>(bl)->opt1;
    return 0;
}

/// Get a pointer to a being's "opt2" field
sint16 *battle_get_opt2(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_MOB)
        return &static_cast<struct mob_data *>(bl)->opt2;
    if (bl->type == BL_PC)
        return &static_cast<MapSessionData *>(bl)->opt2;
    if (bl->type == BL_NPC)
        return &static_cast<struct npc_data *>(bl)->opt2;
    return 0;
}

/// Get a pointer to a being's "opt3" field
sint16 *battle_get_opt3(BlockList *bl)
{
    nullpo_ret(bl);
    if (bl->type == BL_MOB)
        return &static_cast<struct mob_data *>(bl)->opt3;
    if (bl->type == BL_PC)
        return &static_cast<MapSessionData *>(bl)->opt3;
    if (bl->type == BL_NPC)
        return &static_cast<struct npc_data *>(bl)->opt3;
    return 0;
}

/// Get a pointer to a being's "option" field
OPTION *battle_get_option(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_MOB)
        return &static_cast<struct mob_data *>(bl)->option;
    if (bl->type == BL_PC)
        return &static_cast<MapSessionData *>(bl)->status.option;
    if (bl->type == BL_NPC)
        return &static_cast<struct npc_data *>(bl)->option;
    return NULL;
}

//-------------------------------------------------------------------

/// Decrease the HP of target
// if trying to damage by a negative amount, heal instead
sint32 battle_damage(BlockList *src, BlockList *target, sint32 damage)
{
    nullpo_ret(target);
    // src may be NULL for programmatic damage

    if (!damage)
        return 0;
    if (!target->prev)
        return 0;
    if (src && !src->prev)
        return 0;

    if (damage < 0)
        return battle_heal(src, target, -damage, 0);

    if (target->type == BL_MOB)
    {
        struct mob_data *md = static_cast<struct mob_data *>(target);
        return mob_damage(src, md, damage, 0);
    }
    if (target->type == BL_PC)
    {
        MapSessionData *tsd = static_cast<MapSessionData *>(target);
        return pc_damage(src, tsd, damage);
    }
    return 0;
}

/// Increase HP of a being (and SP of a PC)
// if hp is negative, do damage and ignore sp
sint32 battle_heal(BlockList *src, BlockList *target, sint32 hp, sint32 sp)
{
    nullpo_ret(target);
    // src may be NULL for programmatic healing

    if (target->type == BL_PC && pc_isdead(static_cast<MapSessionData *>(target)))
        return 0;
    if (!hp && !sp)
        return 0;

    if (hp < 0)
        return battle_damage(src, target, -hp);

    if (target->type == BL_MOB)
        return mob_heal(static_cast<struct mob_data *>(target), hp);
    if (target->type == BL_PC)
        return pc_heal(static_cast<MapSessionData *>(target), hp, sp);
    return 0;
}

/// A being should stop attacking its target
sint32 battle_stopattack(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_MOB)
        return mob_stopattack(static_cast<struct mob_data *>(bl));
    if (bl->type == BL_PC)
        return pc_stopattack(static_cast<MapSessionData *>(bl));
    return 0;
}

/// Change the amount of elemental damage by the lookup table
sint32 battle_attr_fix(sint32 damage, sint32 atk_elem, sint32 def_elem)
{
    sint32 def_type = def_elem % 10, def_lv = def_elem / 10 / 2;

    if (atk_elem < 0 || atk_elem > 9 || def_type < 0 || def_type > 9 ||
        def_lv < 1 || def_lv > 4)
    {
        map_log("battle_attr_fix: unknown attr type: atk=%d def_type=%d def_lv=%d\n",
               atk_elem, def_type, def_lv);
        return damage;
    }

    percent_adjust(damage, attr_fix_table[def_lv - 1][atk_elem][def_type]);
    return damage;
}

/// Calculate the damage of attacking
sint32 battle_calc_damage(BlockList *target, sint32 damage, sint32 div_, BattleFlags flag)
{
    nullpo_ret(target);

    if (battle_config.skill_min_damage || flag & BattleFlags::MISC)
    {
        if (div_ < 255)
        {
            if (damage > 0 && damage < div_)
                damage = div_;
        }
        else if (damage > 0 && damage < 3)
            damage = 3;
    }
    return damage;
}

/// A monster attacks another being
static struct Damage battle_calc_mob_weapon_attack(struct mob_data *md,
                                                   BlockList *target)
{
    struct Damage wd = {};
    wd.damage2 = 0;
    wd.amotion = battle_get_amotion(md);
    wd.dmotion = battle_get_dmotion(target);
    wd.flag = BattleFlags::SHORT | BattleFlags::WEAPON;
    wd.dmg_lv = AttackResult::ZERO;

    wd.type = 0;                   // normal
    wd.div_ = 1;                   // single attack
    wd.damage = 0;

    MapSessionData *tsd = NULL;
    if (target->type == BL_PC)
        tsd = static_cast<MapSessionData *>(target);
    struct mob_data *tmd = NULL;
    if (target->type == BL_MOB)
        tmd = static_cast<struct mob_data *>(target);

    sint32 flee = battle_get_flee(target);

    if (battle_config.agi_penaly_type == 1)
    {
        sint32 target_count = battle_counttargeted(target, md, static_cast<AttackResult>(battle_config.agi_penaly_count_lv));
        target_count -= battle_config.agi_penaly_count;
        if (target_count > 0)
            percent_subtract(flee, target_count * battle_config.agi_penaly_num);
    }
    if (battle_config.agi_penaly_type == 2)
    {
        sint32 target_count = battle_counttargeted(target, md, static_cast<AttackResult>(battle_config.agi_penaly_count_lv));
        target_count -= battle_config.agi_penaly_count;
        if (target_count > 0)
            flee -= target_count * battle_config.agi_penaly_num;
    }

    if (flee < 1)
        flee = 1;

    if (battle_config.enemy_str)
        wd.damage = battle_get_baseatk(md);

    if (mob_db[md->mob_class].range > 3)
        wd.flag = (wd.flag & ~BattleFlags::RANGE_MASK) | BattleFlags::LONG;

    sint32 atkmin = battle_get_atk(md);
    sint32 atkmax = battle_get_atk2(md);
    if (atkmin > atkmax)
        atkmin = atkmax;

    sint32 cri = battle_get_critical(md);
    cri -= battle_get_luk(target) * 3;
    percent_adjust(cri, battle_config.enemy_critical_rate);
    if (cri < 1)
        cri = 1;
    if (tsd && tsd->critical_def)
        percent_subtract(cri, tsd->critical_def);

    sint32 s_ele = battle_get_attack_element(md);

    if (battle_config.enemy_critical && MRAND(1000) < cri)
    {
        wd.damage += atkmax;
        wd.type = 0x0a;
    }
    else
    {
        wd.damage += atkmin;
        if (atkmax > atkmin)
            wd.damage += MRAND(atkmax + 1 - atkmin);

        sint32 def1 = battle_get_def(target);
        sint32 def2 = battle_get_def2(target);
        sint32 t_vit = battle_get_vit(target);

        if (battle_config.vit_penaly_type == 1)
        {
            sint32 target_count = battle_counttargeted(target, md, static_cast<AttackResult>(battle_config.vit_penaly_count_lv));
            target_count -= battle_config.vit_penaly_count;
            if (target_count > 0)
            {
                percent_subtract(def1, target_count * battle_config.vit_penaly_num);
                percent_subtract(def2, target_count * battle_config.vit_penaly_num);
                percent_subtract(t_vit, target_count * battle_config.vit_penaly_num);
            }
        }
        if (battle_config.vit_penaly_type == 2)
        {
            sint32 target_count = battle_counttargeted(target, md, static_cast<AttackResult>(battle_config.vit_penaly_count_lv));
            target_count -= battle_config.vit_penaly_count;
            if (target_count > 0)
            {
                def1 -= target_count * battle_config.vit_penaly_num;
                def2 -= target_count * battle_config.vit_penaly_num;
                t_vit -= target_count * battle_config.vit_penaly_num;
            }
        }

        if (def1 < 0)
            def1 = 0;
        if (def2 < 1)
            def2 = 1;
        if (t_vit < 1)
            t_vit = 1;

        sint32 t_def = def2 * 8 / 10;

        sint32 vitbonusmax = (t_vit / 20) * (t_vit / 20) - 1;
        if (battle_config.monster_defense_type)
            wd.damage -= def1 * battle_config.monster_defense_type;
        else
            percent_subtract(wd.damage, def1);
        wd.damage -= t_def + (vitbonusmax < 1 ? 0 : MRAND(vitbonusmax + 1));
    }

    if (wd.damage < 1)
        wd.damage = 1;

    sint32 hitrate = battle_get_hit(md) - flee + 80;
    hitrate = hitrate > 95 ? 95 : (hitrate < 5 ? 5 : hitrate);
    if (wd.type == 0 && MRAND(100) >= hitrate)
    {
        wd.damage = 0;
        wd.dmg_lv = AttackResult::FLEE;
    }
    else
    {
        wd.dmg_lv = AttackResult::DEF;
    }

    if (tsd)
    {
        sint32 cardfix = 100;

        if (wd.flag & BattleFlags::LONG)
            percent_subtract(cardfix, tsd->long_attack_def_rate);
        if (wd.flag & BattleFlags::SHORT)
            percent_subtract(cardfix, tsd->near_attack_def_rate);
        percent_adjust(wd.damage, cardfix);
    }

    if (wd.damage < 0)
        wd.damage = 0;

    if (s_ele != 0 || !battle_config.mob_attack_attr_none)
        wd.damage = battle_attr_fix(wd.damage, s_ele, battle_get_element(target));

    if (tsd && MRAND(1000) < battle_get_flee2(target))
    {
        wd.damage = 0;
        wd.type = 0x0b;
        wd.dmg_lv = AttackResult::LUCKY;
    }

    if (tmd && battle_config.enemy_perfect_flee && MRAND(1000) < battle_get_flee2(target))
    {
        wd.damage = 0;
        wd.type = 0x0b;
        wd.dmg_lv = AttackResult::LUCKY;
    }

    if (battle_get_mode(target) & MobMode::PLANT && wd.damage > 0)
        wd.damage = 1;
    if (tsd && tsd->special_state.no_weapon_damage)
        wd.damage = 0;
    wd.damage = battle_calc_damage(target, wd.damage, wd.div_, wd.flag);
    return wd;
}

/// Check if a PC is unarmed
bool battle_is_unarmed(BlockList *bl)
{
    if (!bl || bl->type != BL_PC)
        return 0;
    MapSessionData *sd = static_cast<MapSessionData *>(bl);
    return sd->equip_index[EQUIP::SHIELD] == -1 && sd->equip_index[EQUIP::WEAPON] == -1;
}

/// A PC attacks another being
static struct Damage battle_calc_pc_weapon_attack(MapSessionData *sd,
                                                  BlockList *target)
{
    struct Damage wd = {};
    sd->state.attack_type_is_weapon = true;

    wd.amotion = battle_get_amotion(sd);
    wd.dmotion = battle_get_dmotion(target);
    wd.dmg_lv = AttackResult::ZERO;
    wd.flag = BattleFlags::SHORT | BattleFlags::WEAPON;
    wd.type = 0;                   // normal
    wd.div_ = 1;                   // single attack

    sint32 def1 = battle_get_def(target);
    sint32 def2 = battle_get_def2(target);
    sint32 t_vit = battle_get_vit(target);

    sint32 s_ele = battle_get_attack_element(sd);
    sint32 s_ele_ = battle_get_attack_element2(sd);
    sint32 t_race = battle_get_race(target);
    sint32 t_ele = battle_get_element(target) % 10;
    MobMode t_mode = battle_get_mode(target);

    MapSessionData *tsd = NULL;
    if (target->type == BL_PC)
        tsd = static_cast<MapSessionData *>(target);
    struct mob_data *tmd = NULL;
    if (target->type == BL_MOB)
        tmd = static_cast<struct mob_data *>(target);

    sint32 flee = battle_get_flee(target);
    if (battle_config.agi_penaly_type == 1)
    {
        sint32 target_count = battle_counttargeted(target, sd, static_cast<AttackResult>(battle_config.agi_penaly_count_lv));
        target_count -= battle_config.agi_penaly_count;
        if (target_count > 0)
            percent_subtract(flee, target_count * battle_config.agi_penaly_num);
    }
    if (battle_config.agi_penaly_type == 2)
    {
        sint32 target_count = battle_counttargeted(target, sd, static_cast<AttackResult>(battle_config.agi_penaly_count_lv));
        target_count -= battle_config.agi_penaly_count;
        if (target_count > 0)
            flee -= target_count * battle_config.agi_penaly_num;
    }
    if (flee < 1)
        flee = 1;

    sint32 target_distance = max(abs(sd->x - target->x), abs(sd->y - target->y));
    // NOTE: dividing by 75 means the penalty distance is only decreased by 2
    // even at maximum skill, whereas the range increases every 60, maximum 3
    sint32 malus_dist = max(0, target_distance - skill_power(sd, AC_OWL) / 75);
    sint32 hitrate = battle_get_hit(sd) - flee + 80;
    hitrate -= malus_dist * (malus_dist + 1);

    sint32 dex = battle_get_dex(sd);
    sint32 watk = battle_get_atk(sd);
    sint32 watk_ = battle_get_atk_(sd);

    wd.damage = wd.damage2 = battle_get_baseatk(sd);
    if (sd->attackrange > 2)
    {
        // up to 31.25% bonus for long-range hit
        const sint32 range_damage_bonus = 80;
        per256_add(wd.damage, (range_damage_bonus * target_distance) / sd->attackrange);
        per256_add(wd.damage2, (range_damage_bonus * target_distance) / sd->attackrange);
    }

    sint32 atkmin = dex;
    sint32 atkmin_ = dex;
    sd->state.arrow_atk = 0;
    if (sd->equip_index[EQUIP::WEAPON] >= 0 && sd->inventory_data[sd->equip_index[EQUIP::WEAPON]])
        percent_adjust(atkmin, 80 + sd->inventory_data[sd->equip_index[EQUIP::WEAPON]]->wlv * 20);
    if (sd->equip_index[EQUIP::SHIELD] >= 0 && sd->inventory_data[sd->equip_index[EQUIP::SHIELD]])
        percent_adjust(atkmin_, 80 + sd->inventory_data[sd->equip_index[EQUIP::SHIELD]]->wlv * 20);
    if (sd->status.weapon == 11)
    {
        atkmin = watk * min(atkmin, watk) / 100;
        wd.flag = (wd.flag & ~BattleFlags::RANGE_MASK) | BattleFlags::LONG;
        if (sd->arrow_ele > 0)
            s_ele = sd->arrow_ele;
        sd->state.arrow_atk = 1;
    }

    sint32 atkmax = watk;
    sint32 atkmax_ = watk_;

    if (atkmin > atkmax && !(sd->state.arrow_atk))
        atkmin = atkmax;
    if (atkmin_ > atkmax_)
        atkmin_ = atkmax_;

    /// Double attack?
    bool da = 0;
    if (sd->double_rate > 0)
        da = MRAND(100) < sd->double_rate;

    sint32 cri = 0;
    if (da == 0)
    {
        cri = battle_get_critical(sd);

        if (sd->state.arrow_atk)
            cri += sd->arrow_cri;
        if (sd->status.weapon == 16)
            cri <<= 1;
        cri -= battle_get_luk(target) * 3;
    }

    if (tsd && tsd->critical_def)
        percent_subtract(cri, tsd->critical_def);

    if (da == 0 && MRAND(1000) < cri)
    {
        wd.damage += atkmax;
        wd.damage2 += atkmax_;
        percent_adjust(wd.damage, sd->atk_rate);
        percent_adjust(wd.damage2, sd->atk_rate);
        if (sd->state.arrow_atk)
            wd.damage += sd->arrow_atk;
        wd.type = 0x0a;
    }
    else
    {
        if (atkmax > atkmin)
            wd.damage += atkmin + MRAND(atkmax - atkmin + 1);
        else
            wd.damage += atkmin;
        if (atkmax_ > atkmin_)
            wd.damage2 += atkmin_ + MRAND(atkmax_ - atkmin_ + 1);
        else
            wd.damage2 += atkmin_;
        percent_adjust(wd.damage, sd->atk_rate);
        percent_adjust(wd.damage2, sd->atk_rate);

        if (sd->state.arrow_atk)
        {
            if (sd->arrow_atk > 0)
                wd.damage += MRAND(sd->arrow_atk + 1);
            hitrate += sd->arrow_hit;
        }

        /// Ignore defense? (or, defense already used)
        bool idef_flag = 0, idef_flag_ = 0;
        if (sd->def_ratio_atk_ele & (1 << t_ele) || sd->def_ratio_atk_race & (1 << t_race))
        {
            percent_adjust(wd.damage, def1 + def2);
            idef_flag = 1;
        }
        if (sd->def_ratio_atk_ele_ & (1 << t_ele) || sd->def_ratio_atk_race_ & (1 << t_race))
        {
            percent_adjust(wd.damage2, def1 + def2);
            idef_flag_ = 1;
        }
        if (t_mode & MobMode::BOSS)
        {
            if (!idef_flag && sd->def_ratio_atk_race & (1 << 10))
            {
                percent_adjust(wd.damage, def1 + def2);
                idef_flag = 1;
            }
            if (!idef_flag_ && sd->def_ratio_atk_race_ & (1 << 10))
            {
                percent_adjust(wd.damage2, def1 + def2);
                idef_flag_ = 1;
            }
        }
        else
        {
            if (!idef_flag && sd->def_ratio_atk_race & (1 << 11))
            {
                percent_adjust(wd.damage, def1 + def2);
                idef_flag = 1;
            }
            if (!idef_flag_ && sd->def_ratio_atk_race_ & (1 << 11))
            {
                percent_adjust(wd.damage2, def1 + def2);
                idef_flag_ = 1;
            }
        }

        if (battle_config.vit_penaly_type == 1)
        {
            sint32 target_count = battle_counttargeted(target, sd, static_cast<AttackResult>(battle_config.vit_penaly_count_lv));
            target_count -= battle_config.vit_penaly_count;
            if (target_count > 0)
            {
                percent_subtract(def1, target_count * battle_config.vit_penaly_num);
                percent_subtract(def2, target_count * battle_config.vit_penaly_num);
                percent_subtract(t_vit, target_count * battle_config.vit_penaly_num);
            }
        }
        if (battle_config.vit_penaly_type == 2)
        {
            sint32 target_count = battle_counttargeted(target, sd, static_cast<AttackResult>(battle_config.vit_penaly_count_lv));
            target_count -= battle_config.vit_penaly_count;
            if (target_count > 0)
            {
                def1 -= target_count * battle_config.vit_penaly_num;
                def2 -= target_count * battle_config.vit_penaly_num;
                t_vit -= target_count * battle_config.vit_penaly_num;
            }
        }
        if (def1 < 0)
            def1 = 0;
        if (def2 < 1)
            def2 = 1;
        if (t_vit < 1)
            t_vit = 1;

        sint32 t_def = def2 * 8 / 10;
        sint32 vitbonusmax = (t_vit / 20) * (t_vit / 20) - 1;
        if (sd->ignore_def_ele & (1 << t_ele) || sd->ignore_def_race & (1 << t_race))
            idef_flag = 1;
        if (sd->ignore_def_ele_ & (1 << t_ele) || sd->ignore_def_race_ & (1 << t_race))
            idef_flag_ = 1;
        if (t_mode & MobMode::BOSS)
        {
            if (sd->ignore_def_race & (1 << 10))
                idef_flag = 1;
            if (sd->ignore_def_race_ & (1 << 10))
                idef_flag_ = 1;
        }
        else
        {
            if (sd->ignore_def_race & (1 << 11))
                idef_flag = 1;
            if (sd->ignore_def_race_ & (1 << 11))
                idef_flag_ = 1;
        }

        if (!idef_flag)
        {
            if (battle_config.player_defense_type)
                wd.damage -= def1 * battle_config.player_defense_type;
            else
                percent_subtract(wd.damage, def1);
            wd.damage -= t_def + (vitbonusmax < 1 ? 0 : MRAND(vitbonusmax + 1));
        }
        if (!idef_flag_)
        {
            if (battle_config.player_defense_type)
                wd.damage2 -= def1 * battle_config.player_defense_type;
            else
                percent_subtract(wd.damage2, def1);
            wd.damage2 -= t_def + (vitbonusmax < 1 ? 0 : MRAND(vitbonusmax + 1));
        }
    }

    wd.damage += battle_get_atk2(sd);
    wd.damage2 += battle_get_atk_2(sd);

    if (wd.damage < 1)
        wd.damage = 1;
    if (wd.damage2 < 1)
        wd.damage2 = 1;

    if (sd->perfect_hit > 0)
    {
        if (MRAND(100) < sd->perfect_hit)
            hitrate = 100;
    }

    hitrate = (hitrate < 5) ? 5 : hitrate;
    if (wd.type == 0 && MRAND(100) >= hitrate)
    {
        wd.damage = wd.damage2 = 0;
        wd.dmg_lv = AttackResult::FLEE;
    }
    else
    {
        wd.dmg_lv = AttackResult::DEF;
    }

    if (wd.damage < 0)
        wd.damage = 0;
    if (wd.damage2 < 0)
        wd.damage2 = 0;

    wd.damage = battle_attr_fix(wd.damage, s_ele, battle_get_element(target));
    wd.damage2 = battle_attr_fix(wd.damage2, s_ele_, battle_get_element(target));

    wd.damage += sd->star;
    wd.damage2 += sd->star_;

    if (sd->weapontype1 == 0 && sd->weapontype2 > 0)
    {
        wd.damage = wd.damage2;
        wd.damage2 = 0;
    }

    if (sd->status.weapon > 16)
    {
        sint32 dmg = wd.damage, dmg2 = wd.damage2;
        wd.damage = wd.damage * 50 / 100;
        if (dmg > 0 && wd.damage < 1)
            wd.damage = 1;
        wd.damage2 = wd.damage2 * 30 / 100;
        if (dmg2 > 0 && wd.damage2 < 1)
            wd.damage2 = 1;
    }
    else
        wd.damage2 = 0;

    if (da == 1)
    {
        /// double attack
        wd.div_ = 2;
        wd.damage += wd.damage;
        wd.type = 0x08;
    }

    if (sd->status.weapon == 16)
    {
        wd.damage2 = wd.damage / 100;
        if (wd.damage > 0 && wd.damage2 < 1)
            wd.damage2 = 1;
    }

    if (tsd && wd.div_ < 255 && MRAND(1000) < battle_get_flee2(target))
    {
        wd.damage = wd.damage2 = 0;
        wd.type = 0x0b;
        wd.dmg_lv = AttackResult::LUCKY;
    }

    if (battle_config.enemy_perfect_flee)
    {
        if (tmd && wd.div_ < 255 && MRAND(1000) < battle_get_flee2(target))
        {
            wd.damage = wd.damage2 = 0;
            wd.type = 0x0b;
            wd.dmg_lv = AttackResult::LUCKY;
        }
    }

    // plant flag, but the comment said "robust"
    if (t_mode & MobMode::PLANT)
    {
        if (wd.damage > 0)
            wd.damage = 1;
        if (wd.damage2 > 0)
            wd.damage2 = 1;
    }

    if (tsd && tsd->special_state.no_weapon_damage)
        wd.damage = wd.damage2 = 0;

    if (wd.damage > 0 || wd.damage2 > 0)
    {
        if (wd.damage2 < 1)
            wd.damage = battle_calc_damage(target, wd.damage, wd.div_, wd.flag);
        else if (wd.damage < 1)
            wd.damage2 = battle_calc_damage(target, wd.damage2, wd.div_, wd.flag);
        else
        {
            sint32 d1 = wd.damage + wd.damage2, d2 = wd.damage2;
            wd.damage = battle_calc_damage(target, d1, wd.div_, wd.flag);
            wd.damage2 = (d2 * 100 / d1) * wd.damage / 100;
            if (wd.damage > 1 && wd.damage2 < 1)
                wd.damage2 = 1;
            wd.damage -= wd.damage2;
        }
    }

    if (sd->random_attack_increase_add > 0 && sd->random_attack_increase_per > 0)
    {
        if (MRAND(100) < sd->random_attack_increase_per)
        {
            percent_adjust(wd.damage, sd->random_attack_increase_add);
            percent_adjust(wd.damage2, sd->random_attack_increase_add);
        }
    }

    return wd;
}

/// Calculate damage of one being attacking another
struct Damage battle_calc_weapon_attack(BlockList *src, BlockList *target)
{
    struct Damage wd = {};
    nullpo_retr(wd, src);
    nullpo_retr(wd, target);

    MapSessionData *sd = NULL;
    struct mob_data *md = NULL;
    if (src->type == BL_PC)
    {
        sd = static_cast<MapSessionData *>(src);
        wd = battle_calc_pc_weapon_attack(sd, target);
    }
    else if (src->type == BL_MOB)
    {
        md = static_cast<struct mob_data *>(src);
        wd = battle_calc_mob_weapon_attack(md, target);
    }

    return wd;
}

/// One being attacks another at some time
AttackResult battle_weapon_attack(BlockList *src, BlockList *target, tick_t tick)
{
    nullpo_retr(AttackResult::ZERO, src);
    nullpo_retr(AttackResult::ZERO, target);

    MapSessionData *sd = NULL;
    if (src->type == BL_PC)
        sd = static_cast<MapSessionData *>(src);

    if (src->prev == NULL || target->prev == NULL)
        return AttackResult::ZERO;
    if (sd && pc_isdead(sd))
        return AttackResult::ZERO;
    MapSessionData *tsd = NULL;
    if (target->type == BL_PC)
        tsd = static_cast<MapSessionData *>(target);
    if (tsd && pc_isdead(tsd))
        return AttackResult::ZERO;

    sint16 *opt1 = battle_get_opt1(src);
    if (opt1 && *opt1 > 0)
    {
        battle_stopattack(src);
        return AttackResult::ZERO;
    }

    if (!battle_check_target(src, target) || !battle_check_range(src, target, 0))
        return AttackResult::ZERO;

    if (sd && sd->status.weapon == 11)
    {
        if (sd->equip_index[EQUIP::ARROW] >= 0)
        {
            pc_delitem(sd, sd->equip_index[EQUIP::ARROW], 1, 0);
        }
        else
        {
            clif_arrow_fail(sd, ArrowFail::NO_AMMO);
            return AttackResult::ZERO;
        }
    }

    struct Damage wd = battle_calc_weapon_attack(src, target);

    struct status_change *t_sc_data = battle_get_sc_data(target);
    // significantly increase injuries for hasted characters
    if (wd.damage > 0 && t_sc_data[SC_HASTE].timer)
    {
        per_unit_add<16>(wd.damage, t_sc_data[SC_HASTE].val1);
    }

    if (wd.damage > 0 && t_sc_data[SC_PHYS_SHIELD].timer && tsd)
    {
        sint32 reduction = t_sc_data[SC_PHYS_SHIELD].val1;
        if (reduction > wd.damage)
            reduction = wd.damage;

        wd.damage -= reduction;
        MAP_LOG_PC(static_cast<MapSessionData *>(target), "MAGIC-ABSORB-DMG %d", reduction);
    }

    sint32 damage = wd.damage + wd.damage2;
    // returned damage
    sint32 rdamage = 0;
    if (damage > 0 && src != target && tsd)
    {
        if (wd.flag & BattleFlags::SHORT || tsd->short_weapon_damage_return > 0)
        {
            rdamage = damage * tsd->short_weapon_damage_return / 100;
            if (rdamage < 1)
                rdamage = 1;
        }
        if (wd.flag & BattleFlags::LONG && tsd->long_weapon_damage_return > 0)
        {
            rdamage = damage * tsd->long_weapon_damage_return / 100;
            if (rdamage < 1)
                rdamage = 1;
        }

        if (rdamage > 0)
            clif_damage(src, src, tick, wd.amotion, DEFAULT, rdamage, 1, 4, 0);
    }

    if (wd.div_ == 255 && sd)
    {
        interval_t delay = std::chrono::milliseconds(1000 - 4 * battle_get_agi(src) - 2 * battle_get_dex(src));
        sd->attackabletime = sd->canmove_tick = tick + delay;
    }
    else
    {
        clif_damage(src, target, tick, wd.amotion, wd.dmotion,
                    wd.damage, wd.div_, wd.type, wd.damage2);
        if (sd && sd->status.weapon >= 16 && wd.damage2 == 0)
            clif_damage(src, target, tick + std::chrono::milliseconds(10), wd.amotion, wd.dmotion, 0, 1, 0, 0);
    }

    map_freeblock_lock();

    if (sd)
    {
        sint32 weapon_index = sd->equip_index[EQUIP::WEAPON];
        sint32 weapon = 0;
        if (sd->inventory_data[weapon_index]
                && sd->status.inventory[weapon_index].equip & EPOS::WEAPON)
            weapon = sd->inventory_data[weapon_index]->nameid;

        MAP_LOG("PC%d %d:%d,%d WPNDMG %s%d %d FOR %d WPN %d",
                sd->status.char_id, src->m, src->x, src->y,
                tsd ? "PC" : "MOB",
                target->id,
                tsd ? 0 : static_cast<struct mob_data *>(target)->mob_class,
                wd.damage + wd.damage2, weapon);
    }

    if (tsd)
    {
        MAP_LOG("PC%d %d:%d,%d WPNINJURY %s%d %d FOR %d",
                target->id, target->m, target->x, target->y,
                tsd ? "PC" : "MOB",
                src->id,
                sd ? 0 : static_cast<struct mob_data *>(src)->mob_class,
                wd.damage + wd.damage2);
    }

    battle_damage(src, target, (wd.damage + wd.damage2));
    if (rdamage > 0)
        // target might have been killed, but the blocks won't have been freed
        battle_damage(target, src, rdamage);

    map_freeblock_unlock();

    return wd.dmg_lv;
}

/// Is a target an enemy?
bool battle_check_target(BlockList *src, BlockList *target)
{
    nullpo_ret(src);
    nullpo_ret(target);
    if (src == target)
        return 0;
    // The master (if a mob)
    BlockList *ss = src;

    MapSessionData *sd = NULL;
    struct mob_data *md = NULL;
    if (src->type == BL_PC)
        sd = static_cast<MapSessionData *>(src);
    if (src->type == BL_MOB)
        md = static_cast<struct mob_data *>(src);

    MapSessionData *tsd = NULL;
    struct mob_data *tmd = NULL;
    if (target->type == BL_PC)
        tsd = static_cast<MapSessionData *>(target);
    if (target->type == BL_MOB)
        tmd = static_cast<struct mob_data *>(target);

    if (tsd && tsd->invincible_timer)
        return 0;

    if (md && md->master_id)
    {
        if (md->master_id == target->id)
            return 0;
        if (md->state.special_mob_ai && tmd)
            return tmd->master_id != md->master_id || md->state.special_mob_ai > 2;
        ss = map_id2bl(md->master_id);
        if (!ss)
            return 0;
        if (ss == target)
            return 0;
    }

    if (tsd && pc_isinvisible(tsd))
        return 0;

    if (src->prev == NULL || (sd && pc_isdead(sd)))
        return 0;

    if (ss->type == BL_PC)
        sd = static_cast<MapSessionData *>(ss);
    if (ss->type == BL_MOB)
        md = static_cast<struct mob_data *>(ss);

    if ((sd && tmd) || (md && tsd))
        return 1;

    // must both be PCs for PvP
    if (!ss || !tsd)
        return 0;

    if (!maps[ss->m].flag.pvp && !pc_iskiller(sd, tsd))
        return 0;

    if (!maps[ss->m].flag.pvp_noparty)
        return 1;

    party_t s_p = battle_get_party_id(ss);
    party_t t_p = battle_get_party_id(target);

    // return true unless they are in the same party
    return !s_p || !t_p || s_p != t_p;
}

/// Check if a target is within the given range
// range 0 = unlimited
bool battle_check_range(BlockList *src, BlockList *bl, sint32 range)
{
    nullpo_ret(src);
    nullpo_ret(bl);

    if (src->m != bl->m)
        return 0;

    sint32 arange = max(abs(bl->x - src->x), abs(bl->y - src->y));


    if (range > 0 && range < arange)
        return 0;

    if (arange < 2)
        return 1;

    struct walkpath_data wpd;
    wpd.path_len = 0;
    wpd.path_pos = 0;
    wpd.path_half = 0;
    // flag bit 0x1 means line of sight
    // flag bit 0x10000 indicates a ranged attack
    // but the different collision is not used in TMW maps
    if (path_search(&wpd, src->m, src->x, src->y, bl->x, bl->y, 0x10001) != -1)
        return 1;
    // path_search only calculates diagonal first
    // this corrects for the case where target is just around the corner
    sint32 dx = (src->x > bl->x) - (src->x < bl->x);
    sint32 dy = (src->x > bl->x) - (src->x < bl->x);
    return path_search(&wpd, src->m, src->x + dx, src->y + dy, bl->x - dx, bl->y - dy, 0x10001) != -1;
}

/// Read the config options
sint32 battle_config_read(const char *cfgName)
{
    FILE *fp = fopen_(cfgName, "r");
    if (!fp)
    {
        printf("file not found: %s\n", cfgName);
        return 1;
    }

    static sint32 count = 0;

    if (!count++)
    {
        battle_config.enemy_critical = 0;
        battle_config.enemy_critical_rate = 100;
        battle_config.enemy_str = 1;
        battle_config.enemy_perfect_flee = 0;
        battle_config.sdelay_attack_enable = 0;
        battle_config.pc_damage_delay = 1;
        battle_config.pc_damage_delay_rate = 100;
        battle_config.random_monster_checklv = 1;
        battle_config.attr_recover = 1;
        battle_config.flooritem_lifetime = LIFETIME_FLOORITEM * 1000;
        battle_config.item_auto_get = 0;
        battle_config.drop_pickup_safety_zone = 20;
        battle_config.item_first_get_time = 3000;
        battle_config.item_second_get_time = 1000;
        battle_config.item_third_get_time = 1000;

        battle_config.drop_rate0item = 0;
        battle_config.base_exp_rate = 100;
        battle_config.job_exp_rate = 100;
        battle_config.pvp_exp = 1;
        battle_config.death_penalty_type = 0;
        battle_config.death_penalty_base = 0;
        battle_config.death_penalty_job = 0;
        battle_config.restart_hp_rate = 0;
        battle_config.restart_sp_rate = 0;
        battle_config.monster_hp_rate = 100;
        battle_config.monster_max_aspd = 199;
        battle_config.monster_active_enable = 1;
        battle_config.monster_damage_delay_rate = 100;
        battle_config.monster_loot_type = 0;
        battle_config.mob_count_rate = 100;
        battle_config.pc_invincible_time = 5000;
        battle_config.skill_min_damage = 0;
        battle_config.natural_healhp_interval = 6000;
        battle_config.natural_healsp_interval = 8000;
        battle_config.natural_heal_skill_interval = 10000;
        battle_config.natural_heal_weight_rate = 50;
        battle_config.itemheal_regeneration_factor = 1;
        battle_config.max_aspd = 199;
        battle_config.max_hp = 32500;
        battle_config.max_sp = 32500;
        battle_config.max_lv = 99;  // [MouseJstr]
        battle_config.max_parameter = 99;
        battle_config.undead_detect_type = 0;
        battle_config.agi_penaly_type = 0;
        battle_config.agi_penaly_count = 3;
        battle_config.agi_penaly_num = 0;
        battle_config.agi_penaly_count_lv = static_cast<sint32>(AttackResult::FLEE);
        battle_config.vit_penaly_type = 0;
        battle_config.vit_penaly_count = 3;
        battle_config.vit_penaly_num = 0;
        battle_config.vit_penaly_count_lv = static_cast<sint32>(AttackResult::DEF);
        battle_config.player_defense_type = 0;
        battle_config.monster_defense_type = 0;
        battle_config.pc_attack_direction_change = 1;
        battle_config.monster_attack_direction_change = 1;
        battle_config.dead_branch_active = 0;
        battle_config.mob_attack_attr_none = 1;
        battle_config.hide_GM_session = 0;
        battle_config.invite_request_check = 1;
        battle_config.disp_experience = 0;
        battle_config.item_rate_common = 100;
        battle_config.item_rate_equip = 100;
        battle_config.item_rate_card = 100;
        battle_config.item_rate_heal = 100; // Added by Valaris
        battle_config.item_rate_use = 100;  // End
        battle_config.item_drop_common_min = 1; // Added by TyrNemesis^
        battle_config.item_drop_common_max = 10000;
        battle_config.item_drop_equip_min = 1;
        battle_config.item_drop_equip_max = 10000;
        battle_config.item_drop_card_min = 1;
        battle_config.item_drop_card_max = 10000;
        battle_config.item_drop_heal_min = 1;   // Added by Valaris
        battle_config.item_drop_heal_max = 10000;
        battle_config.item_drop_use_min = 1;
        battle_config.item_drop_use_max = 10000;    // End
        battle_config.prevent_logout = 1;   // Added by RoVeRT
        battle_config.maximum_level = 255;  // Added by Valaris
        battle_config.drops_by_luk = 0; // [Valaris]
        battle_config.pk_mode = 0;  // [Valaris]
        battle_config.multi_level_up = 0;   // [Valaris]
        battle_config.hack_info_GM_level = 60;  // added by [Yor] (default: 60, GM level)
        battle_config.any_warp_GM_min_level = 20;   // added by [Yor]

        battle_config.area_size = 14;

        battle_config.chat_lame_penalty = 2;
        battle_config.chat_spam_threshold = 10;
        battle_config.chat_spam_flood = 10;
        battle_config.chat_spam_ban = 1;
        battle_config.chat_spam_warn = 8;
        battle_config.chat_maxline = 255;

        battle_config.packet_spam_threshold = 2;
        battle_config.packet_spam_flood = 30;
        battle_config.packet_spam_kick = 1;

        battle_config.mask_ip_gms = 1;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp))
    {
        static const struct
        {
            char str[128];
            sint32 *val;
        } data[] =
        {
            {"enemy_critical", &battle_config.enemy_critical},
            {"enemy_critical_rate", &battle_config.enemy_critical_rate},
            {"enemy_str", &battle_config.enemy_str},
            {"enemy_perfect_flee", &battle_config.enemy_perfect_flee},
            {"skill_delay_attack_enable", &battle_config.sdelay_attack_enable},
            {"player_damage_delay", &battle_config.pc_damage_delay},
            {"player_damage_delay_rate", &battle_config.pc_damage_delay_rate},
            {"random_monster_checklv", &battle_config.random_monster_checklv},
            {"attribute_recover", &battle_config.attr_recover},
            {"flooritem_lifetime", &battle_config.flooritem_lifetime},
            {"item_auto_get", &battle_config.item_auto_get},
            {"drop_pickup_safety_zone", &battle_config.drop_pickup_safety_zone},
            {"item_first_get_time", &battle_config.item_first_get_time},
            {"item_second_get_time", &battle_config.item_second_get_time},
            {"item_third_get_time", &battle_config.item_third_get_time},
            {"drop_rate0item", &battle_config.drop_rate0item},
            {"base_exp_rate", &battle_config.base_exp_rate},
            {"job_exp_rate", &battle_config.job_exp_rate},
            {"pvp_exp", &battle_config.pvp_exp},
            {"death_penalty_type", &battle_config.death_penalty_type},
            {"death_penalty_base", &battle_config.death_penalty_base},
            {"death_penalty_job", &battle_config.death_penalty_job},
            {"restart_hp_rate", &battle_config.restart_hp_rate},
            {"restart_sp_rate", &battle_config.restart_sp_rate},
            {"monster_hp_rate", &battle_config.monster_hp_rate},
            {"monster_max_aspd", &battle_config.monster_max_aspd},
            {"atcommand_spawn_quantity_limit", &battle_config.atc_spawn_quantity_limit},
            {"monster_active_enable", &battle_config.monster_active_enable},
            {"monster_damage_delay_rate", &battle_config.monster_damage_delay_rate},
            {"monster_loot_type", &battle_config.monster_loot_type},
            {"mob_count_rate", &battle_config.mob_count_rate},
            {"player_invincible_time", &battle_config.pc_invincible_time},
            {"skill_min_damage", &battle_config.skill_min_damage},
            {"natural_healhp_interval", &battle_config.natural_healhp_interval},
            {"natural_healsp_interval", &battle_config.natural_healsp_interval},
            {"natural_heal_skill_interval", &battle_config.natural_heal_skill_interval},
            {"natural_heal_weight_rate", &battle_config.natural_heal_weight_rate},
            {"itemheal_regeneration_factor", &battle_config.itemheal_regeneration_factor},
            {"max_aspd", &battle_config.max_aspd},
            {"max_hp", &battle_config.max_hp},
            {"max_sp", &battle_config.max_sp},
            {"max_lv", &battle_config.max_lv},
            {"max_parameter", &battle_config.max_parameter},
            {"undead_detect_type", &battle_config.undead_detect_type},
            {"agi_penaly_type", &battle_config.agi_penaly_type},
            {"agi_penaly_count", &battle_config.agi_penaly_count},
            {"agi_penaly_num", &battle_config.agi_penaly_num},
            {"agi_penaly_count_lv", &battle_config.agi_penaly_count_lv},
            {"vit_penaly_type", &battle_config.vit_penaly_type},
            {"vit_penaly_count", &battle_config.vit_penaly_count},
            {"vit_penaly_num", &battle_config.vit_penaly_num},
            {"vit_penaly_count_lv", &battle_config.vit_penaly_count_lv},
            {"player_defense_type", &battle_config.player_defense_type},
            {"monster_defense_type", &battle_config.monster_defense_type},
            {"player_attack_direction_change", &battle_config.pc_attack_direction_change},
            {"monster_attack_direction_change", &battle_config.monster_attack_direction_change},
            {"dead_branch_active", &battle_config.dead_branch_active},
            {"mob_attack_attr_none", &battle_config.mob_attack_attr_none},
            {"hide_GM_session", &battle_config.hide_GM_session},
            {"invite_request_check", &battle_config.invite_request_check},
            {"disp_experience", &battle_config.disp_experience},
            {"item_rate_common", &battle_config.item_rate_common},   // Added by RoVeRT
            {"item_rate_equip", &battle_config.item_rate_equip},
            {"item_rate_card", &battle_config.item_rate_card},   // End Addition
            {"item_rate_heal", &battle_config.item_rate_heal},   // Added by Valaris
            {"item_rate_use", &battle_config.item_rate_use}, // End
            {"item_drop_common_min", &battle_config.item_drop_common_min},   // Added by TyrNemesis^
            {"item_drop_common_max", &battle_config.item_drop_common_max},
            {"item_drop_equip_min", &battle_config.item_drop_equip_min},
            {"item_drop_equip_max", &battle_config.item_drop_equip_max},
            {"item_drop_card_min", &battle_config.item_drop_card_min},
            {"item_drop_card_max", &battle_config.item_drop_card_max},
            {"prevent_logout", &battle_config.prevent_logout},   // Added by RoVeRT
            {"alchemist_summon_reward", &battle_config.alchemist_summon_reward}, // [Valaris]
            {"maximum_level", &battle_config.maximum_level}, // [Valaris]
            {"drops_by_luk", &battle_config.drops_by_luk},   // [Valaris]
            {"monsters_ignore_gm", &battle_config.monsters_ignore_gm},   // [Valaris]
            {"pk_mode", &battle_config.pk_mode}, // [Valaris]
            {"multi_level_up", &battle_config.multi_level_up},   // [Valaris]
            {"hack_info_GM_level", &battle_config.hack_info_GM_level},   // added by [Yor]
            {"any_warp_GM_min_level", &battle_config.any_warp_GM_min_level}, // added by [Yor]
            {"area_size", &battle_config.area_size}, // added by [MouseJstr]
            {"chat_lame_penalty", &battle_config.chat_lame_penalty},
            {"chat_spam_threshold", &battle_config.chat_spam_threshold},
            {"chat_spam_flood", &battle_config.chat_spam_flood},
            {"chat_spam_ban", &battle_config.chat_spam_ban},
            {"chat_spam_warn", &battle_config.chat_spam_warn},
            {"chat_maxline", &battle_config.chat_maxline},
            {"packet_spam_threshold", &battle_config.packet_spam_threshold},
            {"packet_spam_flood", &battle_config.packet_spam_flood},
            {"packet_spam_kick", &battle_config.packet_spam_kick},
            {"mask_ip_gms", &battle_config.mask_ip_gms}
        };

        if (line[0] == '/' && line[1] == '/')
            continue;

        char w1[1024], w2[1024];
        if (sscanf(line, "%[^:]:%s", w1, w2) != 2)
            continue;

        if (strcasecmp(w1, "import") == 0)
        {
            battle_config_read(w2);
            continue;
        }
        for (sint32 i = 0; i < sizeof(data) / (sizeof(data[0])); i++)
            if (strcasecmp(w1, data[i].str) == 0)
            {
                *data[i].val = config_switch(w2);
                goto continue_outer;
            }
        map_log("%s: No such setting: %s", __func__, line);
    continue_outer: ;
    }
    fclose_(fp);

    if (--count == 0)
    {
        if (battle_config.flooritem_lifetime < 1000)
            battle_config.flooritem_lifetime = LIFETIME_FLOORITEM * 1000;
        if (battle_config.restart_hp_rate < 0)
            battle_config.restart_hp_rate = 0;
        else if (battle_config.restart_hp_rate > 100)
            battle_config.restart_hp_rate = 100;
        if (battle_config.restart_sp_rate < 0)
            battle_config.restart_sp_rate = 0;
        else if (battle_config.restart_sp_rate > 100)
            battle_config.restart_sp_rate = 100;
        if (std::chrono::milliseconds(battle_config.natural_healhp_interval) < NATURAL_HEAL_INTERVAL)
            battle_config.natural_healhp_interval = std::chrono::duration_cast<std::chrono::milliseconds>(NATURAL_HEAL_INTERVAL).count();
        if (std::chrono::milliseconds(battle_config.natural_healsp_interval) < NATURAL_HEAL_INTERVAL)
            battle_config.natural_healsp_interval = std::chrono::duration_cast<std::chrono::milliseconds>(NATURAL_HEAL_INTERVAL).count();
        if (std::chrono::milliseconds(battle_config.natural_heal_skill_interval) < NATURAL_HEAL_INTERVAL)
            battle_config.natural_heal_skill_interval = std::chrono::duration_cast<std::chrono::milliseconds>(NATURAL_HEAL_INTERVAL).count();
        if (battle_config.natural_heal_weight_rate < 50)
            battle_config.natural_heal_weight_rate = 50;
        if (battle_config.natural_heal_weight_rate > 101)
            battle_config.natural_heal_weight_rate = 101;
        battle_config.monster_max_aspd =
            2000 - battle_config.monster_max_aspd * 10;
        if (battle_config.monster_max_aspd < 10)
            battle_config.monster_max_aspd = 10;
        if (battle_config.monster_max_aspd > 1000)
            battle_config.monster_max_aspd = 1000;
        battle_config.max_aspd = 2000 - battle_config.max_aspd * 10;
        if (battle_config.max_aspd < 10)
            battle_config.max_aspd = 10;
        if (battle_config.max_aspd > 1000)
            battle_config.max_aspd = 1000;
        if (battle_config.max_hp > 1000000)
            battle_config.max_hp = 1000000;
        if (battle_config.max_hp < 100)
            battle_config.max_hp = 100;
        if (battle_config.max_sp > 1000000)
            battle_config.max_sp = 1000000;
        if (battle_config.max_sp < 100)
            battle_config.max_sp = 100;
        if (battle_config.max_parameter < 10)
            battle_config.max_parameter = 10;
        if (battle_config.max_parameter > 10000)
            battle_config.max_parameter = 10000;

        if (battle_config.agi_penaly_count < 2)
            battle_config.agi_penaly_count = 2;
        if (battle_config.vit_penaly_count < 2)
            battle_config.vit_penaly_count = 2;

        if (battle_config.item_drop_common_min < 1)
            battle_config.item_drop_common_min = 1;
        if (battle_config.item_drop_common_max > 10000)
            battle_config.item_drop_common_max = 10000;
        if (battle_config.item_drop_equip_min < 1)
            battle_config.item_drop_equip_min = 1;
        if (battle_config.item_drop_equip_max > 10000)
            battle_config.item_drop_equip_max = 10000;
        if (battle_config.item_drop_card_min < 1)
            battle_config.item_drop_card_min = 1;
        if (battle_config.item_drop_card_max > 10000)
            battle_config.item_drop_card_max = 10000;

        if (battle_config.hack_info_GM_level < 0)
            battle_config.hack_info_GM_level = 0;
        else if (battle_config.hack_info_GM_level > 100)
            battle_config.hack_info_GM_level = 100;

        if (battle_config.any_warp_GM_min_level < 0)
            battle_config.any_warp_GM_min_level = 0;
        else if (battle_config.any_warp_GM_min_level > 100)
            battle_config.any_warp_GM_min_level = 100;

        if (battle_config.chat_spam_ban < 0)
            battle_config.chat_spam_ban = 0;
        else if (battle_config.chat_spam_ban > 32767)
            battle_config.chat_spam_ban = 32767;

        if (battle_config.chat_spam_flood < 0)
            battle_config.chat_spam_flood = 0;
        else if (battle_config.chat_spam_flood > 32767)
            battle_config.chat_spam_flood = 32767;

        if (battle_config.chat_spam_warn < 0)
            battle_config.chat_spam_warn = 0;
        else if (battle_config.chat_spam_warn > 32767)
            battle_config.chat_spam_warn = 32767;

        if (battle_config.chat_spam_threshold < 0)
            battle_config.chat_spam_threshold = 0;
        else if (battle_config.chat_spam_threshold > 32767)
            battle_config.chat_spam_threshold = 32767;

        if (battle_config.chat_maxline < 1)
            battle_config.chat_maxline = 1;
        else if (battle_config.chat_maxline > 512)
            battle_config.chat_maxline = 512;

        if (battle_config.packet_spam_threshold < 0)
            battle_config.packet_spam_threshold = 0;
        else if (battle_config.packet_spam_threshold > 32767)
            battle_config.packet_spam_threshold = 32767;

        if (battle_config.packet_spam_flood < 0)
            battle_config.packet_spam_flood = 0;
        else if (battle_config.packet_spam_flood > 32767)
            battle_config.packet_spam_flood = 32767;

        if (battle_config.packet_spam_kick < 0)
            battle_config.packet_spam_kick = 0;
        else if (battle_config.packet_spam_kick > 1)
            battle_config.packet_spam_kick = 1;

        if (battle_config.mask_ip_gms < 0)
            battle_config.mask_ip_gms = 0;
        else if (battle_config.mask_ip_gms > 1)
            battle_config.mask_ip_gms = 1;

    }

    return 0;
}
