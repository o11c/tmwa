#include "char.hpp"

#include <sys/wait.h>

#include "../common/core.hpp"
#include "../common/lock.hpp"
#include "../common/socket.hpp"
#include "../common/timer.hpp"
#include "../common/utils.hpp"
#include "../common/version.hpp"

#include "inter.hpp"
#include "int_party.hpp"
#include "int_storage.hpp"

struct mmo_map_server server[MAX_MAP_SERVERS];
int server_fd[MAX_MAP_SERVERS];
// Map-server anti-freeze system. Counter. 5 ok, 4...0 freezed
int server_freezeflag[MAX_MAP_SERVERS];
int anti_freeze_enable = 0;
int ANTI_FREEZE_INTERVAL = 6;

/// the connection to the login server
int login_fd = -1;
/// the listening socket
int char_fd = -1;
/// The user id and password used by the map-server to connect to us
/// and by us to connect to the login-server
char userid[24];
char passwd[24];
char server_name[20];
char whisper_server_name[24] = "Server";
IP_Address login_ip;
in_port_t login_port = 6901;
IP_Address char_ip;
in_port_t char_port = 6121;
int char_maintenance;
int char_new;
char char_txt[1024];
char unknown_char_name[24] = "Unknown";
const char char_log_filename[] = "log/char.log";
const char char_log_unknown_packets_filename[] = "log/char_unknown_packets.log";
//Added for lan support
IP_Address lan_map_ip;
IP_Mask lan_mask;
/// Allow characters with same name but different case?
// This option is misleading, true means case-sensitive, false means insensitive
bool name_ignoring_case = 0;
// Option to know which letters/symbols are authorised in the name of a character (0: all, 1: only those in char_name_letters, 2: all EXCEPT those in char_name_letters) by [Yor]
enum NameLetterControl
{
    ALL, ONLY, EXCLUDE
} char_name_option = ALL;
// list of letters/symbols authorised (or not) in a character name. by [Yor]
char char_name_letters[255] = "";

class CharSessionData : public SessionData
{
public:
    account_t account_id;
    uint32_t login_id1, login_id2;
    enum gender sex;
    unsigned short packet_tmw_version;
    struct mmo_charstatus *found_char[MAX_CHARS_PER_ACCOUNT];
    char email[40];
    time_t connect_until_time;
};

#define AUTH_FIFO_SIZE 256
struct auth_fifo
{
    account_t account_id;
    charid_t char_id;
    uint32_t login_id1, login_id2;
    IP_Address ip;
    struct mmo_charstatus *char_pos;
    // 0: present, 1: deleted, 2: returning from map-server
    int delflag;
    enum gender sex;
    unsigned short packet_tmw_version;
    time_t connect_until_time;
} auth_fifo[AUTH_FIFO_SIZE];
unsigned auth_fifo_pos = 0;

int char_id_count = 150000;
// TODO: it is very inefficient to store this this way
// instead, have the outer struct be by account, and that store characters
struct mmo_charstatus *char_dat;
int char_num, char_max;
int max_connect_user = 0;
int autosave_interval = DEFAULT_AUTOSAVE_INTERVAL;

// Initial position (set it in conf file)
Point start_point;

struct gm_account *gm_accounts = NULL;
int GM_num = 0;

// online players by [Yor]
char online_txt_filename[1024] = "online.txt";
char online_html_filename[1024] = "online.html";
/// How to sort players
int online_sorting_option = 0;
/// Which columns to display
int online_display_option = 1;
/// Requested browser refresh time
int online_refresh_html = 20;
/// Display 'GM' only if GM level is at least this
// excludes bots and possibly devs
gm_level_t online_gm_display_min_level = 20;

// same size of char_dat, and id value of current server (or -1)
int *online_char_server_fd;
/// When next to update online files when we receiving information from a server (not less than 8 seconds)
time_t update_online;

/// For forked DB writes
pid_t pid = 0;

Log char_log("char");
Log unknown_packet_log("char.unknown");

///Return level of a GM (0 if not a GM)
static gm_level_t isGM(int account_id) __attribute__((pure));
static gm_level_t isGM(int account_id)
{
    for (int i = 0; i < GM_num; i++)
        if (gm_accounts[i].account_id == account_id)
            return gm_accounts[i].level;
    return 0;
}

//----------------------------------------------
// Search an character id
//   (return character index or -1 (if not found))
//   If exact character name is not found,
//   the function checks without case sensitive
//   and returns index if only 1 character is found
//   and similar to the searched name.
//----------------------------------------------
struct mmo_charstatus *character_by_name(const char *character_name)
{
    int quantity = 0;
    struct mmo_charstatus *loose = NULL;
    for (int i = 0; i < char_num; i++)
    {
        if (strcmp(char_dat[i].name, character_name) == 0)
            return &char_dat[i];
        if (strcasecmp(char_dat[i].name, character_name) == 0)
        {
            quantity++;
            loose = &char_dat[i];
        }
    }
    return quantity == 1 ? loose : NULL;
}

/// Write a line of character data
static void mmo_char_tofile(FILE *fp, struct mmo_charstatus *p)
{
    // on multi-map server, sometimes it's posssible that last_point become void. (reason???)
    // We check that to not lost character at restart.
    if (p->last_point.map[0] == '\0')
        p->last_point = start_point;

    fprintf(fp,
             "%d\t"              "%d,%d\t"
             "%s\t"             "%d,%d,%d\t"
             "%d,%d,%d\t"       "%d,%d,%d,%d\t"
             "%d,%d,%d,%d,%d,%d\t"
             "%d,%d\t"          "%d,%d,%d\t"
             "%d,%d,%d\t"       "%d,%d,%d\t"
             "%d,%d,%d,%d,%d\t"
             "%s,%d,%d\t"
             "%s,%d,%d,%d\t",
             p->char_id,        p->account_id, p->char_num,
             p->name,           0/*pc_class*/, p->base_level, p->job_level,
             p->base_exp, p->job_exp, p->zeny,  p->hp, p->max_hp, p->sp, p->max_sp,
             p->stats[ATTR::STR], p->stats[ATTR::AGI], p->stats[ATTR::VIT], p->stats[ATTR::INT], p->stats[ATTR::DEX], p->stats[ATTR::LUK],
             p->status_point, p->skill_point,   p->option, 0/*p->karma*/, 0/*p->manner*/,
             p->party_id, 0, 0,         p->hair, p->hair_color, 0/*p->clothes_color*/,
             p->weapon, p->shield, p->head, p->chest, p->legs,
             &p->last_point.map, p->last_point.x, p->last_point.y,
             &p->save_point.map, p->save_point.x, p->save_point.y, p->partner_id);
    for (int i = 0; i < 10; i++)
        if (p->memo_point[i].map[0])
        {
            fprintf(fp, "%s,%d,%d ", &p->memo_point[i].map,
                    p->memo_point[i].x, p->memo_point[i].y);
        }
    fprintf(fp, "\t");

    for (int i = 0; i < MAX_INVENTORY; i++)
        if (p->inventory[i].nameid)
        {
            fprintf(fp, "%d,%hu,%hu,%hu," "%d,%d,%d," "%d,%d,%d,%d,%d ",
                    0 /*id*/, p->inventory[i].nameid, p->inventory[i].amount,
                    static_cast<uint16_t>(p->inventory[i].equip),

                    0/*identify*/, 0/*refine*/,
                    0/*attribute*/,

                    0, 0, 0, 0 /*card[3]*/,
                    0/*broken*/);
        }
    fprintf(fp, "\t");

    // cart was here
    fprintf(fp, "\t");

    for (int i = 0; i < MAX_SKILL; i++)
        if (p->skill[i].id)
        {
            fprintf(fp, "%d,%d ", p->skill[i].id,
                     p->skill[i].lv | (p->skill[i].flags << 16));
        }
    fprintf(fp, "\t");

    for (int i = 0; i < p->global_reg_num; i++)
        if (p->global_reg[i].str[0])
            fprintf(fp, "%s,%d ", p->global_reg[i].str, p->global_reg[i].value);
    fprintf(fp, "\t\n");
}

///Read character information from a line
// return 0 or negative for errors, positive for OK
static int mmo_char_fromstr(char *str, struct mmo_charstatus *p)
{
    memset(p, '\0', sizeof(struct mmo_charstatus));

    int next;
    int set = sscanf(str,
                     "%u\t"            "%d,%hhu\t"
                     "%[^\t]\t"        "%*u,%hhu,%hhu\t"
                     "%d,%d,%d\t"      "%d,%d,%d,%d\t"
                     "%hd,%hd,%hd,%hd,%hd,%hd\t"
                     "%hd,%hd\t"       "%hd,%*d,%*d\t"
                     "%d,%*d,%*d\t"      "%hd,%hd,%*d\t"
                     "%hd,%hd,%hd,%hd,%hd\t"
                     "%[^,],%hd,%hd\t"
                     "%[^,],%hd,%hd,%d" "%n",
                     &p->char_id,       &p->account_id, &p->char_num,
                     p->name,           /*pc_class,*/ &p->base_level, &p->job_level,
                     &p->base_exp, &p->job_exp, &p->zeny,       &p->hp, &p->max_hp, &p->sp, &p->max_sp,
                     &p->stats[ATTR::STR], &p->stats[ATTR::AGI], &p->stats[ATTR::VIT], &p->stats[ATTR::INT], &p->stats[ATTR::DEX], &p->stats[ATTR::LUK],
                     &p->status_point,  &p->skill_point,        &p->option, /*karma,*/ /*manner,*/
                     &p->party_id, /*guild_id,*/ /*pet_id,*/    &p->hair, &p->hair_color, /*clothes_color,*/
                     &p->weapon, &p->shield, &p->head, &p->chest, &p->legs,
                     &p->last_point.map, &p->last_point.x, &p->last_point.y,
                     &p->save_point.map, &p->save_point.x, &p->save_point.y,
                     &p->partner_id, &next);
    if (set != 43)
        return 0;

    // Some checks
    for (int i = 0; i < char_num; i++)
    {
        if (char_dat[i].char_id == p->char_id)
        {
            printf("\033[1;31mmmo_auth_init: ******Error: a character has an identical id to another.\n");
            printf("               character id #%d -> new character not read.\n",
                    p->char_id);
            printf("               Character saved in log file.\033[0m\n");
            return -1;
        }
        else if (strcmp(char_dat[i].name, p->name) == 0)
        {
            printf("\033[1;31mmmo_auth_init: ******Error: character name already exists.\n");
            printf("               character name '%s' -> new character not read.\n",
                    p->name);
            printf("               Character saved in log file.\033[0m\n");
            return -2;
        }
    }

    if (strcasecmp(whisper_server_name, p->name) == 0)
    {
        char_log.warn("mmo_auth_init: ******WARNING: character name has whisper server name:\n"
                      "Character name '%s' = whisper server name '%s'.\n",
                      p->name, whisper_server_name);
        char_log.warn("               Character read. Suggestion: change the whisper server name.\n");
    }

    str += next;
    if (str[0] == '\n' || str[0] == '\r')
        return 1;

    str++;

    for (int i = 0; str[0] && str[0] != '\t'; i++)
    {
        if (sscanf(str, "%[^,],%hd,%hd%n", &p->memo_point[i].map,
                    &p->memo_point[i].x, &p->memo_point[i].y, &next) != 3)
            return -3;
        str += next;
        if (str[0] == ' ')
            str++;
    }

    str++;

    /// inventory
    // TODO check against maximum
    for (int i = 0; str[0] && str[0] != '\t'; i++)
    {
        int sn = sscanf(str, "%*d,%hu,%hu,%hu,%*d,%*d,%*d,%*d,%*d,%*d,%*d,%*d%n",
                        &p->inventory[i].nameid, &p->inventory[i].amount,
                        reinterpret_cast<uint16_t *>(&p->inventory[i].equip),
                        &next);
        if (sn != 12)
            return -4;

        str += next;
        if (str[0] == ' ')
            str++;
    }

    str++;

    /// cart
    // TODO check against maximum
    for (int i = 0; str[0] && str[0] != '\t'; i++)
    {
        int sn = sscanf(str, "%*d,%*d,%*d,%*u,%*d,%*d,%*d,%*d,%*d,%*d,%*d,%*d%n",
                        &next);
        if (sn != 12)
            return -5;
        str += next;
        if (str[0] == ' ')
            str++;
    }

    str++;

    for (int i = 0; str[0] && str[0] != '\t'; i++)
    {
        int skill_id, skill_lvl_and_flags;
        if (sscanf(str, "%d,%d%n", &skill_id, &skill_lvl_and_flags, &next) != 2)
            return -6;
        p->skill[skill_id].id = skill_id;
        p->skill[skill_id].lv = skill_lvl_and_flags & 0xffff;
        p->skill[skill_id].flags = ((skill_lvl_and_flags >> 16) & 0xffff);
        str += next;
        if (str[0] == ' ')
            str++;
    }

    str++;

    for (int i = 0; str[0] && str[0] != '\t' && str[0] != '\n' && str[0] != '\r'; i++, p->global_reg_num++)
    {
        if (sscanf(str, "%[^,],%d%n", p->global_reg[i].str,
                    &p->global_reg[i].value, &next) != 2)
            // There used to be a check to allow and discard empty string
            return -7;
        str += next;
        if (str[0] == ' ')
            str++;
    }
    return 1;
}

