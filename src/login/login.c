#include "login.h"

#include <stdio.h>
#include <stdlib.h>
/// for gmtime()
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

/// for gettimeofday()
#include <sys/time.h>
/// for stat(), used on the GM_account file
#include <sys/stat.h>
/// for waitpid()
#include <sys/wait.h>

/// for in_addr_t, etc.; for htonl(), etc.; for inet_addr()
#include <netinet/in.h>
#include <arpa/inet.h>

/// for gethostbyname (obselete, currently used) or getaddrinfo (TODO use instead)
#include <netdb.h>

#include "../common/core.h"
#include "../common/socket.h"
#include "../common/timer.h"
#include "../common/mmo.h"
#include "../common/version.h"
#include "../common/db.h"
#include "../common/lock.h"
#include "../common/mt_rand.h"

#include "../common/md5calc.h"

#define STRZCPY2(dst,src) strzcpy (dst, src, ARRAY_SIZEOF(src))

account_t account_id_count = START_ACCOUNT_NUM;
int  new_account_flag = 0;
in_port_t login_port = 6900;
/// TODO make this in_addr_t
char lan_char_ip[16];
uint8_t subneti[4];
uint8_t subnetmaski[4];
char update_host[128] = "";
char main_server[20] = "";

char account_filename[1024] = "save/account.txt";
char GM_account_filename[1024] = "conf/GM_account.txt";
char login_log_filename[1024] = "log/login.log";
char login_log_unknown_packets_filename[1024] = "log/login_unknown_packets.log";

/// TODO: instead of using this manually, create a stamp_time() function
// maybe also a stamp_time_milli()
#define DATE_FORMAT "%Y-%m-%d %H:%M:%S"
// 4+1+2+1+2 + 1 + 2+1+2+1+2 = 10 + 1 + 8 = 19, plus the NUL-terminator
#define DATE_FORMAT_MAX 20
/// log unknown packets from the initial login
// unknown ladmin and char-server packets are always logged
bool save_unknown_packets = 0;
/// When the gm account file was last modified
time_t creation_time_GM_account_file;
/// Interval (in seconds) after which to recheck GM file.
unsigned int gm_account_filename_check_timer = 15;

bool display_parse_login = 0;
bool display_parse_admin = 0;
enum char_packets_display_t
{
CP_NONE,
CP_MOST,
CP_ALL
} display_parse_fromchar = CP_NONE;

// FIXME: merge the arrays into members of struct mmo_char_server
struct mmo_char_server server[MAX_SERVERS];
int  server_fd[MAX_SERVERS];
// Char-server anti-freeze system. Set to 5 when receiving
// packet 0x2714, decrements every ANTI_FREEZE_INTERVAL seconds.
// If it reaches zero, assume the char server has frozen and disconnect it.
// DON'T enable this if you are going to use gdb
int  server_freezeflag[MAX_SERVERS];
bool anti_freeze_enable = false;
unsigned int ANTI_FREEZE_INTERVAL = 15;

int  login_fd;

enum ACO
{
    ACO_DENY_ALLOW = 0,
    ACO_ALLOW_DENY,
    ACO_MUTUAL_FAILURE,
};

typedef char access_entry[128];

enum ACO access_order = ACO_DENY_ALLOW;
unsigned int access_allownum = 0;
unsigned int access_denynum = 0;
access_entry *access_allow = NULL;
access_entry *access_deny = NULL;

unsigned int access_ladmin_allownum = 0;
access_entry *access_ladmin_allow = NULL;

// minimum level of player/GM (0: player, 1-99: gm) to connect on the server
gm_level_t min_level_to_connect = 0;
// Give possibility or not to adjust (ladmin command: timeadd) the time of an unlimited account.
bool add_to_unlimited_account = 0;
// Starting additional sec from now for the limited time at creation of accounts (-1: unlimited time, 0 or more: additional sec from now)
int  start_limited_time = -1;

/// some random data to be MD5'ed?
struct login_session_data
{
    int  md5keylen;
    char md5key[20];
};

#define AUTH_FIFO_SIZE 256
struct
{
    account_t account_id;
    uint32_t login_id1, login_id2;
    in_addr_t ip;
    enum gender sex;
    bool delflag;
} auth_fifo[AUTH_FIFO_SIZE];
int  auth_fifo_pos = 0;

struct auth_dat
{
    account_t account_id;
    enum gender sex;
    char userid[24], pass[40], lastlogin[24];
    unsigned int logincount;
    enum auth_failure state;
    char email[40];             // e-mail (by default: a@a.com)
    char error_message[20];     // Message of error code #6 = Your are Prohibited to log in until %s (packet 0x006a)
    time_t ban_until_time;      // # of seconds 1/1/1970 (timestamp): ban time limit of the account (0 = no ban)
    /// TODO remove this (and maybe add account creation time)
    time_t connect_until_time;  // # of seconds 1/1/1970 (timestamp): Validity limit of the account (0 = unlimited)
    char last_ip[16];           // save of last IP of connection
    char memo[255];             // a memo field
    int  account_reg2_num;
    struct global_reg account_reg2[ACCOUNT_REG2_NUM];
}   *auth_dat;

unsigned int  auth_num = 0, auth_max = 0;

/// whether ladmin is allowed
bool admin_state = 0;
char admin_pass[24] = ""; // "admin"
/// For a test server, password to make a GM
char gm_pass[64] = "";
gm_level_t level_new_gm = 60;

static struct dbt *gm_account_db;

pid_t pid = 0; // For forked DB writes


#define VERSION_2_UPDATEHOST 0x01   // client supports updatehost
#define VERSION_2_SERVERORDER 0x02  // send servers in forward order

const char *ip_of (int fd)
{
    static char out[16];
    ip_to_str (session[fd]->client_addr.sin_addr.s_addr, out);
    return out;
}

/// Sort accounts before saving or sending to ladmin
// currently uses a bubble-sort - TODO replace with something better
// FIXME - does this even need to be done at all?
void sort_accounts (int id[])
{
    for (int i = 0; i < auth_num; i++)
    {
        id[i] = i;
        for (int j = 0; j < i; j++)
        {
            if (auth_dat[i].account_id < auth_dat[id[j]].account_id)
            {
                for (int k = i; k > j; k--)
                    id[k] = id[k - 1];
                id[j] = i;      // id[i]
                break;
            }
        }
    }
}

/// The main login server log
static FILE *logfp;
/// log file for unknown packets
static FILE *unk_packets;

