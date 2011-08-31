#include "atcommand.hpp"

#include <cmath>

#include "../common/core.hpp" // runflag
#include "../common/mt_rand.hpp"
#include "../common/nullpo.hpp"
#include "../common/timer.hpp"
#include "../common/utils.hpp"

#include "battle.hpp"
#include "chrif.hpp"
#include "clif.hpp"
#include "itemdb.hpp"
#include "map.hpp"
#include "mob.hpp"
#include "npc.hpp"
#include "party.hpp"
#include "pc.hpp"
#include "skill.hpp"
#include "storage.hpp"
#include "tmw.hpp"
#include "trade.hpp"

#define ATCOMMAND_FUNC(x) static int atcommand_##x(int fd, MapSessionData* sd, const char *command, const char *message)
ATCOMMAND_FUNC(setup);
ATCOMMAND_FUNC(broadcast);
ATCOMMAND_FUNC(localbroadcast);
ATCOMMAND_FUNC(charwarp);
ATCOMMAND_FUNC(warp);
ATCOMMAND_FUNC(where);
ATCOMMAND_FUNC(goto);
ATCOMMAND_FUNC(jump);
ATCOMMAND_FUNC(who);
ATCOMMAND_FUNC(whogroup);
ATCOMMAND_FUNC(whomap);
ATCOMMAND_FUNC(whomapgroup);
ATCOMMAND_FUNC(whogm);         // by Yor
ATCOMMAND_FUNC(save);
ATCOMMAND_FUNC(load);
ATCOMMAND_FUNC(speed);
ATCOMMAND_FUNC(storage);
ATCOMMAND_FUNC(option);
ATCOMMAND_FUNC(hide);
ATCOMMAND_FUNC(die);
ATCOMMAND_FUNC(kill);
ATCOMMAND_FUNC(alive);
ATCOMMAND_FUNC(kami);
ATCOMMAND_FUNC(heal);
ATCOMMAND_FUNC(item);
ATCOMMAND_FUNC(itemreset);
ATCOMMAND_FUNC(itemcheck);
ATCOMMAND_FUNC(baselevelup);
ATCOMMAND_FUNC(joblevelup);
ATCOMMAND_FUNC(help);
ATCOMMAND_FUNC(gm);
ATCOMMAND_FUNC(pvpoff);
ATCOMMAND_FUNC(pvpon);
ATCOMMAND_FUNC(model);
ATCOMMAND_FUNC(spawn);
ATCOMMAND_FUNC(killmonster);
ATCOMMAND_FUNC(killmonster2);
ATCOMMAND_FUNC(memo);
ATCOMMAND_FUNC(gat);
ATCOMMAND_FUNC(statuspoint);
ATCOMMAND_FUNC(skillpoint);
ATCOMMAND_FUNC(zeny);
template<ATTR attr>
ATCOMMAND_FUNC(param);
ATCOMMAND_FUNC(recall);
ATCOMMAND_FUNC(recallall);
ATCOMMAND_FUNC(revive);
ATCOMMAND_FUNC(character_stats);
ATCOMMAND_FUNC(character_stats_all);
ATCOMMAND_FUNC(character_option);
ATCOMMAND_FUNC(character_save);
ATCOMMAND_FUNC(doom);
ATCOMMAND_FUNC(doommap);
ATCOMMAND_FUNC(raise);
ATCOMMAND_FUNC(raisemap);
ATCOMMAND_FUNC(character_baselevel);
ATCOMMAND_FUNC(character_joblevel);
ATCOMMAND_FUNC(kick);
ATCOMMAND_FUNC(kickall);
ATCOMMAND_FUNC(party);
ATCOMMAND_FUNC(charskreset);
ATCOMMAND_FUNC(charstreset);
ATCOMMAND_FUNC(charreset);
ATCOMMAND_FUNC(charstpoint);
ATCOMMAND_FUNC(charmodel);
ATCOMMAND_FUNC(charskpoint);
ATCOMMAND_FUNC(charzeny);
ATCOMMAND_FUNC(mapexit);
ATCOMMAND_FUNC(idsearch);
ATCOMMAND_FUNC(mapinfo);
ATCOMMAND_FUNC(hair_style);
ATCOMMAND_FUNC(hair_color);
ATCOMMAND_FUNC(all_stats);
ATCOMMAND_FUNC(char_change_sex);
ATCOMMAND_FUNC(char_block);
ATCOMMAND_FUNC(char_ban);
ATCOMMAND_FUNC(char_unblock);
ATCOMMAND_FUNC(char_unban);
ATCOMMAND_FUNC(partyrecall);
ATCOMMAND_FUNC(enablenpc);
ATCOMMAND_FUNC(disablenpc);
ATCOMMAND_FUNC(servertime);
ATCOMMAND_FUNC(chardelitem);
ATCOMMAND_FUNC(email);
ATCOMMAND_FUNC(effect);
ATCOMMAND_FUNC(character_item_list);
ATCOMMAND_FUNC(character_storage_list);
ATCOMMAND_FUNC(addwarp);
ATCOMMAND_FUNC(killer);
ATCOMMAND_FUNC(npcmove);
ATCOMMAND_FUNC(killable);
ATCOMMAND_FUNC(charkillable);
ATCOMMAND_FUNC(chareffect);
ATCOMMAND_FUNC(storeall);
ATCOMMAND_FUNC(skillid);
ATCOMMAND_FUNC(summon);
ATCOMMAND_FUNC(adjgmlvl);
ATCOMMAND_FUNC(adjcmdlvl);
ATCOMMAND_FUNC(trade);
ATCOMMAND_FUNC(char_wipe);
ATCOMMAND_FUNC(set_magic);
ATCOMMAND_FUNC(magic_info);
ATCOMMAND_FUNC(log);
ATCOMMAND_FUNC(tee);
ATCOMMAND_FUNC(invisible);
ATCOMMAND_FUNC(visible);
ATCOMMAND_FUNC(list_nearby);
ATCOMMAND_FUNC(iterate_forward_over_players);
ATCOMMAND_FUNC(iterate_backwards_over_players);
ATCOMMAND_FUNC(skillpool_info);
ATCOMMAND_FUNC(skillpool_focus);
ATCOMMAND_FUNC(skillpool_unfocus);
ATCOMMAND_FUNC(skill_learn);
ATCOMMAND_FUNC(wgm);
ATCOMMAND_FUNC(ipcheck);

typedef AtCommandCategory ATCC;
/// atcommand dispatch table
// sorted by category, then level
// levels can be overridden in atcommand_athena.conf
static AtCommandInfo atcommand_info[] = {
    {"@help", 0,        atcommand_help,         ATCC::MISC,
    "[@cmd | cat]",     "Display help about @commands."},
    {"@wgm", 0,         atcommand_wgm,          ATCC::MSG,
    "message",          "Send a message to all online GMs."},
    {"@kami", 40,       atcommand_kami,         ATCC::MSG,
    "message",          "Make a global announcement without displaying your name."},
    {"@broadcast", 40,  atcommand_broadcast,    ATCC::MSG,
    "message",          "Make a global announcement across all servers."},
    {"@localbroadcast", 40, atcommand_localbroadcast, ATCC::MSG,
    "message",          "Make a global announcement on the current server."},
    {"@die", 1,         atcommand_die,          ATCC::SELF,
    "",                 "Suicide."},
    {"@goto", 20,       atcommand_goto,         ATCC::SELF,
    "charname",         "Warp yourself to a player."},
    {"@model", 20,      atcommand_model,        ATCC::SELF,
    "hairstyle haircolor",
                        "Change your appearance."},
    {"@warp", 40,       atcommand_warp,         ATCC::SELF,
    "map x y",          "Warp yourself to a location on any map (random x,y if not specified)."},
    {"@jump", 40,       atcommand_jump,         ATCC::SELF,
    "[x [y]]",          "Warp to a location on the current map (random x,y if not specified)."},
    {"@hide", 40,       atcommand_hide,         ATCC::SELF,
    "",                 "Toggle detectability to mobs and scripts."},
    {"@heal", 40,       atcommand_heal,         ATCC::SELF,
    "[hp [sp]]",        "Restore your HP/SP, fully or by a specified amount."},
    {"@save", 40,       atcommand_save,         ATCC::SELF,
    "",                 "Set your respawn point to your current location."},
    {"@return", 40,     atcommand_load,         ATCC::SELF,
    "",                 "Warp yourself to your respawn point"},
    {"@load", 40,       atcommand_load,         ATCC::SELF,
    "",                 "Warp yourself to your respawn point"},
    {"@killable", 40,   atcommand_killable,     ATCC::SELF,
    "",                 "Make yourself killable by other players."},
    {"@storeall", 40,   atcommand_storeall,     ATCC::SELF,
    "",                 "Put the contents of your inventory into storage."},
    {"@speed", 40,      atcommand_speed,        ATCC::SELF,
    "[1-1000]",         "Set your walk speed delay in milliseconds. Default is 150."},
    {"@memo", 40,       atcommand_memo,         ATCC::SELF,
    "[pos]",            "Set a memo point (list points if no location specified)."},
    {"@hairstyle", 40,  atcommand_hair_style,   ATCC::SELF,
    "",                 "Change your hairstyle."},
    {"@haircolor", 40,  atcommand_hair_color,   ATCC::SELF,
    "",                 "Change your hair color."},
    {"@effect", 40,     atcommand_effect,       ATCC::SELF,
    "ID [flag]",        "Apply an effect to yourself."},
    {"@sp-info", 40,    atcommand_skillpool_info, ATCC::SELF,
    "charname",         "display magic skills"},
    {"@option", 40,     atcommand_option,       ATCC::SELF,
    "param1 p2 p3",
    "    <param1>      <param2>      <p3>(stackable)   <param3>               <param3>\n"
    "    1 Petrified   (stackable)   01 Sight           32 Peco Peco riding   2048 Orc Head\n"
    "    2 Frozen      01 Poison     02 Hide            64 GM Perfect Hide    4096 Wedding Sprites\n"
    "    3 Stunned     02 Cursed     04 Cloak          128 Level 2 Cart       8192 Ruwach\n"
    "    4 Sleeping    04 Silenced   08 Level 1 Cart   256 Level 3 Cart\n"
    "    6 darkness    08 ???        16 Falcon         512 Level 4 Cart\n"
    "                  16 darkness                    1024 Level 5 Cart"},
    {"@alive", 60,      atcommand_alive,        ATCC::SELF,
    "",                 "Resurrect yourself."},
    {"@blvl", 60,       atcommand_baselevelup,  ATCC::SELF,
    "count",            "Raise your base level."},
    {"@jlvl", 60,       atcommand_joblevelup,   ATCC::SELF,
    "count",            "Raise your job level (slightly broken)."},
    {"@allstats", 60,   atcommand_all_stats,    ATCC::SELF,
    "[num]",            "Increase all stats (to maximum if no amount specified)."},
    {"@stpoint", 60,    atcommand_statuspoint,  ATCC::SELF,
    "count",            "Give yourself status points"},
    {"@skpoint", 60,    atcommand_skillpoint,   ATCC::SELF,
    "count",            "Give yourself skill points"},
    {"@zeny", 60,       atcommand_zeny,         ATCC::SELF,
    "count",            "Give yourself some gold"},
    {"@str", 60,        atcommand_param<ATTR::STR>, ATCC::SELF,
    "count",            "Increase your strength"},
    {"@agi", 60,        atcommand_param<ATTR::AGI>, ATCC::SELF,
    "count",            "Increase your agility"},
    {"@vit", 60,        atcommand_param<ATTR::VIT>, ATCC::SELF,
    "count",            "Increase your vitality"},
    {"@int", 60,        atcommand_param<ATTR::INT>, ATCC::SELF,
    "count",            "Increase your intelligence"},
    {"@dex", 60,        atcommand_param<ATTR::DEX>, ATCC::SELF,
    "count",            "Increase your dexterity"},
    {"@luk", 60,        atcommand_param<ATTR::LUK>, ATCC::SELF,
    "count",            "Increase your luck"},
    {"@killer", 60,     atcommand_killer,       ATCC::SELF,
    "",                 "Let yourself kill other players."},
    {"@invisible", 60,  atcommand_invisible,    ATCC::SELF,
    "",                 "Make yourself invisible to players."},
    {"@visible", 60,    atcommand_visible,      ATCC::SELF,
    "",                 "Make yourself visible to players."},
    {"@hugo", 60,       atcommand_iterate_forward_over_players, ATCC::SELF,
    "",                 "Warp yourself to the next player in the online list."},
    {"@linus", 60,      atcommand_iterate_backwards_over_players, ATCC::SELF,
    "",                 "Warp yourself to the previous player in the online list."},
    {"@sp-focus", 80,   atcommand_skillpool_focus, ATCC::SELF,
    "num [charname]",   "focus a skill"},
    {"@sp-unfocus", 80, atcommand_skillpool_unfocus, ATCC::SELF,
    "num [charname]",   "unfocus a skill"},
    {"@skill-learn", 80, atcommand_skill_learn, ATCC::SELF,
    "num [level [charname]]",
                        "learn a skill"},
    {"@kick", 20,       atcommand_kick,         ATCC::CHAR,
    "charname",         "Disconnect a player from the server."},
    {"@charitemlist", 40, atcommand_character_item_list, ATCC::CHAR,
    "charname",         "List the contents of a player's inventory."},
    {"@charstoragelist", 40, atcommand_character_storage_list, ATCC::CHAR,
    "charname",         "List the contents of a player's storage."},
    {"@charkillable", 40, atcommand_charkillable, ATCC::CHAR,
    "charname",         "Make a player killable by others."},
    {"@chareffect", 40, atcommand_chareffect,   ATCC::CHAR,
    "",                 "??"},
    {"@charstats", 40,  atcommand_character_stats, ATCC::CHAR,
    "charname",         "display stats of a character"},
    {"@charstatsall", 40, atcommand_character_stats_all, ATCC::CHAR,
    "",                 "display stats of all characters"},
    {"@charmodel", 50,  atcommand_charmodel,    ATCC::CHAR,
    "hairstyle haircolor charname",
                        "Change a player's appearance."},
    {"@charwarp", 60,   atcommand_charwarp,     ATCC::CHAR,
    "map x y charname", "Warp a player to a location on any map (random x,y if unspecified)."},
    {"@kill", 60,       atcommand_kill,         ATCC::CHAR,
    "charname",         "Kill a player."},
    {"@charbaselvl", 60, atcommand_character_baselevel, ATCC::CHAR,
    "num charname",     "Raise a player's base level."},
    {"@charjlvl", 60,   atcommand_character_joblevel, ATCC::CHAR,
    "num charname",     "Raise a player's job level (slightly broken)."},
    {"@charskreset", 60, atcommand_charskreset, ATCC::CHAR,
    "charname",         "Reset a player's skills."},
    {"@charstreset", 60, atcommand_charstreset, ATCC::CHAR,
    "charname",         "Reset a player's stats."},
    {"@charreset", 60,  atcommand_charreset,    ATCC::CHAR,
    "charname",         "Reset a player's stats and skills."},
    {"@charskpoint", 60, atcommand_charskpoint, ATCC::CHAR,
    "num charname",     "Give an amount of skill points to a player."},
    {"@charstpoint", 60, atcommand_charstpoint, ATCC::CHAR,
    "num charname",     "Give an amount of stat points to a player."},
    {"@charzeny", 60,   atcommand_charzeny,     ATCC::CHAR,
    "amount charname",  "Give an amount of money to a player."},
    {"@charchangesex", 60, atcommand_char_change_sex, ATCC::CHAR,
    "name",             "Change a player's gender."},
    {"@block", 60,      atcommand_char_block,   ATCC::CHAR,
    "charname",         "Permanently block a player's account."},
    {"@unblock", 60,    atcommand_char_unblock, ATCC::CHAR,
    "charname",         "Unblock a player's account."},
    {"@ban", 60,        atcommand_char_ban,     ATCC::CHAR,
    "time charname",    "Ban a player's account for a specified time (+-#y/m/d/h/mn/s)."},
    {"@unban", 60,      atcommand_char_unban,   ATCC::CHAR,
    "charname",         "Unban a player's account."},
    {"@chardelitem", 60, atcommand_chardelitem, ATCC::CHAR,
    "name|ID qty charname",
                        "Remove an amount of a specified item from a player's inventory."},
    {"@trade", 60,      atcommand_trade,        ATCC::CHAR,
    "charname",         "Open trade window with a player."},
    {"@charwipe", 60,   atcommand_char_wipe,    ATCC::CHAR,
    "",                 "??"},
    {"@charoption", 60, atcommand_character_option, ATCC::CHAR,
    "param1 param2 param3 charname",
                        "set display options of a character"},
    {"@revive", 60,     atcommand_revive,       ATCC::CHAR,
    "charname",         "resurrect someone else"},
    {"@recall", 60,     atcommand_recall,       ATCC::CHAR,
    "charname",         "warp a player to you"},
    {"@charsave", 60,   atcommand_character_save, ATCC::CHAR,
    "map x y charname", "changes somebody's respawn point"},
    {"@doom", 80,       atcommand_doom,         ATCC::CHAR,
    "",                 "Kill all online non-GM players."},
    {"@doommap", 80,    atcommand_doommap,      ATCC::CHAR,
    "",                 "Kill all non-GM players on the map."},
    {"@raise", 80,      atcommand_raise,        ATCC::CHAR,
    "",                 "Resurrect all online players."},
    {"@raisemap", 80,   atcommand_raisemap,     ATCC::CHAR,
    "",                 "Resurrect all players on the map."},
    {"@recallall", 80,  atcommand_recallall,    ATCC::CHAR,
    "",                 "Warp all online players to you."},
    {"@kickall", 99,    atcommand_kickall,      ATCC::CHAR,
    "",                 "Disconnect all online players."},
    {"@servertime", 0,  atcommand_servertime,   ATCC::INFO,
    "",                 "Display the time of the server (usually UTC)."},
    {"@where", 1,       atcommand_where,        ATCC::INFO,
    "[charname]",       "Display the location of a player."},
    {"@who", 20,        atcommand_who,          ATCC::INFO,
    "[substring]",      "List all online players and their locations."},
    {"@whogroup", 20,   atcommand_whogroup,     ATCC::INFO,
    "[substring]",      "List all online players and their parties."},
    {"@whomap", 20,     atcommand_whomap,       ATCC::INFO,
    "[map]",            "List all players on a map and their locations."},
    {"@whomapgroup", 20, atcommand_whomapgroup, ATCC::INFO,
    "[map]",            "List all players on a map and their parties."},
    {"@whogm", 20,      atcommand_whogm,        ATCC::INFO,
    "[substring]",      "List online GMs"},
    {"@skillid", 40,    atcommand_skillid,      ATCC::INFO,
    "name",             "Display the ID of a skill."},
    {"@mapinfo", 99,    atcommand_mapinfo,      ATCC::INFO,
    "[type [map]]",     "information about a map. type 1 add players, type 2 add NPCs, type 3 add shops/chat"},
    {"@storage", 1,     atcommand_storage,      ATCC::ITEM,
    "",                 "Open your storage."},
    {"@itemreset", 40,  atcommand_itemreset,    ATCC::ITEM,
    "",                 "Delete the contents of your inventory."},
    {"@item", 60,       atcommand_item,         ATCC::ITEM,
    "name|ID qty",      "Add an item to your inventory."},
    {"@idsearch", 60,   atcommand_idsearch,     ATCC::ITEM,
    "name",             "List items by substring."},
    {"@itemcheck", 60,  atcommand_itemcheck,    ATCC::ITEM,
    "",                 "check authorization of your inventory"},
    {"@npcmove", 20,    atcommand_npcmove,      ATCC::ADMIN,
    "",                 "??"},
    {"@ipcheck", 60,    atcommand_ipcheck,      ATCC::ADMIN,
    "charname",         "List players with the same IP addresses."},
    {"@enablenpc", 80,  atcommand_enablenpc,    ATCC::ADMIN,
    "npcname",          "Enable an NPC."},
    {"@disablenpc", 80, atcommand_disablenpc,   ATCC::ADMIN,
    "npcname",          "Disable an NPC."},
    {"@gat", 99,        atcommand_gat,          ATCC::ADMIN,
    "",                 "Display the map's collision information."},
    {"@mapexit", 99,    atcommand_mapexit,      ATCC::ADMIN,
    "",                 "Shut the map server down."},
    {"@adjgmlvl", 99,   atcommand_adjgmlvl,     ATCC::ADMIN,
    "",                 "Temporarily adjust the GM level of a player."},
    {"@adjcmdlvl", 99,  atcommand_adjcmdlvl,    ATCC::ADMIN,
    "",                 "Temporarily adjust the required @level of a @command."},
    {"@gm", 100,        atcommand_gm,           ATCC::ADMIN,
    "password",         "Make yourself a GM."},
    {"@party", 1,       atcommand_party,        ATCC::GROUP,
    "name",             "Create a party."},
    {"@pvpoff", 40,     atcommand_pvpoff,       ATCC::GROUP,
    "",                 "Disable PvP on current map."},
    {"@pvpon", 40,      atcommand_pvpon,        ATCC::GROUP,
    "",                 "Enable PvP on current map."},
    {"@partyrecall", 60, atcommand_partyrecall, ATCC::GROUP,
    "partyname",        "Warp all members of a party to you."},
    {"@killmonster2", 40, atcommand_killmonster2, ATCC::MOB,
    "",                 "Kill all monsters on the map, without them dropping items."},
    {"@spawn", 50,      atcommand_spawn,        ATCC::MOB,
    "name|ID [count [x [y]]]",
                        "Spawn an amount of specified mobs around you."},
    {"@killmonster", 60, atcommand_killmonster, ATCC::MOB,
    "[map]",            "Kill all monsters on the map, dropping items."},
    {"@summon", 60,     atcommand_summon,       ATCC::MOB,
    "name|ID [num [desired name [x [y]]]]",
                        "Summon an amount of specified mobs around you."},
    {"@addwarp", 20,    atcommand_addwarp,      ATCC::ENV,
    "map x y",          "Add a semipermanent warp to a location from your current position."},
    {"@email", 0,       atcommand_email,        ATCC::MISC,
    "old@e.mail new@e.mail",
                        "change stored email"},
    {"@magicinfo", 60,  atcommand_magic_info,   ATCC::MISC,
    "",                 "??"},
    {"@log", 60,        atcommand_log,          ATCC::MISC,
    "message",          "Record a message to the GM log."},
    {"@l", 60,          atcommand_log,          ATCC::MISC,
    "message",          "Record a message to the GM log."},
    {"@tee", 60,        atcommand_tee,          ATCC::MISC,
    "",                 "Say a message aloud and record it to the GM log."},
    {"@t", 60,          atcommand_tee,          ATCC::MISC,
    "",                 "Say a message aloud and record it to the GM log."},
    {"@setmagic", 99,   atcommand_set_magic,    ATCC::MISC,
    "school|all [value [charname]]",
                        "set magic skill levels"},
    {"@setup", 40,      atcommand_setup,        ATCC::UNK,
    "",                 "??"},
    {"@listnearby", 40, atcommand_list_nearby,  ATCC::UNK,
    "",                 "??"},
};