/// Read and parse the characters file (athena.txt)
static void mmo_char_init(void)
{
    char_max = 256;
    CREATE(char_dat, struct mmo_charstatus, 256);
    CREATE(online_char_server_fd, int, 256);
    for (int i = 0; i < char_max; i++)
        online_char_server_fd[i] = -1;
    char_num = 0;

    FILE *fp = fopen_(char_txt, "r");
    if (!fp)
    {
        char_log.warn("Characters file not found: %s.\n", char_txt);
        char_log.info("Id for the next created character: %d.\n", char_id_count);
        return;
    }

    char line[65536];
    int line_count = 0;
    while (fgets(line, sizeof(line), fp))
    {
        line_count++;

        if (line[0] == '/' && line[1] == '/')
            continue;

        int id;
        // needed to confirm that %newid% was found
        int got_end = 0;
        if (sscanf(line, "%d\t%%newid%%%n", &id, &got_end) == 1 && got_end)
        {
            if (char_id_count < id)
                char_id_count = id;
            continue;
        }

        if (char_num >= char_max)
        {
            char_max += 256;
            RECREATE(char_dat, struct mmo_charstatus, char_max);
            RECREATE(online_char_server_fd, int, char_max);
            for (int i = char_max - 256; i < char_max; i++)
                online_char_server_fd[i] = -1;
        }

        int ret = mmo_char_fromstr(line, &char_dat[char_num]);
        if (ret > 0)
        {
            // negative value or zero for errors
            if (char_dat[char_num].char_id >= char_id_count)
                char_id_count = char_dat[char_num].char_id + 1;
            char_num++;
            continue;
        }
        printf("mmo_char_init: in characters file, unable to read the line #%d.\n",
                line_count);
        printf("               -> Character saved in log file.\n");
        switch (ret)
        {
        case -1:
            char_log.warn("Duplicate character id in the next character line (character not read):\n");
            break;
        case -2:
            char_log.warn("Duplicate character name in the next character line (character not read):\n");
            break;
        case -3:
            char_log.warn("Invalid memo point structure in the next character line (character not read):\n");
            break;
        case -4:
            char_log.warn("Invalid inventory item structure in the next character line (character not read):\n");
            break;
        case -5:
            char_log.warn("Invalid cart item structure in the next character line (character not read):\n");
            break;
        case -6:
            char_log.warn("Invalid skill structure in the next character line (character not read):\n");
            break;
        case -7:
            char_log.warn("Invalid register structure in the next character line (character not read):\n");
            break;
        default:       // 0
            char_log.warn("Unabled to get a character in the next line - Basic structure of line (before inventory) is incorrect (character not read):\n");
            break;
        }
        char_log.warn("%s", line);
    }
    fclose_(fp);

    if (char_num == 0)
    {
        char_log.info("mmo_char_init: No character found in %s.\n", char_txt);
    }
    else if (char_num == 1)
    {
        char_log.info("mmo_char_init: 1 character read in %s.\n", char_txt);
    }
    else
    {
        char_log.info("mmo_char_init: %d characters read in %s.\n", char_num, char_txt);
    }

    char_log.info("Id for the next created character: %d.\n", char_id_count);
}

/// Save characters in athena.txt
static void mmo_char_sync(void)
{
    int id[char_num];
    /// Sort before save
    // FIXME is this necessary or useful?
    // Note that this sorts by account id and slot, not by character id
    for (int i = 0; i < char_num; i++)
    {
        id[i] = i;
        for (int j = 0; j < i; j++)
        {
            if ((char_dat[i].account_id < char_dat[id[j]].account_id) ||
                // if same account id, we sort by slot.
                (char_dat[i].account_id == char_dat[id[j]].account_id &&
                 char_dat[i].char_num < char_dat[id[j]].char_num))
            {
                for (int k = i; k > j; k--)
                    id[k] = id[k - 1];
                id[j] = i;      // id[i]
                break;
            }
        }
    }
    int lock;
    FILE *fp = lock_fopen(char_txt, &lock);
    if (!fp)
    {
        char_log.error("WARNING: Server can't save characters.\n");
        return;
    }
    for (int i = 0; i < char_num; i++)
        mmo_char_tofile(fp, &char_dat[id[i]]);
    fprintf(fp, "%d\t%%newid%%\n", char_id_count);
    lock_fclose(fp, char_txt, &lock);
}

//----------------------------------------------------
// Function to save (in a periodic way) datas in files
//----------------------------------------------------
static void mmo_char_sync_timer(timer_id, tick_t)
{
    if (pid && !waitpid(pid, NULL, WNOHANG))
        // still running
        return;

    pid = fork();
    if (pid > 0)
        return;
    // either we are the child, or fork() failed
    mmo_char_sync();
    inter_save();

    // If we're a child we should suicide now.
    if (pid == 0)
        _exit(0);
    pid = 0;
}

//----------------------------------------------------
// Remove trailing whitespace from a name
//----------------------------------------------------
static void remove_trailing_blanks(char *name)
{
    char *tail = name + strlen(name) - 1;
    while (tail > name && *tail == ' ')
        *tail-- = 0;
}


struct new_char_dat
{
    char name[24];
    earray<uint8_t, ATTR, ATTR::COUNT> stats;
    uint8_t slot;
    // Note: the mana client says these are 16-bit ints
    // but no color or style is ever likely to be that high
    // this is a prime location for additions. Race? Clothes color?
    uint8_t hair_color, hair_color_high;
    uint8_t hair_style, hair_style_high;
};

/// Create a new character, from
static struct mmo_charstatus *make_new_char(int fd, const uint8_t *raw_dat)
{
    struct new_char_dat dat = *reinterpret_cast<const struct new_char_dat *>(raw_dat);
    CharSessionData *sd = static_cast<CharSessionData *>(session[fd]->session_data);

    // remove control characters from the name
    char *name = dat.name;
    name[23] = '\0';
    if (has_control_chars(name))
    {
        char_log.info("Make new char error (control char received in the name): (connection #%d, account: %d).\n",
                      fd, sd->account_id);
        return NULL;
    }

    // Eliminate surrounding whitespace
    while (*name == ' ')
        name++;
    remove_trailing_blanks(name);

    // check length of character name
    if (strlen(name) < 4)
    {
        char_log.info("Make new char error (character name too small): (connection #%d, account: %d, name: '%s').\n",
                      fd, sd->account_id, name);
        return NULL;
    }

    switch (char_name_option)
    {
    case ONLY:
        for (int i = 0; name[i]; i++)
            if (strchr(char_name_letters, name[i]) == NULL)
            {
                char_log.info("Make new char error (invalid letter in the name): (connection #%d, account: %d), name: %s, invalid letter: %c.\n",
                              fd, sd->account_id, name, name[i]);
                return NULL;
            }
        break;
    case EXCLUDE:
        for (int i = 0; name[i]; i++)
            if (strchr(char_name_letters, name[i]) != NULL)
            {
                char_log.info("Make new char error (invalid letter in the name): (connection #%d, account: %d), name: %s, invalid letter: %c.\n",
                              fd, sd->account_id, name, name[i]);
                    return NULL;
            }
        break;
    }
    if (dat.stats[ATTR::STR] + dat.stats[ATTR::AGI] + dat.stats[ATTR::VIT] + dat.stats[ATTR::INT] + dat.stats[ATTR::DEX] + dat.stats[ATTR::LUK] != 5 * 6 ||
        dat.slot >= MAX_CHARS_PER_ACCOUNT ||
        dat.hair_style_high || dat.hair_style >= NUM_HAIR_STYLES ||
        dat.hair_color_high || dat.hair_color >= NUM_HAIR_COLORS)
    {
        char_log.info("Make new char error (%s): (connection #%d, account: %d) slot %hhu, name: %s, stats: %hhu+%hhu+%hhu+%hhu+%hhu+%hhu=%u, hair: %hhu, hair color: %hhu\n",
                      "invalid values",
                      fd, sd->account_id, dat.slot, name,
                      dat.stats[ATTR::STR], dat.stats[ATTR::AGI], dat.stats[ATTR::VIT], dat.stats[ATTR::INT], dat.stats[ATTR::DEX], dat.stats[ATTR::LUK],
                      dat.stats[ATTR::STR] + dat.stats[ATTR::AGI] + dat.stats[ATTR::VIT] + dat.stats[ATTR::INT] + dat.stats[ATTR::DEX] + dat.stats[ATTR::LUK],
                      dat.hair_style, dat.hair_color);
        return NULL;
    }

    // check individual stat value
    for (ATTR i : ATTRs)
    {
        if (dat.stats[i] < 1 || dat.stats[i] > 9)
        {
            char_log.info("Make new char error (%s): (connection #%d, account: %d) slot %hhu, name: %s, stats: %hhu+%hhu+%hhu+%hhu+%hhu+%hhu=%u, hair: %hhu, hair color: %hhu\n",
                          "invalid stat value: not between 1 to 9",
                          fd, sd->account_id, dat.slot, name,
                          dat.stats[ATTR::STR], dat.stats[ATTR::AGI], dat.stats[ATTR::VIT], dat.stats[ATTR::INT], dat.stats[ATTR::DEX], dat.stats[ATTR::LUK],
                          dat.stats[ATTR::STR] + dat.stats[ATTR::AGI] + dat.stats[ATTR::VIT] + dat.stats[ATTR::INT] + dat.stats[ATTR::DEX] + dat.stats[ATTR::LUK],
                          dat.hair_style, dat.hair_color);
            return NULL;
        }
    }

    for (int i = 0; i < char_num; i++)
    {
        if (name_ignoring_case ? (strcmp(char_dat[i].name, name) == 0) : (strcasecmp(char_dat[i].name, name) == 0))
        {
            char_log.info("Make new char error (%s): (connection #%d, account: %d) slot %hhu, name: %s (actual name of other char: %s), stats: %hhu+%hhu+%hhu+%hhu+%hhu+%hhu=%u, hair: %hhu, hair color: %hhu.\n",
                          "name already exists",
                          fd, sd->account_id, dat.slot, name, char_dat[i].name,
                          dat.stats[ATTR::STR], dat.stats[ATTR::AGI], dat.stats[ATTR::VIT], dat.stats[ATTR::INT], dat.stats[ATTR::DEX], dat.stats[ATTR::LUK],
                          dat.stats[ATTR::STR] + dat.stats[ATTR::AGI] + dat.stats[ATTR::VIT] + dat.stats[ATTR::INT] + dat.stats[ATTR::DEX] + dat.stats[ATTR::LUK],
                          dat.hair_style, dat.hair_color);
            return NULL;
        }
        if (char_dat[i].account_id == sd->account_id
            && char_dat[i].char_num == dat.slot)
        {
            char_log.info("Make new char error (%s): (connection #%d, account: %d) slot %hhu, name: %s (name of other char: %s), stats: %hhu+%hhu+%hhu+%hhu+%hhu+%hhu=%u, hair: %hhu, hair color: %hhu.\n",
                          "slot already used",
                          fd, sd->account_id, dat.slot, name, char_dat[i].name,
                          dat.stats[ATTR::STR], dat.stats[ATTR::AGI], dat.stats[ATTR::VIT], dat.stats[ATTR::INT], dat.stats[ATTR::DEX], dat.stats[ATTR::LUK],
                          dat.stats[ATTR::STR] + dat.stats[ATTR::AGI] + dat.stats[ATTR::VIT] + dat.stats[ATTR::INT] + dat.stats[ATTR::DEX] + dat.stats[ATTR::LUK],
                          dat.hair_style, dat.hair_color);
            return NULL;
        }
    }

    if (strcmp(whisper_server_name, name) == 0)
    {
        char_log.info("Make new char error (%s): (connection #%d, account: %d) slot %hhu, name: %s, stats: %hhu+%hhu+%hhu+%hhu+%hhu+%hhu=%u, hair: %hhu, hair color: %hhu.\n",
                      "name used is whisper name for server",
                      fd, sd->account_id, dat.slot, name,
                      dat.stats[ATTR::STR], dat.stats[ATTR::AGI], dat.stats[ATTR::VIT], dat.stats[ATTR::INT], dat.stats[ATTR::DEX], dat.stats[ATTR::LUK],
                      dat.stats[ATTR::STR] + dat.stats[ATTR::AGI] + dat.stats[ATTR::VIT] + dat.stats[ATTR::INT] + dat.stats[ATTR::DEX] + dat.stats[ATTR::LUK],
                      dat.hair_style, dat.hair_color);
        return NULL;
    }

    if (char_num >= char_max)
    {
        char_max += 256;
        RECREATE(char_dat, struct mmo_charstatus, char_max);
        RECREATE(online_char_server_fd, int, char_max);
        for (int j = char_max - 256; j < char_max; j++)
            online_char_server_fd[j] = -1;
    }

