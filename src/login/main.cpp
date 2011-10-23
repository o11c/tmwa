/// for stat(), used on the gm_account file
#include <sys/stat.h>
/// for waitpid()
#include <sys/wait.h>

#include <vector>

#include "../common/core.hpp"
#include "../common/lock.hpp"
#include "../common/md5calc.hpp"
#include "../common/mmo.hpp"
#include "../common/mt_rand.hpp"
#include "../common/socket.hpp"
#include "../common/timer.hpp"
#include "../common/utils.hpp"
#include "../common/version.hpp"

#include "../lib/dmap.hpp"
#include "../lib/ip.hpp"
#include "../lib/member_map.hpp"
#include "../lib/cxxstdio.hpp"

/// Max number of char servers to accept
# define MAX_SERVERS 30

# define LOGIN_CONF_NAME "conf/login_athena.conf"
# define LAN_CONF_NAME "conf/lan_support.conf"
/// Start and end of user accounts
// TODO figure out why it is like this
constexpr account_t START_ACCOUNT_NUM = account_t(2000000);
constexpr account_t END_ACCOUNT_NUM = account_t(100000000);

enum class SEX
{
    FEMALE,
    MALE,
    SERVER,
    ERROR,
};

static inline SEX sex_from_char(char c)
{
    switch (c | 0x20)
    {
        case 's': return SEX::SERVER;
        case 'm': return SEX::MALE;
        case 'f': return SEX::FEMALE;
        default: return SEX::ERROR;
    }
}

static inline char sex_to_char(SEX sex)
{
    switch (sex)
    {
        case SEX::FEMALE: return 'F';
        case SEX::MALE: return 'M';
        case SEX::SERVER: return 'S';
        default: return '\0';
    }
}

struct mmo_account
{
    char userid[24];
    char passwd[24];

    account_t account_id;
    /// magic cookies used to authenticate?
    uint32 login_id1, login_id2;
    // ? This is not used by the login server ...
    uint32 char_id;
    // why is this needed?
    char lastlogin[24];
    // this is used redundantly ...
    SEX sex;
};

struct mmo_char_server
{
    char name[20];
    IP_Address ip;
    in_port_t port;
    uint32 users;
    uint32 maintenance;
    uint32 is_new;

    // merged in
    sint32 fd;
    sint32 freezeflag;
};

enum passwd_failure
{
    PASSWD_NO_ACCOUNT = 0,
    PASSWD_OK = 1,
    PASSWD_WRONG_PASSWD = 2,
    PASSWD_TOO_SHORT = 3,
};



account_t account_id_count = START_ACCOUNT_NUM;
sint32 new_account_flag = 0;
in_port_t login_port = 6901;

IP_Address lan_char_ip;
IP_Mask lan_mask;
char update_host[128] = "";
char main_server[20] = "";

const char account_filename[] = "save/account.txt";
const char gm_account_filename[] = "save/gm_account.txt";
const char login_log_filename[] = "log/login.log";
const char login_log_unknown_packets_filename[] = "log/login_unknown_packets.log";

/// log unknown packets from the initial login
// unknown ladmin and char-server packets are always logged
bool save_unknown_packets = 0;
/// When the gm account file was last modified
time_t creation_time_gm_account_file;
/// Interval (in seconds) after which to recheck GM file.
std::chrono::seconds gm_account_filename_check_timer(15);

bool display_parse_login = 0;
bool display_parse_admin = 0;
static enum char_packets_display_t
{
    CP_NONE,
    CP_MOST,
    CP_ALL
} display_parse_fromchar = CP_NONE;

// FIXME: merge the arrays into members of struct mmo_char_server
struct mmo_char_server server[MAX_SERVERS];
// Char-server anti-freeze system. Set to 5 when receiving
// packet 0x2714, decrements every ANTI_FREEZE_INTERVAL seconds.
// If it reaches zero, assume the char server has frozen and disconnect it.
// DON'T enable this if you are going to use gdb
bool anti_freeze_enable = false;
std::chrono::seconds ANTI_FREEZE_INTERVAL(15);

sint32 login_fd;

enum class ACO
{
    DENY_ALLOW = 0,
    ALLOW_DENY,
    MUTUAL_FAILURE,
};

ACO access_order = ACO::DENY_ALLOW;

std::vector<IP_Mask> access_allow;
std::vector<IP_Mask> access_deny;

std::vector<IP_Mask> access_ladmin_allow;

// minimum level of player/GM (0: player, 1-99: gm) to connect on the server
gm_level_t min_level_to_connect = gm_level_t(0);
// Give possibility or not to adjust (ladmin command: timeadd) the time of an unlimited account.
bool add_to_unlimited_account = 0;

/// some random data to be MD5'ed
// only used for ladmin
class LoginSessionData : public SessionData
{
public:
    LoginSessionData() : md5keylen(MPRAND(12, 4))
    {
        // Create coding key of length [12, 16)
        for (sint32 i = 0; i < md5keylen; i++)
            md5key[i] = MPRAND(1, 255);
    }
    sint32 md5keylen;
    char md5key[20];
};

#define AUTH_FIFO_SIZE 256
struct auth_fifo
{
    account_t account_id;
    uint32 login_id1, login_id2;
    IP_Address ip;
    SEX sex;
    bool delflag;
} auth_fifo[AUTH_FIFO_SIZE];
sint32 auth_fifo_pos = 0;

struct AuthData
{
    // this should be const but it's hard to make the scanf code work with that
    account_t account_id;

    SEX sex;
    char userid[24], pass[40], lastlogin[24];
    uint32 logincount;
    enum auth_failure state;
    char email[40];             // e-mail (by default: a@a.com)
    char error_message[20];     // Message of error code #6 = Your are Prohibited to log in until %s (packet 0x006a)
    time_t ban_until_time;      // # of seconds 1/1/1970 (timestamp): ban time limit of the account (0 = no ban)
    char last_ip[16];           // save of last IP of connection
    char memo[255];             // a memo field
    sint32 account_reg2_num;
    struct global_reg account_reg2[ACCOUNT_REG2_NUM];
};

template class MemberMap<AuthData, account_t, &AuthData::account_id>;
MemberMap<AuthData, account_t, &AuthData::account_id> auth_dat;

/// whether ladmin is allowed
bool admin_state = 0;
char admin_pass[24] = ""; // "admin"
/// For a test server, password to make a GM
char gm_pass[64] = "";
gm_level_t level_new_gm = gm_level_t(60);

static DMap<account_t, gm_level_t> gm_account_db;

pid_t pid = 0; // For forked DB writes

BIT_ENUM(Version2, uint8)
{
    NONE = 0,
    // client supports updatehost
    UPDATEHOST = 0x01,
    // send servers in forward order
    SERVERORDER = 0x02,
};

static const char *ip_of(sint32 fd)
{
    static char out[16];
    strcpy(out, session[fd]->client_addr.to_string().c_str());
    return out;
}

/// Writing something to log file (with timestamp) and to stderr (without)
// TODO add options to print common stuff: function/line, connection type,
// connection id, IP
// That *probably* means doing something like boost::format
// and just passing the account structure
Log login_log("login");
#define LOG_DEBUG(...)  login_log.debug(STR_PRINTF(__VA_ARGS__))
#define LOG_CONF(...)   login_log.conf(STR_PRINTF(__VA_ARGS__))
#define LOG_INFO(...)   login_log.info(STR_PRINTF(__VA_ARGS__))
#define LOG_WARN(...)   login_log.warn(STR_PRINTF(__VA_ARGS__))
#define LOG_ERROR(...)  login_log.error(STR_PRINTF(__VA_ARGS__))
#define LOG_FATAL(...)  login_log.fatal(STR_PRINTF(__VA_ARGS__))

Log unknown_packet_log("login.unknown");

/// Determine GM level of account (0 is not a GM)
__attribute__((pure))
static gm_level_t is_gm(account_t account_id)
{
    return gm_account_db.get(account_id);
}

/// Read GM accounts file
static void read_gm_account(void)
{
    gm_account_db.clear();

    FILE *fp = fopen_(gm_account_filename, "r");
    if (!fp)
    {
        LOG_ERROR("%s: %s: %m\n",
                  __func__, gm_account_filename);
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), fp))
    {
        if ((line[0] == '/' && line[1] == '/') || line[0] == '\n' || line[0] == '\r')
            continue;
        account_t account_id;
        gm_level_t level;
        if ((SSCANF(line, "%u %hhu", &account_id, &level) != 2
             && SSCANF(line, "%u: %hhu", &account_id, &level) != 2)
            || !level)
        {
            LOG_ERROR("%s: %s: invalid format of entry: %s.\n",
                      __func__, gm_account_filename, line);
            continue;
        }
        gm_level_t old_level = gm_account_db.replace(account_id, level);

        if (old_level)
            LOG_WARN("%s: duplicate GM account %d (levels: %d and %d).\n",
                     __func__, account_id, old_level, level);
    }
    fclose_(fp);
    LOG_CONF("%s: %s: %d GM accounts read.\n",
             __func__, gm_account_filename, gm_account_db.size());
}

/// Check whether an IP is allowed
// You are allowed if both lists are empty, or
// mode\ in list-> both    allow   deny    none
// allow,deny      Y       Y       N       N
// deny,allow      N       Y       N       Y
// mutual-failure  N       Y       N       N

static bool check_ip(IP_Address ip) __attribute__((pure));
static bool check_ip(IP_Address ip)
{
    // When there is no restriction, all IP are authorised.
    if (access_allow.empty() && access_deny.empty())
        return 1;

    /// Flag of whether the ip is in the allow or deny list
    bool allowed = false;

    for (IP_Mask mask : access_allow)
    {
        if (mask.covers(ip))
        {
            if (access_order == ACO::ALLOW_DENY)
                return 1;
                // With 'allow, deny' (deny if not allow), allow has priority
            allowed = true;
            break;
        }
    }

    for (IP_Mask mask : access_deny)
        if (mask.covers(ip))
            return 0;

    return allowed || access_order == ACO::DENY_ALLOW;
}

/// Check whether an IP is allowed for ladmin
static bool check_ladminip(IP_Address ip) __attribute__((pure));
static bool check_ladminip(IP_Address ip)
{
    if (access_ladmin_allow.empty())
        return 1;
    for (IP_Mask mask : access_ladmin_allow)
        if (mask.covers(ip))
            return 1;
    return 0;
}

/// Find an account by name. Now case sensitive!
__attribute__((pure))
static AuthData *account_by_name(const std::string& account_name)
{
    for (AuthData& ad : auth_dat)
        if (ad.userid == account_name)
            return &ad;
    return NULL;
}

__attribute__((pure))
static AuthData *account_by_id(account_t acc)
{
    auto it = auth_dat.find(acc);
    if (it != auth_dat.end())
        return &*it;
    return NULL;
}

/// Save an auth to file
static void mmo_auth_to_file(FILE *fp, AuthData *p)
{
    FPRINTF(fp, "%u\t" "%s\t" "%s\t" "%s\t"
                "%c\t" "%u\t" "%d\t" "%s\t"
                "%s\t" "%d\t" "%s\t"
                "%s\t" "%d\t",
            p->account_id, p->userid, p->pass, p->lastlogin,
            sex_to_char(p->sex), p->logincount, p->state, p->email,
            p->error_message, 0/*connect_until_time*/, p->last_ip,
            p->memo, static_cast<sint32>(p->ban_until_time));

    // Save ## variables.
    // It looks like strings aren't supported in our version of eAthena
    // There has been a bug in the char server that broke these.
    // Note that we don't actually use any of these.
    for (sint32 i = 0; i < p->account_reg2_num; i++)
        if (p->account_reg2[i].str[0])
            fprintf(fp, "%s,%d ", p->account_reg2[i].str,
                     p->account_reg2[i].value);
    fputc('\n', fp);
}

/// Read save/account.txt
static void mmo_auth_init(void)
{
    sint32 gm_count = 0;
    sint32 server_count = 0;

    FILE *fp = fopen_(account_filename, "r");
    if (!fp)
    {
        // no accounts means not even the char server can log in
        // ladmin can, but cannot add a server account
        // TODO: remove server accounts from account.txt and put them in config
        LOG_ERROR("%s: %s: %m\n",
                  __func__, account_filename);
        return;
    }

    char line[2048];
    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;

        AuthData tmp = {};
        char sex;
        sint32 n;
        uint8 state;
        sint32 i = SSCANF(line,
                           "%u\t"        "%23[^\t]\t"   "%39[^\t]\t"  "%39[^\t]\t"
                           "%c\t"        "%u\t"         "%hhu\t"      "%39[^\t]\t"
                           "%19[^\t]\t"  "%*d\t"
                           "%15[^\t]\t"  "%254[^\t]\t"  "%ld"         "%n",
                           &tmp.account_id, tmp.userid,  tmp.pass, tmp.lastlogin,
                           &sex, &tmp.logincount, &state, tmp.email,
                           tmp.error_message, /*&tmp.connect_until_time,*/
                           tmp.last_ip, tmp.memo, &tmp.ban_until_time, &n);
        if (i < 12 || line[n] != '\t')
        {
            if (SSCANF(line, "%d\t%%newid%%\n%n", &tmp.account_id, &n) == 1
                    && tmp.account_id > account_id_count)
                account_id_count = tmp.account_id;
            continue;
        }
        n++;

        // Some checks
        if (tmp.account_id > END_ACCOUNT_NUM)
        {
            LOG_ERROR("%s: account id %d exceeds %d.\n%s",
                      __func__, tmp.account_id, END_ACCOUNT_NUM, line);
            continue;
        }
        remove_control_chars(tmp.userid);

        for (AuthData& ad : auth_dat)
        {
            if (ad.account_id == tmp.account_id)
            {
                LOG_ERROR("%s: duplicate account %d.\n%s",
                          __func__, tmp.account_id, line);
                goto continue_outer;
            }
            else if (strcmp(ad.userid, tmp.userid) == 0)
            {
                LOG_ERROR("%s: duplicate account name %s (%d, %d).\n%s",
                          __func__, tmp.userid, ad.account_id,
                          tmp.account_id, line);
                goto continue_outer;
            }
        }
        // This is ugly but needed for -Wjump-misses-init
        // TODO refactor into separate function
        if (false)
        {
        continue_outer:
            continue;
        }

        remove_control_chars(tmp.memo);
        remove_control_chars(tmp.pass);
        // If a password is not encrypted, we encrypt it now.
        // A password beginning with ! and - in the memo field is our magic
        if (tmp.pass[0] != '!' && tmp.memo[0] == '-') {
            STRZCPY(tmp.pass, MD5_saltcrypt(tmp.pass, make_salt()));
            tmp.memo[0] = '!';
        }

        remove_control_chars(tmp.lastlogin);

        tmp.sex = sex_from_char(sex);
        tmp.state = static_cast<enum auth_failure>(state);

        if (!e_mail_check(tmp.email))
            STRZCPY(tmp.email, "a@a.com");
        else
            remove_control_chars(tmp.email);

        remove_control_chars(tmp.error_message);
        if (tmp.error_message[0] == '\0' ||
            tmp.state != AUTH_BANNED_TEMPORARILY)
            STRZCPY(tmp.error_message, "-");

        if (i != 13)
            tmp.ban_until_time = 0;

        remove_control_chars(tmp.last_ip);

        char *p = line;
        for (sint32 j = 0; j < ACCOUNT_REG2_NUM; j++, tmp.account_reg2_num++)
        {
            p += n;
            if (SSCANF(p, "%31[^\t,],%d %n",
                       tmp.account_reg2[j].str, &tmp.account_reg2[j].value, &n) != 2)
                // There used to be a check to allow and discard empty string
                // Note - this just discards trailing garbage!
                break;
            remove_control_chars(tmp.account_reg2[j].str);
        }

        if (is_gm(tmp.account_id))
            gm_count++;
        if (tmp.sex == SEX::SERVER)
            server_count++;

        auth_dat.insert(std::move(tmp));
        if (tmp.account_id >= account_id_count)
            account_id_count = tmp.account_id,
            ++account_id_count;
    }
    fclose_(fp);

    LOG_INFO("%s: %d account(s) (%d GM(s) and %d server(s))",
             __func__, auth_dat.size(), gm_count, server_count);
}