/// Log an atcommand
static void log_atcommand(MapSessionData *sd, const char *cmd, const char *arg)
{
    gm_log("%s(%d,%d) %s(%d) : %s %s", &maps[sd->m].name, sd->x,
                sd->y, sd->status.name, sd->status.account_id, cmd, arg);
}

char *gm_logfile_name = NULL;

/// Write to gm logfile with timestamp
// the log is automatically rotated monthly
void gm_log(const char *fmt, ...)
{
    static int last_logfile_nr = 0;
    static FILE *gm_logfile = NULL;

    if (!gm_logfile_name)
        return;

    time_t time_v = time(NULL);
    struct tm *time_bits = gmtime(&time_v);

    int year = time_bits->tm_year + 1900;
    int month = time_bits->tm_mon + 1;
    int logfile_nr = (year * 12) + month;

    if (logfile_nr != last_logfile_nr)
    {
        char *fullname;
        CREATE(fullname, char, strlen(gm_logfile_name) + 10);
        sprintf(fullname, "%s.%04d-%02d", gm_logfile_name, year, month);

        if (gm_logfile)
            fclose_(gm_logfile);

        gm_logfile = fopen_(fullname, "a");
        free(fullname);

        if (!gm_logfile)
        {
            perror("GM log file");
            gm_logfile_name = NULL;
            return;
        }
        last_logfile_nr = logfile_nr;
    }

    fputs(stamp_time(time_v, NULL), gm_logfile);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(gm_logfile, fmt, ap);
    va_end(ap);

    fputc('\n', gm_logfile);
}

static void atcommand_help_long(int fd, const AtCommandInfo& info);
static AtCommandInfo *atcommand(gm_level_t level, const char *message);

bool is_atcommand(int fd, MapSessionData *sd, const char *message,
                   gm_level_t gmlvl)
{
    nullpo_ret(sd);

    if (!message || !*message)
        return 0;

    AtCommandInfo *info = atcommand(gmlvl ? gmlvl : pc_isGM(sd), message);
    if (!info)
        return false;

    char command[100];
    const char *str = message;
    const char *p = message;
    memset(command, '\0', sizeof(command));
    while (*p && !isspace(*p))
        p++;
    if (p - str >= sizeof(command))    // too long
        return 0;
    strncpy(command, str, p - str);
    while (isspace(*p))
        p++;

    if (info->proc(fd, sd, command, p) != 0)
    {
        atcommand_help_long(fd, *info);
        return true;
    }
    // Don't log level 0 commands
    if (info->level)
        log_atcommand(sd, command, p);

    return true;
}

/// get info about command
AtCommandInfo *atcommand(gm_level_t level, const char *message)
{
    if (!message || !*message)
    {
        fprintf(stderr, "at command message is empty\n");
        return NULL;
    }

    if (*message != '@')
        return NULL;

    char command[101];

    sscanf(message, "%100s", command);
    command[sizeof(command) - 1] = '\0';

    for (int i = 0; i < ARRAY_SIZEOF(atcommand_info); i++)
    {
        if (strcasecmp(command, atcommand_info[i].command) == 0
            && level >= atcommand_info[i].level)
            return &atcommand_info[i];
    }

    return NULL;
}

/// Kill an individual monster (with or without loot)
static void atkillmonster_sub(BlockList *bl, bool flag)
{
    nullpo_retv(bl);
    struct mob_data *md = static_cast<struct mob_data *>(bl);
    if (flag)
        mob_damage(NULL, md, md->hp, 2);
    else
        mob_delete(md);
}

/// Find the @command by name
static AtCommandInfo *get_atcommandinfo_byname(const char *name)
{
    for (int i = 0; i < ARRAY_SIZEOF(atcommand_info); i++)
        if (strcasecmp(atcommand_info[i].command + 1, name) == 0)
            return &atcommand_info[i];

    return NULL;
}

/// read conf/atcommand_athena.conf, customizing the levels of the GM commands
void atcommand_config_read(const char *cfgName)
{
    FILE *fp = fopen_(cfgName, "r");
    if (!fp)
    {
        map_log("At commands configuration file not found: %s\n", cfgName);
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line) - 1, fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;

        char w1[1024], w2[1024];

        if (sscanf(line, "%1023[^:]:%1023s", w1, w2) != 2)
            continue;
        AtCommandInfo *p = get_atcommandinfo_byname(w1);
        if (p)
        {
            p->level = atoi(w2);
            if (p->level > 100)
                p->level = 100;
        }

        if (strcasecmp(w1, "import") == 0)
            atcommand_config_read(w2);
    }
    fclose_(fp);
}

// The rest of the file is the actual implementations of @commands

/// @setup - Safely set a chars levels and warp them to a special place
// from TAW, unused by TMW
int atcommand_setup(int fd, MapSessionData *sd,
                     const char *, const char *message)
{
    char character[100];
    int level = 1;

    if (!message || !*message
        || sscanf(message, "%d %99[^\n]", &level, character) < 2)
        return -1;
    level--;

    char buf[256];
    snprintf(buf, sizeof(buf), "-255 %s", character);
    atcommand_character_baselevel(fd, sd, "@charbaselvl", buf);

    snprintf(buf, sizeof(buf), "%d %s", level, character);
    atcommand_character_baselevel(fd, sd, "@charbaselvl", buf);

    // Emote skill
    snprintf(buf, sizeof(buf), "1 1 %s", character);
    atcommand_skill_learn(fd, sd, "@skill-learn", buf);

    // Trade skill
    snprintf(buf, sizeof(buf), "2 1 %s", character);
    atcommand_skill_learn(fd, sd, "@skill-learn", buf);

    // Party skill
    snprintf(buf, sizeof(buf), "2 2 %s", character);
    atcommand_skill_learn(fd, sd, "@skill-learn", buf);

    snprintf(buf, sizeof(buf), "018-1.gat 24 98 %s", character);
    atcommand_charwarp(fd, sd, "@charwarp", buf);

    return 0;

}

/// Warp player to another map
int atcommand_charwarp(int fd, MapSessionData *sd,
                        const char *, const char *message)
{
    char character[100];
    short x, y;

    fixed_string<16> map_name;
    if (!message || !*message
        || sscanf(message, "%15s %hd %hd %99[^\n]", &map_name, &x, &y,
                   character) < 4)
        return -1;

    if (!map_name.contains(".gat") && map_name.length() < 13)   // 16 - 4 (.gat)
        strcat(&map_name, ".gat");

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;
    }

    // FIXME: This really should use actual map bounds, but we only have that data for local maps
    if (x <= 0)
        x = MRAND(399) + 1;
    if (y <= 0)
        y = MRAND(399) + 1;

    int m = map_mapname2mapid(map_name);
    if (m >= 0 && maps[m].flag.nowarpto
        && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp someone to this map.");
        return -1;
    }
    if (maps[pl_sd->m].flag.nowarp
        && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp this player from its current map.");
        return -1;
    }
    if (pc_setpos(pl_sd, Point{map_name, x, y}, BeingRemoveType::WARP) == 0)
    {
        clif_displaymessage(pl_sd->fd, "Warped.");
        clif_displaymessage(fd, "Player warped");
    }
    else
    {
        clif_displaymessage(fd, "Map not found.");
        return -1;
    }

    return 0;
}

/// Warp yourself to another map
int atcommand_warp(int fd, MapSessionData *sd,
                    const char *, const char *message)
{
    fixed_string<16> map_name;
    short x = 0, y = 0;

    if (!message || !*message
        || sscanf(message, "%15s %hd %hd", &map_name, &x, &y) < 1)
        return -1;

    if (x <= 0)
        x = MRAND(399) + 1;
    if (y <= 0)
        y = MRAND(399) + 1;

    if (!map_name.contains(".gat") && map_name.length() < 13)   // 16 - 4 (.gat)
        strcat(&map_name, ".gat");

    int m = map_mapname2mapid(map_name);
    if (m >= 0 && maps[m].flag.nowarpto
        && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp you to this map.");
        return -1;
    }
    if (maps[sd->m].flag.nowarp
        && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp you from your actual map.");
        return -1;
    }

    if (pc_setpos(sd, Point{map_name, x, y}, BeingRemoveType::WARP) == 0)
        clif_displaymessage(fd, "Warped.");
    else
    {
        clif_displaymessage(fd, "Map not found.");
        return -1;
    }

    return 0;
}

/// Find location of a character (or yourself)
int atcommand_where(int fd, MapSessionData *sd,
                     const char *, const char *message)
{
    char character[100];
    char output[200];

    if (sscanf(message, "%99[^\n]", character) < 1)
        strcpy(character, sd->status.name);

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd || (
            (battle_config.hide_GM_session || (pl_sd->status.option & OPTION_HIDE))
            && pc_isGM(pl_sd) > pc_isGM(sd) ) )
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    sprintf(output, "%s: %s (%d,%d)", pl_sd->status.name, &pl_sd->mapname,
             pl_sd->x, pl_sd->y);
    clif_displaymessage(fd, output);

    return 0;
}

/// warp to a player
int atcommand_goto(int fd, MapSessionData *sd,
                    const char *, const char *message)
{
    char character[100];

    if (!message || !*message || sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }

    if (maps[pl_sd->m].flag.nowarpto
        && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp to the map of this player.");
        return -1;
    }
    if (maps[sd->m].flag.nowarp
        && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp from your current map.");
        return -1;
    }
    pc_setpos(sd, Point{pl_sd->mapname, pl_sd->x, pl_sd->y}, BeingRemoveType::WARP);

    char output[200];
    sprintf(output, "Jump to %s", character);
    clif_displaymessage(fd, output);
    return 0;
}

