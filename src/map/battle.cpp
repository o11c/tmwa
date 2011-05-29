#include "battle.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <algorithm>

#include "../common/timer.hpp"
#include "../common/nullpo.hpp"

#include "clif.hpp"
#include "itemdb.hpp"
#include "map.hpp"
#include "mob.hpp"
#include "pc.hpp"
#include "skill.hpp"
#include "../common/socket.hpp"
#include "../common/mt_rand.hpp"

static int battle_attr_fix(int damage, int atk_elem, int def_elem);
static int battle_stopattack(struct block_list *bl);
static int battle_get_hit(struct block_list *bl);
static int battle_get_flee(struct block_list *bl);
static int battle_get_flee2(struct block_list *bl);
static int battle_get_def2(struct block_list *bl);
static int battle_get_baseatk(struct block_list *bl);
static int battle_get_atk(struct block_list *bl);
static int battle_get_atk2(struct block_list *bl);
static int battle_get_attack_element(struct block_list *bl);
static int battle_get_attack_element2(struct block_list *bl);

/// Table of elemental damage modifiers
int attr_fix_table[4][10][10];

struct Battle_Config battle_config;

// There are lots of battle_get_foo(struct block_list *) entries
// to hide the differences between PCs and MOBs.
// In addition, there are some effects

/// Count the number of beings attacking this being, that are at least the given level
int battle_counttargeted(struct block_list *bl, struct block_list *src, int target_lv)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_PC)
        return pc_counttargeted((struct map_session_data *) bl, src, target_lv);
    if (bl->type == BL_MOB)
        return mob_counttargeted((struct mob_data *) bl, src, target_lv);
    return 0;
}

/// which way the object is facing
Direction battle_get_dir(struct block_list *bl)
{
    nullpo_retr(DIR_S, bl);

    if (bl->type == BL_MOB)
        return ((struct mob_data *) bl)->dir;
    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->dir;
    return DIR_S;
}

/// Get the (base) level of this being
int battle_get_lv(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_MOB)
        return ((struct mob_data *) bl)->stats[MOB_LV];
    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->status.base_level;
    return 0;
}

/// Get the maximum attack distance of this being
int battle_get_range(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_MOB)
        return mob_db[((struct mob_data *) bl)->mob_class].range;
    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->attackrange;
    return 0;
}

/// Get current HP of this being
int battle_get_hp(struct block_list *bl)
{
    nullpo_retr(1, bl);

    if (bl->type == BL_MOB)
        return ((struct mob_data *) bl)->hp;
    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->status.hp;
    return 1;
}

/// Get maximum HP of this being
int battle_get_max_hp(struct block_list *bl)
{
    nullpo_retr(1, bl);

    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->status.max_hp;
    if (bl->type != BL_MOB)
        return 1;

    int max_hp = ((struct mob_data *) bl)->stats[MOB_MAX_HP];
    percent_adjust(max_hp, battle_config.monster_hp_rate);
    return std::max(1, max_hp);
}

/// Get strength of a being
int battle_get_str(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_MOB)
        return std::max(0, (int) ((struct mob_data *) bl)->stats[MOB_STR]);
    if (bl->type == BL_PC)
        return std::max(0, (int) ((struct map_session_data *) bl)->paramc[0]);
    return 0;
}

/// Get agility of a being
int battle_get_agi(struct block_list *bl)
{
    nullpo_retr(0, bl);
    if (bl->type == BL_MOB)
        std::max(0, (int) ((struct mob_data *) bl)->stats[MOB_AGI]);
    if (bl->type == BL_PC)
        std::max(0, (int) ((struct map_session_data *) bl)->paramc[1]);
    return 0;
}

/// Get vitality of a being
int battle_get_vit(struct block_list *bl)
{
    nullpo_retr(0, bl);
    if (bl->type == BL_MOB)
        std::max(0, (int) ((struct mob_data *) bl)->stats[MOB_VIT]);
    if (bl->type == BL_PC)
        std::max(0, (int) ((struct map_session_data *) bl)->paramc[2]);
    return 0;
}

/// Get intelligence of a being
int battle_get_int(struct block_list *bl)
{
    nullpo_retr(0, bl);
    if (bl->type == BL_MOB)
        std::max(0, (int) ((struct mob_data *) bl)->stats[MOB_INT]);
    if (bl->type == BL_PC)
        std::max(0, (int) ((struct map_session_data *) bl)->paramc[3]);
    return 0;
}

/// Get dexterity of a being
int battle_get_dex(struct block_list *bl)
{
    nullpo_retr(0, bl);
    if (bl->type == BL_MOB)
        std::max(0, (int) ((struct mob_data *) bl)->stats[MOB_DEX]);
    if (bl->type == BL_PC)
        std::max(0, (int) ((struct map_session_data *) bl)->paramc[4]);
    return 0;
}

/// Get luck of a being
int battle_get_luk(struct block_list *bl)
{
    nullpo_retr(0, bl);
    if (bl->type == BL_MOB)
        std::max(0, (int) ((struct mob_data *) bl)->stats[MOB_LUK]);
    if (bl->type == BL_PC)
        std::max(0, (int) ((struct map_session_data *) bl)->paramc[5]);
    return 0;
}

/// Calculate a being's ability to not be hit
int battle_get_flee(struct block_list *bl)
{
    nullpo_retr(1, bl);

    int flee;
    if (bl->type == BL_PC)
        flee = ((struct map_session_data *) bl)->flee;
    else
        flee = battle_get_agi(bl) + battle_get_lv(bl);

    if (battle_is_unarmed(bl))
        flee += (skill_power_bl(bl, TMW_BRAWLING) >> 3);
    // +25 for 200
    flee += skill_power_bl(bl, TMW_SPEED) >> 3;

    return std::max(1, flee);
}

/// Calculate a being's ability to hit something
int battle_get_hit(struct block_list *bl)
{
    nullpo_retr(1, bl);

    int hit;
    if (bl->type == BL_PC)
        hit = ((struct map_session_data *) bl)->hit;
    else
        hit = battle_get_dex(bl) + battle_get_lv(bl);

    if (battle_is_unarmed(bl))
        // +12 for 200
        hit += (skill_power_bl(bl, TMW_BRAWLING) >> 4);

    return std::max(1, hit);
}

/// Calculate a being's luck at not getting hit
int battle_get_flee2(struct block_list *bl)
{
    nullpo_retr(1, bl);

    int flee2;
    if (bl->type == BL_PC)
        flee2 = ((struct map_session_data *) bl)->flee2;
    else
        flee2 = battle_get_luk(bl) + 1;

    if (battle_is_unarmed(bl))
        flee2 += (skill_power_bl(bl, TMW_BRAWLING) >> 3);
    // +25 for 200
    flee2 += skill_power_bl(bl, TMW_SPEED) >> 3;

    return std::max(1, flee2);
}

/// Calculate being's ability to make a critical hit
static int battle_get_critical(struct block_list *bl)
{
    nullpo_retr(1, bl);

    if (bl->type == BL_PC)
        // FIXME was this intended?
        return std::max(1, ((struct map_session_data *) bl)->critical - battle_get_luk(bl));
    return battle_get_luk(bl) * 3 + 1;
}