/// Save accounts to database file (text)
// usually called in a forked child, as it may take a while
static void mmo_auth_sync(void)
{
    // Data save
    sint32 lock;
    FILE *fp = lock_fopen(account_filename, &lock);
    if (!fp)
        return;
    static const char * const header[] =
    {
    "Accounts file:",
    "fields are tab-separated",
    "ID, name, password, last login, sex, # of logins, error state, email, ban message, expiration time, ip of last successful login, memo field, ban timestamp",
    "then all ## variables as name,value",
    "Some explanations:",
    "  account name    : 4-23 char for players, may be less for servers",
    "  account password: MD5-encoded password",
    "  sex             : M or F for normal accounts, S for server accounts",
    "  state           : 0: account is ok, else failure reason (see enum auth_failure in login.h)",
    "  email           : 3-39 char, default a@a.com",
    "  ban message     : 19 char, 'You are prohibited to log in until <date>'.",
    "  expiration time : 0: unlimited, else time after which you can't log in",
    "  memo field      : up 254 char, first char is ! to indicate MD5 password",
    "  ban time        : 0: not banned, else time until which you are banned",
    };
    for (sint32 i = 0; i < sizeof(header) / sizeof(header[0]); i++)
        fprintf(fp, "// %s\n", header[i]);
    for (auto& ad : auth_dat)
        mmo_auth_to_file(fp, &ad);
    FPRINTF(fp, "%d\t%%newid%%\n", account_id_count);

    lock_fclose(fp, account_filename, &lock);
}

/// Timer to sync the DB to disk as little as possible
// this is resource-intensive, so fork() if possible
static void check_auth_sync(timer_id, tick_t)
{
    if (pid && !waitpid(pid, NULL, WNOHANG))
        // if already running
        return;

    pid = fork();
    if (pid > 0)
        return;
    // either we are the child, or fork() failed
    mmo_auth_sync();

    // If we're a child we should suicide now.
    if (pid == 0)
        _exit(0);
    pid = 0;
}

/// Send a packet to all char servers, excluding sfd
// often called with sfd == -1 to not exclude anything
static void charif_sendallwos(sint32 sfd, const uint8 *buf, uint32 len)
{
    for (sint32 i = 0; i < MAX_SERVERS; i++)
    {
        sint32 fd = server[i].fd;
        if (fd >= 0 && fd != sfd)
        {
            memcpy(WFIFOP(fd, 0), buf, len);
            WFIFOSET(fd, len);
        }
    }
}

/// Send GM accounts to all char servers
static void send_gm_accounts(void)
{
    // TODO: make a new API for sending packets
    uint8 buf[32000];
    sint32 len = 4;
    WBUFW(buf, 0) = 0x2732;
    // shouldn't we just iterate over the db instead?
    for (auto& elt : gm_account_db)
    {
        if (len + 5 > 32000)
            break;
        WBUFL(buf, len) = uint32(elt.first);
        WBUFB(buf, len + 4) = uint8(elt.second);
        len += 5;
    }
    WBUFW(buf, 2) = len;
    charif_sendallwos(-1, buf, len);
}

/// Timer to check if GM file account have been changed
// TODO replace this with inotify on systems where it is available
static void check_gm_file(timer_id, tick_t)
{
    // if checking is disabled
    if (gm_account_filename_check_timer == std::chrono::seconds::zero())
        return;

    struct stat file_stat;
    time_t new_time = 0;
    if (stat(gm_account_filename, &file_stat) == 0)
        new_time = file_stat.st_mtime;

    if (new_time != creation_time_gm_account_file)
    {
        read_gm_account();
        creation_time_gm_account_file = new_time;
        send_gm_accounts();
    }
}

/// Create a new account from the given connection
static AuthData *mmo_auth_new(struct mmo_account *account, const char *email)
{
    // disallow creating an account that already is labeled a GM
    while (is_gm(account_id_count))
        account_id_count++;

    AuthData *auth = &*auth_dat.insert({account_id: account_id_count++}).first;

    STRZCPY(auth->userid, account->userid);
    STRZCPY(auth->pass, MD5_saltcrypt(account->passwd, make_salt()));
    STRZCPY(auth->lastlogin, "-");

    auth->sex = account->sex;
    auth->logincount = 0;
    auth->state = AUTH_OK;

    if (!e_mail_check(email))
        STRZCPY(auth->email, "a@a.com");
    else
        STRZCPY(auth->email, email);

    STRZCPY(auth->error_message, "-");
    auth->ban_until_time = 0;

    STRZCPY(auth->last_ip, "-");
    STRZCPY(auth->memo, "!");

    auth->account_reg2_num = 0;

    return auth;
}

/// Try to authenticate a connection
static enum auth_failure mmo_auth(struct mmo_account *account, sint32 fd)
{
    const char *const ip = ip_of(fd);

    size_t len = strlen(account->userid) - 2;
    // Account creation with _M/_F
    // TODO does this actually happen? I thought "create account" was a login-server thing
    bool newaccount =
        account->userid[len] == '_' &&
        (account->userid[len + 1] == 'F' || account->userid[len + 1] == 'M') &&
        new_account_flag &&
        account_id_count <= END_ACCOUNT_NUM &&
        len >= 4 &&
        strlen(account->passwd) >= 4;
    if (newaccount)
        account->userid[len] = '\0';

    AuthData *auth = account_by_name(account->userid);
    if (!auth)
    {
        if (!newaccount)
        {
            LOG_INFO("%s: unknown account %s (ip: %s)\n",
                     __func__, account->userid, ip);
            return AUTH_UNREGISTERED_ID;
        }
        account->sex = sex_from_char(account->userid[len + 1]);
        auth = mmo_auth_new(account, "a@a.com");
        LOG_INFO("%s: created account %s (id: %d, sex: %c, ip: %s)\n",
                 __func__, account->userid, auth->account_id,
                 account->userid[len + 1], ip);
    }
    else
    {
        // for the possible tests/checks afterwards (copy correct case).
        STRZCPY(account->userid, auth->userid);

        if (newaccount)
        {
            LOG_INFO("%s: tried to create existing account %s_%c (ip %s)\n",
                     __func__, account->userid, account->userid[len + 1], ip);
            return AUTH_ALREADY_EXISTS;
        }
        if (!pass_ok(account->passwd, auth->pass))
        {
            LOG_INFO("%s: invalid password for %s (ip %s)\n",
                     __func__, account->userid, ip);
            return AUTH_INVALID_PASSWORD;
        }
        if (auth->state != AUTH_OK)
        {
            // is this right? What about bans that expire?
            LOG_INFO("%s: refused %s due to state %d (ip: %s)\n",
                     __func__, account->userid, auth->state, ip);
            if (auth->state < AUTH_ALREADY_EXISTS)
                return auth->state;
            else
                return AUTH_ID_ERASED;
        }
        // can this even happen?
        if (auth->ban_until_time)
        {
            const char *tmpstr = stamp_time(auth->ban_until_time, NULL);
            if (auth->ban_until_time > time(NULL))
            {
                LOG_INFO("%s: refuse %s - banned until %s (ip: %s)\n",
                         __func__, account->userid, tmpstr, ip);
                return AUTH_BANNED_TEMPORARILY;
            }
            else
            {
                LOG_INFO("%s: end ban %s - banned until %s (ip: %s)\n",
                         __func__, account->userid, tmpstr, ip);
                auth->ban_until_time = 0; // reset the ban time
            }
        }

        LOG_INFO("%s: authenticated %s (id: %d, ip: %s)\n",
                 __func__, account->userid, auth->account_id, ip);
    }

    account->account_id = auth->account_id;
    account->login_id1 = mt_random();
    account->login_id2 = mt_random();
    STRZCPY(account->lastlogin, auth->lastlogin);
    STRZCPY(auth->lastlogin, stamp_now(true));
    account->sex = auth->sex;
    STRZCPY(auth->last_ip, ip);
    auth->logincount++;

    return AUTH_OK;
}

/// Kill char servers that don't send the common packet after 5 calls
static void char_anti_freeze_system(timer_id, tick_t)
{
    for (sint32 i = 0; i < MAX_SERVERS; i++)
    {
        if (server[i].fd < 0)
            continue;
        if (!server[i].freezeflag--)
        {
            LOG_WARN("%s: disconnect frozen char-server #%d '%s'.\n",
                     __func__, i, server[i].name);
            session[server[i].fd]->eof = 1;
        }
    }
}




/// authenticate an account to the char-server
// uint16 packet, uint32 acc, uint32 login_id[2], char sex, uint32 ip
static void x2712(sint32 fd, sint32 id)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    for (sint32 i = 0; i < AUTH_FIFO_SIZE; i++)
    {
        if (auth_fifo[i].account_id != acc ||
                auth_fifo[i].login_id1 != RFIFOL(fd, 6) ||
                auth_fifo[i].login_id2 != RFIFOL(fd, 10) ||
                auth_fifo[i].sex != static_cast<SEX>(RFIFOB(fd, 14)) ||
                auth_fifo[i].ip.to_n() != RFIFOL(fd, 15) ||
                auth_fifo[i].delflag)
            continue;
        auth_fifo[i].delflag = 1;
        LOG_INFO("Char-server '%s': authenticated %d (ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        for (AuthData& ad : auth_dat)
        {
            if (ad.account_id != acc)
                continue;
            // send ## variables
            WFIFOW(fd, 0) = 0x2729;
            WFIFOL(fd, 4) = uint32(acc);
            sint32 p = 8;
            for (sint32 j = 0; j < ad.account_reg2_num; j++)
            {
                STRZCPY2(sign_cast<char *>(WFIFOP(fd, p)),
                         ad.account_reg2[j].str);
                p += 32;
                WFIFOL(fd, p) = ad.account_reg2[j].value;
                p += 4;
            }
            WFIFOW(fd, 2) = p;
            WFIFOSET(fd, p);
            // send player email and expiration
            WFIFOW(fd, 0) = 0x2713;
            WFIFOL(fd, 2) = uint32(acc);
            WFIFOB(fd, 6) = 0;
            STRZCPY2(sign_cast<char *>(WFIFOP(fd, 7)), ad.email);
            WFIFOL(fd, 47) = 0; //ad.connect_until_time;
            WFIFOSET(fd, 51);
            return;
        } // for k in auth_dat
        return;
    } // for i in auth_fifo
    // authentication not found
    LOG_INFO("Char-server '%s': denied auth %d (ip: %s).\n",
             server[id].name, acc, ip_of(fd));
    WFIFOW(fd, 0) = 0x2713;
    WFIFOL(fd, 2) = uint32(acc);
    WFIFOB(fd, 6) = 1;
    // It is unnecessary to send email
    // It is unnecessary to send validity date of the account
    WFIFOSET(fd, 51);
}

/// Report of number of users on the server
// uint16 packet, uint32 usercount
static void x2714(sint32 fd, sint32 id)
{
    server[id].users = RFIFOL(fd, 2);
    if (anti_freeze_enable)
        server[id].freezeflag = 5;
}