/// warp, but within a map
int atcommand_jump(int fd, MapSessionData *sd,
                    const char *, const char *message)
{
    short x = 0, y = 0;

    // parameters optional
    sscanf(message, "%hd %hd", &x, &y);

    if (x <= 0 || x >= maps[sd->m].xs)
        x = MRAND(maps[sd->m].xs - 1) + 1;
    if (y <= 0 || x >= maps[sd->m].xs)
        y = MRAND(maps[sd->m].xs - 1) + 1;

    if ((maps[sd->m].flag.nowarpto || maps[sd->m].flag.nowarp)
        && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp within your current map.");
        return -1;
    }

    pc_setpos(sd, Point{sd->mapname, x, y}, BeingRemoveType::WARP);

    char output[200];
    sprintf(output, "Jump to %d %d", x, y);
    clif_displaymessage(fd, output);

    return 0;
}

/// List online players with location
int atcommand_who(int fd, MapSessionData *sd,
                   const char *, const char *message)
{
    char match_text[100];

    if (sscanf(message, "%99[^\n]", match_text) < 1)
        strcpy(match_text, "");

    int count = 0;
    gm_level_t gm_level = pc_isGM(sd);
    for (MapSessionData *pl_sd : auth_sessions)
    {
        gm_level_t pl_gm_level = pc_isGM(pl_sd);
        if ((battle_config.hide_GM_session || (pl_sd->status.option & OPTION_HIDE))
                && pl_gm_level > gm_level)
            continue;
        if (!strcasestr(pl_sd->status.name, match_text))
            continue;

        char output[200];
        if (pl_gm_level)
        sprintf(output, "Name: %s (GM:%d) | Location: %s %d %d",
                     pl_sd->status.name, pl_gm_level,
                     &pl_sd->mapname, pl_sd->x, pl_sd->y);
        else
            sprintf(output, "Name: %s | Location: %s %d %d",
                     pl_sd->status.name, &pl_sd->mapname,
                     pl_sd->x, pl_sd->y);
        clif_displaymessage(fd, output);
        count++;
    }

    if (count == 0)
        clif_displaymessage(fd, "No player found.");
    else if (count == 1)
        clif_displaymessage(fd, "1 player found.");
    else
    {
        char output[200];
        sprintf(output, "%d players found.", count);
        clif_displaymessage(fd, output);
    }

    return 0;
}

/// List online players with party name
int atcommand_whogroup(int fd, MapSessionData *sd,
                        const char *, const char *message)
{
    char match_text[100];
    if (sscanf(message, "%99[^\n]", match_text) < 1)
        strcpy(match_text, "");

    int count = 0;
    gm_level_t gm_level = pc_isGM(sd);
    for (MapSessionData *pl_sd : auth_sessions)
    {
        gm_level_t pl_gm_level = pc_isGM(pl_sd);
        if ((battle_config.hide_GM_session || (pl_sd->status.option & OPTION_HIDE))
                && (pl_gm_level > gm_level))
            continue;

        if (!strcasestr(pl_sd->status.name, match_text))
            continue;

        const char *temp0 = "None";
        struct party *p = party_search(pl_sd->status.party_id);
        if (p)
            temp0 = p->name;

        char output[200];
        if (pl_gm_level)
                sprintf(output, "Name: %s (GM:%d) | Party: '%s'",
                     pl_sd->status.name, pl_gm_level, temp0);
        else
            sprintf(output, "Name: %s | Party: '%s'",
                     pl_sd->status.name, temp0);
        clif_displaymessage(fd, output);
        count++;
    }

    if (count == 0)
        clif_displaymessage(fd, "No player found.");
    else if (count == 1)
        clif_displaymessage(fd, "1 player found.");
    else
    {
        char output[200];
        sprintf(output, "%d players found", count);
        clif_displaymessage(fd, output);
    }

    return 0;
}

/// List online players on map, with location
int atcommand_whomap(int fd, MapSessionData *sd,
                      const char *, const char *message)
{
    int map_id = sd->m;
    if (message && *message)
    {
        fixed_string<16> map_name;
        sscanf(message, "%15s", &map_name);
        if (!map_name.contains(".gat") && map_name.length() < 13)   // 16 - 4 (.gat)
        strcat(&map_name, ".gat");
        int m = map_mapname2mapid(map_name);
        if (m >= 0)
            map_id = m;
    }

    int count = 0;
    gm_level_t gm_level = pc_isGM(sd);
    for (MapSessionData *pl_sd : auth_sessions)
    {
        if (pl_sd->m != map_id)
            continue;

        gm_level_t pl_gm_level = pc_isGM(pl_sd);
        if ((battle_config.hide_GM_session || (pl_sd->status.option & OPTION_HIDE))
                && pl_gm_level > gm_level)
            continue;

        char output[200];
        if (pl_gm_level)
        sprintf(output, "Name: %s (GM:%d) | Location: %s %d %d",
                     pl_sd->status.name, pl_gm_level,
                     &pl_sd->mapname, pl_sd->x, pl_sd->y);
        else
            sprintf(output, "Name: %s | Location: %s %d %d",
                     pl_sd->status.name, &pl_sd->mapname,
                     pl_sd->x, pl_sd->y);
        clif_displaymessage(fd, output);
        count++;
    }

    char output[200];
    if (count == 0)
        sprintf(output, "No player found in map '%s'.", &maps[map_id].name);
    else if (count == 1)
        sprintf(output, "1 player found in map '%s'.", &maps[map_id].name);
    else
    {
        sprintf(output, "%d players found in map '%s'.", count, &maps[map_id].name);
    }
    clif_displaymessage(fd, output);

    return 0;
}

/// List online players on map, with party
int atcommand_whomapgroup(int fd, MapSessionData *sd,
                           const char *, const char *message)
{
    int map_id = sd->m;
    if (message && *message)
    {
        fixed_string<16> map_name;
        sscanf(message, "%15s", &map_name);
        if (!map_name.contains(".gat") && map_name.length() < 13)   // 16 - 4 (.gat)
            strcat(&map_name, ".gat");
        int m = map_mapname2mapid(map_name);
        if (m >= 0)
            map_id = m;
    }

    int count = 0;
    gm_level_t gm_level = pc_isGM(sd);
    for (MapSessionData *pl_sd : auth_sessions)
    {
        gm_level_t pl_gm_level = pc_isGM(pl_sd);
        if ((battle_config.hide_GM_session || (pl_sd->status.option & OPTION_HIDE))
                && pl_gm_level > gm_level)
            continue;
        if (pl_sd->m == map_id)
        {
            struct party *p = party_search(pl_sd->status.party_id);
            const char *temp0 = "None";
            if (p)
                temp0 = p->name;
            char output[200];
            if (pl_gm_level)
                sprintf(output, "Name: %s (GM:%d) | Party: '%s'",
                         pl_sd->status.name, pl_gm_level, temp0);
            else
                sprintf(output, "Name: %s | Party: '%s'",
                         pl_sd->status.name, temp0);
            clif_displaymessage(fd, output);
            count++;
        }
    }

    char output[200];
    if (count == 0)
        sprintf(output, "No player found in map '%s'.", &maps[map_id].name);
    else if (count == 1)
        sprintf(output, "1 player found in map '%s'.", &maps[map_id].name);
    else
    {
        sprintf(output, "%d players found in map '%s'.", count, &maps[map_id].name);
    }
    clif_displaymessage(fd, output);

    return 0;
}

/// List online GMs, with various info
int atcommand_whogm(int fd, MapSessionData *sd,
                     const char *, const char *message)
{
    char match_text[100];

    if (sscanf(message, "%99[^\n]", match_text) < 1)
        strcpy(match_text, "");

    int count = 0;
    gm_level_t gm_level = pc_isGM(sd);
    for (MapSessionData *pl_sd : auth_sessions)
    {
        gm_level_t pl_gm_level = pc_isGM(pl_sd);
        if (!pl_gm_level)
            continue;
        if ((battle_config.hide_GM_session || (pl_sd->status.option & OPTION_HIDE))
                && pl_gm_level > gm_level)
            continue;
        if (!strcasestr(pl_sd->status.name, match_text))
            continue;

        char output[200];
        sprintf(output, "Name: %s (GM:%d) | Location: %s %d %d",
                 pl_sd->status.name, pl_gm_level,
                 &pl_sd->mapname, pl_sd->x, pl_sd->y);
        clif_displaymessage(fd, output);

        struct party *p = party_search(pl_sd->status.party_id);
        const char *temp0 = "None";
        if (p)
            temp0 = p->name;
        sprintf(output, "       BLvl: %d | Party: %s",
                 pl_sd->status.base_level, temp0);
        clif_displaymessage(fd, output);

        count++;
    }

    if (count == 0)
        clif_displaymessage(fd, "No GM found.");
    else if (count == 1)
        clif_displaymessage(fd, "1 GM found.");
    else
    {
        char output[200];
        sprintf(output, "%d GMs found.", count);
        clif_displaymessage(fd, output);
    }

    return 0;
}

/// Set savepoint at your current location
int atcommand_save(int fd, MapSessionData *sd,
                    const char *, const char *)
{
    nullpo_retr(-1, sd);

    pc_setsavepoint(sd, Point{sd->mapname, sd->x, sd->y});
    pc_makesavestatus(sd);
    chrif_save(sd);
    clif_displaymessage(fd, "Character data respawn point saved.");

    return 0;
}

/// Warp to your savepoint
int atcommand_load(int fd, MapSessionData *sd,
                    const char *, const char *)
{
    int m = map_mapname2mapid(sd->status.save_point.map);
    if (m >= 0 && maps[m].flag.nowarpto
        && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp you to your save map.");
        return -1;
    }
    if (maps[sd->m].flag.nowarp
        && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp you from your actual map.");
        return -1;
    }

    pc_setpos(sd, sd->status.save_point, BeingRemoveType::ZERO);
    clif_displaymessage(fd, "Warping to respawn point.");

    return 0;
}

/// Set your walk delay
int atcommand_speed(int fd, MapSessionData *sd,
                     const char *, const char *message)
{
    if (!message || !*message)
        return -1;

    int speed = atoi(message);
    if (speed < MIN_WALK_SPEED || speed > MAX_WALK_SPEED)
        return -1;
    sd->speed = speed;

    clif_updatestatus(sd, SP::SPEED);
    clif_displaymessage(fd, "Speed changed.");

    return 0;
}

/// Open your storage from anywhere
int atcommand_storage(int fd, MapSessionData *sd,
                       const char *, const char *)
{
    nullpo_retr(-1, sd);

    if (sd->state.storage_flag)
    {
        clif_displaymessage(fd, "??");
        return -1;
    }

    struct storage *stor = account2storage2(sd->status.account_id);
    if (stor && stor->storage_status == 1)
    {
        clif_displaymessage(fd, "??");
        return -1;
    }

    storage_storageopen(sd);

    return 0;
}

/// Set display options (mostly unused)
int atcommand_option(int fd, MapSessionData *sd,
                      const char *, const char *message)
{
    int param1 = 0, param2 = 0, param3 = 0;
    nullpo_retr(-1, sd);

    if (!message || !*message)
        return -1;
    if (sscanf(message, "%d %d %d", &param1, &param2, &param3) < 1)
        return -1;
    if (param1 < 0 || param2 < 0 || param3 < 0)
        return -1;

    sd->opt1 = param1;
    sd->opt2 = param2;
    sd->status.option = param3;

    clif_changeoption(sd);
    pc_calcstatus(sd, 0);
    clif_displaymessage(fd, "Options changed.");

    return 0;
}

/// Hide from monsters and scripts
int atcommand_hide(int fd, MapSessionData *sd,
                    const char *, const char *)
{
    if (sd->status.option & OPTION_HIDE)
    {
        sd->status.option &= ~OPTION_HIDE;
        clif_displaymessage(fd, "Invisible: Off");
    }
    else
    {
        sd->status.option |= OPTION_HIDE;
        clif_displaymessage(fd, "Invisible: On");
    }
    clif_changeoption(sd);

    return 0;
}

/// Suicide so you can respawn
int atcommand_die(int fd, MapSessionData *sd,
                   const char *, const char *)
{
    pc_damage(NULL, sd, sd->status.hp);
    clif_displaymessage(fd, "A pity! You've died.");

    return 0;
}

/// Kill another player
int atcommand_kill(int fd, MapSessionData *sd,
                    const char *, const char *message)
{
    char character[100];

    if (!message || !*message)
        return -1;
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    MapSessionData *pl_sd  = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;
    }
    pc_damage(NULL, pl_sd, pl_sd->status.hp);
    clif_displaymessage(fd, "Character killed.");

    return 0;
}

/// revive yourself from being dead
int atcommand_alive(int fd, MapSessionData *sd,
                     const char *, const char *)
{
    sd->status.hp = sd->status.max_hp;
    sd->status.sp = sd->status.max_sp;
    pc_setstand(sd);
    if (battle_config.pc_invincible_time > 0)
        pc_setinvincibletimer(sd, battle_config.pc_invincible_time);
    clif_updatestatus(sd, SP::HP);
    clif_updatestatus(sd, SP::SP);
    clif_resurrection(sd, 1);
    clif_displaymessage(fd, "You've been revived! It's a miracle!");

    return 0;
}

/// Do a global announcement
int atcommand_kami(int, MapSessionData *,
                    const char *, const char *message)
{
    if (!message || !*message)
        return -1;

    char output[200];
    sscanf(message, "%199[^\n]", output);
    intif_GMmessage(output, strlen(output) + 1);

    return 0;
}

/// Recover HP and SP
int atcommand_heal(int fd, MapSessionData *sd,
                    const char *, const char *message)
{
    int hp = 0, sp = 0;

    sscanf(message, "%d %d", &hp, &sp);

    if (hp == 0 && sp == 0)
    {
        hp = sd->status.max_hp - sd->status.hp;
        sp = sd->status.max_sp - sd->status.sp;
    }
    else
    {
        if (hp > 0 && hp + sd->status.hp > sd->status.max_hp )
            hp = sd->status.max_hp - sd->status.hp;
        else if (hp < 0 && hp + sd->status.hp < 1)
            hp = 1 - sd->status.hp;

        if (sp > 0 && sp + sd->status.sp > sd->status.max_sp)
            sp = sd->status.max_sp - sd->status.sp;
        else if (sp < 0 && sp + sd->status.sp < 1)
            sp = 1 - sd->status.sp;
    }

    if (hp < 0)            // display like damage
        clif_damage(sd, sd, gettick(), 0, 0, -hp, 0, 4, 0);

    // happens unless you already had full hp and sp
    if (hp || sp)
    {
        pc_heal(sd, hp, sp);
        if (hp >= 0 && sp >= 0)
            clif_displaymessage(fd, "HP, SP recovered.");
        else
            clif_displaymessage(fd, "HP and/or SP modified.");
    }
    else
    {
        clif_displaymessage(fd, "HP and SP are already full.");
        return -1;
    }

    return 0;
}

/// Spawn items in your inventory
int atcommand_item(int fd, MapSessionData *sd,
                    const char *, const char *message)
{
    char item_name[100];
    int number = 0;

    if (!message || !*message)
        return -1;
    if (sscanf(message, "%99s %d", item_name, &number) < 1)
        return -1;

    if (number <= 0)
        number = 1;

    struct item_data *item_data = itemdb_searchname(item_name);
    if (!item_data)
        item_data = itemdb_exists(atoi(item_name));
    if (!item_data)
    {
        clif_displaymessage(fd, "Invalid item ID or name.");
        return -1;
    }
    int item_id = item_data->nameid;

    // number of items to spawn at once
    int get_count = number;
    if (item_data->type == 4 || item_data->type == 5 ||
        item_data->type == 7 || item_data->type == 8)
    {
        // nonstackable items
        get_count = 1;
    }
    for (int i = 0; i < number; i += get_count)
    {
        struct item item_tmp = {};
        item_tmp.nameid = item_id;
        PickupFail flag = pc_additem(sd, &item_tmp, get_count);
        if (flag != PickupFail::OKAY)
            clif_additem(sd, 0, 0, flag);
    }
    clif_displaymessage(fd, "Item created.");

    return 0;
}