/// Writing something to log file (with timestamp) and to stderr (without)
// TODO make this more general (multiple log files ...)
// TODO add options to print common stuff: function/line, connection type,
// connection id, IP
void login_log (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#define log(fmt, ...)\
    fprintf (logfp, "%s:%d: in func %s by %s:", __FILE__, __LINE__, __func__, ip),\
    login_log(fmt ,##__VA_ARGS__)
void login_log (const char *fmt, ...)
{
    if (!logfp)
        logfp = fopen_ (login_log_filename, "a");
    if (!logfp)
        return;
    if (!fmt || !fmt[0])
    {
        fputc ('\n', logfp);
        return;
    }
    va_list ap;
    va_start (ap, fmt);

    struct timeval tv;
    gettimeofday (&tv, NULL);
    char tmpstr[DATE_FORMAT_MAX];
    strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime (&tv.tv_sec));
    fputs(tmpstr, logfp);
    fprintf(logfp, ".%03d: ", (int) tv.tv_usec / 1000);

    vfprintf (logfp, fmt, ap);
    vfprintf (stderr, fmt, ap);
    fflush (logfp);
    va_end (ap);
}

/// Determine GM level of account (0 is not a GM)
gm_level_t isGM (account_t account_id)
{
    struct gm_account *p = (struct gm_account*) numdb_search (gm_account_db, (numdb_key_t)account_id);
    if (!p)
        return 0;
    return p->level;
}

/// Read GM accounts file
void read_gm_account (void)
{
    if (gm_account_db)
        numdb_final(gm_account_db, NULL);
    gm_account_db = numdb_init ();

    FILE *fp = fopen_ (GM_account_filename, "r");
    if (!fp)
    {
        login_log ("%s: %s: %m\n", __func__, GM_account_filename);
        return;
    }
    // Limit of 4000 GMs because of how we send information to char-servers
    // 8 bytes * 4000 = 32k (limit of packets in windows - what about Linux?)
    // This is wrong: it only uses 5 bytes per GM, plus 4 bytes of protocol
    // Also, why does this matter, since TCP is stream-based not packet-based?
    unsigned int count = 0;
    char line[512];
    while (count < 4000 && fgets (line, sizeof (line), fp))
    {
        if ((line[0] == '/' && line[1] == '/') || line[0] == '\n' || line[0] == '\r')
            continue;
        struct gm_account p;
        if ((sscanf (line, "%u %hhu", &p.account_id, &p.level) != 2
            && sscanf (line, "%u: %hhu", &p.account_id, &p.level) != 2)
            || !p.level)
        {
            printf ("%s: %s: invalid format of entry %u.\n", __func__,
                    GM_account_filename, count);
            continue;
        }
        if (p.level > 99)
            p.level = 99;
        gm_level_t GM_level = isGM (p.account_id);

        if (GM_level)
            login_log ("%s: duplicate GM account %d (levels: %d and %d).\n",
                       __func__, p.account_id, GM_level, p.level);
        if (GM_level == p.level)
            continue;
        struct gm_account *ptr;
        CREATE (ptr, struct gm_account, 1);
        *ptr = p;
        numdb_insert (gm_account_db, (numdb_key_t)p.account_id, ptr);
        if (!GM_level)
            count++;
    }
    fclose_ (fp);
    if (count == 4000)
        login_log ("%s: %s: hit GM account limit\n", __func__,
                   GM_account_filename);
    login_log ("%s: %s: %d GM accounts read.\n", __func__,
               GM_account_filename, count);
}

/// Check whether an IP is allowed by a mask
// ip: IP address, network byte order
// str: prefix x[.y[.z[.w]]] or mask x.x.x.x/# or x.x.x.x/y.y.y.y)
bool check_ipmask (in_addr_t ip, const char *str)
{
    uint8_t p[4];
    int offset = 0;
    switch (sscanf (str, "%hhu.%hhu.%hhu.%hhu%n", &p[0], &p[1], &p[2], &p[3], &offset))
    {
    case 0: return true;
    case 1: return memcmp (&ip, p, 1) == 0;
    case 2: return memcmp (&ip, p, 2) == 0;
    case 3: return memcmp (&ip, p, 3) == 0;
    case 4: break;
    default: return false;
    }
    ip = ntohl (ip);
    in_addr_t ip2 = ntohl(*(in_addr_t*)p);
    if (str[offset] != '/')
        return ip == ip2;
    offset++;

    if (sscanf (str + offset, "%hhu.%hhu.%hhu.%hhu", &p[0], &p[1], &p[2], &p[3]) == 4)
    {
        in_addr_t mask = ntohl (*(in_addr_t*)p);
        return (ip & mask) == (ip2 & mask);
    }
    unsigned int bits;
    if (sscanf (str + offset, "%u", &bits) == 1 && bits <= 32)
        return !bits || (ip >> (32 - bits)) == (ip2 >> (32 - bits));
    login_log ("check_ipmask: invalid mask [%s].\n", str);
    return 0;
}

/// Check whether an IP is allowed
// ip: IP address, network byte order
// You are allowed if both lists are empty, or
// mode\ in list-> both    allow   deny    none
// allow,deny      Y       Y       N       N
// deny,allow      N       Y       N       Y
// mutual-failure  N       Y       N       N

bool check_ip (in_addr_t ip)
{
    if (access_allownum == 0 && access_denynum == 0)
        return 1;               // When there is no restriction, all IP are authorised.

    /// Flag of whether the ip is in the allow or deny list
    bool allowed = false;

    for (unsigned int i = 0; i < access_allownum; i++)
    {
        if (check_ipmask (ip, access_allow[i]))
        {
            if (access_order == ACO_ALLOW_DENY)
                return 1;
                // With 'allow, deny' (deny if not allow), allow has priority
            allowed = true;
            break;
        }
    }

    for (unsigned int i = 0; i < access_denynum; i++)
        if (check_ipmask (ip, access_deny[i]))
            return 0;

    return allowed || access_order == ACO_DENY_ALLOW;
}

/// Check whether an IP is allowed for ladmin
// ip: IP address, network byte order
bool check_ladminip (in_addr_t ip)
{
    if (access_ladmin_allownum == 0)
        return 1;
    for (unsigned int i = 0; i < access_ladmin_allownum; i++)
        if (check_ipmask (ip, access_ladmin_allow[i]))
            return 1;
    return 0;
}

/// Make a string safe by replacing control characters with _
// What about higher characters?
void remove_control_chars (char *str)
{
    for (int i = 0; str[i]; i++)
        // This behaves differently depending on whether char is signed or not
        if (str[i] < 32)
            str[i] = '_';
}

/// Check whether it looks like a valid email
bool e_mail_check (const char *email)
{
    if (email[0] == '.' || email[0] == '@')
        return 0;

    size_t len = strlen(email);
    if (len < 3 || len > 39)
        return 0;

    if (email[len - 1] == '@' || email[len - 1] == '.')
        return 0;

    const char *at = strchr (email, '@');
    if (!at)
        return 0;
    if (at[1] == '.' || at[-1] == '.')
        return 0;
    if (strchr (at + 1, '@'))
        return 0;

    if (strstr (email, ".."))
        return 0;

    do
    {
        // Note: this doesn't support quoted local parts - nobody uses them.
        // Note: this doesn't support UTF-8 addresses - it's not allowed yet.
        // Note: this doesn't support user@[ip.add.r.ess].
        if (*email < 0x20 || *email >= 0x7f || strchr(" \",:;<>[\\]", *email))
            return 0;
    } while (*++email);

    return 1;
}

//-----------------------------------------------
// Search an account id
//   (return account index or -1 (if not found))
//   If exact account name is not found,
//   the function checks without case sensitive
//   and returns index if only 1 account is found
//   and similar to the searched name.
//-----------------------------------------------
struct auth_dat *account_by_name (char *account_name)
{
    int quantity = 0;
    struct auth_dat *loose = NULL;

    for (int i = 0; i < auth_num; i++)
    {
        if (strcmp (auth_dat[i].userid, account_name) == 0)
            return &auth_dat[i];
        if (strcasecmp (auth_dat[i].userid, account_name) == 0)
        {
            quantity++;
            loose = &auth_dat[i];
        }
    }
    return quantity == 1 ? loose : NULL;
}

struct auth_dat *account_by_id (account_t acc)
{
    for (int i = 0; i < auth_num; i++)
        if (auth_dat[i].account_id == acc)
            return &auth_dat[i];
    return NULL;
}

/// Save an auth to file
void mmo_auth_to_file (FILE *fp, struct auth_dat *p)
{
    fprintf (fp, "%u\t" "%s\t" "%s\t" "%s\t"
                 "%c\t" "%u\t" "%d\t" "%s\t"
                 "%s\t" "%ld\t" "%s\t"
                 "%s\t" "%ld\t",
            p->account_id, p->userid, p->pass, p->lastlogin,
            sex_to_char(p->sex), p->logincount, p->state, p->email,
            p->error_message, (long)p->connect_until_time, p->last_ip,
            p->memo, (long)p->ban_until_time);

    // Save ## variables.
    // It looks like strings aren't supported in our version of eAthena
    // There has been a bug in the char server that broke these.
    // Note that we don't actually use any of these.
    for (int i = 0; i < p->account_reg2_num; i++)
        if (p->account_reg2[i].str[0])
            fprintf (fp, "%s,%d ", p->account_reg2[i].str,
                     p->account_reg2[i].value);
    fputc('\n', fp);
}

/// Read save/account.txt
void mmo_auth_init (void)
{
    int  GM_count = 0;
    int  server_count = 0;

    CREATE (auth_dat, struct auth_dat, 256);
    auth_max = 256;

    FILE *fp = fopen_ (account_filename, "r");
    if (!fp)
    {
        // no accounts means not even the char server can log in
        // hm, but can ladmin?
        login_log ("%s: %s: %m\n", __func__, account_filename);
        return;
    }

    char line[2048];
    while (fgets (line, sizeof (line), fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;

        struct auth_dat tmp = {};
        char sex;
        int n;
        uint8_t state;
        int i = sscanf (line,
                        "%u\t"        "%23[^\t]\t"   "%39[^\t]\t"  "%39[^\t]\t"
                        "%c\t"        "%u\t"         "%hhu\t"      "%39[^\t]\t"
                        "%19[^\t]\t"  "%ld\t"
                        "%15[^\t]\t"  "%254[^\t]\t"  "%ld"         "%n",
                        &tmp.account_id, tmp.userid,  tmp.pass, tmp.lastlogin,
                        &sex, &tmp.logincount, &state, tmp.email,
                        tmp.error_message, &tmp.connect_until_time,
                        tmp.last_ip, tmp.memo, &tmp.ban_until_time, &n);
        if (i < 12 || line[n] != '\t')
        {
            if (sscanf (line, "%d\t%%newid%%\n%n", &tmp.account_id, &n) == 1 &&
                    tmp.account_id > account_id_count)
                account_id_count = tmp.account_id;
            continue;
        }
        n++;

        // Some checks
        if (tmp.account_id > END_ACCOUNT_NUM)
        {
            login_log ("%s: account id %d exceeds %d.\n%s", __func__,
                       tmp.account_id, END_ACCOUNT_NUM, line);
            continue;
        }
        remove_control_chars (tmp.userid);

        int j;
        for (j = 0; j < auth_num; j++)
        {
            if (auth_dat[j].account_id == tmp.account_id)
            {
                login_log ("%s: duplicate account %d.\n%s", __func__,
                           tmp.account_id, line);
                break;
            }
            else if (strcmp (auth_dat[j].userid, tmp.userid) == 0)
            {
                login_log ("%s: duplicate account name %s (%d, %d).\n%s",
                           __func__, tmp.userid, auth_dat[j].account_id,
                           tmp.account_id, line);
                break;
            }
        }
        if (j != auth_num)
            continue;

        if (auth_num >= auth_max)
        {
            auth_max += 256;
            RECREATE (auth_dat, struct auth_dat, auth_max);
        }

        remove_control_chars (tmp.memo);
        remove_control_chars (tmp.pass);
        // If a password is not encrypted, we encrypt it now.
        // A password beginning with ! and - in the memo field is our magic
        if (tmp.pass[0] != '!' && tmp.memo[0] == '-') {
            STRZCPY(tmp.pass, MD5_saltcrypt(tmp.pass, make_salt()));
            tmp.memo[0] = '!';
        }

        remove_control_chars (tmp.lastlogin);

        tmp.sex = sex_from_char(sex);
        tmp.state = (enum auth_failure)state;

        if (!e_mail_check (tmp.email))
            STRZCPY (tmp.email, "a@a.com");
        else
            remove_control_chars (tmp.email);

        remove_control_chars (tmp.error_message);
        if (tmp.error_message[0] == '\0' ||
            tmp.state != AUTH_BANNED_TEMPORARILY)
            STRZCPY (tmp.error_message, "-");

        if (i != 13)
            tmp.ban_until_time = 0;

        remove_control_chars (tmp.last_ip);

        char *p = line;
//        int j;
        for (j = 0; j < ACCOUNT_REG2_NUM; j++)
        {
            p += n;
            if (sscanf (p, "%31[^\t,],%d %n", tmp.account_reg2[j].str,
                        &tmp.account_reg2[j].value, &n) != 2)
            {
                int v;
                if (p[0] == ',' && sscanf (p, ",%d %n", &v, &n) == 1)
                {
                    j--;
                    continue;
                }
                break;
            }
            remove_control_chars (tmp.account_reg2[j].str);
        }
        tmp.account_reg2_num = j;

        if (isGM (tmp.account_id) > 0)
            GM_count++;
        if (tmp.sex == SEX_SERVER)
            server_count++;

        auth_dat[auth_num] = tmp;
        auth_num++;
        if (tmp.account_id >= account_id_count)
            account_id_count = tmp.account_id + 1;
    }
    fclose_ (fp);

    if (auth_num == 0)
    {
        login_log ("%s: No account found in %s.", __func__, account_filename);
        return;
    }
    if (auth_num == 1)
        login_log ("%s: 1 account read in %s,", __func__, account_filename);
    else
        login_log ("%s: %d accounts read in %s,", __func__, auth_num,
                   account_filename);
    if (GM_count == 0)
        login_log (" of which is no GM account and");
    else if (GM_count == 1)
        login_log (" of which is 1 GM account and");
    else
        login_log (" of which is %d GM accounts and", GM_count);
    if (server_count == 0)
        login_log (" no server account ('S').\n");
    else if (server_count == 1)
        login_log (" 1 server account ('S').\n");
    else
        login_log (" %d server accounts ('S').\n", server_count);
}

/// Save accounts to database file (text)
// usually called in a forked child, as it may take a while
void mmo_auth_sync (void)
{
    // Note: this is a vla
    int  id[auth_num];
    sort_accounts (id);

    // Data save
    int lock;
    FILE *fp = lock_fopen (account_filename, &lock);
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
    for (int i = 0; i < sizeof(header) / sizeof(header[0]); i++)
        fprintf (fp, "// %s\n", header[i]);
    for (int i = 0; i < auth_num; i++)
        mmo_auth_to_file (fp, &auth_dat[id[i]]);
    fprintf (fp, "%d\t%%newid%%\n", account_id_count);

    lock_fclose (fp, account_filename, &lock);
}

void term_func (void)
{
    // does this cause problem if it is already syncing in the child?
    mmo_auth_sync ();
}


/// Timer to sync the DB to disk as little as possible
// this is resource-intensive, so fork() if possible
void check_auth_sync (timer_id UNUSED, tick_t UNUSED, custom_id_t UNUSED, custom_data_t UNUSED)
{
    if (pid && !waitpid (pid, NULL, WNOHANG))
        // if already running
        return;

    pid = fork ();
    if (pid > 0)
        return;
    // either we are the child, or fork() failed
    mmo_auth_sync ();

    // If we're a child we should suicide now.
    if (pid == 0)
        _exit (0);
}

/// Send a packet to all char servers, excluding sfd
// often called with sfd == -1 to not exclude anything
void charif_sendallwos (int sfd, unsigned char *buf, unsigned int len)
{
    for (int i = 0; i < MAX_SERVERS; i++)
    {
        int fd = server_fd[i];
        if (fd >= 0 && fd != sfd)
        {
            memcpy (WFIFOP (fd, 0), buf, len);
            WFIFOSET (fd, len);
        }
    }
}

/// Send GM accounts to all char servers
void send_GM_accounts (void)
{
    uint8_t buf[32000];
    int  len = 4;
    WBUFW (buf, 0) = 0x2732;
    // shouldn't we just iterate over the db instead?
    for (int i = 0; i < auth_num; i++)
    {
        gm_level_t GM_value = isGM (auth_dat[i].account_id);
        if (GM_value)
        {
            WBUFL (buf, len) = auth_dat[i].account_id;
            WBUFB (buf, len + 4) = (unsigned char) GM_value;
            len += 5;
        }
    }
    WBUFW (buf, 2) = len;
    charif_sendallwos (-1, buf, len);
}

/// Timer to check if GM file account have been changed
// TODO replace this with inotify on systems where it is available
void check_GM_file (timer_id UNUSED, tick_t UNUSED, custom_id_t UNUSED,
                    custom_data_t UNUSED)
{
    // if checking is disabled
    if (!gm_account_filename_check_timer)
        return;

    struct stat file_stat;
    time_t new_time = 0;
    if (stat (GM_account_filename, &file_stat) == 0)
        new_time = file_stat.st_mtime;

    if (new_time != creation_time_GM_account_file)
    {
        read_gm_account ();
        creation_time_GM_account_file = new_time;
        send_GM_accounts ();
    }
}

/// Create a new account from the given connection
account_t mmo_auth_new (struct mmo_account *account, const char *email)
{
    if (auth_num >= auth_max)
    {
        auth_max += 256;
        RECREATE (auth_dat, struct auth_dat, auth_max);
    }

    struct auth_dat *auth = &auth_dat[auth_num];

    memset (auth, '\0', sizeof (struct auth_dat));

    // disallow creating an account that already is labeled a GM
    while (isGM (account_id_count))
        account_id_count++;

    auth->account_id = account_id_count++;

    STRZCPY (auth->userid, account->userid);
    STRZCPY (auth->pass, MD5_saltcrypt(account->passwd, make_salt()));
    STRZCPY (auth->lastlogin, "-");

    auth->sex = account->sex;
    auth->logincount = 0;
    auth->state = AUTH_OK;

    if (!e_mail_check (email))
        STRZCPY (auth->email, "a@a.com");
    else
        STRZCPY (auth->email, email);

    STRZCPY (auth->error_message, "-");
    auth->ban_until_time = 0;

    if (start_limited_time < 0)
        auth->connect_until_time = 0;
    else
        auth->connect_until_time = time (NULL) + start_limited_time;

    STRZCPY (auth->last_ip, "-");
    STRZCPY (auth->memo, "!");

    auth->account_reg2_num = 0;

    auth_num++;
    return auth->account_id;
}

/// Try to authenticate a connection
enum auth_failure mmo_auth (struct mmo_account *account, int fd)
{
    struct timeval tv;
    char ip[16];
    ip_to_str (session[fd]->client_addr.sin_addr.s_addr, ip);


    size_t len = strlen (account->userid) - 2;
    // Account creation with _M/_F
    // TODO does this actually happen? I thought "create account" was a login-server thing
    bool newaccount =
        account->userid[len] == '_' &&
        (account->userid[len + 1] == 'F' || account->userid[len + 1] == 'M') &&
        new_account_flag &&
        account_id_count <= END_ACCOUNT_NUM &&
        len >= 4 &&
        strlen (account->passwd) >= 4;
    if (newaccount)
        account->userid[len] = '\0';

    struct auth_dat *auth = account_by_name (account->userid);
    if (!auth)
    {
        auth = &auth_dat[auth_num];
        if (!newaccount)
        {
            login_log ("%s: unknown account %s (ip: %s)\n", __func__,
                       account->userid, ip);
            return AUTH_UNREGISTERED_ID;
        }
        account->sex = sex_from_char (account->userid[len + 1]);
        account_t new_id = mmo_auth_new (account, "a@a.com");
        login_log ("%s: created account %s (id: %d, sex: %c, ip: %s)\n",
                   __func__, account->userid, new_id, account->userid[len + 1],
                   ip);
    }
    else
    {
        // for the possible tests/checks afterwards (copy correct case).
        // Note: this copies back into the rfifo
        STRZCPY2 (account->userid, auth->userid);

        if (newaccount)
        {
            login_log ("%s: tried to create existing account %s_%c (ip %s)\n",
                       __func__, account->userid, account->userid[len + 1], ip);
            return AUTH_ALREADY_EXISTS;
        }
        if (!pass_ok (account->passwd, auth->pass))
        {
            login_log ("%s: invalid password for %s (ip %s)\n", __func__,
                       account->userid, ip);
            return AUTH_INVALID_PASSWORD;
        }
        if (auth->state != AUTH_OK)
        {
            // is this right? What about bans that expire?
            login_log ("%s: refused %s due to state %d (ip: %s)\n",
                       __func__, account->userid, auth->state, ip);
            if (auth->state < AUTH_ALREADY_EXISTS)
                return auth->state;
            else
                return AUTH_ID_ERASED;
        }
        // can this even happen?
        if (auth->ban_until_time)
        {
            char tmpstr[DATE_FORMAT_MAX];
            strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT,
                      gmtime (&auth->ban_until_time));
            if (auth->ban_until_time > time (NULL))
            {
                login_log ("%s: refuse %s - banned until %s (ip: %s)\n",
                           __func__, account->userid, tmpstr, ip);
                return AUTH_BANNED_TEMPORARILY;
            }
            else
            {
                login_log ("%s: end ban %s - banned until %s (ip: %s)\n",
                           __func__, account->userid, tmpstr, ip);
                auth->ban_until_time = 0; // reset the ban time
            }
        }

        if (auth->connect_until_time &&
                auth->connect_until_time < time (NULL))
        {
            login_log ("%s: %s has expired ID (ip: %s)\n", __func__,
                       account->userid, ip);
            return AUTH_EXPIRED;
        }

        login_log ("%s: authenticated %s (id: %d, ip: %s)\n", __func__,
                   account->userid, auth->account_id, ip);
    }

    gettimeofday (&tv, NULL);
    char tmpstr[DATE_FORMAT_MAX + 4];
    strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime (&tv.tv_sec));
    sprintf (tmpstr + strlen (tmpstr), ".%03d", (int) tv.tv_usec / 1000);

    account->account_id = auth->account_id;
    account->login_id1 = mt_random ();
    account->login_id2 = mt_random ();
    STRZCPY (account->lastlogin, auth->lastlogin);
    STRZCPY (auth->lastlogin, tmpstr);
    account->sex = auth->sex;
    STRZCPY (auth->last_ip, ip);
    auth->logincount++;

    return AUTH_OK;
}

/// Kill char servers that don't send the common packet after 5 calls
void char_anti_freeze_system (timer_id UNUSED, tick_t UNUSED,
                              custom_id_t UNUSED, custom_data_t UNUSED)
{
    for (int i = 0; i < MAX_SERVERS; i++)
    {
        if (server_fd[i] < 0)
            continue;
        if (!server_freezeflag[i]--)
        {
            login_log ("%s: disconnect frozen char-server #%d '%s'.\n",
                       __func__, i, server[i].name);
            session[server_fd[i]]->eof = 1;
        }
    }
}



/// Reload GM accounts
/// Forwarded from map-server
// uint16_t packet
void x2709(int fd, int id)
{
    login_log ("Char-server '%s': Request to re-load GM configuration file (ip: %s).\n",
               server[id].name, ip_of (fd));
    read_gm_account ();
    // send GM accounts to all char-servers
    send_GM_accounts ();
}