    char_log.info("Creation of New Character: (connection #%d, account: %d) slot %hhu, character Name: %s, stats: %hhu+%hhu+%hhu+%hhu+%hhu+%hhu=%u, hair: %hhu, hair color: %hhu. [%s]\n",
                  fd, sd->account_id, dat.slot, name,
                  dat.stats[ATTR::STR], dat.stats[ATTR::AGI], dat.stats[ATTR::VIT], dat.stats[ATTR::INT], dat.stats[ATTR::DEX], dat.stats[ATTR::LUK],
                  dat.stats[ATTR::STR] + dat.stats[ATTR::AGI] + dat.stats[ATTR::VIT] + dat.stats[ATTR::INT] + dat.stats[ATTR::DEX] + dat.stats[ATTR::LUK],
                  dat.hair_style, dat.hair_color, session[fd]->client_addr.to_string().c_str());

    struct mmo_charstatus *chardat = &char_dat[char_num];
    memset(chardat, 0, sizeof(struct mmo_charstatus));

    chardat->char_id = char_id_count++;
    chardat->account_id = sd->account_id;
    chardat->char_num = dat.slot;
    STRZCPY(chardat->name, name);
//     chardat->pc_class = 0;
    chardat->base_level = 1;
    chardat->job_level = 1;
    chardat->base_exp = 0;
    chardat->job_exp = 0;
    chardat->zeny = 0;
    for (ATTR attr : ATTRs)
        chardat->stats[attr] = dat.stats[attr];
    chardat->max_hp = 40 * (100 + chardat->stats[ATTR::VIT]) / 100;
    chardat->max_sp = 11 * (100 + chardat->stats[ATTR::INT]) / 100;
    chardat->hp = chardat->max_hp;
    chardat->sp = chardat->max_sp;
    chardat->status_point = 0;
    chardat->skill_point = 0;
    chardat->option = 0;
    chardat->party_id = 0;
    chardat->hair = dat.hair_style;
    chardat->hair_color = dat.hair_color;
    chardat->weapon = 1;
    chardat->shield = 0;
    chardat->head = 0;
    chardat->chest = 0;
    chardat->legs = 0;
    chardat->last_point = start_point;
    chardat->save_point = start_point;
    char_num++;

    return chardat;
}

/// Generate online.txt and online.html
// These files should be served in the root of the domain.
static void create_online_files(void)
{
    if (!online_display_option)
        return;

    unsigned players = 0;
    int id[char_num];
    // sort online characters.
    for (int i = 0; i < char_num; i++)
    {
        if (online_char_server_fd[i] == -1)
            continue;
        id[players] = i;
        // TODO: put the for i loop inside the switch (maybe put switch cases in new func?)
        switch (online_sorting_option)
        {
        /// by name (case insensitive)
        case 1:
            for (int j = 0; j < players; j++)
            {
                int casecmp = strcasecmp(char_dat[i].name, char_dat[id[j]].name);
                if (casecmp > 0)
                    continue;
                if (casecmp == 0 && strcmp(char_dat[i].name, char_dat[id[j]].name) > 0)
                    continue;
                for (int k = players; k > j; k--)
                    id[k] = id[k - 1];
                id[j] = i;  // id[players]
                break;
            }
            break;
        /// by zeny
        case 2:
            for (int j = 0; j < players; j++)
            {
                if (char_dat[i].zeny > char_dat[id[j]].zeny)
                    continue;
                // if same number of zenys, we sort by name.
                if (char_dat[i].zeny == char_dat[id[j]].zeny &&
                    strcasecmp(char_dat[i].name, char_dat[id[j]].name) > 0)
                    continue;
                for (int k = players; k > j; k--)
                    id[k] = id[k - 1];
                id[j] = i;  // id[players]
                break;
            }
            break;
        // by experience
        case 3:
            for (int j = 0; j < players; j++)
            {
                if (char_dat[i].base_level > char_dat[id[j]].base_level)
                    continue;
                if (char_dat[i].base_level == char_dat[id[j]].base_level
                        && char_dat[i].base_exp > char_dat[id[j]].base_exp)
                    continue;
                for (int k = players; k > j; k--)
                    id[k] = id[k - 1];
                id[j] = i;  // id[players]
                break;
            }
            break;
        // /// by job (and job level)
        // case 4: (deleted)
        /// by map, then name
        case 5:
            for (int j = 0; j < players; j++)
            {
                /// Don't use strcasecmp as maps can't be the same except case
                int cpm_result = char_dat[i].last_point.map == char_dat[id[j]].last_point.map;
                if (cpm_result > 0)
                    continue;
                if (cpm_result == 0 && strcasecmp(char_dat[i].name, char_dat[id[j]].name) > 0)
                    continue;
                for (int k = players; k > j; k--)
                    id[k] = id[k - 1];
                id[j] = i;  // id[players]
                break;
            }
            break;
        default:
            break;
        }
        players++;
    }

    // write files
    FILE *txt = fopen_(online_txt_filename, "w");
    if (!txt)
        return;
    FILE *html = fopen_(online_html_filename, "w");
    if (!html)
    {
        fclose_(txt);
        return;
    }
    const char *timestr = stamp_now(false);

    // write heading
    fprintf(html, "<HTML>\n");
    fprintf(html, "  <META http-equiv=\"Refresh\" content=\"%d\">\n", online_refresh_html);
    fprintf(html, "  <HEAD>\n");
    fprintf(html, "    <TITLE>%u player%s on %s</TITLE>\n", players,
             players == 1 ? "" : "s", server_name);
    fprintf(html, "  </HEAD>\n");
    fprintf(html, "  <BODY>\n");
    fprintf(html, "    <H3>%u player%s on %s (%s):</H3>\n", players,
             players == 1 ? "" : "s", server_name, timestr);

    fprintf(html, "    <H3>%u player%s on %s (%s):</H3>\n", players,
             players == 1 ? "" : "s", server_name, timestr);
    fprintf(txt, "%u player%s on %s (%s):\n", players, players == 1 ? "" : "s",
             server_name, timestr);
    fprintf(txt, "\n");

    int j = 0;
    if (!players)
        goto end_online_files;
    // count the number of characters in text line
    // the sole purpose of this is to put dashes in the second line
    fprintf(html, "    <table border=\"1\" cellspacing=\"1\">\n");
    fprintf(html, "      <tr>\n");
    if (online_display_option & 65)
        fprintf(html, "        <td><b>Name</b></td>\n");
    if (online_display_option & 64)
    {
        fprintf(txt, "Name                      GM? ");
        j += 30;
    }
    else if (online_display_option & 1)
    {
        fprintf(txt, "Name                     ");
        j += 25;
    }
    if (online_display_option & 4)
    {
        fprintf(html, "        <td><b>Levels</b></td>\n");
        fprintf(txt, " Levels ");
        j += 8;
    }
    if (online_display_option & 24)
    {
        fprintf(html, "        <td><b>Location</b></td>\n");
        if (online_display_option & 16)
        {
            fprintf(txt, "Location     ( x , y ) ");
            j += 23;
        }
        else
        {
            fprintf(txt, "Location     ");
            j += 13;
        }
    }
    if (online_display_option & 32)
    {
        fprintf(html, "        <td ALIGN=CENTER><b>zenys</b></td>\n");
        fprintf(txt, "          Zenys ");
        j += 16;
    }
    fprintf(html, "      </tr>\n");
    fprintf(txt, "\n");
    for (int k = 0; k < j; k++)
        fprintf(txt, "-");
    fprintf(txt, "\n");

    // display each player.
    for (int i = 0; i < players; i++)
    {
        struct mmo_charstatus *chardat = &char_dat[id[i]];
        // get id of the character (more speed)
        fprintf(html, "      <tr>\n");
        // displaying the character name
        if (online_display_option & 65)
        {
            // without/with 'GM' display
            gm_level_t l = isGM(char_dat[j].account_id);
            if (online_display_option & 64)
            {
                if (l >= online_gm_display_min_level)
                    fprintf(txt, "%-24s (GM) ", chardat->name);
                else
                    fprintf(txt, "%-24s      ", chardat->name);
            }
            else
                fprintf(txt, "%-24s ", chardat->name);
            // name of the character in the html (no < >, because that create problem in html code)
            fprintf(html, "        <td>");
            if ((online_display_option & 64) && l >= online_gm_display_min_level)
                fprintf(html, "<b>");
            for (int k = 0; chardat->name[k]; k++)
            {
                switch (chardat->name[k])
                {
                case '<':
                    fprintf(html, "&lt;");
                    break;
                case '>':
                    fprintf(html, "&gt;");
                    break;
                case '&':
                    fprintf(html, "&amp;");
                    break;
                default:
                    fprintf(html, "%c", chardat->name[k]);
                        break;
                };
            }
            if ((online_display_option & 64) && l >= online_gm_display_min_level)
                fprintf(html, "</b> (GM)");
            fprintf(html, "</td>\n");
        }
        // displaying of the job
        if (online_display_option & 4)
        {
            fprintf(html, "        <td>%d/%d</td>\n", chardat->base_level, chardat->job_level);
            fprintf(txt, "%3d/%3d ", chardat->base_level, chardat->job_level);
        }
        // displaying of the map
        if (online_display_option & 24)
        {
            // 8 or 16
            // prepare map name
            fixed_string<16> temp = chardat->last_point.map;
            char *period = strchr(&temp, '.');
            if (period)
                *period = '\0';
            // write map name
            if (online_display_option & 16)
            {
                // map-name AND coordinates
                fprintf(html, "        <td>%s (%d, %d)</td>\n",
                         &temp, chardat->last_point.x,
                         chardat->last_point.y);
                fprintf(txt, "%-12s (%3d,%3d) ", &temp,
                         chardat->last_point.x,
                         chardat->last_point.y);
            }
            else
            {
                fprintf(html, "        <td>%s</td>\n", &temp);
                fprintf(txt, "%-12s ", &temp);
            }
        }
        // displaying number of zenys
        if (online_display_option & 32)
        {
            // write number of zenys
            if (chardat->zeny == 0)
            {
                // if no zeny
                fprintf(html, "        <td ALIGN=RIGHT>no zeny</td>\n");
                fprintf(txt, "        no zeny ");
            }
            else
            {
                fprintf(html, "        <td ALIGN=RIGHT>%d z</td>\n", chardat->zeny);
                fprintf(txt, "%13d z ", chardat->zeny);
            }
        }
        fprintf(txt, "\n");
        fprintf(html, "      </tr>\n");
    }
    fprintf(html, "    </table>\n");
    fprintf(txt, "\n");

end_online_files:
    fprintf(html, "  </BODY>\n");
    fprintf(html, "</HTML>\n");
    fclose_(html);
    fclose_(txt);
}

/// Calculate the total number of users on all map-servers
static unsigned count_users(void) __attribute__((pure));
static unsigned count_users(void)
{
    unsigned users = 0;
    for (int i = 0; i < MAX_MAP_SERVERS; i++)
        if (server_fd[i] >= 0)
            users += server[i].users;
    return users;
}

/// Return item type that is equipped in the given slot
static int find_equip_view(struct mmo_charstatus *p, EPOS equipmask)
{
    for (int i = 0; i < MAX_INVENTORY; i++)
        if (p->inventory[i].nameid && p->inventory[i].amount
                && p->inventory[i].equip & equipmask)
            return p->inventory[i].nameid;
    return 0;
}

/// List slots
static void mmo_char_send006b(int fd, CharSessionData *sd)
{
    int found_num = 0;
    for (int i = 0; i < char_num; i++)
    {
        if (char_dat[i].account_id == sd->account_id)
        {
            sd->found_char[found_num] = &char_dat[i];
            found_num++;
            if (found_num == MAX_CHARS_PER_ACCOUNT)
                break;
        }
    }
    for (int i = found_num; i < MAX_CHARS_PER_ACCOUNT; i++)
        sd->found_char[i] = NULL;

    const int offset = 24;

    memset(WFIFOP(fd, 0), 0, offset + found_num * 106);
    WFIFOW(fd, 0) = 0x6b;
    WFIFOW(fd, 2) = offset + found_num * 106;

    for (int i = 0; i < found_num; i++)
    {
        struct mmo_charstatus *p = sd->found_char[i];
        int j = offset + (i * 106);

        WFIFOL(fd, j) = p->char_id;
        WFIFOL(fd, j + 4) = p->base_exp;
        WFIFOL(fd, j + 8) = p->zeny;
        WFIFOL(fd, j + 12) = p->job_exp;
        // [Fate] We no longer reveal this to the player, as its meaning is weird.
        WFIFOL(fd, j + 16) = 0;    //p->job_level;

        WFIFOW(fd, j + 20) = find_equip_view(p, EPOS::SHOES);
        WFIFOW(fd, j + 22) = find_equip_view(p, EPOS::GLOVES);
        WFIFOW(fd, j + 24) = find_equip_view(p, EPOS::CAPE);
        WFIFOW(fd, j + 26) = find_equip_view(p, EPOS::MISC1);
        WFIFOL(fd, j + 28) = p->option;

        WFIFOL(fd, j + 32) = 0;//p->karma;
        WFIFOL(fd, j + 36) = 0;//p->manner;

        WFIFOW(fd, j + 40) = p->status_point;
        WFIFOW(fd, j + 42) = min(p->hp, 0x7fff);
        WFIFOW(fd, j + 44) = min(p->max_hp, 0x7fff);
        WFIFOW(fd, j + 46) = min(p->sp, 0x7fff);
        WFIFOW(fd, j + 48) = min(p->max_sp, 0x7fff);
        WFIFOW(fd, j + 50) = DEFAULT_WALK_SPEED;   // p->speed;
        WFIFOW(fd, j + 52) = 0; // p->pc_class;
        WFIFOW(fd, j + 54) = p->hair;
        WFIFOW(fd, j + 56) = p->weapon;
        WFIFOW(fd, j + 58) = p->base_level;
        WFIFOW(fd, j + 60) = p->skill_point;
        WFIFOW(fd, j + 62) = p->legs;
        WFIFOW(fd, j + 64) = p->shield;
        WFIFOW(fd, j + 66) = p->head;
        WFIFOW(fd, j + 68) = p->chest;
        WFIFOW(fd, j + 70) = p->hair_color;
        WFIFOW(fd, j + 72) = find_equip_view(p, EPOS::MISC2);
//      WFIFOW(fd,j+72) = p->clothes_color;

        memcpy(WFIFOP(fd, j + 74), p->name, 24);

        WFIFOB(fd, j + 98) = min(p->stats[ATTR::STR], 255);
        WFIFOB(fd, j + 99) = min(p->stats[ATTR::AGI], 255);
        WFIFOB(fd, j + 100) = min(p->stats[ATTR::VIT], 255);
        WFIFOB(fd, j + 101) = min(p->stats[ATTR::INT], 255);
        WFIFOB(fd, j + 102) = min(p->stats[ATTR::DEX], 255);
        WFIFOB(fd, j + 103) = min(p->stats[ATTR::LUK], 255);
        WFIFOB(fd, j + 104) = p->char_num;
    }

    WFIFOSET(fd, WFIFOW(fd, 2));
}