/// Get a being's base attack strength
int battle_get_baseatk(struct block_list *bl)
{
    nullpo_retr(1, bl);

    if (bl->type == BL_PC)
        return std::max(1, (int) ((struct map_session_data *) bl)->base_atk);

    int str = battle_get_str(bl);
    int dstr = str / 10;
    return dstr * dstr + str;
}

/// Get minimum attack strength of a PC's main weapon
int battle_get_atk(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_PC)
        return std::max(0, (int) ((struct map_session_data *) bl)->watk);
    if (bl->type == BL_MOB)
        return std::max(0, (int) ((struct mob_data *) bl)->stats[MOB_ATK1]);
    return 0;
}

/// Get minimum attack strength of a PC's second weapon
static int battle_get_atk_(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->watk_;
    return 0;
}

/// Get maximum attack strength of a PC's main weapon
int battle_get_atk2(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->watk2;
    if (bl->type != BL_MOB)
        return 0;
    return std::max(0, (int) ((struct mob_data *) bl)->stats[MOB_ATK2]);
}

/// Get maximum attack strength of a PC's second weapon
static int battle_get_atk_2(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->watk_2;
    return 0;
}

/// Get a being's defense
int battle_get_def(struct block_list *bl)
{
    nullpo_retr(0, bl);

    int def = 0;
    if (bl->type == BL_PC)
    {
        def = ((struct map_session_data *) bl)->def;
    }
    if (bl->type == BL_MOB)
    {
        def = ((struct mob_data *) bl)->stats[MOB_DEF];
    }

    struct status_change *sc_data = battle_get_sc_data(bl);
    if (sc_data)
    {
        if (sc_data[SC_POISON].timer != -1 && bl->type != BL_PC)
            percent_adjust(def, 75);
    }
    return std::max(0, def);
}

/// Get a being's magical defense
int battle_get_mdef(struct block_list *bl)
{
    nullpo_retr(0, bl);

    int mdef = 0;
    if (bl->type == BL_PC)
        mdef = ((struct map_session_data *) bl)->mdef;
    if (bl->type == BL_MOB)
        mdef = ((struct mob_data *) bl)->stats[MOB_MDEF];

    struct status_change *sc_data = battle_get_sc_data(bl);
    if (sc_data)
    {
        if (mdef < 90 && sc_data[SC_MBARRIER].timer != -1)
        {
            mdef += sc_data[SC_MBARRIER].val1;
            if (mdef > 90)
                mdef = 90;
        }
    }
    return std::max(0, mdef);
}

/// Get a being's secondary defense
int battle_get_def2(struct block_list *bl)
{
    nullpo_retr(1, bl);

    int def2 = 1;
    if (bl->type == BL_PC)
        def2 = ((struct map_session_data *) bl)->def2;
    if (bl->type == BL_MOB)
        def2 = ((struct mob_data *) bl)->stats[MOB_VIT];

    struct status_change *sc_data = battle_get_sc_data(bl);
    if (sc_data)
    {
        if (sc_data[SC_POISON].timer != -1 && bl->type != BL_PC)
            percent_adjust(def2, 75);
    }
    return std::max(1, def2);
}

/// Get a being's walk delay
int battle_get_speed(struct block_list *bl)
{
    nullpo_retr(1000, bl);

    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->speed;

    int speed = 1000;
    if (bl->type == BL_MOB)
        speed = ((struct mob_data *) bl)->stats[MOB_SPEED];
    return std::max(1, speed);
}

/// Get a being's attack delay
int battle_get_adelay(struct block_list *bl)
{
    nullpo_retr(4000, bl);

    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->aspd << 1;

    struct status_change *sc_data = battle_get_sc_data(bl);
    int adelay = 4000;
    int aspd_rate = 100;
    if (bl->type == BL_MOB)
        adelay = ((struct mob_data *) bl)->stats[MOB_ADELAY];

    if (sc_data)
    {
        if (sc_data[SC_SPEEDPOTION0].timer != -1)
            aspd_rate -= sc_data[SC_SPEEDPOTION0].val1;
        // Fate's `haste' spell works the same as the above
        if (sc_data[SC_HASTE].timer != -1)
            aspd_rate -= sc_data[SC_HASTE].val1;
    }

    percent_adjust(adelay, aspd_rate);
    if (adelay < battle_config.monster_max_aspd << 1)
        adelay = battle_config.monster_max_aspd << 1;
    return adelay;
}

/// Being's attack motion rate?
int battle_get_amotion(struct block_list *bl)
{
    nullpo_retr(2000, bl);

    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->amotion;
    struct status_change *sc_data = battle_get_sc_data(bl);
    int amotion = 2000, aspd_rate = 100;
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        amotion = mob_db[((struct mob_data *) bl)->mob_class].amotion;

    if (sc_data)
    {
        if (sc_data[SC_SPEEDPOTION0].timer != -1)
            aspd_rate -= sc_data[SC_SPEEDPOTION0].val1;
        if (sc_data[SC_HASTE].timer != -1)
            aspd_rate -= sc_data[SC_HASTE].val1;
    }

    percent_adjust(amotion, aspd_rate);
    if (amotion < battle_config.monster_max_aspd)
        amotion = battle_config.monster_max_aspd;
    return amotion;
}

/// Being's defense motion rate?
int battle_get_dmotion(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_MOB)
    {
        int ret = mob_db[((struct mob_data *) bl)->mob_class].dmotion;
        percent_adjust(ret, battle_config.monster_damage_delay_rate);
        return ret;
    }
    if (bl->type == BL_PC)
    {
        int ret = ((struct map_session_data *) bl)->dmotion;
        percent_adjust(ret, battle_config.pc_damage_delay_rate);
        return ret;
    }
    return 2000;
}

/// Get a being's (encoded) defense element
int battle_get_element(struct block_list *bl)
{
    nullpo_retr(20, bl);

    if (bl->type == BL_MOB)
        return ((struct mob_data *) bl)->def_ele;
    if (bl->type == BL_PC)
        // This adds 1 level ...
        return 20 + ((struct map_session_data *) bl)->def_ele;
    // 20 = level 1 neutral
    return 20;
}

/// Get a PC's (secondary?) attack element (TODO rewrite element system)
int battle_get_attack_element(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->atk_ele;
    return 0;
}

/// Get a PC's (secondary?) attack element
int battle_get_attack_element2(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->atk_ele_;
    return 0;
}

/// Return a party ID (or fake one) for a being
int battle_get_party_id(struct block_list *bl)
{
    nullpo_retr(0, bl);
    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->status.party_id;
    if (bl->type == BL_MOB)
    {
        struct mob_data *md = (struct mob_data *) bl;
        if (md->master_id > 0)
            // slave mobs
            return -md->master_id;
        // else, it is its own party
        return -md->bl.id;
    }
    return 0;
}

/// Get a being's race
int battle_get_race(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_MOB)
        return mob_db[((struct mob_data *) bl)->mob_class].race;
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
int battle_get_mode(struct block_list *bl)
{
    nullpo_retr(0x01, bl);

    if (bl->type == BL_MOB)
        return mob_db[((struct mob_data *) bl)->mob_class].mode;
    return 0x01;
}

