/// Global structures and defines
// BUG: many of these are only applicable for one of the servers
#ifndef MMO_H
#define MMO_H

# include <time.h>
# include <arpa/inet.h>

# include "../lib/fixed_string.hpp"

# include "utils.hpp"

# define FIFOSIZE_SERVERLINK    256*1024

// set to 0 to not check IP of player between each server.
// set to another value if you want to check (1)
# define CMP_AUTHFIFO_IP 1

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

// for produce
# define MIN_ATTRIBUTE 0
# define MAX_ATTRIBUTE 4
# define ATTRIBUTE_NORMAL 0
# define MIN_STAR 0
# define MAX_STAR 3

# define MIN_PORTAL_MEMO 0
# define MAX_PORTAL_MEMO 2

# define MAX_STATUS_TYPE 5

# define CHAR_CONF_NAME  "conf/char_athena.conf"

typedef uint32_t account_t;
typedef uint8_t gm_level_t;
typedef uint32_t charid_t;
typedef uint32_t party_t;
// kept for now to prevent breaking TOO much code
typedef uint8_t level_t;

struct item
{
    int id;
    short nameid;
    short amount;
    unsigned short equip;
    char identify;
    char refine;
    char attribute;
    short card[4];
    short broken;
};

struct point
{
    fixed_string<16> map;
    short x, y;
};

struct skill
{
    unsigned short id, lv, flags;
};

struct global_reg
{
    char str[32];
    int value;
};

struct mmo_charstatus
{
    charid_t char_id;
    account_t account_id;
    charid_t partner_id;

    int base_exp, job_exp, zeny;

    short status_point, skill_point;
    int hp, max_hp, sp, max_sp;
    short option;
    short hair, hair_color;
    party_t party_id;

    short weapon, shield;
    short head_top, head_mid, head_bottom;

    char name[24];
    level_t base_level, job_level;
    short str, agi, vit, int_, dex, luk;
    uint8_t char_num, sex;

    in_addr_t mapip;
    in_port_t mapport;

    struct point last_point, save_point, memo_point[10];
    struct item inventory[MAX_INVENTORY];
    struct skill skill[MAX_SKILL];
    int global_reg_num;
    struct global_reg global_reg[GLOBAL_REG_NUM];
    int account_reg_num;
    struct global_reg account_reg[ACCOUNT_REG_NUM];
    int account_reg2_num;
    struct global_reg account_reg2[ACCOUNT_REG2_NUM];
};

struct storage
{
    int dirty;
    account_t account_id;
    short storage_status;
    short storage_amount;
    struct item storage_[MAX_STORAGE];
};

struct map_session_data;

struct gm_account
{
    account_t account_id;
    gm_level_t level;
};

struct party_member
{
    account_t account_id;
    char name[24], map[16];
    int leader;
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
    int val1[5];
    int val2[5];
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

#endif // MMO_H