/// authenticate an account to the char-server
// uint16_t packet, uint32_t acc, uint32_t login_id[2], char sex, uint32_t ip
void x2712(int fd, int id)
{
    account_t acc = RFIFOL (fd, 2);
    for (int i = 0; i < AUTH_FIFO_SIZE; i++)
    {
        if (auth_fifo[i].account_id != acc ||
                auth_fifo[i].login_id1 != RFIFOL (fd, 6) ||
                auth_fifo[i].login_id2 != RFIFOL (fd, 10) ||
                auth_fifo[i].sex != (enum gender)RFIFOB (fd, 14) ||
                auth_fifo[i].ip != RFIFOL (fd, 15) ||
                auth_fifo[i].delflag)
            continue;
        auth_fifo[i].delflag = 1;
        login_log ("Char-server '%s': authenticated %d (ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        for (int k = 0; k < auth_num; k++)
        {
            if (auth_dat[k].account_id != acc)
                continue;
            // send ## variables
            WFIFOW (fd, 0) = 0x2729;
            WFIFOL (fd, 4) = acc;
            int p = 8;
            for (int j = 0; j < auth_dat[k].account_reg2_num; j++)
            {
                STRZCPY2 ((char *)WFIFOP (fd, p),
                          auth_dat[k].account_reg2[j].str);
                p += 32;
                WFIFOL (fd, p) = auth_dat[k].account_reg2[j].value;
                p += 4;
            }
            WFIFOW (fd, 2) = p;
            WFIFOSET (fd, p);
            // send player email and expiration
            WFIFOW (fd, 0) = 0x2713;
            WFIFOL (fd, 2) = acc;
            WFIFOB (fd, 6) = 0;
            STRZCPY2 ((char *)WFIFOP (fd, 7), auth_dat[k].email);
            WFIFOL (fd, 47) = auth_dat[k].connect_until_time;
            WFIFOSET (fd, 51);
            return;
        } // for k in auth_dat
        return;
    } // for i in auth_fifo
    // authentication not found
    login_log ("Char-server '%s': denied auth %d (ip: %s).\n",
                server[id].name, acc, ip_of (fd));
    WFIFOW (fd, 0) = 0x2713;
    WFIFOL (fd, 2) = acc;
    WFIFOB (fd, 6) = 1;
    // It is unnecessary to send email
    // It is unnecessary to send validity date of the account
    WFIFOSET (fd, 51);
}

/// Report of number of users on the server
// uint16_t packet, uint32_t usercount
void x2714(int fd, int id)
{
    server[id].users = RFIFOL (fd, 2);
    if (anti_freeze_enable)
        server_freezeflag[id] = 5;
}

/// Request initial setting of email (no answer, but may fail)
// uint16_t packet, uint32_t acc, char email[40]
void x2715(int fd, int id)
{
    account_t acc = RFIFOL (fd, 2);
    char email[40];
    STRZCPY (email, (char *)RFIFOP (fd, 6));
    remove_control_chars (email);
    if (!e_mail_check (email))
    {
        login_log ("Char-server '%s': refused to init email by %d (ip: %s)\n",
                    server[id].name, acc, ip_of (fd));
        return;
    }
    struct auth_dat *auth = account_by_id (acc);
    if (!auth)
    {
        login_log ("Char-server '%s': refused to init email - no such account %d (ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    if (strcmp (auth->email, "a@a.com") != 0)
    {
        login_log ("Char-server '%s': refused to init email for %d - it is already set (ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    STRZCPY (auth->email, email);
    login_log ("Char-server '%s': init email (account: %d, e-mail: %s, ip: %s).\n",
               server[id].name, acc, email, ip_of (fd));
}

/// Request email and expiration time
// uint16_t packet, uint32_t account
void x2716 (int fd, int id)
{
    account_t acc = RFIFOL (fd, 2);
    struct auth_dat *auth = account_by_id (acc);
    if (!auth)
    {
        login_log ("Char-server '%s': can't send e-mail - no account %d (ip: %s).\n",
                   server[id].name, RFIFOL (fd, 2), ip_of (fd));
        return;
    }
    login_log ("Char-server '%s': send e-mail of %u (ip: %s).\n",
               server[id].name, acc, ip_of (fd));
    WFIFOW (fd, 0) = 0x2717;
    WFIFOL (fd, 2) = acc;
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->email);
    WFIFOL (fd, 46) = auth->connect_until_time;
    WFIFOSET (fd, 50);
}

/// Request to become GM
// uint16_t packet, uint16_t len, char gm_pass[len]
void x2720 (int fd, int id)
{
    unsigned char buf[10];
    account_t acc = RFIFOL (fd, 4);
    WBUFW (buf, 0) = 0x2721;
    WBUFL (buf, 2) = acc;
    WBUFL (buf, 6) = 0; // level of gm they became
    if (!level_new_gm)
    {
        login_log ("Char-server '%s': request to make %u a GM, but GM creation is disable (ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    if (strcmp ((char *)RFIFOP (fd, 8), gm_pass) != 0)
    {
        login_log ("Failed to make %u a GM: incorrect password (ip: %s).\n",
                   acc, ip_of (fd));
        return;
    }
    if (isGM (acc))
    {
        login_log ("Char-server '%s': Error: %d is already a GM (ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    FILE *fp = fopen_ (GM_account_filename, "a");
    if (!fp)
    {
        login_log ("Char-server '%s': %s: %m\n", server[id].name,
                   GM_account_filename);
        return;
    }
    struct timeval tv;
    gettimeofday (&tv, NULL);
    char tmpstr[DATE_FORMAT_MAX];
    strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime (&tv.tv_sec));
    fprintf (fp, "\n// %s: @GM command\n%d %d\n", tmpstr, acc, level_new_gm);
    fclose_ (fp);
    WBUFL (buf, 6) = level_new_gm;
    // FIXME: this is stupid
    read_gm_account ();
    send_GM_accounts ();
    login_log("Char-server '%s': give %d gm level %d (ip: %s).\n",
              server[id].name, acc, level_new_gm, ip_of (fd));
    // Note: this used to be sent even if it failed
    charif_sendallwos (-1, buf, 10);
}

/// Map server request (via char-server) to change an email
// uint16_t packet, uint32_t acc, char email[40], char new_email[40]
void x2722 (int fd, int id)
{
    account_t acc = RFIFOL (fd, 2);
    char actual_email[40];
    STRZCPY (actual_email, (char *)RFIFOP (fd, 6));
    remove_control_chars (actual_email);
    char new_email[40];
    STRZCPY (new_email, (char *)RFIFOP (fd, 46));
    remove_control_chars (new_email);

    // is this needed?
    if (!e_mail_check (actual_email))
    {
        login_log ("Char-server '%s': actual email is invalid (account: %d, ip: %s)\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    if (!e_mail_check (new_email))
    {
        login_log ("Char-server '%s': invalid new e-mail (account: %d, ip: %s)\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    if (strcasecmp (new_email, "a@a.com") == 0)
    {
        login_log ("Char-server '%s': setting email to default is not allowed (account: %d, ip: %s)\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    struct auth_dat *auth = account_by_id (acc);
    if (!auth)
    {
        login_log ("Char-server '%s': Attempt to modify an e-mail on an account (@email GM command), but account doesn't exist (account: %d, ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }

    if (strcasecmp (auth->email, actual_email) != 0)
    {
        login_log ("Char-server '%s': fail to change email (account: %u (%s), actual e-mail: %s, but given e-mail: %s, ip: %s).\n",
                   server[id].name, acc, auth->userid,
                   auth->email, actual_email, ip_of (fd));
        return;
    }
    STRZCPY (auth->email, new_email);
    login_log ("Char-server '%s': change e-mail for %d (%s) to %s (ip: %s).\n",
               server[id].name, acc, auth->userid, new_email, ip_of (fd));
}

/// change state of a player (only used for block/unblock)
// uint16_t packet, uint32_t acc, uint32_t state (0 or 5)
void x2724 (int fd, int id)
{
    account_t acc = RFIFOL (fd, 2);
    enum auth_failure state = (enum auth_failure) RFIFOL (fd, 6);
    struct auth_dat *auth = account_by_id (acc);
    if (!auth)
    {
        login_log ("Char-server '%s': failed to change state of %d to %d - no such account (ip: %s).\n",
                   server[id].name, acc, state, ip_of (fd));
        return;
    }
    if (auth->state == state)
    {
        login_log ("Char-server '%s':  Error: state of %d already %d (ip: %s).\n",
                   server[id].name, acc, state, ip_of (fd));
        return;
    }
    login_log ("Char-server '%s': change state of %d to %hhu (ip: %s).\n",
               server[id].name, acc, (uint8_t)state, ip_of (fd));
    auth->state = state;
    if (!state)
        return;
    unsigned char buf[11];
    WBUFW (buf, 0) = 0x2731;
    WBUFL (buf, 2) = acc;
    // 0: change of state, 1: ban
    WBUFB (buf, 6) = 0;
    // state (or final date of a banishment)
    WBUFL (buf, 7) = state;
    charif_sendallwos (-1, buf, 11);
    for (int j = 0; j < AUTH_FIFO_SIZE; j++)
        if (auth_fifo[j].account_id == acc)
            // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
            auth_fifo[j].login_id1++;
}

/// ban request from map-server (via char-server)
// uint16_t packet, uint32_t acc, uint16_t Y,M,D,h,m,s
void x2725 (int fd, int id)
{
    account_t acc = RFIFOL (fd, 2);
    struct auth_dat *auth = account_by_id (acc);
    if (!auth)
    {
        login_log ("Char-server '%s': Error of ban request (account: %d not found, ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    time_t timestamp = time (NULL);
    if (auth->ban_until_time >= timestamp)
        timestamp = auth->ban_until_time;
    // TODO check for overflow
    // years (365.25 days)
    timestamp += 31557600 * (short) RFIFOW (fd, 6);
    // a month isn't well-defined - use 1/12 of a year
    timestamp += 2629800 * (short) RFIFOW (fd, 8);
    timestamp += 86400 * (short) RFIFOW (fd, 10);
    timestamp += 3600 * (short) RFIFOW (fd, 12);
    timestamp += 60 * (short) RFIFOW (fd, 14);
    timestamp += (short) RFIFOW (fd, 16);
    if (auth->ban_until_time == timestamp)
    {
        login_log ("Char-server '%s': Error of ban request (account: %d, no change for ban date, ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    if (timestamp <= time (NULL))
    {
        login_log ("Char-server '%s': Error of ban request (account: %d, new date unbans the account, ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    char tmpstr[DATE_FORMAT_MAX] = "no banishment";
    if (timestamp)
        strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime (&timestamp));
    login_log ("Char-server '%s': Ban request (account: %d, new final date of banishment: %ld (%s), ip: %s).\n",
               server[id].name, acc, (long)timestamp, tmpstr, ip_of (fd));
    unsigned char buf[11];
    WBUFW (buf, 0) = 0x2731;
    WBUFL (buf, 2) = auth->account_id;
    // 0: change of state, 1: ban
    WBUFB (buf, 6) = 1;
    // final date of a banishment (or new state)
    WBUFL (buf, 7) = timestamp;
    charif_sendallwos (-1, buf, 11);
    for (int j = 0; j < AUTH_FIFO_SIZE; j++)
        if (auth_fifo[j].account_id == acc)
            // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
            auth_fifo[j].login_id1++;
    auth->ban_until_time = timestamp;
}

/// Request for sex change
// uint16_t packet, uint32_t acc
void x2727 (int fd, int id)
{
    account_t acc = RFIFOL (fd, 2);
    struct auth_dat *auth = account_by_id (acc);
    if (!auth)
    {
        login_log ("Char-server '%s': Error of sex change (account: %d not found, sex would be reversed, ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    switch (auth->sex)
    {
    case SEX_FEMALE: auth->sex = SEX_MALE; break;
    case SEX_MALE: auth->sex = SEX_FEMALE; break;
    case SEX_SERVER:
        login_log ("Char-server '%s': can't change sex of server account %d (ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    unsigned char buf[16];
    login_log ("Char-server '%s': change sex of %u to %c (ip: %s).\n",
               server[id].name, acc, sex_to_char(auth->sex), ip_of (fd));
    for (int j = 0; j < AUTH_FIFO_SIZE; j++)
        if (auth_fifo[j].account_id == acc)
            // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
            auth_fifo[j].login_id1++;
    WBUFW (buf, 0) = 0x2723;
    WBUFL (buf, 2) = acc;
    WBUFB (buf, 6) = (uint8_t)auth->sex;
    charif_sendallwos (-1, buf, 7);
}

/// Receive ## variables a char-server, and forward them to other char-servers
// uint16_t packet, uint16_t len, {char[32] name, int32_t val}[]
// note - this code assumes that len is proper, i.e len % 36 == 4
void x2728 (int fd, int id)
{
    account_t acc = RFIFOL (fd, 4);
    struct auth_dat *auth = account_by_id (acc);
    if (!auth)
    {
        login_log ("Char-server '%s': receiving (from the char-server) of account_reg2 (account: %d not found, ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }

    login_log ("Char-server '%s': receiving ## variables (account: %d, ip: %s).\n",
               server[id].name, acc, ip_of (fd));
    int p = 8;
    int j;
    for (j = 0; p < RFIFOW (fd, 2) && j < ACCOUNT_REG2_NUM; j++)
    {
        STRZCPY (auth->account_reg2[j].str, (char *)RFIFOP (fd, p));
        p += 32;
        remove_control_chars (auth->account_reg2[j].str);
        auth->account_reg2[j].value = RFIFOL (fd, p);
        p += 4;
    }
    auth->account_reg2_num = j;
    // Sending information towards the other char-servers.
    RFIFOW (fd, 0) = 0x2729;
    charif_sendallwos (fd, RFIFOP (fd, 0), RFIFOW (fd, 2));
}

/// unban request
// uint16_t packet, uint32_t acc
void x272a (int fd, int id)
{
    account_t acc = RFIFOL (fd, 2);
    struct auth_dat *auth = account_by_id (acc);
    if (!auth)
    {
        login_log ("Char-server '%s': Error of UnBan request (account: %d not found, ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    if (!auth->ban_until_time)
    {
        login_log ("Char-server '%s': request to unban account %d, which wasn't banned (ip: %s).\n",
                   server[id].name, acc, ip_of (fd));
        return;
    }
    auth->ban_until_time = 0;
    login_log ("Char-server '%s': unban account %d (ip: %s).\n",
               server[id].name, acc, ip_of (fd));
}

/// request to change account password
// uint16_t packet, uint32_t acc, char old[24], char new[24]
void x2740 (int fd, int id)
{
    account_t acc = RFIFOL (fd, 2);
    char actual_pass[24];
    STRZCPY (actual_pass, (char *)RFIFOP (fd, 6));
    remove_control_chars (actual_pass);
    char new_pass[24];
    STRZCPY (new_pass, (char *)RFIFOP (fd, 30));
    remove_control_chars (new_pass);

    enum passwd_failure status = PASSWD_NO_ACCOUNT;
    struct auth_dat *auth = account_by_id (acc);
    if (!auth)
        goto send_x272a_reply;

    if (pass_ok (actual_pass, auth->pass))
    {
        if (strlen (new_pass) < 4)
            status = PASSWD_TOO_SHORT;
        else
        {
            status = PASSWD_OK;
            STRZCPY (auth->pass, MD5_saltcrypt(new_pass, make_salt()));
            login_log ("Char-server '%s': Change pass success (account: %d (%s), ip: %s.\n",
                       server[id].name, acc, auth->userid, ip_of (fd));
        }
    }
    else
    {
        status = PASSWD_WRONG_PASSWD;
        login_log ("Char-server '%s': Attempt to modify a pass failed, wrong password. (account: %d (%s), ip: %s).\n",
                   server[id].name, acc, auth->userid, ip_of (fd));
    }
send_x272a_reply:
    WFIFOW (fd, 0) = 0x2741;
    WFIFOL (fd, 2) = acc;
    WFIFOB (fd, 6) = status;
    WFIFOSET (fd, 7);
}


/// Parse packets from a char server
void parse_fromchar (int fd)
{
    int id;
    for (id = 0; id < MAX_SERVERS; id++)
        if (server_fd[id] == fd)
            break;
    if (id == MAX_SERVERS || session[fd]->eof)
    {
        if (id < MAX_SERVERS)
        {
            login_log ("Char-server '%s' has disconnected (ip: %s).\n",
                       server[id].name, ip_of (fd));
            server_fd[id] = -1;
            memset (&server[id], 0, sizeof (struct mmo_char_server));
        }
        else
        {
            login_log ("Invalid char server (ip: %s)\n", ip_of (fd));
        }
        close (fd);
        delete_session (fd);
        return;
    }

    while (RFIFOREST (fd) >= 2)
    {
        if (display_parse_fromchar == CP_ALL ||
            (display_parse_fromchar == CP_MOST && RFIFOW (fd, 0) != 0x2714))
            // 0x2714 is done very often (number of players)
            login_log ("%s: connection #%d, packet: 0x%x (with %d bytes).\n",
                       __func__, fd, RFIFOW (fd, 0), RFIFOREST (fd));

        switch (RFIFOW (fd, 0))
        {
            /// Reload GM accounts
            /// Forwarded from map-server
            // uint16_t packet
            case 0x2709:
                x2709(fd, id);
                RFIFOSKIP (fd, 2);
                break;

            /// authenticate an account to the char-server
            // uint16_t packet, uint32_t acc, uint32_t login_id[2], char sex, uint32_t ip
            case 0x2712:
                if (RFIFOREST (fd) < 19)
                    return;
                x2712 (fd, id);
                RFIFOSKIP (fd, 19);
                break;

            /// Report of number of users on the server
            // uint16_t packet, uint32_t usercount
            case 0x2714:
                if (RFIFOREST (fd) < 6)
                    return;
                x2714 (fd, id);
                RFIFOSKIP (fd, 6);
                break;

            /// Request initial setting of email (no answer, but may fail)
            // uint16_t packet, uint32_t acc, char email[40]
            case 0x2715:
                if (RFIFOREST (fd) < 46)
                    return;
                x2715 (fd, id);
                RFIFOSKIP (fd, 46);
                break;

            /// Request email and expiration time
            // uint16_t packet, uint32_t account
            case 0x2716:
                if (RFIFOREST (fd) < 6)
                    return;
                x2716 (fd, id);
                RFIFOSKIP (fd, 6);
                break;

            /// Request to become GM
            // uint16_t packet, uint16_t len, char gm_pass[len]
            case 0x2720:
                if (RFIFOREST (fd) < 4 || RFIFOREST (fd) < RFIFOW (fd, 2))
                    return;
                x2720 (fd, id);
                RFIFOSKIP (fd, RFIFOW (fd, 2));
                return;

            /// Map server request (via char-server) to change an email
            // uint16_t packet, uint32_t acc, char email[40], char new_email[40]
            case 0x2722:
                if (RFIFOREST (fd) < 86)
                    return;
                x2722 (fd, id);
                RFIFOSKIP (fd, 86);
                break;

            /// change state of a player (only used for block/unblock)
            // uint16_t packet, uint32_t acc, uint32_t state (0 or 5)
            case 0x2724:
                if (RFIFOREST (fd) < 10)
                    return;
                x2724 (fd, id);
                RFIFOSKIP (fd, 10);
                return;

            /// ban request from map-server (via char-server)
            // uint16_t packet, uint32_t acc, uint16_t Y,M,D,h,m,s
            case 0x2725:
                if (RFIFOREST (fd) < 18)
                    return;
                x2725 (fd, id);
                RFIFOSKIP (fd, 18);
                return;

            /// Request for sex change
            // uint16_t packet, uint32_t acc
            case 0x2727:
                if (RFIFOREST (fd) < 6)
                    return;
                x2727 (fd, id);
                RFIFOSKIP (fd, 6);
                return;

            /// Receive ## variables a char-server, and forward them to other char-servers
            // uint16_t packet, uint16_t len, {char[32] name, int32_t val}[]
            // note - this code assumes that len is proper, i.e len % 36 == 4
            case 0x2728:
                if (RFIFOREST (fd) < 4 || RFIFOREST (fd) < RFIFOW (fd, 2))
                    return;
                x2728 (fd, id);
                RFIFOSKIP (fd, RFIFOW (fd, 2));
                break;

            /// unban request
            // uint16_t packet, uint32_t acc
            case 0x272a:
                if (RFIFOREST (fd) < 6)
                    return;
                x272a (fd, id);
                RFIFOSKIP (fd, 6);
                return;

            /// request to change account password
            // uint16_t packet, uint32_t acc, char old[24], char new[24]
            case 0x2740:
                if (RFIFOREST (fd) < 54)
                    return;
                x2740 (fd, id);
                RFIFOSKIP (fd, 54);
                break;

            default:
                if (!unk_packets)
                    unk_packets = fopen_ (login_log_unknown_packets_filename, "a");
                if (unk_packets)
                {
                    struct timeval tv;
                    gettimeofday (&tv, NULL);
                    char tmpstr[DATE_FORMAT_MAX];
                    strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime (&(tv.tv_sec)));
                    fprintf (unk_packets,
                             "%s.%03d: receiving of an unknown packet -> disconnection\n",
                             tmpstr, (int) tv.tv_usec / 1000);
                    fprintf (unk_packets, "parse_fromchar: connection #%d (ip: %s), packet: 0x%hu (with %u bytes available).\n",
                             fd, ip_of (fd), RFIFOW (fd, 0), RFIFOREST (fd));
                    hexdump (unk_packets, RFIFOP (fd, 0), RFIFOREST (fd));
                    fputc ('\n', unk_packets);
                }
                login_log ("parse_fromchar: Unknown packet 0x%hu (from a char-server)! -> disconnection.\n",
                           RFIFOW (fd, 0));
                session[fd]->eof = 1;
                login_log ("Char-server has been disconnected (unknown packet).\n");
                return;
        } // switch packet
    } // while packets available
    return;
}



/// Server version
// uint16_t packet
void x7530 (int fd, bool ladmin)
{
    login_log ("%sRequest server version (ip: %s)\n", ladmin ? "'ladmin': " : "", ip_of (fd));
    WFIFOW (fd, 0) = 0x7531;
    if (ladmin)
    {
        WFIFOB (fd, 2) = ATHENA_MAJOR_VERSION;
        WFIFOB (fd, 3) = ATHENA_MINOR_VERSION;
        WFIFOB (fd, 4) = ATHENA_REVISION;
        WFIFOB (fd, 5) = ATHENA_RELEASE_FLAG;
        WFIFOB (fd, 6) = ATHENA_OFFICIAL_FLAG;
        WFIFOB (fd, 7) = ATHENA_SERVER_LOGIN;
        WFIFOW (fd, 8) = ATHENA_MOD_VERSION;
    }
    else // normal player
    {
        WFIFOB (fd, 2) = -1;
        WFIFOB (fd, 3) = PUBLIC_VERSION[0];
        WFIFOB (fd, 4) = PUBLIC_VERSION[1];
        WFIFOB (fd, 5) = PUBLIC_VERSION[2];
        // NOTE: this would be a good place to advertise features
        WFIFOL (fd, 6) = new_account_flag ? 1 : 0;
    }
    WFIFOSET (fd, 10);
}

/// Request of end of connection
// uint16_t packet
void x7532 (int fd, const char *pfx)
{
    login_log ("%sEnd of connection (ip: %s)\n", pfx, ip_of (fd));
    session[fd]->eof = 1;
}

/// Request list of accounts
// uint16_t packet, uint32_t start, uint32_t end
void x7920 (int fd)
{
    account_t st = RFIFOL (fd, 2);
    account_t ed = RFIFOL (fd, 6);
    WFIFOW (fd, 0) = 0x7921;
    if (st >= END_ACCOUNT_NUM)
        st = 0;
    if (ed > END_ACCOUNT_NUM || ed < st)
        ed = END_ACCOUNT_NUM;
    login_log ("'ladmin': List accounts from %d to %d (ip: %s)\n",
                st, ed, ip_of (fd));
    // Note: this is a vla
    int  id[auth_num];
    sort_accounts (id);
    // Sending accounts information
    uint16_t len = 4;
    for (int i = 0; i < auth_num && len < 30000; i++)
    {
        account_t account_id = auth_dat[id[i]].account_id;   // use sorted index
        if (account_id < st || account_id > ed)
            continue;
        WFIFOL (fd, len) = account_id;
        WFIFOB (fd, len + 4) = isGM (account_id);
        STRZCPY2 ((char *)WFIFOP (fd, len + 5), auth_dat[id[i]].userid);
        WFIFOB (fd, len + 29) = (uint8_t)auth_dat[id[i]].sex;
        WFIFOL (fd, len + 30) = auth_dat[id[i]].logincount;
        // if no state, but banished - FIXME can this happen?
        if (!auth_dat[id[i]].state == 0 && auth_dat[id[i]].ban_until_time)
            WFIFOL (fd, len + 34) = 7;
        else
            WFIFOL (fd, len + 34) = auth_dat[id[i]].state;
        len += 38;
    }
    WFIFOW (fd, 2) = len;
    WFIFOSET (fd, len);
}

/// Itemfrob: change ID of an existing item
// uint16_t packet, uint32_t old_id, uint32_t new_id
void x7924 (int fd)
{
    charif_sendallwos (-1, RFIFOP (fd, 0), 10); // forward package to char servers
    WFIFOW (fd, 0) = 0x7925;
    WFIFOSET (fd, 2);
}

/// Request for account creation
// uint16_t packet, char userid[24], char passwd[24], char sex, char email[40]
void x7930 (int fd)
{
    struct mmo_account ma;
    ma.userid = (char *)RFIFOP (fd, 2);
    ma.passwd = (char *)RFIFOP (fd, 26);
    STRZCPY (ma.lastlogin, "-");
    ma.sex = sex_from_char(RFIFOB (fd, 50));
    WFIFOW (fd, 0) = 0x7931;
    WFIFOL (fd, 2) = -1;
    strzcpy ((char *)WFIFOP (fd, 6), ma.userid, 24);
    if (strlen (ma.userid) < 4 || strlen (ma.passwd) < 4)
    {
        login_log ("'ladmin': Attempt to create an invalid account (account or pass is too short, ip: %s)\n",
                   ip_of (fd));
        return;
    }
    if (ma.sex != SEX_FEMALE && ma.sex != SEX_MALE)
    {
        login_log ("'ladmin': Attempt to create an invalid account (account: %s, invalid sex, ip: %s)\n",
                   ma.userid, ip_of (fd));
        return;
    }
    if (account_id_count > END_ACCOUNT_NUM)
    {
        login_log ("'ladmin': Attempt to create an account, but there is no more available id number (account: %s, sex: %c, ip: %s)\n",
                   ma.userid, sex_to_char(ma.sex), ip_of (fd));
        return;
    }
    remove_control_chars (ma.userid);
    remove_control_chars (ma.passwd);
    for (int i = 0; i < auth_num; i++)
    {
        if (strcmp (auth_dat[i].userid, ma.userid) == 0)
        {
            login_log ("'ladmin': Attempt to create an already existing account (account: %s, ip: %s)\n",
                       auth_dat[i].userid, ip_of (fd));
            return;
        }
    }
    char email[40] = {};
    STRZCPY (email, (char *)RFIFOP (fd, 51));
    remove_control_chars (email);
    int new_id = mmo_auth_new (&ma, email);
    login_log ("'ladmin': Account creation (account: %s (id: %d), sex: %c, email: %s, ip: %s)\n",
               ma.userid, new_id, sex_to_char(ma.sex), auth_dat[auth_num-1].email, ip_of (fd));
    WFIFOL (fd, 2) = new_id;
}

/// Request for an account deletion
// uint16_t packet, char userid[24]
void x7932 (int fd)
{
    WFIFOW (fd, 0) = 0x7933;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
        login_log ("'ladmin': Attempt to delete an unknown account (account: %s, ip: %s)\n",
                    account_name, ip_of (fd));
        return;
    }
    // Char-server is notified of deletion (for characters deletion).
    unsigned char buf[6];
    WBUFW (buf, 0) = 0x2730;
    WBUFL (buf, 2) = auth->account_id;
    charif_sendallwos (-1, buf, 6);
    // send answer
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);
    WFIFOL (fd, 2) = auth->account_id;
    // save deleted account in log file
    login_log ("'ladmin': Account deletion (account: %s, id: %d, ip: %s) - saved in next line:\n",
                auth->userid, auth->account_id, ip_of (fd));
    mmo_auth_to_file (logfp, auth);
    // delete account
    memset (auth->userid, '\0', sizeof (auth->userid));
    auth->account_id = -1;
}

/// Request to change password
// uint16_t packet, char userid[24], char passwd[24]
void x7934 (int fd)
{
    WFIFOW (fd, 0) = 0x7935;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
        login_log ("'ladmin': Attempt to modify the password of an unknown account (account: %s, ip: %s)\n",
                   account_name, ip_of (fd));
        return;
    }
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);
    STRZCPY (auth->pass, MD5_saltcrypt((char *)RFIFOP (fd, 26), make_salt()));
    WFIFOL (fd, 2) = auth->account_id;
    login_log ("'ladmin': Modification of a password (account: %s, new password: %s, ip: %s)\n",
               auth->userid, auth->pass, ip_of (fd));
}

/// Modify a state
// uint16_t packet, char userid[24], uint32_t state, char error_message[20]
// error_message is usually the date of the end of banishment
void x7936 (int fd)
{
    WFIFOW (fd, 0) = 0x7937;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    uint32_t status = RFIFOL (fd, 26);
    WFIFOL (fd, 30) = status;
    char error_message[20];
    STRZCPY (error_message, (char *)RFIFOP (fd, 30));
    remove_control_chars (error_message);
    if (status != 7 || error_message[0] == '\0')
        // 7: // 6 = Your are Prohibited to log in until %s
        STRZCPY (error_message, "-");
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
        login_log ("'ladmin': Attempt to modify the state of an unknown account (account: %s, received state: %d, ip: %s)\n",
                    account_name, status, ip_of (fd));
        return;
    }
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);
    WFIFOL (fd, 2) = auth->account_id;
    if (auth->state == status &&
            strcmp (auth->error_message, error_message) == 0)
    {
        login_log ("'ladmin': Modification of a state, but the state of the account is already the requested state (account: %s, received state: %d, ip: %s)\n",
                   account_name, status, ip_of (fd));
        return;
    }
    if (status == 7)
        login_log ("'ladmin': Modification of a state (account: %s, new state: %d - prohibited to login until '%s', ip: %s)\n",
                    auth->userid, status, error_message, ip_of (fd));
    else
        login_log ("'ladmin': Modification of a state (account: %s, new state: %d, ip: %s)\n",
                    auth->userid, status, ip_of (fd));
    if (auth->state == 0)
    {
        unsigned char buf[16];
        WBUFW (buf, 0) = 0x2731;
        WBUFL (buf, 2) = auth->account_id;
        WBUFB (buf, 6) = 0; // 0: change of state, 1: ban
        WBUFL (buf, 7) = status;    // status or final date of a banishment
        charif_sendallwos (-1, buf, 11);
        for (int j = 0; j < AUTH_FIFO_SIZE; j++)
            if (auth_fifo[j].account_id == auth->account_id)
                // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
                auth_fifo[j].login_id1++;
    }
    auth->state = (enum auth_failure)status;
    STRZCPY (auth->error_message, error_message);
}

/// Request for servers list and # of online players
// uint32_t packet
void x7938 (int fd)
{
    login_log ("'ladmin': Sending of servers list (ip: %s)\n", ip_of (fd));
    int server_num = 0;
    for (int i = 0; i < MAX_SERVERS; i++)
    {
        if (server_fd[i] < 0)
            continue;
        WFIFOL (fd, 4 + server_num * 32) = server[i].ip;
        WFIFOW (fd, 4 + server_num * 32 + 4) = server[i].port;
        STRZCPY2 ((char *)WFIFOP (fd, 4 + server_num * 32 + 6), server[i].name);
        WFIFOW (fd, 4 + server_num * 32 + 26) = server[i].users;
        WFIFOW (fd, 4 + server_num * 32 + 28) = server[i].maintenance;
        WFIFOW (fd, 4 + server_num * 32 + 30) = server[i].is_new;
        server_num++;
    }
    WFIFOW (fd, 0) = 0x7939;
    WFIFOW (fd, 2) = 4 + 32 * server_num;
    WFIFOSET (fd, 4 + 32 * server_num);
}

/// Request to check password
// uint16_t packet, char userid[24], char passwd[24]
void x793a (int fd)
{
    WFIFOW (fd, 0) = 0x793b;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
        login_log ("'ladmin': Attempt to check the password of an unknown account (account: %s, ip: %s)\n",
                   account_name, ip_of (fd));
        return;
    }
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);
    if (pass_ok((char *)RFIFOP (fd, 26), auth->pass))
    {
        WFIFOL (fd, 2) = auth->account_id;
        login_log ("'ladmin': Check of password OK (account: %s, password: %s, ip: %s)\n",
                    auth->userid, auth->pass, ip_of (fd));
    }
    else
    {
        char pass[24];
        STRZCPY (pass, (char *)RFIFOP (fd, 26));
        remove_control_chars (pass);
        login_log ("'ladmin': Failure of password check (account: %s, proposed pass: %s, ip: %s)\n",
                    auth->userid, pass, ip_of (fd));
    }
}

/// Request to modify sex
// uint32_t packet, char userid[24], char sex
void x793c (int fd)
{
    WFIFOW (fd, 0) = 0x793d;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
    enum gender sex = sex_from_char(RFIFOB (fd, 26));
    if (sex != SEX_FEMALE && sex != SEX_MALE)
    {
        if (RFIFOB (fd, 26) >= 0x20 && RFIFOB (fd, 26) < 0x7f)
            login_log ("'ladmin': Attempt to give an invalid sex (account: %s, received sex: %c, ip: %s)\n",
                        account_name, RFIFOB (fd, 26), ip_of (fd));
        else
            login_log ("'ladmin': Attempt to give an invalid sex (account: %s, received sex: %02hhx, ip: %s)\n",
                        account_name, RFIFOB (fd, 26), ip_of (fd));
        return;
    }
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        login_log ("'ladmin': Attempt to modify the sex of an unknown account (account: %s, received sex: %c, ip: %s)\n",
                   account_name, sex_to_char (sex), ip_of (fd));
        return;
    }
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);
    if (auth->sex == sex)
    {
        login_log ("'ladmin': Modification of a sex, but the sex is already the requested sex (account: %s, sex: %c, ip: %s)\n",
                   auth->userid, sex_to_char (sex), ip_of (fd));
        return;
    }
    WFIFOL (fd, 2) = auth->account_id;
    for (int j = 0; j < AUTH_FIFO_SIZE; j++)
        if (auth_fifo[j].account_id == auth->account_id)
            // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
            auth_fifo[j].login_id1++;
    auth->sex = sex;
    login_log ("'ladmin': Modification of a sex (account: %s, new sex: %c, ip: %s)\n",
               auth->userid, sex_to_char (sex), ip_of (fd));
    // send to all char-server the change
    unsigned char buf[7];
    WBUFW (buf, 0) = 0x2723;
    WBUFL (buf, 2) = auth->account_id;
    WBUFB (buf, 6) = (uint8_t)auth->sex;
    charif_sendallwos (-1, buf, 7);
}

/// Request to modify GM level
// uint16_t packet, char userid[24], uint8_t new_gm_level
void x793e (int fd)
{
    WFIFOW (fd, 0) = 0x793f;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
    gm_level_t new_gm_level = new_gm_level = RFIFOB (fd, 26);
    if (new_gm_level > 99)
    {
        login_log ("'ladmin': Attempt to give an invalid GM level (account: %s, received GM level: %d, ip: %s)\n",
                   account_name, (int) new_gm_level, ip_of (fd));
        return;
    }
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        login_log ("'ladmin': Attempt to modify the GM level of an unknown account (account: %s, received GM level: %d, ip: %s)\n",
                   account_name, (int) new_gm_level, ip_of (fd));
        return;
    }
    account_t acc = auth->account_id;
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);
    if (isGM (acc) == new_gm_level)
    {
        login_log ("'ladmin': Attempt to modify of a GM level, but the GM level is already the good GM level (account: %s (%u), GM level: %hhu, ip: %s)\n",
                   auth->userid, acc, new_gm_level, ip_of (fd));
        return;
    }
    // modification of the file
    int  lock;
    FILE *fp2 = lock_fopen (GM_account_filename, &lock);
    if (!fp2)
    {
        login_log ("'ladmin': Attempt to modify of a GM level - impossible to write GM accounts file (account: %s (%u), received GM level: %hhu, ip: %s)\n",
                   auth->userid, acc, new_gm_level, ip_of (fd));
        return;
    }
    FILE *fp = fopen_ (GM_account_filename, "r");
    if (!fp)
    {
        login_log ("'ladmin': Attempt to modify of a GM level - impossible to read GM accounts file (account: %s (%u), received GM level: %hhu, ip: %s)\n",
                   auth->userid, acc, new_gm_level, ip_of (fd));
        goto end_x793e_lock;
    }
    {
        struct timeval tv;
        gettimeofday (&tv, NULL);
        char tmpstr[DATE_FORMAT_MAX];
        strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime (&tv.tv_sec));
        bool modify_flag = 0;
        char line[512];
        // read/write GM file
        while (fgets (line, sizeof (line), fp))
        {
            // strip off both the '\r' and the '\n', if they exist
            // FIXME - isn't this file opened in text mode?
            while (line[0] != '\0' && (line[strlen (line) - 1] == '\n' || line[strlen (line) - 1] == '\r'))
                line[strlen (line) - 1] = '\0';
            // FIXME: won't this case be caught below, when sscanf fails?
            if ((line[0] == '/' && line[1] == '/') || line[0] == '\0')
            {
                fprintf (fp2, "%s\n", line);
                continue;
            }
            account_t GM_account;
            gm_level_t GM_level;
            if (sscanf (line, "%u %hhu", &GM_account, &GM_level) != 2
                && sscanf (line, "%u: %hhu", &GM_account, &GM_level) != 2)
            {
                fprintf (fp2, "%s\n", line);
                continue;
            }
            if (GM_account != acc)
            {
                fprintf (fp2, "%s\n", line);
                continue;
            }
            modify_flag = 1;
            if (!new_gm_level)
            {
                fprintf (fp2, "// %s: 'ladmin' remove %u '%s' GM level %hhu\n"
                                "//%d %d\n",
                            tmpstr, acc, auth->userid, GM_level,
                            acc, new_gm_level);
            }
            else
            {
                fprintf (fp2, "// %s: 'ladmin' change %u '%s' from GM level %hhu\n"
                              "%u %hhu\n",
                         tmpstr, acc, auth->userid, GM_level,
                         acc, new_gm_level);
            }
        }
        if (!modify_flag)
            fprintf (fp2, "// %s: 'ladmin' make %d '%s' a new GM\n"
                          "%d %d\n",
                     tmpstr, acc, auth->userid,
                     acc, new_gm_level);
        fclose_ (fp);
    }
end_x793e_lock:
    lock_fclose(fp2, GM_account_filename, &lock);
    WFIFOL (fd, 2) = acc;
    login_log ("'ladmin': Modification of a GM level (account: %s (%d), new GM level: %hhu, ip: %s)\n",
                auth->userid, acc, new_gm_level, ip_of (fd));
    // read and send new GM informations
    // FIXME: this is stupid
    read_gm_account ();
    send_GM_accounts ();
}

/// Request to modify e-mail
// uint16_t packet, char userid[24], char email[40]
void x7940 (int fd)
{
    WFIFOW (fd, 0) = 0x7941;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
    char email[40] = {};
    STRZCPY (email, (char *)RFIFOP (fd, 26));
    if (!e_mail_check (email))
    {
        login_log ("'ladmin': Attempt to give an invalid e-mail (account: %s, ip: %s)\n",
                   account_name, ip_of (fd));
        return;
    }
    remove_control_chars (email);
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        login_log ("'ladmin': Attempt to modify the e-mail of an unknown account (account: %s, received e-mail: %s, ip: %s)\n",
                   account_name, email, ip_of (fd));
        return;
    }
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);
    STRZCPY (auth->email, email);
    WFIFOL (fd, 2) = auth->account_id;
    login_log ("'ladmin': Modification of an email (account: %s, new e-mail: %s, ip: %s)\n",
               auth->userid, email, ip_of (fd));
}

/// Request to modify memo field
// uint16_t packet, char usercount[24], uint16_t msglen, char msg[msglen]
void x7942 (int fd)
{
    WFIFOW (fd, 0) = 0x7943;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
        login_log ("'ladmin': Attempt to modify the memo field of an unknown account (account: %s, ip: %s)\n",
                   account_name, ip_of (fd));
        return;
    }
    static const size_t size_of_memo = sizeof (auth->memo) - 1;
    // auth->memo[0] must always be '!' or stuff breaks
    char *memo = auth->memo + 1;
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);
    strzcpy (memo, (char *)RFIFOP (fd, 28), MIN (size_of_memo, RFIFOW (fd, 26)));
    remove_control_chars (memo);
    WFIFOL (fd, 2) = auth->account_id;
    login_log ("'ladmin': Modification of a memo field (account: %s, new memo: %s, ip: %s)\n",
                auth->userid, memo, ip_of (fd));
}

/// Find account id from name
// uint16_t packet, char userid[24]
void x7944 (int fd)
{
    WFIFOW (fd, 0) = 0x7945;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
        login_log ("'ladmin': ID request (by the name) of an unknown account (account: %s, ip: %s)\n",
                   account_name, ip_of (fd));
        return;
    }
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);
    WFIFOL (fd, 2) = auth->account_id;
    login_log ("'ladmin': Request (by the name) of an account id (account: %s, id: %u, ip: %s)\n",
               auth->userid, auth->account_id, ip_of (fd));
}

/// Find an account name from id
// uint16_t packet, uint32_t acc
void x7946 (int fd)
{
    WFIFOW (fd, 0) = 0x7947;
    WFIFOL (fd, 2) = RFIFOL (fd, 2);
    memset (WFIFOP (fd, 6), '\0', 24);
    struct auth_dat *auth = account_by_id(RFIFOL (fd, 2));
    if (!auth)
    {
        login_log ("'ladmin': Name request (by id) of an unknown account (id: %d, ip: %s)\n",
                   RFIFOL (fd, 2), ip_of (fd));
        // strcpy (WFIFOP (fd, 6), "");
        return;
    }
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);
    login_log ("'ladmin': Request (by id) of an account name (account: %s, id: %d, ip: %s)\n",
               auth->userid, RFIFOL (fd, 2), ip_of (fd));
}

/// Set the validity timestamp
// uint16_t packet, char userid[24], uint32_t timestamp
void x7948 (int fd)
{
    WFIFOW (fd, 0) = 0x7949;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    // time_t might not be 32 bits
    time_t timestamp = (time_t) RFIFOL (fd, 26);
    char tmpstr[DATE_FORMAT_MAX] = "unlimited";
    if (timestamp)
        strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime (&timestamp));
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
        login_log ("'ladmin': Attempt to change the validity limit of an unknown account (account: %s, received validity: %ld (%s), ip: %s)\n",
                   account_name, (long)timestamp, tmpstr, ip_of (fd));
        return;
    }
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);
    login_log ("'ladmin': Change of a validity limit (account: %s, new validity: %ld (%s), ip: %s)\n",
               auth->userid, (long)timestamp, tmpstr, ip_of (fd));
    auth->connect_until_time = timestamp;
    WFIFOL (fd, 2) = auth->account_id;
    WFIFOL (fd, 30) = timestamp;
}

/// Set the banishment timestamp
// uint16_t packet, char userid[24], uint32_t timestamp
void x794a (int fd)
{
    WFIFOW (fd, 0) = 0x794b;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    time_t timestamp = (time_t) RFIFOL (fd, 26);
    if (timestamp <= time (NULL))
        timestamp = 0;
    WFIFOL (fd, 30) = timestamp;
    char tmpstr[DATE_FORMAT_MAX] = "no banishment";
    if (timestamp)
        strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime (&timestamp));

    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
        login_log ("'ladmin': Attempt to change the final date of a banishment of an unknown account (account: %s, received final date of banishment: %ld (%s), ip: %s)\n",
                   account_name, (long)timestamp, tmpstr, ip_of (fd));
        return;
    }

    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);
    WFIFOL (fd, 2) = auth->account_id;
    login_log ("'ladmin': Change of the final date of a banishment (account: %s, new final date of banishment: %ld (%s), ip: %s)\n",
               auth->userid, (long)timestamp, tmpstr, ip_of (fd));
    if (auth->ban_until_time == timestamp)
        return;
    auth->ban_until_time = timestamp;
    if (!timestamp)
        return;
    uint8_t buf[11];
    WBUFW (buf, 0) = 0x2731;
    WBUFL (buf, 2) = auth->account_id;
    WBUFB (buf, 6) = 1; // 0: change of status, 1: ban
    WBUFL (buf, 7) = timestamp; // status or final date of a banishment
    charif_sendallwos (-1, buf, 11);
    for (int j = 0; j < AUTH_FIFO_SIZE; j++)
        if (auth_fifo[j].account_id == auth->account_id)
            // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
            auth_fifo[j].login_id1++;
}

/// Adjust the banishment end timestamp
// uint16_t packet, char userid[24], int16_t year,mon,day,hr,min,sec
void x794c (int fd)
{
    WFIFOW (fd, 0) = 0x794d;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
        login_log ("'ladmin': Attempt to adjust the final date of a banishment of an unknown account (account: %s, ip: %s)\n",
                   account_name, ip_of (fd));
        WFIFOL (fd, 30) = 0;
        return;
    }
    WFIFOL (fd, 2) = auth->account_id;
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);

    time_t timestamp = time (NULL);
    if (auth->ban_until_time >= timestamp)
        timestamp = auth->ban_until_time;

    // TODO check for overflow
    // years (365.25 days)
    timestamp += 31557600 * (short) RFIFOW (fd, 26);
    // a month isn't well-defined - use 1/12 of a year
    timestamp += 2629800 * (short) RFIFOW (fd, 28);
    timestamp += 86400 * (short) RFIFOW (fd, 30);
    timestamp += 3600 * (short) RFIFOW (fd, 32);
    timestamp += 60 * (short) RFIFOW (fd, 34);
    timestamp += (short) RFIFOW (fd, 36);
    if (timestamp <= time (NULL))
        timestamp = 0;

    char tmpstr[DATE_FORMAT_MAX] = "no banishment";
    if (timestamp)
        strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime (&timestamp));
    login_log ("'ladmin': Adjustment of a final date of a banishment (account: %s, (%+d y %+d m %+d d %+d h %+d mn %+d s) -> new validity: %ld (%s), ip: %s)\n",
               auth->userid,
               (short) RFIFOW (fd, 26), (short) RFIFOW (fd, 28),
               (short) RFIFOW (fd, 30), (short) RFIFOW (fd, 32),
               (short) RFIFOW (fd, 34), (short) RFIFOW (fd, 36),
               (long)timestamp, tmpstr, ip_of (fd));
    WFIFOL (fd, 30) = timestamp;
    if (auth->ban_until_time == timestamp)
        return;
    auth->ban_until_time = timestamp;
    if (!timestamp)
        return;
    unsigned char buf[11];
    WBUFW (buf, 0) = 0x2731;
    WBUFL (buf, 2) = auth->account_id;
    WBUFB (buf, 6) = 1; // 0: change of status, 1: ban
    WBUFL (buf, 7) = timestamp; // status or final date of a banishment
    charif_sendallwos (-1, buf, 11);
    for (int j = 0; j < AUTH_FIFO_SIZE; j++)
        if (auth_fifo[j].account_id == auth->account_id)
            // ?? to avoid reconnection error when come back from map-server (char-server will ask again the authentication)
            auth_fifo[j].login_id1++;
}