/// Set a permanent variable on an account, rather than on a character
// TODO inline this ?
static void set_account_reg2(account_t acc, size_t num, struct global_reg *reg)
{
    for (int i = 0; i < char_num; i++)
    {
        // This happens more than once!
        if (char_dat[i].account_id == acc)
        {
            memcpy(char_dat[i].account_reg2, reg,
                    ACCOUNT_REG2_NUM * sizeof(struct global_reg));
            char_dat[i].account_reg2_num = num;
        }
    }
}

/// Divorce a character from it's partner and let the map server know
static void char_divorce(struct mmo_charstatus *cs)
{
    if (!cs)
        return;

    uint8_t buf[10];
    WBUFW(buf, 0) = 0x2b12;
    WBUFL(buf, 2) = cs->char_id;
    if (cs->partner_id <= 0)
    {
        // partner id 0 means failure
        WBUFL(buf, 6) = 0;
        mapif_sendall(buf, 10);
        return;
    }

    for (int i = 0; i < char_num; i++)
    {
        if (char_dat[i].char_id != cs->partner_id)
            continue;
        // Normal case
        if (char_dat[i].partner_id == cs->char_id)
        {
            WBUFL(buf, 6) = cs->partner_id;
            mapif_sendall(buf, 10);
            cs->partner_id = 0;
            char_dat[i].partner_id = 0;
            return;
        }
        // The other char doesn't have us as their partner, so just clear our partner
        // Don't worry about this, as the map server should verify itself that
        // the other doesn't have us as a partner, and so won't mess with their marriage
        WBUFL(buf, 6) = cs->partner_id;
        mapif_sendall(buf, 10);
        cs->partner_id = 0;
        return;
    }

    // Our partner wasn't found, so just clear our marriage
    WBUFL(buf, 6) = cs->partner_id;
    cs->partner_id = 0;
    mapif_sendall(buf, 10);
}

/// Forcibly disconnect an online player, by account (from login server)
// called when sex changed, account deleted, or banished
static void disconnect_player(account_t account_id)
{
    // disconnect player if online on char-server
    for (int i = 0; i < fd_max; i++)
    {
        if (!session[i])
            continue;
        CharSessionData *sd = static_cast<CharSessionData*>(session[i]->session_data);
        if (!sd)
            continue;
        if (sd->account_id == account_id)
        {
            session[i]->eof = 1;
            return;
        }
    }
}

/// Delete a character safely, removing references
static void char_delete(struct mmo_charstatus *cs)
{
    if (cs->party_id)
        inter_party_leave(cs->party_id, cs->account_id);
    if (cs->partner_id)
        char_divorce(cs);

    /// Force the character to leave all map servers
    // BUG: it shouldn't have to kick another character of the same account
    uint8_t buf[6];
    WBUFW(buf, 0) = 0x2afe;
    WBUFL(buf, 2) = cs->account_id;
    mapif_sendall(buf, 6);
}

static void parse_tologin(int fd)
{
    if (fd != login_fd)
    {
        char_log.fatal("Error: %s called but isn't the login-server\n", __func__);
    }

    if (session[fd]->eof)
    {
        char_log.info("Char-server can't connect to login-server (connection #%d).\n",
                      fd);
        login_fd = -1;
        close(fd);
        delete_session(fd);
        return;
    }

    while (RFIFOREST(fd) >= 2)
    {
        switch (RFIFOW(fd, 0))
        {
        case 0x2711:
            if (RFIFOREST(fd) < 3)
                return;
        {
            if (RFIFOB(fd, 2))
            {
                printf("Can not connect to login-server.\n");
                printf("The server communication passwords (default s1/p1) is probably invalid.\n");
                printf("Also, please make sure your accounts file (default: accounts.txt) has those values present.\n");
                printf("If you changed the communication passwords, change them back at map_athena.conf and char_athena.conf\n");
                exit(1);
            }
            printf("Connected to login-server (connection #%d).\n", fd);
            // if no map-server already connected, display a message...
            for (int i = 0; i < MAX_MAP_SERVERS; i++)
                // if map-server online and at least 1 map
                if (server_fd[i] >= 0 && server[i].map[0][0])
                    goto end_x7211;
            printf("Awaiting maps from map-server.\n");
        }
        end_x7211:
            RFIFOSKIP(fd, 3);
            break;

        /// Login server auth result
        // uint16_t packet, uint32_t acc, uint8_t failed, char email[40], uint32_t time limit
        case 0x2713:
            if (RFIFOREST(fd) < 51)
                return;
        {
            for (int i = 0; i < fd_max; i++)
            {
                if (!session[i])
                    continue;
                CharSessionData *sd = static_cast<CharSessionData*>(session[i]->session_data);
                if (!sd || sd->account_id != RFIFOL(fd, 2))
                    continue;
                if (RFIFOB(fd, 6))
                {
                    // login server denied the auth
                    WFIFOW(i, 0) = 0x6c;
                    WFIFOB(i, 2) = 0x42;
                    WFIFOSET(i, 3);
                    goto end_x2713;
                }
                if (max_connect_user && count_users() >= max_connect_user)
                {
                    // refuse connection: too many online players
                    WFIFOW(i, 0) = 0x6c;
                    WFIFOW(i, 2) = 0;
                    WFIFOSET(i, 3);
                    goto end_x2713;
                }
                // connection ok
                STRZCPY(sd->email, sign_cast<const char *>(RFIFOP(fd, 7)));
                if (!e_mail_check(sd->email))
                    STRZCPY(sd->email, "a@a.com");
                sd->connect_until_time = RFIFOL(fd, 47);
                // send characters to player
                mmo_char_send006b(i, sd);
                goto end_x2713;
            } // for i in fds
        }
        end_x2713:
            RFIFOSKIP(fd, 51);
            break;

        /// Receiving of an e-mail/time limit from the login-server
        // (answer of a request because a player comes back from map-server to char-server) by [Yor]
        // uint16_t packet, uint32_t acc, char email[40], uint32_t time limit
        case 0x2717:
            if (RFIFOREST(fd) < 50)
                return;
        {
            for (int i = 0; i < fd_max; i++)
            {
                if (!session[i])
                    continue;
                CharSessionData *sd = static_cast<CharSessionData*>(session[i]->session_data);
                if (!sd || sd->account_id != RFIFOL(fd, 2))
                    continue;
                STRZCPY(sd->email, sign_cast<const char *>(RFIFOP(fd, 6)));
                if (!e_mail_check(sd->email))
                    STRZCPY(sd->email, "a@a.com");
                sd->connect_until_time = RFIFOL(fd, 46);
                goto end_x2717;
            }
        }
        end_x2717:
            RFIFOSKIP(fd, 50);
            break;

        /// gm reply
        // uint16_t packet, uint32_t acc, uint32_t gm_level
        case 0x2721:
            if (RFIFOREST(fd) < 10)
                return;
        {
            session[fd]->rfifo_change_packet(0x2b0b);
            mapif_sendall(RFIFOP(fd, 0), 10);
        }
            RFIFOSKIP(fd, 10);
            break;

        /// sex changed
        // uint16_t packet, uint32_t acc, uint8_t sex
        case 0x2723:
            if (RFIFOREST(fd) < 7)
                return;
        {
            account_t acc = RFIFOL(fd, 2);
            enum gender sex = static_cast<enum gender>(RFIFOB(fd, 6));
            if (!acc)
                // ?
                goto reply_x2323_x2b0d;
            for (int i = 0; i < char_num; i++)
            {
                if (char_dat[i].account_id != acc)
                    continue;
                // Note: the loop body may be executed more than once
                char_dat[i].sex = sex;
                // to avoid any problem with equipment and invalid sex, equipment is unequiped.
                for (int j = 0; j < MAX_INVENTORY; j++)
                    char_dat[i].inventory[j].equip = EPOS::NONE;
                char_dat[i].weapon = 0;
                char_dat[i].shield = 0;
                char_dat[i].head = 0;
                char_dat[i].chest = 0;
                char_dat[i].legs = 0;
            }
            // disconnect player if online on char-server
            disconnect_player(acc);
        reply_x2323_x2b0d:
            session[fd]->rfifo_change_packet(0x2b0d);
            mapif_sendall(RFIFOP(fd, 0), 7);
        }
            RFIFOSKIP(fd, 7);
            break;

        /// broadcast message (no answer)
        // uint16_t packet, uint16_t blue, uint32_t msglen, char msg[msglen]
        case 0x2726:
            if (RFIFOREST(fd) < 8 || RFIFOREST(fd) < (8 + RFIFOL(fd, 4)))
                return;
        {
            if (RFIFOL(fd, 4) < 1)
            {
                char_log.info("Receiving a message for broadcast, but message is void.\n");
                goto end_x2726;
            }
            // at least 1 map-server
            for (int i = 0; i < MAX_MAP_SERVERS; i++)
                if (server_fd[i] >= 0)
                    goto tmp_x2726;
            char_log.info("'ladmin': Receiving a message for broadcast, but no map-server is online.\n");
            goto end_x2726;
        tmp_x2726: ;
            char message[RFIFOL(fd, 4) + 1];
            STRZCPY(message, sign_cast<const char *>(RFIFOP(fd, 8)));
            remove_control_chars(message);
            // remove all first spaces
            char *p = message;
            while (p[0] == ' ')
                p++;
            // if message is only composed of spaces
            if (p[0] == '\0')
            {
                char_log.info("Receiving a message for broadcast, but message is only a lot of spaces.\n");
                goto end_x2726;
            }
            char_log.info("'ladmin': Receiving a message for broadcast: %s\n",
                          message);
            // split message to max 80 char
            size_t len = strlen(p);
            while (p[0])
            {
                while (p[0] == ' ')
                    p++;
                if (!p[0])
                    break;
                char *last_space = p + min(len, 80);
                while (last_space != p && *last_space != ' ')
                    last_space--;
                if (last_space == p)
                    last_space = p + min(len, 80);

                // send broadcast to all map-servers
                uint8_t buf[84] = {};
                WBUFW(buf, 0) = 0x3800;
                WBUFW(buf, 2) = 4 + (last_space - p) + 1;
                memcpy(WBUFP(buf, 4), p, last_space - p);
                mapif_sendall(buf, WBUFW(buf, 2));
                p = last_space;
            } // while p
        }
        end_x2726:
            RFIFOSKIP(fd, 8 + RFIFOL(fd, 4));
            break;

        /// set account_reg2
        // uint16_t packet, uint16_t packetlen, uint32_t acc, {char varname[32], int32_t val}[]
        case 0x2729:
            if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
        {
            struct global_reg reg[ACCOUNT_REG2_NUM];
            account_t acc = RFIFOL(fd, 4);
            int p = 8;
            int j;
            for (j = 0; p < RFIFOW(fd, 2) && j < ACCOUNT_REG2_NUM; j++)
            {
                STRZCPY(reg[j].str, sign_cast<const char *>(RFIFOP(fd, p)));
                p += 32;
                reg[j].value = RFIFOL(fd, p);
                p += 4;
            }
            set_account_reg2(acc, j, reg);
            // ?? need to send a ban if something about a dirty login ?
            session[fd]->rfifo_change_packet(0x2b11);
            mapif_sendall(RFIFOP(fd, 0), RFIFOW(fd, 2));
            RFIFOSKIP(fd, RFIFOW(fd, 2));
        }
            break;

        case 0x7531:
            if (RFIFOREST(fd) < 10)
                return;
        {
            const Version& server_version = *reinterpret_cast<const Version *>(RFIFOP(login_fd, 2));
            if (!(server_version.what_server & ATHENA_SERVER_LOGIN))
            {
                char_log.fatal("Not a login server!");
            }
            if (server_version != tmwAthenaVersion)
            {
                char_log.fatal("Version mismatch!");
            }
        }
            RFIFOSKIP(fd, 10);
            break;

        /// [Fate] Itemfrob package: forwarded from login-server
        // uint16_t packet, uint32_t srcid, uint32_t dstid
        case 0x7924:
            if (RFIFOREST(fd) < 10)
                return;
        {
            int source_id = RFIFOL(fd, 2);
            int dest_id = RFIFOL(fd, 6);
            session[fd]->rfifo_change_packet(0x2afa);
            mapif_sendall(RFIFOP(fd, 0), 10);
            for (int i = 0; i < char_num; i++)
            {
                struct mmo_charstatus *c = &char_dat[i];
                int changes = 0;
#define FIX(v) if (v == source_id) {v = dest_id; ++changes; }
                for (int j = 0; j < MAX_INVENTORY; j++)
                    FIX(c->inventory[j].nameid);
                FIX(c->weapon);
                FIX(c->shield);
                FIX(c->head);
                FIX(c->chest);
                FIX(c->legs);

                struct storage *s = account2storage(c->account_id);
                if (s)
                    for (int j = 0; j < s->storage_amount; j++)
                        FIX(s->storage_[j].nameid);
#undef FIX
                if (changes)
                    char_log.info("itemfrob(%d -> %d):  `%s'(%d, account %d): changed %d times\n",
                                  source_id, dest_id, c->name, c->char_id,
                                  c->account_id, changes);
            }
            mmo_char_sync();
            inter_storage_save();
        }
            RFIFOSKIP(fd, 10);
            break;

        /// Account deletion notification (from login-server)
        // uint16_t packet, uint32_t acc
        case 0x2730:
            if (RFIFOREST(fd) < 6)
                return;
        {
            account_t acc = RFIFOL(fd, 2);
            // Deletion of all characters of the account
            for (int i = 0; i < char_num; i++)
            {
                if (char_dat[i].account_id != acc)
                    continue;
                char_delete(&char_dat[i]);
                char_num--;
                if (i == char_num)
                    break;
                // remove blank in table
                char_dat[i] = char_dat[char_num];
                if (char_dat[i].account_id == acc)
                {
                    // if the tail character was also to be deleted,
                    // we don't have to clear any references to it,
                    // just feed it (in the new location) to the loop again
                    i--;
                    continue;
                }
                // fix any references to the moved character
                for (int j = 0; j < fd_max; j++)
                {
                    if (!session[j])
                        continue;
                    CharSessionData *sd2 = static_cast<CharSessionData*>(session[j]->session_data);
                    if (!sd2 || sd2->account_id != char_dat[char_num].account_id)
                        continue;
                    for (int k = 0; k < MAX_CHARS_PER_ACCOUNT; k++)
                    {
                        if (sd2->found_char[k] == &char_dat[char_num])
                        {
                            sd2->found_char[k] = &char_dat[i];
                            break;
                        }
                    }
                    break;
                } // for j up to fd_max
            } // for i up to char_num
            // Deletion of the storage
            inter_storage_delete(acc);
            // send to all map-servers to disconnect the player
            session[fd]->rfifo_change_packet(0x2b13);
            mapif_sendall(WFIFOP(fd, 0), 6);
            // disconnect player if online on char-server
            disconnect_player(RFIFOL(fd, 2));
        }
            RFIFOSKIP(fd, 6);
            break;

        /// State change of account/ban notification (from login-server) by [Yor]
        // uint16_t packet, uint32_t acc, uint8_t ban? , uint32_t end date or new state
        case 0x2731:
            if (RFIFOREST(fd) < 11)
                return;
            // send to all map-servers to disconnect the player
        {
            session[fd]->rfifo_change_packet(0x2b14);
            mapif_sendall(RFIFOP(fd, 0), 11);
            // disconnect player if online on char-server
            disconnect_player(RFIFOL(fd, 2));
        }
            RFIFOSKIP(fd, 11);
            break;

        /// Receiving GM acounts info from login-server (by [Yor])
        // uint16_t packet, uint16_t byte_len, {uint32_t acc, uint8_t}[]
        case 0x2732:
            if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                return;
        {
            free(gm_accounts);
            GM_num = (RFIFOW(fd, 2) - 4) / 5;
            CREATE(gm_accounts, struct gm_account, GM_num);
            for (int i = 0; i < GM_num; i++)
            {
                gm_accounts[i].account_id = RFIFOL(fd, 4 + 5*i);
                gm_accounts[i].level = RFIFOB(fd, 4 + 5*i + 4);
            }
            char_log.info("From login-server: receiving of %d GM accounts information.\n",
                          GM_num);
            update_online = time(NULL) + 8;
            /// update online players files (perhaps some online players change of GM level)
            create_online_files();
            // send new gm acccounts level to map-servers
            session[fd]->rfifo_change_packet(0x2b15);
            mapif_sendall(RFIFOP(fd, 0), RFIFOW(fd, 2));
        }
            RFIFOSKIP(fd, RFIFOW(fd, 2));
            break;

        /// change password reply
        // uint16_t packet, uint32_t acc, uint8_t status
        case 0x2741:
            if (RFIFOREST(fd) < 7)
                return;
        {
            account_t acc = RFIFOL(fd, 2);
            uint8_t status = RFIFOB(fd, 6);

            for (int i = 0; i < fd_max; i++)
            {
                if (!session[i])
                    continue;
                CharSessionData *sd = static_cast<CharSessionData*>(session[i]->session_data);
                if (!sd || sd->account_id != acc)
                    continue;
                WFIFOW(i, 0) = 0x62;
                WFIFOB(i, 2) = status;
                WFIFOSET(i, 3);
                goto end_x2741;
            }
        }
        end_x2741:
            RFIFOSKIP(fd, 7);
            break;

        default:
            //TODO: (unconditionally?) log unknown packets?
            session[fd]->eof = 1;
            return;
        }
    }
}

