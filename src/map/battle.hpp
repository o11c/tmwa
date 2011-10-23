#ifndef BATTLE_HPP
#define BATTLE_HPP

# include "battle.structs.hpp"

# include "main.structs.hpp"

inline bool operator < (AttackResult lhs, AttackResult rhs) { return static_cast<sint32>(lhs) < static_cast<sint32>(rhs); }
inline bool operator <= (AttackResult lhs, AttackResult rhs) { return static_cast<sint32>(lhs) <= static_cast<sint32>(rhs); }
inline bool operator > (AttackResult lhs, AttackResult rhs) { return static_cast<sint32>(lhs) > static_cast<sint32>(rhs); }
inline bool operator >= (AttackResult lhs, AttackResult rhs) { return static_cast<sint32>(lhs) >= static_cast<sint32>(rhs); }

sint32 battle_damage(BlockList *bl, BlockList *target, sint32 damage);
sint32 battle_heal(BlockList *bl, BlockList *target, sint32 hp, sint32 sp);

AttackResult battle_weapon_attack(BlockList *bl, BlockList *target, tick_t tick);

bool battle_is_unarmed(BlockList *bl) __attribute__((pure));
Direction battle_get_dir(BlockList *bl);
level_t battle_get_level(BlockList *bl);
sint32 battle_get_range(BlockList *bl);
sint32 battle_get_hp(BlockList *bl);
sint32 battle_get_max_hp(BlockList *bl);
sint32 battle_get_str(BlockList *bl);
sint32 battle_get_agi(BlockList *bl);
sint32 battle_get_vit(BlockList *bl);
sint32 battle_get_int(BlockList *bl);
sint32 battle_get_dex(BlockList *bl);
sint32 battle_get_luk(BlockList *bl);
sint32 battle_get_def(BlockList *bl);
sint32 battle_get_mdef(BlockList *bl);
interval_t battle_get_speed(BlockList *bl);
interval_t battle_get_adelay(BlockList *bl);
interval_t battle_get_amotion(BlockList *bl);
interval_t battle_get_dmotion(BlockList *bl);
sint32 battle_get_element(BlockList *bl);
sint32 battle_get_stat(SP stat_id, BlockList *bl);

struct status_change *battle_get_sc_data(BlockList *bl);
sint16 *battle_get_sc_count(BlockList *bl);
sint16 *battle_get_opt1(BlockList *bl);
sint16 *battle_get_opt2(BlockList *bl);
sint16 *battle_get_opt3(BlockList *bl);
OPTION *battle_get_option(BlockList *bl);

bool battle_check_target(BlockList *src, BlockList *target);
bool battle_check_range(BlockList *src, BlockList *bl, sint32 range);

extern struct Battle_Config
{
    sint32 enemy_critical;
    sint32 enemy_critical_rate;
    sint32 enemy_str;
    sint32 enemy_perfect_flee;
    sint32 sdelay_attack_enable;
    sint32 pc_damage_delay;
    sint32 pc_damage_delay_rate;
    sint32 random_monster_checklv;
    sint32 attr_recover;
    sint32 flooritem_lifetime;
    sint32 item_auto_get;
    sint32 item_first_get_time;
    sint32 item_second_get_time;
    sint32 item_third_get_time;
    sint32 base_exp_rate;
    sint32 job_exp_rate;
    sint32 drop_rate0item;
    sint32 death_penalty_type;
    sint32 death_penalty_base, death_penalty_job;
    sint32 pvp_exp;
    sint32 restart_hp_rate;
    sint32 restart_sp_rate;
    sint32 monster_hp_rate;
    sint32 monster_max_aspd;
    sint32 atc_spawn_quantity_limit;
    sint32 monster_active_enable;
    sint32 monster_damage_delay_rate;
    sint32 monster_loot_type;
    sint32 mob_count_rate;
    sint32 pc_invincible_time;
    sint32 skill_min_damage;
    sint32 natural_healhp_interval;
    sint32 natural_healsp_interval;
    sint32 natural_heal_skill_interval;
    sint32 natural_heal_weight_rate;
    sint32 max_aspd;
    sint32 max_hp;
    sint32 max_sp;
    sint32 max_lv;
    sint32 max_parameter;
    sint32 undead_detect_type;
    sint32 agi_penaly_type;
    sint32 agi_penaly_count;
    sint32 agi_penaly_num;
    sint32 vit_penaly_type;
    sint32 vit_penaly_count;
    sint32 vit_penaly_num;
    sint32 player_defense_type;
    sint32 monster_defense_type;
    sint32 pc_attack_direction_change;
    sint32 monster_attack_direction_change;
    sint32 dead_branch_active;
    sint32 mob_attack_attr_none;

    sint32 item_rate_common;
    sint32 item_rate_card;
    sint32 item_rate_equip;
    sint32 item_rate_heal;
    sint32 item_rate_use;

    sint32 item_drop_common_min;
    sint32 item_drop_common_max;
    sint32 item_drop_card_min;
    sint32 item_drop_card_max;
    sint32 item_drop_equip_min;
    sint32 item_drop_equip_max;
    sint32 item_drop_heal_min;
    sint32 item_drop_heal_max;
    sint32 item_drop_use_min;
    sint32 item_drop_use_max;

    sint32 prevent_logout;

    sint32 alchemist_summon_reward;
    sint32 maximum_level;
    sint32 drops_by_luk;
    sint32 monsters_ignore_gm;
    sint32 multi_level_up;
    // TODO remove this
    sint32 pk_mode;

    sint32 agi_penaly_count_lv;
    sint32 vit_penaly_count_lv;

    sint32 hide_GM_session;
    sint32 invite_request_check;
    sint32 disp_experience;

    sint32 hack_info_GM_level;
    sint32 any_warp_GM_min_level;

    sint32 area_size;

    sint32 chat_lame_penalty;
    sint32 chat_spam_threshold;
    sint32 chat_spam_flood;
    sint32 chat_spam_ban;
    sint32 chat_spam_warn;
    sint32 chat_maxline;

    sint32 packet_spam_threshold;
    sint32 packet_spam_flood;
    sint32 packet_spam_kick;

    sint32 mask_ip_gms;

    sint32 drop_pickup_safety_zone;
    sint32 itemheal_regeneration_factor;

} battle_config;

sint32 battle_config_read(const char *cfgName);

#endif // BATTLE_HPP
