#ifndef SKILL_HPP
#define SKILL_HPP

# include "../common/mmo.hpp"
# include "../common/timer.structs.hpp"

# include "map.structs.hpp"

# define SKILL_POOL_FLAG      0x1 // is a pool skill
# define SKILL_POOL_ACTIVE    0x2 // is an active pool skill
# define SKILL_POOL_ACTIVATED 0x4 // pool skill has been activated (used for clif)

// スキルデータベース
struct skill_db
{
    SP stat;
    int32_t poolflags, max_raise;
};
extern struct skill_db skill_db[MAX_SKILL];

struct skill_name_db
{
    int32_t id;                    // skill id
    const char *name;                 // search strings
    const char *desc;                 // description that shows up for search's
};
extern struct skill_name_db skill_names[];

class BlockList;
class MapSessionData;

void do_init_skill(void);

int32_t skill_get_max_raise(int32_t id) __attribute__((pure));

// 詠唱キャンセル
int32_t skill_castcancel(BlockList *bl);

// ステータス異常
int32_t skill_status_effect(BlockList *bl, int32_t type, int32_t val1, tick_t tick, int32_t spell_invocation);
int32_t skill_status_change_start(BlockList *bl, int32_t type, int32_t val1, tick_t tick);
int32_t skill_status_change_active(BlockList *bl, int32_t type);  // [fate]
int32_t skill_status_change_end(BlockList *bl, int32_t type, timer_id tid);
int32_t skill_status_change_clear(BlockList *bl, int32_t type);

void skill_update_heal_animation(MapSessionData *sd); // [Fate]  Check whether the healing flag must be updated, do so if needed

enum
{
    ST_NONE, ST_HIDING, ST_CLOAKING, ST_HIDDEN,
    ST_SHIELD, ST_SIGHT, ST_EXPLOSIONSPIRITS,
    ST_RECOV_WEIGHT_RATE, ST_MOVE_ENABLE, ST_WATER,
};

/// effects
const int32_t SC_SENDMAX = 256;
const int32_t SC_SLOWPOISON = 14;
const int32_t SC_SPEEDPOTION0 = 37;
const int32_t SC_HEALING = 70;
const int32_t SC_POISON = 132;
const int32_t SC_ATKPOT = 185;

// Added for Fate's spells
const int32_t SC_HIDE = 194;              // Hide from `detect' magic
const int32_t SC_HALT_REGENERATE = 195;   // Suspend regeneration
const int32_t SC_FLYING_BACKPACK = 196;   // Flying backpack
const int32_t SC_MBARRIER = 197;          // Magical barrier; magic resistance (val1 : power (%))
const int32_t SC_HASTE = 198;             // `Haste' spell (val1 : power)
const int32_t SC_PHYS_SHIELD = 199;       // `Protect' spell; reduce damage (val1: power)

/// skills

const int32_t NV_EMOTE = 1;
const int32_t NV_TRADE = 2;
const int32_t NV_PARTY = 3;

const int32_t AC_OWL = 45;

const int32_t TMW_SKILLPOOL = 339;        // skill pool size

const int32_t TMW_MAGIC = 340;
const int32_t TMW_MAGIC_LIFE = 341;
const int32_t TMW_MAGIC_WAR = 342;
const int32_t TMW_MAGIC_TRANSMUTE = 343;
const int32_t TMW_MAGIC_NATURE = 344;
const int32_t TMW_MAGIC_ETHER = 345;

const int32_t TMW_BRAWLING = 350;
const int32_t TMW_LUCKY_COUNTER = 351;
const int32_t TMW_SPEED = 352;
const int32_t TMW_RESIST_POISON = 353;
const int32_t TMW_ASTRAL_SOUL = 354;
const int32_t TMW_RAGING = 355;

// [Fate] Skill pools API

// Max. # of active entries in the skill pool
#define MAX_SKILL_POOL 3
// Max. # of skills that may be classified as pool skills in db/skill_db.txt
#define MAX_POOL_SKILLS 128

extern int32_t skill_pool_skills[MAX_POOL_SKILLS];  // All pool skills
extern int32_t skill_pool_skills_size;  // Number of entries in skill_pool_skills

void skill_pool_register(int32_t id);   // [Fate] Remember that a certain skill ID belongs to a pool skill
int32_t skill_pool(MapSessionData *sd, int32_t *skills); // Yields all active skills in the skill pool; no more than MAX_SKILL_POOL.  Return is number of skills.
int32_t skill_pool_max(MapSessionData *sd) __attribute__((pure));  // Max. number of pool skills
int32_t skill_pool_activate(MapSessionData *sd, int32_t skill);  // Skill into skill pool.  Return is zero iff okay.
int32_t skill_pool_is_activated(MapSessionData *sd, int32_t skill) __attribute__((pure));  // Skill into skill pool.  Return is zero when activated.
int32_t skill_pool_deactivate(MapSessionData *sd, int32_t skill);    // Skill out of skill pool.  Return is zero iff okay.
const char *skill_name(int32_t skill) __attribute__((pure));   // Yield configurable skill name
int32_t skill_power(MapSessionData *sd, int32_t skill);  // Yields the power of a skill.  This is zero if the skill is unknown or if it's a pool skill that is outside of the skill pool,
                             // otherwise a value from 0 to 255 (with 200 being the `normal maximum')
int32_t skill_power_bl(BlockList *bl, int32_t skill); // Yields the power of a skill.  This is zero if the skill is unknown or if it's a pool skill that is outside of the skill pool,
                             // otherwise a value from 0 to 255 (with 200 being the `normal maximum')

#endif // SKILL_HPP