/// Map-server anti-freeze system
static void map_anti_freeze_system(timer_id, tick_t)
{
    for (int i = 0; i < MAX_MAP_SERVERS; i++)
    {
        if (server_fd[i] < 0)
            continue;
        if (!server_freezeflag[i]--)
        {
            char_log.warn("Map-server anti-freeze system: char-server #%d is freezed -> disconnection.\n",
                          i);
            session[server_fd[i]]->eof = 1;
        }
    }
}

static void parse_frommap(int fd)
{
    int id;
    for (id = 0; id < MAX_MAP_SERVERS; id++)
        if (server_fd[id] == fd)
            break;
    if (id == MAX_MAP_SERVERS || session[fd]->eof)
    {
        if (id < MAX_MAP_SERVERS)
        {
            printf("Map-server %d (session #%d) has disconnected.\n", id, fd);
            memset(&server[id], 0, sizeof(struct mmo_map_server));
            server_fd[id] = -1;
            for (int j = 0; j < char_num; j++)
                if (online_char_server_fd[j] == fd)
                    online_char_server_fd[j] = -1;
            // remove all online players of this server from the online list
            // is this really necessary? it will update in 8 seconds anyway
            update_online = time(NULL) + 8;
            create_online_files();
        }
        close(fd);
        delete_session(fd);
        return;
    }

    while (RFIFOREST(fd) >= 2)
    {
        switch (RFIFOW(fd, 0))
        {
            /// Receiving map names list from the map-server
            case 0x2afa:
                if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                    return;
            {
                memset(server[id].map, 0, sizeof(server[id].map));
                int j = 0;
                for (int i = 4; i < RFIFOW(fd, 2); i += 16)
                {
                    server[id].map[j].copy_from(sign_cast<const char *>(RFIFOP(fd, i)));
                    j++;
                }
                char_log.info("Map-Server %d connected: %d maps, from IP %s port %d.\n Map-server %d loading complete.\n",
                              id, j, server[id].ip.to_string().c_str(), server[id].port, id);
                WFIFOW(fd, 0) = 0x2afb;
                WFIFOB(fd, 2) = 0;
                STRZCPY2(sign_cast<char *>(WFIFOP(fd, 3)), whisper_server_name);
                WFIFOSET(fd, 27);
                if (j == 0)
                {
                    char_log.warn("WARNING: Map-Server %d has NO maps.\n", id);
                }
                else
                {
                    // Transmitting maps information to the other map-servers
                    uint8_t buf[j * 16 + 10];
                    WBUFW(buf, 0) = 0x2b04;
                    WBUFW(buf, 2) = j * 16 + 10;
                    WBUFL(buf, 4) = server[id].ip.to_n();
                    WBUFW(buf, 8) = server[id].port;
                    memcpy(WBUFP(buf, 10), RFIFOP(fd, 4), j * 16);
                    mapif_sendallwos(fd, buf, WBUFW(buf, 2));
                }
                // Transmitting the maps of the other map-servers to the new map-server
                for (int x = 0; x < MAX_MAP_SERVERS; x++)
                {
                    if (server_fd[x] < 0 || x == id)
                        continue;
                    WFIFOW(fd, 0) = 0x2b04;
                    WFIFOL(fd, 4) = server[x].ip.to_n();
                    WFIFOW(fd, 8) = server[x].port;
                    int n = 0;
                    for (int i = 0; i < MAX_MAP_PER_SERVER; i++)
                        if (server[x].map[i][0])
                            server[x].map[i].write_to(sign_cast<char *>(WFIFOP(fd, 10 + (n++) * 16)));
                    if (n)
                    {
                        WFIFOW(fd, 2) = n * 16 + 10;
                        WFIFOSET(fd, WFIFOW(fd, 2));
                    }
                } // for x in map servers
            }
                RFIFOSKIP(fd, RFIFOW(fd, 2));
                break;

            // Request authentication
            case 0x2afc:
                if (RFIFOREST(fd) < 22)
                    return;
            {
                for (int i = 0; i < AUTH_FIFO_SIZE; i++)
                {
                    if (auth_fifo[i].delflag ||
                            auth_fifo[i].account_id != RFIFOL(fd, 2) ||
                            auth_fifo[i].char_id != RFIFOL(fd, 6) ||
                            auth_fifo[i].login_id1 != RFIFOL(fd, 10) ||
                            auth_fifo[i].ip.to_n() != RFIFOL(fd, 18))
                        continue;
                    // this is the only place where we might not know login_id2
                    // (map-server asks just after 0x72 packet, which doesn't given the value)
                    if (RFIFOL(fd, 14) && auth_fifo[i].login_id2 != RFIFOL(fd, 14))
                        continue;

                    auth_fifo[i].delflag = 1;
                    WFIFOW(fd, 0) = 0x2afd;
                    /// Note: the struct has changed size during the rewrite!
                    // and the map-server doesn't actually check the size
                    WFIFOW(fd, 2) = 18 + sizeof(struct mmo_charstatus);
                    WFIFOL(fd, 4) = RFIFOL(fd, 2);
                    WFIFOL(fd, 8) = auth_fifo[i].login_id2;
                    WFIFOL(fd, 12) = auth_fifo[i].connect_until_time;
                    auth_fifo[i].char_pos->sex = auth_fifo[i].sex;
                    WFIFOW(fd, 16) = auth_fifo[i].packet_tmw_version;
                    fprintf(stderr, "Client %d uses packet version %d\n",
                             i, auth_fifo[i].packet_tmw_version);
                    /// Note: the struct has changed size during the rewrite!
                    memcpy(WFIFOP(fd, 18), auth_fifo[i].char_pos, sizeof(struct mmo_charstatus));
                    WFIFOSET(fd, WFIFOW(fd, 2));
                    goto end_x2afc;
                }
                WFIFOW(fd, 0) = 0x2afe;
                WFIFOL(fd, 2) = RFIFOL(fd, 2);
                WFIFOSET(fd, 6);
                char_log.debug("auth_fifo search error! account %d not authentified.\n",
                               RFIFOL(fd, 2));
            }
            end_x2afc:
                RFIFOSKIP(fd, 22);
                break;

            /// Number of users on map server
            // uint16_t packet, uint16_t packetlen, uint16_t num_users, uint32_t char_id[num_users]
            case 0x2aff:
                if (RFIFOREST(fd) < 6 || RFIFOREST(fd) < RFIFOW(fd, 2))
                    return;
            {
                server[id].users = RFIFOW(fd, 4);
                if (anti_freeze_enable)
                    server_freezeflag[id] = 5;
                // remove all previously online players of the server
                for (int i = 0; i < char_num; i++)
                    if (online_char_server_fd[i] == id)
                        online_char_server_fd[i] = -1;
                // add online players in the list by [Yor]
                for (int i = 0; i < server[id].users; i++)
                {
                    charid_t char_id = RFIFOL(fd, 6 + i * 4);
                    for (int j = 0; j < char_num; j++)
                        if (char_dat[j].char_id == char_id)
                        {
                            online_char_server_fd[j] = id;
                            break;
                        }
                }
                if (update_online < time(NULL))
                {
                    update_online = time(NULL) + 8;
                    create_online_files();
                    // only every 8 sec. (normally, 1 server send users every 5 sec.) Don't update every time, because that takes time, but only every 2 connection.
                    // it set to 8 sec because is more than 5 (sec) and if we have more than 1 map-server, informations can be received in shifted.
                }
            }
                RFIFOSKIP(fd, 6 + server[id].users * 4);
                break;

            /// char data storage
            // uint16_t packet, uint16_t packetlen, uint32_t acc, uint32_t charid, struct mmo_charstatus
            case 0x2b01:
                if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                    return;
            {
                for (int i = 0; i < char_num; i++)
                {
                    if (char_dat[i].account_id == RFIFOL(fd, 4) &&
                        char_dat[i].char_id == RFIFOL(fd, 8))
                    {
                        char_dat[i] = *reinterpret_cast<const struct mmo_charstatus *>(RFIFOP(fd, 12));
                        break;
                    }
                }
            }
                RFIFOSKIP(fd, RFIFOW(fd, 2));
                break;

            /// player wants to switch to a different character
            // uint16_t packet, uint32_t acc, uint32_t magic[2], uint32_t ip
            case 0x2b02:
                if (RFIFOREST(fd) < 18)
                    return;
            {
                if (auth_fifo_pos >= AUTH_FIFO_SIZE)
                    auth_fifo_pos = 0;
                auth_fifo[auth_fifo_pos].account_id = RFIFOL(fd, 2);
                auth_fifo[auth_fifo_pos].char_id = 0;
                auth_fifo[auth_fifo_pos].login_id1 = RFIFOL(fd, 6);
                auth_fifo[auth_fifo_pos].login_id2 = RFIFOL(fd, 10);
                auth_fifo[auth_fifo_pos].delflag = 2;
                auth_fifo[auth_fifo_pos].char_pos = 0;
                auth_fifo[auth_fifo_pos].connect_until_time = 0;
                auth_fifo[auth_fifo_pos].ip.from_n(RFIFOL(fd, 14));
                auth_fifo_pos++;
                WFIFOW(fd, 0) = 0x2b03;
                WFIFOL(fd, 2) = RFIFOL(fd, 2);
                WFIFOB(fd, 6) = 0;
                WFIFOSET(fd, 7);
            }
                RFIFOSKIP(fd, 18);
                break;

            /// change of map servers
            // uint16_t packet, uint32_t acc, uint32_t magic[2], uint32_t charid, {char name[16], uint16_t x,y uint32_t mapip, uint16_t mapport}, uint8_t gender, uint32_t clientip
            case 0x2b05:
                if (RFIFOREST(fd) < 49)
                    return;
            {
                if (auth_fifo_pos >= AUTH_FIFO_SIZE)
                    auth_fifo_pos = 0;
                WFIFOW(fd, 0) = 0x2b06;
                memcpy(WFIFOP(fd, 2), RFIFOP(fd, 2), 42);
                auth_fifo[auth_fifo_pos].account_id = RFIFOL(fd, 2);
                auth_fifo[auth_fifo_pos].char_id = RFIFOL(fd, 14);
                auth_fifo[auth_fifo_pos].login_id1 = RFIFOL(fd, 6);
                auth_fifo[auth_fifo_pos].login_id2 = RFIFOL(fd, 10);
                auth_fifo[auth_fifo_pos].delflag = 0;
                auth_fifo[auth_fifo_pos].sex = static_cast<enum gender>(RFIFOB(fd, 44));
                auth_fifo[auth_fifo_pos].connect_until_time = 0;
                auth_fifo[auth_fifo_pos].ip.from_n(RFIFOL(fd, 45));
                for (int i = 0; i < char_num; i++)
                {
                    if (char_dat[i].account_id != RFIFOL(fd, 2) ||
                            char_dat[i].char_id != RFIFOL(fd, 14))
                        continue;
                    auth_fifo[auth_fifo_pos].char_pos = &char_dat[i];
                    auth_fifo_pos++;
                    WFIFOL(fd, 6) = 0;
                    goto end_x2b05;
                }
                // not found - invalidate
                WFIFOW(fd, 6) = 1;
            end_x2b05:
                WFIFOSET(fd, 44);
            }
                RFIFOSKIP(fd, 49);
                break;

            /// Get character name from id
            // uint16_t packet, uint32_t charid
            case 0x2b08:
                if (RFIFOREST(fd) < 6)
                    return;
            {
                WFIFOW(fd, 0) = 0x2b09;
                WFIFOL(fd, 2) = RFIFOL(fd, 2);
                STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), unknown_char_name);
                for (int i = 0; i < char_num; i++)
                {
                    if (char_dat[i].char_id == RFIFOL(fd, 2))
                    {
                        STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), char_dat[i].name);
                        break;
                    }
                }
                WFIFOSET(fd, 30);
            }
                RFIFOSKIP(fd, 6);
                break;

            /// request to become GM (@gm command)
            // uint16_t packet, uint16_t packetlen, uint32_t acc, char passwd[]
            case 0x2b0a:
                if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                    return;
            {
                if (login_fd >= 0)
                {
                    WFIFOW(login_fd, 0) = 0x2720;
                    memcpy(WFIFOP(login_fd, 2), RFIFOP(fd, 2),
                            RFIFOW(fd, 2) - 2);
                    WFIFOSET(login_fd, RFIFOW(fd, 2));
                }
                else
                {
                    WFIFOW(fd, 0) = 0x2b0b;
                    WFIFOL(fd, 2) = RFIFOL(fd, 4);
                    WFIFOL(fd, 6) = 0;
                    WFIFOSET(fd, 10);
                }
            }
                RFIFOSKIP(fd, RFIFOW(fd, 2));
                break;

            /// Email change
            // uint16_t packet, uint32_t acc, char oldemail[40], char newemail[40]
            case 0x2b0c:
                if (RFIFOREST(fd) < 86)
                    return;
            {
                if (login_fd < 0)
                    goto end_x2b0c;
                memcpy(WFIFOP(login_fd, 0), RFIFOP(fd, 0), 86);
                WFIFOW(login_fd, 0) = 0x2722;
                WFIFOSET(login_fd, 86);
            }
            end_x2b0c:
                RFIFOSKIP(fd, 86);
                break;

            /// Do special operations on a character name
            // uint16_t packet, uint32_t src_account, char target_name[24], uint16_t what, int16_t timechanges[6]
            // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
            case 0x2b0e:
                if (RFIFOREST(fd) < 44)
                    return;
            {
                account_t acc = RFIFOL(fd, 2);
                char character_name[24];
                STRZCPY(character_name, sign_cast<const char *>(RFIFOP(fd, 6)));
                // prepare answer
                WFIFOW(fd, 0) = 0x2b0f;
                WFIFOL(fd, 2) = acc;
                WFIFOW(fd, 30) = RFIFOW(fd, 30);
                // search character
                struct mmo_charstatus *character = character_by_name(character_name);
                if (!character)
                {
                    // character name not found
                    memcpy(WFIFOP(fd, 6), character_name, 24);
                    WFIFOW(fd, 32) = 1;
                    goto end_x2b0e;
                }

                STRZCPY2(sign_cast<char *>(WFIFOP(fd, 6)), character->name);
                WFIFOW(fd, 32) = 0;
                // FIXME - should GMs have power over those the same GM level?
                if (acc != -1 && isGM(acc) < isGM(character->account_id))
                {
                    WFIFOW(fd, 32) = 2;
                    goto maybe_reply_2b0e;
                }
                if (login_fd < 0)
                {
                    WFIFOW(fd, 32) = 3;
                    goto maybe_reply_2b0e;
                }
                switch (RFIFOW(fd, 30))
                {
                case 1:    // block
                    WFIFOW(login_fd, 0) = 0x2724;
                    WFIFOL(login_fd, 2) = character->account_id;  // account value
                    WFIFOL(login_fd, 6) = 5;   // status of the account
                    WFIFOSET(login_fd, 10);
                    break;
                case 2:    // ban
                    WFIFOW(login_fd, 0) = 0x2725;
                    WFIFOL(login_fd, 2) = character->account_id;
                    WFIFOW(login_fd, 6) = RFIFOW(fd, 32);     // year
                    WFIFOW(login_fd, 8) = RFIFOW(fd, 34);     // month
                    WFIFOW(login_fd, 10) = RFIFOW(fd, 36);    // day
                    WFIFOW(login_fd, 12) = RFIFOW(fd, 38);    // hour
                    WFIFOW(login_fd, 14) = RFIFOW(fd, 40);    // minute
                    WFIFOW(login_fd, 16) = RFIFOW(fd, 42);    // second
                    WFIFOSET(login_fd, 18);
                    break;
                case 3:    // unblock
                    WFIFOW(login_fd, 0) = 0x2724;
                    WFIFOL(login_fd, 2) = character->account_id;
                    WFIFOL(login_fd, 6) = 0;   // status of the account
                    WFIFOSET(login_fd, 10);
                    break;
                case 4:    // unban
                    WFIFOW(login_fd, 0) = 0x272a;
                    WFIFOL(login_fd, 2) = character->account_id;
                    WFIFOSET(login_fd, 6);
                    break;
                case 5:    // changesex
                    WFIFOW(login_fd, 0) = 0x2727;
                    WFIFOL(login_fd, 2) = character->account_id;
                    WFIFOSET(login_fd, 6);
                    break;
                }
            maybe_reply_2b0e:
                // send answer if a player ask, not if the server ask
                if (acc != -1)
                    WFIFOSET(fd, 34);
            }
            end_x2b0e:
                RFIFOSKIP(fd, 44);
                break;

            /// store account_reg
            // uint16_t packet, uint16_t packetlen, {char varname[32], int32_t var}
            case 0x2b10:
                if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd, 2))
                    return;
            {
                struct global_reg reg[ACCOUNT_REG2_NUM];
                account_t acc = RFIFOL(fd, 4);
                int p = 8;
                int j;
                for (j = 0; p < RFIFOW(fd, 2) && j < ACCOUNT_REG2_NUM; j++)
                {
                    STRZCPY(reg[j].str, sign_cast<const char *>(RFIFOP(fd, p)));
                    p += 32;
                    reg[j].value = RFIFOL(fd, p);
                    p += 4;
                }
                set_account_reg2(acc, j, reg);
                if (login_fd >= 0)
                {
                    memcpy(WFIFOP(login_fd, 0), RFIFOP(fd, 0), RFIFOW(fd, 2));
                    WFIFOW(login_fd, 0) = 0x2728;
                    WFIFOSET(login_fd, WFIFOW(login_fd, 2));
                }
            }
                RFIFOSKIP(fd, RFIFOW(fd, 2));
                break;

            /// Divorce
            case 0x2b16:
                if (RFIFOREST(fd) < 4)
                    return;
            {
                for (int i = 0; i < char_num; i++)
                    if (char_dat[i].char_id == RFIFOL(fd, 2))
                    {
                        char_divorce(&char_dat[i]);
                        break;
                    }
            }
                RFIFOSKIP(fd, 6);
                break;

            // else pass parsing to the inter server
            default:
            {
                int r = inter_parse_frommap(fd);
                if (r == 1)
                    // handled - repeat the while loop, it may be a char packet
                    break;
                if (r == 2)
                    // packet too short
                    return;
                /// inter server didn't understand it either
                // TODO parse unknown packets
                printf("char: unknown packet 0x%04x (%d bytes to read in buffer)! (from map).\n",
                        RFIFOW(fd, 0), RFIFOREST(fd));
                session[fd]->eof = 1;
                return;
            }
        } // switch packet
    } // while packet available
}

