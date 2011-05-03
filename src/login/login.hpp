#ifndef LOGIN_H
#define LOGIN_H

/// Max number of char servers to accept
# define MAX_SERVERS 30

# define LOGIN_CONF_NAME "conf/login_athena.conf"
# define LAN_CONF_NAME "conf/lan_support.conf"
/// Start and end of user accounts
// TODO figure out why it is like this
# define START_ACCOUNT_NUM 2000000
# define END_ACCOUNT_NUM 100000000

#include <netinet/in.h>

#include "../common/sanity.hpp"
#include "../common/mmo.hpp"

enum gender
{
    SEX_FEMALE,
    SEX_MALE,
    SEX_SERVER,
    SEX_ERROR,
};

static inline enum gender sex_from_char(char c)
{
    switch (c | 0x20)
    {
    case 's': return SEX_SERVER;
    case 'm': return SEX_MALE;
    case 'f': return SEX_FEMALE;
    default: return SEX_ERROR;
    }
}

static inline char sex_to_char(enum gender sex)
{
    switch (sex)
    {
    case SEX_FEMALE: return 'F';
    case SEX_MALE: return 'M';
    case SEX_SERVER: return 'S';
    default: return '\0';
    }
}

struct mmo_account
{
    char *userid;
    char *passwd;

    account_t account_id;
    /// magic cookies used to authenticate?
    uint32_t login_id1, login_id2;
    // ? This is not used by the login server ...
    uint32_t char_id;
    // why is this needed?
    char lastlogin[24];
    // this is used redundantly ...
    enum gender sex;
};

struct mmo_char_server
{
    char name[20];
    in_addr_t ip;
    in_port_t port;
    uint32_t users;
    uint32_t maintenance;
    uint32_t is_new;
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

enum passwd_failure
{
    PASSWD_NO_ACCOUNT = 0,
    PASSWD_OK = 1,
    PASSWD_WRONG_PASSWD = 2,
    PASSWD_TOO_SHORT = 3,
};
#endif
