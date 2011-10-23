#ifndef SKILL_HPP
#define SKILL_HPP

# include "../common/mmo.hpp"
# include "../common/timer.structs.hpp"

# include "main.structs.hpp"

# define SKILL_POOL_FLAG      0x1 // is a pool skill
# define SKILL_POOL_ACTIVE    0x2 // is an active pool skill
# define SKILL_POOL_ACTIVATED 0x4 // pool skill has been activated (used for clif)

// スキルデータベース
struct skill_db
{
    SP stat;
    sint32 poolflags, max_raise;
};
extern struct skill_db skill_db[MAX_SKILL];

struct skill_name_db
{
    sint32 id;                    // skill id
    const char *name;                 // search strings
    const char *desc;                 // description that shows up for search's
};
extern struct skill_name_db skill_names[];

class BlockList;
class MapSessionData;

void do_init_skill(void);

sint32 skill_get_max_raise(sint32 id) __attribute__((pure));

// 詠唱キャンセル
sint32 skill_castcancel(BlockList *bl);

// ステータス異常
sint32 skill_status_effect(BlockList *bl, sint32 type, sint32 val1, interval_t tick, BlockID spell_invocation);
sint32 skill_status_change_start(BlockList *bl, sint32 type, sint32 val1, interval_t tick);
sint32 skill_status_change_active(BlockList *bl, sint32 type);  // [fate]
sint32 skill_status_change_end(BlockList *bl, sint32 type, timer_id tid);
sint32 skill_status_change_clear(BlockList *bl, sint32 type);

void skill_update_heal_animation(MapSessionData *sd); // [Fate]  Check whether the healing flag must be updated, do so if needed

enum
{
    ST_NONE, ST_HIDING, ST_CLOAKING, ST_HIDDEN,
    ST_SHIELD, ST_SIGHT, ST_EXPLOSIONSPIRITS,
    ST_RECOV_WEIGHT_RATE, ST_MOVE_ENABLE, ST_WATER,
};

/// effects
const sint32 SC_SENDMAX = 256;
const sint32 SC_SLOWPOISON = 14;
const sint32 SC_SPEEDPOTION0 = 37;
const sint32 SC_HEALING = 70;
const sint32 SC_POISON = 132;
const sint32 SC_ATKPOT = 185;

// Added for Fate's spells
const sint32 SC_HIDE = 194;              // Hide from `detect' magic
const sint32 SC_HALT_REGENERATE = 195;   // Suspend regeneration
const sint32 SC_FLYING_BACKPACK = 196;   // Flying backpack
const sint32 SC_MBARRIER = 197;          // Magical barrier; magic resistance (val1 : power (%))
const sint32 SC_HASTE = 198;             // `Haste' spell (val1 : power)
const sint32 SC_PHYS_SHIELD = 199;       // `Protect' spell; reduce damage (val1: power)

/// skills

const sint32 NV_EMOTE = 1;
const sint32 NV_TRADE = 2;
const sint32 NV_PARTY = 3;

const sint32 AC_OWL = 45;

const sint32 TMW_SKILLPOOL = 339;        // skill pool size

const sint32 TMW_MAGIC = 340;
const sint32 TMW_MAGIC_LIFE = 341;
const sint32 TMW_MAGIC_WAR = 342;
const sint32 TMW_MAGIC_TRANSMUTE = 343;
const sint32 TMW_MAGIC_NATURE = 344;
const sint32 TMW_MAGIC_ETHER = 345;

const sint32 TMW_BRAWLING = 350;
const sint32 TMW_LUCKY_COUNTER = 351;
const sint32 TMW_SPEED = 352;
const sint32 TMW_RESIST_POISON = 353;
const sint32 TMW_ASTRAL_SOUL = 354;
const sint32 TMW_RAGING = 355;

// [Fate] Skill pools API

// Max. # of active entries in the skill pool
#define MAX_SKILL_POOL 3
// Max. # of skills that may be classified as pool skills in db/skill_db.txt
#define MAX_POOL_SKILLS 128

extern sint32 skill_pool_skills[MAX_POOL_SKILLS];  // All pool skills
extern sint32 skill_pool_skills_size;  // Number of entries in skill_pool_skills

void skill_pool_register(sint32 id);   // [Fate] Remember that a certain skill ID belongs to a pool skill
sint32 skill_pool(MapSessionData *sd, sint32 *skills); // Yields all active skills in the skill pool; no more than MAX_SKILL_POOL.  Return is number of skills.
sint32 skill_pool_max(MapSessionData *sd) __attribute__((pure));  // Max. number of pool skills
sint32 skill_pool_activate(MapSessionData *sd, sint32 skill);  // Skill into skill pool.  Return is zero iff okay.
sint32 skill_pool_is_activated(MapSessionData *sd, sint32 skill) __attribute__((pure));  // Skill into skill pool.  Return is zero when activated.
sint32 skill_pool_deactivate(MapSessionData *sd, sint32 skill);    // Skill out of skill pool.  Return is zero iff okay.
const char *skill_name(sint32 skill) __attribute__((pure));   // Yield configurable skill name
sint32 skill_power(MapSessionData *sd, sint32 skill);  // Yields the power of a skill.  This is zero if the skill is unknown or if it's a pool skill that is outside of the skill pool,
                             // otherwise a value from 0 to 255 (with 200 being the `normal maximum')
sint32 skill_power_bl(BlockList *bl, sint32 skill); // Yields the power of a skill.  This is zero if the skill is unknown or if it's a pool skill that is outside of the skill pool,
                             // otherwise a value from 0 to 255 (with 200 being the `normal maximum')

#endif // SKILL_HPP