/// Request initial setting of email (no answer, but may fail)
// uint16 packet, uint32 acc, char email[40]
static void x2715(sint32 fd, sint32 id)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    char email[40];
    STRZCPY(email, sign_cast<const char *>(RFIFOP(fd, 6)));
    remove_control_chars(email);
    if (!e_mail_check(email))
    {
        LOG_INFO("Char-server '%s': refused to init email by %d (ip: %s)\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    AuthData *auth = account_by_id(acc);
    if (!auth)
    {
        LOG_INFO("Char-server '%s': refused to init email - no such account %d (ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    if (strcmp(auth->email, "a@a.com") != 0)
    {
        LOG_INFO("Char-server '%s': refused to init email for %d - it is already set (ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    STRZCPY(auth->email, email);
    LOG_INFO("Char-server '%s': init email (account: %d, e-mail: %s, ip: %s).\n",
             server[id].name, acc, email, ip_of(fd));
}

/// Request email and expiration time
// uint16 packet, uint32 account
static void x2716(sint32 fd, sint32 id)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    AuthData *auth = account_by_id(acc);
    if (!auth)
    {
        LOG_INFO("Char-server '%s': can't send e-mail - no account %d (ip: %s).\n",
                 server[id].name, RFIFOL(fd, 2), ip_of(fd));
        return;
    }
    LOG_INFO("Char-server '%s': send e-mail of %u (ip: %s).\n",
             server[id].name, acc, ip_of(fd));
    WFIFOW(fd, 0) = 0x2717;
    WFIFOL(fd, 2) = uint32(acc);
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->email);
    WFIFOL(fd, 46) = 0; //auth->connect_until_time;
    WFIFOSET(fd, 50);
}

/// Request to become GM
// uint16 packet, uint16 len, char gm_pass[len]
static void x2720(sint32 fd, sint32 id)
{
    uint8 buf[10];
    account_t acc = account_t(RFIFOL(fd, 4));
    WBUFW(buf, 0) = 0x2721;
    WBUFL(buf, 2) = uint32(acc);
    WBUFL(buf, 6) = 0; // level of gm they became
    if (!level_new_gm)
    {
        LOG_INFO("Char-server '%s': request to make %u a GM, but GM creation is disable (ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    if (strcmp(sign_cast<const char *>(RFIFOP(fd, 8)), gm_pass) != 0)
    {
        LOG_INFO("Failed to make %u a GM: incorrect password (ip: %s).\n",
                 acc, ip_of(fd));
        return;
    }
    if (is_gm(acc))
    {
        LOG_INFO("Char-server '%s': Error: %d is already a GM (ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    FILE *fp = fopen_(gm_account_filename, "a");
    if (!fp)
    {
        LOG_INFO("Char-server '%s': %s: %m\n",
                 server[id].name, gm_account_filename);
        return;
    }
    FPRINTF(fp, "\n// %s: @GM command\n%d %d\n", stamp_now(false), acc, level_new_gm);
    fclose_(fp);
    WBUFL(buf, 6) = uint8(level_new_gm);
    // FIXME: this is stupid
    read_gm_account();
    send_gm_accounts();
    LOG_INFO("Char-server '%s': give %d gm level %d (ip: %s).\n",
             server[id].name, acc, level_new_gm, ip_of(fd));
    // Note: this used to be sent even if it failed
    charif_sendallwos(-1, buf, 10);
}

/// Map server request (via char-server) to change an email
// uint16 packet, uint32 acc, char email[40], char new_email[40]
static void x2722(sint32 fd, sint32 id)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    char actual_email[40];
    STRZCPY(actual_email, sign_cast<const char *>(RFIFOP(fd, 6)));
    remove_control_chars(actual_email);
    char new_email[40];
    STRZCPY(new_email, sign_cast<const char *>(RFIFOP(fd, 46)));
    remove_control_chars(new_email);

    // is this needed?
    if (!e_mail_check(actual_email))
    {
        LOG_INFO("Char-server '%s': actual email is invalid (account: %d, ip: %s)\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    if (!e_mail_check(new_email))
    {
        LOG_INFO("Char-server '%s': invalid new e-mail (account: %d, ip: %s)\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    if (strcasecmp(new_email, "a@a.com") == 0)
    {
        LOG_INFO("Char-server '%s': setting email to default is not allowed (account: %d, ip: %s)\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    AuthData *auth = account_by_id(acc);
    if (!auth)
    {
        LOG_INFO("Char-server '%s': Attempt to modify an e-mail on an account (@email GM command), but account doesn't exist (account: %d, ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }

    if (strcasecmp(auth->email, actual_email) != 0)
    {
        LOG_INFO("Char-server '%s': fail to change email (account: %u (%s), actual e-mail: %s, but given e-mail: %s, ip: %s).\n",
                 server[id].name, acc, auth->userid, auth->email, actual_email,
                 ip_of(fd));
        return;
    }
    STRZCPY(auth->email, new_email);
    LOG_INFO("Char-server '%s': change e-mail for %d (%s) to %s (ip: %s).\n",
             server[id].name, acc, auth->userid, new_email, ip_of(fd));
}

/// change state of a player (only used for block/unblock)
// uint16 packet, uint32 acc, uint32 state (0 or 5)
static void x2724(sint32 fd, sint32 id)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    enum auth_failure state = static_cast<enum auth_failure>(RFIFOL(fd, 6));
    AuthData *auth = account_by_id(acc);
    if (!auth)
    {
        LOG_INFO("Char-server '%s': failed to change state of %d to %d - no such account (ip: %s).\n",
                 server[id].name, acc, state, ip_of(fd));
        return;
    }
    if (auth->state == state)
    {
        LOG_INFO("Char-server '%s':  Error: state of %d already %d (ip: %s).\n",
                 server[id].name, acc, state, ip_of(fd));
        return;
    }
    LOG_INFO("Char-server '%s': change state of %d to %hhu (ip: %s).\n",
             server[id].name, acc, static_cast<uint8>(state), ip_of(fd));
    auth->state = state;
    if (!state)
        return;
    uint8 buf[11];
    WBUFW(buf, 0) = 0x2731;
    WBUFL(buf, 2) = uint32(acc);
    // 0: change of state, 1: ban
    WBUFB(buf, 6) = 0;
    // state (or final date of a banishment)
    WBUFL(buf, 7) = state;
    charif_sendallwos(-1, buf, 11);
    for (sint32 j = 0; j < AUTH_FIFO_SIZE; j++)
        if (auth_fifo[j].account_id == acc)
            // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
            auth_fifo[j].login_id1++;
}

/// ban request from map-server (via char-server)
// uint16 packet, uint32 acc, uint16 Y,M,D,h,m,s
static void x2725(sint32 fd, sint32 id)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    AuthData *auth = account_by_id(acc);
    if (!auth)
    {
        LOG_INFO("Char-server '%s': Error of ban request (account: %d not found, ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    time_t timestamp = time(NULL);
    if (auth->ban_until_time >= timestamp)
        timestamp = auth->ban_until_time;
    // TODO check for overflow
    // years (365.25 days)
    timestamp += 31557600 * static_cast<sint16>(RFIFOW(fd, 6));
    // a month isn't well-defined - use 1/12 of a year
    timestamp += 2629800 * static_cast<sint16>(RFIFOW(fd, 8));
    timestamp += 86400 * static_cast<sint16>(RFIFOW(fd, 10));
    timestamp += 3600 * static_cast<sint16>(RFIFOW(fd, 12));
    timestamp += 60 * static_cast<sint16>(RFIFOW(fd, 14));
    timestamp += static_cast<sint16>(RFIFOW(fd, 16));
    if (auth->ban_until_time == timestamp)
    {
        LOG_INFO("Char-server '%s': Error of ban request (account: %d, no change for ban date, ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    if (timestamp <= time(NULL))
    {
        LOG_INFO("Char-server '%s': Error of ban request (account: %d, new date unbans the account, ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    LOG_INFO("Char-server '%s': Ban request (account: %d, new final date of banishment: %d (%s), ip: %s).\n",
             server[id].name, acc, static_cast<sint32>(timestamp),
             stamp_time(timestamp, "no banishment"), ip_of(fd));
    uint8 buf[11];
    WBUFW(buf, 0) = 0x2731;
    WBUFL(buf, 2) = uint32(auth->account_id);
    // 0: change of state, 1: ban
    WBUFB(buf, 6) = 1;
    // final date of a banishment (or new state)
    WBUFL(buf, 7) = timestamp;
    charif_sendallwos(-1, buf, 11);
    for (sint32 j = 0; j < AUTH_FIFO_SIZE; j++)
        if (auth_fifo[j].account_id == acc)
            // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
            auth_fifo[j].login_id1++;
    auth->ban_until_time = timestamp;
}

/// Request for sex change
// uint16 packet, uint32 acc
static void x2727(sint32 fd, sint32 id)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    AuthData *auth = account_by_id(acc);
    if (!auth)
    {
        LOG_INFO("Char-server '%s': Error of sex change (account: %d not found, sex would be reversed, ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    switch (auth->sex)
    {
    case SEX::FEMALE: auth->sex = SEX::MALE; break;
    case SEX::MALE: auth->sex = SEX::FEMALE; break;
    case SEX::SERVER:
        LOG_INFO("Char-server '%s': can't change sex of server account %d (ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    uint8 buf[16];
    LOG_INFO("Char-server '%s': change sex of %u to %c (ip: %s).\n",
             server[id].name, acc, sex_to_char(auth->sex), ip_of(fd));
    for (sint32 j = 0; j < AUTH_FIFO_SIZE; j++)
        if (auth_fifo[j].account_id == acc)
            // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
            auth_fifo[j].login_id1++;
    WBUFW(buf, 0) = 0x2723;
    WBUFL(buf, 2) = uint32(acc);
    WBUFB(buf, 6) = static_cast<uint8>(auth->sex);
    charif_sendallwos(-1, buf, 7);
}

/// Receive ## variables a char-server, and forward them to other char-servers
// uint16 packet, uint16 len, {char[32] name, sint32 val}[]
// note - this code assumes that len is proper, i.e len % 36 == 4
static void x2728(sint32 fd, sint32 id)
{
    account_t acc = account_t(RFIFOL(fd, 4));
    AuthData *auth = account_by_id(acc);
    if (!auth)
    {
        LOG_INFO("Char-server '%s': receiving (from the char-server) of account_reg2 (account: %d not found, ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }

    LOG_INFO("Char-server '%s': receiving ## variables (account: %d, ip: %s).\n",
             server[id].name, acc, ip_of(fd));
    sint32 p = 8;
    sint32 j;
    for (j = 0; p < RFIFOW(fd, 2) && j < ACCOUNT_REG2_NUM; j++)
    {
        STRZCPY(auth->account_reg2[j].str, sign_cast<const char *>(RFIFOP(fd, p)));
        p += 32;
        remove_control_chars(auth->account_reg2[j].str);
        auth->account_reg2[j].value = RFIFOL(fd, p);
        p += 4;
    }
    auth->account_reg2_num = j;
    // Sending information towards the other char-servers.
    session[fd]->rfifo_change_packet(0x2729);
    charif_sendallwos(fd, RFIFOP(fd, 0), RFIFOW(fd, 2));
}

/// unban request
// uint16 packet, uint32 acc
static void x272a(sint32 fd, sint32 id)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    AuthData *auth = account_by_id(acc);
    if (!auth)
    {
        LOG_INFO("Char-server '%s': Error of UnBan request (account: %d not found, ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    if (!auth->ban_until_time)
    {
        LOG_INFO("Char-server '%s': request to unban account %d, which wasn't banned (ip: %s).\n",
                 server[id].name, acc, ip_of(fd));
        return;
    }
    auth->ban_until_time = 0;
    LOG_INFO("Char-server '%s': unban account %d (ip: %s).\n",
             server[id].name, acc, ip_of(fd));
}

/// request to change account password
// uint16 packet, uint32 acc, char old[24], char new[24]
static void x2740(sint32 fd, sint32 id)
{
    account_t acc = account_t(RFIFOL(fd, 2));
    char actual_pass[24];
    STRZCPY(actual_pass, sign_cast<const char *>(RFIFOP(fd, 6)));
    remove_control_chars(actual_pass);
    char new_pass[24];
    STRZCPY(new_pass, sign_cast<const char *>(RFIFOP(fd, 30)));
    remove_control_chars(new_pass);

    enum passwd_failure status = PASSWD_NO_ACCOUNT;
    AuthData *auth = account_by_id(acc);
    if (!auth)
        goto send_x272a_reply;

    if (pass_ok(actual_pass, auth->pass))
    {
        if (strlen(new_pass) < 4)
            status = PASSWD_TOO_SHORT;
        else
        {
            status = PASSWD_OK;
            STRZCPY(auth->pass, MD5_saltcrypt(new_pass, make_salt()));
            LOG_INFO("Char-server '%s': Change pass success (account: %d (%s), ip: %s.\n",
                     server[id].name, acc, auth->userid, ip_of(fd));
        }
    }
    else
    {
        status = PASSWD_WRONG_PASSWD;
        LOG_INFO("Char-server '%s': Attempt to modify a pass failed, wrong password. (account: %d (%s), ip: %s).\n",
                 server[id].name, acc, auth->userid, ip_of(fd));
    }
send_x272a_reply:
    WFIFOW(fd, 0) = 0x2741;
    WFIFOL(fd, 2) = uint32(acc);
    WFIFOB(fd, 6) = status;
    WFIFOSET(fd, 7);
}


/// Parse packets from a char server
static void parse_fromchar(sint32 fd)
{
    sint32 id;
    for (id = 0; id < MAX_SERVERS; id++)
        if (server[id].fd == fd)
            break;
    if (id == MAX_SERVERS || session[fd]->eof)
    {
        if (id < MAX_SERVERS)
        {
            LOG_INFO("Char-server '%s' has disconnected (ip: %s).\n",
                     server[id].name, ip_of(fd));
            memset(&server[id], 0, sizeof(struct mmo_char_server));
            server[id].fd = -1;
        }
        else
        {
            LOG_INFO("Invalid char server (ip: %s)\n", ip_of(fd));
        }
        close(fd);
        delete_session(fd);
        return;
    }

    while (RFIFOREST(fd) >= 2)
    {
        if (display_parse_fromchar == CP_ALL ||
            (display_parse_fromchar == CP_MOST && RFIFOW(fd, 0) != 0x2714))
            // 0x2714 is done very often (number of players)
            LOG_INFO("%s: connection #%d, packet: 0x%x (with %d bytes).\n",
                     __func__, fd, RFIFOW(fd, 0), RFIFOREST(fd));

        switch (RFIFOW(fd, 0))
        {
            /// authenticate an account to the char-server
            // uint16 packet, uint32 acc, uint32 login_id[2], char sex, uint32 ip
            case 0x2712:
                if (RFIFOREST(fd) < 19)
                    return;
                x2712(fd, id);
                RFIFOSKIP(fd, 19);
                break;

            /// Report of number of users on the server
            // uint16 packet, uint32 usercount
            case 0x2714:
                if (RFIFOREST(fd) < 6)
                    return;
                x2714(fd, id);
                RFIFOSKIP(fd, 6);
                break;

            /// Request initial setting of email (no answer, but may fail)
            // uint16 packet, uint32 acc, char email[40]
            case 0x2715:
                if (RFIFOREST(fd) < 46)
                    return;
                x2715(fd, id);
                RFIFOSKIP(fd, 46);
                break;

            /// Request email and expiration time
            // uint16 packet, uint32 account
            case 0x2716:
                if (RFIFOREST(fd) < 6)
                    return;
                x2716(fd, id);
                RFIFOSKIP(fd, 6);
                break;

            /// Request to become GM
            // uint16 packet, uint16 len, char gm_pass[len]
            case 0x2720:
                if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                    return;
                x2720(fd, id);
                RFIFOSKIP(fd, RFIFOW(fd, 2));
                return;

            /// Map server request (via char-server) to change an email
            // uint16 packet, uint32 acc, char email[40], char new_email[40]
            case 0x2722:
                if (RFIFOREST(fd) < 86)
                    return;
                x2722(fd, id);
                RFIFOSKIP(fd, 86);
                break;

            /// change state of a player (only used for block/unblock)
            // uint16 packet, uint32 acc, uint32 state (0 or 5)
            case 0x2724:
                if (RFIFOREST(fd) < 10)
                    return;
                x2724(fd, id);
                RFIFOSKIP(fd, 10);
                return;

            /// ban request from map-server (via char-server)
            // uint16 packet, uint32 acc, uint16 Y,M,D,h,m,s
            case 0x2725:
                if (RFIFOREST(fd) < 18)
                    return;
                x2725(fd, id);
                RFIFOSKIP(fd, 18);
                return;

            /// Request for sex change
            // uint16 packet, uint32 acc
            case 0x2727:
                if (RFIFOREST(fd) < 6)
                    return;
                x2727(fd, id);
                RFIFOSKIP(fd, 6);
                return;

            /// Receive ## variables a char-server, and forward them to other char-servers
            // uint16 packet, uint16 len, {char[32] name, sint32 val}[]
            // note - this code assumes that len is proper, i.e len % 36 == 4
            case 0x2728:
                if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                    return;
                x2728(fd, id);
                RFIFOSKIP(fd, RFIFOW(fd, 2));
                break;

            /// unban request
            // uint16 packet, uint32 acc
            case 0x272a:
                if (RFIFOREST(fd) < 6)
                    return;
                x272a(fd, id);
                RFIFOSKIP(fd, 6);
                return;

            /// request to change account password
            // uint16 packet, uint32 acc, char old[24], char new[24]
            case 0x2740:
                if (RFIFOREST(fd) < 54)
                    return;
                x2740(fd, id);
                RFIFOSKIP(fd, 54);
                break;

            default:
                unknown_packet_log.warn("%s: unknown packet %hx-> disconnection\n",
                                        __func__, RFIFOW(fd, 0));
                unknown_packet_log.debug("%s: connection #%d (ip: %s), packet: 0x%hu (with %u bytes available).\n",
                                         __func__, fd, ip_of(fd), RFIFOW(fd, 0), RFIFOREST(fd));
                hexdump(unknown_packet_log, RFIFOP(fd, 0), RFIFOREST(fd));
                unknown_packet_log.debug("\n");
                session[fd]->eof = 1;
                return;
        } // switch packet
    } // while packets available
    return;
}



/// Server version
// uint16 packet
static void x7530(sint32 fd, bool ladmin)
{
    LOG_INFO("%sRequest server version (ip: %s)\n",
             ladmin ? "'ladmin': " : "", ip_of(fd));
    WFIFOW(fd, 0) = 0x7531;
    memcpy(WFIFOP(fd, 2), &tmwAthenaVersion, 8);
    WFIFOB(fd, 6) = new_account_flag ? 1 : 0;
    WFIFOB(fd, 7) = ATHENA_SERVER_LOGIN;
    WFIFOSET(fd, 10);
}

/// Request of end of connection
// uint16 packet
static void x7532(sint32 fd, const char *pfx)
{
    LOG_INFO("%sEnd of connection (ip: %s)\n", pfx, ip_of(fd));
    session[fd]->eof = 1;
}

/// Request list of accounts
// uint16 packet, uint32 start, uint32 end
static void x7920(sint32 fd)
{
    account_t st(RFIFOL(fd, 2));
    account_t ed(RFIFOL(fd, 6));
    WFIFOW(fd, 0) = 0x7921;
    if (st >= END_ACCOUNT_NUM)
        st = account_t(0);
    if (ed > END_ACCOUNT_NUM || ed < st)
        ed = END_ACCOUNT_NUM;
    LOG_INFO("'ladmin': List accounts from %d to %d (ip: %s)\n",
             st, ed, ip_of(fd));
    // Sending accounts information
    uint16 len = 4;
    for (AuthData& ad : range(auth_dat.lower_bound(st), auth_dat.upper_bound(ed)))
    {
        WFIFOL(fd, len) = uint32(ad.account_id);
        WFIFOB(fd, len + 4) = uint8(is_gm(ad.account_id));
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, len + 5)), ad.userid);
        WFIFOB(fd, len + 29) = static_cast<uint8>(ad.sex);
        WFIFOL(fd, len + 30) = ad.logincount;
        // if no state, but banished - FIXME can this happen?
        if (!ad.state == 0 && ad.ban_until_time)
            WFIFOL(fd, len + 34) = 7;
        else
            WFIFOL(fd, len + 34) = ad.state;
        len += 38;
    }
    WFIFOW(fd, 2) = len;
    WFIFOSET(fd, len);
}

/// Itemfrob: change ID of an existing item
// uint16 packet, uint32 old_id, uint32 new_id
static void x7924(sint32 fd)
{
    charif_sendallwos(-1, RFIFOP(fd, 0), 10); // forward package to char servers
    WFIFOW(fd, 0) = 0x7925;
    WFIFOSET(fd, 2);
}

/// Request for account creation
// uint16 packet, char userid[24], char passwd[24], char sex, char email[40]
static void x7930(sint32 fd)
{
    struct mmo_account ma;
    STRZCPY(ma.userid, sign_cast<const char *>(RFIFOP(fd, 2)));
    STRZCPY(ma.passwd, sign_cast<const char *>(RFIFOP(fd, 26)));
    STRZCPY(ma.lastlogin, "-");
    ma.sex = sex_from_char(RFIFOB(fd, 50));
    WFIFOW(fd, 0) = 0x7931;
    WFIFOL(fd, 2) = -1;
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), ma.userid);
    if (strlen(ma.userid) < 4 || strlen(ma.passwd) < 4)
    {
        LOG_INFO("'ladmin': Attempt to create an invalid account (account or pass is too short, ip: %s)\n",
                 ip_of(fd));
        return;
    }
    if (ma.sex != SEX::FEMALE && ma.sex != SEX::MALE)
    {
        LOG_INFO("'ladmin': Attempt to create an invalid account (account: %s, invalid sex, ip: %s)\n",
                 ma.userid, ip_of(fd));
        return;
    }
    if (account_id_count > END_ACCOUNT_NUM)
    {
        LOG_INFO("'ladmin': Attempt to create an account, but there is no more available id number (account: %s, sex: %c, ip: %s)\n",
                 ma.userid, sex_to_char(ma.sex), ip_of(fd));
        return;
    }
    remove_control_chars(ma.userid);
    remove_control_chars(ma.passwd);
    for (AuthData& ad : auth_dat)
    {
        if (strcmp(ad.userid, ma.userid) == 0)
        {
            LOG_INFO("'ladmin': Attempt to create an already existing account (account: %s, ip: %s)\n",
                     ad.userid, ip_of(fd));
            return;
        }
    }
    char email[40] = {};
    STRZCPY(email, sign_cast<const char *>(RFIFOP(fd, 51)));
    remove_control_chars(email);
    AuthData *new_ad = mmo_auth_new(&ma, email);
    LOG_INFO("'ladmin': Account creation (account: %s (id: %d), sex: %c, email: %s, ip: %s)\n",
             ma.userid, new_ad->account_id, sex_to_char(ma.sex), new_ad->email, ip_of(fd));
    WFIFOL(fd, 2) = uint32(new_ad->account_id);
}

/// Request for an account deletion
// uint16 packet, char userid[24]
static void x7932(sint32 fd)
{
    WFIFOW(fd, 0) = 0x7933;
    WFIFOL(fd, 2) = -1;
    char account_name[24];
    STRZCPY(account_name, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account_name);
    AuthData *auth = account_by_name(account_name);
    if (!auth)
    {
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), account_name);
        LOG_INFO("'ladmin': Attempt to delete an unknown account (account: %s, ip: %s)\n",
                 account_name, ip_of(fd));
        return;
    }
    // Char-server is notified of deletion (for characters deletion).
    uint8 buf[6];
    WBUFW(buf, 0) = 0x2730;
    WBUFL(buf, 2) = uint32(auth->account_id);
    charif_sendallwos(-1, buf, 6);
    // send answer
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->userid);
    WFIFOL(fd, 2) = uint32(auth->account_id);
    // save deleted account in log file
    LOG_INFO("'ladmin': Account deletion (account: %s, id: %d, ip: %s) - not saved:\n",
             auth->userid, auth->account_id, ip_of(fd));
    auth_dat.erase(auth->account_id);
}

/// Request to change password
// uint16 packet, char userid[24], char passwd[24]
static void x7934(sint32 fd)
{
    WFIFOW(fd, 0) = 0x7935;
    WFIFOL(fd, 2) = -1;
    char account_name[24];
    STRZCPY(account_name, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account_name);
    AuthData *auth = account_by_name(account_name);
    if (!auth)
    {
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), account_name);
        LOG_INFO("'ladmin': Attempt to modify the password of an unknown account (account: %s, ip: %s)\n",
                 account_name, ip_of(fd));
        return;
    }
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->userid);
    STRZCPY(auth->pass, MD5_saltcrypt(sign_cast<const char *>(RFIFOP(fd, 26)), make_salt()));
    WFIFOL(fd, 2) = uint32(auth->account_id);
    LOG_INFO("'ladmin': Modification of a password (account: %s, new password: %s, ip: %s)\n",
             auth->userid, auth->pass, ip_of(fd));
}

/// Modify a state
// uint16 packet, char userid[24], uint32 state, char error_message[20]
// error_message is usually the date of the end of banishment
static void x7936(sint32 fd)
{
    WFIFOW(fd, 0) = 0x7937;
    WFIFOL(fd, 2) = -1;
    char account_name[24];
    STRZCPY(account_name, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account_name);
    uint32 status = RFIFOL(fd, 26);
    WFIFOL(fd, 30) = status;
    char error_message[20];
    STRZCPY(error_message, sign_cast<const char *>(RFIFOP(fd, 30)));
    remove_control_chars(error_message);
    if (status != 7 || error_message[0] == '\0')
        // 7: // 6 = Your are Prohibited to log in until %s
        STRZCPY(error_message, "-");
    AuthData *auth = account_by_name(account_name);
    if (!auth)
    {
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), account_name);
        LOG_INFO("'ladmin': Attempt to modify the state of an unknown account (account: %s, received state: %d, ip: %s)\n",
                 account_name, status, ip_of(fd));
        return;
    }
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->userid);
    WFIFOL(fd, 2) = uint32(auth->account_id);
    if (auth->state == status &&
            strcmp(auth->error_message, error_message) == 0)
    {
        LOG_INFO("'ladmin': Modification of a state, but the state of the account is already the requested state (account: %s, received state: %d, ip: %s)\n",
                 account_name, status, ip_of(fd));
        return;
    }
    if (status == 7)
        LOG_INFO("'ladmin': Modification of a state (account: %s, new state: %d - prohibited to login until '%s', ip: %s)\n",
                 auth->userid, status, error_message, ip_of(fd));
    else
        LOG_INFO("'ladmin': Modification of a state (account: %s, new state: %d, ip: %s)\n",
                 auth->userid, status, ip_of(fd));
    if (auth->state == 0)
    {
        uint8 buf[16];
        WBUFW(buf, 0) = 0x2731;
        WBUFL(buf, 2) = uint32(auth->account_id);
        WBUFB(buf, 6) = 0; // 0: change of state, 1: ban
        WBUFL(buf, 7) = status;    // status or final date of a banishment
        charif_sendallwos(-1, buf, 11);
        for (sint32 j = 0; j < AUTH_FIFO_SIZE; j++)
            if (auth_fifo[j].account_id == auth->account_id)
                // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
                auth_fifo[j].login_id1++;
    }
    auth->state = static_cast<enum auth_failure>(status);
    STRZCPY(auth->error_message, error_message);
}

/// Request for servers list and # of online players
// uint32 packet
static void x7938(sint32 fd)
{
    LOG_INFO("'ladmin': Sending of servers list (ip: %s)\n", ip_of(fd));
    sint32 server_num = 0;
    for (sint32 i = 0; i < MAX_SERVERS; i++)
    {
        if (server[i].fd < 0)
            continue;
        WFIFOL(fd, 4 + server_num * 32) = server[i].ip.to_n();
        WFIFOW(fd, 4 + server_num * 32 + 4) = server[i].port;
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 4 + server_num * 32 + 6)), server[i].name);
        WFIFOW(fd, 4 + server_num * 32 + 26) = server[i].users;
        WFIFOW(fd, 4 + server_num * 32 + 28) = server[i].maintenance;
        WFIFOW(fd, 4 + server_num * 32 + 30) = server[i].is_new;
        server_num++;
    }
    WFIFOW(fd, 0) = 0x7939;
    WFIFOW(fd, 2) = 4 + 32 * server_num;
    WFIFOSET(fd, 4 + 32 * server_num);
}

/// Request to check password
// uint16 packet, char userid[24], char passwd[24]
static void x793a(sint32 fd)
{
    WFIFOW(fd, 0) = 0x793b;
    WFIFOL(fd, 2) = -1;
    char account_name[24];
    STRZCPY(account_name, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account_name);
    AuthData *auth = account_by_name(account_name);
    if (!auth)
    {
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), account_name);
        LOG_INFO("'ladmin': Attempt to check the password of an unknown account (account: %s, ip: %s)\n",
                 account_name, ip_of(fd));
        return;
    }
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->userid);
    if (pass_ok(sign_cast<const char *>(RFIFOP(fd, 26)), auth->pass))
    {
        WFIFOL(fd, 2) = uint32(auth->account_id);
        LOG_INFO("'ladmin': Check of password OK (account: %s, password: %s, ip: %s)\n",
                 auth->userid, auth->pass, ip_of(fd));
    }
    else
    {
        char pass[24];
        STRZCPY(pass, sign_cast<const char *>(RFIFOP(fd, 26)));
        remove_control_chars(pass);
        LOG_INFO("'ladmin': Failure of password check (account: %s, proposed pass: %s, ip: %s)\n",
                 auth->userid, pass, ip_of(fd));
    }
}

/// Request to modify sex
// uint32 packet, char userid[24], char sex
static void x793c(sint32 fd)
{
    WFIFOW(fd, 0) = 0x793d;
    WFIFOL(fd, 2) = -1;
    char account_name[24];
    STRZCPY(account_name, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account_name);
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), account_name);
    SEX sex = sex_from_char(RFIFOB(fd, 26));
    if (sex != SEX::FEMALE && sex != SEX::MALE)
    {
        if (RFIFOB(fd, 26) >= 0x20 && RFIFOB(fd, 26) < 0x7f)
            LOG_INFO("'ladmin': Attempt to give an invalid sex (account: %s, received sex: %c, ip: %s)\n",
                     account_name, RFIFOB(fd, 26), ip_of(fd));
        else
            LOG_INFO("'ladmin': Attempt to give an invalid sex (account: %s, received sex: %02hhx, ip: %s)\n",
                     account_name, RFIFOB(fd, 26), ip_of(fd));
        return;
    }
    AuthData *auth = account_by_name(account_name);
    if (!auth)
    {
        LOG_INFO("'ladmin': Attempt to modify the sex of an unknown account (account: %s, received sex: %c, ip: %s)\n",
                 account_name, sex_to_char(sex), ip_of(fd));
        return;
    }
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->userid);
    if (auth->sex == sex)
    {
        LOG_INFO("'ladmin': Modification of a sex, but the sex is already the requested sex (account: %s, sex: %c, ip: %s)\n",
                 auth->userid, sex_to_char(sex), ip_of(fd));
        return;
    }
    WFIFOL(fd, 2) = uint32(auth->account_id);
    for (sint32 j = 0; j < AUTH_FIFO_SIZE; j++)
        if (auth_fifo[j].account_id == auth->account_id)
            // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
            auth_fifo[j].login_id1++;
    auth->sex = sex;
    LOG_INFO("'ladmin': Modification of a sex (account: %s, new sex: %c, ip: %s)\n",
             auth->userid, sex_to_char(sex), ip_of(fd));
    // send to all char-server the change
    uint8 buf[7];
    WBUFW(buf, 0) = 0x2723;
    WBUFL(buf, 2) = uint32(auth->account_id);
    WBUFB(buf, 6) = static_cast<uint8>(auth->sex);
    charif_sendallwos(-1, buf, 7);
}