/// Broadcast a message
// uint16_t packet, uint16_t color, uint32_t msglen, char msg[msglen]
// color is not with TMW client, but eA had yellow == 0, else blue
void x794e (int fd)
{
    WFIFOW (fd, 0) = 0x794f;
    WFIFOW (fd, 2) = -1;
    if (!RFIFOL (fd, 4))
    {
        login_log ("'ladmin': Receiving a message for broadcast, but message is void (ip: %s)\n",
                   ip_of (fd));
        return;
    }
    // at least 1 char-server
    int i;
    for (i = 0; i < MAX_SERVERS; i++)
        if (server_fd[i] >= 0)
            break;
    if (i == MAX_SERVERS)
    {
        login_log ("'ladmin': Receiving a message for broadcast, but no char-server is online (ip: %s)\n",
                   ip_of (fd));
        return;
    }
    char *message = (char *)RFIFOP (fd, 8);
    WFIFOW (fd, 2) = 0;
    // This should already be NUL, but we don't trust anyone
    message[RFIFOL (fd, 4) - 1] = '\0';
    // Edit the message in-place
    remove_control_chars (message);
    login_log ("'ladmin': Relay broadcast %s (ip: %s)\n",
               message, ip_of (fd));
    // forward the same message to all char-servers (no answer)
    RFIFOW (fd, 0) = 0x2726;
    charif_sendallwos (-1, RFIFOP (fd, 0), 8 + RFIFOL (fd, 4));
}