/// Return index of the server with the given map
static int search_mapserver(const fixed_string<16>& map)
{
    fixed_string<16> temp_map = map;
    char *period = strchr(&temp_map, '.');
    if (period)
        // suppress the '.gat', but conserve the '.' to be sure of the name of the map
        period[1] = '\0';

    for (int i = 0; i < MAX_MAP_SERVERS; i++)
    {
        if (server_fd[i] < 0)
            continue;
        for (int j = 0; server[i].map[j][0]; j++)
            if (server[i].map[j] == temp_map)
            {
                return i;
            }
    }
    return -1;
}

/// Check if IP is LAN instead of WAN
// (send alternate map IP)
static bool lan_ip_check(IP_Address addr)
{
    bool lancheck = lan_mask.covers(addr);
    printf("LAN test (result): %s source\033[0m.\n",
            lancheck ? "\033[1;36mLAN" : "\033[1;32mWAN");
    return lancheck;
}

static void parse_char(int fd)
{
    if (fd == login_fd)
    {
        char_log.fatal("Login fd (%d) called %s", fd, __func__);
    }
    if (login_fd < 0 || session[fd]->eof)
    {
        // disconnect any player (already connected to char-server or coming back from map-server) if login-server is diconnected.
        close(fd);
        delete_session(fd);
        return;
    }

    CharSessionData *sd = static_cast<CharSessionData*>(session[fd]->session_data);

    while (RFIFOREST(fd) >= 2)
    {
        switch (RFIFOW(fd, 0))
        {
        /// change password
        // uint16_t packet, char old[24], char new[24]
        case 0x61:
            if (RFIFOREST(fd) < 50)
                return;
        {
            WFIFOW(login_fd, 0) = 0x2740;
            WFIFOL(login_fd, 2) = sd->account_id;
            memcpy(WFIFOP(login_fd, 6), RFIFOP(fd, 2), 48);
            WFIFOSET(login_fd, 54);
        }
            RFIFOSKIP(fd, 50);
            break;

        /// Connection request
        case 0x65:
            if (RFIFOREST(fd) < 17)
                return;
        {
            account_t acc = RFIFOL(fd, 2);
            gm_level_t GM_value = isGM(acc);
            if (GM_value)
                printf("Account Logged On; Account ID: %d (GM level %d).\n",
                        acc, GM_value);
            else
                printf("Account Logged On; Account ID: %d.\n", acc);
            // TODO - can non-null sd happen?
            if (sd)
                char_log.error("non-null session data on initial connection - I thought this couldn't happen");
            if (!sd)
            {
                sd = new CharSessionData;
                session[fd]->session_data = sd;
                // put here a mail without '@' to refuse deletion if we don't receive the e-mail
                STRZCPY(sd->email, "no mail");
                sd->connect_until_time = 0;
            }
            sd->account_id = acc;
            sd->login_id1 = RFIFOL(fd, 6);
            sd->login_id2 = RFIFOL(fd, 10);
            sd->packet_tmw_version = RFIFOW(fd, 14);
            sd->sex = static_cast<enum gender>(RFIFOB(fd, 16));
            // send back account_id
            WFIFOL(fd, 0) = acc;
            WFIFOSET(fd, 4);
            // search authentication
            for (int i = 0; i < AUTH_FIFO_SIZE; i++)
            {
                if (auth_fifo[i].account_id != sd->account_id ||
                        auth_fifo[i].login_id1 != sd->login_id1 ||
                        auth_fifo[i].login_id2 != sd->login_id2 ||
                        auth_fifo[i].ip != session[fd]->client_addr ||
                        auth_fifo[i].delflag != 2)
                    continue;
                auth_fifo[i].delflag = 1;
                if (max_connect_user && count_users() >= max_connect_user)
                {
                    // refuse connection (over populated)
                    WFIFOW(fd, 0) = 0x6c;
                    WFIFOW(fd, 2) = 0;
                    WFIFOSET(fd, 3);
                    goto end_x65;
                }
                // request to login-server to obtain e-mail/time limit
                WFIFOW(login_fd, 0) = 0x2716;
                WFIFOL(login_fd, 2) = sd->account_id;
                WFIFOSET(login_fd, 6);
                // Record client version
                auth_fifo[i].packet_tmw_version = sd->packet_tmw_version;
                // send characters to player
                mmo_char_send006b(fd, sd);
                goto end_x65;
            }
            // authentication not found
            // ask login-server to authentify an account
            WFIFOW(login_fd, 0) = 0x2712;
            WFIFOL(login_fd, 2) = sd->account_id;
            WFIFOL(login_fd, 6) = sd->login_id1;
            WFIFOL(login_fd, 10) = sd->login_id2;
            WFIFOB(login_fd, 14) = sd->sex;
            WFIFOL(login_fd, 15) = session[fd]->client_addr.to_n();
            WFIFOSET(login_fd, 19);
        }
        end_x65:
            RFIFOSKIP(fd, 17);
            break;

        /// Character selection
        // uint16_t packet, uint8_t slot
        case 0x66:
            if (!sd || RFIFOREST(fd) < 3)
                return;
        {
            char ip[16];
            strcpy(ip, session[fd]->client_addr.to_string().c_str());

            int ch;
            for (ch = 0; ch < MAX_CHARS_PER_ACCOUNT; ch++)
                if (sd->found_char[ch]
                        && sd->found_char[ch]->char_num == RFIFOB(fd, 2))
                    break;
            if (ch == MAX_CHARS_PER_ACCOUNT)
                goto end_x0066;
            char_log.info("Character Selected, Account ID: %d, Character Slot: %d, Character Name: %s [%s]\n",
                          sd->account_id, RFIFOB(fd, 2), sd->found_char[ch]->name, ip);
            // Try to find the map-server of where the player last was.
            // if that fails, warp the player to the first available map
            int i = search_mapserver(sd->found_char[ch]->last_point.map);
            if (i < 0)
            {
                // get first online server (with a map)
                for (int j = 0; j < MAX_MAP_SERVERS; j++)
                    if (server_fd[j] >= 0 && server[j].map[0][0])
                    {
                        i = j;
                        sd->found_char[ch]->last_point.map = server[j].map[0];
                        printf("Map-server #%d found with a map: '%s'.\n",
                                j, &server[j].map[0]);
                        // coordinates are unknown
                        goto gotmap_x66;
                    }
                // if no map-server is connected, we send: server closed
                WFIFOW(fd, 0) = 0x81;
                WFIFOL(fd, 2) = 1; // 01 = Server closed
                WFIFOSET(fd, 3);
                RFIFOSKIP(fd, 3);
                break;
            }
        gotmap_x66:
            WFIFOW(fd, 0) = 0x71;
            WFIFOL(fd, 2) = sd->found_char[ch]->char_id;
            sd->found_char[ch]->last_point.map.write_to(sign_cast<char *>(WFIFOP(fd, 6)));
            printf("Character selection '%s' (account: %d, slot: %d) [%s]\n",
                    sd->found_char[ch]->name, sd->account_id, ch, ip);
            printf("--Send IP of map-server. ");
            if (lan_ip_check(session[fd]->client_addr))
                WFIFOL(fd, 22) = lan_map_ip.to_n();
            else
                WFIFOL(fd, 22) = server[i].ip.to_n();
            WFIFOW(fd, 26) = server[i].port;
            WFIFOSET(fd, 28);
            if (auth_fifo_pos >= AUTH_FIFO_SIZE)
                auth_fifo_pos = 0;
            auth_fifo[auth_fifo_pos].account_id = sd->account_id;
            auth_fifo[auth_fifo_pos].char_id = sd->found_char[ch]->char_id;
            auth_fifo[auth_fifo_pos].login_id1 = sd->login_id1;
            auth_fifo[auth_fifo_pos].login_id2 = sd->login_id2;
            auth_fifo[auth_fifo_pos].delflag = 0;
            auth_fifo[auth_fifo_pos].char_pos = sd->found_char[ch];
            auth_fifo[auth_fifo_pos].sex = sd->sex;
            auth_fifo[auth_fifo_pos].connect_until_time = sd->connect_until_time;
            auth_fifo[auth_fifo_pos].ip = session[fd]->client_addr;
            auth_fifo[auth_fifo_pos].packet_tmw_version = sd->packet_tmw_version;
            auth_fifo_pos++;
        }
        end_x0066:
            RFIFOSKIP(fd, 3);
            break;

        /// Create a character in a slot
        case 0x67:
            if (!sd || RFIFOREST(fd) < 37)
                return;
        {
            struct mmo_charstatus *chardat = make_new_char(fd, RFIFOP(fd, 2));
            if (!chardat)
            {
                WFIFOW(fd, 0) = 0x6e;
                WFIFOB(fd, 2) = 0x00;
                WFIFOSET(fd, 3);
                goto end_x0067;
            }

            WFIFOW(fd, 0) = 0x6d;
            memset(WFIFOP(fd, 2), 0, 106);

            // the 2 + off is by analogy to packet 0x006b
            // TODO merge this
            WFIFOL(fd, 2) = chardat->char_id;
            WFIFOL(fd, 2 + 4) = chardat->base_exp;
            WFIFOL(fd, 2 + 8) = chardat->zeny;
            WFIFOL(fd, 2 + 12) = chardat->job_exp;
            WFIFOL(fd, 2 + 16) = 0; // chardat->job_level;
            // these didn't used to be sent
            WFIFOW(fd, 2 + 20) = find_equip_view(chardat, EPOS::SHOES);
            WFIFOW(fd, 2 + 22) = find_equip_view(chardat, EPOS::GLOVES);
            WFIFOW(fd, 2 + 24) = find_equip_view(chardat, EPOS::CAPE);
            WFIFOW(fd, 2 + 26) = find_equip_view(chardat, EPOS::MISC1);

            WFIFOL(fd, 2 + 28) = chardat->option;
            WFIFOL(fd, 2 + 32) = 0;//chardat->karma;
            WFIFOL(fd, 2 + 36) = 0;//chardat->manner;
            // This used to send 0x30, which is wrong
            WFIFOW(fd, 2 + 40) = chardat->status_point;

            WFIFOW(fd, 2 + 42) = min(chardat->hp, 0x7fff);
            WFIFOW(fd, 2 + 44) = min(chardat->max_hp, 0x7fff);
            WFIFOW(fd, 2 + 46) = min(chardat->sp, 0x7fff);
            WFIFOW(fd, 2 + 48) = min(chardat->max_sp, 0x7fff);
            WFIFOW(fd, 2 + 50) = DEFAULT_WALK_SPEED;   // chardat->speed;
            WFIFOW(fd, 2 + 52) = 0; //chardat->pc_class;
            WFIFOW(fd, 2 + 54) = chardat->hair;

            WFIFOW(fd, 2 + 58) = chardat->base_level;
            WFIFOW(fd, 2 + 60) = chardat->skill_point;

            WFIFOW(fd, 2 + 64) = chardat->shield;
            WFIFOW(fd, 2 + 66) = chardat->head;
            WFIFOW(fd, 2 + 68) = chardat->chest;
            WFIFOW(fd, 2 + 70) = chardat->hair_color;

            STRZCPY2(sign_cast<char *>(WFIFOP(fd, 2 + 74)), chardat->name);

            WFIFOB(fd, 2 + 98) = min(chardat->stats[ATTR::STR], 255);
            WFIFOB(fd, 2 + 99) = min(chardat->stats[ATTR::AGI], 255);
            WFIFOB(fd, 2 + 100) = min(chardat->stats[ATTR::VIT], 255);
            WFIFOB(fd, 2 + 101) = min(chardat->stats[ATTR::INT], 255);
            WFIFOB(fd, 2 + 102) = min(chardat->stats[ATTR::DEX], 255);
            WFIFOB(fd, 2 + 103) = min(chardat->stats[ATTR::LUK], 255);
            WFIFOB(fd, 2 + 104) = chardat->char_num;

            WFIFOSET(fd, 108);
            for (int ch = 0; ch < MAX_CHARS_PER_ACCOUNT; ch++)
            {
                if (!sd->found_char[ch])
                {
                    sd->found_char[ch] = chardat;
                    break;
                }
            }
        }
        end_x0067:
            RFIFOSKIP(fd, 37);
            break;

        /// delete char
        case 0x68:
            if (!sd || RFIFOREST(fd) < 46)
                return;
        {
            char email[40];
            STRZCPY(email, sign_cast<const char *>(RFIFOP(fd, 6)));
            if (e_mail_check(email) == 0)
                STRZCPY(email, "a@a.com");

            for (int i = 0; i < MAX_CHARS_PER_ACCOUNT; i++)
            {
                struct mmo_charstatus *cs = sd->found_char[i];
                if (!cs || cs->char_id != RFIFOL(fd, 2))
                    continue;
                char_delete(cs);
                char_num--;
                if (sd->found_char[i] == &char_dat[char_num])
                    goto almost_end_x0068;
                // Need to remove references to the character
                // by putting the former last character where the deleted character was
                *sd->found_char[i] = char_dat[char_num];
                for (int j = 0; j < fd_max; j++)
                {
                    if (!session[j])
                        continue;
                    CharSessionData *sd2 = static_cast<CharSessionData*>(session[j]->session_data);
                    if (!sd2 || sd2->account_id != char_dat[char_num].account_id)
                        continue;
                    for (int k = 0; k < MAX_CHARS_PER_ACCOUNT; k++)
                    {
                        if (sd2->found_char[k] == &char_dat[char_num])
                        {
                            sd2->found_char[k] = sd->found_char[i];
                            break;
                        }
                    }
                    goto almost_end_x0068;
                }
                almost_end_x0068:
                for (int ch = i; ch < MAX_CHARS_PER_ACCOUNT - 1; ch++)
                    sd->found_char[ch] = sd->found_char[ch + 1];
                sd->found_char[MAX_CHARS_PER_ACCOUNT - 1] = NULL;
                WFIFOW(fd, 0) = 0x6f;
                WFIFOSET(fd, 2);
                goto end_x0068;
            }
            // no such character, cannot delete
            // note: the same message used to be sent for "no email" errors
            WFIFOW(fd, 0) = 0x70;
            WFIFOB(fd, 2) = 0;
            WFIFOSET(fd, 3);
        }
        end_x0068:
            RFIFOSKIP(fd, 46);
            break;

        /// map server auth
        // uint16_t packet, char userid[24], char passwd[24], char unk[4], uint32_t ip, uint16_t port
        case 0x2af8:
            if (RFIFOREST(fd) < 60)
                return;
        {
            WFIFOW(fd, 0) = 0x2af9;
            int i;
            // find first free server index
            for (i = 0; i < MAX_MAP_SERVERS; i++)
            {
                if (server_fd[i] < 0)
                    break;
            }

            if (i == MAX_MAP_SERVERS
                    || strcmp(sign_cast<const char *>(RFIFOP(fd, 2)), userid) != 0
                    || strcmp(sign_cast<const char *>(RFIFOP(fd, 26)), passwd) != 0)
            {
                WFIFOB(fd, 2) = 3;
                WFIFOSET(fd, 3);
                RFIFOSKIP(fd, 60);
                goto end_x2af8;
            }
            WFIFOB(fd, 2) = 0;
            session[fd]->func_parse = parse_frommap;
            server_fd[i] = fd;
            if (anti_freeze_enable)
                // Map anti-freeze system. Counter. 5 ok, 4...0 freezed
                server_freezeflag[i] = 5;
            server[i].ip.from_n(RFIFOL(fd, 54));
            server[i].port = RFIFOW(fd, 58);
            server[i].users = 0;
            memset(server[i].map, 0, sizeof(server[i].map));
            WFIFOSET(fd, 3);
            RFIFOSKIP(fd, 60);
            realloc_fifo(fd, FIFOSIZE_SERVERLINK, FIFOSIZE_SERVERLINK);
            // send gm acccounts level to map-servers
            int len = 4;
            WFIFOW(fd, 0) = 0x2b15;
            for (int j = 0; j < GM_num; j++)
            {
                WFIFOL(fd, len) = gm_accounts[j].account_id;
                WFIFOB(fd, len + 4) = gm_accounts[j].level;
                len += 5;
            }
            WFIFOW(fd, 2) = len;
            WFIFOSET(fd, len);
            return;
        }
        end_x2af8:
            // no RFIFOSKIP here as this doesn't always happen
            break;

        /// Still alive
        case 0x187:
            if (RFIFOREST(fd) < 6)
                return;
            RFIFOSKIP(fd, 6);
            break;

        /// Request server version
        // note that this is not currently sent by the client, but the map-server
        // uint16_t packet
        case 0x7530:
        {
            WFIFOW(fd, 0) = 0x7531;
            memcpy(WFIFOP(fd, 2), &tmwAthenaVersion, 8);
            // WFIFOB (fd, 6) = 0;
            WFIFOB(fd, 7) = ATHENA_SERVER_INTER | ATHENA_SERVER_CHAR;
            WFIFOSET(fd, 10);
        }
            RFIFOSKIP(fd, 2);
            return;

        /// Explicitly request disconnection
        case 0x7532:
            session[fd]->eof = 1;
            return;

        // TODO implement unknown packet logs
        default:
            session[fd]->eof = 1;
            return;
        }
    }
}