/// Request to modify GM level
// uint16 packet, char userid[24], uint8 new_gm_level
static void x793e(sint32 fd)
{
    WFIFOW(fd, 0) = 0x793f;
    WFIFOL(fd, 2) = -1;
    char account_name[24];
    STRZCPY(account_name, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account_name);
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), account_name);
    gm_level_t new_gm_level = gm_level_t(RFIFOB(fd, 26));
    AuthData *auth = account_by_name(account_name);
    if (!auth)
    {
        LOG_INFO("'ladmin': Attempt to modify the GM level of an unknown account (account: %s, received GM level: %hhu, ip: %s)\n",
                 account_name, new_gm_level, ip_of(fd));
        return;
    }
    account_t acc = auth->account_id;
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->userid);
    if (is_gm(acc) == new_gm_level)
    {
        LOG_INFO("'ladmin': Attempt to modify of a GM level, but the GM level is already the good GM level (account: %s (%u), GM level: %hhu, ip: %s)\n",
                 auth->userid, acc, new_gm_level, ip_of(fd));
        return;
    }
    // modification of the file
    sint32 lock;
    FILE *fp2 = lock_fopen(gm_account_filename, &lock);
    if (!fp2)
    {
        LOG_INFO("'ladmin': Attempt to modify of a GM level - impossible to write GM accounts file (account: %s (%u), received GM level: %hhu, ip: %s)\n",
                 auth->userid, acc, new_gm_level, ip_of(fd));
        return;
    }
    FILE *fp = fopen_(gm_account_filename, "r");
    if (!fp)
    {
        LOG_INFO("'ladmin': Attempt to modify of a GM level - impossible to read GM accounts file (account: %s (%u), received GM level: %hhu, ip: %s)\n",
                 auth->userid, acc, new_gm_level, ip_of(fd));
        goto end_x793e_lock;
    }
    // code block to allow local variables
    {
        const char *tmpstr = stamp_now(false);
        bool modify_flag = 0;
        char line[512];
        // read/write GM file
        while (fgets(line, sizeof(line), fp))
        {
            // strip off both the '\r' and the '\n', if they exist
            // FIXME - isn't this file opened in text mode?
            while (line[0] != '\0' && (line[strlen(line) - 1] == '\n' || line[strlen(line) - 1] == '\r'))
                line[strlen(line) - 1] = '\0';
            // FIXME: won't this case be caught below, when sscanf fails?
            if ((line[0] == '/' && line[1] == '/') || line[0] == '\0')
            {
                fprintf(fp2, "%s\n", line);
                continue;
            }
            account_t gm_account;
            gm_level_t gm_level;
            if (SSCANF(line, "%u %hhu", &gm_account, &gm_level) != 2
                && SSCANF(line, "%u: %hhu", &gm_account, &gm_level) != 2)
            {
                fprintf(fp2, "%s\n", line);
                continue;
            }
            if (gm_account != acc)
            {
                fprintf(fp2, "%s\n", line);
                continue;
            }
            modify_flag = 1;
            if (!new_gm_level)
            {
                FPRINTF(fp2, "// %s: 'ladmin' remove %u '%s' GM level %hhu\n"
                             "//%d %d\n",
                        tmpstr, acc, auth->userid, gm_level,
                        acc, new_gm_level);
            }
            else
            {
                FPRINTF(fp2, "// %s: 'ladmin' change %u '%s' from GM level %hhu\n"
                             "%u %hhu\n",
                        tmpstr, acc, auth->userid, gm_level,
                        acc, new_gm_level);
            }
        }
        if (!modify_flag)
            FPRINTF(fp2, "// %s: 'ladmin' make %d '%s' a new GM\n"
                         "%d %d\n",
                    tmpstr, acc, auth->userid,
                    acc, new_gm_level);
        fclose_(fp);
    }