/// stat_id: like SP_VIT
// this is only used by the skill pool system
int battle_get_stat(int stat_id, struct block_list *bl)
{
    switch (stat_id)
    {
    case SP_STR:
        return battle_get_str(bl);
    case SP_AGI:
        return battle_get_agi(bl);
    case SP_DEX:
        return battle_get_dex(bl);
    case SP_VIT:
        return battle_get_vit(bl);
    case SP_INT:
        return battle_get_int(bl);
    case SP_LUK:
        return battle_get_luk(bl);
    default:
        return 0;
    }
}

/// Get the list of status effects on a being
struct status_change *battle_get_sc_data(struct block_list *bl)
{
    nullpo_retr(NULL, bl);
    if (bl->type == BL_MOB)
        return ((struct mob_data *) bl)->sc_data;
    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->sc_data;
    return NULL;
}

/// Get pointer to the number of status effects on a being?
short *battle_get_sc_count(struct block_list *bl)
{
    nullpo_retr(NULL, bl);

    if (bl->type == BL_MOB)
        return &((struct mob_data *) bl)->sc_count;
    if (bl->type == BL_PC)
        return &((struct map_session_data *) bl)->sc_count;
    return NULL;
}

/// Get a pointer to a being's "opt1" field
short *battle_get_opt1(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_MOB)
        return &((struct mob_data *) bl)->opt1;
    if (bl->type == BL_PC && (struct map_session_data *) bl)
        return &((struct map_session_data *) bl)->opt1;
    if (bl->type == BL_NPC && (struct npc_data *) bl)
        return &((struct npc_data *) bl)->opt1;
    return 0;
}

/// Get a pointer to a being's "opt2" field
short *battle_get_opt2(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_MOB)
        return &((struct mob_data *) bl)->opt2;
    if (bl->type == BL_PC)
        return &((struct map_session_data *) bl)->opt2;
    if (bl->type == BL_NPC)
        return &((struct npc_data *) bl)->opt2;
    return 0;
}

/// Get a pointer to a being's "opt3" field
short *battle_get_opt3(struct block_list *bl)
{
    nullpo_retr(0, bl);
    if (bl->type == BL_MOB)
        return &((struct mob_data *) bl)->opt3;
    if (bl->type == BL_PC)
        return &((struct map_session_data *) bl)->opt3;
    if (bl->type == BL_NPC)
        return &((struct npc_data *) bl)->opt3;
    return 0;
}

/// Get a pointer to a being's "option" field
short *battle_get_option(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_MOB)
        return &((struct mob_data *) bl)->option;
    if (bl->type == BL_PC)
        return &((struct map_session_data *) bl)->status.option;
    if (bl->type == BL_NPC)
        return &((struct npc_data *) bl)->option;
    return 0;
}

//-------------------------------------------------------------------

/// Information for delayed damage
struct battle_delay_damage_
{
    struct block_list *src, *target;
    int damage;
};

/// Actually apply delayed damage
static void battle_delay_damage_sub(timer_id, tick_t, custom_id_t id, custom_data_t data)
{
    struct battle_delay_damage_ *dat = (struct battle_delay_damage_ *) data.p;
    if (!dat)
        return;
    if (dat->target->prev && map_id2bl(id) == dat->src)
        battle_damage(dat->src, dat->target, dat->damage);
    free(dat);
}

/// Setup delayed damage
int battle_delay_damage(tick_t tick, struct block_list *src,
                        struct block_list *target, int damage)
{
    nullpo_retr(0, src);
    nullpo_retr(0, target);

    struct battle_delay_damage_ *dat;
    CREATE(dat, struct battle_delay_damage_, 1);
    dat->src = src;
    dat->target = target;
    dat->damage = damage;
    add_timer(tick, battle_delay_damage_sub, src->id, (void *)dat);
    return 0;
}

/// Decrease the HP of target
// if trying to damage by a negative amount, heal instead
int battle_damage(struct block_list *src, struct block_list *target, int damage)
{
    nullpo_retr(0, target);
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
        struct mob_data *md = (struct mob_data *) target;
        return mob_damage(src, md, damage, 0);
    }
    if (target->type == BL_PC)
    {
        struct map_session_data *tsd = (struct map_session_data *) target;
        return pc_damage(src, tsd, damage);
    }
    return 0;
}

/// Increase HP of a being (and SP of a PC)
// if hp is negative, do damage and ignore sp
int battle_heal(struct block_list *src, struct block_list *target, int hp, int sp)
{
    nullpo_retr(0, target);
    // src may be NULL for programmatic healing

    if (target->type == BL_PC && pc_isdead((struct map_session_data *) target))
        return 0;
    if (!hp && !sp)
        return 0;

    if (hp < 0)
        return battle_damage(src, target, -hp);

    if (target->type == BL_MOB)
        return mob_heal((struct mob_data *) target, hp);
    if (target->type == BL_PC)
        return pc_heal((struct map_session_data *) target, hp, sp);
    return 0;
}

/// A being should stop attacking its target
int battle_stopattack(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_MOB)
        return mob_stopattack((struct mob_data *) bl);
    if (bl->type == BL_PC)
        return pc_stopattack((struct map_session_data *) bl);
    return 0;
}

/// A being should stop walking
int battle_stopwalking(struct block_list *bl, int type)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_MOB)
        return mob_stop_walking((struct mob_data *) bl, type);
    if (bl->type == BL_PC)
        return pc_stop_walking((struct map_session_data *) bl, type);
    return 0;
}