/// Remove all of your items
int atcommand_itemreset(int fd, MapSessionData *sd,
                         const char *, const char *)
{

    for (int i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].amount && sd->status.inventory[i].equip == 0)
            pc_delitem(sd, i, sd->status.inventory[i].amount, 0);
    }
    clif_displaymessage(fd, "All of your items have been removed.");

    return 0;
}

/// Check whether your items are valid
int atcommand_itemcheck(int, MapSessionData *sd,
                         const char *, const char *)
{
    pc_checkitem(sd);

    return 0;
}

/// Gain levels
int atcommand_baselevelup(int fd, MapSessionData *sd,
                           const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int level = atoi(message);
    if (level == 0)
        return -1;

    if (sd->status.base_level + level > battle_config.maximum_level)
        level = battle_config.maximum_level - sd->status.base_level;
    else if (sd->status.base_level + level < 1 )
        level = 1 - sd->status.base_level;

    if (level > 0)
    {
        if (sd->status.base_level == battle_config.maximum_level)
        {                       // check for max level by Valaris
            clif_displaymessage(fd, "Base level can't go any higher.");
            return -1;
        }                       // End Addition
        for (int i = 1; i <= level; i++)
            sd->status.status_point += (sd->status.base_level + i + 14) / 4;
        sd->status.base_level += level;
        clif_updatestatus(sd, SP::BASELEVEL);
        clif_updatestatus(sd, SP::NEXTBASEEXP);
        clif_updatestatus(sd, SP::STATUSPOINT);
        pc_calcstatus(sd, 0);
        pc_heal(sd, sd->status.max_hp, sd->status.max_sp);
        clif_misceffect(sd, 0);
        clif_displaymessage(fd, "Base level raised.");
    }
    else // level < 0
    {
        if (sd->status.base_level == 1)
        {
            clif_displaymessage(fd, "Base level can't go any lower.");
            return -1;
        }
        if (sd->status.status_point > 0)
        {
            for (int i = 0; i > level; i--)
                sd->status.status_point -=
                    (sd->status.base_level + i + 14) / 4;
            if (sd->status.status_point < 0)
                sd->status.status_point = 0;
            // TODO: remove status points from stats
            clif_updatestatus(sd, SP::STATUSPOINT);
        }
        sd->status.base_level += level;
        clif_updatestatus(sd, SP::BASELEVEL);
        clif_updatestatus(sd, SP::NEXTBASEEXP);
        pc_calcstatus(sd, 0);
        clif_displaymessage(fd, "Base level lowered.");
    }

    return 0;
}

/// Increase job level
int atcommand_joblevelup(int fd, MapSessionData *sd,
                          const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int level = atoi(message);
    if (level == 0)
        return -1;

    // FIXME: This is a horrible remnant of the eA "Novice" class
    // it should be replaced by whatever the real max is
    int up_level = 10;

    if (sd->status.job_level + level > up_level)
        level = up_level - sd->status.job_level;
    if (level + sd->status.job_level < 1 )
        level = 1 - sd->status.job_level;

    if (level > 0)
    {
        if (sd->status.job_level == up_level)
        {
            clif_displaymessage(fd, "Job level can't go any higher.");
            return -1;
        }
        sd->status.job_level += level;
        clif_updatestatus(sd, SP::JOBLEVEL);
        clif_updatestatus(sd, SP::NEXTJOBEXP);
        sd->status.skill_point += level;
        clif_updatestatus(sd, SP::SKILLPOINT);
        pc_calcstatus(sd, 0);
        clif_misceffect(sd, 1);
        clif_displaymessage(fd, "Job level raised.");
    }
    else
    {
        if (sd->status.job_level == 1)
        {
            clif_displaymessage(fd, "Job level can't go any lower.");
            return -1;
        }
        sd->status.job_level += level;
        clif_updatestatus(sd, SP::JOBLEVEL);
        clif_updatestatus(sd, SP::NEXTJOBEXP);
        if (sd->status.skill_point > 0)
        {
            // is this even how TMW does skill points?
            sd->status.skill_point += level;
            if (sd->status.skill_point < 0)
                sd->status.skill_point = 0;
            // TODO: remove status points from skills
            clif_updatestatus(sd, SP::SKILLPOINT);
        }
        pc_calcstatus(sd, 0);
        clif_displaymessage(fd, "Job level lowered.");
    }

    return 0;
}


/// Show a header for a help category
static void atcommand_help_cat_name(int fd, AtCommandCategory cat)
{
    switch (cat)
    {
    case ATCC::UNK:      clif_displaymessage(fd, "-- Unknown Commands --"); return;
    case ATCC::MISC:     clif_displaymessage(fd, "-- Miscellaneous Commands --"); return;
    case ATCC::INFO:     clif_displaymessage(fd, "-- Information Commands --"); return;
    case ATCC::MSG:      clif_displaymessage(fd, "-- Message Commands --"); return;
    case ATCC::SELF:     clif_displaymessage(fd, "-- Self Char Commands --"); return;
    case ATCC::MOB:      clif_displaymessage(fd, "-- Mob Commands --"); return;
    case ATCC::ITEM:     clif_displaymessage(fd, "-- Item Commands --"); return;
    case ATCC::GROUP:    clif_displaymessage(fd, "-- Group Commands --"); return;
    case ATCC::CHAR:     clif_displaymessage(fd, "-- Other Char Commands --"); return;
    case ATCC::ENV:      clif_displaymessage(fd, "-- Environment Commands --"); return;
    case ATCC::ADMIN:    clif_displaymessage(fd, "-- Admin Commands --"); return;
    default: abort();
    }
}

/// Show usage for a command
static void atcommand_help_brief(int fd, const AtCommandInfo& info)
{
    size_t command_len = strlen(info.command);
    size_t arg_help_len = strlen(info.arg_help);
    char buf[command_len + arg_help_len + 2];
    memcpy(buf, info.command, command_len);
    buf[command_len] = ' ';
    memcpy(buf + command_len + 1, info.arg_help, arg_help_len);
    buf[command_len + arg_help_len + 1] = '\0';
    clif_displaymessage(fd, buf);
}

/// Show usage and description of a command
static void atcommand_help_long(int fd, const AtCommandInfo& info)
{
    atcommand_help_brief(fd, info);
    clif_displaymessage(fd, info.long_help);
}

/// Show help for all @commands accessible at the given level
static void atcommand_help_all(int fd, gm_level_t gm_level)
{
    AtCommandCategory cat = atcommand_info[0].cat;
    atcommand_help_cat_name(fd, cat);
    for (int i = 0; i < ARRAY_SIZEOF(atcommand_info); i++)
        if (gm_level >= atcommand_info[i].level)
        {
            if (cat != atcommand_info[i].cat)
            {
                cat = atcommand_info[i].cat;
                atcommand_help_cat_name(fd, cat);
            }
            atcommand_help_brief(fd, atcommand_info[i]);
        }
}

/// Show usage for all commands in category
static void atcommand_help_cat(int fd, gm_level_t gm_level, AtCommandCategory cat)
{
    atcommand_help_cat_name(fd, cat);
    for (int i = 0; i < ARRAY_SIZEOF(atcommand_info); i++)
        if (gm_level >= atcommand_info[i].level && atcommand_info[i].cat == cat)
            atcommand_help_brief(fd, atcommand_info[i]);
}

/// Show help for a command or a category
int atcommand_help(int fd, MapSessionData *sd,
                    const char *, const char *message)
{
    gm_level_t gm_level = pc_isGM(sd);
    if (message[0] == '@')
    {
        for (int i = 0; i < ARRAY_SIZEOF(atcommand_info); i++)
        {
            if (strcasecmp(message, atcommand_info[i].command) == 0)
            {
                if (atcommand_info[i].level > gm_level)
                    break;
                atcommand_help_long(fd, atcommand_info[i]);
                return 0;
            }
        }
        clif_displaymessage(fd, "No such command or level too low");
        return 0;
    }
    if (!*message)
    {
        clif_displaymessage(fd, "You must specify a @command or a category.");
        clif_displaymessage(fd, "Categories: all unk misc info msg self char env admin");
        return 0;
    }
    if (strcasecmp(message, "all") == 0)
    {
        atcommand_help_all(fd, gm_level);
        return 0;
    }
    if (strcasecmp(message, "unk") == 0 || strcasecmp(message, "unknown") == 0)
    {
        atcommand_help_cat(fd, gm_level, ATCC::UNK);
        return 0;
    }
    if (strcasecmp(message, "misc") == 0 || strcasecmp(message, "miscellaneous") == 0)
    {
        atcommand_help_cat(fd, gm_level, ATCC::MISC);
        return 0;
    }
    if (strcasecmp(message, "info") == 0 || strcasecmp(message, "information") == 0)
    {
        atcommand_help_cat(fd, gm_level, ATCC::INFO);
        return 0;
    }
    if (strcasecmp(message, "msg") == 0 || strcasecmp(message, "message") == 0 || strcasecmp(message, "messaging") == 0)
    {
        atcommand_help_cat(fd, gm_level, ATCC::MSG);
        return 0;
    }
    if (strcasecmp(message, "self") == 0)
    {
        atcommand_help_cat(fd, gm_level, ATCC::SELF);
        return 0;
    }
    if (strcasecmp(message, "monster") == 0 || strcasecmp(message, "monsters") == 0 ||
            strcasecmp(message, "mob") == 0 || strcasecmp(message, "mobs") == 0)
    {
        atcommand_help_cat(fd, gm_level, ATCC::MOB);
        return 0;
    }
    if (strcasecmp(message, "item") == 0 || strcasecmp(message, "items") == 0)
    {
        atcommand_help_cat(fd, gm_level, ATCC::ITEM);
        return 0;
    }
    if (strcasecmp(message, "group") == 0 || strcasecmp(message, "groups") == 0 || strcasecmp(message, "pvp") == 0)
    {
        atcommand_help_cat(fd, gm_level, ATCC::GROUP);
        return 0;
    }
    if (strcasecmp(message, "char") == 0)
    {
        atcommand_help_cat(fd, gm_level, ATCC::CHAR);
        return 0;
    }
    if (strcasecmp(message, "env") == 0 ||strcasecmp(message, "environment") == 0)
    {
        atcommand_help_cat(fd, gm_level, ATCC::ENV);
        return 0;
    }
    if (strcasecmp(message, "admin") == 0 || strcasecmp(message, "admininstration") == 0)
    {
        atcommand_help_cat(fd, gm_level, ATCC::ADMIN);
        return 0;
    }

    clif_displaymessage(fd, "No such category");
    return 0;
}

/// Become a GM (not usable by GMs), level controlled by level_new_gm in login_athena.conf
int atcommand_gm(int fd, MapSessionData *sd,
                  const char *, const char *message)
{
    char password[100];

    memset(password, '\0', sizeof(password));

    if (!message || !*message || sscanf(message, "%99[^\n]", password) < 1)
        return -1;

    if (pc_isGM(sd))
    {
        clif_displaymessage(fd, "You already have some GM powers.");
        return -1;
    }
    else
        chrif_changegm(sd->status.account_id, password, strlen(password) + 1);

    return 0;
}

/// disable PvP on the current map
int atcommand_pvpoff(int fd, MapSessionData *sd,
                      const char *, const char *)
{
    if (battle_config.pk_mode)
    {                           //disable command if server is in PK mode [Valaris]
        clif_displaymessage(fd, "This option cannot be used in PK Mode.");
        return -1;
    }

    if (!maps[sd->m].flag.pvp)
    {
        clif_displaymessage(fd, "PvP is already Off.");
        return -1;
    }
    maps[sd->m].flag.pvp = 0;
    for (MapSessionData *pl_sd : auth_sessions)
    {
        if (sd->m != pl_sd->m)
            continue;
        if (pl_sd->pvp_timer == NULL)
            continue;
        delete_timer(pl_sd->pvp_timer);
        pl_sd->pvp_timer = NULL;
    }
    clif_displaymessage(fd, "PvP: Off.");

    return 0;
}

/// Enable PvP on the current map
int atcommand_pvpon(int fd, MapSessionData *sd,
                     const char *, const char *)
{
    if (battle_config.pk_mode)
    {
        clif_displaymessage(fd, "This option cannot be used in PK Mode.");
        return -1;
    }

    if (maps[sd->m].flag.nopvp)
    {
        clif_displaymessage(fd, "PvP not allowed on this map.");
        return -1;
    }
    if (maps[sd->m].flag.pvp)
    {
        clif_displaymessage(fd, "PvP is already On.");
        return -1;
    }
    maps[sd->m].flag.pvp = 1;
    for (MapSessionData *pl_sd : auth_sessions)
    {
        if (sd->m != pl_sd->m || pl_sd->pvp_timer)
            continue;
        pl_sd->pvp_timer = add_timer(gettick() + 200, pc_calc_pvprank_timer, pl_sd->id);
        pl_sd->pvp_rank = 0;
        pl_sd->pvp_lastusers = 0;
        pl_sd->pvp_point = 5;
    }
    clif_displaymessage(fd, "PvP: On.");

    return 0;
}

/// Change your appearance (hairstyle, hair color, clothes color (unused))
int atcommand_model(int fd, MapSessionData *sd,
                     const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    unsigned int hair_style, hair_color;
    if (sscanf(message, "%u %u", &hair_style, &hair_color) < 2)
        return -1;

    if (hair_style >= NUM_HAIR_STYLES || hair_color >= NUM_HAIR_COLORS)
    {
        clif_displaymessage(fd, "An invalid number was specified.");
        return -1;
    }
    pc_changelook(sd, LOOK::HAIR, hair_style);
    pc_changelook(sd, LOOK::HAIR_COLOR, hair_color);
    clif_displaymessage(fd, "Appearance changed.");

    return 0;
}

/// Change hair style
int atcommand_hair_style(int fd, MapSessionData *sd,
                          const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    unsigned int hair_style = 0;
    if (sscanf(message, "%u", &hair_style) < 1)
        return -1;

    if (hair_style >= NUM_HAIR_STYLES)
    {
        clif_displaymessage(fd, "An invalid number was specified.");
        return -1;
    }
    pc_changelook(sd, LOOK::HAIR, hair_style);
    clif_displaymessage(fd, "Appearance changed.");

    return 0;
}


/// Change hair color
int atcommand_hair_color(int fd, MapSessionData *sd,
                          const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    unsigned int hair_color;
    if (sscanf(message, "%d", &hair_color) < 1)
        return -1;

    if (hair_color >= NUM_HAIR_COLORS)
    {
        clif_displaymessage(fd, "An invalid number was specified.");
        return -1;
    }

    pc_changelook(sd, LOOK::HAIR_COLOR, hair_color);
    clif_displaymessage(fd, "Appearance changed.");

    return 0;
}

/// Create monsters
int atcommand_spawn(int fd, MapSessionData *sd,
                     const char *command, const char *message)
{
    if (!message || !*message)
        return -1;

    char monster[100];
    unsigned int number = 0;
    unsigned short x = 0, y = 0;
    if (sscanf(message, "%99s %u %hu %hu", monster, &number, &x, &y) < 1)
        return -1;

    // If monster identifier/name argument is a name
    // check name first (to avoid possible name begining by a number)
    int mob_id = mobdb_searchname(monster);
    if (!mob_id)
        mob_id = mobdb_checkid(atoi(monster));

    if (!mob_id)
    {
        clif_displaymessage(fd, "Invalid monster ID or name.");
        return -1;
    }

    if (number <= 0)
        number = 1;

    // If value of atcommand_spawn_quantity_limit directive is greater than or equal to 1 and quantity of monsters is greater than value of the directive
    if (battle_config.atc_spawn_quantity_limit && number > battle_config.atc_spawn_quantity_limit)
        number = battle_config.atc_spawn_quantity_limit;

    map_log("%s monster='%s' id=%d count=%d at (%d,%d)\n", command, monster,
             mob_id, number, x, y);

    int count = 0;
    int range = sqrt(number) + 5;
    for (int i = 0; i < number; i++)
    {
        int j = 0;
        bool k = 0;
        while (j++ < 8 && k == 0)
        {                       // try 8 times to spawn the monster (needed for close area)
            uint16_t mx, my;
            if (x <= 0)
                mx = sd->x + (MRAND(range) - (range / 2));
            else
                mx = x;
            if (y <= 0)
                my = sd->y + (MRAND(range) - (range / 2));
            else
                my = y;
            fixed_string<16> ths;
            ths.copy_from("this");
            k = mob_once_spawn(sd, {ths, mx, my}, "", mob_id, 1, "");
        }
        count += k;
    }

    if (!count)
    {
        clif_displaymessage(fd, "Failed to summon monsters.");
        return -1;
    }
    else if (count == number)
        clif_displaymessage(fd, "All monster summoned!");
    else
    {
        char output[200];
        sprintf(output, "Summoned only %d/%d monsters", count, number);
        clif_displaymessage(fd, output);
    }

    return 0;
}