end_x793e_lock:
    lock_fclose(fp2, gm_account_filename, &lock);
    WFIFOL(fd, 2) = uint32(acc);
    LOG_INFO("'ladmin': Modification of a GM level (account: %s (%d), new GM level: %hhu, ip: %s)\n",
             auth->userid, acc, new_gm_level, ip_of(fd));
    // read and send new GM informations
    // FIXME: this is stupid
    read_gm_account();
    send_gm_accounts();
}

/// Request to modify e-mail
// uint16 packet, char userid[24], char email[40]
static void x7940(sint32 fd)
{
    WFIFOW(fd, 0) = 0x7941;
    WFIFOL(fd, 2) = -1;
    char account_name[24];
    STRZCPY(account_name, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account_name);
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), account_name);
    char email[40] = {};
    STRZCPY(email, sign_cast<const char *>(RFIFOP(fd, 26)));
    if (!e_mail_check(email))
    {
        LOG_INFO("'ladmin': Attempt to give an invalid e-mail (account: %s, ip: %s)\n",
                 account_name, ip_of(fd));
        return;
    }
    remove_control_chars(email);
    AuthData *auth = account_by_name(account_name);
    if (!auth)
    {
        LOG_INFO("'ladmin': Attempt to modify the e-mail of an unknown account (account: %s, received e-mail: %s, ip: %s)\n",
                 account_name, email, ip_of(fd));
        return;
    }
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->userid);
    STRZCPY(auth->email, email);
    WFIFOL(fd, 2) = uint32(auth->account_id);
    LOG_INFO("'ladmin': Modification of an email (account: %s, new e-mail: %s, ip: %s)\n",
             auth->userid, email, ip_of(fd));
}

/// Request to modify memo field
// uint16 packet, char usercount[24], uint16 msglen, char msg[msglen]
static void x7942(sint32 fd)
{
    WFIFOW(fd, 0) = 0x7943;
    WFIFOL(fd, 2) = -1;
    char account_name[24];
    STRZCPY(account_name, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account_name);
    AuthData *auth = account_by_name(account_name);
    if (!auth)
    {
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), account_name);
        LOG_INFO("'ladmin': Attempt to modify the memo field of an unknown account (account: %s, ip: %s)\n",
                 account_name, ip_of(fd));
        return;
    }
    static const size_t size_of_memo = sizeof(auth->memo) - 1;
    // auth->memo[0] must always be '!' or stuff breaks
    char *memo = auth->memo + 1;
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->userid);
    strzcpy(memo, sign_cast<const char *>(RFIFOP(fd, 28)), min(size_of_memo, RFIFOW(fd, 26)));
    remove_control_chars(memo);
    WFIFOL(fd, 2) = uint32(auth->account_id);
    LOG_INFO("'ladmin': Modification of a memo field (account: %s, new memo: %s, ip: %s)\n",
             auth->userid, memo, ip_of(fd));
}

/// Find account id from name
// uint16 packet, char userid[24]
static void x7944(sint32 fd)
{
    WFIFOW(fd, 0) = 0x7945;
    WFIFOL(fd, 2) = -1;
    char account_name[24];
    STRZCPY(account_name, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account_name);
    AuthData *auth = account_by_name(account_name);
    if (!auth)
    {
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), account_name);
        LOG_INFO("'ladmin': ID request (by the name) of an unknown account (account: %s, ip: %s)\n",
                 account_name, ip_of(fd));
        return;
    }
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->userid);
    WFIFOL(fd, 2) = uint32(auth->account_id);
    LOG_INFO("'ladmin': Request (by the name) of an account id (account: %s, id: %u, ip: %s)\n",
             auth->userid, auth->account_id, ip_of(fd));
}

/// Find an account name from id
// uint16 packet, uint32 acc
static void x7946(sint32 fd)
{
    WFIFOW(fd, 0) = 0x7947;
    account_t acc = account_t(RFIFOL(fd, 2));
    WFIFOL(fd, 2) = uint32(acc);
    AuthData *auth = account_by_id(acc);
    if (!auth)
    {
        LOG_INFO("'ladmin': Name request (by id) of an unknown account (id: %d, ip: %s)\n",
                 acc, ip_of(fd));
        // strcpy(WFIFOP(fd, 6), "");
        return;
    }
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->userid);
    LOG_INFO("'ladmin': Request (by id) of an account name (account: %s, id: %d, ip: %s)\n",
             auth->userid, RFIFOL(fd, 2), ip_of(fd));
}

/// Set the banishment timestamp
// uint16 packet, char userid[24], uint32 timestamp
static void x794a(sint32 fd)
{
    WFIFOW(fd, 0) = 0x794b;
    WFIFOL(fd, 2) = -1;
    char account_name[24];
    STRZCPY(account_name, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account_name);
    time_t timestamp = RFIFOL(fd, 26);
    if (timestamp <= time(NULL))
        timestamp = 0;
    WFIFOL(fd, 30) = timestamp;
    const char *tmpstr = stamp_time(timestamp, "no banishment");

    AuthData *auth = account_by_name(account_name);
    if (!auth)
    {
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), account_name);
        LOG_INFO("'ladmin': Attempt to change the final date of a banishment of an unknown account (account: %s, received final date of banishment: %d (%s), ip: %s)\n",
                 account_name, static_cast<sint32>(timestamp), tmpstr, ip_of(fd));
        return;
    }

    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->userid);
    WFIFOL(fd, 2) = uint32(auth->account_id);
    LOG_INFO("'ladmin': Change of the final date of a banishment (account: %s, new final date of banishment: %d (%s), ip: %s)\n",
             auth->userid, static_cast<sint32>(timestamp), tmpstr, ip_of(fd));
    if (auth->ban_until_time == timestamp)
        return;
    auth->ban_until_time = timestamp;
    if (!timestamp)
        return;
    uint8 buf[11];
    WBUFW(buf, 0) = 0x2731;
    WBUFL(buf, 2) = uint32(auth->account_id);
    WBUFB(buf, 6) = 1; // 0: change of status, 1: ban
    WBUFL(buf, 7) = timestamp; // status or final date of a banishment
    charif_sendallwos(-1, buf, 11);
    for (sint32 j = 0; j < AUTH_FIFO_SIZE; j++)
        if (auth_fifo[j].account_id == auth->account_id)
            // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
            auth_fifo[j].login_id1++;
}

/// Adjust the banishment end timestamp
// uint16 packet, char userid[24], sint16 year,mon,day,hr,min,sec
static void x794c(sint32 fd)
{
    WFIFOW(fd, 0) = 0x794d;
    WFIFOL(fd, 2) = -1;
    char account_name[24];
    STRZCPY(account_name, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account_name);
    AuthData *auth = account_by_name(account_name);
    if (!auth)
    {
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), account_name);
        LOG_INFO("'ladmin': Attempt to adjust the final date of a banishment of an unknown account (account: %s, ip: %s)\n",
                 account_name, ip_of(fd));
        WFIFOL(fd, 30) = 0;
        return;
    }
    WFIFOL(fd, 2) = uint32(auth->account_id);
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), auth->userid);

    time_t timestamp = time(NULL);
    if (auth->ban_until_time >= timestamp)
        timestamp = auth->ban_until_time;

    // TODO check for overflow
    // years (365.25 days)
    timestamp += 31557600 * static_cast<sint16>(RFIFOW(fd, 6));
    // a month isn't well-defined - use 1/12 of a year
    timestamp += 2629800 * static_cast<sint16>(RFIFOW(fd, 8));
    timestamp += 86400 * static_cast<sint16>(RFIFOW(fd, 10));
    timestamp += 3600 * static_cast<sint16>(RFIFOW(fd, 12));
    timestamp += 60 * static_cast<sint16>(RFIFOW(fd, 14));
    timestamp += static_cast<sint16>(RFIFOW(fd, 16));
    if (timestamp <= time(NULL))
        timestamp = 0;

    const char *tmpstr = stamp_time(timestamp, "no banishment");
    LOG_INFO("'ladmin': Adjustment of a final date of a banishment (account: %s, (%+d y %+d m %+d d %+d h %+d mn %+d s) -> new validity: %d (%s), ip: %s)\n",
             auth->userid,
             static_cast<sint16>(RFIFOW(fd, 26)), static_cast<sint16>(RFIFOW(fd, 28)),
             static_cast<sint16>(RFIFOW(fd, 30)), static_cast<sint16>(RFIFOW(fd, 32)),
             static_cast<sint16>(RFIFOW(fd, 34)), static_cast<sint16>(RFIFOW(fd, 36)),
             static_cast<sint32>(timestamp), tmpstr, ip_of(fd));
    WFIFOL(fd, 30) = timestamp;
    if (auth->ban_until_time == timestamp)
        return;
    auth->ban_until_time = timestamp;
    if (!timestamp)
        return;
    uint8 buf[11];
    WBUFW(buf, 0) = 0x2731;
    WBUFL(buf, 2) = uint32(auth->account_id);
    WBUFB(buf, 6) = 1; // 0: change of status, 1: ban
    WBUFL(buf, 7) = timestamp; // status or final date of a banishment
    charif_sendallwos(-1, buf, 11);
    for (sint32 j = 0; j < AUTH_FIFO_SIZE; j++)
        if (auth_fifo[j].account_id == auth->account_id)
            // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
            auth_fifo[j].login_id1++;
}

/// Broadcast a message
// uint16 packet, uint16 color, uint32 msglen, char msg[msglen]
// color is not with TMW client, but eA had yellow == 0, else blue
static void x794e(sint32 fd)
{
    WFIFOW(fd, 0) = 0x794f;
    WFIFOW(fd, 2) = -1;
    if (!RFIFOL(fd, 4))
    {
        LOG_INFO("'ladmin': Receiving a message for broadcast, but message is void (ip: %s)\n",
                 ip_of(fd));
        return;
    }
    // at least 1 char-server
    sint32 i;
    for (i = 0; i < MAX_SERVERS; i++)
        if (server[i].fd >= 0)
            break;
    if (i == MAX_SERVERS)
    {
        LOG_INFO("'ladmin': Receiving a message for broadcast, but no char-server is online (ip: %s)\n",
                 ip_of(fd));
        return;
    }
    char message[RFIFOL(fd, 4)];
    STRZCPY(message, sign_cast<const char *>(RFIFOP(fd, 8)));
    WFIFOW(fd, 2) = 0;
    // This should already be NUL, but we don't trust anyone
    message[RFIFOL(fd, 4) - 1] = '\0';
    remove_control_chars(message);
    LOG_INFO("'ladmin': Relay broadcast %s (ip: %s)\n",
             static_cast<const char *>(message), ip_of(fd));
    // forward the same message to all char-servers (no answer)
    session[fd]->rfifo_change_packet(0x2726);
    charif_sendallwos(-1, RFIFOP(fd, 0), 8 + RFIFOL(fd, 4));
}

static void ladmin_reply_account_info(sint32 fd, AuthData *auth)
{
    WFIFOL(fd, 2) = uint32(auth->account_id);
    WFIFOB(fd, 6) = uint8(is_gm(auth->account_id));
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 7)), auth->userid);
    WFIFOB(fd, 31) = static_cast<uint8>(auth->sex);
    WFIFOL(fd, 32) = auth->logincount;
    WFIFOL(fd, 36) = auth->state;
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 40)), auth->error_message);
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 60)), auth->lastlogin);
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 84)), auth->last_ip);
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 100)), auth->email);
    WFIFOL(fd, 140) = 0; //auth->connect_until_time;
    WFIFOL(fd, 144) = auth->ban_until_time;
    // discard the password magic
    char *memo = auth->memo + 1;
    WFIFOW(fd, 148) = strlen(memo);
    strzcpy(sign_cast<char *>(WFIFOP(fd, 150)), memo, sizeof(auth->memo)-1);
    WFIFOSET(fd, 150 + strlen(memo));
}
/// Account information by name
// uint16 packet, char userid[24]
static void x7952(sint32 fd)
{
    WFIFOW(fd, 0) = 0x7953;
    WFIFOL(fd, 2) = -1;
    char account_name[24];
    STRZCPY(account_name, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account_name);
    AuthData *auth = account_by_name(account_name);
    if (!auth)
    {
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 7)), account_name);
        WFIFOW(fd, 148) = 0;
        LOG_INFO("'ladmin': No account information for name: %s (ip: %s)\n",
                 account_name, ip_of(fd));
        WFIFOSET(fd, 150);
        return;
    }
    LOG_INFO("'ladmin': Sending information of an account (request by the name; account: %s, id: %d, ip: %s)\n",
             auth->userid, auth->account_id, ip_of(fd));
    ladmin_reply_account_info(fd, auth);
}