/// Adjust the validity timestamp
// uint16_t packet, char userid[24], int16_t year,mon,day,hr,min,sec
void x7950 (int fd)
{
    WFIFOW (fd, 0) = 0x7951;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        strzcpy ((char *)WFIFOP (fd, 6), account_name, 24);
        login_log ("'ladmin': Attempt to adjust the validity limit of an unknown account (account: %s, ip: %s)\n",
                   account_name, ip_of (fd));
        WFIFOL (fd, 30) = 0;
        return;
    }
    WFIFOL (fd, 2) = auth->account_id;
    STRZCPY2 ((char *)WFIFOP (fd, 6), auth->userid);

    time_t timestamp = auth->connect_until_time;
    if (!add_to_unlimited_account && !timestamp)
    {
        login_log ("'ladmin': Refused to adjust the validity limit of an unlimited account (account: %s, ip: %s)\n",
                    auth->userid, ip_of (fd));
        WFIFOL (fd, 30) = 0;
        return;
    }
    if (timestamp == 0 || timestamp < time (NULL))
        timestamp = time (NULL);
    // TODO check for overflow
    // years (365.25 days)
    timestamp += 31557600 * (short) RFIFOW (fd, 26);
    // a month isn't well-defined - use 1/12 of a year
    timestamp += 2629800 * (short) RFIFOW (fd, 28);
    timestamp += 86400 * (short) RFIFOW (fd, 30);
    timestamp += 3600 * (short) RFIFOW (fd, 32);
    timestamp += 60 * (short) RFIFOW (fd, 34);
    timestamp += (short) RFIFOW (fd, 36);

    WFIFOL (fd, 30) = timestamp;

    char tmpstr[DATE_FORMAT_MAX] = "unlimited";
    if (auth->connect_until_time)
        strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT,
                  gmtime (&auth->connect_until_time));

    if (timestamp == auth->connect_until_time)
    {
        login_log ("'ladmin': No adjustment of a validity limit (account: %s, %ld (%s), ip: %s)\n",
                   auth->userid,
                   (long)auth->connect_until_time, tmpstr, ip_of (fd));
        return;
    }

    char tmpstr2[DATE_FORMAT_MAX] = "unlimited";
    if (timestamp)
        strftime (tmpstr2, DATE_FORMAT_MAX, DATE_FORMAT, gmtime (&timestamp));
    login_log ("'ladmin': Adjustment of a validity limit (account: %s, %ld (%s) + (%+d y %+d m %+d d %+d h %+d mn %+d s) -> new validity: %ld (%s), ip: %s)\n",
               auth->userid,
               (long)auth->connect_until_time, tmpstr,
               (short) RFIFOW (fd, 26), (short) RFIFOW (fd, 28),
               (short) RFIFOW (fd, 30), (short) RFIFOW (fd, 32),
               (short) RFIFOW (fd, 34), (short) RFIFOW (fd, 36),
               (long)timestamp, tmpstr2, ip_of (fd));
    auth->connect_until_time = timestamp;
}

void ladmin_reply_account_info (int fd, struct auth_dat *auth)
{
    WFIFOL (fd, 2) = auth->account_id;
    WFIFOB (fd, 6) = isGM (auth->account_id);
    STRZCPY2 ((char *)WFIFOP (fd, 7), auth->userid);
    WFIFOB (fd, 31) = (uint8_t)auth->sex;
    WFIFOL (fd, 32) = auth->logincount;
    WFIFOL (fd, 36) = auth->state;
    STRZCPY2 ((char *)WFIFOP (fd, 40), auth->error_message);
    STRZCPY2 ((char *)WFIFOP (fd, 60), auth->lastlogin);
    STRZCPY2 ((char *)WFIFOP (fd, 84), auth->last_ip);
    STRZCPY2 ((char *)WFIFOP (fd, 100), auth->email);
    WFIFOL (fd, 140) = auth->connect_until_time;
    WFIFOL (fd, 144) = auth->ban_until_time;
    // discard the password magic
    char *memo = auth->memo + 1;
    WFIFOW (fd, 148) = strlen (memo);
    strzcpy ((char *)WFIFOP (fd, 150), memo, sizeof(auth->memo)-1);
    WFIFOSET (fd, 150 + strlen (memo));
}
/// Account information by name
// uint16_t packet, char userid[24]
// FIXME reduce duplication
void x7952 (int fd)
{
    WFIFOW (fd, 0) = 0x7953;
    WFIFOL (fd, 2) = -1;
    char *account_name = (char *)RFIFOP (fd, 2);
    account_name[23] = '\0';
    remove_control_chars (account_name);
    struct auth_dat *auth = account_by_name (account_name);
    if (!auth)
    {
        strzcpy ((char *)WFIFOP (fd, 7), account_name, 24);
        WFIFOW (fd, 148) = 0;
        login_log ("'ladmin': No account information for name: %s (ip: %s)\n",
                   account_name, ip_of (fd));
        WFIFOSET (fd, 150);
        return;
    }
    login_log ("'ladmin': Sending information of an account (request by the name; account: %s, id: %d, ip: %s)\n",
               auth->userid, auth->account_id, ip_of (fd));
    ladmin_reply_account_info (fd, auth);
}

/// Account information by id
// uint16_t packet, uint32_t acc
// FIXME reduce duplication
void x7954 (int fd)
{
    WFIFOW (fd, 0) = 0x7953;
    WFIFOL (fd, 2) = RFIFOL (fd, 2);
    struct auth_dat *auth = account_by_id (RFIFOL (fd, 2));
    if (!auth)
    {
        login_log ("'ladmin': Attempt to obtain information (by the id) of an unknown account (id: %d, ip: %s)\n",
                   RFIFOL (fd, 2), ip_of (fd));
        memset (WFIFOP (fd, 7), 0, 24);
        WFIFOW (fd, 148) = 0;
        WFIFOSET (fd, 150);
        return;
    }
    login_log ("'ladmin': Sending information of an account (request by the id; account: %s, id: %d, ip: %s)\n",
               auth->userid, RFIFOL (fd, 2), ip_of (fd));
    ladmin_reply_account_info (fd, auth);
}