/// Kill a monster, optionally dropping loot
static void atcommand_killmonster_sub(int fd, MapSessionData *sd,
                                       const char *message, bool drop)
{
    int map_id = sd->m;
    if (message && *message)
    {
        fixed_string<16> map_name;
        sscanf(message, "%15s", &map_name);
        if (!map_name.contains(".gat") && map_name.length() < 13)   // 16 - 4 (.gat)
            strcat(&map_name, ".gat");
        int m = map_mapname2mapid(map_name);
        if (m >= 0)
            map_id = m;
    }

    map_foreachinarea(atkillmonster_sub, map_id, 0, 0, maps[map_id].xs,
                      maps[map_id].ys, BL_MOB, drop);

    clif_displaymessage(fd, "All monsters killed!");
}

/// Kill monsters, with loot
int atcommand_killmonster(int fd, MapSessionData *sd,
                           const char *, const char *message)
{
    atcommand_killmonster_sub(fd, sd, message, 1);
    return 0;
}

/// Print a nearby player
static void atlist_nearby_sub(BlockList *bl, int fd)
{
    nullpo_retv(bl);

    char buf[32];
    sprintf(buf, " - \"%s\"", static_cast<MapSessionData *>(bl)->status.name);

    clif_displaymessage(fd, buf);
}

/// Print all nearby players
int atcommand_list_nearby(int fd, MapSessionData *sd,
                           const char *, const char *)
{
    clif_displaymessage(fd, "Nearby players:");
    map_foreachinarea(atlist_nearby_sub, sd->m, sd->x - 1,
                      sd->y - 1, sd->x + 1, sd->x + 1, BL_PC, fd);
    return 0;
}

/// Kill monsters, without loot
int atcommand_killmonster2(int fd, MapSessionData *sd,
                            const char *, const char *message)
{
    atcommand_killmonster_sub(fd, sd, message, 0);
    return 0;
}

/// Display memo points
static void atcommand_memo_sub(MapSessionData *sd)
{
    clif_displaymessage(sd->fd, "Your actual memo positions are:");
    for (int i = 0; i <= 2; i++)
    {
        char output[200];
        if (sd->status.memo_point[i].map[0])
            sprintf(output, "%d - %s (%d,%d)", i,
                     &sd->status.memo_point[i].map, sd->status.memo_point[i].x,
                     sd->status.memo_point[i].y);
        else
            sprintf(output, "%d - void", i);
        clif_displaymessage(sd->fd, output);
    }
}

/// Set a memo point
int atcommand_memo(int fd, MapSessionData *sd,
                    const char *, const char *message)
{
    int position;

    if (!message || !*message || sscanf(message, "%d", &position) < 1)
    {
        atcommand_memo_sub(sd);
        return 0;
    }
    if (position < 0 || position > 2)
    {
        atcommand_memo_sub(sd);
        return -1;
    }

    if (maps[sd->m].flag.nowarpto && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to set a memo point on this map.");
        return -1;
    }
    if (sd->status.memo_point[position].map[0])
    {
        char output[200];
        sprintf(output, "You replace previous memo position %d - %s (%d,%d).",
                 position, &sd->status.memo_point[position].map,
                 sd->status.memo_point[position].x, sd->status.memo_point[position].y);
        clif_displaymessage(fd, output);
    }
    sd->status.memo_point[position].map = maps[sd->m].name;
    sd->status.memo_point[position].x = sd->x;
    sd->status.memo_point[position].y = sd->y;
    atcommand_memo_sub(sd);

    return 0;
}

/// print the collision of the map around you
int atcommand_gat(int fd, MapSessionData *sd,
                   const char *, const char *)
{
    char output[200];
    int y;

    memset(output, '\0', sizeof(output));

    for (y = -2; y <= 2; y++)
    {
        sprintf(output, "%s (x= %d, y= %d) %02X %02X %02X %02X %02X",
                 &maps[sd->m].name, sd->x - 2, sd->y + y,
                 map_getcell(sd->m, sd->x - 2, sd->y + y),
                 map_getcell(sd->m, sd->x - 1, sd->y + y),
                 map_getcell(sd->m, sd->x, sd->y + y),
                 map_getcell(sd->m, sd->x + 1, sd->y + y),
                 map_getcell(sd->m, sd->x + 2, sd->y + y));
        clif_displaymessage(fd, output);
    }

    return 0;
}

/// modify number of status points
int atcommand_statuspoint(int fd, MapSessionData *sd,
                           const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int points = atoi(message);
    if (!points)
        return -1;

    int new_status_points = sd->status.status_point + points;
    if (new_status_points > 0x7FFF)
        new_status_points = 0x7FFF;
    if (new_status_points < 0)
        new_status_points = 0;

    if (new_status_points == sd->status.status_point)
    {
        if (points < 0)
            clif_displaymessage(fd, "Impossible to decrease the number/value.");
        else
            clif_displaymessage(fd, "Impossible to increase the number/value.");
        return -1;
    }
    sd->status.status_point = static_cast<short>(new_status_points);
    clif_updatestatus(sd, SP::STATUSPOINT);
    clif_displaymessage(fd, "Number of status points changed!");

    return 0;
}

/// Modify number of skill points
int atcommand_skillpoint(int fd, MapSessionData *sd,
                          const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int points = atoi(message);
    if (!points)
        return -1;

    int new_skill_points = sd->status.skill_point + points;
    if (new_skill_points > 0x7FFF)
        new_skill_points = 0x7FFF;
    if (new_skill_points < 0)
        new_skill_points = 0;

    if (new_skill_points == sd->status.skill_point)
    {
        if (points < 0)
            clif_displaymessage(fd, "Impossible to decrease the number/value.");
        else
            clif_displaymessage(fd, "Impossible to increase the number/value.");
        return -1;
    }
    sd->status.skill_point = static_cast<short>(new_skill_points);
    clif_updatestatus(sd, SP::SKILLPOINT);
    clif_displaymessage(fd, "Number of skill points changed!");

    return 0;
}

/// Change your amount of gold
int atcommand_zeny(int fd, MapSessionData *sd,
                    const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int zeny = atoi(message);
    if (!zeny)
        return -1;

    int new_zeny = sd->status.zeny + zeny;
    if (new_zeny > MAX_ZENY)
        new_zeny = MAX_ZENY;
    if (new_zeny < 0)
        new_zeny = 0;

    if (new_zeny == sd->status.zeny)
    {
        if (zeny < 0)
            clif_displaymessage(fd, "Impossible to decrease the number/value.");
        else
            clif_displaymessage(fd, "Impossible to increase the number/value.");
        return -1;
    }
    sd->status.zeny = new_zeny;
    clif_updatestatus(sd, SP::ZENY);
    clif_displaymessage(fd, "Number of zenys changed!");

    return 0;
}

/// Set a stat (str, agi, vit ...)
template<ATTR attr>
int atcommand_param(int fd, MapSessionData *sd,
                     const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int value;
    if (sscanf(message, "%d", &value) < 1 || value == 0)
        return -1;

    int new_value = sd->status.stats[attr] + value;
    if (new_value > battle_config.max_parameter)
        new_value = battle_config.max_parameter;
    if (new_value < 1)
        new_value = 1;

    if (new_value == sd->status.stats[attr])
    {
        if (value < 0)
            clif_displaymessage(fd, "Impossible to decrease the number/value.");
        else
            clif_displaymessage(fd, "Impossible to increase the number/value.");
        return -1;
    }
    sd->status.stats[attr] = static_cast<short>(new_value);
    clif_updatestatus(sd, ATTR_TO_SP_BASE(attr));
    clif_updatestatus(sd, ATTR_TO_SP_UP(attr));
    pc_calcstatus(sd, 0);
    clif_displaymessage(fd, "Stat changed.");

    return 0;
}

/// Add points to *all* status
int atcommand_all_stats(int fd, MapSessionData *sd,
                         const char *, const char *message)
{
    int value;
    if (!message || !*message || sscanf(message, "%d", &value) < 1 || value == 0)
        value = battle_config.max_parameter;

    int count = 0;
    for (ATTR idx : ATTRs)
    {
        int new_value = static_cast<int>(sd->status.stats[idx]) + value;
        if (new_value > battle_config.max_parameter)
            new_value = battle_config.max_parameter;
        if (new_value < 1)
            new_value = 1;

        if (new_value == sd->status.stats[idx])
            continue;

        sd->status.stats[idx] = new_value;
        clif_updatestatus(sd, ATTR_TO_SP_BASE(idx));
        clif_updatestatus(sd, ATTR_TO_SP_UP(idx));
        pc_calcstatus(sd, 0);
        count++;
    }

    if (count == 6)
        clif_displaymessage(fd, "All stats changed!");
    else if (count > 0)
        clif_displaymessage(fd, "Some stats changed!");
    else
    {
        if (value < 0)
            clif_displaymessage(fd, "Impossible to decrease stats.");
        else
            clif_displaymessage(fd, "Impossible to increase stats.");
        return -1;
    }

    return 0;
}

/// Summon a player to you
int atcommand_recall(int fd, MapSessionData *sd,
                      const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);

    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level doesn't authorise you to do this action on this player.");
        return -1;
    }
    if (maps[sd->m].flag.nowarpto && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp anyone to your current map.");
        return -1;
    }
    if (maps[pl_sd->m].flag.nowarp && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp from this player's current map.");
        return -1;
    }
    pc_setpos(pl_sd, Point{sd->mapname, sd->x, sd->y}, BeingRemoveType::QUIT);
    char output[200];
    sprintf(output, "%s recalled!", character);
    clif_displaymessage(fd, output);

    return 0;
}

/// Resurrect someone else
int atcommand_revive(int fd, MapSessionData *sd,
                      const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    pl_sd->status.hp = pl_sd->status.max_hp;
    pc_setstand(pl_sd);
    if (battle_config.pc_invincible_time > 0)
        pc_setinvincibletimer(sd, battle_config.pc_invincible_time);
    clif_updatestatus(pl_sd, SP::HP);
    clif_updatestatus(pl_sd, SP::SP);
    clif_resurrection(pl_sd, 1);
    clif_displaymessage(fd, "Character revived.");

    return 0;
}

/// Show stats of another character
int atcommand_character_stats(int fd, MapSessionData *,
                               const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }

    struct
    {
        const char *name;
        int value;
    } output_table[] =
    {
        {"Base Level", pl_sd->status.base_level},
        {"Job level", pl_sd->status.job_level},
        {"Hp", pl_sd->status.hp},
        {"MaxHp", pl_sd->status.max_hp},
        {"Sp", pl_sd->status.sp},
        {"MaxSp", pl_sd->status.max_sp},
        {"Str", pl_sd->status.stats[ATTR::STR]},
        {"Agi", pl_sd->status.stats[ATTR::AGI]},
        {"Vit", pl_sd->status.stats[ATTR::VIT]},
        {"Int", pl_sd->status.stats[ATTR::INT]},
        {"Dex", pl_sd->status.stats[ATTR::DEX]},
        {"Luk", pl_sd->status.stats[ATTR::LUK]},
        {"Zeny", pl_sd->status.zeny},
    };
    char output[200];
    sprintf(output, "'%s' stats:", pl_sd->status.name);
    clif_displaymessage(fd, output);
    for (int i = 0; i < ARRAY_SIZEOF(output_table); i++)
    {
        sprintf(output, "%s - %d", output_table[i].name, output_table[i].value);
        clif_displaymessage(fd, output);
    }

    return 0;
}

/// All stats of characters
int atcommand_character_stats_all(int fd, MapSessionData *,
                                   const char *, const char *)
{
    int count = 0;
    for (MapSessionData *pl_sd : auth_sessions)
    {
        char gmlevel[1024] = "";
        if (pc_isGM(pl_sd))
            sprintf(gmlevel, "| GM Lvl: %d", pc_isGM(pl_sd));
        else
            sprintf(gmlevel, " ");

        char output[1024];
        sprintf(output,
                 "Name: %s | BLvl: %d | Job: %s (Lvl: %d) | HP: %d/%d | SP: %d/%d",
                 pl_sd->status.name, pl_sd->status.base_level,
                 "N/A", pl_sd->status.job_level,
                 pl_sd->status.hp, pl_sd->status.max_hp, pl_sd->status.sp,
                 pl_sd->status.max_sp);
        clif_displaymessage(fd, output);

        sprintf(output,
                 "STR: %d | AGI: %d | VIT: %d | INT: %d | DEX: %d | LUK: %d | Zeny: %d %s",
                 pl_sd->status.stats[ATTR::STR], pl_sd->status.stats[ATTR::AGI], pl_sd->status.stats[ATTR::VIT],
                 pl_sd->status.stats[ATTR::INT], pl_sd->status.stats[ATTR::DEX], pl_sd->status.stats[ATTR::LUK],
                 pl_sd->status.zeny, gmlevel);
        clif_displaymessage(fd, output);
        clif_displaymessage(fd, "--------");

        count++;
    }

    if (count == 0)
        clif_displaymessage(fd, "No player found.");
    else if (count == 1)
        clif_displaymessage(fd, "1 player found.");
    else
    {
        char output[1024];
        sprintf(output, "%d players found.", count);
        clif_displaymessage(fd, output);
    }

    return 0;
}

/// Change display options of a character
int atcommand_character_option(int fd, MapSessionData *sd,
                                const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    int opt1 = 0, opt2 = 0, opt3 = 0;
    if (sscanf(message, "%d %d %d %99[^\n]", &opt1, &opt2, &opt3, character) < 4
            || opt1 < 0 || opt2 < 0 || opt3 < 0)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);

    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;
    }
    pl_sd->opt1 = opt1;
    pl_sd->opt2 = opt2;
    pl_sd->status.option = opt3;
    clif_changeoption(pl_sd);
    pc_calcstatus(pl_sd, 0);
    clif_displaymessage(fd, "Character's options changed.");

    return 0;
}

/// Toggle a character's sex
int atcommand_char_change_sex(int fd, MapSessionData *sd,
                               const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    if (strlen(character) > 23)
    {
        clif_displaymessage(fd, "Sorry, but a player name have 23 characters maximum.");
        return -1;
    }
    else
    {
        // type: 5 - changesex
        chrif_char_ask_name(sd->status.account_id, character, CharOperation::CHANGE_SEX);
        clif_displaymessage(fd, "Character name sends to char-server to ask it.");
    }

    return 0;
}

/// Block the account a character indefinitely
int atcommand_char_block(int fd, MapSessionData *sd,
                          const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    if (strlen(character) > 23)
    {
        clif_displaymessage(fd, "Sorry, but a player name have 23 characters maximum.");
        return -1;
    }
    else
    {
        chrif_char_ask_name(sd->status.account_id, character, CharOperation::BLOCK);
        clif_displaymessage(fd, "Character name sends to char-server to ask it.");
    }

    return 0;
}

/**
 * charban command(usage: charban <time> <player_name>)
 * This command do a limited ban on a player
 * Time is done as follows:
 *   Adjustment value (-1, 1, +1, etc...)
 *   Modified element:
 *     y: year
 *     m:  month
 *     d: day
 *     h:  hour
 *     mn: minute
 *     s:  second
 * <example> @ban +1m-2mn1s-6y test_player
 *           this example adds 1 month and 1 second, and substracts 2 minutes and 6 years at the same time.
 */