/// Change the amount of elemental damage by the lookup table
int battle_attr_fix(int damage, int atk_elem, int def_elem)
{
    int def_type = def_elem % 10, def_lv = def_elem / 10 / 2;

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
int battle_calc_damage(struct block_list *target, int damage, int div_, int flag)
{
    nullpo_retr(0, target);

    if (battle_config.skill_min_damage || flag & BF_MISC)
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
                                                   struct block_list *target)
{
    struct Damage wd = {};
    wd.damage2 = 0;
    wd.amotion = battle_get_amotion(&md->bl);
    wd.dmotion = battle_get_dmotion(target);
    wd.flag = BF_SHORT | BF_WEAPON;
    wd.dmg_lv = 0;

    wd.type = 0;                   // normal
    wd.div_ = 1;                   // single attack
    wd.damage = 0;

    struct map_session_data *tsd = NULL;
    if (target->type == BL_PC)
        tsd = (struct map_session_data *) target;
    struct mob_data *tmd = NULL;
    if (target->type == BL_MOB)
        tmd = (struct mob_data *) target;

    int flee = battle_get_flee(target);

    if (battle_config.agi_penaly_type == 1)
    {
        int target_count = battle_counttargeted(target, &md->bl, battle_config.agi_penaly_count_lv);
        target_count -= battle_config.agi_penaly_count;
        if (target_count > 0)
            percent_subtract(flee, target_count * battle_config.agi_penaly_num);
    }
    if (battle_config.agi_penaly_type == 2)
    {
        int target_count = battle_counttargeted(target, &md->bl, battle_config.agi_penaly_count_lv);
        target_count -= battle_config.agi_penaly_count;
        if (target_count > 0)
            flee -= target_count * battle_config.agi_penaly_num;
    }

    if (flee < 1)
        flee = 1;

    if (battle_config.enemy_str)
        wd.damage = battle_get_baseatk(&md->bl);

    if (mob_db[md->mob_class].range > 3)
        wd.flag = (wd.flag & ~BF_RANGEMASK) | BF_LONG;

    int atkmin = battle_get_atk(&md->bl);
    int atkmax = battle_get_atk2(&md->bl);
    if (atkmin > atkmax)
        atkmin = atkmax;

    int cri = battle_get_critical(&md->bl);
    cri -= battle_get_luk(target) * 3;
    percent_adjust(cri, battle_config.enemy_critical_rate);
    if (cri < 1)
        cri = 1;
    if (tsd && tsd->critical_def)
        percent_subtract(cri, tsd->critical_def);

    int s_ele = battle_get_attack_element(&md->bl);

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

        int def1 = battle_get_def(target);
        int def2 = battle_get_def2(target);
        int t_vit = battle_get_vit(target);

        if (battle_config.vit_penaly_type == 1)
        {
            int target_count = battle_counttargeted(target, &md->bl, battle_config.vit_penaly_count_lv);
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
            int target_count = battle_counttargeted(target, &md->bl, battle_config.vit_penaly_count_lv);
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

        int t_def = def2 * 8 / 10;

        int vitbonusmax = (t_vit / 20) * (t_vit / 20) - 1;
        if (battle_config.monster_defense_type)
            wd.damage -= def1 * battle_config.monster_defense_type;
        else
            percent_subtract(wd.damage, def1);
        wd.damage -= t_def + (vitbonusmax < 1 ? 0 : MRAND(vitbonusmax + 1));
    }

    if (wd.damage < 1)
        wd.damage = 1;

    int hitrate = battle_get_hit(&md->bl) - flee + 80;
    hitrate = hitrate > 95 ? 95 : (hitrate < 5 ? 5 : hitrate);
    if (wd.type == 0 && MRAND(100) >= hitrate)
    {
        wd.damage = 0;
        wd.dmg_lv = ATK_FLEE;
    }
    else
    {
        wd.dmg_lv = ATK_DEF;
    }

    if (tsd)
    {
        int cardfix = 100;

        if (wd.flag & BF_LONG)
            percent_subtract(cardfix, tsd->long_attack_def_rate);
        if (wd.flag & BF_SHORT)
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
        wd.dmg_lv = ATK_LUCKY;
    }

    if (tmd && battle_config.enemy_perfect_flee && MRAND(1000) < battle_get_flee2(target))
    {
        wd.damage = 0;
        wd.type = 0x0b;
        wd.dmg_lv = ATK_LUCKY;
    }

    if (battle_get_mode(target) & 0x40 && wd.damage > 0)
        wd.damage = 1;
    if (tsd && tsd->special_state.no_weapon_damage)
        wd.damage = 0;
    wd.damage = battle_calc_damage(target, wd.damage, wd.div_, wd.flag);
    return wd;
}

/// Check if a PC is unarmed
int battle_is_unarmed(struct block_list *bl)
{
    if (!bl || bl->type != BL_PC)
        return 0;
    struct map_session_data *sd = (struct map_session_data *) bl;
    return sd->equip_index[EQUIP_SHIELD] == -1 && sd->equip_index[EQUIP_WEAPON] == -1;
}

/// A PC attacks another being
static struct Damage battle_calc_pc_weapon_attack(struct map_session_data *sd,
                                                  struct block_list *target)
{
    struct Damage wd = {};
    sd->state.attack_type = BF_WEAPON;

    wd.amotion = battle_get_amotion(&sd->bl);
    wd.dmotion = battle_get_dmotion(target);
    wd.dmg_lv = 0;
    wd.flag = BF_SHORT | BF_WEAPON;
    wd.type = 0;                   // normal
    wd.div_ = 1;                   // single attack

    int def1 = battle_get_def(target);
    int def2 = battle_get_def2(target);
    int t_vit = battle_get_vit(target);

    int s_ele = battle_get_attack_element(&sd->bl);
    int s_ele_ = battle_get_attack_element2(&sd->bl);
    int t_race = battle_get_race(target);
    int t_ele = battle_get_elem_type(target);
    int t_mode = battle_get_mode(target);

    struct map_session_data *tsd = NULL;
    if (target->type == BL_PC)
        tsd = (struct map_session_data *) target;
    struct mob_data *tmd = NULL;
    if (target->type == BL_MOB)
        tmd = (struct mob_data *) target;

    int flee = battle_get_flee(target);
    if (battle_config.agi_penaly_type == 1)
    {
        int target_count = battle_counttargeted(target, &sd->bl, battle_config.agi_penaly_count_lv);
        target_count -= battle_config.agi_penaly_count;
        if (target_count > 0)
            percent_subtract(flee, target_count * battle_config.agi_penaly_num);
    }
    if (battle_config.agi_penaly_type == 2)
    {
        int target_count = battle_counttargeted(target, &sd->bl, battle_config.agi_penaly_count_lv);
        target_count -= battle_config.agi_penaly_count;
        if (target_count > 0)
            flee -= target_count * battle_config.agi_penaly_num;
    }
    if (flee < 1)
        flee = 1;

    int target_distance = std::max(abs(sd->bl.x - target->x), abs(sd->bl.y - target->y));
    // NOTE: dividing by 75 means the penalty distance is only decreased by 2
    // even at maximum skill, whereas the range increases every 60, maximum 3
    int malus_dist = std::max(0, target_distance - skill_power(sd, AC_OWL) / 75);
    int hitrate = battle_get_hit(&sd->bl) - flee + 80;
    hitrate -= malus_dist * (malus_dist + 1);

    int dex = battle_get_dex(&sd->bl);
    int watk = battle_get_atk(&sd->bl);
    int watk_ = battle_get_atk_(&sd->bl);

    wd.damage = wd.damage2 = battle_get_baseatk(&sd->bl);
    if (sd->attackrange > 2)
    {
        // up to 31.25% bonus for long-range hit
        const int range_damage_bonus = 80;
        per256_add(wd.damage, (range_damage_bonus * target_distance) / sd->attackrange);
        per256_add(wd.damage2, (range_damage_bonus * target_distance) / sd->attackrange);
    }

    int atkmin = dex;
    int atkmin_ = dex;
    sd->state.arrow_atk = 0;
    if (sd->equip_index[9] >= 0 && sd->inventory_data[sd->equip_index[9]])
        percent_adjust(atkmin, 80 + sd->inventory_data[sd->equip_index[9]]->wlv * 20);
    if (sd->equip_index[8] >= 0 && sd->inventory_data[sd->equip_index[8]])
        percent_adjust(atkmin_, 80 + sd->inventory_data[sd->equip_index[8]]->wlv * 20);
    if (sd->status.weapon == 11)
    {
        atkmin = watk * std::min(atkmin, watk) / 100;
        wd.flag = (wd.flag & ~BF_RANGEMASK) | BF_LONG;
        if (sd->arrow_ele > 0)
            s_ele = sd->arrow_ele;
        sd->state.arrow_atk = 1;
    }

    int atkmax = watk;
    int atkmax_ = watk_;

    if (atkmin > atkmax && !(sd->state.arrow_atk))
        atkmin = atkmax;
    if (atkmin_ > atkmax_)
        atkmin_ = atkmax_;

    /// Double attack?
    bool da = 0;
    if (sd->double_rate > 0)
        da = MRAND(100) < sd->double_rate;

    if (sd->overrefine > 0)
        wd.damage += MPRAND(1, sd->overrefine);
    if (sd->overrefine_ > 0)
        wd.damage2 += MPRAND(1, sd->overrefine_);

    int cri = 0;
    if (da == 0)
    {
        cri = battle_get_critical(&sd->bl);

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
        if (t_mode & 0x20)
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
            int target_count = battle_counttargeted(target, &sd->bl, battle_config.vit_penaly_count_lv);
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
            int target_count = battle_counttargeted(target, &sd->bl, battle_config.vit_penaly_count_lv);
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

        int t_def = def2 * 8 / 10;
        int vitbonusmax = (t_vit / 20) * (t_vit / 20) - 1;
        if (sd->ignore_def_ele & (1 << t_ele) || sd->ignore_def_race & (1 << t_race))
            idef_flag = 1;
        if (sd->ignore_def_ele_ & (1 << t_ele) || sd->ignore_def_race_ & (1 << t_race))
            idef_flag_ = 1;
        if (t_mode & 0x20)
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

    wd.damage += battle_get_atk2(&sd->bl);
    wd.damage2 += battle_get_atk_2(&sd->bl);

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
        wd.dmg_lv = ATK_FLEE;
    }
    else
    {
        wd.dmg_lv = ATK_DEF;
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
        int dmg = wd.damage, dmg2 = wd.damage2;
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
        wd.dmg_lv = ATK_LUCKY;
    }

    if (battle_config.enemy_perfect_flee)
    {
        if (tmd && wd.div_ < 255 && MRAND(1000) < battle_get_flee2(target))
        {
            wd.damage = wd.damage2 = 0;
            wd.type = 0x0b;
            wd.dmg_lv = ATK_LUCKY;
        }
    }

    // plant flag, but the comment said "robust"
    if (t_mode & 0x40)
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
            int d1 = wd.damage + wd.damage2, d2 = wd.damage2;
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
struct Damage battle_calc_weapon_attack(struct block_list *src, struct block_list *target)
{
    struct Damage wd = {};
    nullpo_retr(wd, src);
    nullpo_retr(wd, target);

    struct map_session_data *sd = NULL;
    struct mob_data *md = NULL;
    if (src->type == BL_PC)
    {
        sd = (struct map_session_data *) src;
        wd = battle_calc_pc_weapon_attack(sd, target);
    }
    else if (src->type == BL_MOB)
    {
        md = (struct mob_data *) src;
        wd = battle_calc_mob_weapon_attack(md, target);
    }
    else
        return wd;


    if (battle_config.equipment_breaking && sd
        && (wd.damage > 0 || wd.damage2 > 0))
    {
        if (sd->status.weapon && sd->status.weapon != 11)
        {
            int breakrate = 1;
            if (wd.type == 0x0a)
                breakrate *= 2;
            if (breakrate >= 10000 || MRAND(10000) < breakrate * battle_config.equipment_break_rate / 100)
            {
                pc_breakweapon(sd);
                memset(&wd, 0, sizeof(wd));
            }
        }
    }

    if (battle_config.equipment_breaking && target->type == BL_PC
        && (wd.damage > 0 || wd.damage2 > 0))
    {
        int breakrate = 1;
        if (wd.type == 0x0a)
            breakrate *= 2;
        if (breakrate >= 10000 || MRAND(10000) < breakrate * battle_config.equipment_break_rate / 100)
        {
            pc_breakarmor((struct map_session_data *) target);
        }
    }

    return wd;
}

/*==========================================
 * 通常攻撃処理まとめ
 *------------------------------------------
 */
int battle_weapon_attack(struct block_list *src, struct block_list *target,
                          unsigned int tick, int flag)
{
    struct map_session_data *sd = NULL;
    struct status_change *t_sc_data = battle_get_sc_data(target);
    short *opt1;
    int damage, rdamage = 0;
    struct Damage wd;

    nullpo_retr(0, src);
    nullpo_retr(0, target);

    if (src->type == BL_PC)
        sd = (struct map_session_data *) src;

    if (src->prev == NULL || target->prev == NULL)
        return 0;
    if (src->type == BL_PC && pc_isdead(sd))
        return 0;
    if (target->type == BL_PC
        && pc_isdead((struct map_session_data *) target))
        return 0;

    opt1 = battle_get_opt1(src);
    if (opt1 && *opt1 > 0)
    {
        battle_stopattack(src);
        return 0;
    }

    if (battle_check_target(src, target, BCT_ENEMY) > 0 &&
        battle_check_range(src, target, 0))
    {
        // 攻撃対象となりうるので攻撃
        if (sd && sd->status.weapon == 11)
        {
            if (sd->equip_index[10] >= 0)
            {
                if (battle_config.arrow_decrement)
                    pc_delitem(sd, sd->equip_index[10], 1, 0);
            }
            else
            {
                clif_arrow_fail(sd, 0);
                return 0;
            }
        }
        if (flag & 0x8000)
        {
            if (sd && battle_config.pc_attack_direction_change)
                sd->dir = sd->head_dir =
                    map_calc_dir(src, target->x, target->y);
            else if (src->type == BL_MOB
                     && battle_config.monster_attack_direction_change)
                ((struct mob_data *) src)->dir =
                    map_calc_dir(src, target->x, target->y);
        }
        else
            wd = battle_calc_weapon_attack(src, target);

        // significantly increase injuries for hasted characters
        if (wd.damage > 0 && (t_sc_data[SC_HASTE].timer != -1))
        {
            wd.damage = (wd.damage * (16 + t_sc_data[SC_HASTE].val1)) >> 4;
        }

        if (wd.damage > 0
            && t_sc_data[SC_PHYS_SHIELD].timer != -1 && target->type == BL_PC)
        {
            int reduction = t_sc_data[SC_PHYS_SHIELD].val1;
            if (reduction > wd.damage)
                reduction = wd.damage;

            wd.damage -= reduction;
            MAP_LOG_PC(((struct map_session_data *) target),
                        "MAGIC-ABSORB-DMG %d", reduction);
        }

        if ((damage = wd.damage + wd.damage2) > 0 && src != target)
        {
            if (wd.flag & BF_SHORT)
            {
                if (target->type == BL_PC)
                {
                    struct map_session_data *tsd =
                        (struct map_session_data *) target;
                    if (tsd && tsd->short_weapon_damage_return > 0)
                    {
                        rdamage +=
                            damage * tsd->short_weapon_damage_return / 100;
                        if (rdamage < 1)
                            rdamage = 1;
                    }
                }
            }
            else if (wd.flag & BF_LONG)
            {
                if (target->type == BL_PC)
                {
                    struct map_session_data *tsd =
                        (struct map_session_data *) target;
                    if (tsd && tsd->long_weapon_damage_return > 0)
                    {
                        rdamage +=
                            damage * tsd->long_weapon_damage_return / 100;
                        if (rdamage < 1)
                            rdamage = 1;
                    }
                }
            }

            if (rdamage > 0)
                clif_damage(src, src, tick, wd.amotion, 0, rdamage, 1, 4, 0);
        }

        if (wd.div_ == 255 && sd)
        {                       //三段掌
            int delay =
                1000 - 4 * battle_get_agi(src) - 2 * battle_get_dex(src);
            sd->attackabletime = sd->canmove_tick = tick + delay;
        }
        else
        {
            clif_damage(src, target, tick, wd.amotion, wd.dmotion,
                         wd.damage, wd.div_, wd.type, wd.damage2);
            //二刀流左手とカタール追撃のミス表示(無理やり〜)
            if (sd && sd->status.weapon >= 16 && wd.damage2 == 0)
                clif_damage(src, target, tick + 10, wd.amotion, wd.dmotion,
                             0, 1, 0, 0);
        }
        map_freeblock_lock();

        if (src->type == BL_PC)
        {
            int weapon_index = sd->equip_index[9];
            int weapon = 0;
            if (sd->inventory_data[weapon_index]
                && sd->status.inventory[weapon_index].equip & 0x2)
                weapon = sd->inventory_data[weapon_index]->nameid;

            map_log("PC%d %d:%d,%d WPNDMG %s%d %d FOR %d WPN %d",
                     sd->status.char_id, src->m, src->x, src->y,
                     (target->type == BL_PC) ? "PC" : "MOB",
                     (target->type ==
                      BL_PC) ? ((struct map_session_data *) target)->
                     status.char_id : target->id,
                     (target->type ==
                      BL_PC) ? 0 : ((struct mob_data *) target)->mob_class,
                     wd.damage + wd.damage2, weapon);
        }

        if (target->type == BL_PC)
        {
            struct map_session_data *sd2 = (struct map_session_data *) target;
            map_log("PC%d %d:%d,%d WPNINJURY %s%d %d FOR %d",
                     sd2->status.char_id, target->m, target->x, target->y,
                     (src->type == BL_PC) ? "PC" : "MOB",
                     (src->type ==
                      BL_PC) ? ((struct map_session_data *) src)->
                     status.char_id : src->id,
                     (src->type ==
                      BL_PC) ? 0 : ((struct mob_data *) src)->mob_class,
                     wd.damage + wd.damage2);
        }

        battle_damage(src, target, (wd.damage + wd.damage2));

        if (rdamage > 0)
            battle_damage(target, src, rdamage);

        map_freeblock_unlock();
    }
    return wd.dmg_lv;
}

int battle_check_undead(int race, int element)
{
    if (battle_config.undead_detect_type == 0)
    {
        if (element == 9)
            return 1;
    }
    else if (battle_config.undead_detect_type == 1)
    {
        if (race == 1)
            return 1;
    }
    else
    {
        if (element == 9 || race == 1)
            return 1;
    }
    return 0;
}

/*==========================================
 * 敵味方判定(1=肯定,0=否定,-1=エラー)
 * flag&0xf0000 = 0x00000:敵じゃないか判定（ret:1＝敵ではない）
 *                              = 0x10000:パーティー判定（ret:1=パーティーメンバ)
 *                              = 0x20000:全て(ret:1=敵味方両方)
 *                              = 0x40000:敵か判定(ret:1=敵)
 *                              = 0x50000:パーティーじゃないか判定(ret:1=パーティでない)
 *------------------------------------------
 */
int battle_check_target(struct block_list *src, struct block_list *target,
                         int flag)
{
    int s_p, t_p;
    struct block_list *ss = src;

    nullpo_retr(0, src);
    nullpo_retr(0, target);

    if (flag & 0x40000)
    {                           // 反転フラグ
        int ret = battle_check_target(src, target, flag & 0x30000);
        if (ret != -1)
            return !ret;
        return -1;
    }

    if (flag & 0x20000)
    {
        if (target->type == BL_MOB || target->type == BL_PC)
            return 1;
        else
            return -1;
    }

    if (target->type == BL_PC
        && ((struct map_session_data *) target)->invincible_timer != -1)
        return -1;

    // Mobでmaster_idがあってspecial_mob_aiなら、召喚主を求める
    if (src->type == BL_MOB)
    {
        struct mob_data *md = (struct mob_data *) src;
        if (md && md->master_id > 0)
        {
            if (md->master_id == target->id)    // 主なら肯定
                return 1;
            if (md->state.special_mob_ai)
            {
                if (target->type == BL_MOB)
                {               //special_mob_aiで対象がMob
                    struct mob_data *tmd = (struct mob_data *) target;
                    if (tmd)
                    {
                        if (tmd->master_id != md->master_id)    //召喚主が一緒でなければ否定
                            return 0;
                        else
                        {       //召喚主が一緒なので肯定したいけど自爆は否定
                            if (md->state.special_mob_ai > 2)
                                return 0;
                            else
                                return 1;
                        }
                    }
                }
            }
            if ((ss = map_id2bl(md->master_id)) == NULL)
                return -1;
        }
    }

    if (src == target || ss == target)  // 同じなら肯定
        return 1;

    if (target->type == BL_PC
        && pc_isinvisible((struct map_session_data *) target))
        return -1;

    if (src->prev == NULL ||    // 死んでるならエラー
        (src->type == BL_PC && pc_isdead((struct map_session_data *) src)))
        return -1;

    if ((ss->type == BL_PC && target->type == BL_MOB) ||
        (ss->type == BL_MOB && target->type == BL_PC))
        return 0;               // PCvsMOBなら否定

    s_p = battle_get_party_id(ss);

    t_p = battle_get_party_id(target);

    if (flag & 0x10000)
    {
        if (s_p && t_p && s_p == t_p)   // 同じパーティなら肯定（味方）
            return 1;
        else                    // パーティ検索なら同じパーティじゃない時点で否定
            return 0;
    }

//printf("ss:%d src:%d target:%d flag:0x%x %d %d ",ss->id,src->id,target->id,flag,src->type,target->type);
//printf("p:%d %d g:%d %d\n",s_p,t_p,s_g,t_g);

    if (ss->type == BL_PC && target->type == BL_PC)
    {                           // 両方PVPモードなら否定（敵）
        if (maps[ss->m].flag.pvp
            || pc_iskiller((struct map_session_data *) ss,
                            (struct map_session_data *) target))
        {                       // [MouseJstr]
            if (battle_config.pk_mode)
                return 1;       // prevent novice engagement in pk_mode [Valaris]
            else if (maps[ss->m].flag.pvp_noparty && s_p > 0 && t_p > 0
                     && s_p == t_p)
                return 1;
            return 0;
        }
    }

    return 1;                   // 該当しないので無関係人物（まあ敵じゃないので味方）
}

/*==========================================
 * 射程判定
 *------------------------------------------
 */
int battle_check_range(struct block_list *src, struct block_list *bl,
                        int range)
{

    int dx, dy;
    struct walkpath_data wpd;
    int arange;

    nullpo_retr(0, src);
    nullpo_retr(0, bl);

    dx = abs(bl->x - src->x);
    dy = abs(bl->y - src->y);
    arange = ((dx > dy) ? dx : dy);

    if (src->m != bl->m)        // 違うマップ
        return 0;

    if (range > 0 && range < arange)    // 遠すぎる
        return 0;

    if (arange < 2)             // 同じマスか隣接
        return 1;

    // 障害物判定
    wpd.path_len = 0;
    wpd.path_pos = 0;
    wpd.path_half = 0;
    if (path_search(&wpd, src->m, src->x, src->y, bl->x, bl->y, 0x10001) !=
        -1)
        return 1;

    dx = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
    dy = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
    return (path_search(&wpd, src->m, src->x + dx, src->y + dy,
                         bl->x - dx, bl->y - dy, 0x10001) != -1) ? 1 : 0;
}

/*==========================================
 * 設定ファイルを読み込む
 *------------------------------------------
 */
int battle_config_read(const char *cfgName)
{
    int i;
    char line[1024], w1[1024], w2[1024];
    FILE *fp;
    static int count = 0;

    if ((count++) == 0)
    {
        battle_config.warp_point_debug = 0;
        battle_config.enemy_critical = 0;
        battle_config.enemy_critical_rate = 100;
        battle_config.enemy_str = 1;
        battle_config.enemy_perfect_flee = 0;
        battle_config.cast_rate = 100;
        battle_config.delay_rate = 100;
        battle_config.delay_dependon_dex = 0;
        battle_config.sdelay_attack_enable = 0;
        battle_config.left_cardfix_to_right = 0;
        battle_config.pc_damage_delay = 1;
        battle_config.pc_damage_delay_rate = 100;
        battle_config.defnotenemy = 1;
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
        battle_config.zeny_penalty = 0;
        battle_config.restart_hp_rate = 0;
        battle_config.restart_sp_rate = 0;
        battle_config.monster_hp_rate = 100;
        battle_config.monster_max_aspd = 199;
        battle_config.gm_allequip = 0;
        battle_config.wp_rate = 100;
        battle_config.pp_rate = 100;
        battle_config.monster_active_enable = 1;
        battle_config.monster_damage_delay_rate = 100;
        battle_config.monster_loot_type = 0;
        battle_config.mob_count_rate = 100;
        battle_config.pc_invincible_time = 5000;
        battle_config.skill_min_damage = 0;
        battle_config.finger_offensive_type = 0;
        battle_config.heal_exp = 0;
        battle_config.resurrection_exp = 0;
        battle_config.shop_exp = 0;
        battle_config.combo_delay_rate = 100;
        battle_config.item_check = 1;
        battle_config.wedding_modifydisplay = 0;
        battle_config.natural_healhp_interval = 6000;
        battle_config.natural_healsp_interval = 8000;
        battle_config.natural_heal_skill_interval = 10000;
        battle_config.natural_heal_weight_rate = 50;
        battle_config.itemheal_regeneration_factor = 1;
        battle_config.item_name_override_grffile = 1;
        battle_config.arrow_decrement = 1;
        battle_config.max_aspd = 199;
        battle_config.max_hp = 32500;
        battle_config.max_sp = 32500;
        battle_config.max_lv = 99;  // [MouseJstr]
        battle_config.max_parameter = 99;
        battle_config.save_clothcolor = 0;
        battle_config.undead_detect_type = 0;
        battle_config.pc_auto_counter_type = 1;
        battle_config.monster_auto_counter_type = 1;
        battle_config.agi_penaly_type = 0;
        battle_config.agi_penaly_count = 3;
        battle_config.agi_penaly_num = 0;
        battle_config.agi_penaly_count_lv = ATK_FLEE;
        battle_config.vit_penaly_type = 0;
        battle_config.vit_penaly_count = 3;
        battle_config.vit_penaly_num = 0;
        battle_config.vit_penaly_count_lv = ATK_DEF;
        battle_config.player_defense_type = 0;
        battle_config.monster_defense_type = 0;
        battle_config.magic_defense_type = 0;
        battle_config.pc_cloak_check_type = 0;
        battle_config.monster_cloak_check_type = 0;
        battle_config.pc_attack_direction_change = 1;
        battle_config.monster_attack_direction_change = 1;
        battle_config.pc_undead_nofreeze = 0;
        battle_config.monster_class_change_full_recover = 0;
        battle_config.produce_item_name_input = 1;
        battle_config.produce_potion_name_input = 1;
        battle_config.making_arrow_name_input = 1;
        battle_config.holywater_name_input = 1;
        battle_config.chat_warpportal = 0;
        battle_config.mob_warpportal = 0;
        battle_config.dead_branch_active = 0;
        battle_config.show_steal_in_same_party = 0;
        battle_config.pc_attack_attr_none = 0;
        battle_config.mob_attack_attr_none = 1;
        battle_config.mob_ghostring_fix = 0;
        battle_config.gx_allhit = 0;
        battle_config.gx_cardfix = 0;
        battle_config.gx_dupele = 1;
        battle_config.gx_disptype = 1;
        battle_config.hide_GM_session = 0;
        battle_config.unit_movement_type = 0;
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
        battle_config.equipment_breaking = 0;   // [Valaris]
        battle_config.equipment_break_rate = 100;   // [Valaris]
        battle_config.pk_mode = 0;  // [Valaris]
        battle_config.multi_level_up = 0;   // [Valaris]
        battle_config.backstab_bow_penalty = 0; // Akaru
        battle_config.show_mob_hp = 0;  // [Valaris]
        battle_config.hack_info_GM_level = 60;  // added by [Yor] (default: 60, GM level)
        battle_config.any_warp_GM_min_level = 20;   // added by [Yor]
        battle_config.packet_ver_flag = 63; // added by [Yor]
        battle_config.min_hair_style = 0;
        battle_config.max_hair_style = 20;
        battle_config.min_hair_color = 0;
        battle_config.max_hair_color = 9;
        battle_config.min_cloth_color = 0;
        battle_config.max_cloth_color = 4;

        battle_config.castrate_dex_scale = 150;

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

    fp = fopen_(cfgName, "r");
    if (fp == NULL)
    {
        printf("file not found: %s\n", cfgName);
        return 1;
    }
    while (fgets(line, 1020, fp))
    {
        const struct
        {
            char str[128];
            int *val;
        } data[] =
        {
            {"warp_point_debug", &battle_config.warp_point_debug},
            {"enemy_critical", &battle_config.enemy_critical},
            {"enemy_critical_rate", &battle_config.enemy_critical_rate},
            {"enemy_str", &battle_config.enemy_str},
            {"enemy_perfect_flee", &battle_config.enemy_perfect_flee},
            {"casting_rate", &battle_config.cast_rate},
            {"delay_rate", &battle_config.delay_rate},
            {"delay_dependon_dex", &battle_config.delay_dependon_dex},
            {"skill_delay_attack_enable", &battle_config.sdelay_attack_enable},
            {"left_cardfix_to_right", &battle_config.left_cardfix_to_right},
            {"player_damage_delay", &battle_config.pc_damage_delay},
            {"player_damage_delay_rate", &battle_config.pc_damage_delay_rate},
            {"defunit_not_enemy", &battle_config.defnotenemy},
            {"random_monster_checklv", &battle_config.random_monster_checklv},
            {"attribute_recover", &battle_config.attr_recover},
            {"flooritem_lifetime", &battle_config.flooritem_lifetime},
            {"item_auto_get", &battle_config.item_auto_get},
            {"drop_pickup_safety_zone", &battle_config.drop_pickup_safety_zone},
            {"item_first_get_time", &battle_config.item_first_get_time},
            {"item_second_get_time", &battle_config.item_second_get_time},
            {"item_third_get_time", &battle_config.item_third_get_time},
            {"item_rate", &battle_config.item_rate},
            {"drop_rate0item", &battle_config.drop_rate0item},
            {"base_exp_rate", &battle_config.base_exp_rate},
            {"job_exp_rate", &battle_config.job_exp_rate},
            {"pvp_exp", &battle_config.pvp_exp},
            {"death_penalty_type", &battle_config.death_penalty_type},
            {"death_penalty_base", &battle_config.death_penalty_base},
            {"death_penalty_job", &battle_config.death_penalty_job},
            {"zeny_penalty", &battle_config.zeny_penalty},
            {"restart_hp_rate", &battle_config.restart_hp_rate},
            {"restart_sp_rate", &battle_config.restart_sp_rate},
            {"monster_hp_rate", &battle_config.monster_hp_rate},
            {"monster_max_aspd", &battle_config.monster_max_aspd},
            {"atcommand_spawn_quantity_limit", &battle_config.atc_spawn_quantity_limit},
            {"gm_all_equipment", &battle_config.gm_allequip},
            {"weapon_produce_rate", &battle_config.wp_rate},
            {"potion_produce_rate", &battle_config.pp_rate},
            {"monster_active_enable", &battle_config.monster_active_enable},
            {"monster_damage_delay_rate", &battle_config.monster_damage_delay_rate},
            {"monster_loot_type", &battle_config.monster_loot_type},
            {"mob_count_rate", &battle_config.mob_count_rate},
            {"player_invincible_time", &battle_config.pc_invincible_time},
            {"skill_min_damage", &battle_config.skill_min_damage},
            {"finger_offensive_type", &battle_config.finger_offensive_type},
            {"heal_exp", &battle_config.heal_exp},
            {"resurrection_exp", &battle_config.resurrection_exp},
            {"shop_exp", &battle_config.shop_exp},
            {"combo_delay_rate", &battle_config.combo_delay_rate},
            {"item_check", &battle_config.item_check},
            {"wedding_modifydisplay", &battle_config.wedding_modifydisplay},
            {"natural_healhp_interval", &battle_config.natural_healhp_interval},
            {"natural_healsp_interval", &battle_config.natural_healsp_interval},
            {"natural_heal_skill_interval", &battle_config.natural_heal_skill_interval},
            {"natural_heal_weight_rate", &battle_config.natural_heal_weight_rate},
            {"itemheal_regeneration_factor", &battle_config.itemheal_regeneration_factor},
            {"item_name_override_grffile", &battle_config.item_name_override_grffile},
            {"arrow_decrement", &battle_config.arrow_decrement},
            {"max_aspd", &battle_config.max_aspd},
            {"max_hp", &battle_config.max_hp},
            {"max_sp", &battle_config.max_sp},
            {"max_lv", &battle_config.max_lv},
            {"max_parameter", &battle_config.max_parameter},
            {"save_clothcolor", &battle_config.save_clothcolor},
            {"undead_detect_type", &battle_config.undead_detect_type},
            {"player_auto_counter_type", &battle_config.pc_auto_counter_type},
            {"monster_auto_counter_type", &battle_config.monster_auto_counter_type},
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
            {"magic_defense_type", &battle_config.magic_defense_type},
            {"player_cloak_check_type", &battle_config.pc_cloak_check_type},
            {"monster_cloak_check_type", &battle_config.monster_cloak_check_type},
            {"player_attack_direction_change", &battle_config.pc_attack_direction_change},
            {"monster_attack_direction_change", &battle_config.monster_attack_direction_change},
            {"monster_class_change_full_recover", &battle_config.monster_class_change_full_recover},
            {"produce_item_name_input", &battle_config.produce_item_name_input},
            {"produce_potion_name_input", &battle_config.produce_potion_name_input},
            {"making_arrow_name_input", &battle_config.making_arrow_name_input},
            {"holywater_name_input", &battle_config.holywater_name_input},
            {"chat_warpportal", &battle_config.chat_warpportal},
            {"mob_warpportal", &battle_config.mob_warpportal},
            {"dead_branch_active", &battle_config.dead_branch_active},
            {"show_steal_in_same_party", &battle_config.show_steal_in_same_party},
            {"mob_attack_attr_none", &battle_config.mob_attack_attr_none},
            {"mob_ghostring_fix", &battle_config.mob_ghostring_fix},
            {"pc_attack_attr_none", &battle_config.pc_attack_attr_none},
            {"gx_allhit", &battle_config.gx_allhit},
            {"gx_cardfix", &battle_config.gx_cardfix},
            {"gx_dupele", &battle_config.gx_dupele},
            {"gx_disptype", &battle_config.gx_disptype},
            {"hide_GM_session", &battle_config.hide_GM_session},
            {"unit_movement_type", &battle_config.unit_movement_type},
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
            {"equipment_breaking", &battle_config.equipment_breaking},   // [Valaris]
            {"equipment_break_rate", &battle_config.equipment_break_rate},   // [Valaris]
            {"pk_mode", &battle_config.pk_mode}, // [Valaris]
            {"multi_level_up", &battle_config.multi_level_up},   // [Valaris]
            {"backstab_bow_penalty", &battle_config.backstab_bow_penalty},
            {"show_mob_hp", &battle_config.show_mob_hp}, // [Valaris]
            {"hack_info_GM_level", &battle_config.hack_info_GM_level},   // added by [Yor]
            {"any_warp_GM_min_level", &battle_config.any_warp_GM_min_level}, // added by [Yor]
            {"packet_ver_flag", &battle_config.packet_ver_flag}, // added by [Yor]
            {"min_hair_style", &battle_config.min_hair_style},   // added by [MouseJstr]
            {"max_hair_style", &battle_config.max_hair_style},   // added by [MouseJstr]
            {"min_hair_color", &battle_config.min_hair_color},   // added by [MouseJstr]
            {"max_hair_color", &battle_config.max_hair_color},   // added by [MouseJstr]
            {"min_cloth_color", &battle_config.min_cloth_color}, // added by [MouseJstr]
            {"max_cloth_color", &battle_config.max_cloth_color}, // added by [MouseJstr]
            {"castrate_dex_scale", &battle_config.castrate_dex_scale},   // added by [MouseJstr]
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
        if (sscanf(line, "%[^:]:%s", w1, w2) != 2)
            continue;
        for (i = 0; i < sizeof(data) / (sizeof(data[0])); i++)
            if (strcasecmp(w1, data[i].str) == 0)
                *data[i].val = config_switch(w2);

        if (strcasecmp(w1, "import") == 0)
            battle_config_read(w2);
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
        if (battle_config.natural_healhp_interval < NATURAL_HEAL_INTERVAL)
            battle_config.natural_healhp_interval = NATURAL_HEAL_INTERVAL;
        if (battle_config.natural_healsp_interval < NATURAL_HEAL_INTERVAL)
            battle_config.natural_healsp_interval = NATURAL_HEAL_INTERVAL;
        if (battle_config.natural_heal_skill_interval < NATURAL_HEAL_INTERVAL)
            battle_config.natural_heal_skill_interval = NATURAL_HEAL_INTERVAL;
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

        if (battle_config.item_drop_common_min < 1) // Added by TyrNemesis^
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

        if (battle_config.hack_info_GM_level < 0)   // added by [Yor]
            battle_config.hack_info_GM_level = 0;
        else if (battle_config.hack_info_GM_level > 100)
            battle_config.hack_info_GM_level = 100;

        if (battle_config.any_warp_GM_min_level < 0)    // added by [Yor]
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

        // at least 1 client must be accepted
        if ((battle_config.packet_ver_flag & 63) == 0)  // added by [Yor]
            battle_config.packet_ver_flag = 63; // accept all clients

    }

    return 0;
}
