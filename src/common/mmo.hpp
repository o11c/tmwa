/// Global structures and defines
// BUG: many of these are only applicable for one of the servers
#ifndef MMO_HPP
#define MMO_HPP

# include <chrono>

# include "../lib/fixed_string.hpp"
# include "../lib/ip.hpp"
# include "../lib/earray.hpp"
# include "../lib/enum.hpp"
# include "../lib/ints.hpp"

# define MAX_MAP_PER_SERVER 512
# define MAX_INVENTORY 100
# define MAX_AMOUNT 30000
# define MAX_ZENY 1000000000     // 1G zeny
# define MAX_SKILL 450
# define GLOBAL_REG_NUM 96
# define ACCOUNT_REG_NUM 16
# define ACCOUNT_REG2_NUM 16
constexpr std::chrono::milliseconds DEFAULT_WALK_SPEED(150);
# define MAX_STORAGE 300
# define MAX_PARTY 12

/// These were incorrect and generally not respected, so I am changing them
/// Back to hard-coded values for the places there were magic numbers
// # define MAX_HAIR_STYLE battle_config.max_hair_style
// # define MAX_HAIR_COLOR battle_config.max_hair_color
// # define MAX_CLOTH_COLOR battle_config.max_cloth_color
# define NUM_HAIR_STYLES 20
# define NUM_HAIR_COLORS 12

UNIQUE_TYPE(account_t, uint32);
UNIQUE_TYPE(gm_level_t, uint8);
UNIQUE_TYPE(charid_t, uint32);
UNIQUE_TYPE(party_t, uint32);
UNIQUE_TYPE(level_t, uint8);

BIT_ENUM(EPOS, uint16)
{
    NONE    = 0x0000,

    LEGS    = 0x0001,
    WEAPON  = 0x0002,
    GLOVES  = 0x0004,
    CAPE    = 0x0008,
    MISC1   = 0x0010,
    SHIELD  = 0x0020,
    SHOES   = 0x0040,
    MISC2   = 0x0080,
    HELMET  = 0x0100,
    CHEST   = 0x0200,
    ARROW   = 0x8000,

    ALL     = 0x83FF
};

// [Fate] status.option properties.  These are persistent status changes.
// IDs that are not listed are not used in the code (to the best of my knowledge)
BIT_ENUM(OPTION, uint16)
{
    NONE        = 0x0000,

    HIDE2       = 0x0002,   // apparently some weaker non-GM hide
    CLOAK       = 0x0004,
    _10         = 0x0010,
    _20         = 0x0020,
    HIDE        = 0x0040,   // [Fate] This is the GM `@hide' flag
    _800        = 0x0800,
    INVISIBILITY= 0x1000,   // [Fate] Complete invisibility to other clients
    SCRIBE      = 0x2000,   // [Fate] Auto-logging of nearby comments
    CHASEWALK   = 0x4000,

    ALL         = 0x7876,   // 421 8 421 42
    MASK        = 0xd7b8,   // 841 421 821 8 where did this number come from?
};

struct item
{
    uint16 nameid;
    uint16 amount;
    // I think this is a mask of equip slots, but only one is usually (ever?) used
    EPOS equip;
};

struct Point
{
    fixed_string<16> map;
    uint16 x, y;
};

struct skill
{
    uint16 id, lv, flags;
};

struct global_reg
{
    char str[32];
    sint32 value;
};

enum class ATTR
{
    STR, AGI, VIT, INT, DEX, LUK,

    COUNT
};
constexpr ATTR ATTRs[6] =
{
    ATTR::STR, ATTR::AGI, ATTR::VIT, ATTR::INT, ATTR::DEX, ATTR::LUK
};

struct mmo_charstatus
{
    charid_t char_id;
    account_t account_id;
    charid_t partner_id;

    sint32 base_exp, job_exp, zeny;

    sint16 status_point, skill_point;
    sint32 hp, max_hp, sp, max_sp;
    OPTION option;
    sint16 hair, hair_color;
    party_t party_id;

    sint16 weapon, shield;
    sint16 head, chest, legs;

    char name[24];
    level_t base_level, job_level;
    earray<sint16, ATTR, ATTR::COUNT> stats;
    uint8 char_num, sex;

    IP_Address mapip;
    in_port_t mapport;

    Point last_point, save_point, memo_point[10];
    struct item inventory[MAX_INVENTORY];
    struct skill skill[MAX_SKILL];
    sint32 global_reg_num;
    struct global_reg global_reg[GLOBAL_REG_NUM];
    sint32 account_reg_num;
    struct global_reg account_reg[ACCOUNT_REG_NUM];
    sint32 account_reg2_num;
    struct global_reg account_reg2[ACCOUNT_REG2_NUM];
};

struct storage
{
    sint32 dirty;
    account_t account_id;
    sint16 storage_status;
    sint16 storage_amount;
    struct item storage_[MAX_STORAGE];
};

struct party_member
{
    account_t account_id;
    char name[24], map[16];
    sint32 leader;
    bool online;
    level_t lv;
    class MapSessionData *sd;
};

struct party
{
    party_t party_id;
    char name[24];
    bool exp;
    bool item;
    struct party_member member[MAX_PARTY];
};

struct square
{
    sint32 val1[5];
    sint32 val2[5];
};

/// Reason a login can fail
// Note - packet 0x6a uses one less than this (no AUTH_OK)
enum auth_failure
{
    AUTH_OK = 0,
    AUTH_UNREGISTERED_ID = 1,
    AUTH_INVALID_PASSWORD = 2,
    AUTH_EXPIRED = 3,
    AUTH_REJECTED_BY_SERVER = 4,
    AUTH_BLOCKED_BY_GM = 5,
    AUTH_CLIENT_TOO_OLD = 6,
    AUTH_BANNED_TEMPORARILY = 7,
    AUTH_SERVER_OVERPOPULATED = 8,
    AUTH_ALREADY_EXISTS = 10,
    AUTH_ID_ERASED = 100,
};

#endif // MMO_HPP