int atcommand_char_ban(int fd, MapSessionData *sd,
                        const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char modif[100], character[100];
    if (sscanf(message, "%99s %99[^\n]", modif, character) < 2)
        return -1;

    const char *modif_p = modif;
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    while (*modif_p)
    {
        int value = atoi(modif_p);
        if (value == 0)
        {
            modif_p++;
            continue;
        }
        if (modif_p[0] == '-' || modif_p[0] == '+')
            modif_p++;
        while (modif_p[0] >= '0' && modif_p[0] <= '9')
            modif_p++;
        if (modif_p[0] == 's')
        {
            second = value;
            modif_p++;
        }
        else if (modif_p[0] == 'm' && modif_p[1] == 'n')
        {
            minute = value;
            modif_p += 2;
        }
        else if (modif_p[0] == 'h')
        {
            hour = value;
            modif_p++;
        }
        else if (modif_p[0] == 'd')
        {
            day = value;
            modif_p++;
        }
        else if (modif_p[0] == 'm')
        {
            month = value;
            modif_p++;
        }
        else if (modif_p[0] == 'y')
        {
            year = value;
            modif_p++;
        }
        else if (modif_p[0])
        {
            modif_p++;
        }
    }
    if (!year && !month && !day && !hour && !minute && !second)
    {
        clif_displaymessage(fd, "Can't ban for no time.");
        return -1;
    }

    if (strlen(character) > 23)
    {
        clif_displaymessage(fd, "Sorry, but a player name have 23 characters maximum.");
        return -1;
    }
    else
    {
        chrif_char_ask_name(sd->status.account_id, character, CharOperation::BAN, year, month, day, hour, minute, second);
        clif_displaymessage(fd, "Character name sends to char-server to ask it.");
    }

    return 0;
}

/// Remove an indefinite block
int atcommand_char_unblock(int fd, MapSessionData *sd,
                            const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    if (strlen(character) > 23)
    {
        clif_displaymessage(fd, "Sorry, but a player name have 23 characters maximum.");
        return -1;
    }
    else
    {
        // send answer to login server via char-server
        chrif_char_ask_name(sd->status.account_id, character, CharOperation::UNBLOCK);
        clif_displaymessage(fd, "Character name sends to char-server to ask it.");
    }

    return 0;
}

/// Remove a temporary ban
int atcommand_char_unban(int fd, MapSessionData *sd,
                          const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    if (strlen(character) > 23)
    {
        clif_displaymessage(fd, "Sorry, but a player name have 23 characters maximum.");
        return -1;
    }
    else
    {
        // send answer to login server via char-server
        chrif_char_ask_name(sd->status.account_id, character, CharOperation::UNBAN);
        clif_displaymessage(fd, "Character name sends to char-server to ask it.");
    }

    return 0;
}

/// Set save point for a character
int atcommand_character_save(int fd, MapSessionData *sd,
                              const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    short x, y;
    char character[100];
    fixed_string<16> map_name;
    if (sscanf(message, "%15s %hd %hd %99[^\n]", &map_name, &x, &y, character) < 4
            || x < 0 || y < 0)
        return -1;

    if (!map_name.contains(".gat") && map_name.length() < 13)   // 16 - 4 (.gat)
        strcat(&map_name, ".gat");

    MapSessionData *pl_sd = map_nick2sd(character);

    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;
    }
    int m = map_mapname2mapid(map_name);
    if (m < 0)
    {
        clif_displaymessage(fd, "Map not found.");
        return -1;
    }
    if (maps[m].flag.nowarpto && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to set this map as a save map.");
        return -1;
    }
    pc_setsavepoint(pl_sd, Point{map_name, x, y});
    clif_displaymessage(fd, "Character's respawn point changed.");

    return 0;
}

/// Kill everybody
int atcommand_doom(int fd, MapSessionData *sd,
                    const char *, const char *)
{
    for (MapSessionData *pl_sd : auth_sessions)
    {
        if (sd == pl_sd)
            continue;
        if (pc_isGM(sd) < pc_isGM(pl_sd))
            continue;

        pc_damage(NULL, pl_sd, pl_sd->status.hp + 1);
        clif_displaymessage(pl_sd->fd, "The holy messenger has given judgement.");
    }
    clif_displaymessage(fd, "Judgement was made.");

    return 0;
}

/// Kill everybody on the current map
int atcommand_doommap(int fd, MapSessionData *sd,
                       const char *, const char *)
{
    for (MapSessionData *pl_sd : auth_sessions)
    {
        if (sd == pl_sd)
            continue;
        if (sd->m != pl_sd->m)
            continue;
        if (pc_isGM(sd) < pc_isGM(pl_sd))
            continue;
        pc_damage(NULL, pl_sd, pl_sd->status.hp + 1);
        clif_displaymessage(pl_sd->fd, "The holy messenger has given judgement.");
    }
    clif_displaymessage(fd, "Judgement was made.");

    return 0;
}

/// Resurrect a character
static void atcommand_raise_sub(MapSessionData *sd)
{
    if (pc_isdead(sd))
    {
        sd->status.hp = sd->status.max_hp;
        sd->status.sp = sd->status.max_sp;
        pc_setstand(sd);
        clif_updatestatus(sd, SP::HP);
        clif_updatestatus(sd, SP::SP);
        clif_resurrection(sd, 1);
        clif_displaymessage(sd->fd, "Mercy has been shown.");
    }
}

/// Resurrect all characters
int atcommand_raise(int fd, MapSessionData *,
                     const char *, const char *)
{
    for (MapSessionData *pl_sd : auth_sessions)
    {
        atcommand_raise_sub(pl_sd);
    }
    clif_displaymessage(fd, "Mercy has been granted.");

    return 0;
}

/// Raise all characters on a map
int atcommand_raisemap(int fd, MapSessionData *sd,
                        const char *, const char *)
{
    for (MapSessionData *pl_sd : auth_sessions)
    {
        if (sd->m != pl_sd->m)
            continue;
        atcommand_raise_sub(pl_sd);
    }
    clif_displaymessage(fd, "Mercy has been granted.");

    return 0;
}

/// Give a character levels
int atcommand_character_baselevel(int fd, MapSessionData *sd,
                                   const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    int level;
    if (sscanf(message, "%d %99[^\n]", &level, character) < 2 || level == 0)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;
    }

    if (pl_sd->status.base_level + level > battle_config.maximum_level)
        level = battle_config.maximum_level - sd->status.base_level;
    if (pl_sd->status.base_level + level < 1 )
        level = 1 - pl_sd->status.base_level;

    if (level > 0)
    {
        for (int i = 1; i <= level; i++)
            pl_sd->status.status_point += (pl_sd->status.base_level + i + 14) / 4;
        pl_sd->status.base_level += level;
        clif_updatestatus(pl_sd, SP::BASELEVEL);
        clif_updatestatus(pl_sd, SP::NEXTBASEEXP);
        clif_updatestatus(pl_sd, SP::STATUSPOINT);
        pc_calcstatus(pl_sd, 0);
        pc_heal(pl_sd, pl_sd->status.max_hp, pl_sd->status.max_sp);
        clif_misceffect(pl_sd, 0);
        clif_displaymessage(fd, "Character's base level raised.");
    }
    else // level < 0
    {
        if (pl_sd->status.status_point > 0)
        {
            for (int i = 0; i > level; i--)
                pl_sd->status.status_point -= (pl_sd->status.base_level + i + 14) / 4;
            if (pl_sd->status.status_point < 0)
                pl_sd->status.status_point = 0;
            clif_updatestatus(pl_sd, SP::STATUSPOINT);
        }
        pl_sd->status.base_level += level;
        pl_sd->status.base_exp = 0;
        clif_updatestatus(pl_sd, SP::BASELEVEL);
        clif_updatestatus(pl_sd, SP::NEXTBASEEXP);
        clif_updatestatus(pl_sd, SP::BASEEXP);
        pc_calcstatus(pl_sd, 0);
        clif_displaymessage(fd, "Character's base level lowered.");
    }
    // Reset their stat points to prevent extra points from stacking
    atcommand_charstreset(fd, sd,"@charstreset", character);

    return 0;
}

/// Raise a character's job level
int atcommand_character_joblevel(int fd, MapSessionData *sd,
                                  const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    int level = 0;
    if (sscanf(message, "%d %99[^\n]", &level, character) < 2 || level == 0)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level doesn't authorise you to do this action on this player.");
        return -1;
    }
    int max_level = 10;
    if (pl_sd->status.job_level + level > max_level)
        level = max_level - pl_sd->status.job_level;
    if (level + pl_sd->status.job_level < 1 )
        level = 1 - pl_sd->status.job_level;

    pl_sd->status.job_level += level;
    clif_updatestatus(pl_sd, SP::JOBLEVEL);
    clif_updatestatus(pl_sd, SP::NEXTJOBEXP);

    if (level > 0)
    {
        pl_sd->status.skill_point += level;
        clif_updatestatus(pl_sd, SP::SKILLPOINT);
        pc_calcstatus(pl_sd, 0);
        clif_misceffect(pl_sd, 1);
        clif_displaymessage(fd, "character's job level raised.");
    }
    else // level < 0
    {
        if (pl_sd->status.skill_point > 0)
        {
            pl_sd->status.skill_point += level;
            if (pl_sd->status.skill_point < 0)
                pl_sd->status.skill_point = 0;
            clif_updatestatus(pl_sd, SP::SKILLPOINT);
        }
        // TODO: remove points from skills
        pc_calcstatus(pl_sd, 0);
        clif_displaymessage(fd, "Character's job level lowered.");
    }

    return 0;
}

/// Disconnect a player
int atcommand_kick(int fd, MapSessionData *sd,
                    const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;
    }
    clif_GM_kick(sd, pl_sd, 1);

    return 0;
}

/// Disconnect all players
int atcommand_kickall(int fd, MapSessionData *sd,
                       const char *, const char *)
{
    for (MapSessionData *pl_sd : auth_sessions)
    {
        if (sd == pl_sd)
            continue;
        if (pc_isGM(sd) < pc_isGM(pl_sd))
            continue;
        clif_GM_kick(sd, pl_sd, 0);
    }

    clif_displaymessage(fd, "All players have been kicked!");

    return 0;
}

/// Create a new party, even if you don't have the party skill
int atcommand_party(int, MapSessionData *sd,
                     const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char party[100];
    if (sscanf(message, "%99[^\n]", party) < 1)
        return -1;

    party_create(sd, party);
    return 0;
}

/// Kick all players, then shutdown the map server
int atcommand_mapexit(int, MapSessionData *sd,
                       const char *, const char *)
{
    atcommand_kickall(-1, sd, NULL, NULL);
    clif_GM_kick(sd, sd, 0);
    runflag = 0;
    return 0;
}

/// Search for items including the name
int atcommand_idsearch(int fd, MapSessionData *,
                        const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char item_name[100];
    if (sscanf(message, "%99s", item_name) < 1)
        return -1;

    char output[200];
    sprintf(output, "The reference result of '%s' (name: id):", item_name);
    clif_displaymessage(fd, output);

    int match = 0;
    /// FIXME remove hard limit
    for (int i = 0; i < 20000; i++)
    {
        struct item_data *item = itemdb_exists(i);
        if (!item)
            continue;
        if (!strstr(item->jname, item_name))
            continue;
        match++;
        sprintf(output, "%s: %d", item->jname, item->nameid);
        clif_displaymessage(fd, output);
    }
    sprintf(output, "%d matches.", match);
    clif_displaymessage(fd, output);

    return 0;
}

/// Reset a characters's skills
int atcommand_charskreset(int fd, MapSessionData *sd,
                           const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;
    }
    char output[200];
    pc_resetskill(pl_sd);
    sprintf(output, "'%s' skill points reseted!", character);
    clif_displaymessage(fd, output);

    return 0;
}

/// Reset someone's stats
int atcommand_charstreset(int fd, MapSessionData *sd,
                           const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;
    }
    pc_resetstate(pl_sd);
    char output[200];
    sprintf(output, "'%s' stats points reset!", character);
    clif_displaymessage(fd, output);

    return 0;
}

/// More-or-less completely reset character
int atcommand_charreset(int fd, MapSessionData *sd,
                         const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;
    }

    pc_resetstate(pl_sd);
    pc_resetskill(pl_sd);
    /// Reset magic quest variables and experience
    // I'm not convince this should be hard-coded, maybe there should be a script?
    // TODO should anything else be reset?
    // NOTE: also done in charwipe
    pc_setglobalreg(pl_sd, "MAGIC_FLAGS", 0);
    pc_setglobalreg(pl_sd, "MAGIC_EXP", 0);
    char output[200];
    sprintf(output, "'%s' skill and stats points reset!", character);
    clif_displaymessage(fd, output);

    return 0;
}

/// (Try to) completely reset a character
int atcommand_char_wipe(int fd, MapSessionData *sd,
                         const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;

    }

    // Reset base level
    pl_sd->status.base_level = 1;
    pl_sd->status.base_exp = 0;
    clif_updatestatus(pl_sd, SP::BASELEVEL);
    clif_updatestatus(pl_sd, SP::NEXTBASEEXP);
    clif_updatestatus(pl_sd, SP::BASEEXP);

    // Reset job level
    pl_sd->status.job_level = 1;
    pl_sd->status.job_exp = 0;
    clif_updatestatus(pl_sd, SP::JOBLEVEL);
    clif_updatestatus(pl_sd, SP::NEXTJOBEXP);
    clif_updatestatus(pl_sd, SP::JOBEXP);

    // Zeny to 50
    pl_sd->status.zeny = 0;
    clif_updatestatus(pl_sd, SP::ZENY);

    // Clear inventory
    for (int i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].amount)
        {
            if (sd->status.inventory[i].equip)
                pc_unequipitem(pl_sd, i, false);
            pc_delitem(pl_sd, i, sd->status.inventory[i].amount, 0);
        }
    }

    // Reset stats and skills
    pc_calcstatus(pl_sd, 0);
    pc_resetstate(pl_sd);
    pc_resetskill(pl_sd);
    pc_setglobalreg(pl_sd, "MAGIC_FLAGS", 0);  // [Fate] Reset magic quest variables
    pc_setglobalreg(pl_sd, "MAGIC_EXP", 0);    // [Fate] Reset magic experience

    char output[200];
    sprintf(output, "%s:  wiped.", character); // '%s' skill and stats points reseted!
    clif_displaymessage(fd, output);

    return 0;
}

/// Change another player's appearance
int atcommand_charmodel(int fd, MapSessionData *,
                         const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    unsigned int hair_style, hair_color;
    char character[100];
    if (sscanf(message, "%u %u %99[^\n]", &hair_style, &hair_color, character) < 3)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (hair_style >= NUM_HAIR_STYLES || hair_color >= NUM_HAIR_COLORS)
    {
        clif_displaymessage(fd, "An invalid number was specified.");
        return -1;
    }
    pc_changelook(pl_sd, LOOK::HAIR, hair_style);
    pc_changelook(pl_sd, LOOK::HAIR_COLOR, hair_color);
    clif_displaymessage(fd, "Appearance changed.");

    return 0;
}

/// Adjust someone's skill points
int atcommand_charskpoint(int fd, MapSessionData *,
                           const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int points;
    char character[100];
    if (sscanf(message, "%d %99[^\n]", &points, character) < 2 || points == 0)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }

    int new_skill_points = pl_sd->status.skill_point + points;
    if (new_skill_points > 0x7FFF)
        new_skill_points = 0x7FFF;
    if (new_skill_points < 0)
        new_skill_points = 0;
    if (new_skill_points != pl_sd->status.skill_point)
    {
        pl_sd->status.skill_point = new_skill_points;
        clif_updatestatus(pl_sd, SP::SKILLPOINT);
        clif_displaymessage(fd, "Character's number of skill points changed!");
    }
    else
    {
        if (points < 0)
            clif_displaymessage(fd, "Impossible to decrease the number/value.");
        else
            clif_displaymessage(fd, "Impossible to increase the number/value.");
        return -1;
    }

    return 0;
}