/// Send a packet to all map servers, possibly excluding 1
void mapif_sendallwos(int sfd, const uint8_t *buf, unsigned int len)
{
    for (int i = 0; i < MAX_MAP_SERVERS; i++)
    {
        int fd = server_fd[i];
        if (fd >= 0 && fd != sfd)
        {
            memcpy(WFIFOP(fd, 0), buf, len);
            WFIFOSET(fd, len);
        }
    }
}

/// Send data only if fd is a map server
// This is mostly a convenience method, but I think the check is also
// because sometimes FDs are saved but the server might disconnect
void mapif_send(int fd, const uint8_t *buf, unsigned int len)
{
    if (fd < 0)
        return;
    for (int i = 0; i < MAX_MAP_SERVERS; i++)
    {
        if (fd != server_fd[i])
            continue;
        memcpy(WFIFOP(fd, 0), buf, len);
        WFIFOSET(fd, len);
        return;
    }
}

/// Report number of users on maps to login server and all map servers
static void send_users_tologin(timer_id, tick_t)
{
    int users = count_users();
    uint8_t buf[16];

    if (login_fd >= 0 && session[login_fd])
    {
        // send number of user to login server
        WFIFOW(login_fd, 0) = 0x2714;
        WFIFOL(login_fd, 2) = users;
        WFIFOSET(login_fd, 6);
    }
    // send number of players to all map-servers
    WBUFW(buf, 0) = 0x2b00;
    WBUFL(buf, 2) = users;
    mapif_sendall(buf, 6);
}