/// Request to reload GM file (no answer)
// uint16_t packet
void x7955 (int fd)
{
    login_log ("'ladmin': Request to re-load GM configuration file (ip: %s).\n",
               ip_of (fd));
    read_gm_account ();
    // send GM accounts to all char-servers
    send_GM_accounts ();
}


/// Parse packets from an administration login
void parse_admin (int fd)
{
    if (session[fd]->eof)
    {
        close (fd);
        delete_session (fd);
        login_log ("Remote administration has disconnected (session #%d).\n", fd);
        return;
    }

    while (RFIFOREST (fd) >= 2)
    {
        if (display_parse_admin)
            login_log ("%s: connection #%d, packet: 0x%x (with %d bytes).\n",
                       __func__, fd, RFIFOW (fd, 0), RFIFOREST (fd));

        switch (RFIFOW (fd, 0))
        {
            /// Request of the server version
            // uint16_t packet
            case 0x7530:
                x7530 (fd, true);
                RFIFOSKIP (fd, 2);
                break;

            /// Request of end of connection
            // uint16_t packet
            case 0x7532:
                x7532 (fd, "'ladmin': ");
                RFIFOSKIP (fd, 2);
                return;

            /// Request list of accounts
            // uint16_t packet, uint32_t start, uint32_t end
            case 0x7920:
                if (RFIFOREST (fd) < 10)
                    return;
                x7920 (fd);
                RFIFOSKIP (fd, 10);
                break;

            /// Itemfrob: change ID of an existing item
            // uint16_t packet, uint32_t old_id, uint32_t new_id
            case 0x7924:
                if (RFIFOREST (fd) < 10)
                    return;
                x7924 (fd);
                RFIFOSKIP (fd, 10);
                break;

            /// Request for account creation
            // uint16_t packet, char userid[24], char passwd[24], char sex, char email[40]
            case 0x7930:
                if (RFIFOREST (fd) < 91)
                    return;
                x7930 (fd);
                WFIFOSET (fd, 30);
                RFIFOSKIP (fd, 91);
                break;

            /// Request for an account deletion
            // uint16_t packet, char userid[24]
            case 0x7932:
                if (RFIFOREST (fd) < 26)
                    return;
                x7932 (fd);
                WFIFOSET (fd, 30);
                RFIFOSKIP (fd, 26);
                break;

            /// Request to change password
            // uint16_t packet, char userid[24], char passwd[24]
            case 0x7934:
                if (RFIFOREST (fd) < 50)
                    return;
                x7934 (fd);
                WFIFOSET (fd, 30);
                RFIFOSKIP (fd, 50);
                break;

            /// Modify a state
            // uint16_t packet, char userid[24], uint32_t state, char error_message[20]
            // error_message is usually the date of the end of banishment
            case 0x7936:
                if (RFIFOREST (fd) < 50)
                    return;
                x7936 (fd);
                WFIFOSET (fd, 34);
                RFIFOSKIP (fd, 50);
                break;

            /// Request for servers list and # of online players
            // uint32_t packet
            case 0x7938:
                x7938 (fd);
                RFIFOSKIP (fd, 2);
                break;

            /// Request to check password
            // uint16_t packet, char userid[24], char passwd[24]
            case 0x793a:
                if (RFIFOREST (fd) < 50)
                    return;
                x793a (fd);
                WFIFOSET (fd, 30);
                RFIFOSKIP (fd, 50);
                break;

            /// Request to modify sex
            // uint32_t packet, char userid[24], char sex
            case 0x793c:
                if (RFIFOREST (fd) < 27)
                    return;
                x793c (fd);
                WFIFOSET (fd, 30);
                RFIFOSKIP (fd, 27);
                break;

            /// Request to modify GM level
            // uint16_t packet, char userid[24], uint8_t new_gm_level
            case 0x793e:
                if (RFIFOREST (fd) < 27)
                    return;
                x793e (fd);
                WFIFOSET (fd, 30);
                RFIFOSKIP (fd, 27);
                break;

            /// Request to modify e-mail
            // uint16_t packet, char userid[24], char email[40]
            case 0x7940:
                if (RFIFOREST (fd) < 66)
                    return;
                x7940 (fd);
                WFIFOSET (fd, 30);
                RFIFOSKIP (fd, 66);
                break;

            /// Request to modify memo field
            // uint16_t packet, char usercount[24], uint16_t msglen, char msg[msglen]
            case 0x7942:
                if (RFIFOREST (fd) < 28
                    || RFIFOREST (fd) < (28 + RFIFOW (fd, 26)))
                    return;
                x7942 (fd);
                WFIFOSET (fd, 30);
                RFIFOSKIP (fd, 28 + RFIFOW (fd, 26));
                break;

            /// Find account id from name
            // uint16_t packet, char userid[24]
            case 0x7944:
                if (RFIFOREST (fd) < 26)
                    return;
                x7944 (fd);
                WFIFOSET (fd, 30);
                RFIFOSKIP (fd, 26);
                break;

            /// Find an account name from id
            // uint16_t packet, uint32_t acc
            case 0x7946:
                if (RFIFOREST (fd) < 6)
                    return;
                x7946 (fd);
                WFIFOSET (fd, 30);
                RFIFOSKIP (fd, 6);
                break;

            /// Set the validity timestamp
            // uint16_t packet, char userid[24], uint32_t timestamp
            case 0x7948:
                if (RFIFOREST (fd) < 30)
                    return;
                x7948 (fd);
                WFIFOSET (fd, 34);
                RFIFOSKIP (fd, 30);
                break;

            /// Set the banishment timestamp
            // uint16_t packet, char userid[24], uint32_t timestamp
            case 0x794a:
                if (RFIFOREST (fd) < 30)
                    return;
                x794a (fd);
                WFIFOSET (fd, 34);
                RFIFOSKIP (fd, 30);
                break;

            /// Adjust the banishment end timestamp
            // uint16_t packet, char userid[24], int16_t year,mon,day,hr,min,sec
            case 0x794c:
                if (RFIFOREST (fd) < 38)
                    return;
                x794c (fd);
                WFIFOSET (fd, 34);
                RFIFOSKIP (fd, 38);
                break;

            /// Broadcast a message
            // uint16_t packet, uint16_t color, uint32_t msglen, char msg[msglen]
            // color is not with TMW client, but eA had yellow == 0, else blue
            case 0x794e:
                if (RFIFOREST (fd) < 8 || RFIFOREST (fd) < (8 + RFIFOL (fd, 4)))
                    return;
                x794e (fd);
                WFIFOSET (fd, 4);
                RFIFOSKIP (fd, 8 + RFIFOL (fd, 4));
                break;

            /// Adjust the validity timestamp
            // uint16_t packet, char userid[24], int16_t year,mon,day,hr,min,sec
            case 0x7950:
                if (RFIFOREST (fd) < 38)
                    return;
                x7950 (fd);
                WFIFOSET (fd, 34);
                RFIFOSKIP (fd, 38);
                break;

            /// Account information by name
            // uint16_t packet, char userid[24]
            // FIXME reduce duplication
            case 0x7952:
                if (RFIFOREST (fd) < 26)
                    return;
                x7952 (fd);
                RFIFOSKIP (fd, 26);
                break;

            /// Account information by id
            // uint16_t packet, uint32_t acc
            // FIXME reduce duplication
            case 0x7954:
                if (RFIFOREST (fd) < 6)
                    return;
                x7954 (fd);
                RFIFOSKIP (fd, 6);
                break;

            /// Request to reload GM file (no answer)
            // uint16_t packet
            case 0x7955:
                x7955 (fd);
                RFIFOSKIP (fd, 2);
                break;

            default:
                if (!unk_packets)
                    unk_packets = fopen_ (login_log_unknown_packets_filename, "a");
                if (unk_packets)
                {
                    struct timeval tv;
                    gettimeofday (&tv, NULL);
                    char tmpstr[DATE_FORMAT_MAX];
                    strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime (&tv.tv_sec));
                    fprintf (unk_packets,
                             "%s.%03d: receiving of an unknown packet -> disconnection\n",
                             tmpstr, (int) tv.tv_usec / 1000);
                    fprintf (unk_packets,
                             "parse_admin: connection #%d (ip: %s), packet: 0x%x (with %u bytes available).\n",
                             fd, ip_of (fd), RFIFOW (fd, 0), RFIFOREST (fd));
                    hexdump (unk_packets, RFIFOP (fd, 0), RFIFOREST (fd));
                    fputc ('\n', unk_packets);
                }
                login_log ("'ladmin': End of connection, unknown packet (ip: %s)\n",
                           ip_of (fd));
                session[fd]->eof = 1;
                return;
        } // switch packet
    } // while packet available
    return;
}




/// Check if IP is LAN instead of WAN
// (send alternate char IP)
bool lan_ip_check (uint8_t *p)
{
    bool lancheck = 1;
    for (int i = 0; i < 4; i++)
    {
        if ((subneti[i] & subnetmaski[i]) != (p[i] & subnetmaski[i]))
        {
            lancheck = 0;
            break;
        }
    }
    printf ("LAN test (result): %s source\033[0m.\n",
            (lancheck) ? "\033[1;36mLAN" : "\033[1;32mWAN");
    return lancheck;
}




/// Client is alive
// uint16_t packet, char userid[24]
// this packet is not sent by any known TMW server/client
void x200(int UNUSED)
{
}

/// Client is alive
// uint16_t packet, char crypted_userid[16]
// (new ragexe from 22 june 2004)
// this packet is not sent by any known TMW server/client
void x204 (int UNUSED)
{
}

/// Client connect
// uint16_t packet, uint8_t unk[4], char userid[24], char passwd[24], uint8_t version_2_flags
void x64 (int fd)
{
    struct mmo_account account;
    account.userid = (char *)RFIFOP (fd, 6);
    account.userid[23] = '\0';
    remove_control_chars (account.userid);
    account.passwd = (char *)RFIFOP (fd, 30);
    account.passwd[23] = '\0';
    remove_control_chars (account.passwd);

    login_log ("Request for connection of %s (ip: %s).\n",
               account.userid, ip_of (fd));

    if (!check_ip (session[fd]->client_addr.sin_addr.s_addr))
    {
        login_log ("Connection refused: IP isn't authorised (deny/allow, ip: %s).\n",
                   ip_of (fd));
        WFIFOW (fd, 0) = 0x6a;
        WFIFOB (fd, 2) = 0x03;
        WFIFOSET (fd, 3);
        // FIXME: shouldn't this set eof?
        return;
    }

    enum auth_failure result = mmo_auth (&account, fd);
    // putting the version_2_flags here feels hackish
    // but it makes the code much nicer, especially for future
    uint8_t version_2_flags = RFIFOB (fd, 54);
    // As an update_host is required for all known TMW servers,
    // and all clients since 0.0.25 support it,
    // I am making it fail if not set
    if (!(version_2_flags & VERSION_2_UPDATEHOST))
        result = AUTH_CLIENT_TOO_OLD;
    if (result != AUTH_OK)
    {
        WFIFOW (fd, 0) = 0x6a;
        WFIFOB (fd, 2) = result - 1;
        memset (WFIFOP (fd, 3), '\0', 20);
        struct auth_dat *auth;
        if (result != AUTH_BANNED_TEMPORARILY)
            goto end_x0064_006a;
        // You are Prohibited to log in until %s
        auth = account_by_name (account.userid);
        // This cannot happen
        // if (i == -1)
            // goto end_x0064_006a;
        if (auth->ban_until_time)
        {
            // if account is banned, we send ban timestamp
            char tmpstr[DATE_FORMAT_MAX];
            strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT,
                      gmtime (&auth->ban_until_time));
            STRZCPY2 ((char *)WFIFOP (fd, 3), tmpstr);
        }
        else
        {
            // can this happen?
            // we send error message
            // hm, it seems there is a ladmin command to set this arbitrarily
            STRZCPY2 ((char *)WFIFOP (fd, 3), auth->error_message);
        }
    end_x0064_006a:
        WFIFOSET (fd, 23);
        return;
    }
    gm_level_t gm_level = isGM (account.account_id);
    if (min_level_to_connect > gm_level)
    {
        login_log ("Connection refused: only allowing GMs of level %hhu (account: %s, GM level: %d, ip: %s).\n",
                   min_level_to_connect, account.userid, gm_level, ip_of (fd));
        WFIFOW (fd, 0) = 0x81;
        WFIFOL (fd, 2) = 1; // 01 = Server closed
        WFIFOSET (fd, 3);
        return;
    }

    if (gm_level)
        login_log ("Connection of the GM (level:%d) account '%s' accepted.\n",
                   gm_level, account.userid);
    else
        login_log ("Connection of the account '%s' accepted.\n",
                   account.userid);

    /// If there is an update_host, send it
    // (version_2_flags requires bit 0, above)
    size_t host_len = strlen (update_host);
    if (host_len)
    {
        WFIFOW (fd, 0) = 0x63;
        WFIFOW (fd, 2) = 4 + host_len;
        memcpy (WFIFOP (fd, 4), update_host, host_len);
        WFIFOSET (fd, 4 + host_len);
    }

    // Load list of char servers into outbound packet
    int server_num = 0;
    // This has been set since 0.0.29
    // and it will only be an inconvenience for older clients,
    // so always send in forward-order.
    // if (version_2_flags & VERSION_2_SERVERORDER)
    for (int i = 0; i < MAX_SERVERS; i++)
    {
        if (server_fd[i] < 0)
            continue;
        if (lan_ip_check ((uint8_t *) &session[fd]->client_addr.sin_addr))
            WFIFOL (fd, 47 + server_num * 32) = inet_addr (lan_char_ip);
        else
            WFIFOL (fd, 47 + server_num * 32) = server[i].ip;
        WFIFOW (fd, 47 + server_num * 32 + 4) = server[i].port;
        STRZCPY2 ((char *)WFIFOP (fd, 47 + server_num * 32 + 6), server[i].name);
        WFIFOW (fd, 47 + server_num * 32 + 26) = server[i].users;
        WFIFOW (fd, 47 + server_num * 32 + 28) = server[i].maintenance;
        WFIFOW (fd, 47 + server_num * 32 + 30) = server[i].is_new;
        server_num++;
    }
    // if no char-server, don't send void list of servers, just disconnect the player with proper message
    if (!server_num)
    {
        login_log ("Connection refused: there is no char-server online (account: %s, ip: %s).\n",
                   account.userid, ip_of (fd));
        WFIFOW (fd, 0) = 0x81;
        WFIFOL (fd, 2) = 1; // 01 = Server closed
        WFIFOSET (fd, 3);
        return;
    }
    // if at least 1 char-server
    WFIFOW (fd, 0) = 0x69;
    WFIFOW (fd, 2) = 47 + 32 * server_num;
    WFIFOL (fd, 4) = account.login_id1;
    WFIFOL (fd, 8) = account.account_id;
    WFIFOL (fd, 12) = account.login_id2;
    /// in old eAthena, this was for an ip
    WFIFOL (fd, 16) = 0;
    /// in old eAthena, this was for a name
    STRZCPY2 ((char *)WFIFOP (fd, 20), account.lastlogin);
    // nothing is written in the word at 44
    WFIFOB (fd, 46) = (uint8_t)account.sex;
    WFIFOSET (fd, 47 + 32 * server_num);
    if (auth_fifo_pos >= AUTH_FIFO_SIZE)
        auth_fifo_pos = 0;
    // wait, is this just blithely wrapping and invalidating old entries?
    // this could be a cause of DoS attacks
    auth_fifo[auth_fifo_pos].account_id = account.account_id;
    auth_fifo[auth_fifo_pos].login_id1 = account.login_id1;
    auth_fifo[auth_fifo_pos].login_id2 = account.login_id2;
    auth_fifo[auth_fifo_pos].sex = account.sex;
    auth_fifo[auth_fifo_pos].delflag = 0;
    auth_fifo[auth_fifo_pos].ip = session[fd]->client_addr.sin_addr.s_addr;
    auth_fifo_pos++;
}

/// Sending request of the coding key (ladmin packet)
// uint16_t packet
void x791a (int fd)
{
    if (session[fd]->session_data)
    {
        login_log ("login: abnormal request of MD5 key (already opened session).\n");
        session[fd]->eof = 1;
        return;
    }
    struct login_session_data *ld;
    CREATE (ld, struct login_session_data, 1);
    session[fd]->session_data = ld;
    login_log ("'ladmin': Sending request of the coding key (ip: %s)\n",
               ip_of (fd));
    // Create coding key of length [12, 16)
    ld->md5keylen = MPRAND(12, 4);
    for (int i = 0; i < ld->md5keylen; i++)
        ld->md5key[i] = MPRAND(1, 255);

    RFIFOSKIP (fd, 2);
    WFIFOW (fd, 0) = 0x01dc;
    WFIFOW (fd, 2) = 4 + ld->md5keylen;
    memcpy (WFIFOP (fd, 4), ld->md5key, ld->md5keylen);
    WFIFOSET (fd, WFIFOW (fd, 2));
}

