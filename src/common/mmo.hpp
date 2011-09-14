/// Global structures and defines
// BUG: many of these are only applicable for one of the servers
#ifndef MMO_HPP
#define MMO_HPP

# include "../lib/fixed_string.hpp"
# include "../lib/ip.hpp"
# include "../lib/earray.hpp"
# include "../lib/enum.hpp"

# define MAX_MAP_PER_SERVER 512
# define MAX_INVENTORY 100
# define MAX_AMOUNT 30000
# define MAX_ZENY 1000000000     // 1G zeny
# define MAX_SKILL 450
# define GLOBAL_REG_NUM 96
# define ACCOUNT_REG_NUM 16
# define ACCOUNT_REG2_NUM 16
# define DEFAULT_WALK_SPEED 150
# define MIN_WALK_SPEED 0
# define MAX_WALK_SPEED 1000
# define MAX_STORAGE 300
# define MAX_PARTY 12

/// These were incorrect and generally not respected, so I am changing them
/// Back to hard-coded values for the places there were magic numbers
// # define MAX_HAIR_STYLE battle_config.max_hair_style
// # define MAX_HAIR_COLOR battle_config.max_hair_color
// # define MAX_CLOTH_COLOR battle_config.max_cloth_color
# define NUM_HAIR_STYLES 20
# define NUM_HAIR_COLORS 12

typedef uint32_t account_t;
typedef uint8_t gm_level_t;
typedef uint32_t charid_t;
typedef uint32_t party_t;
// kept for now to prevent breaking TOO much code
typedef uint8_t level_t;

BIT_ENUM(EPOS, uint16_t)
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

struct item
{
    uint16_t nameid;
    uint16_t amount;
    // I think this is a mask of equip slots, but only one is usually (ever?) used
    EPOS equip;
};

struct Point
{
    fixed_string<16> map;
    uint16_t x, y;
};

struct skill
{
    uint16_t id, lv, flags;
};

struct global_reg
{
    char str[32];
    int32_t value;
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

    int32_t base_exp, job_exp, zeny;

    int16_t status_point, skill_point;
    int32_t hp, max_hp, sp, max_sp;
    int16_t option;
    int16_t hair, hair_color;
    party_t party_id;

    int16_t weapon, shield;
    int16_t head, chest, legs;

    char name[24];
    level_t base_level, job_level;
    earray<int16_t, ATTR, ATTR::COUNT> stats;
    uint8_t char_num, sex;

    IP_Address mapip;
    in_port_t mapport;

    Point last_point, save_point, memo_point[10];
    struct item inventory[MAX_INVENTORY];
    struct skill skill[MAX_SKILL];
    int32_t global_reg_num;
    struct global_reg global_reg[GLOBAL_REG_NUM];
    int32_t account_reg_num;
    struct global_reg account_reg[ACCOUNT_REG_NUM];
    int32_t account_reg2_num;
    struct global_reg account_reg2[ACCOUNT_REG2_NUM];
};

struct storage
{
    int32_t dirty;
    account_t account_id;
    int16_t storage_status;
    int16_t storage_amount;
    struct item storage_[MAX_STORAGE];
};

struct gm_account
{
    account_t account_id;
    gm_level_t level;
};

struct party_member
{
    account_t account_id;
    char name[24], map[16];
    int32_t leader;
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
    int32_t val1[5];
    int32_t val2[5];
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