/// Account information by id
// uint16 packet, uint32 acc
static void x7954(sint32 fd)
{
    WFIFOW(fd, 0) = 0x7953;
    account_t acc = account_t(RFIFOL(fd, 2));
    WFIFOL(fd, 2) = uint32(acc);
    AuthData *auth = account_by_id(acc);
    if (!auth)
    {
        LOG_INFO("'ladmin': Attempt to obtain information (by the id) of an unknown account (id: %d, ip: %s)\n",
                 acc, ip_of(fd));
        memset(WFIFOP(fd, 7), 0, 24);
        WFIFOW(fd, 148) = 0;
        WFIFOSET(fd, 150);
        return;
    }
    LOG_INFO("'ladmin': Sending information of an account (request by the id; account: %s, id: %d, ip: %s)\n",
             auth->userid, acc, ip_of(fd));
    ladmin_reply_account_info(fd, auth);
}

/// Request to reload GM file (no answer)
// uint16 packet
static void x7955(sint32 fd)
{
    LOG_INFO("'ladmin': Request to re-load GM configuration file (ip: %s).\n",
             ip_of(fd));
    read_gm_account();
    // send GM accounts to all char-servers
    send_gm_accounts();
}


/// Parse packets from an administration login
static void parse_admin(sint32 fd)
{
    if (session[fd]->eof)
    {
        close(fd);
        delete_session(fd);
        LOG_INFO("Remote administration has disconnected (session #%d).\n", fd);
        return;
    }

    while (RFIFOREST(fd) >= 2)
    {
        if (display_parse_admin)
            LOG_INFO("%s: connection #%d, packet: 0x%x (with %d bytes).\n",
                     __func__, fd, RFIFOW(fd, 0), RFIFOREST(fd));

        switch (RFIFOW(fd, 0))
        {
            /// Request of the server version
            // uint16 packet
            case 0x7530:
                x7530(fd, true);
                RFIFOSKIP(fd, 2);
                break;

            /// Request of end of connection
            // uint16 packet
            case 0x7532:
                x7532(fd, "'ladmin': ");
                RFIFOSKIP(fd, 2);
                return;

            /// Request list of accounts
            // uint16 packet, uint32 start, uint32 end
            case 0x7920:
                if (RFIFOREST(fd) < 10)
                    return;
                x7920(fd);
                RFIFOSKIP(fd, 10);
                break;

            /// Itemfrob: change ID of an existing item
            // uint16 packet, uint32 old_id, uint32 new_id
            case 0x7924:
                if (RFIFOREST(fd) < 10)
                    return;
                x7924(fd);
                RFIFOSKIP(fd, 10);
                break;

            /// Request for account creation
            // uint16 packet, char userid[24], char passwd[24], char sex, char email[40]
            case 0x7930:
                if (RFIFOREST(fd) < 91)
                    return;
                x7930(fd);
                WFIFOSET(fd, 30);
                RFIFOSKIP(fd, 91);
                break;

            /// Request for an account deletion
            // uint16 packet, char userid[24]
            case 0x7932:
                if (RFIFOREST(fd) < 26)
                    return;
                x7932(fd);
                WFIFOSET(fd, 30);
                RFIFOSKIP(fd, 26);
                break;

            /// Request to change password
            // uint16 packet, char userid[24], char passwd[24]
            case 0x7934:
                if (RFIFOREST(fd) < 50)
                    return;
                x7934(fd);
                WFIFOSET(fd, 30);
                RFIFOSKIP(fd, 50);
                break;

            /// Modify a state
            // uint16 packet, char userid[24], uint32 state, char error_message[20]
            // error_message is usually the date of the end of banishment
            case 0x7936:
                if (RFIFOREST(fd) < 50)
                    return;
                x7936(fd);
                WFIFOSET(fd, 34);
                RFIFOSKIP(fd, 50);
                break;

            /// Request for servers list and # of online players
            // uint32 packet
            case 0x7938:
                x7938(fd);
                RFIFOSKIP(fd, 2);
                break;

            /// Request to check password
            // uint16 packet, char userid[24], char passwd[24]
            case 0x793a:
                if (RFIFOREST(fd) < 50)
                    return;
                x793a(fd);
                WFIFOSET(fd, 30);
                RFIFOSKIP(fd, 50);
                break;

            /// Request to modify sex
            // uint32 packet, char userid[24], char sex
            case 0x793c:
                if (RFIFOREST(fd) < 27)
                    return;
                x793c(fd);
                WFIFOSET(fd, 30);
                RFIFOSKIP(fd, 27);
                break;

            /// Request to modify GM level
            // uint16 packet, char userid[24], uint8 new_gm_level
            case 0x793e:
                if (RFIFOREST(fd) < 27)
                    return;
                x793e(fd);
                WFIFOSET(fd, 30);
                RFIFOSKIP(fd, 27);
                break;

            /// Request to modify e-mail
            // uint16 packet, char userid[24], char email[40]
            case 0x7940:
                if (RFIFOREST(fd) < 66)
                    return;
                x7940(fd);
                WFIFOSET(fd, 30);
                RFIFOSKIP(fd, 66);
                break;

            /// Request to modify memo field
            // uint16 packet, char usercount[24], uint16 msglen, char msg[msglen]
            case 0x7942:
                if (RFIFOREST(fd) < 28
                    || RFIFOREST(fd) < (28 + RFIFOW(fd, 26)))
                    return;
                x7942(fd);
                WFIFOSET(fd, 30);
                RFIFOSKIP(fd, 28 + RFIFOW(fd, 26));
                break;

            /// Find account id from name
            // uint16 packet, char userid[24]
            case 0x7944:
                if (RFIFOREST(fd) < 26)
                    return;
                x7944(fd);
                WFIFOSET(fd, 30);
                RFIFOSKIP(fd, 26);
                break;

            /// Find an account name from id
            // uint16 packet, uint32 acc
            case 0x7946:
                if (RFIFOREST(fd) < 6)
                    return;
                x7946(fd);
                WFIFOSET(fd, 30);
                RFIFOSKIP(fd, 6);
                break;

            /// Set the banishment timestamp
            // uint16 packet, char userid[24], uint32 timestamp
            case 0x794a:
                if (RFIFOREST(fd) < 30)
                    return;
                x794a(fd);
                WFIFOSET(fd, 34);
                RFIFOSKIP(fd, 30);
                break;

            /// Adjust the banishment end timestamp
            // uint16 packet, char userid[24], sint16 year,mon,day,hr,min,sec
            case 0x794c:
                if (RFIFOREST(fd) < 38)
                    return;
                x794c(fd);
                WFIFOSET(fd, 34);
                RFIFOSKIP(fd, 38);
                break;

            /// Broadcast a message
            // uint16 packet, uint16 color, uint32 msglen, char msg[msglen]
            // color is not with TMW client, but eA had yellow == 0, else blue
            case 0x794e:
                if (RFIFOREST(fd) < 8 || RFIFOREST(fd) < (8 + RFIFOL(fd, 4)))
                    return;
                x794e(fd);
                WFIFOSET(fd, 4);
                RFIFOSKIP(fd, 8 + RFIFOL(fd, 4));
                break;

            /// Account information by name
            // uint16 packet, char userid[24]
            case 0x7952:
                if (RFIFOREST(fd) < 26)
                    return;
                x7952(fd);
                RFIFOSKIP(fd, 26);
                break;

            /// Account information by id
            // uint16 packet, uint32 acc
            case 0x7954:
                if (RFIFOREST(fd) < 6)
                    return;
                x7954(fd);
                RFIFOSKIP(fd, 6);
                break;

            /// Request to reload GM file (no answer)
            // uint16 packet
            case 0x7955:
                x7955(fd);
                RFIFOSKIP(fd, 2);
                break;

            default:
                unknown_packet_log.warn("%s: End of connection: unknown packet %hx (ip: %s)\n",
                                        __func__, RFIFOW(fd, 0), ip_of(fd));
                unknown_packet_log.debug("%s: connection #%d (ip: %s), packet: 0x%x (with %u bytes available).\n",
                                         __func__, fd, ip_of(fd), RFIFOW(fd, 0), RFIFOREST(fd));
                hexdump(unknown_packet_log, RFIFOP(fd, 0), RFIFOREST(fd));
                unknown_packet_log.debug("\n");
                session[fd]->eof = 1;
                return;
        } // switch packet
    } // while packet available
    return;
}




/// Check if IP is LAN instead of WAN
// (send alternate char IP)
static bool lan_ip_check(IP_Address addr)
{
    bool lancheck = lan_mask.covers(addr);
    printf("LAN test (result): %s source\033[m.\n",
            lancheck ? "\033[1;36mLAN" : "\033[1;32mWAN");
    return lancheck;
}




/// Client is alive
// uint16 packet, char userid[24]
// this packet is not sent by any known TMW server/client
static void x200(sint32)
{
}

/// Client is alive
// uint16 packet, char crypted_userid[16]
// (new ragexe from 22 june 2004)
// this packet is not sent by any known TMW server/client
static void x204(sint32)
{
}

/// Client connect
// uint16 packet, uint8 unk[4], char userid[24], char passwd[24], uint8 version_2_flags
static void x64(sint32 fd)
{
    struct mmo_account account;
    STRZCPY(account.userid, sign_cast<const char *>(RFIFOP(fd, 6)));
    remove_control_chars(account.userid);
    STRZCPY(account.passwd, sign_cast<const char *>(RFIFOP(fd, 30)));
    remove_control_chars(account.passwd);

    LOG_INFO("Request for connection of %s (ip: %s).\n",
             account.userid, ip_of(fd));

    if (!check_ip(session[fd]->client_addr))
    {
        LOG_INFO("Connection refused: IP isn't authorised (deny/allow, ip: %s).\n",
                 ip_of(fd));
        WFIFOW(fd, 0) = 0x6a;
        WFIFOB(fd, 2) = 0x03;
        WFIFOSET(fd, 3);
        // FIXME: shouldn't this set eof?
        return;
    }

    enum auth_failure result = mmo_auth(&account, fd);
    // putting the version_2_flags here feels hackish
    // but it makes the code much nicer, especially for future
    Version2 version_2_flags = static_cast<Version2>(RFIFOB(fd, 54));
    // As an update_host is required for all known TMW servers,
    // and all clients since 0.0.25 support it,
    // I am making it fail if not set
    if (!(version_2_flags & Version2::UPDATEHOST))
        result = AUTH_CLIENT_TOO_OLD;
    if (result != AUTH_OK)
    {
        WFIFOW(fd, 0) = 0x6a;
        WFIFOB(fd, 2) = result - 1;
        memset(WFIFOP(fd, 3), '\0', 20);
        AuthData *auth;
        if (result != AUTH_BANNED_TEMPORARILY)
            goto end_x0064_006a;
        // You are Prohibited to log in until %s
        auth = account_by_name(account.userid);
        // This cannot happen
        // if (i == -1)
            // goto end_x0064_006a;
        if (auth->ban_until_time)
        {
            // if account is banned, we send ban timestamp
            strzcpy(sign_cast<char *>(WFIFOP(fd, 3)), stamp_time(auth->ban_until_time, NULL), 20);
        }
        else
        {
            // can this happen?
            // we send error message
            // hm, it seems there is a ladmin command to set this arbitrarily
            STRZCPY2(sign_cast<char *>(WFIFOP(fd, 3)), auth->error_message);
        }
    end_x0064_006a:
        WFIFOSET(fd, 23);
        return;
    }
    gm_level_t gm_level = is_gm(account.account_id);
    if (min_level_to_connect > gm_level)
    {
        LOG_INFO("Connection refused: only allowing GMs of level %hhu (account: %s, GM level: %d, ip: %s).\n",
                 min_level_to_connect, account.userid, gm_level, ip_of(fd));
        WFIFOW(fd, 0) = 0x81;
        WFIFOL(fd, 2) = 1; // 01 = Server closed
        WFIFOSET(fd, 3);
        return;
    }

    if (gm_level)
        LOG_INFO("Connection of the GM (level:%d) account '%s' accepted.\n",
                 gm_level, account.userid);
    else
        LOG_INFO("Connection of the account '%s' accepted.\n",
                 account.userid);

    /// If there is an update_host, send it
    // (version_2_flags requires bit 0, above)
    size_t host_len = strlen(update_host);
    if (host_len)
    {
        WFIFOW(fd, 0) = 0x63;
        WFIFOW(fd, 2) = 4 + host_len;
        memcpy(WFIFOP(fd, 4), update_host, host_len);
        WFIFOSET(fd, 4 + host_len);
    }

    // Load list of char servers into outbound packet
    sint32 server_num = 0;
    // This has been set since 0.0.29
    // and it will only be an inconvenience for older clients,
    // so always send in forward-order.
    // if (version_2_flags & VERSION_2_SERVERORDER)
    for (sint32 i = 0; i < MAX_SERVERS; i++)
    {
        if (server[i].fd < 0)
            continue;
        if (lan_ip_check((session[fd]->client_addr)))
            WFIFOL(fd, 47 + server_num * 32) = lan_char_ip.to_n();
        else
            WFIFOL(fd, 47 + server_num * 32) = server[i].ip.to_n();
        WFIFOW(fd, 47 + server_num * 32 + 4) = server[i].port;
        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 47 + server_num * 32 + 6)), server[i].name);
        WFIFOW(fd, 47 + server_num * 32 + 26) = server[i].users;
        WFIFOW(fd, 47 + server_num * 32 + 28) = server[i].maintenance;
        WFIFOW(fd, 47 + server_num * 32 + 30) = server[i].is_new;
        server_num++;
    }
    // if no char-server, don't send void list of servers, just disconnect the player with proper message
    if (!server_num)
    {
        LOG_INFO("Connection refused: there is no char-server online (account: %s, ip: %s).\n",
                 account.userid, ip_of(fd));
        WFIFOW(fd, 0) = 0x81;
        WFIFOL(fd, 2) = 1; // 01 = Server closed
        WFIFOSET(fd, 3);
        return;
    }
    // if at least 1 char-server
    WFIFOW(fd, 0) = 0x69;
    WFIFOW(fd, 2) = 47 + 32 * server_num;
    WFIFOL(fd, 4) = account.login_id1;
    WFIFOL(fd, 8) = uint32(account.account_id);
    WFIFOL(fd, 12) = account.login_id2;
    /// in old eAthena, this was for an ip
    WFIFOL(fd, 16) = 0;
    /// in old eAthena, this was for a name
    STRZCPY2(sign_cast<char *>(WFIFOP(fd, 20)), account.lastlogin);
    // nothing is written in the word at 44
    WFIFOB(fd, 46) = static_cast<uint8>(account.sex);
    WFIFOSET(fd, 47 + 32 * server_num);
    if (auth_fifo_pos >= AUTH_FIFO_SIZE)
        auth_fifo_pos = 0;
    // wait, is this just blithely wrapping and invalidating old entries?
    // this could be a cause of DoS attacks
    auth_fifo[auth_fifo_pos].account_id = account.account_id;
    auth_fifo[auth_fifo_pos].login_id1 = account.login_id1;
    auth_fifo[auth_fifo_pos].login_id2 = account.login_id2;
    auth_fifo[auth_fifo_pos].sex = account.sex;
    auth_fifo[auth_fifo_pos].delflag = 0;
    auth_fifo[auth_fifo_pos].ip = session[fd]->client_addr;
    auth_fifo_pos++;
}