/// char-server connects
// uint16_t packet, char userid[24], char passwd[24], char unk[4],
//   uint32_t ip, uint16_t port, char server_name[20],
//   char unk[2], uint16_t maintenance, uint16_t is_new
void x2710 (int fd)
{
    struct mmo_account account;
    account.userid = (char *)RFIFOP (fd, 2);
    account.userid[23] = '\0';
    remove_control_chars (account.userid);
    account.passwd = (char *)RFIFOP (fd, 26);
    account.passwd[23] = '\0';
    remove_control_chars (account.passwd);
    char *server_name = (char *)RFIFOP (fd, 60);
    server_name[19] = '\0';
    remove_control_chars (server_name);
    login_log ("Connection request of the char-server '%s' @ %d.%d.%d.%d:%d (ip: %s)\n",
               server_name, RFIFOB (fd, 54), RFIFOB (fd, 55),
               RFIFOB (fd, 56), RFIFOB (fd, 57), RFIFOW (fd, 58), ip_of (fd));
    enum auth_failure result = mmo_auth (&account, fd);

    if (result == AUTH_OK && account.sex == SEX_SERVER)
    {
        // If this is the main server, and we don't already have a main server
        if (server_fd[0] == -1
            && strcasecmp (server_name, main_server) == 0)
        {
            account.account_id = 0;
            goto char_server_ok;
        }
        for (int i = 1; i < MAX_SERVERS; i++)
        {
            if (server_fd[i] == -1)
            {
                account.account_id = i;
                goto char_server_ok;
            }
        }
    }
    login_log ("Connection of the char-server '%s' REFUSED (account: %s, pass: %s, ip: %s)\n",
               server_name, account.userid,
               account.passwd, ip_of (fd));
    WFIFOW (fd, 0) = 0x2711;
    WFIFOB (fd, 2) = 3;
    WFIFOSET (fd, 3);
    return;

char_server_ok:
    login_log ("Connection of the char-server '%s' accepted (account: %s, pass: %s, ip: %s)\n",
               server_name, account.userid, account.passwd, ip_of (fd));
    server[account.account_id].ip = RFIFOL (fd, 54);
    server[account.account_id].port = RFIFOW (fd, 58);
    STRZCPY (server[account.account_id].name, server_name);
    server[account.account_id].users = 0;
    server[account.account_id].maintenance = RFIFOW (fd, 82);
    server[account.account_id].is_new = RFIFOW (fd, 84);
    server_fd[account.account_id] = fd;
    if (anti_freeze_enable)
        // Char-server anti-freeze system. Counter. 5 ok, 4...0 freezed
        server_freezeflag[account.account_id] = 5;
    WFIFOW (fd, 0) = 0x2711;
    WFIFOB (fd, 2) = 0;
    WFIFOSET (fd, 3);
    session[fd]->func_parse = parse_fromchar;
    realloc_fifo (fd, FIFOSIZE_SERVERLINK, FIFOSIZE_SERVERLINK);
    // send GM account list to char-server
    uint16_t len = 4;
    WFIFOW (fd, 0) = 0x2732;
    for (int i = 0; i < auth_num; i++)
    {
        gm_level_t GM_value = isGM (auth_dat[i].account_id);
        // send only existing accounts. We can not create a GM account when server is online.
        if (GM_value)
        {
            WFIFOL (fd, len) = auth_dat[i].account_id;
            WFIFOB (fd, len + 4) = GM_value;
            len += 5;
        }
    }
    WFIFOW (fd, 2) = len;
    WFIFOSET (fd, len);
}

/// Request for administation login
// uint16_t packet, uint16_t type = {0,1,2}, char passwd[24] (if type 0) or uint8_t hash[16] otherwise
// ladmin always sends the encrypted form
void x7918 (int fd)
{
    WFIFOW (fd, 0) = 0x7919;
    WFIFOB (fd, 2) = 1;
    if (!check_ladminip (session[fd]->client_addr.sin_addr.s_addr))
    {
        login_log ("'ladmin'-login: Connection in administration mode refused: IP isn't authorised (ladmin_allow, ip: %s).\n",
                   ip_of (fd));
        return;
    }
    struct login_session_data *ld = (struct login_session_data *)session[fd]->session_data;
    if (RFIFOW (fd, 2) == 0)
    {
        login_log ("'ladmin'-login: Connection in administration mode refused: not encrypted (ip: %s).\n",
                   ip_of (fd));
        return;
    }
    if (RFIFOW (fd, 2) > 2)
    {
        login_log ("'ladmin'-login: Connection in administration mode refused: unknown encryption (ip: %s).\n",
                   ip_of (fd));
        return;
    }
    if (!ld)
    {
        login_log ("'ladmin'-login: error! MD5 key not created/requested for an administration login.\n");
        return;
    }
    if (!admin_state)
    {
        login_log ("'ladmin'-login: Connection in administration mode refused: remote administration is disabled (ip: %s)\n",
                   ip_of (fd));
        return;
    }
    char md5str[64] = {};
    uint8_t md5bin[32];
    if (RFIFOW (fd, 2) == 1)
    {
        strncpy (md5str, ld->md5key, sizeof (ld->md5key));  // 20
        strcat (md5str, admin_pass);    // 24
    }
    else if (RFIFOW (fd, 2) == 2)
    {
        // This is always sent by ladmin
        strncpy (md5str, admin_pass, sizeof (admin_pass));  // 24
        strcat (md5str, ld->md5key);    // 20
    }
    MD5_to_bin(MD5_from_cstring(md5str), md5bin);
    // If password hash sent by client matches hash of password read from login server configuration file
    if (memcmp (md5bin, RFIFOP (fd, 4), 16) == 0)
    {
        login_log ("'ladmin'-login: Connection in administration mode accepted (encrypted password, ip: %s)\n",
                   ip_of (fd));
        WFIFOB (fd, 2) = 0;
        session[fd]->func_parse = parse_admin;
    }
    else
        login_log ("'ladmin'-login: Connection in administration mode REFUSED - invalid password (encrypted password, ip: %s)\n",
                   ip_of (fd));
}



/// Default packet parsing
// * normal players
// * administation/char-server before authenticated
void parse_login (int fd)
{
    if (session[fd]->eof)
    {
        close (fd);
        delete_session (fd);
        return;
    }

    while (RFIFOREST (fd) >= 2)
    {
        if (display_parse_login)
        {
#if 0
// This information is useless - better available below
// and is not safe (it might not be NUL-terminated)
            if (RFIFOW (fd, 0) == 0x64)
            {
                if (RFIFOREST (fd) >= 55)
                    printf ("parse_login: connection #%d, packet: 0x%x (with being read: %d), account: %s.\n",
                            fd, RFIFOW (fd, 0), RFIFOREST (fd), RFIFOP (fd, 6));
            }
            else if (RFIFOW (fd, 0) == 0x2710)
            {
                if (RFIFOREST (fd) >= 86)
                    printf ("parse_login: connection #%d, packet: 0x%x (with being read: %d), server: %s.\n",
                            fd, RFIFOW (fd, 0), RFIFOREST (fd), RFIFOP (fd, 60));
            }
            else
#endif
                login_log ("%s: connection #%d, packet: 0x%hx (with %u bytes).\n",
                           __func__, fd, RFIFOW (fd, 0), RFIFOREST (fd));
        }

        switch (RFIFOW (fd, 0))
        {
            /// Client is alive
            // uint16_t packet, char userid[24]
            // this packet is not sent by any known TMW server/client
            case 0x200:
                if (RFIFOREST (fd) < 26)
                    return;
                x200 (fd);
                RFIFOSKIP (fd, 26);
                break;

            /// Client is alive
            // uint16_t packet, char crypted_userid[16]
            // (new ragexe from 22 june 2004)
            // this packet is not sent by any known TMW server/client
            case 0x204:
                if (RFIFOREST (fd) < 18)
                    return;
                x204 (fd);
                RFIFOSKIP (fd, 18);
                break;

            /// Client connect
            // uint16_t packet, uint8_t unk[4], char userid[24], char passwd[24], uint8_t version_2_flags
            case 0x64:
                if (RFIFOREST (fd) < 55 )
                    return;
                x64 (fd);
                RFIFOSKIP (fd, 55);
                break;

            /// Sending request of the coding key (ladmin packet)
            // uint16_t packet
            case 0x791a:
                x791a (fd);
                RFIFOSKIP (fd, 2);
                break;

            /// char-server connects
            // uint16_t packet, char userid[24], char passwd[24], char unk[4],
            //   uint32_t ip, uint16_t port, char server_name[20],
            //   char unk[2], uint16_t maintenance, uint16_t is_new
            case 0x2710:
                if (RFIFOREST (fd) < 86)
                    return;
                x2710 (fd);
                RFIFOSKIP (fd, 86);
                return;

            /// Server version
            // uint16_t packet
            case 0x7530:
                x7530 (fd, false);
                RFIFOSKIP (fd, 2);
                break;

            /// End connection
            case 0x7532:
                x7532 (fd,"");
                return;

            /// Request for administation login
            // uint16_t packet, uint16_t type = {0,1,2}, char passwd[24] (if type 0) or uint8_t hash[16] otherwise
            // ladmin always sends the encrypted form
            case 0x7918:
                if (RFIFOREST (fd) < 4
                    || RFIFOREST (fd) < ((RFIFOW (fd, 2) == 0) ? 28 : 20))
                    return;
                x7918 (fd);
                WFIFOSET (fd, 3);
                RFIFOSKIP (fd, (RFIFOW (fd, 2) == 0) ? 28 : 20);
                break;

            default:
                if (!save_unknown_packets)
                    goto end_default;
                if (!unk_packets)
                    unk_packets = fopen_ (login_log_unknown_packets_filename, "a");
                if (!unk_packets)
                    goto end_default;
                struct timeval tv;
                gettimeofday (&tv, NULL);
                char tmpstr[DATE_FORMAT_MAX];
                strftime (tmpstr, DATE_FORMAT_MAX, DATE_FORMAT, gmtime (&tv.tv_sec));
                fprintf (unk_packets,
                        "%s.%03d: receiving of an unknown packet -> disconnection\n",
                        tmpstr, (int) tv.tv_usec / 1000);
                fprintf (unk_packets,
                        "parse_login: connection #%d (ip: %s), packet: 0x%x (with being read: %d).\n",
                        fd, ip_of (fd), RFIFOW (fd, 0),
                        RFIFOREST (fd));

                hexdump (unk_packets, RFIFOP (fd, 0), RFIFOREST (fd));
                fputc ('\n', unk_packets);
            end_default:
                login_log ("End of connection, unknown packet (ip: %s)\n",
                           ip_of (fd));
                session[fd]->eof = 1;
                return;
        }
    }
    return;
}

/// Convert string to number
// Parses booleans: on/off and yes/no in english, français, deutsch, español
// Then falls back to atoi (which means non-integers are parsed as 0)
// TODO move this to common, as it is used by other servers/ladmin
// TODO replace by config_parse_bool and config_parse_int?
int config_switch (const char *str)
{
    if (strcasecmp (str, "on") == 0 || strcasecmp (str, "yes") == 0
        || strcasecmp (str, "oui") == 0 || strcasecmp (str, "ja") == 0
        || strcasecmp (str, "si") == 0)
        return 1;
    if (strcasecmp (str, "off") == 0 || strcasecmp (str, "no") == 0
        || strcasecmp (str, "non") == 0 || strcasecmp (str, "nein") == 0)
        return 0;

    return atoi (str);
}

