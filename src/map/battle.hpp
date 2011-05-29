#ifndef BATTLE_H
#define BATTLE_H

#include "map.hpp"

// ダメージ
struct Damage
{
    int damage, damage2;
    int type, div_;
    int amotion, dmotion;
    int flag;
    int dmg_lv;                //囲まれ減算計算用　0:スキル攻撃 ATK_LUCKY,ATK_FLEE,ATK_DEF
};

// 属性表（読み込みはpc.c、battle_attr_fixで使用）
extern int attr_fix_table[4][10][10];

struct map_session_data;
struct mob_data;
struct block_list;

struct Damage battle_calc_weapon_attack(struct block_list *bl, struct block_list *target);


int battle_calc_damage(struct block_list *target, int damage, int div_, int flag);

/// flags for battle_calc_damage
const int
    BF_WEAPON = 0x0001,
    BF_MAGIC = 0x0002,
    BF_MISC = 0x0004,
    BF_SHORT = 0x0010,
    BF_LONG = 0x0040,
    BF_WEAPONMASK = 0x000f,
    BF_RANGEMASK = 0x00f0;

// 実際にHPを増減
int battle_delay_damage(tick_t tick, struct block_list *src,
                        struct block_list *target, int damage);
int battle_damage(struct block_list *bl, struct block_list *target, int damage);
int battle_heal(struct block_list *bl, struct block_list *target, int hp, int sp);

int battle_stopwalking(struct block_list *bl, int type);

// 通常攻撃処理まとめ
int battle_weapon_attack(struct block_list *bl, struct block_list *target,
                          unsigned int tick, int flag);

// 各種パラメータを得る
int battle_counttargeted(struct block_list *bl, struct block_list *src,
                          int target_lv);
int battle_is_unarmed(struct block_list *bl);
Direction battle_get_dir(struct block_list *bl);
int battle_get_lv(struct block_list *bl);
int battle_get_range(struct block_list *bl);
int battle_get_hp(struct block_list *bl);
int battle_get_max_hp(struct block_list *bl);
int battle_get_str(struct block_list *bl);
int battle_get_agi(struct block_list *bl);
int battle_get_vit(struct block_list *bl);
int battle_get_int(struct block_list *bl);
int battle_get_dex(struct block_list *bl);
int battle_get_luk(struct block_list *bl);
int battle_get_def(struct block_list *bl);
int battle_get_mdef(struct block_list *bl);
int battle_get_speed(struct block_list *bl);
int battle_get_adelay(struct block_list *bl);
int battle_get_amotion(struct block_list *bl);
int battle_get_dmotion(struct block_list *bl);
int battle_get_element(struct block_list *bl);
#define battle_get_elem_type(bl) (battle_get_element(bl)%10)
int battle_get_party_id(struct block_list *bl);
int battle_get_race(struct block_list *bl);
int battle_get_mode(struct block_list *bl);
int battle_get_stat(int stat_id /* SP_VIT or similar */ ,
                     struct block_list *bl);

struct status_change *battle_get_sc_data(struct block_list *bl);
short *battle_get_sc_count(struct block_list *bl);
short *battle_get_opt1(struct block_list *bl);
short *battle_get_opt2(struct block_list *bl);
short *battle_get_opt3(struct block_list *bl);
short *battle_get_option(struct block_list *bl);

enum
{
    BCT_NOENEMY = 0x00000,
    BCT_PARTY = 0x10000,
    BCT_ENEMY = 0x40000,
    BCT_NOPARTY = 0x50000,
    BCT_ALL = 0x20000,
    BCT_NOONE = 0x60000,
};

int battle_check_undead(int race, int element);
int battle_check_target(struct block_list *src, struct block_list *target,
                         int flag);
int battle_check_range(struct block_list *src, struct block_list *bl,
                        int range);

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
    int save_clothcolor;
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

    int max_hair_style;
    int max_hair_color;
    int max_cloth_color;

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
