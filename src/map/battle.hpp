#ifndef BATTLE_H
#define BATTLE_H

#include "../common/timer.hpp"

enum class Direction
{
    S,
    SW,
    W,
    NW,
    N,
    NE,
    E,
    SE,
};

enum class AttackResult
{
    // what is this first?
    ZERO,
    LUCKY,
    FLEE,
    DEF
};
inline bool operator < (AttackResult lhs, AttackResult rhs) { return static_cast<int>(lhs) < static_cast<int>(rhs); }
inline bool operator <= (AttackResult lhs, AttackResult rhs) { return static_cast<int>(lhs) <= static_cast<int>(rhs); }
inline bool operator > (AttackResult lhs, AttackResult rhs) { return static_cast<int>(lhs) > static_cast<int>(rhs); }
inline bool operator >= (AttackResult lhs, AttackResult rhs) { return static_cast<int>(lhs) >= static_cast<int>(rhs); }

struct Damage
{
    int damage, damage2;
    int type, div_;
    int amotion, dmotion;
    int flag;
    AttackResult dmg_lv;
};

/// Elemental damage modifiers (read in pc.cpp)
extern int attr_fix_table[4][10][10];

class MapSessionData;
struct mob_data;
class BlockList;

/// flags for battle_calc_damage
const int
    BF_WEAPON = 0x0001,
    BF_MAGIC = 0x0002,
    BF_MISC = 0x0004,
    BF_SHORT = 0x0010,
    BF_LONG = 0x0040,
    BF_WEAPONMASK = 0x000f,
    BF_RANGEMASK = 0x00f0;

int battle_damage(BlockList *bl, BlockList *target, int damage);
int battle_heal(BlockList *bl, BlockList *target, int hp, int sp);

AttackResult battle_weapon_attack(BlockList *bl, BlockList *target, tick_t tick);

int battle_is_unarmed(BlockList *bl);
Direction battle_get_dir(BlockList *bl);
int battle_get_lv(BlockList *bl);
int battle_get_range(BlockList *bl);
int battle_get_hp(BlockList *bl);
int battle_get_max_hp(BlockList *bl);
int battle_get_str(BlockList *bl);
int battle_get_agi(BlockList *bl);
int battle_get_vit(BlockList *bl);
int battle_get_int(BlockList *bl);
int battle_get_dex(BlockList *bl);
int battle_get_luk(BlockList *bl);
int battle_get_def(BlockList *bl);
int battle_get_mdef(BlockList *bl);
int battle_get_speed(BlockList *bl);
int battle_get_adelay(BlockList *bl);
int battle_get_amotion(BlockList *bl);
int battle_get_dmotion(BlockList *bl);
int battle_get_element(BlockList *bl);
int battle_get_stat(int stat_id, BlockList *bl);

struct status_change *battle_get_sc_data(BlockList *bl);
short *battle_get_sc_count(BlockList *bl);
short *battle_get_opt1(BlockList *bl);
short *battle_get_opt2(BlockList *bl);
short *battle_get_opt3(BlockList *bl);
short *battle_get_option(BlockList *bl);

bool battle_check_target(BlockList *src, BlockList *target);
bool battle_check_range(BlockList *src, BlockList *bl, int range);

extern struct Battle_Config
{
    int enemy_critical;
    int enemy_critical_rate;
    int enemy_str;
    int enemy_perfect_flee;
    int sdelay_attack_enable;
    int pc_damage_delay;
    int pc_damage_delay_rate;
    int random_monster_checklv;
    int attr_recover;
    int flooritem_lifetime;
    int item_auto_get;
    int item_first_get_time;
    int item_second_get_time;
    int item_third_get_time;
    int base_exp_rate;
    int job_exp_rate;
    int drop_rate0item;
    int death_penalty_type;
    int death_penalty_base, death_penalty_job;
    int pvp_exp;
    int restart_hp_rate;
    int restart_sp_rate;
    int monster_hp_rate;
    int monster_max_aspd;
    int atc_spawn_quantity_limit;
    int monster_active_enable;
    int monster_damage_delay_rate;
    int monster_loot_type;
    int mob_count_rate;
    int pc_invincible_time;
    int skill_min_damage;
    int natural_healhp_interval;
    int natural_healsp_interval;
    int natural_heal_skill_interval;
    int natural_heal_weight_rate;
    int max_aspd;
    int max_hp;
    int max_sp;
    int max_lv;
    int max_parameter;
    int undead_detect_type;
    int agi_penaly_type;
    int agi_penaly_count;
    int agi_penaly_num;
    int vit_penaly_type;
    int vit_penaly_count;
    int vit_penaly_num;
    int player_defense_type;
    int monster_defense_type;
    int pc_attack_direction_change;
    int monster_attack_direction_change;
    int dead_branch_active;
    int mob_attack_attr_none;

    int item_rate_common;
    int item_rate_card;
    int item_rate_equip;
    int item_rate_heal;
    int item_rate_use;

    int item_drop_common_min;
    int item_drop_common_max;
    int item_drop_card_min;
    int item_drop_card_max;
    int item_drop_equip_min;
    int item_drop_equip_max;
    int item_drop_heal_min;
    int item_drop_heal_max;
    int item_drop_use_min;
    int item_drop_use_max;

    int prevent_logout;

    int alchemist_summon_reward;
    int maximum_level;
    int drops_by_luk;
    int monsters_ignore_gm;
    int equipment_breaking;
    int equipment_break_rate;
    int multi_level_up;
    int pk_mode;

    int agi_penaly_count_lv;
    int vit_penaly_count_lv;

    int hide_GM_session;
    int invite_request_check;
    int disp_experience;

    int hack_info_GM_level;
    int any_warp_GM_min_level;

    int area_size;

    int chat_lame_penalty;
    int chat_spam_threshold;
    int chat_spam_flood;
    int chat_spam_ban;
    int chat_spam_warn;
    int chat_maxline;

    int packet_spam_threshold;
    int packet_spam_flood;
    int packet_spam_kick;

    int mask_ip_gms;

    int drop_pickup_safety_zone;
    int itemheal_regeneration_factor;

} battle_config;

int battle_config_read(const char *cfgName);

#endif // BATTLE_H