/// read conf/lan_support.conf
// This file is to give a different IP for connections from the LAN
// Note: it assumes that all char-servers have the same IP, just different ports
// if this isn't how it it set up, you'll have to do some port-forwarding
void login_lan_config_read (const char *lancfgName)
{
    // set default configuration
    STRZCPY (lan_char_ip, "127.0.0.1");
    subneti[0] = 127;
    subneti[1] = 0;
    subneti[2] = 0;
    subneti[3] = 1;
    for (int j = 0; j < 4; j++)
        subnetmaski[j] = 255;

    FILE *fp = fopen_ (lancfgName, "r");
    if (!fp)
    {
        printf ("***WARNING: LAN Support configuration file is not found: %s\n",
                lancfgName);
        return;
    }

    printf ("---Start reading Lan Support configuration file\n");

    char line[1024], w1[1024], w2[1024];
    while (fgets (line, sizeof (line), fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;
        if (sscanf (line, "%[^:]: %[^\r\n]", w1, w2) != 2)
            continue;

        remove_control_chars (w1);
        remove_control_chars (w2);
        // WARNING: I don't think this hsould be calling gethostbyname at all, it should just parse the IP
        if (strcasecmp (w1, "lan_char_ip") == 0)
        {
            // Read Char-Server Lan IP Address
            struct hostent *h = gethostbyname (w2);
            if (h)
            {
                sprintf (lan_char_ip, "%hhu.%hhu.%hhu.%hhu",
                         h->h_addr[0], h->h_addr[1],
                         h->h_addr[2], h->h_addr[3]);
            }
            else
            {
                STRZCPY (lan_char_ip, w2);
            }
            printf ("LAN IP of char-server: %s.\n", lan_char_ip);
        }
        else if (strcasecmp (w1, "subnet") == 0)
        {
            // Read Subnetwork
            for (int j = 0; j < 4; j++)
                subneti[j] = 0;
            struct hostent *h = gethostbyname (w2);
            if (h)
            {
                for (int j = 0; j < 4; j++)
                    subneti[j] = h->h_addr[j];
            }
            else
            {
                sscanf (w2, "%hhu.%hhu.%hhu.%hhu", &subneti[0], &subneti[1],
                        &subneti[2], &subneti[3]);
            }
            printf ("LAN IP range: %hhu.%hhu.%hhu.%hhu.\n",
                    subneti[0], subneti[1], subneti[2], subneti[3]);
        }
        else if (strcasecmp (w1, "subnetmask") == 0)
        {
            // Read Subnetwork Mask
            for (int j = 0; j < 4; j++)
                subnetmaski[j] = 255;
            struct hostent *h = gethostbyname (w2);
            if (h)
            {
                for (int j = 0; j < 4; j++)
                    subnetmaski[j] = h->h_addr[j];
            }
            else
            {
                sscanf (w2, "%hhu.%hhu.%hhu.%hhu", &subnetmaski[0], &subnetmaski[1],
                        &subnetmaski[2], &subnetmaski[3]);
            }
            printf ("Subnet mask to send LAN char-server IP: %hhu.%hhu.%hhu.%hhu.\n",
                    subnetmaski[0], subnetmaski[1], subnetmaski[2],
                    subnetmaski[3]);
        }
    }
    fclose_ (fp);

    // log the LAN configuration
    login_log ("The LAN configuration of the server is set:\n");
    login_log ("- with LAN IP of char-server: %s.\n", lan_char_ip);
    login_log ("- with the sub-network of the char-server: %hhu.%hhu.%hhu.%hhu/%hhu.%hhu.%hhu.%hhu.\n",
               subneti[0], subneti[1], subneti[2], subneti[3],
               subnetmaski[0], subnetmaski[1], subnetmaski[2], subnetmaski[3]);

    // sub-network check of the char-server
    uint8_t p[4];
    sscanf (lan_char_ip, "%hhu.%hhu.%hhu.%hhu", &p[0], &p[1], &p[2], &p[3]);
    printf ("LAN test of LAN IP of the char-server: ");
    if (!lan_ip_check (p))
    {
        /// Actually, this could be considered a legitimate setting
        login_log ("***ERROR: LAN IP of the char-server doesn't belong to the specified Sub-network.\n");
    }

    printf ("---End reading of Lan Support configuration file\n");
}

/// Read general configuration file
// Note: DO NOT call login_log in here; its name is an option!
//-----------------------------------
void login_config_read (const char *cfgName)
{
    char line[1024], w1[1024], w2[1024];
    FILE *fp = fopen_ (cfgName, "r");
    if (!fp)
    {
        printf ("Configuration file (%s) not found.\n", cfgName);
        return;
    }

    printf ("---Start reading of Login Server configuration file (%s)\n",
            cfgName);
    while (fgets (line, sizeof (line), fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;
        if (sscanf (line, "%[^:]: %[^\r\n]", w1, w2) != 2)
            continue;

        remove_control_chars (w1);
        remove_control_chars (w2);

        if (strcasecmp (w1, "admin_state") == 0)
        {
            admin_state = config_switch (w2);
            continue;
        }
        if (strcasecmp (w1, "admin_pass") == 0)
        {
            STRZCPY (admin_pass, w2);
            continue;
        }
        if (strcasecmp (w1, "ladminallowip") == 0)
        {
            if (strcasecmp (w2, "clear") == 0)
            {
                if (access_ladmin_allow)
                    free (access_ladmin_allow);
                access_ladmin_allow = NULL;
                access_ladmin_allownum = 0;
                continue;
            }
            if (strcasecmp (w2, "all") == 0)
            {
                // reset all previous values
                free (access_ladmin_allow);
                // set to all (empty string matches prefix of everything)
                CREATE (access_ladmin_allow, access_entry, 1);
                access_ladmin_allownum = 1;
                continue;
            }
            if (!w2[0])
                continue;
            // don't add IP if already 'all'
            if ((access_ladmin_allownum == 1 && access_ladmin_allow[0] == '\0'))
                continue;

            RECREATE (access_ladmin_allow, access_entry, access_ladmin_allownum + 1);
            STRZCPY (access_ladmin_allow[access_ladmin_allownum], w2);
            access_ladmin_allownum++;
            continue;
        }
        if (strcasecmp (w1, "gm_pass") == 0)
        {
            STRZCPY (gm_pass, w2);
            continue;
        }
        if (strcasecmp (w1, "level_new_gm") == 0)
        {
            level_new_gm = atoi (w2);
            continue;
        }
        if (strcasecmp (w1, "new_account") == 0)
        {
            new_account_flag = config_switch (w2);
            continue;
        }
        if (strcasecmp (w1, "login_port") == 0)
        {
            login_port = atoi (w2);
            continue;
        }
        if (strcasecmp (w1, "account_filename") == 0)
        {
            STRZCPY (account_filename, w2);
            continue;
        }
        if (strcasecmp (w1, "gm_account_filename") == 0)
        {
            STRZCPY (GM_account_filename, w2);
            continue;
        }
        if (strcasecmp (w1, "gm_account_filename_check_timer") == 0)
        {
            gm_account_filename_check_timer = atoi (w2);
            continue;
        }
        if (strcasecmp (w1, "login_log_filename") == 0)
        {
            STRZCPY (login_log_filename, w2);
            continue;
        }
        if (strcasecmp (w1, "login_log_unknown_packets_filename") == 0)
        {
            STRZCPY (login_log_unknown_packets_filename, w2);
            continue;
        }
        if (strcasecmp (w1, "save_unknown_packets") == 0)
        {
            save_unknown_packets = config_switch (w2);
            continue;
        }
        if (strcasecmp (w1, "display_parse_login") == 0)
        {
            display_parse_login = config_switch (w2);
            continue;
        }
        if (strcasecmp (w1, "display_parse_admin") == 0)
        {
            display_parse_admin = config_switch (w2);
            continue;
        }
        if (strcasecmp (w1, "display_parse_fromchar") == 0)
        {
            // 0: no, 1: yes (without packet 0x2714), 2: all packets
            switch (config_switch (w2))
            {
            default: display_parse_fromchar = CP_NONE;
            case 1: display_parse_fromchar = CP_MOST;
            case 2: display_parse_fromchar = CP_ALL;
            }
            continue;
        }
        if (strcasecmp (w1, "min_level_to_connect") == 0)
        {
            min_level_to_connect = atoi (w2);
            continue;
        }
        if (strcasecmp (w1, "add_to_unlimited_account") == 0)
        {
            add_to_unlimited_account = config_switch (w2);
            continue;
        }
        if (strcasecmp (w1, "start_limited_time") == 0)
        {
            start_limited_time = atoi (w2);
            continue;
        }
        if (strcasecmp (w1, "order") == 0)
        {
            int i = atoi (w2);
            if (i == 0 || strcasecmp (w2, "deny,allow") == 0 ||
                strcasecmp (w2, "deny, allow") == 0)
                access_order = ACO_DENY_ALLOW;
            else if (i == 1 || strcasecmp (w2, "allow,deny") == 0 ||
                strcasecmp (w2, "allow, deny") == 0)
                access_order = ACO_ALLOW_DENY;
            else if (i == 2 || strcasecmp (w2, "mutual-failure") == 0)
                access_order = ACO_MUTUAL_FAILURE;
            else
                printf ("***WARNING: unknown access order: %s\n", w2);
            continue;
        }
        if (strcasecmp (w1, "allow") == 0)
        {
            if (strcasecmp (w2, "clear") == 0)
            {
                free (access_allow);
                access_allow = NULL;
                access_allownum = 0;
                continue;
            }
            if (strcasecmp (w2, "all") == 0)
            {
                // reset all previous values
                free (access_allow);
                // set to all
                CREATE (access_allow, access_entry, 1);
                access_allownum = 1;
            }
            if (!w2[0])
                continue;
            // don't add IP if already 'all'
            if (access_allownum == 1 && access_allow[0] == '\0')
                continue;
            RECREATE (access_allow, access_entry, access_allownum + 1);
            STRZCPY (access_allow[access_allownum], w2);
            access_allownum++;
            continue;
        }
        if (strcasecmp (w1, "deny") == 0)
        {
            if (strcasecmp (w2, "clear") == 0)
            {
                free (access_deny);
                access_deny = NULL;
                access_denynum = 0;
                continue;
            }
            if (strcasecmp (w2, "all") == 0)
            {
                // reset all previous values
                free (access_deny);
                // set to all
                CREATE (access_deny, access_entry, 1);
                access_denynum = 1;
                continue;
            }
            if (!w2[0])
                continue;
            // don't add IP if already 'all'
            if (access_denynum == 1 && access_deny[0] == '\0')
                continue;
            RECREATE (access_deny, access_entry, access_denynum + 1);
            STRZCPY (access_deny[access_denynum], w2);
            access_denynum++;
            continue;
        }
        if (strcasecmp (w1, "anti_freeze_enable") == 0)
        {
            anti_freeze_enable = config_switch (w2);
            continue;
        }
        if (strcasecmp (w1, "anti_freeze_interval") == 0)
        {
            ANTI_FREEZE_INTERVAL = atoi (w2);
            if (ANTI_FREEZE_INTERVAL < 5)
                // minimum 5 seconds
                ANTI_FREEZE_INTERVAL = 5;
            continue;
        }
        if (strcasecmp (w1, "import") == 0)
        {
            login_config_read (w2);
            continue;
        }
        if (strcasecmp (w1, "update_host") == 0)
        {
            STRZCPY (update_host, w2);
            continue;
        }
        if (strcasecmp (w1, "main_server") == 0)
        {
            STRZCPY (main_server, w2);
            continue;
        }
        printf ("%s: unknown option: %s\n", __func__, line);
    }
    fclose_ (fp);

    printf ("---End reading of Login Server configuration file.\n");
}

/// Displaying of configuration warnings
// this is not done while parsing because the log filename might change
// TODO merge it anyways, since this isn't logged :/
void display_conf_warnings (void)
{
    if (admin_state)
    {
        if (admin_pass[0] == '\0')
        {
            printf ("***WARNING: Administrator password is void (admin_pass).\n");
        }
        else if (strcmp (admin_pass, "admin") == 0)
        {
            printf ("***WARNING: You are using the default administrator password (admin_pass).\n");
            printf ("            We highly recommend that you change it.\n");
        }
    }

    if (gm_pass[0] == '\0')
    {
        printf ("***WARNING: 'To GM become' password is void (gm_pass).\n");
        printf ("            We highly recommend that you set one password.\n");
    }
    else if (strcmp (gm_pass, "gm") == 0)
    {
        printf ("***WARNING: You are using the default GM password (gm_pass).\n");
        printf ("            We highly recommend that you change it.\n");
    }

    if (level_new_gm > 99)
    {
        printf ("***WARNING: Invalid value for level_new_gm parameter -> set to 60 (default).\n");
        level_new_gm = 60;
    }

    if (login_port < 1024)
    {
        printf ("***WARNING: Invalid value for login_port parameter -> set to 6900 (default).\n");
        login_port = 6900;
    }

    if (gm_account_filename_check_timer == 1)
    {
        printf ("***WARNING: Invalid value for gm_account_filename_check_timer parameter.\n");
        printf ("            -> set to 2 sec (minimum value).\n");
        gm_account_filename_check_timer = 2;
    }

    if (min_level_to_connect > 99)
    {
        // 0: all players, 1-99 at least gm level x
        printf ("***WARNING: Invalid value for min_level_to_connect (%d) parameter\n",
                min_level_to_connect);
        printf ("            -> set to 99 (only GM level 99).\n");
        min_level_to_connect = 99;
    }

    if (start_limited_time < -1)
    {
        // -1: create unlimited account, 0 or more: additionnal sec from now to create limited time
        printf ("***WARNING: Invalid value for start_limited_time parameter\n");
        printf ("            -> set to -1 (new accounts are created with unlimited time).\n");
        start_limited_time = -1;
    }

    if (access_order == ACO_DENY_ALLOW)
    {
        if (access_denynum == 1 && access_deny[0] == '\0')
        {
            printf ("***WARNING: The IP security order is 'deny,allow' (allow if not denied).\n");
            printf ("            But you denied ALL IP!\n");
        }
    }
    if (access_order == ACO_ALLOW_DENY)
    {
        if (access_allownum == 0 && access_denynum != 0)
        {
            printf ("***WARNING: The IP security order is 'allow,deny' (deny if not allowed).\n");
            printf ("            But you never allowed any IP!\n");
        }
    }
    else
    {
        // ACO_MUTUAL_FAILURE
        if (access_allownum == 0 && access_denynum != 0)
        {
            printf ("***WARNING: The IP security order is 'mutual-failure'\n");
            printf ("            (allow if in the allow list and not in the deny list).\n");
            printf ("            But you never allowed any IP!\n");
        }
        else if (access_denynum == 1 && access_deny[0] == '\0')
        {
            printf ("***WARNING: The IP security order is mutual-failure\n");
            printf ("            (allow if in the allow list and not in the deny list).\n");
            printf ("            But, you denied ALL IP!\n");
        }
    }
}

//-------------------------------
// Save configuration in log file
//-------------------------------
void save_config_in_log (void)
{
    login_log ("\nThe login-server starting...\n");

    // save configuration in log file
    login_log ("The configuration of the server is set:\n");

    if (!admin_state)
        login_log ("- with no remote administration.\n");
    else if (admin_pass[0] == '\0')
        login_log ("- with a remote administration with a VOID password.\n");
    else if (strcmp (admin_pass, "admin") == 0)
        login_log ("- with a remote administration with the DEFAULT password.\n");
    else
        login_log ("- with a remote administration with the password of %d character(s).\n",
                   strlen (admin_pass));
    if (access_ladmin_allownum == 0 || (access_ladmin_allownum == 1 && access_ladmin_allow[0] == '\0'))
    {
        login_log ("- to accept any IP for remote administration\n");
    }
    else
    {
        login_log ("- to accept following IP for remote administration:\n");
        for (int i = 0; i < access_ladmin_allownum; i++)
            login_log ("  %s\n", access_ladmin_allow[i]);
    }

    if (gm_pass[0] == '\0')
        login_log ("- with a VOID 'To GM become' password (gm_pass).\n");
    else if (strcmp (gm_pass, "gm") == 0)
        login_log ("- with the DEFAULT 'To GM become' password (gm_pass).\n");
    else
        login_log ("- with a 'To GM become' password (gm_pass) of %d character(s).\n",
                   strlen (gm_pass));
    if (!level_new_gm)
        login_log ("- to refuse any creation of GM with @gm.\n");
    else
        login_log ("- to create GM with level '%hhu' when @gm is used.\n",
                   level_new_gm);

    if (new_account_flag)
        login_log ("- to ALLOW new users (with _F/_M).\n");
    else
        login_log ("- to NOT ALLOW new users (with _F/_M).\n");
    login_log ("- with port: %d.\n", login_port);
    login_log ("- with the accounts file name: '%s'.\n",
               account_filename);
    login_log ("- with the GM accounts file name: '%s'.\n",
               GM_account_filename);
    if (!gm_account_filename_check_timer)
        login_log ("- to NOT check GM accounts file modifications.\n");
    else
        login_log ("- to check GM accounts file modifications every %d seconds.\n",
                   gm_account_filename_check_timer);

    // not necessary to log the 'login_log_filename', we are inside :)

    login_log ("- with the unknown packets file name: '%s'.\n",
               login_log_unknown_packets_filename);
    if (save_unknown_packets)
        login_log ("- to SAVE all unknown packets.\n");
    else
        login_log ("- to SAVE only unknown packets sending by a char-server or a remote administration.\n");
    if (display_parse_login)
        login_log ("- to display normal parse packets on console.\n");
    else
        login_log ("- to NOT display normal parse packets on console.\n");
    if (display_parse_admin)
        login_log ("- to display administration parse packets on console.\n");
    else
        login_log ("- to NOT display administration parse packets on console.\n");
    if (display_parse_fromchar)
        login_log ("- to display char-server parse packets on console.\n");
    else
        login_log ("- to NOT display char-server parse packets on console.\n");

    if (!min_level_to_connect)
        login_log ("- with no minimum level for connection.\n");
    else if (min_level_to_connect == 99)
        login_log ("- to accept only GM with level 99.\n");
    else
        login_log ("- to accept only GM with level %d or more.\n",
                   min_level_to_connect);

    if (add_to_unlimited_account)
        login_log ("- to authorize adjustment (with timeadd ladmin) on an unlimited account.\n");
    else
        login_log ("- to refuse adjustment (with timeadd ladmin) on an unlimited account. You must use timeset (ladmin command) before.\n");

    if (start_limited_time < 0)
        login_log ("- to create new accounts with an unlimited time.\n");
    else if (start_limited_time == 0)
        login_log ("- to create new accounts with a limited time: time of creation.\n");
    else
        login_log ("- to create new accounts with a limited time: time of creation + %d second(s).\n",
                   start_limited_time);

    login_log ("- with control of players IP between login-server and char-server.\n");

    if (access_order == ACO_DENY_ALLOW)
    {
        if (access_denynum == 0)
        {
            login_log
                ("- with the IP security order: 'deny,allow' (allow if not deny). You refuse no IP.\n");
        }
        else if (access_denynum == 1 && access_deny[0] == '\0')
        {
            login_log ("- with the IP security order: 'deny,allow' (allow if not deny). You refuse ALL IP.\n");
        }
        else
        {
            login_log ("- with the IP security order: 'deny,allow' (allow if not deny). Refused IP are:\n");
            for (int i = 0; i < access_denynum; i++)
                login_log ("  %s\n", access_deny[i]);
        }
    }
    else if (access_order == ACO_ALLOW_DENY)
    {
        if (access_allownum == 0)
        {
            login_log ("- with the IP security order: 'allow,deny' (deny if not allow). But, NO IP IS AUTHORISED!\n");
        }
        else if (access_allownum == 1 && access_allow[0] == '\0')
        {
            login_log ("- with the IP security order: 'allow,deny' (deny if not allow). You authorise ALL IP.\n");
        }
        else
        {
            login_log ("- with the IP security order: 'allow,deny' (deny if not allow). Authorised IP are:\n");
            for (int i = 0; i < access_allownum; i++)
                login_log ("  %s\n", access_allow[i]);
        }
    }
    else
    {
        // ACO_MUTUAL_FAILURE
        login_log ("- with the IP security order: 'mutual-failure' (allow if in the allow list and not in the deny list).\n");
        if (access_allownum == 0)
        {
            login_log ("  But, NO IP IS AUTHORISED!\n");
        }
        else if (access_denynum == 1 && access_deny[0] == '\0')
        {
            login_log ("  But, you refuse ALL IP!\n");
        }
        else
        {
            if (access_allownum == 1 && access_allow[0] == '\0')
            {
                login_log ("  You authorise ALL IP.\n");
            }
            else
            {
                login_log ("  Authorised IP are:\n");
                for (int i = 0; i < access_allownum; i++)
                    login_log ("    %s\n", access_allow[i]);
            }
            login_log ("  Refused IP are:\n");
            for (int i = 0; i < access_denynum; i++)
                login_log ("    %s\n", access_deny[i]);
        }
    }
}

/// Function called at exit of the server
// is all of this really needed?
void do_final (void)
{
    mmo_auth_sync ();

    for (int i = 0; i < MAX_SERVERS; i++)
        delete_session (server_fd[i]);
    delete_session (login_fd);

    login_log ("----End of login-server (normal end with closing of all files).\n");
}

/// Main function of login-server (read conf and set up parsers)
void do_init (int argc, char **argv)
{
    // read login-server configuration
    login_config_read ((argc > 1) ? argv[1] : LOGIN_CONF_NAME);
    // not in login_config_read, because we can use 'import' option, and display same message twice or more
    // TODO - shouldn't the warnings display anytime an invalid option is set?
    display_conf_warnings ();
    // not before, because log file name can be changed
    save_config_in_log ();
    login_lan_config_read ((argc > 2) ? argv[2] : LAN_CONF_NAME);

    for (int i = 0; i < AUTH_FIFO_SIZE; i++)
        auth_fifo[i].delflag = 1;
    for (int i = 0; i < MAX_SERVERS; i++)
        server_fd[i] = -1;

    gm_account_db = numdb_init ();

    read_gm_account ();
    mmo_auth_init ();

    set_defaultparse (parse_login);
    login_fd = make_listen_port (login_port);

    // Trigger auth sync every 5 minutes
    add_timer_interval (gettick () + 300000, check_auth_sync, 0, 0, 300000);

    if (anti_freeze_enable)
        add_timer_interval (gettick () + 1000, char_anti_freeze_system, 0,
                            0, ANTI_FREEZE_INTERVAL * 1000);

    // add timer to check GM accounts file modification
    int j = gm_account_filename_check_timer;
    if (j)
        add_timer_interval (gettick () + j * 1000, check_GM_file, 0, 0, j * 1000);

    login_log ("The login-server is ready (Server is listening on the port %d).\n",
               login_port);

    atexit (do_final);
}