static void check_connect_login_server(timer_id, tick_t)
{
    // can both conditions really happen?
    if (login_fd >= 0 && session[login_fd])
        return;
    printf("Attempt to connect to login-server...\n");
    login_fd = make_connection(login_ip, login_port);
    if (login_fd < 0)
        return;
    session[login_fd]->func_parse = parse_tologin;
    realloc_fifo(login_fd, FIFOSIZE_SERVERLINK, FIFOSIZE_SERVERLINK);

    WFIFOW(login_fd, 0) = 0x7530;
    WFIFOSET(login_fd, 2);

    WFIFOW(login_fd, 0) = 0x2710;
    STRZCPY2(sign_cast<char *>(WFIFOP(login_fd, 2)), userid);
    STRZCPY2(sign_cast<char *>(WFIFOP(login_fd, 26)), passwd);
    WFIFOL(login_fd, 50) = 0;
    WFIFOL(login_fd, 54) = char_ip.to_n();
    WFIFOW(login_fd, 58) = char_port;
    STRZCPY2(sign_cast<char *>(WFIFOP(login_fd, 60)), server_name);
    WFIFOW(login_fd, 80) = 0;
    WFIFOW(login_fd, 82) = char_maintenance;
    WFIFOW(login_fd, 84) = char_new;
    WFIFOSET(login_fd, 86);
}

/// read conf/lan_support.conf
// Note: this file is shared with the login-server
// This file is to give a different IP for connections from the LAN
// Note: it assumes that all map-servers have the same IP, just different ports
// if this isn't how it is set up, you'll have to do some port-forwarding
static void lan_config_read(const char *lancfgName)
{
    // set default configuration
    lan_map_ip.from_string("127.0.0.1");
    lan_mask.from_string("127.0.0.1/32");

    FILE *fp = fopen_(lancfgName, "r");

    if (!fp)
    {
        printf("LAN support configuration file not found: %s\n", lancfgName);
        return;
    }

    printf("---start reading of Lan Support configuration...\n");

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
        if (strcasecmp(w1, "lan_map_ip") == 0)
        {
            lan_map_ip.from_string(w2);
            printf("LAN IP of map-server: %s.\n", lan_map_ip.to_string().c_str());
        }
        else if (strcasecmp(w1, "subnet") == 0)
        {
            lan_mask.addr.from_string(w2);
            printf("Sub-network of the map-server: %s.\n",
                   lan_mask.addr.to_string().c_str());
        }
        else if (strcasecmp(w1, "subnetmask") == 0)
        {
            lan_mask.mask.from_string(w2);
            printf("Sub-network mask of the map-server: %s.\n",
                   lan_mask.mask.to_string().c_str());
        }
    }
    fclose_(fp);

    // sub-network check of the map-server
    printf("LAN test of LAN IP of the map-server: ");
    if (!lan_ip_check(lan_map_ip))
    {
        /// Actually, this could be considered a legitimate entry
        char_log.error("***ERROR: LAN IP of the map-server doesn't belong to the specified Sub-network.\n");
    }

    printf("---End reading of Lan Support configuration...\n");
}

static void char_config_read(const char *cfgName)
{
    FILE *fp = fopen_(cfgName, "r");

    if (!fp)
    {
        printf("Configuration file not found: %s.\n", cfgName);
        exit(1);
    }

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
        if (strcasecmp(w1, "userid") == 0)
        {
            STRZCPY(userid, w2);
            continue;
        }
        if (strcasecmp(w1, "passwd") == 0)
        {
            STRZCPY(passwd, w2);
            continue;
        }
        if (strcasecmp(w1, "server_name") == 0)
        {
            STRZCPY(server_name, w2);
            printf("%s server has been intialized\n", w2);
            continue;
        }
        if (strcasecmp(w1, "whisper_server_name") == 0)
        {
            // if (strlen (w2) >= 4)
            STRZCPY(whisper_server_name, w2);
            continue;
        }
        if (strcasecmp(w1, "login_ip") == 0)
        {
            login_ip.from_string(w2);
            printf("Login server IP address : %s -> %s\n", w2,
                   login_ip.to_string().c_str());
            continue;
        }
        if (strcasecmp(w1, "login_port") == 0)
        {
            login_port = atoi(w2);
            continue;
        }
        if (strcasecmp(w1, "char_ip") == 0)
        {
            char_ip.from_string(w2);
            printf("Character server IP address : %s -> %s\n",
                   w2, char_ip.to_string().c_str());
            continue;
        }
        if (strcasecmp(w1, "char_port") == 0)
        {
            char_port = atoi(w2);
            continue;
        }
        if (strcasecmp(w1, "char_maintenance") == 0)
        {
            char_maintenance = atoi(w2);
            continue;
        }
        if (strcasecmp(w1, "char_new") == 0)
        {
            char_new = atoi(w2);
            continue;
        }
        if (strcasecmp(w1, "char_txt") == 0)
        {
            strcpy(char_txt, w2);
            continue;
        }
        if (strcasecmp(w1, "max_connect_user") == 0)
        {
            max_connect_user = atoi(w2);
            if (max_connect_user < 0)
                max_connect_user = 0;
            continue;
        }
        if (strcasecmp(w1, "autosave_time") == 0)
        {
            autosave_interval = atoi(w2) * 1000;
            if (autosave_interval <= 0)
                autosave_interval = DEFAULT_AUTOSAVE_INTERVAL;
            continue;
        }
        if (strcasecmp(w1, "start_point") == 0)
        {
            fixed_string<16> map;
            int x, y;
            if (sscanf(w2, "%15[^,],%d,%d", &map, &x, &y) == 3 && map.contains(".gat"))
            {
                // Verify at least if '.gat' is in the map name
                start_point.map = map;
                start_point.x = x;
                start_point.y = y;
                continue;
            }
        }
        if (strcasecmp(w1, "unknown_char_name") == 0)
        {
            STRZCPY(unknown_char_name, w2);
            continue;
        }
        if (strcasecmp(w1, "name_ignoring_case") == 0)
        {
            name_ignoring_case = config_switch(w2);
            continue;
        }
        if (strcasecmp(w1, "char_name_option") == 0)
        {
            switch (atoi(w2))
            {
            case 0: char_name_option = ALL; break;
            case 1: char_name_option = ONLY; break;
            case 2: char_name_option = EXCLUDE; break;
            default: ;// TODO log something
            }
            continue;
        }
        if (strcasecmp(w1, "char_name_letters") == 0)
        {
            strcpy(char_name_letters, w2);
            continue;
        }

        // online files options
        if (strcasecmp(w1, "online_txt_filename") == 0)
        {
            strcpy(online_txt_filename, w2);
            continue;
        }
        if (strcasecmp(w1, "online_html_filename") == 0)
        {
            strcpy(online_html_filename, w2);
            continue;
        }
        if (strcasecmp(w1, "online_sorting_option") == 0)
        {
            online_sorting_option = atoi(w2);
            continue;
        }
        if (strcasecmp(w1, "online_display_option") == 0)
        {
            online_display_option = atoi(w2);
            continue;
        }
        if (strcasecmp(w1, "online_gm_display_min_level") == 0)
        {
            // minimum GM level to display 'GM' in the online files
            online_gm_display_min_level = atoi(w2);
            if (online_gm_display_min_level < 1)
                online_gm_display_min_level = 1;
            continue;
        }
        if (strcasecmp(w1, "online_refresh_html") == 0)
        {
            online_refresh_html = atoi(w2);
            if (online_refresh_html < 5)
                online_refresh_html = 5;
            continue;
        }

        if (strcasecmp(w1, "anti_freeze_enable") == 0)
        {
            anti_freeze_enable = config_switch(w2);
            continue;
        }
        if (strcasecmp(w1, "anti_freeze_interval") == 0)
        {
            ANTI_FREEZE_INTERVAL = atoi(w2);
            if (ANTI_FREEZE_INTERVAL < 5)
                ANTI_FREEZE_INTERVAL = 5;
            continue;
        }
        if (strcasecmp(w1, "import") == 0)
        {
            char_config_read(w2);
            continue;
        }
        printf("%s: unknown option: %s\n", __func__, line);
    }
    fclose_(fp);
}

void term_func(void)
{
    mmo_char_sync();
    inter_save();

    // this is a bit of a hack
    // write out online files saying there is nobody
    // TODO - should this be replaced with saying "server offline?"
    char_num = 0;
    create_online_files();
}

void do_init(int argc, char **argv)
{
    char_log.add(char_log_filename, true, Level::INFO);
    unknown_packet_log.add(char_log_unknown_packets_filename, false, Level::DEBUG);

    char_log.debug("\nThe char-server starting...\n");

    // FIXME: specifying config by position is deprecated
    char_config_read((argc > 1) ? argv[1] : CHAR_CONF_NAME);
    lan_config_read((argc > 3) ? argv[3] : LOGIN_LAN_CONF_NAME);

    for (int i = 0; i < MAX_MAP_SERVERS; i++)
    {
        memset(&server[i], 0, sizeof(struct mmo_map_server));
        server_fd[i] = -1;
    }

    mmo_char_init();

    update_online = time(NULL);
    create_online_files();

    inter_init((argc > 2) ? argv[2] : inter_cfgName);

    set_defaultparse(parse_char);

    char_fd = make_listen_port(char_port);

    add_timer_interval(gettick() + 1000, 10 * 1000, check_connect_login_server);
    add_timer_interval(gettick() + 1000, 5 * 1000, send_users_tologin);
    add_timer_interval(gettick() + autosave_interval, autosave_interval, mmo_char_sync_timer);

    if (anti_freeze_enable)
        add_timer_interval(gettick() + 1000, ANTI_FREEZE_INTERVAL * 1000, map_anti_freeze_system);

    char_log.info("The char-server is ready (Server is listening on the port %d).\n",
                  char_port);
}
