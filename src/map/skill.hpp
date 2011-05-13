#ifndef SKILL_H
#define SKILL_H

#include "../common/timer.hpp"

#include "map.hpp"
#include "magic.hpp"

#define MAX_SKILL_DB			450
#define MAX_SKILL_PRODUCE_DB	 150
#define MAX_SKILL_ARROW_DB	 150
#define MAX_SKILL_ABRA_DB	 350

#define SKILL_POOL_FLAG		0x1 // is a pool skill
#define SKILL_POOL_ACTIVE	0x2 // is an active pool skill
#define SKILL_POOL_ACTIVATED	0x4 // pool skill has been activated (used for clif)

// スキルデータベース
struct skill_db
{
    int  range[MAX_SKILL_LEVEL], hit, inf, pl, nk, max, stat, poolflags, max_raise; // `max' is the global max, `max_raise' is the maximum attainable via skill-ups
    int  num[MAX_SKILL_LEVEL];
    int  cast[MAX_SKILL_LEVEL], delay[MAX_SKILL_LEVEL];
    int  upkeep_time[MAX_SKILL_LEVEL], upkeep_time2[MAX_SKILL_LEVEL];
    int  castcancel, cast_def_rate;
    int  inf2, maxcount, skill_type;
    int  blewcount[MAX_SKILL_LEVEL];
    int  hp[MAX_SKILL_LEVEL], sp[MAX_SKILL_LEVEL], mhp[MAX_SKILL_LEVEL],
        hp_rate[MAX_SKILL_LEVEL], sp_rate[MAX_SKILL_LEVEL],
        zeny[MAX_SKILL_LEVEL];
    int  weapon, state;
    int  itemid[10], amount[10];
    int  castnodex[MAX_SKILL_LEVEL];
};
extern struct skill_db skill_db[MAX_SKILL_DB];

struct skill_name_db
{
    int  id;                    // skill id
    const char *name;                 // search strings
    const char *desc;                 // description that shows up for search's
};
extern struct skill_name_db skill_names[];

struct block_list;
struct map_session_data;
struct skill_unit;
struct skill_unit_group;

int  do_init_skill (void);

// スキルデータベースへのアクセサ
int  skill_get_hit (int id);
int  skill_get_inf (int id);
int  skill_get_pl (int id);
int  skill_get_max (int id);
int  skill_get_max_raise (int id);
int  skill_get_range (int id, int lv);
int  skill_get_sp (int id, int lv);
int  skill_get_num (int id, int lv);
int  skill_get_delay (int id, int lv);
int  skill_get_castdef (int id);
int  skill_get_inf2 (int id);
int  skill_get_maxcount (int id);
int  skill_get_blewcount (int id, int lv);

// スキルの使用
int  skill_use_id (struct map_session_data *sd, int target_id,
                   int skill_num, int skill_lv);
int  skill_use_pos (struct map_session_data *sd,
                    int skill_x, int skill_y, int skill_num, int skill_lv);

int  skill_castend_map (struct map_session_data *sd, int skill_num,
                        const char *map);

int  skill_cleartimerskill (struct block_list *src);
int  skill_addtimerskill (struct block_list *src, unsigned int tick,
                          int target, int x, int y, int skill_id,
                          int skill_lv, int type, int flag);

// 追加効果
int  skill_additional_effect (struct block_list *src, struct block_list *bl,
                              int skillid, int skilllv, int attack_type,
                              unsigned int tick);

// ユニットスキル
struct skill_unit *skill_initunit (struct skill_unit_group *group, int idx,
                                   int x, int y);
int  skill_delunit (struct skill_unit *unit);
struct skill_unit_group *skill_initunitgroup (struct block_list *src,
                                              int count, int skillid,
                                              int skilllv, int unit_id);
struct skill_unit_group_tickset *skill_unitgrouptickset_search (struct
                                                                block_list
                                                                *bl,
                                                                int group_id);
int  skill_clear_unitgroup (struct block_list *src);

int  skill_unit_ondamaged (struct skill_unit *src, struct block_list *bl,
                           int damage, unsigned int tick);

int  skill_castfix (struct block_list *bl, int time);
int  skill_delayfix (struct block_list *bl, int time);
int  skill_unit_out_all (struct block_list *bl, unsigned int tick, int range);
int  skill_unit_move (struct block_list *bl, unsigned int tick, int range);
int  skill_unit_move_unit_group (struct skill_unit_group *group, int m,
                                 int dx, int dy);

// 詠唱キャンセル
int  skill_castcancel (struct block_list *bl, int type);

#define skill_calc_heal(bl,skill_lv) (( battle_get_lv(bl)+battle_get_int(bl) )/8 *(4+ skill_lv*8))

// その他
int  skill_check_cloaking (struct block_list *bl);