/// Adjust someone's status points
int atcommand_charstpoint(int fd, MapSessionData *,
                           const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int points;
    char character[100];
    if (sscanf(message, "%d %99[^\n]", &points, character) < 2 || points == 0)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    int new_status_points = pl_sd->status.status_point + points;
    if (new_status_points > 0x7FFF)
        new_status_points = 0x7FFF;
    if (new_status_points < 0)
        new_status_points = 0;
    if (new_status_points != pl_sd->status.status_point)
    {
        pl_sd->status.status_point = new_status_points;
        clif_updatestatus(pl_sd, SP::STATUSPOINT);
        clif_displaymessage(fd, "Character's number of status points changed!");
    }
    else
    {
        if (points < 0)
            clif_displaymessage(fd, "Impossible to decrease the number/value.");
        else
            clif_displaymessage(fd, "Impossible to increase the number/value.");
        return -1;
    }

    return 0;
}

/// Give somebody zeny
int atcommand_charzeny(int fd, MapSessionData *,
                        const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int zeny;
    char character[100];
    if (sscanf(message, "%d %99[^\n]", &zeny, character) < 2 || zeny == 0)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    int new_zeny = pl_sd->status.zeny + zeny;
    if (zeny > 0 && (zeny > MAX_ZENY || new_zeny > MAX_ZENY))   // fix positiv overflow
        new_zeny = MAX_ZENY;
    else if (zeny < 0 && (zeny < -MAX_ZENY || new_zeny < 0))    // fix negativ overflow
        new_zeny = 0;
    if (new_zeny != pl_sd->status.zeny)
    {
        pl_sd->status.zeny = new_zeny;
        clif_updatestatus(pl_sd, SP::ZENY);
        clif_displaymessage(fd, "Character's number of zenys changed!");
    }
    else
    {
        if (zeny < 0)
            clif_displaymessage(fd, "Impossible to decrease the number/value.");
        else
            clif_displaymessage(fd, "Impossible to increase the number/value.");
        return -1;
    }

    return 0;
}

/// Warp all online characters to your location
int atcommand_recallall(int fd, MapSessionData *sd,
                         const char *, const char *)
{
    if (maps[sd->m].flag.nowarpto && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp somenone to your current map.");
        return -1;
    }

    int count = 0;
    for (MapSessionData *pl_sd : auth_sessions)
    {
        if (sd->status.account_id != pl_sd->status.account_id)
            continue;
        if (pc_isGM(sd) < pc_isGM(pl_sd))
            continue;

        if (maps[pl_sd->m].flag.nowarp && battle_config.any_warp_GM_min_level > pc_isGM(sd))
            count++;
        else
            pc_setpos(pl_sd, Point{sd->mapname, sd->x, sd->y}, BeingRemoveType::QUIT);
    }

    clif_displaymessage(fd, "All characters recalled!");
    if (count)
    {
        char output[200];
        sprintf(output,
                 "Because you are not authorised to warp from some maps, %d player(s) have not been recalled.",
                 count);
        clif_displaymessage(fd, output);
    }

    return 0;
}

/// Warp all members of a party to your location
int atcommand_partyrecall(int fd, MapSessionData *sd,
                           const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char party_name[100];
    if (sscanf(message, "%99[^\n]", party_name) < 1)
        return -1;

    if (maps[sd->m].flag.nowarpto && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp somenone to your actual map.");
        return -1;
    }

    struct party *p = party_searchname(party_name);
    if (!p)
        p = party_search(atoi(message));
    {
        clif_displaymessage(fd, "Incorrect name or ID, or no one from the party is online.");
        return -1;
    }
    int count = 0;
    for (MapSessionData *pl_sd : auth_sessions)
    {
        if (sd->status.account_id == pl_sd->status.account_id)
            continue;
        if (pl_sd->status.party_id != p->party_id)
            continue;
        if (maps[pl_sd->m].flag.nowarp && battle_config.any_warp_GM_min_level > pc_isGM(sd))
            count++;
        else
            pc_setpos(pl_sd, Point{sd->mapname, sd->x, sd->y}, BeingRemoveType::QUIT);
    }
    char output[200];
    sprintf(output, "All online characters of the %s party are near you.", p->name);
    clif_displaymessage(fd, output);
    if (count)
    {
        sprintf(output,
                 "Because you are not authorised to warp from some maps, %d player(s) have not been recalled.",
                 count);
        clif_displaymessage(fd, output);
    }

    return 0;
}

/**
 * @mapinfo <map name> [0-2] by MC_Cameri
 * => Shows information about the map [map name]
 * 0 = no additional information
 * 1 = Show users in that map and their location
 * 2 = Shows NPCs in that map
 */
int atcommand_mapinfo(int fd, MapSessionData *sd,
                       const char *, const char *message)
{
    int list = 0;
    fixed_string<16> map_name;
    sscanf(message, "%d %15[^\n]", &list, &map_name);
    if (list < 0 || list > 2)
        return -1;

    if (map_name[0] == '\0')
        map_name = sd->mapname;
    if (!map_name.contains(".gat") && map_name.length() < 13)   // 16 - 4 (.gat)
        strcat(&map_name, ".gat");

    int m_id = map_mapname2mapid(map_name);
    if (m_id < 0)
    {
        clif_displaymessage(fd, "Map not found.");
        return -1;
    }

    char output[200];
    clif_displaymessage(fd, "------ Map Info ------");
    sprintf(output, "Map Name: %s", &map_name);
    clif_displaymessage(fd, output);
    sprintf(output, "Players In Map: %d", maps[m_id].users);
    clif_displaymessage(fd, output);
    sprintf(output, "NPCs In Map: %d", maps[m_id].npc_num);
    clif_displaymessage(fd, output);
    clif_displaymessage(fd, "------ Map Flags ------");
    sprintf(output, "Player vs Player: %s | No Party: %s",
             (maps[m_id].flag.pvp) ? "True" : "False",
             (maps[m_id].flag.pvp_noparty) ? "True" : "False");
    clif_displaymessage(fd, output);
    sprintf(output, "No Dead Branch: %s",
             (maps[m_id].flag.nobranch) ? "True" : "False");
    clif_displaymessage(fd, output);
    sprintf(output, "No Memo: %s",
             (maps[m_id].flag.nomemo) ? "True" : "False");
    clif_displaymessage(fd, output);
    sprintf(output, "No Penalty: %s",
             (maps[m_id].flag.nopenalty) ? "True" : "False");
    clif_displaymessage(fd, output);
    sprintf(output, "No Return: %s",
             (maps[m_id].flag.noreturn) ? "True" : "False");
    clif_displaymessage(fd, output);
    sprintf(output, "No Save: %s",
             (maps[m_id].flag.nosave) ? "True" : "False");
    clif_displaymessage(fd, output);
    sprintf(output, "No Teleport: %s",
             (maps[m_id].flag.noteleport) ? "True" : "False");
    clif_displaymessage(fd, output);
    sprintf(output, "No Monster Teleport: %s",
             (maps[m_id].flag.monster_noteleport) ? "True" : "False");
    clif_displaymessage(fd, output);
    sprintf(output, "No Zeny Penalty: %s",
             (maps[m_id].flag.nozenypenalty) ? "True" : "False");
    clif_displaymessage(fd, output);

    switch (list)
    {
    case 1:
        clif_displaymessage(fd, "----- Players in Map -----");
        for (MapSessionData *pl_sd : auth_sessions)
        {
            if (pl_sd->m != m_id)
                continue;
            sprintf(output,
                    "Player '%s' (session #%d) | Location: %d,%d",
                    pl_sd->status.name, pl_sd->fd, pl_sd->x, pl_sd->y);
            clif_displaymessage(fd, output);
        }
        break;
    case 2:
        clif_displaymessage(fd, "----- NPCs in Map -----");
        for (int i = 0; i < maps[m_id].npc_num;)
        {
            struct npc_data *nd = maps[m_id].npc[i];
            const char *direction = "Unknown";
            switch (nd->dir)
            {
            case Direction::S: direction = "south"; break;
            case Direction::SW: direction = "southwest"; break;
            case Direction::W: direction = "west"; break;
            case Direction::NW: direction = "northwest"; break;
            case Direction::N: direction = "north"; break;
            case Direction::NE: direction = "northeast"; break;
            case Direction::E: direction = "east"; break;
            case Direction::SE: direction = "southeast"; break;
            }
            sprintf(output,
                     "NPC %d: %s | Direction: %s | Sprite: %d | Location: %d %d",
                     ++i, nd->name, direction, nd->npc_class, nd->x, nd->y);
            clif_displaymessage(fd, output);
        }
        break;
    }
    return 0;
}

/// Enable an NPC
int atcommand_enablenpc(int fd, MapSessionData *,
                         const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char NPCname[100];
    if (sscanf(message, "%99[^\n]", NPCname) < 1)
        return -1;

    if (npc_enable(NPCname, 1) == 0)
        clif_displaymessage(fd, "Npc Enabled.");
    else
    {
        clif_displaymessage(fd, "This NPC doesn't exist.");
        return -1;
    }

    return 0;
}

/// Disable an NPC
int atcommand_disablenpc(int fd, MapSessionData *,
                          const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char NPCname[100];
    if (sscanf(message, "%99[^\n]", NPCname) < 1)
        return -1;

    if (npc_enable(NPCname, 0) == 0)
        clif_displaymessage(fd, "Npc Disabled.");
    else
    {
        clif_displaymessage(fd, "This NPC doesn't exist.");
        return -1;
    }

    return 0;
}

/// Display the date/time of the server (should be UTC)
int atcommand_servertime(int fd, MapSessionData *,
                          const char *, const char *)
{
    time_t time_server = time(&time_server);
    struct tm *datetime = gmtime(&time_server);

    char temp[256];
    strftime(temp, sizeof(temp), "Server time (normal time): %A, %B %d %Y %X.", datetime);
    clif_displaymessage(fd, temp);

    return 0;
}

/**
 * @chardelitem <item_name_or_ID> <quantity> <player>
 * removes <quantity> item from a character
 * item can be equipped or not.
 */
int atcommand_chardelitem(int fd, MapSessionData *sd,
                           const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char item_name[100];
    int number;
    char character[100];
    if (sscanf(message, "%99s %d %99[^\n]", item_name, &number, character) < 3 || number < 1)
        return -1;

    struct item_data *item_data = itemdb_searchname(item_name);
    if (!item_data)
        item_data = itemdb_exists(atoi(item_name));
    if (!item_data)
    {
        clif_displaymessage(fd, "Invalid item ID or name.");
        return -1;
    }
    int item_id = item_data->nameid;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;
    }
    int item_position = pc_search_inventory(pl_sd, item_id);
    if (item_position < 0)
    {
        clif_displaymessage(fd, "Character does not have the item.");
        return -1;
    }
    int count = 0;
    for (int i = 0; i < number && item_position >= 0; i++)
    {
        pc_delitem(pl_sd, item_position, pl_sd->status.inventory[item_position].amount, 0);
        count++;
        // items might not all be stacked
        item_position = pc_search_inventory(pl_sd, item_id);
    }
    char output[200];
    sprintf(output, "%d item(s) removed by a GM.", count);
    clif_displaymessage(pl_sd->fd, output);
    if (number == count)
        sprintf(output, "%d item(s) removed from the player.", count);
    else
        sprintf(output, "%d item(s) removed, not %d items.", count, number);
    clif_displaymessage(fd, output);

    return 0;
}

/// Broadcast a message, including name, to all map servers
int atcommand_broadcast(int, MapSessionData *sd,
                         const char *, const char *message)
{
    if (!message || !*message)
        return -1;

    // increased the size a bit because GM announcements were sometimes cut off
    char output[512];
    snprintf(output, sizeof(output), "%s : %s", sd->status.name, message);
    intif_GMmessage(output, strlen(output) + 1);

    return 0;
}

/// Broadcast a message, including name, on the current map server
int atcommand_localbroadcast(int, MapSessionData *sd,
                              const char *, const char *message)
{
    if (!message || !*message)
        return -1;

    char output[512];
    snprintf(output, sizeof(output), "%s : %s", sd->status.name, message);

    // flag 1 becomes ALL_SAMEMAP
    clif_GMmessage(sd, output, strlen(output) + 1, 1);

    return 0;
}

/// Change an account's recorded email address
// Note that we can't guaranteed the address actually points to somebody
int atcommand_email(int fd, MapSessionData *sd,
                     const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char actual_email[100];
    char new_email[100];
    if (sscanf(message, "%99s %99s", actual_email, new_email) < 2)
        return -1;

    if (e_mail_check(actual_email) == 0)
    {
        clif_displaymessage(fd, "Invalid actual email. If you have default e-mail, give a@a.com.");
        return -1;
    }
    else if (e_mail_check(new_email) == 0)
    {
        clif_displaymessage(fd, "Invalid new email. Please enter a real e-mail.");
        return -1;
    }
    else if (strcasecmp(new_email, "a@a.com") == 0)
    {
        clif_displaymessage(fd, "New email must be a real e-mail.");
        return -1;
    }
    else if (strcasecmp(actual_email, new_email) == 0)
    {
        clif_displaymessage(fd, "New email must be different of the actual e-mail.");
        return -1;
    }
    else
    {
        chrif_changeemail(sd->status.account_id, actual_email, new_email);
        clif_displaymessage(fd, "Information sent to login-server via char-server.");
    }

    return 0;
}

/// Display an effect on yourself
int atcommand_effect(int fd, MapSessionData *sd,
                      const char *, const char *message)
{
    int type = 0, flag = 0;

    if (!message || !*message || sscanf(message, "%d %d", &type, &flag) < 2)
        return -1;
    if (flag <= 0)
    {
        clif_specialeffect(sd, type, flag);
        clif_displaymessage(fd, "Your effect has changed.");
        return 0;
    }
    for (MapSessionData *pl_sd : auth_sessions)
    {
        clif_specialeffect(pl_sd, type, flag);
        clif_displaymessage(pl_sd->fd, "Your effect has changed.");
    }

    return 0;
}

/// List someone's inventory
int atcommand_character_item_list(int fd, MapSessionData *sd,
                                   const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) >= pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;
    }
    int counter = 0;
    int count = 0;
    for (int i = 0; i < MAX_INVENTORY; i++)
    {
        if (!pl_sd->status.inventory[i].nameid)
            continue;
        struct item_data *item_data = itemdb_search(pl_sd->status.inventory[i].nameid);
        if (!item_data)
            continue;

        counter += pl_sd->status.inventory[i].amount;
        count++;
        char output[200];
        if (count == 1)
        {
            sprintf(output, "------ Items list of '%s' ------", pl_sd->status.name);
            clif_displaymessage(fd, output);
        }
        int equip = pl_sd->status.inventory[i].equip;

        char equipstr[100] = "";
        if (equip)
            strcpy(equipstr, "| equipped: ");
        if (equip & 4)
            strcat(equipstr, "robe/garment, ");
        if (equip & 8)
            strcat(equipstr, "left accessory, ");
        if (equip & 16)
            strcat(equipstr, "body/armor, ");
        if ((equip & 34) == 2)
            strcat(equipstr, "right hand, ");
        if ((equip & 34) == 32)
            strcat(equipstr, "left hand, ");
        if ((equip & 34) == 34)
            strcat(equipstr, "both hands, ");
        if (equip & 64)
            strcat(equipstr, "feet, ");
        if (equip & 128)
            strcat(equipstr, "right accessory, ");
        if ((equip & 769) == 1)
            strcat(equipstr, "lower head, ");
        if ((equip & 769) == 256)
            strcat(equipstr, "top head, ");
        if ((equip & 769) == 257)
            strcat(equipstr, "lower/top head, ");
        if ((equip & 769) == 512)
            strcat(equipstr, "mid head, ");
        if ((equip & 769) == 513)
            strcat(equipstr, "lower/mid head, ");
        if ((equip & 769) == 769)
            strcat(equipstr, "lower/mid/top head, ");
        // remove final ', '
        equipstr[strlen(equipstr) - 2] = '\0';

        sprintf(output, "%d %s (%s, id: %d) %s",
                pl_sd->status.inventory[i].amount,
                item_data->name, item_data->jname,
                pl_sd->status.inventory[i].nameid, equipstr);
        clif_displaymessage(fd, output);
    }

    if (count == 0)
        clif_displaymessage(fd, "No item found on this player.");
    else
    {
        char output[200];
        sprintf(output, "%d item(s) found in %d kind(s) of items.",
                counter, count);
        clif_displaymessage(fd, output);
    }

    return 0;
}