/// Sending request of the coding key (ladmin packet)
// uint16 packet
static void x791a(sint32 fd)
{
    if (session[fd]->session_data)
    {
        LOG_INFO("login: abnormal request of MD5 key (already opened session).\n");
        session[fd]->eof = 1;
        return;
    }
    LoginSessionData *ld = new LoginSessionData;
    session[fd]->session_data = ld;
    LOG_INFO("'ladmin': Sending request of the coding key (ip: %s)\n",
             ip_of(fd));

    WFIFOW(fd, 0) = 0x01dc;
    WFIFOW(fd, 2) = 4 + ld->md5keylen;
    memcpy(WFIFOP(fd, 4), ld->md5key, ld->md5keylen);
    WFIFOSET(fd, WFIFOW(fd, 2));
}

/// char-server connects
// uint16 packet, char userid[24], char passwd[24], char unk[4],
//   uint32 ip, uint16 port, char server_name[20],
//   char unk[2], uint16 maintenance, uint16 is_new
static void x2710(sint32 fd)
{
    struct mmo_account account;
    STRZCPY(account.userid, sign_cast<const char *>(RFIFOP(fd, 2)));
    remove_control_chars(account.userid);
    STRZCPY(account.passwd, sign_cast<const char *>(RFIFOP(fd, 26)));
    remove_control_chars(account.passwd);
    char server_name[20];
    STRZCPY(server_name, sign_cast<const char *>(RFIFOP(fd, 60)));
    remove_control_chars(server_name);
    LOG_INFO("Connection request of the char-server '%s' @ %d.%d.%d.%d:%d (ip: %s)\n",
             server_name, RFIFOB(fd, 54), RFIFOB(fd, 55),
             RFIFOB(fd, 56), RFIFOB(fd, 57), RFIFOW(fd, 58), ip_of(fd));
    enum auth_failure result = mmo_auth(&account, fd);

    if (result == AUTH_OK && account.sex == SEX::SERVER)
    {
        // If this is the main server, and we don't already have a main server
        if (server[0].fd == -1
            && strcasecmp(server_name, main_server) == 0)
        {
            account.account_id = account_t();
            goto char_server_ok;
        }
        for (sint32 i = 1; i < MAX_SERVERS; i++)
        {
            if (server[i].fd == -1)
            {
                account.account_id = account_t(i);
                goto char_server_ok;
            }
        }
    }
    LOG_INFO("Connection of the char-server '%s' REFUSED (account: %s, pass: %s, ip: %s)\n",
             server_name, account.userid, account.passwd, ip_of(fd));
    WFIFOW(fd, 0) = 0x2711;
    WFIFOB(fd, 2) = 3;
    WFIFOSET(fd, 3);
    return;

char_server_ok:
    LOG_INFO("Connection of the char-server '%s' accepted (account: %s, pass: %s, ip: %s)\n",
             server_name, account.userid, account.passwd, ip_of(fd));
    server[uint32(account.account_id)].ip.from_n(RFIFOL(fd, 54));
    server[uint32(account.account_id)].port = RFIFOW(fd, 58);
    STRZCPY(server[uint32(account.account_id)].name, server_name);
    server[uint32(account.account_id)].users = 0;
    server[uint32(account.account_id)].maintenance = RFIFOW(fd, 82);
    server[uint32(account.account_id)].is_new = RFIFOW(fd, 84);
    server[uint32(account.account_id)].fd = fd;
    if (anti_freeze_enable)
        // Char-server anti-freeze system. Counter. 5 ok, 4...0 freezed
        server[uint32(account.account_id)].freezeflag = 5;
    WFIFOW(fd, 0) = 0x2711;
    WFIFOB(fd, 2) = 0;
    WFIFOSET(fd, 3);
    session[fd]->func_parse = parse_fromchar;
    realloc_fifo(fd, FIFOSIZE_SERVERLINK, FIFOSIZE_SERVERLINK);
    // send GM account list to char-server
    uint16 len = 4;
    WFIFOW(fd, 0) = 0x2732;
    for (const AuthData& ad : auth_dat)
    {
        gm_level_t gm_value = is_gm(ad.account_id);
        // send only existing accounts. We can not create a GM account when server is online.
        if (gm_value)
        {
            WFIFOL(fd, len) = uint32(ad.account_id);
            WFIFOB(fd, len + 4) = uint8(gm_value);
            len += 5;
        }
    }
    WFIFOW(fd, 2) = len;
    WFIFOSET(fd, len);
}

/// Request for administation login
// uint16 packet, uint16 type = {0,1,2}, char passwd[24] (if type 0) or uint8 hash[16] otherwise
// ladmin always sends the encrypted form
static void x7918(sint32 fd)
{
    WFIFOW(fd, 0) = 0x7919;
    WFIFOB(fd, 2) = 1;
    if (!check_ladminip(session[fd]->client_addr))
    {
        LOG_INFO("'ladmin'-login: Connection in administration mode refused: IP isn't authorised (ladmin_allow, ip: %s).\n",
                 ip_of(fd));
        return;
    }
    LoginSessionData *ld = static_cast<LoginSessionData *>(session[fd]->session_data);
    if (RFIFOW(fd, 2) == 0)
    {
        LOG_INFO("'ladmin'-login: Connection in administration mode refused: not encrypted (ip: %s).\n",
                 ip_of(fd));
        return;
    }
    if (RFIFOW(fd, 2) > 2)
    {
        LOG_INFO("'ladmin'-login: Connection in administration mode refused: unknown encryption (ip: %s).\n",
                 ip_of(fd));
        return;
    }
    if (!ld)
    {
        LOG_INFO("'ladmin'-login: error! MD5 key not created/requested for an administration login.\n");
        return;
    }
    if (!admin_state)
    {
        LOG_INFO("'ladmin'-login: Connection in administration mode refused: remote administration is disabled (ip: %s)\n",
                 ip_of(fd));
        return;
    }
    char md5str[64] = {};
    uint8 md5bin[32];
    if (RFIFOW(fd, 2) == 1)
    {
        strncpy(md5str, ld->md5key, sizeof(ld->md5key));  // 20
        strcat(md5str, admin_pass);    // 24
    }
    else if (RFIFOW(fd, 2) == 2)
    {
        // This is always sent by ladmin
        strncpy(md5str, admin_pass, sizeof(admin_pass));  // 24
        strcat(md5str, ld->md5key);    // 20
    }
    MD5_to_bin(MD5_from_cstring(md5str), md5bin);
    // If password hash sent by client matches hash of password read from login server configuration file
    if (memcmp(md5bin, RFIFOP(fd, 4), 16) == 0)
    {
        LOG_INFO("'ladmin'-login: Connection in administration mode accepted (encrypted password, ip: %s)\n",
                 ip_of(fd));
        WFIFOB(fd, 2) = 0;
        session[fd]->func_parse = parse_admin;
    }
    else
        LOG_INFO("'ladmin'-login: Connection in administration mode REFUSED - invalid password (encrypted password, ip: %s)\n",
                 ip_of(fd));
}



/// Default packet parsing
// * normal players
// * administation/char-server before authenticated
static void parse_login(sint32 fd)
{
    if (session[fd]->eof)
    {
        close(fd);
        delete_session(fd);
        return;
    }

    while (RFIFOREST(fd) >= 2)
    {
        if (display_parse_login)
        {
#if 0
// This information is useless - better available below
// and is not safe (it might not be NUL-terminated)
            if (RFIFOW(fd, 0) == 0x64)
            {
                if (RFIFOREST(fd) >= 55)
                    printf("parse_login: connection #%d, packet: 0x%x (with being read: %d), account: %s.\n",
                            fd, RFIFOW(fd, 0), RFIFOREST(fd), RFIFOP(fd, 6));
            }
            else if (RFIFOW(fd, 0) == 0x2710)
            {
                if (RFIFOREST(fd) >= 86)
                    printf("parse_login: connection #%d, packet: 0x%x (with being read: %d), server: %s.\n",
                            fd, RFIFOW(fd, 0), RFIFOREST(fd), RFIFOP(fd, 60));
            }
            else
#endif
                LOG_INFO("%s: connection #%d, packet: 0x%hx (with %u bytes).\n",
                         __func__, fd, RFIFOW(fd, 0), RFIFOREST(fd));
        }

        switch (RFIFOW(fd, 0))
        {
            /// Client is alive
            // uint16 packet, char userid[24]
            // this packet is not sent by any known TMW server/client
            case 0x200:
                if (RFIFOREST(fd) < 26)
                    return;
                x200(fd);
                RFIFOSKIP(fd, 26);
                break;

            /// Client is alive
            // uint16 packet, char crypted_userid[16]
            // (new ragexe from 22 june 2004)
            // this packet is not sent by any known TMW server/client
            case 0x204:
                if (RFIFOREST(fd) < 18)
                    return;
                x204(fd);
                RFIFOSKIP(fd, 18);
                break;

            /// Client connect
            // uint16 packet, uint8 unk[4], char userid[24], char passwd[24], uint8 version_2_flags
            case 0x64:
                if (RFIFOREST(fd) < 55 )
                    return;
                x64(fd);
                RFIFOSKIP(fd, 55);
                break;

            /// Sending request of the coding key (ladmin packet)
            // uint16 packet
            case 0x791a:
                x791a(fd);
                RFIFOSKIP(fd, 2);
                break;

            /// char-server connects
            // uint16 packet, char userid[24], char passwd[24], char unk[4],
            //   uint32 ip, uint16 port, char server_name[20],
            //   char unk[2], uint16 maintenance, uint16 is_new
            case 0x2710:
                if (RFIFOREST(fd) < 86)
                    return;
                x2710(fd);
                RFIFOSKIP(fd, 86);
                return;

            /// Server version
            // uint16 packet
            case 0x7530:
                x7530(fd, false);
                RFIFOSKIP(fd, 2);
                break;

            /// End connection
            case 0x7532:
                x7532(fd, "");
                return;

            /// Request for administation login
            // uint16 packet, uint16 type = {0,1,2}, char passwd[24] (if type 0) or uint8 hash[16] otherwise
            // ladmin always sends the encrypted form
            case 0x7918:
                if (RFIFOREST(fd) < 4
                    || RFIFOREST(fd) < ((RFIFOW(fd, 2) == 0) ? 28 : 20))
                    return;
                x7918(fd);
                WFIFOSET(fd, 3);
                RFIFOSKIP(fd, (RFIFOW(fd, 2) == 0) ? 28 : 20);
                break;

            default:
                unknown_packet_log.info("unknown packet: disconnect (ip: %s)\n",
                                        ip_of(fd));
                if (!save_unknown_packets)
                    goto end_default;
                unknown_packet_log.debug("%s: connection #%d (ip: %s), packet: 0x%x (with being read: %d).\n",
                                         __func__, fd, ip_of(fd), RFIFOW(fd, 0), RFIFOREST(fd));

                hexdump(unknown_packet_log, RFIFOP(fd, 0), RFIFOREST(fd));
                unknown_packet_log.debug("\n");
            end_default:
                session[fd]->eof = 1;
                return;
        }
    }
    return;
}

/// read conf/lan_support.conf
// Note: this file is shared with the char-server
// This file is to give a different IP for connections from the LAN
// Note: it assumes that all char-servers have the same IP, just different ports
// if this isn't how it it set up, you'll have to do some port-forwarding
static void login_lan_config_read(const char *lancfgName)
{
    // set default configuration
    lan_char_ip.from_string("127.0.0.1");
    lan_mask.from_string("127.0.0.1/32");

    FILE *fp = fopen_(lancfgName, "r");
    if (!fp)
    {
        printf("***WARNING: LAN Support configuration file is not found: %s\n",
                lancfgName);
        return;
    }

    printf("---Start reading Lan Support configuration file\n");

    char line[1024];
    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;

        char w1[1024], w2[1024];
        if (sscanf(line, "%[^:]: %[^\r\n]", w1, w2) != 2)
            continue;

        remove_control_chars(w1);
        remove_control_chars(w2);
        // WARNING: I don't think this should be calling gethostbyname at all, it should just parse the IP
        if (strcasecmp(w1, "lan_char_ip") == 0)
        {
            lan_char_ip.from_string(w2);
            printf("LAN IP of char-server: %s.\n", lan_char_ip.to_string().c_str());
        }
        else if (strcasecmp(w1, "subnet") == 0)
        {
            lan_mask.addr.from_string(w2);
            printf("LAN IP range: %s\n",
                   lan_mask.addr.to_string().c_str());
        }
        else if (strcasecmp(w1, "subnetmask") == 0)
        {
            lan_mask.mask.from_string(w2);
            printf("Subnet mask to send LAN char-server IP: %s.\n",
                   lan_mask.mask.to_string().c_str());
        }
    }
    fclose_(fp);

    // log the LAN configuration
    LOG_CONF("The LAN configuration of the server is set:\n");
    LOG_CONF("- with LAN IP of char-server: %s.\n",
             lan_char_ip.to_string().c_str());
    LOG_CONF("- with the sub-network of the char-server: %s.\n",
             lan_mask.to_string().c_str());

    printf("LAN test of LAN IP of the char-server: ");
    if (!lan_ip_check(lan_char_ip))
    {
        /// Actually, this could be considered a legitimate setting
        LOG_WARN("***ERROR: LAN IP of the char-server doesn't belong to the specified Sub-network.\n");
    }

    LOG_CONF("---End reading of Lan Support configuration file\n");
}

