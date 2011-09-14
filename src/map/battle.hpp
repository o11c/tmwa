#ifndef BATTLE_HPP
#define BATTLE_HPP

# include "battle.structs.hpp"

# include "map.structs.hpp"

inline bool operator < (AttackResult lhs, AttackResult rhs) { return static_cast<int32_t>(lhs) < static_cast<int32_t>(rhs); }
inline bool operator <= (AttackResult lhs, AttackResult rhs) { return static_cast<int32_t>(lhs) <= static_cast<int32_t>(rhs); }
inline bool operator > (AttackResult lhs, AttackResult rhs) { return static_cast<int32_t>(lhs) > static_cast<int32_t>(rhs); }
inline bool operator >= (AttackResult lhs, AttackResult rhs) { return static_cast<int32_t>(lhs) >= static_cast<int32_t>(rhs); }

/// Elemental damage modifiers (read in pc.cpp)
extern int32_t attr_fix_table[4][10][10];

/// flags for battle_calc_damage
const int32_t
    BF_WEAPON = 0x0001,
    BF_MAGIC = 0x0002,
    BF_MISC = 0x0004,
    BF_SHORT = 0x0010,
    BF_LONG = 0x0040,
    BF_WEAPONMASK = 0x000f,
    BF_RANGEMASK = 0x00f0;

int32_t battle_damage(BlockList *bl, BlockList *target, int32_t damage);
int32_t battle_heal(BlockList *bl, BlockList *target, int32_t hp, int32_t sp);

AttackResult battle_weapon_attack(BlockList *bl, BlockList *target, tick_t tick);

bool battle_is_unarmed(BlockList *bl) __attribute__((pure));
Direction battle_get_dir(BlockList *bl);
int32_t battle_get_level(BlockList *bl);
int32_t battle_get_range(BlockList *bl);
int32_t battle_get_hp(BlockList *bl);
int32_t battle_get_max_hp(BlockList *bl);
int32_t battle_get_str(BlockList *bl);
int32_t battle_get_agi(BlockList *bl);
int32_t battle_get_vit(BlockList *bl);
int32_t battle_get_int(BlockList *bl);
int32_t battle_get_dex(BlockList *bl);
int32_t battle_get_luk(BlockList *bl);
int32_t battle_get_def(BlockList *bl);
int32_t battle_get_mdef(BlockList *bl);
int32_t battle_get_speed(BlockList *bl);
int32_t battle_get_adelay(BlockList *bl);
int32_t battle_get_amotion(BlockList *bl);
int32_t battle_get_dmotion(BlockList *bl);
int32_t battle_get_element(BlockList *bl);
int32_t battle_get_stat(SP stat_id, BlockList *bl);

struct status_change *battle_get_sc_data(BlockList *bl);
int16_t *battle_get_sc_count(BlockList *bl);
int16_t *battle_get_opt1(BlockList *bl);
int16_t *battle_get_opt2(BlockList *bl);
int16_t *battle_get_opt3(BlockList *bl);
int16_t *battle_get_option(BlockList *bl);

bool battle_check_target(BlockList *src, BlockList *target);
bool battle_check_range(BlockList *src, BlockList *bl, int32_t range);

extern struct Battle_Config
{
    int32_t enemy_critical;
    int32_t enemy_critical_rate;
    int32_t enemy_str;
    int32_t enemy_perfect_flee;
    int32_t sdelay_attack_enable;
    int32_t pc_damage_delay;
    int32_t pc_damage_delay_rate;
    int32_t random_monster_checklv;
    int32_t attr_recover;
    int32_t flooritem_lifetime;
    int32_t item_auto_get;
    int32_t item_first_get_time;
    int32_t item_second_get_time;
    int32_t item_third_get_time;
    int32_t base_exp_rate;
    int32_t job_exp_rate;
    int32_t drop_rate0item;
    int32_t death_penalty_type;
    int32_t death_penalty_base, death_penalty_job;
    int32_t pvp_exp;
    int32_t restart_hp_rate;
    int32_t restart_sp_rate;
    int32_t monster_hp_rate;
    int32_t monster_max_aspd;
    int32_t atc_spawn_quantity_limit;
    int32_t monster_active_enable;
    int32_t monster_damage_delay_rate;
    int32_t monster_loot_type;
    int32_t mob_count_rate;
    int32_t pc_invincible_time;
    int32_t skill_min_damage;
    int32_t natural_healhp_interval;
    int32_t natural_healsp_interval;
    int32_t natural_heal_skill_interval;
    int32_t natural_heal_weight_rate;
    int32_t max_aspd;
    int32_t max_hp;
    int32_t max_sp;
    int32_t max_lv;
    int32_t max_parameter;
    int32_t undead_detect_type;
    int32_t agi_penaly_type;
    int32_t agi_penaly_count;
    int32_t agi_penaly_num;
    int32_t vit_penaly_type;
    int32_t vit_penaly_count;
    int32_t vit_penaly_num;
    int32_t player_defense_type;
    int32_t monster_defense_type;
    int32_t pc_attack_direction_change;
    int32_t monster_attack_direction_change;
    int32_t dead_branch_active;
    int32_t mob_attack_attr_none;

    int32_t item_rate_common;
    int32_t item_rate_card;
    int32_t item_rate_equip;
    int32_t item_rate_heal;
    int32_t item_rate_use;

    int32_t item_drop_common_min;
    int32_t item_drop_common_max;
    int32_t item_drop_card_min;
    int32_t item_drop_card_max;
    int32_t item_drop_equip_min;
    int32_t item_drop_equip_max;
    int32_t item_drop_heal_min;
    int32_t item_drop_heal_max;
    int32_t item_drop_use_min;
    int32_t item_drop_use_max;

    int32_t prevent_logout;

    int32_t alchemist_summon_reward;
    int32_t maximum_level;
    int32_t drops_by_luk;
    int32_t monsters_ignore_gm;
    int32_t multi_level_up;
    int32_t pk_mode;

    int32_t agi_penaly_count_lv;
    int32_t vit_penaly_count_lv;

    int32_t hide_GM_session;
    int32_t invite_request_check;
    int32_t disp_experience;

    int32_t hack_info_GM_level;
    int32_t any_warp_GM_min_level;

    int32_t area_size;

    int32_t chat_lame_penalty;
    int32_t chat_spam_threshold;
    int32_t chat_spam_flood;
    int32_t chat_spam_ban;
    int32_t chat_spam_warn;
    int32_t chat_maxline;

    int32_t packet_spam_threshold;
    int32_t packet_spam_flood;
    int32_t packet_spam_kick;

    int32_t mask_ip_gms;

    int32_t drop_pickup_safety_zone;
    int32_t itemheal_regeneration_factor;

} battle_config;

int32_t battle_config_read(const char *cfgName);

#endif // BATTLE_HPP