/// List someone's storage
int atcommand_character_storage_list(int fd, MapSessionData *sd,
                                  const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99[^\n]", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (pc_isGM(sd) < pc_isGM(pl_sd))
    {
        clif_displaymessage(fd, "Your GM level don't authorise you to do this action on this player.");
        return -1;
    }
    struct storage *stor = account2storage2(pl_sd->status.account_id);
    if (!stor)
    {
        clif_displaymessage(fd, "This player has no storage.");
        return -1;
    }

    int counter = 0;
    int count = 0;
    for (int i = 0; i < MAX_STORAGE; i++)
    {
        char output[200];

        if (!stor->storage_[i].nameid)
            continue;

        struct item_data *item_data = itemdb_search(stor->storage_[i].nameid);
        if (!item_data)
            continue;
        counter += stor->storage_[i].amount;
        count++;
        if (count == 1)
        {
            sprintf(output, "------ Storage items list of '%s' ------", pl_sd->status.name);
            clif_displaymessage(fd, output);
        }
        sprintf(output, "%d %s (%s, id: %d)", stor->storage_[i].amount,
                item_data->name, item_data->jname,
                stor->storage_[i].nameid);
        clif_displaymessage(fd, output);
    }
    if (count == 0)
        clif_displaymessage(fd, "No item found in the storage of this player.");
    else
    {
        char output[200];
        sprintf(output, "%d item(s) found in %d kind(s) of items.",
                counter, count);
        clif_displaymessage(fd, output);
    }

    return 0;
}

/// Toggle whether you can kill players out of PvP
// (there is no @charkiller command)
int atcommand_killer(int fd, MapSessionData *sd,
                      const char *, const char *)
{
    sd->special_state.killer = !sd->special_state.killer;

    if (sd->special_state.killer)
        clif_displaymessage(fd, "You be a killa...");
    else
        clif_displaymessage(fd, "You gonna be own3d");

    return 0;
}

/// Allow players to attack you out of PvP
int atcommand_killable(int fd, MapSessionData *sd,
                        const char *, const char *)
{
    sd->special_state.killable = !sd->special_state.killable;

    if (sd->special_state.killable)
        clif_displaymessage(fd, "You are killable");
    else
        clif_displaymessage(fd, "You are no longer killable");

    return 0;
}

/// Allow a player to be attacked outside of PvP
int atcommand_charkillable(int fd, MapSessionData *,
                            const char *, const char *message)
{
    if (!message || !*message)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(message);
    if (!pl_sd)
        return -1;

    pl_sd->special_state.killable = !pl_sd->special_state.killable;

    if (pl_sd->special_state.killable)
        clif_displaymessage(fd, "The player is now killable");
    else
        clif_displaymessage(fd, "The player is no longer killable");

    return 0;
}

/// Move an NPC
int atcommand_npcmove(int, MapSessionData *sd,
                       const char *, const char *message)
{
    if (!sd)
        return -1;

    if (!message || !*message)
        return -1;

    int x, y;
    char character[100];
    if (sscanf(message, "%d %d %99[^\n]", &x, &y, character) < 3)
        return -1;

    struct npc_data *nd = npc_name2id(character);
    if (!nd)
        return -1;

    npc_enable(character, 0);
    nd->x = x;
    nd->y = y;
    npc_enable(character, 1);

    return 0;
}

/// Create a new semipermanent warp (a type of NPC)
int atcommand_addwarp(int fd, MapSessionData *sd,
                       const char *, const char *message)
{
    if (!message || !*message)
        return -1;

    char map[30];
    int x, y;
    if (sscanf(message, "%30s %d %d[^\n]", map, &x, &y) < 3)
        return -1;

    char w1[64], w3[64], w4[64];
    sprintf(w1, "%15s,%d,%d", &sd->mapname, sd->x, sd->y);
    sprintf(w3, "%s%d%d%d%d", map, sd->x, sd->y, x, y);
    sprintf(w4, "1,1,%s.gat,%d,%d", map, x, y);

    int ret = npc_parse_warp(w1, "warp", w3, w4);

    char output[200];
    sprintf(output, "New warp NPC => %s", w3);

    clif_displaymessage(fd, output);

    return ret;
}

/// Apply a visual effect on a player (is this useful?)
int atcommand_chareffect(int fd, MapSessionData *,
                          const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int type = 0;
    char target[255];
    if (sscanf(message, "%d %255s", &type, target) != 2)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(target);
    if (!pl_sd)
        return -1;

    clif_specialeffect(pl_sd, type, 0);
    clif_displaymessage(fd, "EffectType changed.");

    return 0;
}

/// Put everything into storage to simplify your inventory. Intended as a debugging aid
int atcommand_storeall(int fd, MapSessionData *sd,
                        const char *, const char *)
{
    nullpo_retr(-1, sd);

    if (sd->state.storage_flag != 1)
    {
        if (storage_storageopen(sd) != 0)
        {
            clif_displaymessage(fd, "You can't open the storage currently.");
            return 1;
        }
    }
    for (int i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].amount)
        {
            if (sd->status.inventory[i].equip)
                pc_unequipitem(sd, i, false);
            storage_storageadd(sd, i, sd->status.inventory[i].amount);
        }
    }
    storage_storageclose(sd);

    clif_displaymessage(fd, "It is done");
    return 0;
}

/// lookup a skill by name
int atcommand_skillid(int fd, MapSessionData *,
                   const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    for (int idx = 0; skill_names[idx].id; idx++)
    {
        if ((strcasecmp(skill_names[idx].name, message) == 0) ||
            (strcasecmp(skill_names[idx].desc, message) == 0))
        {
            char output[255];
            sprintf(output, "skill %d: %s", skill_names[idx].id,
                     skill_names[idx].desc);
            clif_displaymessage(fd, output);
        }
    }
    return 0;
}

/// Summon monsters
int atcommand_summon(int, MapSessionData *sd,
                      const char *, const char *message)
{
    nullpo_retr(-1, sd);

    if (!message || !*message)
        return -1;
    char name[100];

    if (sscanf(message, "%99s", name) < 1)
        return -1;

    int mob_id = atoi(name);
    if (!mob_id)
        mob_id = mobdb_searchname(name);
    if (!mob_id)
        return -1;

    uint16_t x = sd->x + MPRAND(-5, 10);
    uint16_t y = sd->y + MPRAND(-5, 10);

    fixed_string<16> ths;
    ths.copy_from("this");
    int id = mob_once_spawn(sd, {ths, x, y}, "--ja--", mob_id, 1, "");
    struct mob_data *md = static_cast<struct mob_data *>(map_id2bl(id));
    if (md)
    {
        md->master_id = sd->id;
        md->state.special_mob_ai = 1;
        md->mode = mob_db[md->mob_class].mode | 0x04;
        md->deletetimer = add_timer(gettick() + 60000, mob_timer_delete, id);
        clif_misceffect(md, 344);
    }

    return 0;
}

/// Temporarily adjust the GM level required to use a GM command for testing
int atcommand_adjcmdlvl(int fd, MapSessionData *,
                         const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int newlev;
    char cmd[100];
    if (sscanf(message, "%d %99s", &newlev, cmd) != 2)
        return -1;

    for (int i = 0; i < ARRAY_SIZEOF(atcommand_info); i++)
        if (strcasecmp(cmd, atcommand_info[i].command + 1) == 0)
        {
            atcommand_info[i].level = newlev;
            clif_displaymessage(fd, "@command level changed.");
            return 0;
        }

    clif_displaymessage(fd, "@command not found.");
    return -1;
}

/// Temporarily grant GM powers (for testing)
int atcommand_adjgmlvl(int, MapSessionData *,
                        const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int newlev;
    char user[100];
    if (sscanf(message, "%d %99s", &newlev, user) != 2)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(user);
    if (!pl_sd)
        return -1;

    pc_set_gm_level(pl_sd->status.account_id, newlev);

    return 0;
}

/// Open a trade window with a player without being on the same map
int atcommand_trade(int, MapSessionData *sd,
                     const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    MapSessionData *pl_sd = map_nick2sd(message);
    if (!pl_sd)
        return -1;
    trade_traderequest(sd, pl_sd->id);
    return 0;
}

// TMW Magic atcommands by Fate

static int magic_base = TMW_MAGIC;
static const char *magic_skill_names[] =
{
    "magic",
    "life",
    "war",
    "transmute",
    "nature",
    "astral" };

/// Display magic info for a character
int atcommand_magic_info(int fd, MapSessionData *,
                          const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99s", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    char buf[200];
    sprintf(buf, "`%s' has the following magic skills:", character);
    clif_displaymessage(fd, buf);

    for (int i = 0; i < ARRAY_SIZEOF(magic_skill_names); i++)
    {
        sprintf(buf, "%d in %s", pl_sd->status.skill[i + magic_base].lv,
                 magic_skill_names[i]);
        if (pl_sd->status.skill[i + magic_base].id == i + magic_base)
            clif_displaymessage(fd, buf);
    }

    return 0;
}

static void set_skill(MapSessionData *sd, int i, int level)
{
    sd->status.skill[i].id = level ? i : 0;
    sd->status.skill[i].lv = level;
}

/// Grant somebody some magic skills
int atcommand_set_magic(int fd, MapSessionData *,
                         const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char magic_type[20];
    int value;
    char character[100];
    if (sscanf(message, "%19s %i %99s", magic_type, &value, character) < 3)
        return -1;

    int skill_index = -1;
    if (strcasecmp("all", magic_type) == 0)
        skill_index = 0;
    for (int i = 0; skill_index == -1 && i < ARRAY_SIZEOF(magic_skill_names); i++)
        if (strcasecmp(magic_skill_names[i], magic_type) == 0)
            skill_index = i + magic_base;

    if (skill_index == -1)
    {
        clif_displaymessage(fd, "No such school of magic.");
        return -1;
    }

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (skill_index == 0)
        for (int i = 0; i < ARRAY_SIZEOF(magic_skill_names); i++)
            set_skill(pl_sd, i + magic_base, value);
    else
        set_skill(pl_sd, skill_index, value);

    clif_skillinfoblock(pl_sd);
    return 0;
}

/// @commands are logged anyway, this function doesn't need to do anything
int atcommand_log(int, MapSessionData *,
                   const char *, const char *)
{
    return 0;
}

/// say something in chat, that is also recorded in the GM log
int atcommand_tee(int, MapSessionData *sd,
                   const char *, const char *message)
{
    clif_message(sd, message);
    return 0;
}

/// Become completely undetectable to players
int atcommand_invisible(int, MapSessionData *sd,
                         const char *, const char *)
{
    pc_invisibility(sd, 1);
    return 0;
}

/// Become detectable to players again
int atcommand_visible(int, MapSessionData *sd,
                       const char *, const char *)
{
    pc_invisibility(sd, 0);
    return 0;
}

/// Implementation for the player iterators
static int atcommand_jump_iterate(int fd, MapSessionData *sd,
                                  MapSessionData *(*get_start)(void),
                                  MapSessionData *(*get_next)(MapSessionData* current))
{
    char output[200];

    memset(output, '\0', sizeof(output));

    MapSessionData *pl_sd = map_id2sd(sd->followtarget);

    if (pl_sd)
        pl_sd = get_next(pl_sd);

    if (pl_sd == sd)
        pl_sd = get_next(pl_sd);
    if (pl_sd == sd)
        clif_displaymessage(fd, "No other players");
    if (!pl_sd)
    {
        pl_sd = get_start();
        clif_displaymessage(fd, "Reached end of players, wrapped");
    }
    if (maps[pl_sd->m].flag.nowarpto
        && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp you to the map of this player.");
        return -1;
    }
    if (maps[sd->m].flag.nowarp
        && battle_config.any_warp_GM_min_level > pc_isGM(sd))
    {
        clif_displaymessage(fd, "You are not authorised to warp you from your actual map.");
        return -1;
    }
    pc_setpos(sd, Point{maps[pl_sd->m].name, pl_sd->x, pl_sd->y}, BeingRemoveType::WARP);
    sprintf(output, "Jump to %s", pl_sd->status.name);
    clif_displaymessage(fd, output);

    sd->followtarget = pl_sd->id;

    return 0;
}

/// Warp to next player
int atcommand_iterate_forward_over_players(int fd, MapSessionData *sd,
                                            const char *, const char *)
{
    return atcommand_jump_iterate(fd, sd, map_get_first_session, map_get_next_session);
}

/// Warp to previous player
int atcommand_iterate_backwards_over_players(int fd, MapSessionData *sd,
                                              const char *, const char *)
{
    return atcommand_jump_iterate(fd, sd, map_get_last_session, map_get_prev_session);
}

/// Report something to all GMs
int atcommand_wgm(int fd, MapSessionData *sd,
                   const char *, const char *message)
{
    tmw_GmHackMsg("%s: %s", sd->status.name, message);
    if (!pc_isGM(sd))
        clif_displaymessage(fd, "Message sent.");

    return 0;
}

/// Display a character's skill pool use
int atcommand_skillpool_info(int fd, MapSessionData *,
                              const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    char character[100];
    if (sscanf(message, "%99s", character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    char buf[200];
    int pool_skills[MAX_SKILL_POOL];
    int pool_skills_nr = skill_pool(pl_sd, pool_skills);

    sprintf(buf, "Active skills %d out of %d for %s:", pool_skills_nr,
             skill_pool_max(pl_sd), character);
    clif_displaymessage(fd, buf);
    for (int i = 0; i < pool_skills_nr; ++i)
    {
        sprintf(buf, " - %s [%d]: power %d", skill_name(pool_skills[i]),
                 pool_skills[i], skill_power(pl_sd, pool_skills[i]));
        clif_displaymessage(fd, buf);
    }

    sprintf(buf, "Learned skills out of %d for %s:",
             skill_pool_skills_size, character);
    clif_displaymessage(fd, buf);

    for (int i = 0; i < skill_pool_skills_size; ++i)
    {
        const char *name = skill_name(skill_pool_skills[i]);
        int lvl = pl_sd->status.skill[skill_pool_skills[i]].lv;
        if (!lvl)
            continue;
        sprintf(buf, " - %s [%d]: lvl %d", name, skill_pool_skills[i], lvl);
        clif_displaymessage(fd, buf);
    }
    return 0;
}

/// Focus somebody on a skill
int atcommand_skillpool_focus(int fd, MapSessionData *,
                               const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int skill;
    char character[100];
    if (sscanf(message, "%d %99[^\n]", &skill, character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (skill_pool_activate(pl_sd, skill))
        clif_displaymessage(fd, "Activation failed.");
    else
        clif_displaymessage(fd, "Activation successful.");

    return 0;
}

/// Unfocus somebody's skill
int atcommand_skillpool_unfocus(int fd, MapSessionData *,
                                 const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int skill;
    char character[100];
    if (sscanf(message, "%d %99[^\n]", &skill, character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    if (skill_pool_deactivate(pl_sd, skill))
        clif_displaymessage(fd, "Deactivation failed.");
    else
        clif_displaymessage(fd, "Deactivation successful.");

    return 0;
}

/// Learn a skill
int atcommand_skill_learn(int fd, MapSessionData *,
                           const char *, const char *message)
{
    if (!message || !*message)
        return -1;
    int skill, level;
    char character[100];
    if (sscanf(message, "%d %d %99[^\n]", &skill, &level, character) < 1)
        return -1;

    MapSessionData *pl_sd = map_nick2sd(character);
    if (!pl_sd)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }
    set_skill(pl_sd, skill, level);
    clif_skillinfoblock(pl_sd);

    return 0;
}

/// Check what players are from the same IP
int atcommand_ipcheck(int fd, MapSessionData *,
                       const char *, const char *message)
{
    char character[25];

    if (sscanf(message, "%24[^\n]", character) < 1)
        return -1;

    MapSessionData *sd1 = map_nick2sd(character);
    if (!sd1)
    {
        clif_displaymessage(fd, "Character not found.");
        return -1;
    }

    IP_Address ip = session[sd1->fd]->client_addr;

    for (MapSessionData *pl_sd : auth_sessions)
    {
        if (ip != session[pl_sd->fd]->client_addr)
            continue;

        char output[200];
        snprintf(output, sizeof(output), "Name: %s | Location: %s %d %d",
                  pl_sd->status.name, &pl_sd->mapname, pl_sd->x, pl_sd->y);
        clif_displaymessage(fd, output);
    }

    clif_displaymessage(fd, "End of list");
    return 0;
}