/// Read general configuration file
static void login_config_read(const char *cfgName)
{
    char line[1024], w1[1024], w2[1024];
    FILE *fp = fopen_(cfgName, "r");
    if (!fp)
    {
        LOG_WARN("Configuration file (%s) not found.\n", cfgName);
        return;
    }

    LOG_CONF("---Start reading of Login Server configuration file (%s)\n",
             cfgName);
    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;
        if (sscanf(line, "%[^:]: %[^\r\n]", w1, w2) != 2)
            continue;

        remove_control_chars(w1);
        remove_control_chars(w2);

        if (strcasecmp(w1, "admin_state") == 0)
        {
            admin_state = config_switch(w2);
            continue;
        }
        if (strcasecmp(w1, "admin_pass") == 0)
        {
            STRZCPY(admin_pass, w2);
            continue;
        }
        if (strcasecmp(w1, "ladminallowip") == 0)
        {
            if (strcasecmp(w2, "clear") == 0)
            {
                access_ladmin_allow.empty();
                continue;
            }
            if (strcasecmp(w2, "all") == 0)
            {
                access_ladmin_allow = { IP_Mask() };
                continue;
            }
            if (!w2[0])
                continue;
            // don't add IP if already 'all'
            if ((access_ladmin_allow.size() == 1 && access_ladmin_allow[0].covers_all()))
                continue;

            access_ladmin_allow.push_back(IP_Mask(w2));
            continue;
        }
        if (strcasecmp(w1, "gm_pass") == 0)
        {
            STRZCPY(gm_pass, w2);
            continue;
        }
        if (strcasecmp(w1, "level_new_gm") == 0)
        {
            level_new_gm = gm_level_t(atoi(w2));
            continue;
        }
        if (strcasecmp(w1, "new_account") == 0)
        {
            new_account_flag = config_switch(w2);
            continue;
        }
        if (strcasecmp(w1, "login_port") == 0)
        {
            login_port = atoi(w2);
            continue;
        }
        if (strcasecmp(w1, "gm_account_filename_check_timer") == 0)
        {
            gm_account_filename_check_timer = std::chrono::seconds(atoi(w2));
            continue;
        }
        if (strcasecmp(w1, "save_unknown_packets") == 0)
        {
            save_unknown_packets = config_switch(w2);
            continue;
        }
        if (strcasecmp(w1, "display_parse_login") == 0)
        {
            display_parse_login = config_switch(w2);
            continue;
        }
        if (strcasecmp(w1, "display_parse_admin") == 0)
        {
            display_parse_admin = config_switch(w2);
            continue;
        }
        if (strcasecmp(w1, "display_parse_fromchar") == 0)
        {
            // 0: no, 1: yes (without packet 0x2714), 2: all packets
            switch (config_switch(w2))
            {
            default: display_parse_fromchar = CP_NONE;
            case 1: display_parse_fromchar = CP_MOST;
            case 2: display_parse_fromchar = CP_ALL;
            }
            continue;
        }
        if (strcasecmp(w1, "min_level_to_connect") == 0)
        {
            min_level_to_connect = gm_level_t(atoi(w2));
            continue;
        }
        if (strcasecmp(w1, "add_to_unlimited_account") == 0)
        {
            add_to_unlimited_account = config_switch(w2);
            continue;
        }
        if (strcasecmp(w1, "order") == 0)
        {
            errno = 0;
            sint32 i = strtol(w2, NULL, 0);
            if (errno)
                i = -1;
            if (i == 0 || strcasecmp(w2, "deny,allow") == 0 ||
                strcasecmp(w2, "deny, allow") == 0)
                access_order = ACO::DENY_ALLOW;
            else if (i == 1 || strcasecmp(w2, "allow,deny") == 0 ||
                strcasecmp(w2, "allow, deny") == 0)
                access_order = ACO::ALLOW_DENY;
            else if (i == 2 || strcasecmp(w2, "mutual-failure") == 0)
                access_order = ACO::MUTUAL_FAILURE;
            else
                printf("***WARNING: unknown access order: %s\n", w2);
            continue;
        }
        if (strcasecmp(w1, "allow") == 0)
        {
            if (strcasecmp(w2, "clear") == 0)
            {
                access_allow.empty();
                continue;
            }
            if (strcasecmp(w2, "all") == 0)
            {
                access_allow = { IP_Mask() };
                continue;
            }
            if (!w2[0])
                continue;
            // don't add IP if already 'all'
            if ((access_allow.size() == 1 && access_allow[0].covers_all()))
                continue;
            access_allow.push_back(IP_Mask(w2));
            continue;
        }
        if (strcasecmp(w1, "deny") == 0)
        {
            if (strcasecmp(w2, "clear") == 0)
            {
                access_deny.empty();
                continue;
            }
            if (strcasecmp(w2, "all") == 0)
            {
                access_deny = { IP_Mask() };
                continue;
            }
            if (!w2[0])
                continue;
            if ((access_deny.size() == 1 && access_deny[0].covers_all()))
                continue;
            access_deny.push_back(IP_Mask(w2));
            continue;
        }
        if (strcasecmp(w1, "anti_freeze_enable") == 0)
        {
            anti_freeze_enable = config_switch(w2);
            continue;
        }
        if (strcasecmp(w1, "anti_freeze_interval") == 0)
        {
            ANTI_FREEZE_INTERVAL = std::chrono::seconds(atoi(w2));
            if (ANTI_FREEZE_INTERVAL < std::chrono::seconds(5))
                // minimum 5 seconds
                ANTI_FREEZE_INTERVAL = std::chrono::seconds(5);
            continue;
        }
        if (strcasecmp(w1, "import") == 0)
        {
            login_config_read(w2);
            continue;
        }
        if (strcasecmp(w1, "update_host") == 0)
        {
            STRZCPY(update_host, w2);
            continue;
        }
        if (strcasecmp(w1, "main_server") == 0)
        {
            STRZCPY(main_server, w2);
            continue;
        }
        printf("%s: unknown option: %s\n", __func__, line);
    }
    fclose_(fp);

    printf("---End reading of Login Server configuration file.\n");
}

/// Displaying of configuration warnings
// this is not done while parsing because the log filename might change
// TODO merge it anyways, since this isn't logged :/
static void display_conf_warnings(void)
{
    if (admin_state)
    {
        if (admin_pass[0] == '\0')
        {
            printf("***WARNING: Administrator password is void (admin_pass).\n");
        }
        else if (strcmp(admin_pass, "admin") == 0)
        {
            printf("***WARNING: You are using the default administrator password (admin_pass).\n");
            printf("            We highly recommend that you change it.\n");
        }
    }

    if (gm_pass[0] == '\0')
    {
        printf("***WARNING: 'To GM become' password is void (gm_pass).\n");
        printf("            We highly recommend that you set one password.\n");
    }
    else if (strcmp(gm_pass, "gm") == 0)
    {
        printf("***WARNING: You are using the default GM password (gm_pass).\n");
        printf("            We highly recommend that you change it.\n");
    }

    if (gm_account_filename_check_timer <= std::chrono::seconds(1))
    {
        printf("***WARNING: Invalid for gm_account_filename_check_timer parameter.\n");
        printf("            -> set to 2 sec (minimum).\n");
        gm_account_filename_check_timer = std::chrono::seconds(2);
    }

    if (access_order == ACO::DENY_ALLOW)
    {
        if (access_deny.size() == 1 && access_deny[0].covers_all())
        {
            printf("***WARNING: The IP security order is 'deny,allow' (allow if not denied).\n");
            printf("            But you denied ALL IP!\n");
        }
    }
    if (access_order == ACO::ALLOW_DENY)
    {
        if (access_allow.empty() && !access_deny.empty())
        {
            printf("***WARNING: The IP security order is 'allow,deny' (deny if not allowed).\n");
            printf("            But you never allowed any IP!\n");
        }
    }
    else
    {
        // ACO::MUTUAL_FAILURE
        if (access_allow.empty() && !access_deny.empty())
        {
            printf("***WARNING: The IP security order is 'mutual-failure'\n");
            printf("            (allow if in the allow list and not in the deny list).\n");
            printf("            But you never allowed any IP!\n");
        }
        if (access_deny.size() == 1 && access_deny[0].covers_all())
        {
            printf("***WARNING: The IP security order is mutual-failure\n");
            printf("            (allow if in the allow list and not in the deny list).\n");
            printf("            But, you denied ALL IP!\n");
        }
    }
}

//-------------------------------
// Save configuration in log file
//-------------------------------
static void save_config_in_log(void)
{
    LOG_DEBUG("\nThe login-server starting...\n");

    // save configuration in log file
    LOG_CONF("The configuration of the server is set:\n");

    if (!admin_state)
        LOG_CONF("- with no remote administration.\n");
    else if (admin_pass[0] == '\0')
        LOG_CONF("- with a remote administration with a VOID password.\n");
    else if (strcmp(admin_pass, "admin") == 0)
        LOG_CONF("- with a remote administration with the DEFAULT password.\n");
    else
        LOG_CONF("- with a remote administration with the password of %d character(s).\n",
                   strlen(admin_pass));
    if (access_ladmin_allow.empty() || (access_ladmin_allow.size() == 1 && access_ladmin_allow[0].covers_all()))
    {
        LOG_CONF("- to accept any IP for remote administration\n");
    }
    else
    {
        LOG_CONF("- to accept following IP for remote administration:\n");
        for (IP_Mask mask : access_ladmin_allow)
            LOG_CONF("  %s\n", mask.to_string().c_str());
    }

    if (gm_pass[0] == '\0')
        LOG_CONF("- with a VOID 'To GM become' password (gm_pass).\n");
    else if (strcmp(gm_pass, "gm") == 0)
        LOG_CONF("- with the DEFAULT 'To GM become' password (gm_pass).\n");
    else
        LOG_CONF("- with a 'To GM become' password (gm_pass) of %d character(s).\n",
                 strlen(gm_pass));
    if (!level_new_gm)
        LOG_CONF("- to refuse any creation of GM with @gm.\n");
    else
        LOG_CONF("- to create GM with level '%hhu' when @gm is used.\n",
                 level_new_gm);

    if (new_account_flag)
        LOG_CONF("- to ALLOW new users (with _F/_M).\n");
    else
        LOG_CONF("- to NOT ALLOW new users (with _F/_M).\n");
    LOG_CONF("- with port: %d.\n", login_port);
    LOG_CONF("- with the accounts file name: '%s'.\n",
             account_filename);
    LOG_CONF("- with the GM accounts file name: '%s'.\n",
             gm_account_filename);
    if (gm_account_filename_check_timer == std::chrono::seconds::zero())
        LOG_CONF("- to NOT check GM accounts file modifications.\n");
    else
        LOG_CONF("- to check GM accounts file modifications every %lld seconds.\n",
                 std::chrono::duration_cast<std::chrono::seconds>(gm_account_filename_check_timer).count());

    // not necessary to log the 'login_log_filename', we are inside :)

    LOG_CONF("- with the unknown packets file name: '%s'.\n",
             login_log_unknown_packets_filename);
    if (save_unknown_packets)
        LOG_CONF("- to SAVE all unknown packets.\n");
    else
        LOG_CONF("- to SAVE only unknown packets sending by a char-server or a remote administration.\n");
    if (display_parse_login)
        LOG_CONF("- to display normal parse packets on console.\n");
    else
        LOG_CONF("- to NOT display normal parse packets on console.\n");
    if (display_parse_admin)
        LOG_CONF("- to display administration parse packets on console.\n");
    else
        LOG_CONF("- to NOT display administration parse packets on console.\n");
    if (display_parse_fromchar)
        LOG_CONF("- to display char-server parse packets on console.\n");
    else
        LOG_CONF("- to NOT display char-server parse packets on console.\n");

    if (!min_level_to_connect)
        LOG_CONF("- with no minimum level for connection.\n");
    else
        LOG_CONF("- to accept only GM with level %d or more.\n",
                       min_level_to_connect);

    if (add_to_unlimited_account)
        LOG_CONF("- to authorize adjustment (with timeadd ladmin) on an unlimited account.\n");
    else
        LOG_CONF("- to refuse adjustment (with timeadd ladmin) on an unlimited account. You must use timeset (ladmin command) before.\n");

    LOG_CONF("- with control of players IP between login-server and char-server.\n");

    if (access_order == ACO::DENY_ALLOW)
    {
        if (access_deny.empty())
        {
            LOG_WARN("- with the IP security order: 'deny,allow' (allow if not deny). You refuse no IP.\n");
        }
        else if (access_deny.size() == 1 && access_deny[0].covers_all())
        {
            LOG_WARN("- with the IP security order: 'deny,allow' (allow if not deny). You refuse ALL IP.\n");
        }
        else
        {
            LOG_CONF("- with the IP security order: 'deny,allow' (allow if not deny). Refused IP are:\n");
            for (IP_Mask mask : access_deny)
                LOG_CONF("  %s\n", mask.to_string().c_str());
        }
    }
    else if (access_order == ACO::ALLOW_DENY)
    {
        if (access_allow.empty())
        {
            LOG_WARN("- with the IP security order: 'allow,deny' (deny if not allow). But, NO IP IS AUTHORISED!\n");
        }
        else if (access_allow.size() == 1 && access_allow[0].covers_all())
        {
            LOG_CONF("- with the IP security order: 'allow,deny' (deny if not allow). You authorise ALL IP.\n");
        }
        else
        {
            LOG_CONF("- with the IP security order: 'allow,deny' (deny if not allow). Authorised IP are:\n");
            for (IP_Mask mask : access_allow)
                LOG_CONF("  %s\n", mask.to_string().c_str());
        }
    }
    else
    {
        // ACO::MUTUAL_FAILURE
        LOG_CONF("- with the IP security order: 'mutual-failure' (allow if in the allow list and not in the deny list).\n");
        if (access_allow.empty())
        {
            LOG_WARN("  But, NO IP IS AUTHORISED!\n");
        }
        else if (access_deny.size() == 1 && access_deny[0].covers_all())
        {
            LOG_CONF("  But, you refuse ALL IP!\n");
        }
        else
        {
            if (access_allow.size() == 1 && access_allow[0].covers_all())
            {
                LOG_CONF("  You authorise ALL IP.\n");
            }
            else
            {
                LOG_CONF("  Authorised IP are:\n");
                for (IP_Mask mask : access_allow)
                    LOG_CONF("    %s\n", mask.to_string().c_str());
            }
            LOG_CONF("  Refused IP are:\n");
            for (IP_Mask mask : access_deny)
                LOG_CONF("    %s\n", mask.to_string().c_str());
        }
    }
}

/// Function called at exit of the server
// is all of this really needed?
void term_func(void)
{
    mmo_auth_sync();

    for (sint32 i = 0; i < MAX_SERVERS; i++)
        delete_session(server[i].fd);
    delete_session(login_fd);

    LOG_DEBUG("----End of login-server (normal end with closing of all files).\n");
}

/// Main function of login-server (read conf and set up parsers)
void do_init(sint32 argc, char **argv)
{
    // add some useful logs
    init_log();
    login_log.add(login_log_filename, true, Level::INFO);
    unknown_packet_log.add(login_log_unknown_packets_filename, false, Level::DEBUG);

    login_config_read((argc > 1) ? argv[1] : LOGIN_CONF_NAME);
    // not in login_config_read, because we can use 'import' option, and display same message twice or more
    // TODO - shouldn't the warnings display anytime an invalid option is set?
    display_conf_warnings();
    save_config_in_log();
    login_lan_config_read((argc > 2) ? argv[2] : LAN_CONF_NAME);

    for (sint32 i = 0; i < AUTH_FIFO_SIZE; i++)
        auth_fifo[i].delflag = 1;
    for (sint32 i = 0; i < MAX_SERVERS; i++)
        server[i].fd = -1;

    read_gm_account();
    mmo_auth_init();

    set_defaultparse(parse_login);
    login_fd = make_listen_port(login_port);

    // save account information every 5 minutes
    add_timer_interval(gettick() + std::chrono::minutes(5), std::chrono::minutes(5), check_auth_sync);

    if (anti_freeze_enable)
        add_timer_interval(gettick() + std::chrono::seconds(1), ANTI_FREEZE_INTERVAL, char_anti_freeze_system);

    // add timer to check GM accounts file modification
    // this shouldn't be needed
    std::chrono::seconds j = gm_account_filename_check_timer;
    if (j != std::chrono::seconds::zero())
        add_timer_interval(gettick() + j, j, check_gm_file);

    LOG_INFO("The login-server is ready (Server is listening on the port %d).\n",
             login_port);
}