// ステータス異常
int  skill_status_effect (struct block_list *bl, int type, int val1, int val2,
                          int val3, int val4, int tick, int flag,
                          int spell_invocation);
int  skill_status_change_start (struct block_list *bl, int type, int val1,
                                int val2, int val3, int val4, int tick,
                                int flag);
int  skill_status_change_active (struct block_list *bl, int type);  // [fate]
int  skill_status_change_end (struct block_list *bl, int type, int tid);
int  skill_status_change_clear (struct block_list *bl, int type);

// スキル攻撃一括処理
int  skill_attack (int attack_type, struct block_list *src,
                   struct block_list *dsrc, struct block_list *bl,
                   int skillid, int skilllv, unsigned int tick, int flag);

int  skill_update_heal_animation (struct map_session_data *sd); // [Fate]  Check whether the healing flag must be updated, do so if needed

void skill_reload (void);

enum
{
    ST_NONE, ST_HIDING, ST_CLOAKING, ST_HIDDEN,
    ST_SHIELD, ST_SIGHT, ST_EXPLOSIONSPIRITS,
    ST_RECOV_WEIGHT_RATE, ST_MOVE_ENABLE, ST_WATER,
};

/// effects
const int SC_SENDMAX = 256;
const int SC_SLOWPOISON = 14;
const int SC_BROKNARMOR = 32;
const int SC_BROKNWEAPON = 33;
const int SC_SPEEDPOTION0 = 37;

const int SC_HEALING = 70;

const int SC_POISON = 132;

const int SC_ATKPOT = 185;

// Added for Fate's spells
const int SC_HIDE = 194;              // Hide from `detect' magic
const int SC_HALT_REGENERATE = 195;   // Suspend regeneration
const int SC_FLYING_BACKPACK = 196;   // Flying backpack
const int SC_MBARRIER = 197;          // Magical barrier; magic resistance (val1 : power (%))
const int SC_HASTE = 198;             // `Haste' spell (val1 : power)
const int SC_PHYS_SHIELD = 199;       // `Protect' spell; reduce damage (val1: power)

/// skills

const int NV_EMOTE = 1;
const int NV_TRADE = 2;
const int NV_PARTY = 3;

const int AC_OWL = 45;

const int TMW_SKILLPOOL = 339;        // skill pool size

const int TMW_MAGIC = 340;
const int TMW_MAGIC_LIFE = 341;
const int TMW_MAGIC_WAR = 342;
const int TMW_MAGIC_TRANSMUTE = 343;
const int TMW_MAGIC_NATURE = 344;
const int TMW_MAGIC_ETHER = 345;

const int TMW_BRAWLING = 350;
const int TMW_LUCKY_COUNTER = 351;
const int TMW_SPEED = 352;
const int TMW_RESIST_POISON = 353;
const int TMW_ASTRAL_SOUL = 354;
const int TMW_RAGING = 355;

// [Fate] Skill pools API

// Max. # of active entries in the skill pool
#define MAX_SKILL_POOL 3
// Max. # of skills that may be classified as pool skills in db/skill_db.txt
#define MAX_POOL_SKILLS 128

extern int skill_pool_skills[MAX_POOL_SKILLS];  // All pool skills
extern int skill_pool_skills_size;  // Number of entries in skill_pool_skills

void skill_pool_register (int id);   // [Fate] Remember that a certain skill ID belongs to a pool skill
int  skill_pool (struct map_session_data *sd, int *skills); // Yields all active skills in the skill pool; no more than MAX_SKILL_POOL.  Return is number of skills.
int  skill_pool_max (struct map_session_data *sd);  // Max. number of pool skills
int  skill_pool_activate (struct map_session_data *sd, int skill);  // Skill into skill pool.  Return is zero iff okay.
int  skill_pool_is_activated (struct map_session_data *sd, int skill);  // Skill into skill pool.  Return is zero when activated.
int  skill_pool_deactivate (struct map_session_data *sd, int skill);    // Skill out of skill pool.  Return is zero iff okay.
const char *skill_name (int skill);   // Yield configurable skill name
int  skill_stat (int skill);    // Yields the stat associated with a skill.  Returns zero if none, or SP_STR, SP_VIT, ... otherwise
int  skill_power (struct map_session_data *sd, int skill);  // Yields the power of a skill.  This is zero if the skill is unknown or if it's a pool skill that is outside of the skill pool,
                             // otherwise a value from 0 to 255 (with 200 being the `normal maximum')
int  skill_power_bl (struct block_list *bl, int skill); // Yields the power of a skill.  This is zero if the skill is unknown or if it's a pool skill that is outside of the skill pool,
                             // otherwise a value from 0 to 255 (with 200 being the `normal maximum')

#endif // SKILL_H
