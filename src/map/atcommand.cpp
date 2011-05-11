#include "atcommand.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "../common/socket.hpp"
#include "../common/timer.hpp"
#include "../common/nullpo.hpp"
#include "../common/mt_rand.hpp"

#include "battle.hpp"
#include "clif.hpp"
#include "chrif.hpp"
#include "intif.hpp"
#include "itemdb.hpp"
#include "map.hpp"
#include "mob.hpp"
#include "npc.hpp"
#include "pc.hpp"
#include "party.hpp"
#include "script.hpp"
#include "skill.hpp"
#include "trade.hpp"

#include "../common/core.hpp"
#include "tmw.hpp"

#define STATE_BLIND 0x10

#define ATCOMMAND_FUNC(x) static int atcommand_ ## x (const int fd, struct map_session_data* sd, const char* command, const char* message)
ATCOMMAND_FUNC (setup);
ATCOMMAND_FUNC (broadcast);
ATCOMMAND_FUNC (localbroadcast);
ATCOMMAND_FUNC (charwarp);
// ATCOMMAND_FUNC (warp);
ATCOMMAND_FUNC (where);
// ATCOMMAND_FUNC (goto);
ATCOMMAND_FUNC (jump);
ATCOMMAND_FUNC (who);
ATCOMMAND_FUNC (whogroup);
ATCOMMAND_FUNC (whomap);
ATCOMMAND_FUNC (whomapgroup);
ATCOMMAND_FUNC (whogm);         // by Yor
ATCOMMAND_FUNC (save);
ATCOMMAND_FUNC (load);
ATCOMMAND_FUNC (speed);
ATCOMMAND_FUNC (storage);
ATCOMMAND_FUNC (option);
ATCOMMAND_FUNC (hide);
ATCOMMAND_FUNC (die);
ATCOMMAND_FUNC (kill);
ATCOMMAND_FUNC (alive);
ATCOMMAND_FUNC (kami);
ATCOMMAND_FUNC (heal);
// ATCOMMAND_FUNC (item);
ATCOMMAND_FUNC (itemreset);
ATCOMMAND_FUNC (itemcheck);
ATCOMMAND_FUNC (baselevelup);
ATCOMMAND_FUNC (joblevelup);
ATCOMMAND_FUNC (help);
ATCOMMAND_FUNC (gm);
ATCOMMAND_FUNC (pvpoff);
ATCOMMAND_FUNC (pvpon);
ATCOMMAND_FUNC (model);
ATCOMMAND_FUNC (go);
// ATCOMMAND_FUNC (spawn);
ATCOMMAND_FUNC (killmonster);
ATCOMMAND_FUNC (killmonster2);
ATCOMMAND_FUNC (produce);
ATCOMMAND_FUNC (memo);
ATCOMMAND_FUNC (gat);
ATCOMMAND_FUNC (packet);
ATCOMMAND_FUNC (statuspoint);
ATCOMMAND_FUNC (skillpoint);
ATCOMMAND_FUNC (zeny);
ATCOMMAND_FUNC (param);
// ATCOMMAND_FUNC (recall);
ATCOMMAND_FUNC (recallall);
ATCOMMAND_FUNC (revive);
ATCOMMAND_FUNC (character_stats);
ATCOMMAND_FUNC (character_stats_all);
ATCOMMAND_FUNC (character_option);
ATCOMMAND_FUNC (character_save);
ATCOMMAND_FUNC (night);
ATCOMMAND_FUNC (day);
ATCOMMAND_FUNC (doom);
ATCOMMAND_FUNC (doommap);
ATCOMMAND_FUNC (raise);
ATCOMMAND_FUNC (raisemap);
ATCOMMAND_FUNC (character_baselevel);
ATCOMMAND_FUNC (character_joblevel);
ATCOMMAND_FUNC (kick);
ATCOMMAND_FUNC (kickall);
ATCOMMAND_FUNC (allskills);
ATCOMMAND_FUNC (questskill);
ATCOMMAND_FUNC (charquestskill);
ATCOMMAND_FUNC (lostskill);
ATCOMMAND_FUNC (charlostskill);
ATCOMMAND_FUNC (party);
ATCOMMAND_FUNC (charskreset);
ATCOMMAND_FUNC (charstreset);
ATCOMMAND_FUNC (charreset);
ATCOMMAND_FUNC (charstpoint);
ATCOMMAND_FUNC (charmodel);
ATCOMMAND_FUNC (charskpoint);
ATCOMMAND_FUNC (charzeny);
ATCOMMAND_FUNC (reloaditemdb);
ATCOMMAND_FUNC (reloadmobdb);
ATCOMMAND_FUNC (reloadskilldb);
ATCOMMAND_FUNC (reloadscript);
ATCOMMAND_FUNC (reloadgmdb);    // by Yor
ATCOMMAND_FUNC (mapexit);
ATCOMMAND_FUNC (idsearch);
ATCOMMAND_FUNC (mapinfo);
ATCOMMAND_FUNC (dye);           //** by fritz
ATCOMMAND_FUNC (hair_style);    //** by fritz
ATCOMMAND_FUNC (hair_color);    //** by fritz
ATCOMMAND_FUNC (all_stats);     //** by fritz
ATCOMMAND_FUNC (char_change_sex);   // by Yor
ATCOMMAND_FUNC (char_block);    // by Yor
ATCOMMAND_FUNC (char_ban);      // by Yor
ATCOMMAND_FUNC (char_unblock);  // by Yor
ATCOMMAND_FUNC (char_unban);    // by Yor
ATCOMMAND_FUNC (mount_peco);    // by Valaris
ATCOMMAND_FUNC (char_mount_peco);   // by Yor
ATCOMMAND_FUNC (partyspy);      // [Syrus22]
ATCOMMAND_FUNC (partyrecall);   // by Yor
ATCOMMAND_FUNC (enablenpc);
ATCOMMAND_FUNC (disablenpc);
ATCOMMAND_FUNC (servertime);    // by Yor
ATCOMMAND_FUNC (chardelitem);   // by Yor
ATCOMMAND_FUNC (jail);          // by Yor
ATCOMMAND_FUNC (unjail);        // by Yor
ATCOMMAND_FUNC (disguise);      // [Valaris]
ATCOMMAND_FUNC (undisguise);    // by Yor
ATCOMMAND_FUNC (ignorelist);    // by Yor
ATCOMMAND_FUNC (charignorelist);    // by Yor
ATCOMMAND_FUNC (inall);         // by Yor
ATCOMMAND_FUNC (exall);         // by Yor
ATCOMMAND_FUNC (chardisguise);  // Kalaspuff
ATCOMMAND_FUNC (charundisguise);    // Kalaspuff
ATCOMMAND_FUNC (email);         // by Yor
ATCOMMAND_FUNC (effect);        //by Apple
ATCOMMAND_FUNC (character_item_list);   // by Yor
ATCOMMAND_FUNC (character_storage_list);    // by Yor
ATCOMMAND_FUNC (character_cart_list);   // by Yor
ATCOMMAND_FUNC (addwarp);       // by MouseJstr
ATCOMMAND_FUNC (follow);        // by MouseJstr
ATCOMMAND_FUNC (skillon);       // by MouseJstr
ATCOMMAND_FUNC (skilloff);      // by MouseJstr
ATCOMMAND_FUNC (killer);        // by MouseJstr
ATCOMMAND_FUNC (npcmove);       // by MouseJstr
ATCOMMAND_FUNC (killable);      // by MouseJstr
ATCOMMAND_FUNC (charkillable);  // by MouseJstr
ATCOMMAND_FUNC (chareffect);    // by MouseJstr
ATCOMMAND_FUNC (dropall);       // by MouseJstr
ATCOMMAND_FUNC (chardropall);   // by MouseJstr
ATCOMMAND_FUNC (storeall);      // by MouseJstr
ATCOMMAND_FUNC (charstoreall);  // by MouseJstr
ATCOMMAND_FUNC (skillid);       // by MouseJstr
ATCOMMAND_FUNC (useskill);      // by MouseJstr
ATCOMMAND_FUNC (summon);
ATCOMMAND_FUNC (rain);
ATCOMMAND_FUNC (snow);
ATCOMMAND_FUNC (sakura);
ATCOMMAND_FUNC (fog);
ATCOMMAND_FUNC (leaves);
ATCOMMAND_FUNC (adjgmlvl);      // by MouseJstr
ATCOMMAND_FUNC (adjcmdlvl);     // by MouseJstr
ATCOMMAND_FUNC (trade);         // by MouseJstr
ATCOMMAND_FUNC (unmute);        // [Valaris]
ATCOMMAND_FUNC (char_wipe);     // [Fate]
ATCOMMAND_FUNC (set_magic);     // [Fate]
ATCOMMAND_FUNC (magic_info);    // [Fate]
ATCOMMAND_FUNC (log);           // [Fate]
ATCOMMAND_FUNC (tee);           // [Fate]
ATCOMMAND_FUNC (invisible);     // [Fate]
ATCOMMAND_FUNC (visible);       // [Fate]
ATCOMMAND_FUNC (list_nearby);   // [Fate]
ATCOMMAND_FUNC (iterate_forward_over_players);  // [Fate]
ATCOMMAND_FUNC (iterate_backwards_over_players);    // [Fate]
ATCOMMAND_FUNC (skillpool_info);    // [Fate]
ATCOMMAND_FUNC (skillpool_focus);   // [Fate]
ATCOMMAND_FUNC (skillpool_unfocus); // [Fate]
ATCOMMAND_FUNC (skill_learn);   // [Fate]
ATCOMMAND_FUNC (wgm);
ATCOMMAND_FUNC (ipcheck);

/*==========================================
 *AtCommandInfo atcommand_info[]構造体の定義
 *------------------------------------------
 */

// First char of commands is configured in atcommand_athena.conf. Leave @ in this list for default value.
// to set default level, read atcommand_athena.conf first please.
static AtCommandInfo atcommand_info[] = {
    {"@help", 0,        atcommand_help,         ATCC_MISC,
    "[@cmd | cat]",     "Get help about @commands"},
    {"@setup", 40,      atcommand_setup,        ATCC_UNK,
    "",                 "??"},
    {"@charwarp", 60,   atcommand_charwarp,     ATCC_CHAR,
    "map x y charname", "Warp a char to a location"},
    {"@warp", 40,       atcommand_warp,         ATCC_SELF,
    "map x y",          "Warp yourself to a location"},
    {"@where", 1,       atcommand_where,        ATCC_INFO,
    "[charname]",       "Tells you the location of a character"},
    {"@goto", 20,       atcommand_goto,         ATCC_SELF,
    "charname",         "Warp you to a character"},
    {"@jump", 40,       atcommand_jump,         ATCC_SELF,
    "[x [y]]",          "Warp within a map (randomly if args omitted)"},
    {"@who", 20,        atcommand_who,          ATCC_INFO,
    "[substring]",      "List online characters with location"},
    {"@whogroup", 20,   atcommand_whogroup,     ATCC_INFO,
    "[substring]",      "List online characters with party"},
    {"@whomap", 20,     atcommand_whomap,       ATCC_INFO,
    "[map]",            "List characters on a map with location"},
    {"@whomapgroup", 20, atcommand_whomapgroup, ATCC_INFO,
    "[map]",            "List characters on a map with party"},
    {"@whogm", 20,      atcommand_whogm,        ATCC_INFO,
    "[substring]",      "List online GMs"},
    {"@save", 40,       atcommand_save,         ATCC_SELF,
    "",                 "Set your respawn point at your current location"},
    {"@return", 40,     atcommand_load,         ATCC_SELF,
    "",                 "Warp to your respawn point"},
    {"@load", 40,       atcommand_load,         ATCC_SELF,
    "",                 "Warp to your respawn point"},
    {"@speed", 40,      atcommand_speed,        ATCC_SELF,
    "[1-1000]",         "Set your walk delay (milliseconds). Default 150"},
    {"@storage", 1,     atcommand_storage,      ATCC_ITEM,
    "",                 "Open your storage"},
    {"@option", 40,     atcommand_option,       ATCC_SELF,
    "param1 p2 p3",
    "    <param1>      <param2>      <p3>(stackable)   <param3>               <param3>\n"
    "    1 Petrified   (stackable)   01 Sight           32 Peco Peco riding   2048 Orc Head\n"
    "    2 Frozen      01 Poison     02 Hide            64 GM Perfect Hide    4096 Wedding Sprites\n"
    "    3 Stunned     02 Cursed     04 Cloak          128 Level 2 Cart       8192 Ruwach\n"
    "    4 Sleeping    04 Silenced   08 Level 1 Cart   256 Level 3 Cart\n"
    "    6 darkness    08 ???        16 Falcon         512 Level 4 Cart\n"
    "                  16 darkness                    1024 Level 5 Cart"},
    {"@hide", 40,       atcommand_hide,         ATCC_SELF,
    "",                 "toggle invisibility to monsters and scripts"},
    {"@die", 1,         atcommand_die,          ATCC_SELF,
    "",                 "suicide"},
    {"@kill", 60,       atcommand_kill,         ATCC_CHAR,
    "charname",         "kill a player"},
    {"@alive", 60,      atcommand_alive,        ATCC_SELF,
    "",                 "resurrect yourself"},
    {"@kami", 40,       atcommand_kami,         ATCC_MSG,
    "message",          "broadcast a message without GM name"},
    {"@heal", 40,       atcommand_heal,         ATCC_SELF,
    "[hp [sp]]",        "restore your HP/SP by the amount, or all"},
    {"@item", 60,       atcommand_item,         ATCC_ITEM,
    "name|ID qty",      "Add items to your inventory"},
    {"@itemreset", 40,  atcommand_itemreset,    ATCC_ITEM,
    "",                 "remove all items from your inventory"},
    {"@itemcheck", 60,  atcommand_itemcheck,    ATCC_ITEM,
    "",                 "check authorization of your inventory"},
    {"@blvl", 60,       atcommand_baselevelup,  ATCC_SELF,
    "count",            "increase your base level"},
    {"@jlvl", 60,       atcommand_joblevelup,   ATCC_SELF,
    "count",            "increase your job level"},
    {"@gm", 100,        atcommand_gm,           ATCC_ADMIN,
    "password",         "become a GM"},
    {"@pvpoff", 40,     atcommand_pvpoff,       ATCC_GROUP,
    "",                 "Disable PvP on current map"},
    {"@pvpon", 40,      atcommand_pvpon,        ATCC_GROUP,
    "",                 "Enable PvP on current map"},
    {"@model", 20,      atcommand_model,        ATCC_SELF,
    "hairstyle haircolor clothescolor",
                        "Change your appearance"},
    {"@go", 10,         atcommand_go,           ATCC_SELF,
    "number|name",      "warp to a city or memo point. WARNING does not have TMW maps!"},
    {"@spawn", 50,      atcommand_spawn,        ATCC_MOB,
    "name|ID [count [x [y]]]",
                        "spawn monsters"},
    {"@killmonster", 60, atcommand_killmonster, ATCC_MOB,
    "[map]",            "Kill all monsters, with drops"},
    {"@killmonster2", 40, atcommand_killmonster2, ATCC_MOB,
    "",                 "Kill all monsters, without drops"},
    {"@produce", 60,    atcommand_produce,      ATCC_ITEM,
    "name|ID element strength",
                        "manufacture an item (probably doesn't work)"},
    {"@memo", 40,       atcommand_memo,         ATCC_SELF,
    "[pos]",            "set a memo point (no arg: list points)"},
    {"@gat", 99,        atcommand_gat,          ATCC_ADMIN,
    "",                 "Display collision info of the map"},
    {"@packet", 99,     atcommand_packet,       ATCC_ADMIN,
    "",                 "Display packet info?"},
    {"@stpoint", 60,    atcommand_statuspoint,  ATCC_SELF,
    "count",            "Give yourself status points"},
    {"@skpoint", 60,    atcommand_skillpoint,   ATCC_SELF,
    "count",            "Give yourself skill points"},
    {"@zeny", 60,       atcommand_zeny,         ATCC_SELF,
    "count",            "Give yourself some gold"},
    {"@str", 60,        atcommand_param,        ATCC_SELF,
    "count",            "Increase your strength"},
    {"@agi", 60,        atcommand_param,        ATCC_SELF,
    "count",            "Increase your agility"},
    {"@vit", 60,        atcommand_param,        ATCC_SELF,
    "count",            "Increase your vitality"},
    {"@int", 60,        atcommand_param,        ATCC_SELF,
    "count",            "Increase your intelligence"},
    {"@dex", 60,        atcommand_param,        ATCC_SELF,
    "count",            "Increase your dexterity"},
    {"@luk", 60,        atcommand_param,        ATCC_SELF,
    "count",            "Increase your luck"},
    {"@recall", 60,     atcommand_recall,       ATCC_CHAR,
    "charname",         "warp a player to you"},
    {"@revive", 60,     atcommand_revive,       ATCC_CHAR,
    "charname",         "resurrent someone else"},
    {"@charstats", 40,  atcommand_character_stats, ATCC_CHAR,
    "charname",         "display stats of a character"},
    {"@charstatsall", 40, atcommand_character_stats_all, ATCC_CHAR,
    "",                 "display stats of all characters"},
    {"@charoption", 60, atcommand_character_option, ATCC_CHAR,
    "param1 param2 param3 charname",
                        "set display options of a character"},
    {"@charsave", 60,   atcommand_character_save, ATCC_CHAR,
    "map x y charname", "changes somebody's respawn point"},
    {"@night", 80,      atcommand_night,        ATCC_ENV,
    "",                 "sets all players the darkness option"},
    {"@day", 80,        atcommand_day,          ATCC_ENV,
    "",                 "unsets all players the darkness option"},
    {"@doom", 80,       atcommand_doom,         ATCC_CHAR,
    "",                 "kill all non-GM players on the server"},
    {"@doommap", 80,    atcommand_doommap,      ATCC_CHAR,
    "",                 "kill all non-GM players on the map"},
    {"@raise", 80,      atcommand_raise,        ATCC_CHAR,
    "",                 "resurrect all players on the server"},
    {"@raisemap", 80,   atcommand_raisemap,     ATCC_CHAR,
    "",                 "resurrect all players on the map"},
    {"@charbaselvl", 60, atcommand_character_baselevel, ATCC_CHAR,
    "num charname",     "increase a players's level"},
    {"@charjlvl", 60,   atcommand_character_joblevel, ATCC_CHAR,
    "num charname",     "increase a player's job level"},
    {"@kick", 20,       atcommand_kick,         ATCC_CHAR,
    "charname",         "temporarily disconnect a player"},
    {"@kickall", 99,    atcommand_kickall,      ATCC_CHAR,
    "",                 "temporarily disconnect all players"},
    {"@allskills", 60,  atcommand_allskills,    ATCC_SELF,
    "",                 "give you all skills"},
    {"@questskill", 40, atcommand_questskill,   ATCC_SELF,
    "num",              "give you specified skill"},
    {"@charquestskill", 60, atcommand_charquestskill, ATCC_CHAR,
    "num charname",     "give a player a skill"},
    {"@lostskill", 40,  atcommand_lostskill,    ATCC_SELF,
    "num",              "take away a skill"},
    {"@charlostskill", 60, atcommand_charlostskill, ATCC_CHAR,
    "num charname",     "take away a skill from a player"},
    {"@party", 1,       atcommand_party,        ATCC_GROUP,
    "name",             "create a party"},
    {"@mapexit", 99,    atcommand_mapexit,      ATCC_ADMIN,
    "",                 "shutdown the map server"},
    {"@idsearch", 60,   atcommand_idsearch,     ATCC_ITEM,
    "name",             "list items by substring"},
    {"@broadcast", 40,  atcommand_broadcast,    ATCC_MSG,
    "message",          "do an announcement to all servers"},
    {"@localbroadcast", 40, atcommand_localbroadcast, ATCC_MSG,
    "message",          "do an announcement to current server"},
    {"@recallall", 80,  atcommand_recallall,    ATCC_CHAR,
    "",                 "warp all players to you"},
    {"@charskreset", 60, atcommand_charskreset, ATCC_CHAR,
    "charname",         "reset skills of a player"},
    {"@charstreset", 60, atcommand_charstreset, ATCC_CHAR,
    "charname",         "reset stats of a player"},
    {"@reloaditemdb", 99, atcommand_reloaditemdb, ATCC_ADMIN,
    "",                 "reload items (might cause problems)"},
    {"@reloadmobdb", 99, atcommand_reloadmobdb, ATCC_ADMIN,
    "",                 "reload mobs (might cause problems)"},
    {"@reloadskilldb", 99, atcommand_reloadskilldb, ATCC_ADMIN,
    "",                 "reload skills (might cause problems)"},
    {"@reloadscript", 99, atcommand_reloadscript, ATCC_ADMIN,
    "",                 "reload scripts (likely to cause problems)"},
    {"@reloadgmdb", 99, atcommand_reloadgmdb,   ATCC_ADMIN,
    "",                 "reload GMs (this shouldn't be needed)"},
    {"@charreset", 60,  atcommand_charreset,    ATCC_CHAR,
    "charname",         "reset stats and skills of a player"},
    {"@charmodel", 50,  atcommand_charmodel,    ATCC_CHAR,
    "hairstyle haircolor clothescolor charname",
                        "change appearance of someone else"},
    {"@charskpoint", 60, atcommand_charskpoint, ATCC_CHAR,
    "num charname",     "give player some skill points"},
    {"@charstpoint", 60, atcommand_charstpoint, ATCC_CHAR,
    "num charname",     "give player some stat points"},
    {"@charzeny", 60,   atcommand_charzeny,     ATCC_CHAR,
    "amount charname",  "give player some gold"},
    {"@mapinfo", 99,    atcommand_mapinfo,      ATCC_INFO,
    "[type [map]]",     "information about a map. type 1 add players, type 2 add NPCs, type 3 add shops/chat"},
    {"@dye", 40,        atcommand_dye,          ATCC_SELF,
    "",                 "change your appearance (unimplemented)"},
    {"@ccolor", 40,     atcommand_dye,          ATCC_SELF,
    "",                 "change your appearance (unimplemented)"},
    {"@hairstyle", 40,  atcommand_hair_style,   ATCC_SELF,
    "",                 "change your hairstyle"},
    {"@haircolor", 40,  atcommand_hair_color,   ATCC_SELF,
    "",                 "change your haircolor"},
    {"@allstats", 60,   atcommand_all_stats,    ATCC_SELF,
    "[num]",            "increase all stats by num, or to maximum"},
    {"@charchangesex", 60, atcommand_char_change_sex, ATCC_CHAR,
    "name",             "toggle someone's gender"},
    {"@block", 60,      atcommand_char_block,   ATCC_CHAR,
    "charname",         "permanently block an account"},
    {"@unblock", 60,    atcommand_char_unblock, ATCC_CHAR,
    "charname",         "remove a permanent block"},
    {"@ban", 60,        atcommand_char_ban,     ATCC_CHAR,
    "time charname",    "temporary ban for +-#y|m|d|h|mn|s"},
    {"@unban", 60,      atcommand_char_unban,   ATCC_CHAR,
    "charname",         "remove a temporary ban"},
    {"@mountpeco", 20,  atcommand_mount_peco,   ATCC_SELF,
    "",                 "probably doesn't work"},
    {"@charmountpeco", 50, atcommand_char_mount_peco, ATCC_CHAR,
    "charname",         "probably doesn't work"},
    {"@partyspy", 60,   atcommand_partyspy,     ATCC_GROUP,
    "partyname",        "listen in on a party's chat"},
    {"@partyrecall", 60, atcommand_partyrecall, ATCC_GROUP,
    "partyname",        "warp all members of a party to you"},
    {"@enablenpc", 80,  atcommand_enablenpc,    ATCC_ADMIN,
    "npcname",          "Enable an NPC"},
    {"@disablenpc", 80, atcommand_disablenpc,   ATCC_ADMIN,
    "npcname",          "Disable an NPC"},
    {"@servertime", 0,  atcommand_servertime,   ATCC_INFO,
    "",                 "get the time of the server (usually UTC)"},
    {"@chardelitem", 60, atcommand_chardelitem, ATCC_CHAR,
    "name|ID qty charname",
                        "remove items from player's inventory"},
    {"@listnearby", 40, atcommand_list_nearby,  ATCC_UNK,
    "",                 "??"},
    {"@jail", 60,       atcommand_jail,         ATCC_CHAR,
    "charname",         "put player in prison"},
    {"@unjail", 60,     atcommand_unjail,       ATCC_CHAR,
    "charname",         "release a player from prison"},
    {"@disguise", 20,   atcommand_disguise,     ATCC_SELF,
    "name|ID",          "look like a monster"},
    {"@undisguise", 20, atcommand_undisguise,   ATCC_SELF,
    "",                 "look like yourself"},
    {"@ignorelist", 0,  atcommand_ignorelist,   ATCC_SELF,
    "",                 "display your ignore list"},
    {"@charignorelist", 20, atcommand_charignorelist, ATCC_CHAR,
    "charname",         "display someone's ignore list"},
    {"@inall", 20,      atcommand_inall,        ATCC_CHAR,
    "charname",         "allow all whispers to a player"},
    {"@exall", 20,      atcommand_exall,        ATCC_CHAR,
    "charname",         "deny all whispers to a player"},
    {"@chardisguise", 60, atcommand_chardisguise, ATCC_CHAR,
    "name|ID charname", "make a player look like a monster"},
    {"@charundisguise", 60, atcommand_charundisguise, ATCC_CHAR,
    "charname",         "make a player look like him/her self"},
    {"@email", 0,       atcommand_email,        ATCC_MISC,
    "old@e.mail new@e.mail",
                        "change stored email"},
    {"@effect", 40,     atcommand_effect,       ATCC_SELF,
    "ID [flag]",        "give an effect to your character"},
    {"@charitemlist", 40, atcommand_character_item_list, ATCC_CHAR,
    "charname",         "list cart of a player"},
    {"@charstoragelist", 40, atcommand_character_storage_list, ATCC_CHAR,
    "charname",         "list cart of a player"},
    {"@charcartlist", 40, atcommand_character_cart_list, ATCC_CHAR,
    "charname",         "list cart of a player"},
    {"@follow", 10,     atcommand_follow,       ATCC_SELF,
    "charname",         "automatically follow somebody"},
    {"@addwarp", 20,    atcommand_addwarp,      ATCC_ENV,
    "map x y",          "create a semipermant warp from your current location"},
    {"@skillon", 20,    atcommand_skillon,      ATCC_ENV,
    "",                 "turn on skills for a map"},
    {"@skilloff", 20,   atcommand_skilloff,     ATCC_ENV,
    "",                 "turn on skills for a map"},
    {"@killer", 60,     atcommand_killer,       ATCC_SELF,
    "",                 "allow you to kill attack players outside PvP"},
    {"@npcmove", 20,    atcommand_npcmove,      ATCC_ADMIN,
    "",                 "??"},
    {"@killable", 40,   atcommand_killable,     ATCC_SELF,
    "",                 "make yourself vulnerable outside PvP"},
    {"@charkillable", 40, atcommand_charkillable, ATCC_CHAR,
    "charname",         "make someone vulnerable outside PvP"},
    {"@chareffect", 40, atcommand_chareffect,   ATCC_CHAR,
    "",                 "??"},
    {"@dropall", 40,    atcommand_dropall,      ATCC_SELF,
    "",                 "put all your inventory on the ground"},
    {"@chardropall", 40, atcommand_chardropall, ATCC_CHAR,
    "charname",         "put someone's inventory on the ground"},
    {"@storeall", 40,   atcommand_storeall,     ATCC_SELF,
    "",                 "put all your inventory in storage"},
    {"@charstoreall", 40, atcommand_charstoreall, ATCC_CHAR,
    "charname",         "put someone's inventory in storage"},
    {"@skillid", 40,    atcommand_skillid,      ATCC_INFO,
    "name",             "get ID of a skill"},
    {"@useskill", 40,   atcommand_useskill,     ATCC_SELF,
    "ID lvl target",    "apply a skill"},
    {"@rain", 99,       atcommand_rain,         ATCC_ENV,
    "",                 "??"},
    {"@snow", 99,       atcommand_snow,         ATCC_ENV,
    "",                 "??"},
    {"@sakura", 99,     atcommand_sakura,       ATCC_ENV,
    "",                 "??"},
    {"@fog", 99,        atcommand_fog,          ATCC_ENV,
    "",                 "??"},
    {"@leaves", 99,     atcommand_leaves,       ATCC_ENV,
    "",                 "??"},
    {"@summon", 60,     atcommand_summon,       ATCC_MOB,
    "name|ID [num [desired name [x [y]]]]",
                        "summon monsters"},
    {"@adjgmlvl", 99,   atcommand_adjgmlvl,     ATCC_ADMIN,
    "",                 "temporarily adjust the GM level of a character"},
    {"@adjcmdlvl", 99,  atcommand_adjcmdlvl,    ATCC_ADMIN,
    "",                 "temporarily adjust the level of a @command"},
    {"@trade", 60,      atcommand_trade,        ATCC_CHAR,
    "charname",         "open a trade window with anyone"},
    {"@unmute", 60,     atcommand_unmute,       ATCC_CHAR,
    "",                 "??"},
    {"@charwipe", 60,   atcommand_char_wipe,    ATCC_CHAR,
    "",                 "??"},
    {"@setmagic", 99,   atcommand_set_magic,    ATCC_MISC,
    "",                 "??"},
    {"@magicinfo", 60,  atcommand_magic_info,   ATCC_MISC,
    "",                 "??"},
    {"@log", 60,        atcommand_log,          ATCC_MISC,
    "message",          "Do nothing (just log it like all commands)"},
    {"@l", 60,          atcommand_log,          ATCC_MISC,
    "message",          "Do nothing (just log it like all commands)"},
    {"@tee", 60,        atcommand_tee,          ATCC_MISC,
    "",                 "Say something (and log it)"},
    {"@t", 60,          atcommand_tee,          ATCC_MISC,
    "",                 "Say something (and log it)"},
    {"@invisible", 60,  atcommand_invisible,    ATCC_SELF,
    "",                 "hide from players"},
    {"@visible", 60,    atcommand_visible,      ATCC_SELF,
    "",                 "hide from players"},
    {"@hugo", 60,       atcommand_iterate_forward_over_players, ATCC_SELF,
    "",                 "go to next player"},
    {"@linus", 60,      atcommand_iterate_backwards_over_players, ATCC_SELF,
    "",                 "go to previous player"},
    {"@sp-info", 40,    atcommand_skillpool_info, ATCC_SELF,
    "",                 "??"},
    {"@sp-focus", 80,   atcommand_skillpool_focus, ATCC_SELF,
    "",                 "??"},
    {"@sp-unfocus", 80, atcommand_skillpool_unfocus, ATCC_SELF,
    "",                 "??"},
    {"@skill-learn", 80, atcommand_skill_learn, ATCC_SELF,
    "",                 "??"},
    {"@wgm", 0,         atcommand_wgm,          ATCC_MISC,
    "message",          "report something to the GM team"},
    {"@ipcheck", 60,    atcommand_ipcheck,      ATCC_ADMIN,
    "charname",         "Display all players from the same IP"},
};

/*========================================
 * At-command logging
 */
void log_atcommand (struct map_session_data *sd, const char *fmt, ...)
{
    char message[512];
    va_list ap;

    va_start (ap, fmt);
    vsnprintf (message, sizeof (message), fmt, ap);
    va_end (ap);

    gm_log ("%s(%d,%d) %s(%d) : %s", maps[sd->bl.m].name, sd->bl.x,
                sd->bl.y, sd->status.name, sd->status.account_id, message);
}

char *gm_logfile_name = NULL;
/*==========================================
 * Log a timestamped line to GM log file
 *------------------------------------------
 */
void gm_log (const char *fmt, ...)
{
    static int last_logfile_nr = 0;
    static FILE *gm_logfile = NULL;
    time_t time_v;
    struct tm ctime_;
    int  month, year, logfile_nr;
    char message[512];
    va_list ap;

    if (!gm_logfile_name)
        return;

    va_start (ap, fmt);
    vsnprintf (message, 511, fmt, ap);
    va_end (ap);

    time (&time_v);
    gmtime_r (&time_v, &ctime_);

    year = ctime_.tm_year + 1900;
    month = ctime_.tm_mon + 1;
    logfile_nr = (year * 12) + month;

    if (logfile_nr != last_logfile_nr)
    {
        char *fullname = (char *)malloc (strlen (gm_logfile_name) + 10);
        sprintf (fullname, "%s.%04d-%02d", gm_logfile_name, year, month);

        if (gm_logfile)
            fclose_ (gm_logfile);

        gm_logfile = fopen_ (fullname, "a");
        free (fullname);

        if (!gm_logfile)
        {
            perror ("GM log file");
            gm_logfile_name = NULL;
        }
        last_logfile_nr = logfile_nr;
    }

    fprintf (gm_logfile, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
             year, month, ctime_.tm_mday, ctime_.tm_hour,
             ctime_.tm_min, ctime_.tm_sec, message);

    fflush (gm_logfile);
}


static bool atcommand (gm_level_t level, const char *message, AtCommandInfo *info);

bool is_atcommand (const int fd, struct map_session_data *sd, const char *message,
                   gm_level_t gmlvl)
{
    AtCommandInfo info;

    nullpo_ret (sd);

    if (!message || !*message)
        return 0;

    memset (&info, 0, sizeof (info));

    bool type = atcommand (gmlvl ? gmlvl : pc_isGM (sd), message, &info);
    if (type)
    {
        char command[100];
        char output[200];
        const char *str = message;
        const char *p = message;
        memset (command, '\0', sizeof (command));
        memset (output, '\0', sizeof (output));
        while (*p && !isspace (*p))
            p++;
        if (p - str >= sizeof (command))    // too long
            return 0;
        strncpy (command, str, p - str);
        while (isspace (*p))
            p++;

        if (!type || !info.proc)
        {
            sprintf (output, "%s is Unknown Command.", command);
            clif_displaymessage (fd, output);
        }
        else
        {
            if (info.proc (fd, sd, command, p) != 0)
            {
                // Command can not be executed
                sprintf (output, "%s failed.", command);
                clif_displaymessage (fd, output);
            }
            else
            {
                if (info.level)    // Don't log level 0 commands
                    log_atcommand (sd, "%s %s", command, p);
            }
        }

        return true;
    }

    return false;
}

/// get info about command
bool atcommand (gm_level_t level, const char *message, AtCommandInfo *info)
{
    const char *p = message;

    if (!info)
        return false;
    if (battle_config.atc_gmonly && !level)    // level = pc_isGM(sd)
        return false;
    if (!p || !*p)
    {
        fprintf (stderr, "at command message is empty\n");
        return false;
    }

    if (*p == '@')
    {                           // check first char.
        char command[101];
        int  i = 0;
        memset (info, 0, sizeof (AtCommandInfo));
        sscanf (p, "%100s", command);
        command[sizeof (command) - 1] = '\0';

        while (i < ARRAY_SIZEOF(atcommand_info))
        {
            if (strcasecmp (command, atcommand_info[i].command) == 0
                && level >= atcommand_info[i].level)
                break;
            i++;
        }

        if (i == ARRAY_SIZEOF(atcommand_info))
            return false;
        *info = atcommand_info[i];
    }
    else
        return false;

    return true;
}

/*==========================================
 *
 *------------------------------------------
 */
static int atkillmonster_sub (struct block_list *bl, va_list ap)
{
    int  flag = va_arg (ap, int);

    nullpo_retr (0, bl);

    if (flag)
        mob_damage (NULL, (struct mob_data *) bl,
                    ((struct mob_data *) bl)->hp, 2);
    else
        mob_delete ((struct mob_data *) bl);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
static AtCommandInfo *get_atcommandinfo_byname (const char *name)
{
    for (int i = 0; i < ARRAY_SIZEOF (atcommand_info); i++)
        if (strcasecmp (atcommand_info[i].command + 1, name) == 0)
            return &atcommand_info[i];

    return NULL;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_config_read (const char *cfgName)
{
    char line[1024], w1[1024], w2[1024];
    AtCommandInfo *p;
    FILE *fp;

    if ((fp = fopen_ (cfgName, "r")) == NULL)
    {
        printf ("At commands configuration file not found: %s\n", cfgName);
        return 1;
    }

    while (fgets (line, sizeof (line) - 1, fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;

        if (sscanf (line, "%1023[^:]:%1023s", w1, w2) != 2)
            continue;
        p = get_atcommandinfo_byname (w1);
        if (p != NULL)
        {
            p->level = atoi (w2);
            if (p->level > 100)
                p->level = 100;
        }

        if (strcasecmp (w1, "import") == 0)
            atcommand_config_read (w2);
    }
    fclose_ (fp);

    return 0;
}

/*==========================================
// @ command processing functions
 *------------------------------------------
 */

/*==========================================
 * @setup - Safely set a chars levels and warp them to a special place
 * TAW Specific
 *------------------------------------------
 */
int atcommand_setup (const int fd, struct map_session_data *sd,
                     const char *UNUSED, const char *message)
{
    char buf[256];
    char character[100];
    int  level = 1;

    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%d %99[^\n]", &level, character) < 2)
    {
        clif_displaymessage (fd, "Usage: @setup <level> <char name>");
        return -1;
    }
    level--;

    snprintf (buf, 255, "-255 %s", character);
    atcommand_character_baselevel (fd, sd, "@charbaselvl", buf);

    snprintf (buf, 255, "%d %s", level, character);
    atcommand_character_baselevel (fd, sd, "@charbaselvl", buf);

    // Emote skill
    snprintf (buf, 255, "1 1 %s", character);
    atcommand_skill_learn(fd, sd, "@skill-learn", buf);

    // Trade skill
    snprintf (buf, 255, "2 1 %s", character);
    atcommand_skill_learn(fd, sd, "@skill-learn", buf);

    // Party skill
    snprintf (buf, 255, "2 2 %s", character);
    atcommand_skill_learn(fd, sd, "@skill-learn", buf);

    snprintf (buf, 255, "018-1.gat 24 98 %s", character);
    atcommand_charwarp (fd, sd, "@charwarp", buf);

    return (0);

}

/*==========================================
 * @rura+
 *------------------------------------------
 */
int atcommand_charwarp (const int fd, struct map_session_data *sd,
                        const char *UNUSED, const char *message)
{
    char map_name[100];
    char character[100];
    int  x = 0, y = 0;
    struct map_session_data *pl_sd;
    int  m;

    memset (map_name, '\0', sizeof (map_name));
    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%99s %d %d %99[^\n]", map_name, &x, &y,
                   character) < 4)
    {
        clif_displaymessage (fd,
                             "Usage: @charwarp/@rura+ <mapname> <x> <y> <char name>");
        return -1;
    }

    if (x <= 0)
        x = MRAND (399) + 1;
    if (y <= 0)
        y = MRAND (399) + 1;
    if (strstr (map_name, ".gat") == NULL && strstr (map_name, ".afm") == NULL && strlen (map_name) < 13)   // 16 - 4 (.gat)
        strcat (map_name, ".gat");

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can rura+ only lower or same GM level
            if (x > 0 && x < 800 && y > 0 && y < 800)
            {
                m = map_mapname2mapid (map_name);
                if (m >= 0 && maps[m].flag.nowarpto
                    && battle_config.any_warp_GM_min_level > pc_isGM (sd))
                {
                    clif_displaymessage (fd,
                                         "You are not authorised to warp someone to this map.");
                    return -1;
                }
                if (maps[pl_sd->bl.m].flag.nowarp
                    && battle_config.any_warp_GM_min_level > pc_isGM (sd))
                {
                    clif_displaymessage (fd,
                                         "You are not authorised to warp this player from its actual map.");
                    return -1;
                }
                if (pc_setpos (pl_sd, map_name, x, y, 3) == 0)
                {
                    clif_displaymessage (pl_sd->fd, "Warped.");
                    clif_displaymessage (fd, "Player warped");
                }
                else
                {
                    clif_displaymessage (fd, "Map not found.");
                    return -1;
                }
            }
            else
            {
                clif_displaymessage (fd, "Coordinates out of range.");
                return -1;
            }
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_warp (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *message)
{
    char map_name[100];
    int  x = 0, y = 0;
    int  m;

    memset (map_name, '\0', sizeof (map_name));

    if (!message || !*message
        || sscanf (message, "%99s %d %d", map_name, &x, &y) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a map (usage: @warp <mapname> <x> <y>).");
        return -1;
    }

    if (x <= 0)
        x = MRAND (399) + 1;
    if (y <= 0)
        y = MRAND (399) + 1;

    if (strstr (map_name, ".gat") == NULL && strstr (map_name, ".afm") == NULL && strlen (map_name) < 13)   // 16 - 4 (.gat)
        strcat (map_name, ".gat");

    if (x > 0 && x < 800 && y > 0 && y < 800)
    {
        m = map_mapname2mapid (map_name);
        if (m >= 0 && maps[m].flag.nowarpto
            && battle_config.any_warp_GM_min_level > pc_isGM (sd))
        {
            clif_displaymessage (fd,
                                 "You are not authorised to warp you to this map.");
            return -1;
        }
        if (maps[sd->bl.m].flag.nowarp
            && battle_config.any_warp_GM_min_level > pc_isGM (sd))
        {
            clif_displaymessage (fd,
                                 "You are not authorised to warp you from your actual map.");
            return -1;
        }
        if (pc_setpos (sd, map_name, x, y, 3) == 0)
            clif_displaymessage (fd, "Warped.");
        else
        {
            clif_displaymessage (fd, "Map not found.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Coordinates out of range.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_where (const int fd, struct map_session_data *sd,
                     const char *UNUSED, const char *message)
{
    char character[100];
    char output[200];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));

    if (sscanf (message, "%99[^\n]", character) < 1)
        strcpy (character, sd->status.name);

    if ((pl_sd = map_nick2sd (character)) != NULL &&
        !((battle_config.hide_GM_session
           || (pl_sd->status.option & OPTION_HIDE))
          && (pc_isGM (pl_sd) > pc_isGM (sd))))
    {                           // you can look only lower or same level
        sprintf (output, "%s: %s (%d,%d)", pl_sd->status.name, pl_sd->mapname,
                 pl_sd->bl.x, pl_sd->bl.y);
        clif_displaymessage (fd, output);
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_goto (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *message)
{
    char character[100];
    char output[200];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @jumpto/@warpto/@goto <char name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (maps[pl_sd->bl.m].flag.nowarpto
            && battle_config.any_warp_GM_min_level > pc_isGM (sd))
        {
            clif_displaymessage (fd,
                                 "You are not authorised to warp you to the map of this player.");
            return -1;
        }
        if (maps[sd->bl.m].flag.nowarp
            && battle_config.any_warp_GM_min_level > pc_isGM (sd))
        {
            clif_displaymessage (fd,
                                 "You are not authorised to warp you from your actual map.");
            return -1;
        }
        pc_setpos (sd, pl_sd->mapname, pl_sd->bl.x, pl_sd->bl.y, 3);
        sprintf (output, "Jump to %s", character);
        clif_displaymessage (fd, output);
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_jump (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *message)
{
    char output[200];
    int  x = 0, y = 0;

    memset (output, '\0', sizeof (output));

    sscanf (message, "%d %d", &x, &y);

    if (x <= 0)
        x = MRAND (399) + 1;
    if (y <= 0)
        y = MRAND (399) + 1;
    if (x > 0 && x < 800 && y > 0 && y < 800)
    {
        if (maps[sd->bl.m].flag.nowarpto
            && battle_config.any_warp_GM_min_level > pc_isGM (sd))
        {
            clif_displaymessage (fd,
                                 "You are not authorised to warp you to your actual map.");
            return -1;
        }
        if (maps[sd->bl.m].flag.nowarp
            && battle_config.any_warp_GM_min_level > pc_isGM (sd))
        {
            clif_displaymessage (fd,
                                 "You are not authorised to warp you from your actual map.");
            return -1;
        }
        pc_setpos (sd, sd->mapname, x, y, 3);
        sprintf (output, "Jump to %d %d", x, y);
        clif_displaymessage (fd, output);
    }
    else
    {
        clif_displaymessage (fd, "Coordinates out of range.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_who (const int fd, struct map_session_data *sd,
                   const char *UNUSED, const char *message)
{
    char output[200];
    struct map_session_data *pl_sd;
    int  i, j, count;
    int  pl_GM_level, GM_level;
    char match_text[100];
    char player_name[24];

    memset (output, '\0', sizeof (output));
    memset (match_text, '\0', sizeof (match_text));
    memset (player_name, '\0', sizeof (player_name));

    if (sscanf (message, "%99[^\n]", match_text) < 1)
        strcpy (match_text, "");
    for (j = 0; match_text[j]; j++)
        match_text[j] = tolower (match_text[j]);

    count = 0;
    GM_level = pc_isGM (sd);
    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth)
        {
            pl_GM_level = pc_isGM (pl_sd);
            if (!
                ((battle_config.hide_GM_session
                  || (pl_sd->status.option & OPTION_HIDE))
                 && (pl_GM_level > GM_level)))
            {                   // you can look only lower or same level
                memcpy (player_name, pl_sd->status.name, 24);
                for (j = 0; player_name[j]; j++)
                    player_name[j] = tolower (player_name[j]);
                if (strstr (player_name, match_text) != NULL)
                {               // search with no case sensitive
                    if (pl_GM_level > 0)
                        sprintf (output,
                                 "Name: %s (GM:%d) | Location: %s %d %d",
                                 pl_sd->status.name, pl_GM_level,
                                 pl_sd->mapname, pl_sd->bl.x, pl_sd->bl.y);
                    else
                        sprintf (output, "Name: %s | Location: %s %d %d",
                                 pl_sd->status.name, pl_sd->mapname,
                                 pl_sd->bl.x, pl_sd->bl.y);
                    clif_displaymessage (fd, output);
                    count++;
                }
            }
        }
    }

    if (count == 0)
        clif_displaymessage (fd, "No player found.");
    else if (count == 1)
        clif_displaymessage (fd, "1 player found.");
    else
    {
        sprintf (output, "%d players found.", count);
        clif_displaymessage (fd, output);
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_whogroup (const int fd, struct map_session_data *sd,
                        const char *UNUSED, const char *message)
{
    char temp0[100];
    char temp1[100];
    char output[200];
    struct map_session_data *pl_sd;
    int  i, j, count;
    int  pl_GM_level, GM_level;
    char match_text[100];
    char player_name[24];
    struct party *p;

    memset (temp0, '\0', sizeof (temp0));
    memset (temp1, '\0', sizeof (temp1));
    memset (output, '\0', sizeof (output));
    memset (match_text, '\0', sizeof (match_text));
    memset (player_name, '\0', sizeof (player_name));

    if (sscanf (message, "%99[^\n]", match_text) < 1)
        strcpy (match_text, "");
    for (j = 0; match_text[j]; j++)
        match_text[j] = tolower (match_text[j]);

    count = 0;
    GM_level = pc_isGM (sd);
    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth)
        {
            pl_GM_level = pc_isGM (pl_sd);
            if (!
                ((battle_config.hide_GM_session
                  || (pl_sd->status.option & OPTION_HIDE))
                 && (pl_GM_level > GM_level)))
            {                   // you can look only lower or same level
                memcpy (player_name, pl_sd->status.name, 24);
                for (j = 0; player_name[j]; j++)
                    player_name[j] = tolower (player_name[j]);
                if (strstr (player_name, match_text) != NULL)
                {               // search with no case sensitive
                    p = party_search (pl_sd->status.party_id);
                    if (p == NULL)
                        sprintf (temp0, "None");
                    else
                        sprintf (temp0, "%s", p->name);
                    if (pl_GM_level > 0)
                        sprintf (output,
                                 "Name: %s (GM:%d) | Party: '%s'",
                                 pl_sd->status.name, pl_GM_level, temp0);
                    else
                        sprintf (output,
                                 "Name: %s | Party: '%s'",
                                 pl_sd->status.name, temp0);
                    clif_displaymessage (fd, output);
                    count++;
                }
            }
        }
    }

    if (count == 0)
        clif_displaymessage (fd, "No player found.");
    else if (count == 1)
        clif_displaymessage (fd, "1 player found.");
    else
    {
        sprintf (output, "%d players found", count);
        clif_displaymessage (fd, output);
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_whomap (const int fd, struct map_session_data *sd,
                      const char *UNUSED, const char *message)
{
    char output[200];
    struct map_session_data *pl_sd;
    int  i, count;
    int  pl_GM_level, GM_level;
    int  map_id;
    char map_name[100];

    memset (output, '\0', sizeof (output));
    memset (map_name, '\0', sizeof (map_name));

    if (!message || !*message)
        map_id = sd->bl.m;
    else
    {
        sscanf (message, "%99s", map_name);
        if (strstr (map_name, ".gat") == NULL && strstr (map_name, ".afm") == NULL && strlen (map_name) < 13)   // 16 - 4 (.gat)
            strcat (map_name, ".gat");
        if ((map_id = map_mapname2mapid (map_name)) < 0)
            map_id = sd->bl.m;
    }

    count = 0;
    GM_level = pc_isGM (sd);
    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth)
        {
            pl_GM_level = pc_isGM (pl_sd);
            if (!
                ((battle_config.hide_GM_session
                  || (pl_sd->status.option & OPTION_HIDE))
                 && (pl_GM_level > GM_level)))
            {                   // you can look only lower or same level
                if (pl_sd->bl.m == map_id)
                {
                    if (pl_GM_level > 0)
                        sprintf (output,
                                 "Name: %s (GM:%d) | Location: %s %d %d",
                                 pl_sd->status.name, pl_GM_level,
                                 pl_sd->mapname, pl_sd->bl.x, pl_sd->bl.y);
                    else
                        sprintf (output, "Name: %s | Location: %s %d %d",
                                 pl_sd->status.name, pl_sd->mapname,
                                 pl_sd->bl.x, pl_sd->bl.y);
                    clif_displaymessage (fd, output);
                    count++;
                }
            }
        }
    }

    if (count == 0)
        sprintf (output, "No player found in map '%s'.", maps[map_id].name);
    else if (count == 1)
        sprintf (output, "1 player found in map '%s'.", maps[map_id].name);
    else
    {
        sprintf (output, "%d players found in map '%s'.", count, maps[map_id].name);
    }
    clif_displaymessage (fd, output);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_whomapgroup (const int fd, struct map_session_data *sd,
                           const char *UNUSED, const char *message)
{
    char temp0[100];
    char temp1[100];
    char output[200];
    struct map_session_data *pl_sd;
    int  i, count;
    int  pl_GM_level, GM_level;
    int  map_id = 0;
    char map_name[100];
    struct party *p;

    memset (temp0, '\0', sizeof (temp0));
    memset (temp1, '\0', sizeof (temp1));
    memset (output, '\0', sizeof (output));
    memset (map_name, '\0', sizeof (map_name));

    if (!message || !*message)
        map_id = sd->bl.m;
    else
    {
        sscanf (message, "%99s", map_name);
        if (strstr (map_name, ".gat") == NULL && strstr (map_name, ".afm") == NULL && strlen (map_name) < 13)   // 16 - 4 (.gat)
            strcat (map_name, ".gat");
        if ((map_id = map_mapname2mapid (map_name)) < 0)
            map_id = sd->bl.m;
    }

    count = 0;
    GM_level = pc_isGM (sd);
    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth)
        {
            pl_GM_level = pc_isGM (pl_sd);
            if (!
                ((battle_config.hide_GM_session
                  || (pl_sd->status.option & OPTION_HIDE))
                 && (pl_GM_level > GM_level)))
            {                   // you can look only lower or same level
                if (pl_sd->bl.m == map_id)
                {
                    p = party_search (pl_sd->status.party_id);
                    if (p == NULL)
                        sprintf (temp0, "None");
                    else
                        sprintf (temp0, "%s", p->name);
                    if (pl_GM_level > 0)
                        sprintf (output,
                                 "Name: %s (GM:%d) | Party: '%s'",
                                 pl_sd->status.name, pl_GM_level, temp0);
                    else
                        sprintf (output,
                                 "Name: %s | Party: '%s'",
                                 pl_sd->status.name, temp0);
                    clif_displaymessage (fd, output);
                    count++;
                }
            }
        }
    }

    if (count == 0)
        sprintf (output, "No player found in map '%s'.", maps[map_id].name);
    else if (count == 1)
        sprintf (output, "1 player found in map '%s'.", maps[map_id].name);
    else
    {
        sprintf (output, "%d players found in map '%s'.", count, maps[map_id].name);
    }
    clif_displaymessage (fd, output);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_whogm (const int fd, struct map_session_data *sd,
                     const char *UNUSED, const char *message)
{
    char temp0[100];
    char temp1[100];
    char output[200];
    struct map_session_data *pl_sd;
    int  i, j, count;
    int  pl_GM_level, GM_level;
    char match_text[100];
    char player_name[24];
    struct party *p;

    memset (temp0, '\0', sizeof (temp0));
    memset (temp1, '\0', sizeof (temp1));
    memset (output, '\0', sizeof (output));
    memset (match_text, '\0', sizeof (match_text));
    memset (player_name, '\0', sizeof (player_name));

    if (sscanf (message, "%99[^\n]", match_text) < 1)
        strcpy (match_text, "");
    for (j = 0; match_text[j]; j++)
        match_text[j] = tolower (match_text[j]);

    count = 0;
    GM_level = pc_isGM (sd);
    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth)
        {
            pl_GM_level = pc_isGM (pl_sd);
            if (pl_GM_level > 0)
            {
                if (!
                    ((battle_config.hide_GM_session
                      || (pl_sd->status.option & OPTION_HIDE))
                     && (pl_GM_level > GM_level)))
                {               // you can look only lower or same level
                    memcpy (player_name, pl_sd->status.name, 24);
                    for (j = 0; player_name[j]; j++)
                        player_name[j] = tolower (player_name[j]);
                    if (strstr (player_name, match_text) != NULL)
                    {           // search with no case sensitive
                        sprintf (output,
                                 "Name: %s (GM:%d) | Location: %s %d %d",
                                 pl_sd->status.name, pl_GM_level,
                                 pl_sd->mapname, pl_sd->bl.x, pl_sd->bl.y);
                        clif_displaymessage (fd, output);
                        sprintf (output,
                                 "       BLvl: %d | Job: %s (Lvl: %d)",
                                 pl_sd->status.base_level,
                                 "N/A",
                                 pl_sd->status.job_level);
                        clif_displaymessage (fd, output);
                        p = party_search (pl_sd->status.party_id);
                        if (p == NULL)
                            sprintf (temp0, "None");
                        else
                            sprintf (temp0, "%s", p->name);
                        sprintf (output, "       Party: '%s'", temp0);
                        clif_displaymessage (fd, output);
                        count++;
                    }
                }
            }
        }
    }

    if (count == 0)
        clif_displaymessage (fd, "No GM found.");
    else if (count == 1)
        clif_displaymessage (fd, "1 GM found.");
    else
    {
        sprintf (output, "%d GMs found.", count);
        clif_displaymessage (fd, output);
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_save (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *UNUSED)
{
    nullpo_retr (-1, sd);

    pc_setsavepoint (sd, sd->mapname, sd->bl.x, sd->bl.y);
    pc_makesavestatus (sd);
    chrif_save (sd);
    clif_displaymessage (fd, "Character data respawn point saved.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_load (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *UNUSED)
{
    int  m;

    m = map_mapname2mapid (sd->status.save_point.map);
    if (m >= 0 && maps[m].flag.nowarpto
        && battle_config.any_warp_GM_min_level > pc_isGM (sd))
    {
        clif_displaymessage (fd,
                             "You are not authorised to warp you to your save map.");
        return -1;
    }
    if (maps[sd->bl.m].flag.nowarp
        && battle_config.any_warp_GM_min_level > pc_isGM (sd))
    {
        clif_displaymessage (fd,
                             "You are not authorised to warp you from your actual map.");
        return -1;
    }

    pc_setpos (sd, sd->status.save_point.map, sd->status.save_point.x,
               sd->status.save_point.y, 0);
    clif_displaymessage (fd, "Warping to respawn point.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_speed (const int fd, struct map_session_data *sd,
                     const char *UNUSED, const char *message)
{
    char output[200];
    int  speed;

    memset (output, '\0', sizeof (output));

    if (!message || !*message)
    {
        sprintf (output,
                 "Please, enter a speed value (usage: @speed <%d-%d>).",
                 MIN_WALK_SPEED, MAX_WALK_SPEED);
        clif_displaymessage (fd, output);
        return -1;
    }

    speed = atoi (message);
    if (speed >= MIN_WALK_SPEED && speed <= MAX_WALK_SPEED)
    {
        sd->speed = speed;
        //sd->walktimer = x;
        //この文を追加 by れ
        clif_updatestatus (sd, SP_SPEED);
        clif_displaymessage (fd, "Speed changed.");
    }
    else
    {
        sprintf (output,
                 "Please, enter a valid speed value (usage: @speed <%d-%d>).",
                 MIN_WALK_SPEED, MAX_WALK_SPEED);
        clif_displaymessage (fd, output);
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_storage (const int fd, struct map_session_data *sd,
                       const char *UNUSED, const char *UNUSED)
{
    struct storage *stor;       //changes from Freya/Yor
    nullpo_retr (-1, sd);

    if (sd->state.storage_flag)
    {
        clif_displaymessage (fd, "??");
        return -1;
    }

    if ((stor = account2storage2 (sd->status.account_id)) != NULL
        && stor->storage_status == 1)
    {
        clif_displaymessage (fd, "??");
        return -1;
    }

    storage_storageopen (sd);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_option (const int fd, struct map_session_data *sd,
                      const char *UNUSED, const char *message)
{
    int  param1 = 0, param2 = 0, param3 = 0;
    nullpo_retr (-1, sd);

    if (!message || !*message
        || sscanf (message, "%d %d %d", &param1, &param2, &param3) < 1
        || param1 < 0 || param2 < 0 || param3 < 0)
    {
        clif_displaymessage (fd,
                             "Please, enter at least a option (usage: @option <param1:0+> <param2:0+> <param3:0+>).");
        return -1;
    }

    sd->opt1 = param1;
    sd->opt2 = param2;
    if (!(sd->status.option & CART_MASK) && param3 & CART_MASK)
    {
        clif_cart_itemlist (sd);
        clif_cart_equiplist (sd);
        clif_updatestatus (sd, SP_CARTINFO);
    }
    sd->status.option = param3;
    // fix pecopeco display
    if (pc_isriding (sd))
    {                       // sd have the new value...
        sd->status.option &= ~0x0020;
    }

    clif_changeoption (&sd->bl);
    pc_calcstatus (sd, 0);
    clif_displaymessage (fd, "Options changed.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_hide (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *UNUSED)
{
    if (sd->status.option & OPTION_HIDE)
    {
        sd->status.option &= ~OPTION_HIDE;
        clif_displaymessage (fd, "Invisible: Off");
    }
    else
    {
        sd->status.option |= OPTION_HIDE;
        clif_displaymessage (fd, "Invisible: On");
    }
    clif_changeoption (&sd->bl);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_die (const int fd, struct map_session_data *sd,
                   const char *UNUSED, const char *UNUSED)
{
    pc_damage (NULL, sd, sd->status.hp + 1);
    clif_displaymessage (fd, "A pity! You've died.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_kill (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *message)
{
    char character[100];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @kill <char name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can kill only lower or same level
            pc_damage (NULL, pl_sd, pl_sd->status.hp + 1);
            clif_displaymessage (fd, "Character killed.");
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_alive (const int fd, struct map_session_data *sd,
                     const char *UNUSED, const char *UNUSED)
{
    sd->status.hp = sd->status.max_hp;
    sd->status.sp = sd->status.max_sp;
    pc_setstand (sd);
    if (battle_config.pc_invincible_time > 0)
        pc_setinvincibletimer (sd, battle_config.pc_invincible_time);
    clif_updatestatus (sd, SP_HP);
    clif_updatestatus (sd, SP_SP);
    clif_resurrection (&sd->bl, 1);
    clif_displaymessage (fd, "You've been revived! It's a miracle!");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_kami (const int fd, struct map_session_data *UNUSED,
                    const char *UNUSED, const char *message)
{
    char output[200];

    memset (output, '\0', sizeof (output));

    if (!message || !*message)
    {
        clif_displaymessage (fd,
                             "Please, enter a message (usage: @kami <message>).");
        return -1;
    }

    sscanf (message, "%199[^\n]", output);
    intif_GMmessage (output, strlen (output) + 1, 0);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_heal (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *message)
{
    int  hp = 0, sp = 0;        // [Valaris] thanks to fov

    sscanf (message, "%d %d", &hp, &sp);

    if (hp == 0 && sp == 0)
    {
        hp = sd->status.max_hp - sd->status.hp;
        sp = sd->status.max_sp - sd->status.sp;
    }
    else
    {
        if (hp > 0 && (hp > sd->status.max_hp || hp > (sd->status.max_hp - sd->status.hp))) // fix positiv overflow
            hp = sd->status.max_hp - sd->status.hp;
        else if (hp < 0 && (hp < -sd->status.max_hp || hp < (1 - sd->status.hp)))   // fix negativ overflow
            hp = 1 - sd->status.hp;
        if (sp > 0 && (sp > sd->status.max_sp || sp > (sd->status.max_sp - sd->status.sp))) // fix positiv overflow
            sp = sd->status.max_sp - sd->status.sp;
        else if (sp < 0 && (sp < -sd->status.max_sp || sp < (1 - sd->status.sp)))   // fix negativ overflow
            sp = 1 - sd->status.sp;
    }

    if (hp > 0)                 // display like heal
        clif_heal (fd, SP_HP, hp);
    else if (hp < 0)            // display like damage
        clif_damage (&sd->bl, &sd->bl, gettick (), 0, 0, -hp, 0, 4, 0);
    if (sp > 0)                 // no display when we lost SP
        clif_heal (fd, SP_SP, sp);

    if (hp != 0 || sp != 0)
    {
        pc_heal (sd, hp, sp);
        if (hp >= 0 && sp >= 0)
            clif_displaymessage (fd, "HP, SP recovered.");
        else
            clif_displaymessage (fd, "HP or/and SP modified.");
    }
    else
    {
        clif_displaymessage (fd, "HP and SP are already with the good value.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @item command (usage: @item <name/id_of_item> <quantity>)
 *------------------------------------------
 */
int atcommand_item (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *message)
{
    char item_name[100];
    int  number = 0, item_id, flag;
    struct item item_tmp;
    struct item_data *item_data;
    int  get_count, i;

    memset (item_name, '\0', sizeof (item_name));

    if (!message || !*message
        || sscanf (message, "%99s %d", item_name, &number) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter an item name/id (usage: @item <item name or ID> [quantity]).");
        return -1;
    }

    if (number <= 0)
        number = 1;

    item_id = 0;
    if ((item_data = itemdb_searchname (item_name)) != NULL ||
        (item_data = itemdb_exists (atoi (item_name))) != NULL)
        item_id = item_data->nameid;

    if (item_id >= 500)
    {
        get_count = number;
        if (item_data->type == 4 || item_data->type == 5 ||
            item_data->type == 7 || item_data->type == 8)
        {
            get_count = 1;
        }
        for (i = 0; i < number; i += get_count)
        {
            memset (&item_tmp, 0, sizeof (item_tmp));
            item_tmp.nameid = item_id;
            item_tmp.identify = 1;
            if ((flag =
                 pc_additem ((struct map_session_data *) sd, &item_tmp,
                             get_count)))
                clif_additem ((struct map_session_data *) sd, 0, 0, flag);
        }
        clif_displaymessage (fd, "Item created.");
    }
    else
    {
        clif_displaymessage (fd, "Invalid item ID or name.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_itemreset (const int fd, struct map_session_data *sd,
                         const char *UNUSED, const char *UNUSED)
{
    int  i;

    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].amount
            && sd->status.inventory[i].equip == 0)
            pc_delitem (sd, i, sd->status.inventory[i].amount, 0);
    }
    clif_displaymessage (fd, "All of your items have been removed.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_itemcheck (const int UNUSED, struct map_session_data *sd,
                         const char *UNUSED, const char *UNUSED)
{
    pc_checkitem (sd);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_baselevelup (const int fd, struct map_session_data *sd,
                           const char *UNUSED, const char *message)
{
    int  level, i;

    if (!message || !*message || (level = atoi (message)) == 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a level adjustement (usage: @blvl <number of levels>).");
        return -1;
    }

    if (level > 0)
    {
        if (sd->status.base_level == battle_config.maximum_level)
        {                       // check for max level by Valaris
            clif_displaymessage (fd, "Base level can't go any higher.");
            return -1;
        }                       // End Addition
        if (level > battle_config.maximum_level || level > (battle_config.maximum_level - sd->status.base_level))   // fix positiv overflow
            level = battle_config.maximum_level - sd->status.base_level;
        for (i = 1; i <= level; i++)
            sd->status.status_point += (sd->status.base_level + i + 14) / 4;
        sd->status.base_level += level;
        clif_updatestatus (sd, SP_BASELEVEL);
        clif_updatestatus (sd, SP_NEXTBASEEXP);
        clif_updatestatus (sd, SP_STATUSPOINT);
        pc_calcstatus (sd, 0);
        pc_heal (sd, sd->status.max_hp, sd->status.max_sp);
        clif_misceffect (&sd->bl, 0);
        clif_displaymessage (fd, "Base level raised.");
    }
    else
    {
        if (sd->status.base_level == 1)
        {
            clif_displaymessage (fd, "Base level can't go any lower.");
            return -1;
        }
        if (level < -battle_config.maximum_level || level < (1 - sd->status.base_level))    // fix negativ overflow
            level = 1 - sd->status.base_level;
        if (sd->status.status_point > 0)
        {
            for (i = 0; i > level; i--)
                sd->status.status_point -=
                    (sd->status.base_level + i + 14) / 4;
            if (sd->status.status_point < 0)
                sd->status.status_point = 0;
            clif_updatestatus (sd, SP_STATUSPOINT);
        }                       // to add: remove status points from stats
        sd->status.base_level += level;
        clif_updatestatus (sd, SP_BASELEVEL);
        clif_updatestatus (sd, SP_NEXTBASEEXP);
        pc_calcstatus (sd, 0);
        clif_displaymessage (fd, "Base level lowered.");
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_joblevelup (const int fd, struct map_session_data *sd,
                          const char *UNUSED, const char *message)
{
    int  up_level = 50, level;

    if (!message || !*message || (level = atoi (message)) == 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a level adjustement (usage: @jlvl <number of levels>).");
        return -1;
    }

    up_level -= 40;

    if (level > 0)
    {
        if (sd->status.job_level == up_level)
        {
            clif_displaymessage (fd, "Job level can't go any higher.");
            return -1;
        }
        if (level > up_level || level > (up_level - sd->status.job_level))  // fix positiv overflow
            level = up_level - sd->status.job_level;
        sd->status.job_level += level;
        clif_updatestatus (sd, SP_JOBLEVEL);
        clif_updatestatus (sd, SP_NEXTJOBEXP);
        sd->status.skill_point += level;
        clif_updatestatus (sd, SP_SKILLPOINT);
        pc_calcstatus (sd, 0);
        clif_misceffect (&sd->bl, 1);
        clif_displaymessage (fd, "Job level raised.");
    }
    else
    {
        if (sd->status.job_level == 1)
        {
            clif_displaymessage (fd, "Job level can't go any lower.");
            return -1;
        }
        if (level < -up_level || level < (1 - sd->status.job_level))    // fix negativ overflow
            level = 1 - sd->status.job_level;
        sd->status.job_level += level;
        clif_updatestatus (sd, SP_JOBLEVEL);
        clif_updatestatus (sd, SP_NEXTJOBEXP);
        if (sd->status.skill_point > 0)
        {
            sd->status.skill_point += level;
            if (sd->status.skill_point < 0)
                sd->status.skill_point = 0;
            clif_updatestatus (sd, SP_SKILLPOINT);
        }                       // to add: remove status points from skills
        pc_calcstatus (sd, 0);
        clif_displaymessage (fd, "Job level lowered.");
    }

    return 0;
}


static void atcommand_help_cat_name(int fd, AtCommandCategory cat)
{
    switch (cat)
    {
    case ATCC_UNK:      clif_displaymessage (fd, "-- Unknown Commands --"); return;
    case ATCC_MISC:     clif_displaymessage (fd, "-- Miscellaneous Commands --"); return;
    case ATCC_INFO:     clif_displaymessage (fd, "-- Information Commands --"); return;
    case ATCC_MSG:      clif_displaymessage (fd, "-- Message Commands --"); return;
    case ATCC_SELF:     clif_displaymessage (fd, "-- Self Char Commands --"); return;
    case ATCC_MOB:      clif_displaymessage (fd, "-- Mob Commands --"); return;
    case ATCC_ITEM:     clif_displaymessage (fd, "-- Item Commands --"); return;
    case ATCC_GROUP:    clif_displaymessage (fd, "-- Group Commands --"); return;
    case ATCC_CHAR:     clif_displaymessage (fd, "-- Other Char Commands --"); return;
    case ATCC_ENV:      clif_displaymessage (fd, "-- Environment Commands --"); return;
    case ATCC_ADMIN:    clif_displaymessage (fd, "-- Admin Commands --"); return;
    default: abort();
    }
}

static void atcommand_help_brief(int fd, const AtCommandInfo& info)
{
    size_t command_len = strlen(info.command);
    size_t arg_help_len = strlen(info.arg_help);
    char buf[command_len + arg_help_len + 2];
    memcpy (buf, info.command, command_len);
    buf[command_len] = ' ';
    memcpy (buf + command_len + 1, info.arg_help, arg_help_len);
    buf[command_len + arg_help_len + 1] = '\0';
    clif_displaymessage (fd, buf);
}

static void atcommand_help_long(int fd, const AtCommandInfo& info)
{
    atcommand_help_brief(fd, info);
    clif_displaymessage(fd, info.long_help);
}

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

static void atcommand_help_cat(int fd, gm_level_t gm_level, AtCommandCategory cat)
{
    atcommand_help_cat_name (fd, cat);
    for (int i = 0; i < ARRAY_SIZEOF(atcommand_info); i++)
        if (gm_level >= atcommand_info[i].level && atcommand_info[i].cat == cat)
            atcommand_help_brief(fd, atcommand_info[i]);
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_help (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *message)
{
    gm_level_t gm_level = pc_isGM (sd);
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
        clif_displaymessage (fd, "No such command or level too low");
        return 0;
    }
    if (!*message)
    {
        clif_displaymessage (fd, "You must specify a @command or a category.");
        clif_displaymessage (fd, "Categories: all unk misc info msg self char env admin");
        return 0;
    }
    if (strcasecmp(message, "all") == 0)
    {
        atcommand_help_all (fd, gm_level);
        return 0;
    }
    if (strcasecmp(message, "unk") == 0 || strcasecmp(message, "unknown") == 0)
    {
        atcommand_help_cat (fd, gm_level, ATCC_UNK);
        return 0;
    }
    if (strcasecmp(message, "misc") == 0 || strcasecmp(message, "miscellaneous") == 0)
    {
        atcommand_help_cat (fd, gm_level, ATCC_MISC);
        return 0;
    }
    if (strcasecmp(message, "info") == 0 || strcasecmp(message, "information") == 0)
    {
        atcommand_help_cat (fd, gm_level, ATCC_INFO);
        return 0;
    }
    if (strcasecmp(message, "msg") == 0 || strcasecmp(message, "message") == 0 || strcasecmp(message, "messaging") == 0)
    {
        atcommand_help_cat (fd, gm_level, ATCC_MSG);
        return 0;
    }
    if (strcasecmp(message, "self") == 0)
    {
        atcommand_help_cat (fd, gm_level, ATCC_SELF);
        return 0;
    }
    if (strcasecmp(message, "monster") == 0 || strcasecmp(message, "monsters") == 0 ||
            strcasecmp(message, "mob") == 0 || strcasecmp(message, "mobs") == 0)
    {
        atcommand_help_cat (fd, gm_level, ATCC_MOB);
        return 0;
    }
    if (strcasecmp(message, "item") == 0 || strcasecmp(message, "items") == 0)
    {
        atcommand_help_cat (fd, gm_level, ATCC_ITEM);
        return 0;
    }
    if (strcasecmp(message, "group") == 0 || strcasecmp(message, "groups") == 0 || strcasecmp(message, "pvp") == 0)
    {
        atcommand_help_cat (fd, gm_level, ATCC_GROUP);
        return 0;
    }
    if (strcasecmp(message, "char") == 0)
    {
        atcommand_help_cat (fd, gm_level, ATCC_CHAR);
        return 0;
    }
    if (strcasecmp(message, "env") == 0 ||strcasecmp(message, "environment") == 0)
    {
        atcommand_help_cat (fd, gm_level, ATCC_ENV);
        return 0;
    }
    if (strcasecmp(message, "admin") == 0 || strcasecmp(message, "admininstration") == 0)
    {
        atcommand_help_cat (fd, gm_level, ATCC_ADMIN);
        return 0;
    }

    clif_displaymessage (fd, "No such category");
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_gm (const int fd, struct map_session_data *sd,
                  const char *UNUSED, const char *message)
{
    char password[100];

    memset (password, '\0', sizeof (password));

    if (!message || !*message || sscanf (message, "%99[^\n]", password) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a password (usage: @gm <password>).");
        return -1;
    }

    if (pc_isGM (sd))
    {                           // a GM can not use this function. only a normal player (become gm is not for gm!)
        clif_displaymessage (fd, "You already have some GM powers.");
        return -1;
    }
    else
        chrif_changegm (sd->status.account_id, password,
                        strlen (password) + 1);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_pvpoff (const int fd, struct map_session_data *sd,
                      const char *UNUSED, const char *UNUSED)
{
    struct map_session_data *pl_sd;
    int  i;

    if (battle_config.pk_mode)
    {                           //disable command if server is in PK mode [Valaris]
        clif_displaymessage (fd, "This option cannot be used in PK Mode.");
        return -1;
    }

    if (maps[sd->bl.m].flag.pvp)
    {
        maps[sd->bl.m].flag.pvp = 0;
        clif_send0199 (sd->bl.m, 0);
        for (i = 0; i < fd_max; i++)
        {                       //人数分ループ
            if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
                && pl_sd->state.auth)
            {
                if (sd->bl.m == pl_sd->bl.m)
                {
                    clif_pvpset (pl_sd, 0, 0, 2);
                    if (pl_sd->pvp_timer != -1)
                    {
                        delete_timer (pl_sd->pvp_timer,
                                      pc_calc_pvprank_timer);
                        pl_sd->pvp_timer = -1;
                    }
                }
            }
        }
        clif_displaymessage (fd, "PvP: Off.");
    }
    else
    {
        clif_displaymessage (fd, "PvP is already Off.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_pvpon (const int fd, struct map_session_data *sd,
                     const char *UNUSED, const char *UNUSED)
{
    struct map_session_data *pl_sd;
    int  i;

    if (battle_config.pk_mode)
    {                           //disable command if server is in PK mode [Valaris]
        clif_displaymessage (fd, "This option cannot be used in PK Mode.");
        return -1;
    }

    if (!maps[sd->bl.m].flag.pvp && !maps[sd->bl.m].flag.nopvp)
    {
        maps[sd->bl.m].flag.pvp = 1;
        clif_send0199 (sd->bl.m, 1);
        for (i = 0; i < fd_max; i++)
        {
            if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
                && pl_sd->state.auth)
            {
                if (sd->bl.m == pl_sd->bl.m && pl_sd->pvp_timer == -1)
                {
                    pl_sd->pvp_timer = add_timer (gettick () + 200,
                                                  pc_calc_pvprank_timer,
                                                  pl_sd->bl.id, 0);
                    pl_sd->pvp_rank = 0;
                    pl_sd->pvp_lastusers = 0;
                    pl_sd->pvp_point = 5;
                }
            }
        }
        clif_displaymessage (fd, "PvP: On.");
    }
    else
    {
        clif_displaymessage (fd, "PvP is already On.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_model (const int fd, struct map_session_data *sd,
                     const char *UNUSED, const char *message)
{
    int  hair_style = 0, hair_color = 0, cloth_color = 0;
    char output[200];

    memset (output, '\0', sizeof (output));

    if (!message || !*message
        || sscanf (message, "%d %d %d", &hair_style, &hair_color,
                   &cloth_color) < 1)
    {
        sprintf (output,
                 "Please, enter at least a value (usage: @model <hair ID: %d-%d> <hair color: %d-%d> <clothes color: %d-%d>).",
                 0, NUM_HAIR_STYLES-1, 0,
                 NUM_HAIR_COLORS-1, 0, NUM_CLOTHES_COLORS-1);
        clif_displaymessage (fd, output);
        return -1;
    }

    if (hair_style >= 0 && hair_style < NUM_HAIR_STYLES &&
        hair_color >= 0 && hair_color < NUM_HAIR_COLORS &&
        cloth_color >= 0 && cloth_color < NUM_CLOTHES_COLORS)
    {
        pc_changelook (sd, LOOK_HAIR, hair_style);
        pc_changelook (sd, LOOK_HAIR_COLOR, hair_color);
        pc_changelook (sd, LOOK_CLOTHES_COLOR, cloth_color);
        clif_displaymessage (fd, "Appearence changed.");
    }
    else
    {
        clif_displaymessage (fd, "An invalid number was specified.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @dye && @ccolor
 *------------------------------------------
 */
int atcommand_dye (const int fd, struct map_session_data *sd,
                   const char *UNUSED, const char *message)
{
    int  cloth_color = 0;
    char output[200];

    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%d", &cloth_color) < 1)
    {
        sprintf (output,
                 "Please, enter a clothes color (usage: @dye/@ccolor <clothes color: %d-%d>).",
                 0, NUM_CLOTHES_COLORS);
        clif_displaymessage (fd, output);
        return -1;
    }

    if (cloth_color >= 0 && cloth_color <= NUM_CLOTHES_COLORS)
    {
        pc_changelook (sd, LOOK_CLOTHES_COLOR, cloth_color);
        clif_displaymessage (fd, "Appearance changed.");
    }
    else
    {
        clif_displaymessage (fd, "An invalid number was specified.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @hairstyle && @hstyle
 *------------------------------------------
 */
int atcommand_hair_style (const int fd, struct map_session_data *sd,
                          const char *UNUSED, const char *message)
{
    int  hair_style = 0;
    char output[200];

    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%d", &hair_style) < 1)
    {
        sprintf (output,
                 "Please, enter a hair style (usage: @hairstyle/@hstyle <hair ID: %d-%d>).",
                 0, NUM_HAIR_STYLES-1);
        clif_displaymessage (fd, output);
        return -1;
    }

    if (hair_style >= 0 && hair_style < NUM_HAIR_STYLES)
    {
        pc_changelook (sd, LOOK_HAIR, hair_style);
        clif_displaymessage (fd, "Appearance changed.");
    }
    else
    {
        clif_displaymessage (fd, "An invalid number was specified.");
        return -1;
    }

    return 0;
}


/*==========================================
 * @haircolor && @hcolor
 *------------------------------------------
 */
int atcommand_hair_color (const int fd, struct map_session_data *sd,
                          const char *UNUSED, const char *message)
{
    int  hair_color = 0;
    char output[200];

    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%d", &hair_color) < 1)
    {
        sprintf (output,
                 "Please, enter a hair color (usage: @haircolor/@hcolor <hair color: %d-%d>).",
                 0, NUM_HAIR_COLORS-1);
        clif_displaymessage (fd, output);
        return -1;
    }

    if (hair_color >= 0 && hair_color < NUM_HAIR_COLORS)
    {
        pc_changelook (sd, LOOK_HAIR_COLOR, hair_color);
        clif_displaymessage (fd, "Appearance changed.");
    }
    else
    {
        clif_displaymessage (fd, "An invalid number was specified.");
        return -1;
    }

    return 0;
}


/*==========================================
 * @go [city_number/city_name]: improved by [yor] to add city names and help
 *------------------------------------------
 */
int atcommand_go (const int fd, struct map_session_data *sd,
                  const char *UNUSED, const char *message)
{
    int  i;
    int  town;
    char map_name[100];
    char output[200];
    int  m;

    struct
    {
        char map[16];
        int  x, y;
    } data[] =
    {
        {
        "prontera.gat", 156, 191},  //   0=Prontera
        {
        "morocc.gat", 156, 93}, //   1=Morroc
        {
        "geffen.gat", 119, 59}, //   2=Geffen
        {
        "payon.gat", 162, 233}, //   3=Payon
        {
        "alberta.gat", 192, 147},   //   4=Alberta
        {
        "izlude.gat", 128, 114},    //   5=Izlude
        {
        "aldebaran.gat", 140, 131}, //   6=Al de Baran
        {
        "xmas.gat", 147, 134},  //   7=Lutie
        {
        "comodo.gat", 209, 143},    //   8=Comodo
        {
        "yuno.gat", 157, 51},   //   9=Yuno
        {
        "amatsu.gat", 198, 84}, //  10=Amatsu
        {
        "gonryun.gat", 160, 120},   //  11=Gon Ryun
        {
        "umbala.gat", 89, 157}, //  12=Umbala
        {
        "niflheim.gat", 21, 153},   //  13=Niflheim
        {
        "louyang.gat", 217, 40},    //  14=Lou Yang
        {
        "new_1-1.gat", 53, 111},    //  15=Start point
        {
        "sec_pri.gat", 23, 61}, //  16=Prison
    };

    memset (map_name, '\0', sizeof (map_name));
    memset (output, '\0', sizeof (output));

    // get the number
    town = atoi (message);

    // if no value, display all value
    if (!message || !*message || sscanf (message, "%99s", map_name) < 1
        || town < -3 || town >= (int) (sizeof (data) / sizeof (data[0])))
    {
        clif_displaymessage (fd, "Invalid location number or name.");
        clif_displaymessage (fd, "Please, use one of this number/name:");
        clif_displaymessage (fd,
                             "-3=(Memo point 2)   4=Alberta       11=Gon Ryun");
        clif_displaymessage (fd,
                             "-2=(Memo point 1)   5=Izlude        12=Umbala");
        clif_displaymessage (fd,
                             "-1=(Memo point 0)   6=Al de Baran   13=Niflheim");
        clif_displaymessage (fd,
                             " 0=Prontera         7=Lutie         14=Lou Yang");
        clif_displaymessage (fd,
                             " 1=Morroc           8=Comodo        15=Start point");
        clif_displaymessage (fd,
                             " 2=Geffen           9=Yuno          16=Prison");
        clif_displaymessage (fd, " 3=Payon           10=Amatsu");
        return -1;
    }
    else
    {
        // get possible name of the city and add .gat if not in the name
        map_name[sizeof (map_name) - 1] = '\0';
        for (i = 0; map_name[i]; i++)
            map_name[i] = tolower (map_name[i]);
        if (strstr (map_name, ".gat") == NULL && strstr (map_name, ".afm") == NULL && strlen (map_name) < 13)   // 16 - 4 (.gat)
            strcat (map_name, ".gat");
        // try to see if it's a name, and not a number (try a lot of possibilities, write errors and abbreviations too)
        if (strncmp (map_name, "prontera.gat", 3) == 0)
        {                       // 3 first characters
            town = 0;
        }
        else if (strncmp (map_name, "morocc.gat", 3) == 0)
        {                       // 3 first characters
            town = 1;
        }
        else if (strncmp (map_name, "geffen.gat", 3) == 0)
        {                       // 3 first characters
            town = 2;
        }
        else if (strncmp (map_name, "payon.gat", 3) == 0 || // 3 first characters
                 strncmp (map_name, "paion.gat", 3) == 0)
        {                       // writing error (3 first characters)
            town = 3;
        }
        else if (strncmp (map_name, "alberta.gat", 3) == 0)
        {                       // 3 first characters
            town = 4;
        }
        else if (strncmp (map_name, "izlude.gat", 3) == 0 ||    // 3 first characters
                 strncmp (map_name, "islude.gat", 3) == 0)
        {                       // writing error (3 first characters)
            town = 5;
        }
        else if (strncmp (map_name, "aldebaran.gat", 3) == 0 || // 3 first characters
                 strcmp (map_name, "al.gat") == 0)
        {                       // al (de baran)
            town = 6;
        }
        else if (strncmp (map_name, "lutie.gat", 3) == 0 || // name of the city, not name of the map (3 first characters)
                 strcmp (map_name, "christmas.gat") == 0 || // name of the symbol
                 strncmp (map_name, "xmas.gat", 3) == 0 ||  // 3 first characters
                 strncmp (map_name, "x-mas.gat", 3) == 0)
        {                       // writing error (3 first characters)
            town = 7;
        }
        else if (strncmp (map_name, "comodo.gat", 3) == 0)
        {                       // 3 first characters
            town = 8;
        }
        else if (strncmp (map_name, "yuno.gat", 3) == 0)
        {                       // 3 first characters
            town = 9;
        }
        else if (strncmp (map_name, "amatsu.gat", 3) == 0 ||    // 3 first characters
                 strncmp (map_name, "ammatsu.gat", 3) == 0)
        {                       // writing error (3 first characters)
            town = 10;
        }
        else if (strncmp (map_name, "gonryun.gat", 3) == 0)
        {                       // 3 first characters
            town = 11;
        }
        else if (strncmp (map_name, "umbala.gat", 3) == 0)
        {                       // 3 first characters
            town = 12;
        }
        else if (strncmp (map_name, "niflheim.gat", 3) == 0)
        {                       // 3 first characters
            town = 13;
        }
        else if (strncmp (map_name, "louyang.gat", 3) == 0)
        {                       // 3 first characters
            town = 14;
        }
        else if (strncmp (map_name, "new_1-1.gat", 3) == 0 ||   // 3 first characters (or "newbies")
                 strncmp (map_name, "startpoint.gat", 3) == 0 ||    // name of the position (3 first characters)
                 strncmp (map_name, "begining.gat", 3) == 0)
        {                       // name of the position (3 first characters)
            town = 15;
        }
        else if (strncmp (map_name, "sec_pri.gat", 3) == 0 ||   // 3 first characters
                 strncmp (map_name, "prison.gat", 3) == 0 ||    // name of the position (3 first characters)
                 strncmp (map_name, "jails.gat", 3) == 0)
        {                       // name of the position
            town = 16;
        }

        if (town >= -3 && town <= -1)
        {
            if (sd->status.memo_point[-town - 1].map[0])
            {
                m = map_mapname2mapid (sd->status.memo_point[-town - 1].map);
                if (m >= 0 && maps[m].flag.nowarpto
                    && battle_config.any_warp_GM_min_level > pc_isGM (sd))
                {
                    clif_displaymessage (fd,
                                         "You are not authorised to warp you to this memo map.");
                    return -1;
                }
                if (maps[sd->bl.m].flag.nowarp
                    && battle_config.any_warp_GM_min_level > pc_isGM (sd))
                {
                    clif_displaymessage (fd,
                                         "You are not authorised to warp you from your actual map.");
                    return -1;
                }
                if (pc_setpos
                    (sd, sd->status.memo_point[-town - 1].map,
                     sd->status.memo_point[-town - 1].x,
                     sd->status.memo_point[-town - 1].y, 3) == 0)
                {
                    clif_displaymessage (fd, "Warped.");
                }
                else
                {
                    clif_displaymessage (fd, "Map not found.");
                    return -1;
                }
            }
            else
            {
                sprintf (output, "Your memo point #%d doesn't exist.", -town - 1);
                clif_displaymessage (fd, output);
                return -1;
            }
        }
        else if (town >= 0 && town < (int) (sizeof (data) / sizeof (data[0])))
        {
            m = map_mapname2mapid (data[town].map);
            if (m >= 0 && maps[m].flag.nowarpto
                && battle_config.any_warp_GM_min_level > pc_isGM (sd))
            {
                clif_displaymessage (fd,
                                     "You are not authorised to warp you to this destination map.");
                return -1;
            }
            if (maps[sd->bl.m].flag.nowarp
                && battle_config.any_warp_GM_min_level > pc_isGM (sd))
            {
                clif_displaymessage (fd,
                                     "You are not authorised to warp you from your actual map.");
                return -1;
            }
            if (pc_setpos (sd, data[town].map, data[town].x, data[town].y, 3)
                == 0)
            {
                clif_displaymessage (fd, "Warped.");
            }
            else
            {
                clif_displaymessage (fd, "Map not found.");
                return -1;
            }
        }
        else
        {                       // if you arrive here, you have an error in town variable when reading of names
            clif_displaymessage (fd, "Invalid location number or name.");
            return -1;
        }
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_spawn (const int fd, struct map_session_data *sd,
                     const char *command, const char *message)
{
    char monster[100];
    char output[200];
    int  mob_id;
    int  number = 0;
    int  x = 0, y = 0;
    int  count;
    int  i, j, k;
    int  mx, my, range;

    memset (monster, '\0', sizeof (monster));
    memset (output, '\0', sizeof (output));

    if (!message || !*message
        || sscanf (message, "%99s %d %d %d", monster, &number, &x, &y) < 1)
    {
        clif_displaymessage (fd, "Give a monster name/id please.");
        return -1;
    }

    // If monster identifier/name argument is a name
    if ((mob_id = mobdb_searchname (monster)) == 0) // check name first (to avoid possible name begining by a number)
        mob_id = mobdb_checkid (atoi (monster));

    if (mob_id == 0)
    {
        clif_displaymessage (fd, "Invalid monster ID or name.");
        return -1;
    }

    if (mob_id == 1288)
    {
        clif_displaymessage (fd, "Cannot spawn emperium.");
        return -1;
    }

    if (number <= 0)
        number = 1;

    // If value of atcommand_spawn_quantity_limit directive is greater than or equal to 1 and quantity of monsters is greater than value of the directive
    if (battle_config.atc_spawn_quantity_limit >= 1
        && number > battle_config.atc_spawn_quantity_limit)
        number = battle_config.atc_spawn_quantity_limit;

    if (battle_config.etc_log)
        printf ("%s monster='%s' id=%d count=%d (%d,%d)\n", command, monster,
                mob_id, number, x, y);

    count = 0;
    range = sqrt (number) / 2;
    range = range * 2 + 5;      // calculation of an odd number (+ 4 area around)
    for (i = 0; i < number; i++)
    {
        j = 0;
        k = 0;
        while (j++ < 8 && k == 0)
        {                       // try 8 times to spawn the monster (needed for close area)
            if (x <= 0)
                mx = sd->bl.x + (MRAND (range) - (range / 2));
            else
                mx = x;
            if (y <= 0)
                my = sd->bl.y + (MRAND (range) - (range / 2));
            else
                my = y;
            k = mob_once_spawn ((struct map_session_data *) sd, "this", mx,
                                my, "", mob_id, 1, "");
        }
        count += (k != 0) ? 1 : 0;
    }

    if (count != 0)
        if (number == count)
            clif_displaymessage (fd, "All monster summoned!");
        else
        {
            sprintf (output, "%d monster(s) summoned!", count);
            clif_displaymessage (fd, output);
        }
    else
    {
        clif_displaymessage (fd, "Invalid monster ID or name.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
static void atcommand_killmonster_sub (const int fd, struct map_session_data *sd,
                                const char *message, const int drop)
{
    int  map_id;
    char map_name[100];

    memset (map_name, '\0', sizeof (map_name));

    if (!message || !*message || sscanf (message, "%99s", map_name) < 1)
        map_id = sd->bl.m;
    else
    {
        if (strstr (map_name, ".gat") == NULL && strstr (map_name, ".afm") == NULL && strlen (map_name) < 13)   // 16 - 4 (.gat)
            strcat (map_name, ".gat");
        if ((map_id = map_mapname2mapid (map_name)) < 0)
            map_id = sd->bl.m;
    }

    map_foreachinarea (atkillmonster_sub, map_id, 0, 0, maps[map_id].xs,
                       maps[map_id].ys, BL_MOB, drop);

    clif_displaymessage (fd, "All monsters killed!");

    return;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_killmonster (const int fd, struct map_session_data *sd,
                           const char *UNUSED, const char *message)
{
    atcommand_killmonster_sub (fd, sd, message, 1);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
static int atlist_nearby_sub (struct block_list *bl, va_list ap)
{
    char buf[32];
    int  fd = va_arg (ap, int);
    nullpo_retr (0, bl);

    sprintf (buf, " - \"%s\"", ((struct map_session_data *) bl)->status.name);
    clif_displaymessage (fd, buf);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_list_nearby (const int fd, struct map_session_data *sd,
                           const char *UNUSED, const char *UNUSED)
{
    clif_displaymessage (fd, "Nearby players:");
    map_foreachinarea (atlist_nearby_sub, sd->bl.m, sd->bl.x - 1,
                       sd->bl.y - 1, sd->bl.x + 1, sd->bl.x + 1, BL_PC, fd);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_killmonster2 (const int fd, struct map_session_data *sd,
                            const char *UNUSED, const char *message)
{
    atcommand_killmonster_sub (fd, sd, message, 0);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_produce (const int fd, struct map_session_data *sd,
                       const char *UNUSED, const char *message)
{
    char item_name[100];
    int  item_id, attribute = 0, star = 0;
    int  flag = 0;
    struct item_data *item_data;
    struct item tmp_item;
    char output[200];

    memset (output, '\0', sizeof (output));
    memset (item_name, '\0', sizeof (item_name));

    if (!message || !*message
        || sscanf (message, "%99s %d %d", item_name, &attribute, &star) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter at least an item name/id (usage: @produce <equip name or equip ID> <element> <# of very's>).");
        return -1;
    }

    item_id = 0;
    if ((item_data = itemdb_searchname (item_name)) != NULL ||
        (item_data = itemdb_exists (atoi (item_name))) != NULL)
        item_id = item_data->nameid;

    if (itemdb_exists (item_id) &&
        (item_id <= 500 || item_id > 1099) &&
        (item_id < 4001 || item_id > 4148) &&
        (item_id < 7001 || item_id > 10019) && itemdb_isequip (item_id))
    {
        if (attribute < MIN_ATTRIBUTE || attribute > MAX_ATTRIBUTE)
            attribute = ATTRIBUTE_NORMAL;
        if (star < MIN_STAR || star > MAX_STAR)
            star = 0;
        memset (&tmp_item, 0, sizeof tmp_item);
        tmp_item.nameid = item_id;
        tmp_item.amount = 1;
        tmp_item.identify = 1;
        tmp_item.card[0] = 0x00ff;
        tmp_item.card[1] = ((star * 5) << 8) + attribute;
        *((unsigned long *) (&tmp_item.card[2])) = sd->char_id;
        clif_misceffect (&sd->bl, 3);   // 他人にも成功を通知
        if ((flag = pc_additem (sd, &tmp_item, 1)))
            clif_additem (sd, 0, 0, flag);
    }
    else
    {
        if (battle_config.error_log)
            printf ("@produce NOT WEAPON [%d]\n", item_id);
        if (item_id != 0 && itemdb_exists (item_id))
            sprintf (output, "This item (%d: '%s') is not an equipment.", item_id, item_data->name);
        else
            sprintf (output, "This item is not an equipment.");
        clif_displaymessage (fd, output);
        return -1;
    }

    return 0;
}

/*==========================================
 * Sub-function to display actual memo points
 *------------------------------------------
 */
static void atcommand_memo_sub (struct map_session_data *sd)
{
    int  i;
    char output[200];

    memset (output, '\0', sizeof (output));

    clif_displaymessage (sd->fd,
                         "Your actual memo positions are (except respawn point):");
    for (i = MIN_PORTAL_MEMO; i <= MAX_PORTAL_MEMO; i++)
    {
        if (sd->status.memo_point[i].map[0])
            sprintf (output, "%d - %s (%d,%d)", i,
                     sd->status.memo_point[i].map, sd->status.memo_point[i].x,
                     sd->status.memo_point[i].y);
        else
            sprintf (output, "%d - void", i);
        clif_displaymessage (sd->fd, output);
    }

    return;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_memo (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *message)
{
    int  position = 0;
    char output[200];

    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%d", &position) < 1)
        atcommand_memo_sub (sd);
    else
    {
        if (position >= MIN_PORTAL_MEMO && position <= MAX_PORTAL_MEMO)
        {
            if (maps[sd->bl.m].flag.nowarpto
                && battle_config.any_warp_GM_min_level > pc_isGM (sd))
            {
                clif_displaymessage (fd,
                                     "You are not authorised to memo this map.");
                return -1;
            }
            if (sd->status.memo_point[position].map[0])
            {
                sprintf (output, "You replace previous memo position %d - %s (%d,%d).", position, sd->status.memo_point[position].map, sd->status.memo_point[position].x, sd->status.memo_point[position].y);
                clif_displaymessage (fd, output);
            }
            memcpy (sd->status.memo_point[position].map, maps[sd->bl.m].name,
                    24);
            sd->status.memo_point[position].x = sd->bl.x;
            sd->status.memo_point[position].y = sd->bl.y;
            clif_skill_memo (sd, 0);
            if (pc_checkskill (sd, AL_WARP) <= (position + 1))
                clif_displaymessage (fd, "Note: you don't have the 'Warp' skill level to use it.");
            atcommand_memo_sub (sd);
        }
        else
        {
            sprintf (output,
                     "Please, enter a valid position (usage: @memo <memo_position:%d-%d>).",
                     MIN_PORTAL_MEMO, MAX_PORTAL_MEMO);
            clif_displaymessage (fd, output);
            atcommand_memo_sub (sd);
            return -1;
        }
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_gat (const int fd, struct map_session_data *sd,
                   const char *UNUSED, const char *UNUSED)
{
    char output[200];
    int  y;

    memset (output, '\0', sizeof (output));

    for (y = 2; y >= -2; y--)
    {
        sprintf (output, "%s (x= %d, y= %d) %02X %02X %02X %02X %02X",
                 maps[sd->bl.m].name, sd->bl.x - 2, sd->bl.y + y,
                 map_getcell (sd->bl.m, sd->bl.x - 2, sd->bl.y + y),
                 map_getcell (sd->bl.m, sd->bl.x - 1, sd->bl.y + y),
                 map_getcell (sd->bl.m, sd->bl.x, sd->bl.y + y),
                 map_getcell (sd->bl.m, sd->bl.x + 1, sd->bl.y + y),
                 map_getcell (sd->bl.m, sd->bl.x + 2, sd->bl.y + y));
        clif_displaymessage (fd, output);
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_packet (const int fd, struct map_session_data *sd,
                      const char *UNUSED, const char *message)
{
    int  x = 0, y = 0;

    if (!message || !*message || sscanf (message, "%d %d", &x, &y) < 2)
    {
        clif_displaymessage (fd,
                             "Please, enter a status type/flag (usage: @packet <status type> <flag>).");
        return -1;
    }

    clif_status_change (&sd->bl, x, y);

    return 0;
}

/*==========================================
 * @stpoint (Rewritten by [Yor])
 *------------------------------------------
 */
int atcommand_statuspoint (const int fd, struct map_session_data *sd,
                           const char *UNUSED, const char *message)
{
    int  point, new_status_point;

    if (!message || !*message || (point = atoi (message)) == 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a number (usage: @stpoint <number of points>).");
        return -1;
    }

    new_status_point = (int) sd->status.status_point + point;
    if (point > 0 && (point > 0x7FFF || new_status_point > 0x7FFF)) // fix positiv overflow
        new_status_point = 0x7FFF;
    else if (point < 0 && (point < -0x7FFF || new_status_point < 0))    // fix negativ overflow
        new_status_point = 0;

    if (new_status_point != (int) sd->status.status_point)
    {
        sd->status.status_point = (short) new_status_point;
        clif_updatestatus (sd, SP_STATUSPOINT);
        clif_displaymessage (fd, "Number of status points changed!");
    }
    else
    {
        if (point < 0)
            clif_displaymessage (fd, "Impossible to decrease the number/value.");
        else
            clif_displaymessage (fd, "Impossible to increase the number/value.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @skpoint (Rewritten by [Yor])
 *------------------------------------------
 */
int atcommand_skillpoint (const int fd, struct map_session_data *sd,
                          const char *UNUSED, const char *message)
{
    int  point, new_skill_point;

    if (!message || !*message || (point = atoi (message)) == 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a number (usage: @skpoint <number of points>).");
        return -1;
    }

    new_skill_point = (int) sd->status.skill_point + point;
    if (point > 0 && (point > 0x7FFF || new_skill_point > 0x7FFF))  // fix positiv overflow
        new_skill_point = 0x7FFF;
    else if (point < 0 && (point < -0x7FFF || new_skill_point < 0)) // fix negativ overflow
        new_skill_point = 0;

    if (new_skill_point != (int) sd->status.skill_point)
    {
        sd->status.skill_point = (short) new_skill_point;
        clif_updatestatus (sd, SP_SKILLPOINT);
        clif_displaymessage (fd, "Number of skill points changed!");
    }
    else
    {
        if (point < 0)
            clif_displaymessage (fd, "Impossible to decrease the number/value.");
        else
            clif_displaymessage (fd, "Impossible to increase the number/value.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @zeny (Rewritten by [Yor])
 *------------------------------------------
 */
int atcommand_zeny (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *message)
{
    int  zeny, new_zeny;

    if (!message || !*message || (zeny = atoi (message)) == 0)
    {
        clif_displaymessage (fd,
                             "Please, enter an amount (usage: @zeny <amount>).");
        return -1;
    }

    new_zeny = sd->status.zeny + zeny;
    if (zeny > 0 && (zeny > MAX_ZENY || new_zeny > MAX_ZENY))   // fix positiv overflow
        new_zeny = MAX_ZENY;
    else if (zeny < 0 && (zeny < -MAX_ZENY || new_zeny < 0))    // fix negativ overflow
        new_zeny = 0;

    if (new_zeny != sd->status.zeny)
    {
        sd->status.zeny = new_zeny;
        clif_updatestatus (sd, SP_ZENY);
        clif_displaymessage (fd, "Number of zenys changed!");
    }
    else
    {
        if (zeny < 0)
            clif_displaymessage (fd, "Impossible to decrease the number/value.");
        else
            clif_displaymessage (fd, "Impossible to increase the number/value.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_param (const int fd, struct map_session_data *sd,
                     const char *command, const char *message)
{
    int  i, idx, value = 0, new_value;
    const char *param[] =
        { "@str", "@agi", "@vit", "@int", "@dex", "@luk", NULL };
    short *status[] = {
        &sd->status.str, &sd->status.agi, &sd->status.vit,
        &sd->status.int_, &sd->status.dex, &sd->status.luk
    };
    char output[200];

    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%d", &value) < 1
        || value == 0)
    {
        sprintf (output,
                 "Please, enter a valid value (usage: @str,@agi,@vit,@int,@dex,@luk <+/-adjustement>).");
        clif_displaymessage (fd, output);
        return -1;
    }

    idx = -1;
    for (i = 0; param[i] != NULL; i++)
    {
        if (strcasecmp (command, param[i]) == 0)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0 || idx > MAX_STATUS_TYPE)
    {                           // normaly impossible...
        sprintf (output,
                 "Please, enter a valid value (usage: @str,@agi,@vit,@int,@dex,@luk <+/-adjustement>).");
        clif_displaymessage (fd, output);
        return -1;
    }

    new_value = (int) *status[idx] + value;
    if (value > 0 && (value > battle_config.max_parameter || new_value > battle_config.max_parameter))  // fix positiv overflow
        new_value = battle_config.max_parameter;
    else if (value < 0 && (value < -battle_config.max_parameter || new_value < 1))  // fix negativ overflow
        new_value = 1;

    if (new_value != (int) *status[idx])
    {
        *status[idx] = new_value;
        clif_updatestatus (sd, SP_STR + idx);
        clif_updatestatus (sd, SP_USTR + idx);
        pc_calcstatus (sd, 0);
        clif_displaymessage (fd, "Stat changed.");
    }
    else
    {
        if (value < 0)
            clif_displaymessage (fd, "Impossible to decrease the number/value.");
        else
            clif_displaymessage (fd, "Impossible to increase the number/value.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
//** Stat all by fritz (rewritten by [Yor])
int atcommand_all_stats (const int fd, struct map_session_data *sd,
                         const char *UNUSED, const char *message)
{
    int  idx, count, value = 0, new_value;
    short *status[] = {
        &sd->status.str, &sd->status.agi, &sd->status.vit,
        &sd->status.int_, &sd->status.dex, &sd->status.luk
    };

    if (!message || !*message || sscanf (message, "%d", &value) < 1
        || value == 0)
        value = battle_config.max_parameter;

    count = 0;
    for (idx = 0; idx < (int) (sizeof (status) / sizeof (status[0]));
         idx++)
    {

        new_value = (int) *status[idx] + value;
        if (value > 0 && (value > battle_config.max_parameter || new_value > battle_config.max_parameter))  // fix positiv overflow
            new_value = battle_config.max_parameter;
        else if (value < 0 && (value < -battle_config.max_parameter || new_value < 1))  // fix negativ overflow
            new_value = 1;

        if (new_value != (int) *status[idx])
        {
            *status[idx] = new_value;
            clif_updatestatus (sd, SP_STR + idx);
            clif_updatestatus (sd, SP_USTR + idx);
            pc_calcstatus (sd, 0);
            count++;
        }
    }

    if (count > 0)              // if at least 1 stat modified
        clif_displaymessage (fd, "All stats changed!");
    else
    {
        if (value < 0)
            clif_displaymessage (fd, "Impossible to decrease a stat.");
        else
            clif_displaymessage (fd, "Impossible to increase a stat.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_recall (const int fd, struct map_session_data *sd,
                      const char *UNUSED, const char *message)
{
    char character[100];
    char output[200];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @recall <char name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can recall only lower or same level
            if (maps[sd->bl.m].flag.nowarpto
                && battle_config.any_warp_GM_min_level > pc_isGM (sd))
            {
                clif_displaymessage (fd,
                                     "You are not authorised to warp somenone to your actual map.");
                return -1;
            }
            if (maps[pl_sd->bl.m].flag.nowarp
                && battle_config.any_warp_GM_min_level > pc_isGM (sd))
            {
                clif_displaymessage (fd,
                                     "You are not authorised to warp this player from its actual map.");
                return -1;
            }
            pc_setpos (pl_sd, sd->mapname, sd->bl.x, sd->bl.y, 2);
            sprintf (output, "%s recalled!", character);
            clif_displaymessage (fd, output);
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_revive (const int fd, struct map_session_data *sd,
                      const char *UNUSED, const char *message)
{
    char character[100];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @revive <char name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        pl_sd->status.hp = pl_sd->status.max_hp;
        pc_setstand (pl_sd);
        if (battle_config.pc_invincible_time > 0)
            pc_setinvincibletimer (sd, battle_config.pc_invincible_time);
        clif_updatestatus (pl_sd, SP_HP);
        clif_updatestatus (pl_sd, SP_SP);
        clif_resurrection (&pl_sd->bl, 1);
        clif_displaymessage (fd, "Character revived.");
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_character_stats (const int fd, struct map_session_data *UNUSED,
                               const char *UNUSED, const char *message)
{
    char character[100];
    char job_jobname[100];
    char output[200];
    struct map_session_data *pl_sd;
    int  i;

    memset (character, '\0', sizeof (character));
    memset (job_jobname, '\0', sizeof (job_jobname));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charstats <char name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        struct
        {
            const char *name;
            int  value;
        } output_table[] =
        {
            {"Base Level", pl_sd->status.base_level},
            {job_jobname, pl_sd->status.job_level},
            {"Hp", pl_sd->status.hp},
            {"MaxHp", pl_sd->status.max_hp},
            {"Sp", pl_sd->status.sp},
            {"MaxSp", pl_sd->status.max_sp},
            {"Str", pl_sd->status.str},
            {"Agi", pl_sd->status.agi},
            {"Vit", pl_sd->status.vit},
            {"Int", pl_sd->status.int_},
            {"Dex", pl_sd->status.dex},
            {"Luk", pl_sd->status.luk},
            {"Zeny", pl_sd->status.zeny},
            {NULL, 0}
        };
        sprintf (job_jobname, "Job - %s %s", "N/A",
                 "(level %d)");
                 sprintf (output, "'%s' stats:", pl_sd->status.name);
        clif_displaymessage (fd, output);
        for (i = 0; output_table[i].name; i++)
        {
            sprintf (output, "%s - %d", output_table[i].name, output_table[i].value);
            clif_displaymessage (fd, output);
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
//** Character Stats All by fritz
int atcommand_character_stats_all (const int fd, struct map_session_data *UNUSED,
                                   const char *UNUSED, const char *UNUSED)
{
    char output[1024], gmlevel[1024];
    int  i;
    int  count;
    struct map_session_data *pl_sd;

    memset (output, '\0', sizeof (output));
    memset (gmlevel, '\0', sizeof (gmlevel));

    count = 0;
    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth)
        {

            if (pc_isGM (pl_sd) > 0)
                sprintf (gmlevel, "| GM Lvl: %d", pc_isGM (pl_sd));
            else
                sprintf (gmlevel, " ");

            sprintf (output,
                     "Name: %s | BLvl: %d | Job: %s (Lvl: %d) | HP: %d/%d | SP: %d/%d",
                     pl_sd->status.name, pl_sd->status.base_level,
                     "N/A", pl_sd->status.job_level,
                     pl_sd->status.hp, pl_sd->status.max_hp, pl_sd->status.sp,
                     pl_sd->status.max_sp);
            clif_displaymessage (fd, output);
            sprintf (output,
                     "STR: %d | AGI: %d | VIT: %d | INT: %d | DEX: %d | LUK: %d | Zeny: %d %s",
                     pl_sd->status.str, pl_sd->status.agi, pl_sd->status.vit,
                     pl_sd->status.int_, pl_sd->status.dex, pl_sd->status.luk,
                     pl_sd->status.zeny, gmlevel);
            clif_displaymessage (fd, output);
            clif_displaymessage (fd, "--------");
            count++;
        }
    }

    if (count == 0)
        clif_displaymessage (fd, "No player found.");
    else if (count == 1)
        clif_displaymessage (fd, "1 player found.");
    else
    {
        sprintf (output, "%d players found.", count);
        clif_displaymessage (fd, output);
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_character_option (const int fd, struct map_session_data *sd,
                                const char *UNUSED, const char *message)
{
    char character[100];
    int  opt1 = 0, opt2 = 0, opt3 = 0;
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%d %d %d %99[^\n]", &opt1, &opt2, &opt3,
                   character) < 4 || opt1 < 0 || opt2 < 0 || opt3 < 0)
    {
        clif_displaymessage (fd,
                             "Please, enter valid options and a player name (usage: @charoption <param1> <param2> <param3> <charname>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can change option only to lower or same level
            pl_sd->opt1 = opt1;
            pl_sd->opt2 = opt2;
            pl_sd->status.option = opt3;
            // fix pecopeco display
            if (pc_isriding (pl_sd))
            {               // pl_sd have the new value...
                    pl_sd->status.option &= ~0x0020;
            }
            clif_changeoption (&pl_sd->bl);
            pc_calcstatus (pl_sd, 0);
            clif_displaymessage (fd, "Character's options changed.");
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * charchangesex command (usage: charchangesex <player_name>)
 *------------------------------------------
 */
int atcommand_char_change_sex (const int fd, struct map_session_data *sd,
                               const char *UNUSED, const char *message)
{
    char character[100];

    memset (character, '\0', sizeof (character));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charchangesex <name>).");
        return -1;
    }

    // check player name
    if (strlen (character) < 4)
    {
        clif_displaymessage (fd, "Sorry, but a player name have at least 4 characters.");
        return -1;
    }
    else if (strlen (character) > 23)
    {
        clif_displaymessage (fd, "Sorry, but a player name have 23 characters maximum.");
        return -1;
    }
    else
    {
        chrif_char_ask_name (sd->status.account_id, character, 5, 0, 0, 0, 0, 0, 0);    // type: 5 - changesex
        clif_displaymessage (fd, "Character name sends to char-server to ask it.");
    }

    return 0;
}

/*==========================================
 * charblock command (usage: charblock <player_name>)
 * This command do a definitiv ban on a player
 *------------------------------------------
 */
int atcommand_char_block (const int fd, struct map_session_data *sd,
                          const char *UNUSED, const char *message)
{
    char character[100];

    memset (character, '\0', sizeof (character));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @block <name>).");
        return -1;
    }

    // check player name
    if (strlen (character) < 4)
    {
        clif_displaymessage (fd, "Sorry, but a player name have at least 4 characters.");
        return -1;
    }
    else if (strlen (character) > 23)
    {
        clif_displaymessage (fd, "Sorry, but a player name have 23 characters maximum.");
        return -1;
    }
    else
    {
        chrif_char_ask_name (sd->status.account_id, character, 1, 0, 0, 0, 0, 0, 0);    // type: 1 - block
        clif_displaymessage (fd, "Character name sends to char-server to ask it.");
    }

    return 0;
}

/*==========================================
 * charban command (usage: charban <time> <player_name>)
 * This command do a limited ban on a player
 * Time is done as follows:
 *   Adjustment value (-1, 1, +1, etc...)
 *   Modified element:
 *     a or y: year
 *     m:  month
 *     j or d: day
 *     h:  hour
 *     mn: minute
 *     s:  second
 * <example> @ban +1m-2mn1s-6y test_player
 *           this example adds 1 month and 1 second, and substracts 2 minutes and 6 years at the same time.
 *------------------------------------------
 */
int atcommand_char_ban (const int fd, struct map_session_data *sd,
                        const char *UNUSED, const char *message)
{
    char modif[100], character[100];
    char *modif_p;
    int  year, month, day, hour, minute, second, value;

    memset (modif, '\0', sizeof (modif));
    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%s %99[^\n]", modif, character) < 2)
    {
        clif_displaymessage (fd,
                             "Please, enter ban time and a player name (usage: @charban/@ban/@banish/@charbanish <time> <name>).");
        return -1;
    }

    modif[sizeof (modif) - 1] = '\0';
    character[sizeof (character) - 1] = '\0';

    modif_p = modif;
    year = month = day = hour = minute = second = 0;
    while (modif_p[0] != '\0')
    {
        value = atoi (modif_p);
        if (value == 0)
            modif_p++;
        else
        {
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
                modif_p = modif_p + 2;
            }
            else if (modif_p[0] == 'h')
            {
                hour = value;
                modif_p++;
            }
            else if (modif_p[0] == 'd' || modif_p[0] == 'j')
            {
                day = value;
                modif_p++;
            }
            else if (modif_p[0] == 'm')
            {
                month = value;
                modif_p++;
            }
            else if (modif_p[0] == 'y' || modif_p[0] == 'a')
            {
                year = value;
                modif_p++;
            }
            else if (modif_p[0] != '\0')
            {
                modif_p++;
            }
        }
    }
    if (year == 0 && month == 0 && day == 0 && hour == 0 && minute == 0
        && second == 0)
    {
        clif_displaymessage (fd, "Invalid time for ban command.");
        return -1;
    }

    // check player name
    if (strlen (character) < 4)
    {
        clif_displaymessage (fd, "Sorry, but a player name have at least 4 characters.");
        return -1;
    }
    else if (strlen (character) > 23)
    {
        clif_displaymessage (fd, "Sorry, but a player name have 23 characters maximum.");
        return -1;
    }
    else
    {
        chrif_char_ask_name (sd->status.account_id, character, 2, year, month, day, hour, minute, second);  // type: 2 - ban
        clif_displaymessage (fd, "Character name sends to char-server to ask it.");
    }

    return 0;
}

/*==========================================
 * charunblock command (usage: charunblock <player_name>)
 *------------------------------------------
 */
int atcommand_char_unblock (const int fd, struct map_session_data *sd,
                            const char *UNUSED, const char *message)
{
    char character[100];

    memset (character, '\0', sizeof (character));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charunblock <player_name>).");
        return -1;
    }

    // check player name
    if (strlen (character) < 4)
    {
        clif_displaymessage (fd, "Sorry, but a player name have at least 4 characters.");
        return -1;
    }
    else if (strlen (character) > 23)
    {
        clif_displaymessage (fd, "Sorry, but a player name have 23 characters maximum.");
        return -1;
    }
    else
    {
        // send answer to login server via char-server
        chrif_char_ask_name (sd->status.account_id, character, 3, 0, 0, 0, 0, 0, 0);    // type: 3 - unblock
        clif_displaymessage (fd, "Character name sends to char-server to ask it.");
    }

    return 0;
}

/*==========================================
 * charunban command (usage: charunban <player_name>)
 *------------------------------------------
 */
int atcommand_char_unban (const int fd, struct map_session_data *sd,
                          const char *UNUSED, const char *message)
{
    char character[100];

    memset (character, '\0', sizeof (character));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charunban <player_name>).");
        return -1;
    }

    // check player name
    if (strlen (character) < 4)
    {
        clif_displaymessage (fd, "Sorry, but a player name have at least 4 characters.");
        return -1;
    }
    else if (strlen (character) > 23)
    {
        clif_displaymessage (fd, "Sorry, but a player name have 23 characters maximum.");
        return -1;
    }
    else
    {
        // send answer to login server via char-server
        chrif_char_ask_name (sd->status.account_id, character, 4, 0, 0, 0, 0, 0, 0);    // type: 4 - unban
        clif_displaymessage (fd, "Character name sends to char-server to ask it.");
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_character_save (const int fd, struct map_session_data *sd,
                              const char *UNUSED, const char *message)
{
    char map_name[100];
    char character[100];
    struct map_session_data *pl_sd;
    int  x = 0, y = 0;
    int  m;

    memset (map_name, '\0', sizeof (map_name));
    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%99s %d %d %99[^\n]", map_name, &x, &y,
                   character) < 4 || x < 0 || y < 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a valid save point and a player name (usage: @charsave <map> <x> <y> <charname>).");
        return -1;
    }

    if (strstr (map_name, ".gat") == NULL && strstr (map_name, ".afm") == NULL && strlen (map_name) < 13)   // 16 - 4 (.gat)
        strcat (map_name, ".gat");

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can change save point only to lower or same gm level
            m = map_mapname2mapid (map_name);
            if (m < 0)
            {
                clif_displaymessage (fd, "Map not found.");
                return -1;
            }
            else
            {
                if (m >= 0 && maps[m].flag.nowarpto
                    && battle_config.any_warp_GM_min_level > pc_isGM (sd))
                {
                    clif_displaymessage (fd,
                                         "You are not authorised to set this map as a save map.");
                    return -1;
                }
                pc_setsavepoint (pl_sd, map_name, x, y);
                clif_displaymessage (fd, "Character's respawn point changed.");
            }
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_night (const int fd, struct map_session_data *UNUSED,
                     const char *UNUSED, const char *UNUSED)
{
    struct map_session_data *pl_sd;
    int  i;

    if (night_flag != 1)
    {
        night_flag = 1;         // 0=day, 1=night [Yor]
        for (i = 0; i < fd_max; i++)
        {
            if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
                && pl_sd->state.auth)
            {
                pl_sd->opt2 |= STATE_BLIND;
                clif_changeoption (&pl_sd->bl);
                clif_displaymessage (pl_sd->fd, "Night has fallen.");
            }
        }
    }
    else
    {
        clif_displaymessage (fd, "Sorry, it's already the night. Impossible to execute the command.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_day (const int fd, struct map_session_data *UNUSED,
                   const char *UNUSED, const char *UNUSED)
{
    struct map_session_data *pl_sd;
    int  i;

    if (night_flag != 0)
    {
        night_flag = 0;         // 0=day, 1=night [Yor]
        for (i = 0; i < fd_max; i++)
        {
            if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
                && pl_sd->state.auth)
            {
                pl_sd->opt2 &= ~STATE_BLIND;
                clif_changeoption (&pl_sd->bl);
                clif_displaymessage (pl_sd->fd, "Day has arrived.");
            }
        }
    }
    else
    {
        clif_displaymessage (fd, "Sorry, it's already the day. Impossible to execute the command.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_doom (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *UNUSED)
{
    struct map_session_data *pl_sd;
    int  i;

    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth && i != fd
            && pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can doom only lower or same gm level
            pc_damage (NULL, pl_sd, pl_sd->status.hp + 1);
            clif_displaymessage (pl_sd->fd, "The holy messenger has given judgement.");
        }
    }
    clif_displaymessage (fd, "Judgement was made.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_doommap (const int fd, struct map_session_data *sd,
                       const char *UNUSED, const char *UNUSED)
{
    struct map_session_data *pl_sd;
    int  i;

    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth && i != fd && sd->bl.m == pl_sd->bl.m
            && pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can doom only lower or same gm level
            pc_damage (NULL, pl_sd, pl_sd->status.hp + 1);
            clif_displaymessage (pl_sd->fd, "The holy messenger has given judgement.");
        }
    }
    clif_displaymessage (fd, "Judgement was made.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
static void atcommand_raise_sub (struct map_session_data *sd)
{
    if (sd && sd->state.auth && pc_isdead (sd))
    {
        sd->status.hp = sd->status.max_hp;
        sd->status.sp = sd->status.max_sp;
        pc_setstand (sd);
        clif_updatestatus (sd, SP_HP);
        clif_updatestatus (sd, SP_SP);
        clif_resurrection (&sd->bl, 1);
        clif_displaymessage (sd->fd, "Mercy has been shown.");
    }
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_raise (const int fd, struct map_session_data *UNUSED,
                     const char *UNUSED, const char *UNUSED)
{
    int  i;

    for (i = 0; i < fd_max; i++)
    {
        if (session[i])
            atcommand_raise_sub ((struct map_session_data *)session[i]->session_data);
    }
    clif_displaymessage (fd, "Mercy has been granted.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_raisemap (const int fd, struct map_session_data *sd,
                        const char *UNUSED, const char *UNUSED)
{
    struct map_session_data *pl_sd;
    int  i;

    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth && sd->bl.m == pl_sd->bl.m)
            atcommand_raise_sub (pl_sd);
    }
    clif_displaymessage (fd, "Mercy has been granted.");

    return 0;
}

/*==========================================
 * atcommand_character_baselevel @charbaselvlで対象キャラのレベルを上げる
 *------------------------------------------
*/
int atcommand_character_baselevel (const int fd, struct map_session_data *sd,
                                   const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd;
    char character[100];
    int  level = 0, i;

    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%d %99[^\n]", &level, character) < 2
        || level == 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a level adjustement and a player name (usage: @charblvl <#> <nickname>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can change base level only lower or same gm level

            if (level > 0)
            {
                if (pl_sd->status.base_level == battle_config.maximum_level)
                {               // check for max level by Valaris
                    clif_displaymessage (fd, "Character's base level can't go any higher.");
                    return 0;
                }               // End Addition
                if (level > battle_config.maximum_level || level > (battle_config.maximum_level - pl_sd->status.base_level))    // fix positiv overflow
                    level =
                        battle_config.maximum_level -
                        pl_sd->status.base_level;
                for (i = 1; i <= level; i++)
                    pl_sd->status.status_point +=
                        (pl_sd->status.base_level + i + 14) / 4;
                pl_sd->status.base_level += level;
                clif_updatestatus (pl_sd, SP_BASELEVEL);
                clif_updatestatus (pl_sd, SP_NEXTBASEEXP);
                clif_updatestatus (pl_sd, SP_STATUSPOINT);
                pc_calcstatus (pl_sd, 0);
                pc_heal (pl_sd, pl_sd->status.max_hp, pl_sd->status.max_sp);
                clif_misceffect (&pl_sd->bl, 0);
                clif_displaymessage (fd, "Character's base level raised.");
            }
            else
            {
                if (pl_sd->status.base_level == 1)
                {
                    clif_displaymessage (fd, "Character's base level can't go any lower.");
                    return -1;
                }
                if (level < -battle_config.maximum_level || level < (1 - pl_sd->status.base_level)) // fix negativ overflow
                    level = 1 - pl_sd->status.base_level;
                if (pl_sd->status.status_point > 0)
                {
                    for (i = 0; i > level; i--)
                        pl_sd->status.status_point -=
                            (pl_sd->status.base_level + i + 14) / 4;
                    if (pl_sd->status.status_point < 0)
                        pl_sd->status.status_point = 0;
                    clif_updatestatus (pl_sd, SP_STATUSPOINT);
                }               // to add: remove status points from stats
                pl_sd->status.base_level += level;
                pl_sd->status.base_exp = 0;
                clif_updatestatus (pl_sd, SP_BASELEVEL);
                clif_updatestatus (pl_sd, SP_NEXTBASEEXP);
                clif_updatestatus (pl_sd, SP_BASEEXP);
                pc_calcstatus (pl_sd, 0);
                clif_displaymessage (fd, "Character's base level lowered.");
            }
	    // Reset their stat points to prevent extra points from stacking
	    atcommand_charstreset(fd, sd,"@charstreset", character);
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;                   //正常終了
}

/*==========================================
 * atcommand_character_joblevel @charjoblvlで対象キャラのJobレベルを上げる
 *------------------------------------------
 */
int atcommand_character_joblevel (const int fd, struct map_session_data *sd,
                                  const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd;
    char character[100];
    int  max_level = 50, level = 0;

    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%d %99[^\n]", &level, character) < 2
        || level == 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a level adjustement and a player name (usage: @charjlvl <#> <nickname>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can change job level only lower or same gm level
            max_level -= 40;

            if (level > 0)
            {
                if (pl_sd->status.job_level == max_level)
                {
                    clif_displaymessage (fd, "Character's job level can't go any higher.");
                    return -1;
                }
                if (pl_sd->status.job_level + level > max_level)
                    level = max_level - pl_sd->status.job_level;
                pl_sd->status.job_level += level;
                clif_updatestatus (pl_sd, SP_JOBLEVEL);
                clif_updatestatus (pl_sd, SP_NEXTJOBEXP);
                pl_sd->status.skill_point += level;
                clif_updatestatus (pl_sd, SP_SKILLPOINT);
                pc_calcstatus (pl_sd, 0);
                clif_misceffect (&pl_sd->bl, 1);
                clif_displaymessage (fd, "character's job level raised.");
            }
            else
            {
                if (pl_sd->status.job_level == 1)
                {
                    clif_displaymessage (fd, "Character's job level can't go any lower.");
                    return -1;
                }
                if (pl_sd->status.job_level + level < 1)
                    level = 1 - pl_sd->status.job_level;
                pl_sd->status.job_level += level;
                clif_updatestatus (pl_sd, SP_JOBLEVEL);
                clif_updatestatus (pl_sd, SP_NEXTJOBEXP);
                if (pl_sd->status.skill_point > 0)
                {
                    pl_sd->status.skill_point += level;
                    if (pl_sd->status.skill_point < 0)
                        pl_sd->status.skill_point = 0;
                    clif_updatestatus (pl_sd, SP_SKILLPOINT);
                }               // to add: remove status points from skills
                pc_calcstatus (pl_sd, 0);
                clif_displaymessage (fd, "Character's job level lowered.");
            }
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_kick (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd;
    char character[100];

    memset (character, '\0', sizeof (character));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @kick <charname>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))    // you can kick only lower or same gm level
            clif_GM_kick (sd, pl_sd, 1);
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_kickall (const int fd, struct map_session_data *sd,
                       const char *UNUSED, const char *UNUSED)
{
    struct map_session_data *pl_sd;
    int  i;

    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth && pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can kick only lower or same gm level
            if (sd->status.account_id != pl_sd->status.account_id)
                clif_GM_kick (sd, pl_sd, 0);
        }
    }

    clif_displaymessage (fd, "All players have been kicked!");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_allskills (const int fd, struct map_session_data *sd,
                         const char *UNUSED, const char *UNUSED)
{
    pc_allskillup (sd);         // all skills
    sd->status.skill_point = 0; // 0 skill points
    clif_updatestatus (sd, SP_SKILLPOINT);  // update
    clif_displaymessage (fd, "You have received all skills.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_questskill (const int fd, struct map_session_data *sd,
                          const char *UNUSED, const char *message)
{
    int  skill_id;

    if (!message || !*message || (skill_id = atoi (message)) < 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a quest skill number (usage: @questskill <#:0+>).");
        return -1;
    }

    if (skill_id >= 0 && skill_id < MAX_SKILL_DB)
    {
        if (skill_get_inf2 (skill_id) & 0x01)
        {
            if (pc_checkskill (sd, skill_id) == 0)
            {
                pc_skill (sd, skill_id, 1, 0);
                clif_displaymessage (fd, "You have learned the skill.");
            }
            else
            {
                clif_displaymessage (fd, "You already have this quest skill.");
                return -1;
            }
        }
        else
        {
            clif_displaymessage (fd, "This skill number doesn't exist or isn't a quest skill.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "This skill number doesn't exist.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_charquestskill (const int fd, struct map_session_data *UNUSED,
                              const char *UNUSED, const char *message)
{
    char character[100];
    struct map_session_data *pl_sd;
    int  skill_id = 0;

    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%d %99[^\n]", &skill_id, character) < 2
        || skill_id < 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a quest skill number and a player name (usage: @charquestskill <#:0+> <char_name>).");
        return -1;
    }

    if (skill_id >= 0 && skill_id < MAX_SKILL_DB)
    {
        if (skill_get_inf2 (skill_id) & 0x01)
        {
            if ((pl_sd = map_nick2sd (character)) != NULL)
            {
                if (pc_checkskill (pl_sd, skill_id) == 0)
                {
                    pc_skill (pl_sd, skill_id, 1, 0);
                    clif_displaymessage (fd, "This player has learned the skill.");
                }
                else
                {
                    clif_displaymessage (fd, "This player already has this quest skill.");
                    return -1;
                }
            }
            else
            {
                clif_displaymessage (fd, "Character not found.");
                return -1;
            }
        }
        else
        {
            clif_displaymessage (fd, "This skill number doesn't exist or isn't a quest skill.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "This skill number doesn't exist.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_lostskill (const int fd, struct map_session_data *sd,
                         const char *UNUSED, const char *message)
{
    int  skill_id;

    if (!message || !*message || (skill_id = atoi (message)) < 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a quest skill number (usage: @lostskill <#:0+>).");
        return -1;
    }

    if (skill_id >= 0 && skill_id < MAX_SKILL)
    {
        if (skill_get_inf2 (skill_id) & 0x01)
        {
            if (pc_checkskill (sd, skill_id) > 0)
            {
                sd->status.skill[skill_id].lv = 0;
                sd->status.skill[skill_id].flags = 0;
                clif_skillinfoblock (sd);
                clif_displaymessage (fd, "You have forgotten the skill.");
            }
            else
            {
                clif_displaymessage (fd, "You don't have this quest skill.");
                return -1;
            }
        }
        else
        {
            clif_displaymessage (fd, "This skill number doesn't exist or isn't a quest skill.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "This skill number doesn't exist.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_charlostskill (const int fd, struct map_session_data *UNUSED,
                             const char *UNUSED, const char *message)
{
    char character[100];
    struct map_session_data *pl_sd;
    int  skill_id = 0;

    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%d %99[^\n]", &skill_id, character) < 2
        || skill_id < 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a quest skill number and a player name (usage: @charlostskill <#:0+> <char_name>).");
        return -1;
    }

    if (skill_id >= 0 && skill_id < MAX_SKILL)
    {
        if (skill_get_inf2 (skill_id) & 0x01)
        {
            if ((pl_sd = map_nick2sd (character)) != NULL)
            {
                if (pc_checkskill (pl_sd, skill_id) > 0)
                {
                    pl_sd->status.skill[skill_id].lv = 0;
                    pl_sd->status.skill[skill_id].flags = 0;
                    clif_skillinfoblock (pl_sd);
                    clif_displaymessage (fd, "This player has forgotten the skill.");
                }
                else
                {
                    clif_displaymessage (fd, "This player doesn't have this quest skill.");
                    return -1;
                }
            }
            else
            {
                clif_displaymessage (fd, "Character not found.");
                return -1;
            }
        }
        else
        {
            clif_displaymessage (fd, "This skill number doesn't exist or isn't a quest skill.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "This skill number doesn't exist.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_party (const int fd, struct map_session_data *sd,
                     const char *UNUSED, const char *message)
{
    char party[100];

    memset (party, '\0', sizeof (party));

    if (!message || !*message || sscanf (message, "%99[^\n]", party) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a party name (usage: @party <party_name>).");
        return -1;
    }

    party_create (sd, party);

    return 0;
}

/*==========================================
 * @mapexitでマップサーバーを終了させる
 *------------------------------------------
 */
int atcommand_mapexit (const int UNUSED, struct map_session_data *sd,
                       const char *UNUSED, const char *UNUSED)
{
    struct map_session_data *pl_sd;
    int  i;

    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth)
        {
            if (sd->status.account_id != pl_sd->status.account_id)
                clif_GM_kick (sd, pl_sd, 0);
        }
    }
    clif_GM_kick (sd, sd, 0);

    runflag = 0;

    return 0;
}

/*==========================================
 * idsearch <part_of_name>: revrited by [Yor]
 *------------------------------------------
 */
int atcommand_idsearch (const int fd, struct map_session_data *UNUSED,
                        const char *UNUSED, const char *message)
{
    char item_name[100];
    char output[200];
    int  i, match;
    struct item_data *item;

    memset (item_name, '\0', sizeof (item_name));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99s", item_name) < 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a part of item name (usage: @idsearch <part_of_item_name>).");
        return -1;
    }

    sprintf (output, "The reference result of '%s' (name: id):", item_name);
    clif_displaymessage (fd, output);
    match = 0;
    for (i = 0; i < 20000; i++)
    {
        if ((item = itemdb_exists (i)) != NULL
            && strstr (item->jname, item_name) != NULL)
        {
            match++;
            sprintf (output, "%s: %d", item->jname, item->nameid);
            clif_displaymessage (fd, output);
        }
    }
    sprintf (output, "It is %d affair above.", match);
    clif_displaymessage (fd, output);

    return 0;
}

/*==========================================
 * Character Skill Reset
 *------------------------------------------
 */
int atcommand_charskreset (const int fd, struct map_session_data *sd,
                           const char *UNUSED, const char *message)
{
    char character[100];
    char output[200];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charskreset <charname>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can reset skill points only lower or same gm level
            pc_resetskill (pl_sd);
            sprintf (output, "'%s' skill points reseted!", character);
            clif_displaymessage (fd, output);
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * Character Stat Reset
 *------------------------------------------
 */
int atcommand_charstreset (const int fd, struct map_session_data *sd,
                           const char *UNUSED, const char *message)
{
    char character[100];
    char output[200];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charstreset <charname>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can reset stats points only lower or same gm level
            pc_resetstate (pl_sd);
            sprintf (output, "'%s' stats points reseted!", character);
            clif_displaymessage (fd, output);
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * Character Reset
 *------------------------------------------
 */
int atcommand_charreset (const int fd, struct map_session_data *sd,
                         const char *UNUSED, const char *message)
{
    char character[100];
    char output[200];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charreset <charname>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can reset a character only for lower or same GM level
            pc_resetstate (pl_sd);
            pc_resetskill (pl_sd);
            pc_setglobalreg (pl_sd, "MAGIC_FLAGS", 0);  // [Fate] Reset magic quest variables
            pc_setglobalreg (pl_sd, "MAGIC_EXP", 0);    // [Fate] Reset magic experience
            sprintf (output, "'%s' skill and stats points reseted!", character);
            clif_displaymessage (fd, output);
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * Character Wipe
 *------------------------------------------
 */
int atcommand_char_wipe (const int fd, struct map_session_data *sd,
                         const char *UNUSED, const char *message)
{
    char character[100];
    char output[200];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charwipe <charname>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can reset a character only for lower or same GM level
            int  i;

            // Reset base level
            pl_sd->status.base_level = 1;
            pl_sd->status.base_exp = 0;
            clif_updatestatus (pl_sd, SP_BASELEVEL);
            clif_updatestatus (pl_sd, SP_NEXTBASEEXP);
            clif_updatestatus (pl_sd, SP_BASEEXP);

            // Reset job level
            pl_sd->status.job_level = 1;
            pl_sd->status.job_exp = 0;
            clif_updatestatus (pl_sd, SP_JOBLEVEL);
            clif_updatestatus (pl_sd, SP_NEXTJOBEXP);
            clif_updatestatus (pl_sd, SP_JOBEXP);

            // Zeny to 50
            pl_sd->status.zeny = 50;
            clif_updatestatus (pl_sd, SP_ZENY);

            // Clear inventory
            for (i = 0; i < MAX_INVENTORY; i++)
            {
                if (sd->status.inventory[i].amount)
                {
                    if (sd->status.inventory[i].equip)
                        pc_unequipitem (pl_sd, i, 0);
                    pc_delitem (pl_sd, i, sd->status.inventory[i].amount, 0);
                }
            }

            // Give knife and shirt
            struct item item;
            item.nameid = 1201; // knife
            item.identify = 1;
            item.broken = 0;
            pc_additem (pl_sd, &item, 1);
            item.nameid = 1202; // shirt
            pc_additem (pl_sd, &item, 1);

            // Reset stats and skills
            pc_calcstatus (pl_sd, 0);
            pc_resetstate (pl_sd);
            pc_resetskill (pl_sd);
            pc_setglobalreg (pl_sd, "MAGIC_FLAGS", 0);  // [Fate] Reset magic quest variables
            pc_setglobalreg (pl_sd, "MAGIC_EXP", 0);    // [Fate] Reset magic experience

            sprintf (output, "%s:  wiped.", character); // '%s' skill and stats points reseted!
            clif_displaymessage (fd, output);
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * Character Model by chbrules
 *------------------------------------------
 */
int atcommand_charmodel (const int fd, struct map_session_data *UNUSED,
                         const char *UNUSED, const char *message)
{
    int  hair_style = 0, hair_color = 0, cloth_color = 0;
    struct map_session_data *pl_sd;
    char character[100];
    char output[200];

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));

    if (!message || !*message
        || sscanf (message, "%d %d %d %99[^\n]", &hair_style, &hair_color,
                   &cloth_color, character) < 4 || hair_style < 0
        || hair_color < 0 || cloth_color < 0)
    {
        sprintf (output,
                 "Please, enter a valid model and a player name (usage: @charmodel <hair ID: %d-%d> <hair color: %d-%d> <clothes color: %d-%d> <name>).",
                 0, NUM_HAIR_STYLES-1, 0,
                 NUM_HAIR_COLORS-1, 0, NUM_CLOTHES_COLORS-1);
        clif_displaymessage (fd, output);
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (hair_style >= 0 && hair_style <= NUM_HAIR_STYLES &&
            hair_color >= 0 && hair_color <= NUM_HAIR_COLORS &&
            cloth_color >= 0 && cloth_color <= NUM_CLOTHES_COLORS)
        {
            pc_changelook (pl_sd, LOOK_HAIR, hair_style);
            pc_changelook (pl_sd, LOOK_HAIR_COLOR, hair_color);
            pc_changelook (pl_sd, LOOK_CLOTHES_COLOR, cloth_color);
            clif_displaymessage (fd, "Appearance changed.");
        }
        else
        {
            clif_displaymessage (fd, "An invalid number was specified.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * Character Skill Point (Rewritten by [Yor])
 *------------------------------------------
 */
int atcommand_charskpoint (const int fd, struct map_session_data *UNUSED,
                           const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd;
    char character[100];
    int  new_skill_point;
    int  point = 0;

    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%d %99[^\n]", &point, character) < 2
        || point == 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a number and a player name (usage: @charskpoint <amount> <name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        new_skill_point = (int) pl_sd->status.skill_point + point;
        if (point > 0 && (point > 0x7FFF || new_skill_point > 0x7FFF))  // fix positiv overflow
            new_skill_point = 0x7FFF;
        else if (point < 0 && (point < -0x7FFF || new_skill_point < 0)) // fix negativ overflow
            new_skill_point = 0;
        if (new_skill_point != (int) pl_sd->status.skill_point)
        {
            pl_sd->status.skill_point = new_skill_point;
            clif_updatestatus (pl_sd, SP_SKILLPOINT);
            clif_displaymessage (fd, "Character's number of skill points changed!");
        }
        else
        {
            if (point < 0)
                clif_displaymessage (fd, "Impossible to decrease the number/value.");
            else
                clif_displaymessage (fd, "Impossible to increase the number/value.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * Character Status Point (rewritten by [Yor])
 *------------------------------------------
 */
int atcommand_charstpoint (const int fd, struct map_session_data *UNUSED,
                           const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd;
    char character[100];
    int  new_status_point;
    int  point = 0;

    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%d %99[^\n]", &point, character) < 2
        || point == 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a number and a player name (usage: @charstpoint <amount> <name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        new_status_point = (int) pl_sd->status.status_point + point;
        if (point > 0 && (point > 0x7FFF || new_status_point > 0x7FFF)) // fix positiv overflow
            new_status_point = 0x7FFF;
        else if (point < 0 && (point < -0x7FFF || new_status_point < 0))    // fix negativ overflow
            new_status_point = 0;
        if (new_status_point != (int) pl_sd->status.status_point)
        {
            pl_sd->status.status_point = new_status_point;
            clif_updatestatus (pl_sd, SP_STATUSPOINT);
            clif_displaymessage (fd, "Character's number of status points changed!");
        }
        else
        {
            if (point < 0)
                clif_displaymessage (fd, "Impossible to decrease the number/value.");
            else
                clif_displaymessage (fd, "Impossible to increase the number/value.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * Character Zeny Point (Rewritten by [Yor])
 *------------------------------------------
 */
int atcommand_charzeny (const int fd, struct map_session_data *UNUSED,
                        const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd;
    char character[100];
    int  zeny = 0, new_zeny;

    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%d %99[^\n]", &zeny, character) < 2 || zeny == 0)
    {
        clif_displaymessage (fd,
                             "Please, enter a number and a player name (usage: @charzeny <zeny> <name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        new_zeny = pl_sd->status.zeny + zeny;
        if (zeny > 0 && (zeny > MAX_ZENY || new_zeny > MAX_ZENY))   // fix positiv overflow
            new_zeny = MAX_ZENY;
        else if (zeny < 0 && (zeny < -MAX_ZENY || new_zeny < 0))    // fix negativ overflow
            new_zeny = 0;
        if (new_zeny != pl_sd->status.zeny)
        {
            pl_sd->status.zeny = new_zeny;
            clif_updatestatus (pl_sd, SP_ZENY);
            clif_displaymessage (fd, "Character's number of zenys changed!");
        }
        else
        {
            if (zeny < 0)
                clif_displaymessage (fd, "Impossible to decrease the number/value.");
            else
                clif_displaymessage (fd, "Impossible to increase the number/value.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * Recall All Characters Online To Your Location
 *------------------------------------------
 */
int atcommand_recallall (const int fd, struct map_session_data *sd,
                         const char *UNUSED, const char *UNUSED)
{
    struct map_session_data *pl_sd;
    int  i;
    int  count;
    char output[200];

    memset (output, '\0', sizeof (output));

    if (maps[sd->bl.m].flag.nowarpto
        && battle_config.any_warp_GM_min_level > pc_isGM (sd))
    {
        clif_displaymessage (fd,
                             "You are not authorised to warp somenone to your actual map.");
        return -1;
    }

    count = 0;
    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth
            && sd->status.account_id != pl_sd->status.account_id
            && pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can recall only lower or same level
            if (maps[pl_sd->bl.m].flag.nowarp
                && battle_config.any_warp_GM_min_level > pc_isGM (sd))
                count++;
            else
                pc_setpos (pl_sd, sd->mapname, sd->bl.x, sd->bl.y, 2);
        }
    }

    clif_displaymessage (fd, "All characters recalled!");
    if (count)
    {
        sprintf (output,
                 "Because you are not authorised to warp from some maps, %d player(s) have not been recalled.",
                 count);
        clif_displaymessage (fd, output);
    }

    return 0;
}

/*==========================================
 * Recall online characters of a party to your location
 *------------------------------------------
 */
int atcommand_partyrecall (const int fd, struct map_session_data *sd,
                           const char *UNUSED, const char *message)
{
    int  i;
    struct map_session_data *pl_sd;
    char party_name[100];
    char output[200];
    struct party *p;
    int  count;

    memset (party_name, '\0', sizeof (party_name));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99[^\n]", party_name) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a party name/id (usage: @partyrecall <party_name/id>).");
        return -1;
    }

    if (maps[sd->bl.m].flag.nowarpto
        && battle_config.any_warp_GM_min_level > pc_isGM (sd))
    {
        clif_displaymessage (fd,
                             "You are not authorised to warp somenone to your actual map.");
        return -1;
    }

    if ((p = party_searchname (party_name)) != NULL ||  // name first to avoid error when name begin with a number
        (p = party_search (atoi (message))) != NULL)
    {
        count = 0;
        for (i = 0; i < fd_max; i++)
        {
            if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
                && pl_sd->state.auth
                && sd->status.account_id != pl_sd->status.account_id
                && pl_sd->status.party_id == p->party_id)
            {
                if (maps[pl_sd->bl.m].flag.nowarp
                    && battle_config.any_warp_GM_min_level > pc_isGM (sd))
                    count++;
                else
                    pc_setpos (pl_sd, sd->mapname, sd->bl.x, sd->bl.y, 2);
            }
        }
        sprintf (output, "All online characters of the %s party are near you.", p->name);
        clif_displaymessage (fd, output);
        if (count)
        {
            sprintf (output,
                     "Because you are not authorised to warp from some maps, %d player(s) have not been recalled.",
                     count);
            clif_displaymessage (fd, output);
        }
    }
    else
    {
        clif_displaymessage (fd, "Incorrect name or ID, or no one from the party is online.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_reloaditemdb (const int fd, struct map_session_data *UNUSED,
                            const char *UNUSED, const char *UNUSED)
{
    itemdb_reload ();
    clif_displaymessage (fd, "Item database reloaded.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_reloadmobdb (const int fd, struct map_session_data *UNUSED,
                           const char *UNUSED, const char *UNUSED)
{
    mob_reload ();
    clif_displaymessage (fd, "Monster database reloaded.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_reloadskilldb (const int fd, struct map_session_data *UNUSED,
                             const char *UNUSED, const char *UNUSED)
{
    skill_reload ();
    clif_displaymessage (fd, "Skill database reloaded.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_reloadscript (const int fd, struct map_session_data *UNUSED,
                            const char *UNUSED, const char *UNUSED)
{
    do_init_npc ();
    do_init_script ();

    npc_event_do_oninit ();

    clif_displaymessage (fd, "Scripts reloaded.");

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_reloadgmdb (      // by [Yor]
                             const int fd, struct map_session_data *UNUSED,
                             const char *UNUSED, const char *UNUSED)
{
    chrif_reloadGMdb ();

    clif_displaymessage (fd, "Login-server asked to reload GM accounts and their level.");

    return 0;
}

/*==========================================
 * @mapinfo <map name> [0-2] by MC_Cameri
 * => Shows information about the map [map name]
 * 0 = no additional information
 * 1 = Show users in that map and their location
 * 2 = Shows NPCs in that map
 * 3 = Shows the shops/chats in that map (not implemented)
 *------------------------------------------
 */
int atcommand_mapinfo (const int fd, struct map_session_data *sd,
                       const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd;
    struct npc_data *nd = NULL;
    char output[200], map_name[100];
    char direction[12];
    int  m_id, i, list = 0;

    memset (output, '\0', sizeof (output));
    memset (map_name, '\0', sizeof (map_name));
    memset (direction, '\0', sizeof (direction));

    sscanf (message, "%d %99[^\n]", &list, map_name);

    if (list < 0 || list > 2)
    {
        clif_displaymessage (fd,
                             "Please, enter at least a valid list number (usage: @mapinfo <0-2> [map]).");
        return -1;
    }

    if (map_name[0] == '\0')
        strcpy (map_name, sd->mapname);
    if (strstr (map_name, ".gat") == NULL && strstr (map_name, ".afm") == NULL && strlen (map_name) < 13)   // 16 - 4 (.gat)
        strcat (map_name, ".gat");

    if ((m_id = map_mapname2mapid (map_name)) < 0)
    {
        clif_displaymessage (fd, "Map not found.");
        return -1;
    }

    clif_displaymessage (fd, "------ Map Info ------");
    sprintf (output, "Map Name: %s", map_name);
    clif_displaymessage (fd, output);
    sprintf (output, "Players In Map: %d", maps[m_id].users);
    clif_displaymessage (fd, output);
    sprintf (output, "NPCs In Map: %d", maps[m_id].npc_num);
    clif_displaymessage (fd, output);
    clif_displaymessage (fd, "------ Map Flags ------");
    sprintf (output, "Player vs Player: %s | No Party: %s",
             (maps[m_id].flag.pvp) ? "True" : "False",
             (maps[m_id].flag.pvp_noparty) ? "True" : "False");
    clif_displaymessage (fd, output);
    sprintf (output, "No Dead Branch: %s",
             (maps[m_id].flag.nobranch) ? "True" : "False");
    clif_displaymessage (fd, output);
    sprintf (output, "No Memo: %s",
             (maps[m_id].flag.nomemo) ? "True" : "False");
    clif_displaymessage (fd, output);
    sprintf (output, "No Penalty: %s",
             (maps[m_id].flag.nopenalty) ? "True" : "False");
    clif_displaymessage (fd, output);
    sprintf (output, "No Return: %s",
             (maps[m_id].flag.noreturn) ? "True" : "False");
    clif_displaymessage (fd, output);
    sprintf (output, "No Save: %s",
             (maps[m_id].flag.nosave) ? "True" : "False");
    clif_displaymessage (fd, output);
    sprintf (output, "No Teleport: %s",
             (maps[m_id].flag.noteleport) ? "True" : "False");
    clif_displaymessage (fd, output);
    sprintf (output, "No Monster Teleport: %s",
             (maps[m_id].flag.monster_noteleport) ? "True" : "False");
    clif_displaymessage (fd, output);
    sprintf (output, "No Zeny Penalty: %s",
             (maps[m_id].flag.nozenypenalty) ? "True" : "False");
    clif_displaymessage (fd, output);

    switch (list)
    {
        case 0:
            // Do nothing. It's list 0, no additional display.
            break;
        case 1:
            clif_displaymessage (fd, "----- Players in Map -----");
            for (i = 0; i < fd_max; i++)
            {
                if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
                    && pl_sd->state.auth
                    && strcmp (pl_sd->mapname, map_name) == 0)
                {
                    sprintf (output,
                             "Player '%s' (session #%d) | Location: %d,%d",
                             pl_sd->status.name, i, pl_sd->bl.x, pl_sd->bl.y);
                    clif_displaymessage (fd, output);
                }
            }
            break;
        case 2:
            clif_displaymessage (fd, "----- NPCs in Map -----");
            for (i = 0; i < maps[m_id].npc_num;)
            {
                nd = maps[m_id].npc[i];
                switch (nd->dir)
                {
                    case 0:
                        strcpy (direction, "North");
                        break;
                    case 1:
                        strcpy (direction, "North West");
                        break;
                    case 2:
                        strcpy (direction, "West");
                        break;
                    case 3:
                        strcpy (direction, "South West");
                        break;
                    case 4:
                        strcpy (direction, "South");
                        break;
                    case 5:
                        strcpy (direction, "South East");
                        break;
                    case 6:
                        strcpy (direction, "East");
                        break;
                    case 7:
                        strcpy (direction, "North East");
                        break;
                    case 9:
                        strcpy (direction, "North");
                        break;
                    default:
                        strcpy (direction, "Unknown");
                        break;
                }
                sprintf (output,
                         "NPC %d: %s | Direction: %s | Sprite: %d | Location: %d %d",
                         ++i, nd->name, direction, nd->npc_class, nd->bl.x,
                         nd->bl.y);
                clif_displaymessage (fd, output);
            }
            break;
        default:               // normally impossible to arrive here
            clif_displaymessage (fd,
                                 "Please, enter at least a valid list number (usage: @mapinfo <0-2> [map]).");
            return -1;
            break;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_mount_peco (const int fd, struct map_session_data *sd,
                          const char *UNUSED, const char *UNUSED)
{
    if (sd->disguise > 0)
    {                           // temporary prevention of crash caused by peco + disguise, will look into a better solution [Valaris]
        clif_displaymessage (fd, "Cannot mount a Peco while in disguise.");
        return -1;
    }

    if (!pc_isriding (sd))
    {                           // if actually no peco
        clif_displaymessage (fd, "You can not mount a peco with your job.");
        return -1;
    }
    else
    {
        pc_setoption (sd, sd->status.option & ~0x0020);
        clif_displaymessage (fd, "Unmounted Peco.");
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_char_mount_peco (const int fd, struct map_session_data *UNUSED,
                               const char *UNUSED, const char *message)
{
    char character[100];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charmountpeco <char_name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pl_sd->disguise > 0)
        {                       // temporary prevention of crash caused by peco + disguise, will look into a better solution [Valaris]
            clif_displaymessage (fd, "This player cannot mount a Peco while in disguise.");
            return -1;
        }

        if (!pc_isriding (pl_sd))
        {                       // if actually no peco
            clif_displaymessage (fd, "This player can not mount a peco with his/her job.");
            return -1;
        }
        else
        {
            pc_setoption (pl_sd, pl_sd->status.option & ~0x0020);
            clif_displaymessage (fd, "Now, this player has not more peco.");
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_partyspy (const int fd, struct map_session_data *sd,
                        const char *UNUSED, const char *message)
{
    char party_name[100];
    char output[200];
    struct party *p;

    memset (party_name, '\0', sizeof (party_name));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99[^\n]", party_name) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a party name/id (usage: @partyspy <party_name/id>).");
        return -1;
    }

    if ((p = party_searchname (party_name)) != NULL ||  // name first to avoid error when name begin with a number
        (p = party_search (atoi (message))) != NULL)
    {
        if (sd->partyspy == p->party_id)
        {
            sd->partyspy = 0;
            sprintf (output, "No longer spying on the %s party.", p->name);
            clif_displaymessage (fd, output);
        }
        else
        {
            sd->partyspy = p->party_id;
            sprintf (output, "Spying on the %s party.", p->name);
            clif_displaymessage (fd, output);
        }
    }
    else
    {
        clif_displaymessage (fd, "Incorrect name or ID, or no one from the party is online.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_enablenpc (const int fd, struct map_session_data *UNUSED,
                         const char *UNUSED, const char *message)
{
    char NPCname[100];

    memset (NPCname, '\0', sizeof (NPCname));

    if (!message || !*message || sscanf (message, "%99[^\n]", NPCname) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a NPC name (usage: @npcon <NPC_name>).");
        return -1;
    }

    if (npc_name2id (NPCname) != NULL)
    {
        npc_enable (NPCname, 1);
        clif_displaymessage (fd, "Npc Enabled.");
    }
    else
    {
        clif_displaymessage (fd, "This NPC doesn't exist.");
        return -1;
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_disablenpc (const int fd, struct map_session_data *UNUSED,
                          const char *UNUSED, const char *message)
{
    char NPCname[100];

    memset (NPCname, '\0', sizeof (NPCname));

    if (!message || !*message || sscanf (message, "%99[^\n]", NPCname) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a NPC name (usage: @npcoff <NPC_name>).");
        return -1;
    }

    if (npc_name2id (NPCname) != NULL)
    {
        npc_enable (NPCname, 0);
        clif_displaymessage (fd, "Npc Disabled.");
    }
    else
    {
        clif_displaymessage (fd, "This NPC doesn't exist.");
        return -1;
    }

    return 0;
}

/*==========================================
 * time in txt for time command (by [Yor])
 *------------------------------------------
 */
static char *txt_time (unsigned int duration)
{
    int  days, hours, minutes, seconds;
    char temp[256];
    static char temp1[256];

    memset (temp, '\0', sizeof (temp));
    memset (temp1, '\0', sizeof (temp1));

    days = duration / (60 * 60 * 24);
    duration = duration - (60 * 60 * 24 * days);
    hours = duration / (60 * 60);
    duration = duration - (60 * 60 * hours);
    minutes = duration / 60;
    seconds = duration - (60 * minutes);

    if (days < 2)
        sprintf (temp, "%d day", days);
    else
        sprintf (temp, "%d days", days);
    if (hours < 2)
        sprintf (temp1, "%s %d hour", temp, hours);
    else
        sprintf (temp1, "%s %d hours", temp, hours);
    if (minutes < 2)
        sprintf (temp, "%s %d minute", temp1, minutes);
    else
        sprintf (temp, "%s %d minutes", temp1, minutes);
    if (seconds < 2)
        sprintf (temp1, "%s and %d second", temp, seconds);
    else
        sprintf (temp1, "%s and %d seconds", temp, seconds);

    return temp1;
}

/*==========================================
 * @time/@date/@server_date/@serverdate/@server_time/@servertime: Display the date/time of the server (by [Yor]
 * Calculation management of GM modification (@day/@night GM commands) is done
 *------------------------------------------
 */
int atcommand_servertime (const int fd, struct map_session_data *UNUSED,
                          const char *UNUSED, const char *UNUSED)
{
    struct TimerData *timer_data;
    struct TimerData *timer_data2;
    time_t time_server;         // variable for number of seconds (used with time() function)
    struct tm *datetime;        // variable for time in structure ->tm_mday, ->tm_sec, ...
    char temp[256];

    memset (temp, '\0', sizeof (temp));

    time (&time_server);        // get time in seconds since 1/1/1970
    datetime = gmtime (&time_server);   // convert seconds in structure
    // like sprintf, but only for date/time (Sunday, November 02 2003 15:12:52)
    strftime (temp, sizeof (temp) - 1, "Server time (normal time): %A, %B %d %Y %X.", datetime);
    clif_displaymessage (fd, temp);

    if (battle_config.night_duration == 0 && battle_config.day_duration == 0)
    {
        if (night_flag == 0)
            clif_displaymessage (fd, "Game time: The game is in permanent daylight.");
        else
            clif_displaymessage (fd, "Game time: The game is in permanent night.");
    }
    else if (battle_config.night_duration == 0)
        if (night_flag == 1)
        {                       // we start with night
            timer_data = get_timer (day_timer_tid);
            sprintf (temp, "Game time: The game is actualy in night for %s.", txt_time ((timer_data->tick - gettick ()) / 1000));
            clif_displaymessage (fd, temp);
            clif_displaymessage (fd, "Game time: After, the game will be in permanent daylight.");
        }
        else
            clif_displaymessage (fd, "Game time: The game is in permanent daylight.");
    else if (battle_config.day_duration == 0)
        if (night_flag == 0)
        {                       // we start with day
            timer_data = get_timer (night_timer_tid);
            sprintf (temp, "Game time: The game is actualy in daylight for %s.", txt_time ((timer_data->tick - gettick ()) / 1000));
            clif_displaymessage (fd, temp);
            clif_displaymessage (fd, "Game time: After, the game will be in permanent night.");
        }
        else
            clif_displaymessage (fd, "Game time: The game is in permanent night.");
    else
    {
        if (night_flag == 0)
        {
            timer_data = get_timer (night_timer_tid);
            timer_data2 = get_timer (day_timer_tid);
            sprintf (temp, "Game time: The game is actualy in daylight for %s.", txt_time ((timer_data->tick - gettick ()) / 1000));
            clif_displaymessage (fd, temp);
            if (timer_data->tick > timer_data2->tick)
                sprintf (temp, "Game time: After, the game will be in night for %s.", txt_time ((timer_data->interval - abs (timer_data->tick - timer_data2->tick)) / 1000));
            else
                sprintf (temp, "Game time: After, the game will be in night for %s.", txt_time (abs (timer_data->tick - timer_data2->tick) / 1000));
            clif_displaymessage (fd, temp);
            sprintf (temp, "Game time: A day cycle has a normal duration of %s.", txt_time (timer_data->interval / 1000));
            clif_displaymessage (fd, temp);
        }
        else
        {
            timer_data = get_timer (day_timer_tid);
            timer_data2 = get_timer (night_timer_tid);
            sprintf (temp, "Game time: The game is actualy in daylight for %s.", txt_time ((timer_data->tick - gettick ()) / 1000));
            clif_displaymessage (fd, temp);
            if (timer_data->tick > timer_data2->tick)
                sprintf (temp, "Game time: After, the game will be in daylight for %s.", txt_time ((timer_data->interval - abs (timer_data->tick - timer_data2->tick)) / 1000));
            else
                sprintf (temp, "Game time: After, the game will be in daylight for %s.", txt_time (abs (timer_data->tick - timer_data2->tick) / 1000));
            clif_displaymessage (fd, temp);
            sprintf (temp, "Game time: A day cycle has a normal duration of %s.", txt_time (timer_data->interval / 1000));
            clif_displaymessage (fd, temp);
        }
    }

    return 0;
}

/*==========================================
 * @chardelitem <item_name_or_ID> <quantity> <player> (by [Yor]
 * removes <quantity> item from a character
 * item can be equiped or not.
 * Inspired from a old command created by RoVeRT
 *------------------------------------------
 */
int atcommand_chardelitem (const int fd, struct map_session_data *sd,
                           const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd;
    char character[100];
    char item_name[100];
    int  i, number = 0, item_id, item_position, count;
    char output[200];
    struct item_data *item_data;

    memset (character, '\0', sizeof (character));
    memset (item_name, '\0', sizeof (item_name));
    memset (output, '\0', sizeof (output));

    if (!message || !*message
        || sscanf (message, "%s %d %99[^\n]", item_name, &number,
                   character) < 3 || number < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter an item name/id, a quantity and a player name (usage: @chardelitem <item_name_or_ID> <quantity> <player>).");
        return -1;
    }

    item_id = 0;
    if ((item_data = itemdb_searchname (item_name)) != NULL ||
        (item_data = itemdb_exists (atoi (item_name))) != NULL)
        item_id = item_data->nameid;

    if (item_id > 500)
    {
        if ((pl_sd = map_nick2sd (character)) != NULL)
        {
            if (pc_isGM (sd) >= pc_isGM (pl_sd))
            {                   // you can kill only lower or same level
                item_position = pc_search_inventory (pl_sd, item_id);
                if (item_position >= 0)
                {
                    count = 0;
                    for (i = 0; i < number && item_position >= 0; i++)
                    {
                        pc_delitem (pl_sd, item_position, 1, 0);
                        count++;
                        item_position = pc_search_inventory (pl_sd, item_id);   // for next loop
                    }
                    sprintf (output, "%d item(s) removed by a GM.", count);
                    clif_displaymessage (pl_sd->fd, output);
                    if (number == count)
                        sprintf (output, "%d item(s) removed from the player.", count);
                    else
                        sprintf (output, "%d item(s) removed. Player had only %d of %d items.", count, count, number);
                    clif_displaymessage (fd, output);
                }
                else
                {
                    clif_displaymessage (fd, "Character does not have the item.");
                    return -1;
                }
            }
            else
            {
                clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
                return -1;
            }
        }
        else
        {
            clif_displaymessage (fd, "Character not found.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Invalid item ID or name.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @jail <char_name> by [Yor]
 * Special warp! No check with nowarp and nowarpto flag
 *------------------------------------------
 */
int atcommand_jail (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *message)
{
    char character[100];
    struct map_session_data *pl_sd;
    int  x, y;

    memset (character, '\0', sizeof (character));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @jail <char_name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can jail only lower or same GM
            switch (MRAND (2))
            {
                case 0:
                    x = 24;
                    y = 75;
                    break;
                default:
                    x = 49;
                    y = 75;
                    break;
            }
            if (pc_setpos (pl_sd, "sec_pri.gat", x, y, 3) == 0)
            {
                pc_setsavepoint (pl_sd, "sec_pri.gat", x, y);   // Save Char Respawn Point in the jail room [Lupus]
                clif_displaymessage (pl_sd->fd, "GM has send you in jails.");
                clif_displaymessage (fd, "Player warped in jails.");
            }
            else
            {
                clif_displaymessage (fd, "Map not found.");
                return -1;
            }
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @unjail/@discharge <char_name> by [Yor]
 * Special warp! No check with nowarp and nowarpto flag
 *------------------------------------------
 */
int atcommand_unjail (const int fd, struct map_session_data *sd,
                      const char *UNUSED, const char *message)
{
    char character[100];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @unjail/@discharge <char_name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can jail only lower or same GM
            if (pl_sd->bl.m != map_mapname2mapid ("sec_pri.gat"))
            {
                clif_displaymessage (fd, "This player is not in jails.");
                return -1;
            }
            else if (pc_setpos (pl_sd, "prontera.gat", 156, 191, 3) == 0)
            {
                pc_setsavepoint (pl_sd, "prontera.gat", 156, 191);  // Save char respawn point in Prontera
                clif_displaymessage (pl_sd->fd, "GM has discharge you.");
                clif_displaymessage (fd, "Player warped to Prontera.");
            }
            else
            {
                clif_displaymessage (fd, "Map not found.");
                return -1;
            }
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @disguise <mob_id> by [Valaris] (simplified by [Yor])
 *------------------------------------------
 */
int atcommand_disguise (const int fd, struct map_session_data *sd,
                        const char *UNUSED, const char *message)
{
    int  mob_id;

    if (!message || !*message)
    {
        clif_displaymessage (fd,
                             "Please, enter a Monster/NPC name/id (usage: @disguise <monster_name_or_monster_ID>).");
        return -1;
    }

    if ((mob_id = mobdb_searchname (message)) == 0) // check name first (to avoid possible name begining by a number)
        mob_id = atoi (message);

    if ((mob_id >= 46 && mob_id <= 125) || (mob_id >= 700 && mob_id <= 718) ||  // NPC
        (mob_id >= 721 && mob_id <= 755) || (mob_id >= 757 && mob_id <= 811) || // NPC
        (mob_id >= 813 && mob_id <= 834) || // NPC
        (mob_id > 1000 && mob_id < 1521))
    {                           // monsters
        if (pc_isriding (sd))
        {                       // temporary prevention of crash caused by peco + disguise, will look into a better solution [Valaris]
            clif_displaymessage (fd, "Cannot wear disguise while riding a Peco.");
            return -1;
        }
        sd->disguiseflag = 1;   // set to override items with disguise script [Valaris]
        sd->disguise = mob_id;
        pc_setpos (sd, sd->mapname, sd->bl.x, sd->bl.y, 3);
        clif_displaymessage (fd, "Disguise applied.");
    }
    else
    {
        clif_displaymessage (fd, "Monster/NPC name/id hasn't been found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @undisguise by [Yor]
 *------------------------------------------
 */
int atcommand_undisguise (const int fd, struct map_session_data *sd,
                          const char *UNUSED, const char *UNUSED)
{
    if (sd->disguise)
    {
        clif_clearchar (&sd->bl, 9);
        sd->disguise = 0;
        pc_setpos (sd, sd->mapname, sd->bl.x, sd->bl.y, 3);
        clif_displaymessage (fd, "Undisguise applied.");
    }
    else
    {
        clif_displaymessage (fd, "You're not disguised.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @broadcast by [Valaris]
 *------------------------------------------
 */
int atcommand_broadcast (const int fd, struct map_session_data *sd,
                         const char *UNUSED, const char *message)
{
    char output[200];

    memset (output, '\0', sizeof (output));

    if (!message || !*message)
    {
        clif_displaymessage (fd,
                             "Please, enter a message (usage: @broadcast <message>).");
        return -1;
    }

    snprintf (output, 199, "%s : %s", sd->status.name, message);
    intif_GMmessage (output, strlen (output) + 1, 0);

    return 0;
}

/*==========================================
 * @localbroadcast by [Valaris]
 *------------------------------------------
 */
int atcommand_localbroadcast (const int fd, struct map_session_data *sd,
                              const char *UNUSED, const char *message)
{
    char output[200];

    memset (output, '\0', sizeof (output));

    if (!message || !*message)
    {
        clif_displaymessage (fd,
                             "Please, enter a message (usage: @localbroadcast <message>).");
        return -1;
    }

    snprintf (output, 199, "%s : %s", sd->status.name, message);

    clif_GMmessage (&sd->bl, output, strlen (output) + 1, 1);   // 1: ALL_SAMEMAP

    return 0;
}

/*==========================================
 * @ignorelist by [Yor]
 *------------------------------------------
 */
int atcommand_ignorelist (const int fd, struct map_session_data *sd,
                          const char *UNUSED, const char *UNUSED)
{
    char output[200];
    int  count;
    int  i;

    memset (output, '\0', sizeof (output));

    count = 0;
    for (i = 0; i < (int) (sizeof (sd->ignore) / sizeof (sd->ignore[0])); i++)
        if (sd->ignore[i].name[0])
            count++;

    if (sd->ignoreAll == 0)
        if (count == 0)
            clif_displaymessage (fd, "You accept all whispers");
        else
        {
            sprintf (output, "You accept all whispers, except from %d player(s):", count);
            clif_displaymessage (fd, output);
        }
    else if (count == 0)
        clif_displaymessage (fd, "You refuse all whispers");
    else
    {
        sprintf (output, "You refuse all whispers, especially from %d player(s):", count);
        clif_displaymessage (fd, output);
    }

    if (count > 0)
        for (i = 0; i < (int) (sizeof (sd->ignore) / sizeof (sd->ignore[0]));
             i++)
            if (sd->ignore[i].name[0])
                clif_displaymessage (fd, sd->ignore[i].name);

    return 0;
}

/*==========================================
 * @charignorelist <player_name> by [Yor]
 *------------------------------------------
 */
int atcommand_charignorelist (const int fd, struct map_session_data *UNUSED,
                              const char *UNUSED, const char *message)
{
    char character[100];
    struct map_session_data *pl_sd;
    char output[200];
    int  count;
    int  i;

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charignorelist <char name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        count = 0;
        for (i = 0;
             i < (int) (sizeof (pl_sd->ignore) / sizeof (pl_sd->ignore[0]));
             i++)
            if (pl_sd->ignore[i].name[0])
                count++;

        if (pl_sd->ignoreAll == 0)
            if (count == 0)
            {
                sprintf (output, "'%s' accepts any whispers.", pl_sd->status.name);
                clif_displaymessage (fd, output);
            }
            else
            {
                sprintf (output, "'%s' accepts all whispers, except from %d player(s):", pl_sd->status.name, count);
                clif_displaymessage (fd, output);
            }
        else if (count == 0)
        {
            sprintf (output, "'%s' refuses all whispers.", pl_sd->status.name);
            clif_displaymessage (fd, output);
        }
        else
        {
            sprintf (output, "'%s' refuses all whisps, especially from %d player(s):", pl_sd->status.name, count);
            clif_displaymessage (fd, output);
        }

        if (count > 0)
            for (i = 0;
                 i <
                 (int) (sizeof (pl_sd->ignore) / sizeof (pl_sd->ignore[0]));
                 i++)
                if (pl_sd->ignore[i].name[0])
                    clif_displaymessage (fd, pl_sd->ignore[i].name);

    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @inall <player_name> by [Yor]
 *------------------------------------------
 */
int atcommand_inall (const int fd, struct map_session_data *sd,
                     const char *UNUSED, const char *message)
{
    char character[100];
    char output[200];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @inall <char name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can change whisper option only to lower or same level
            if (pl_sd->ignoreAll == 0)
            {
                sprintf (output, "'%s' already accepts all whispers.", pl_sd->status.name);
                clif_displaymessage (fd, output);
                return -1;
            }
            else
            {
                pl_sd->ignoreAll = 0;
                sprintf (output, "'%s' now accepts all whispers.", pl_sd->status.name);
                clif_displaymessage (fd, output);
                // message to player
                clif_displaymessage (pl_sd->fd, "A GM has authorised all whispers for you.");
                WFIFOW (pl_sd->fd, 0) = 0x0d2;  // R 00d2 <type>.B <fail>.B: type: 0: deny, 1: allow, fail: 0: success, 1: fail
                WFIFOB (pl_sd->fd, 2) = 1;
                WFIFOB (pl_sd->fd, 3) = 0;  // success
                WFIFOSET (pl_sd->fd, 4);    // packet_len_table[0x0d2]
            }
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @exall <player_name> by [Yor]
 *------------------------------------------
 */
int atcommand_exall (const int fd, struct map_session_data *sd,
                     const char *UNUSED, const char *message)
{
    char character[100];
    char output[200];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @exall <char name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can change whisper option only to lower or same level
            if (pl_sd->ignoreAll == 1)
            {
                sprintf (output, "'%s' already blocks all whispers.", pl_sd->status.name);
                clif_displaymessage (fd, output);
                return -1;
            }
            else
            {
                pl_sd->ignoreAll = 1;
                sprintf (output, "'%s' blocks now all whispers.", pl_sd->status.name);
                clif_displaymessage (fd, output);
                // message to player
                clif_displaymessage (pl_sd->fd, "A GM has blocked all whispers for you.");
                WFIFOW (pl_sd->fd, 0) = 0x0d2;  // R 00d2 <type>.B <fail>.B: type: 0: deny, 1: allow, fail: 0: success, 1: fail
                WFIFOB (pl_sd->fd, 2) = 0;
                WFIFOB (pl_sd->fd, 3) = 0;  // success
                WFIFOSET (pl_sd->fd, 4);    // packet_len_table[0x0d2]
            }
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @chardisguise <mob_id> <character> by Kalaspuff (based off Valaris' and Yor's work)
 *------------------------------------------
 */
int atcommand_chardisguise (const int fd, struct map_session_data *sd,
                            const char *UNUSED, const char *message)
{
    int  mob_id;
    char character[100];
    char mob_name[100];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));
    memset (mob_name, '\0', sizeof (mob_name));

    if (!message || !*message
        || sscanf (message, "%s %99[^\n]", mob_name, character) < 2)
    {
        clif_displaymessage (fd,
                             "Please, enter a Monster/NPC name/id and a player name (usage: @chardisguise <monster_name_or_monster_ID> <char name>).");
        return -1;
    }

    if ((mob_id = mobdb_searchname (mob_name)) == 0)    // check name first (to avoid possible name begining by a number)
        mob_id = atoi (mob_name);

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can disguise only lower or same level
            if ((mob_id >= 46 && mob_id <= 125) || (mob_id >= 700 && mob_id <= 718) ||  // NPC
                (mob_id >= 721 && mob_id <= 755) || (mob_id >= 757 && mob_id <= 811) || // NPC
                (mob_id >= 813 && mob_id <= 834) || // NPC
                (mob_id > 1000 && mob_id < 1521))
            {                   // monsters
                if (pc_isriding (pl_sd))
                {               // temporary prevention of crash caused by peco + disguise, will look into a better solution [Valaris]
                    clif_displaymessage (fd, "Character cannot wear disguise while riding a Peco.");
                    return -1;
                }
                pl_sd->disguiseflag = 1;    // set to override items with disguise script [Valaris]
                pl_sd->disguise = mob_id;
                pc_setpos (pl_sd, pl_sd->mapname, pl_sd->bl.x, pl_sd->bl.y,
                           3);
                clif_displaymessage (fd, "Character's disguise applied.");
            }
            else
            {
                clif_displaymessage (fd, "Monster/NPC name/id hasn't been found.");
                return -1;
            }
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @charundisguise <character> by Kalaspuff (based off Yor's work)
 *------------------------------------------
 */
int atcommand_charundisguise (const int fd, struct map_session_data *sd,
                              const char *UNUSED, const char *message)
{
    char character[100];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charundisguise <char name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can undisguise only lower or same level
            if (pl_sd->disguise)
            {
                clif_clearchar (&pl_sd->bl, 9);
                pl_sd->disguise = 0;
                pc_setpos (pl_sd, pl_sd->mapname, pl_sd->bl.x, pl_sd->bl.y,
                           3);
                clif_displaymessage (fd, "Character's undisguise applied.");
            }
            else
            {
                clif_displaymessage (fd, "Character is not disguised.");
                return -1;
            }
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @email <actual@email> <new@email> by [Yor]
 *------------------------------------------
 */
int atcommand_email (const int fd, struct map_session_data *sd,
                     const char *UNUSED, const char *message)
{
    char actual_email[100];
    char new_email[100];

    memset (actual_email, '\0', sizeof (actual_email));
    memset (new_email, '\0', sizeof (new_email));

    if (!message || !*message
        || sscanf (message, "%99s %99s", actual_email, new_email) < 2)
    {
        clif_displaymessage (fd,
                             "Please enter 2 emails (usage: @email <actual@email> <new@email>).");
        return -1;
    }

    if (e_mail_check (actual_email) == 0)
    {
        clif_displaymessage (fd, "Invalid actual email. If you have default e-mail, give a@a.com.");
        return -1;
    }
    else if (e_mail_check (new_email) == 0)
    {
        clif_displaymessage (fd, "Invalid new email. Please enter a real e-mail.");
        return -1;
    }
    else if (strcasecmp (new_email, "a@a.com") == 0)
    {
        clif_displaymessage (fd, "New email must be a real e-mail.");
        return -1;
    }
    else if (strcasecmp (actual_email, new_email) == 0)
    {
        clif_displaymessage (fd, "New email must be different of the actual e-mail.");
        return -1;
    }
    else
    {
        chrif_changeemail (sd->status.account_id, actual_email, new_email);
        clif_displaymessage (fd, "Information sent to login-server via char-server.");
    }

    return 0;
}

/*==========================================
 *@effect
 *------------------------------------------
 */
int atcommand_effect (const int fd, struct map_session_data *sd,
                      const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd;
    int  type = 0, flag = 0, i;

    if (!message || !*message || sscanf (message, "%d %d", &type, &flag) < 2)
    {
        clif_displaymessage (fd,
                             "Please, enter at least a option (usage: @effect <type+>).");
        return -1;
    }
    if (flag <= 0)
    {
        clif_specialeffect (&sd->bl, type, flag);
        clif_displaymessage (fd, "Your effect has changed.");
    }
    else
    {
        for (i = 0; i < fd_max; i++)
        {
            if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
                && pl_sd->state.auth)
            {
                clif_specialeffect (&pl_sd->bl, type, flag);
                clif_displaymessage (pl_sd->fd, "Your effect has changed.");
            }
        }
    }

    return 0;
}

/*==========================================
 * @charitemlist <character>: Displays the list of a player's items.
 *------------------------------------------
 */
int
atcommand_character_item_list (const int fd, struct map_session_data *sd,
                               const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd;
    struct item_data *item_data, *item_temp;
    int  i, j, equip, count, counter, counter2;
    char character[100], output[200], equipstr[100], outputtmp[200];

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));
    memset (equipstr, '\0', sizeof (equipstr));
    memset (outputtmp, '\0', sizeof (outputtmp));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charitemlist <char name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can look items only lower or same level
            counter = 0;
            count = 0;
            for (i = 0; i < MAX_INVENTORY; i++)
            {
                if (pl_sd->status.inventory[i].nameid > 0
                    && (item_data =
                        itemdb_search (pl_sd->status.inventory[i].nameid)) !=
                    NULL)
                {
                    counter = counter + pl_sd->status.inventory[i].amount;
                    count++;
                    if (count == 1)
                    {
                        sprintf (output, "------ Items list of '%s' ------",
                                 pl_sd->status.name);
                        clif_displaymessage (fd, output);
                    }
                    if ((equip = pl_sd->status.inventory[i].equip))
                    {
                        strcpy (equipstr, "| equiped: ");
                        if (equip & 4)
                            strcat (equipstr, "robe/gargment, ");
                        if (equip & 8)
                            strcat (equipstr, "left accessory, ");
                        if (equip & 16)
                            strcat (equipstr, "body/armor, ");
                        if ((equip & 34) == 2)
                            strcat (equipstr, "right hand, ");
                        if ((equip & 34) == 32)
                            strcat (equipstr, "left hand, ");
                        if ((equip & 34) == 34)
                            strcat (equipstr, "both hands, ");
                        if (equip & 64)
                            strcat (equipstr, "feet, ");
                        if (equip & 128)
                            strcat (equipstr, "right accessory, ");
                        if ((equip & 769) == 1)
                            strcat (equipstr, "lower head, ");
                        if ((equip & 769) == 256)
                            strcat (equipstr, "top head, ");
                        if ((equip & 769) == 257)
                            strcat (equipstr, "lower/top head, ");
                        if ((equip & 769) == 512)
                            strcat (equipstr, "mid head, ");
                        if ((equip & 769) == 512)
                            strcat (equipstr, "lower/mid head, ");
                        if ((equip & 769) == 769)
                            strcat (equipstr, "lower/mid/top head, ");
                        // remove final ', '
                        equipstr[strlen (equipstr) - 2] = '\0';
                    }
                    else
                        memset (equipstr, '\0', sizeof (equipstr));
                    if (sd->status.inventory[i].refine)
                        sprintf (output, "%d %s %+d (%s %+d, id: %d) %s",
                                 pl_sd->status.inventory[i].amount,
                                 item_data->name,
                                 pl_sd->status.inventory[i].refine,
                                 item_data->jname,
                                 pl_sd->status.inventory[i].refine,
                                 pl_sd->status.inventory[i].nameid, equipstr);
                    else
                        sprintf (output, "%d %s (%s, id: %d) %s",
                                 pl_sd->status.inventory[i].amount,
                                 item_data->name, item_data->jname,
                                 pl_sd->status.inventory[i].nameid, equipstr);
                    clif_displaymessage (fd, output);
                    memset (output, '\0', sizeof (output));
                    counter2 = 0;
                    for (j = 0; j < item_data->slot; j++)
                    {
                        if (pl_sd->status.inventory[i].card[j])
                        {
                            if ((item_temp =
                                 itemdb_search (pl_sd->status.
                                                inventory[i].card[j])) !=
                                NULL)
                            {
                                if (output[0] == '\0')
                                    sprintf (outputtmp,
                                             " -> (card(s): #%d %s (%s), ",
                                             ++counter2, item_temp->name,
                                             item_temp->jname);
                                else
                                    sprintf (outputtmp, "#%d %s (%s), ",
                                             ++counter2, item_temp->name,
                                             item_temp->jname);
                                strcat (output, outputtmp);
                            }
                        }
                    }
                    if (output[0] != '\0')
                    {
                        output[strlen (output) - 2] = ')';
                        output[strlen (output) - 1] = '\0';
                        clif_displaymessage (fd, output);
                    }
                }
            }
            if (count == 0)
                clif_displaymessage (fd, "No item found on this player.");
            else
            {
                sprintf (output, "%d item(s) found in %d kind(s) of items.",
                         counter, count);
                clif_displaymessage (fd, output);
            }
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @charstoragelist <character>: Displays the items list of a player's storage.
 *------------------------------------------
 */
int
atcommand_character_storage_list (const int fd, struct map_session_data *sd,
                                  const char *UNUSED, const char *message)
{
    struct storage *stor;
    struct map_session_data *pl_sd;
    struct item_data *item_data, *item_temp;
    int  i, j, count, counter, counter2;
    char character[100], output[200], outputtmp[200];

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));
    memset (outputtmp, '\0', sizeof (outputtmp));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charitemlist <char name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can look items only lower or same level
            if ((stor = account2storage2 (pl_sd->status.account_id)) != NULL)
            {
                counter = 0;
                count = 0;
                for (i = 0; i < MAX_STORAGE; i++)
                {
                    if (stor->storage_[i].nameid > 0
                        && (item_data =
                            itemdb_search (stor->storage_[i].nameid)) != NULL)
                    {
                        counter = counter + stor->storage_[i].amount;
                        count++;
                        if (count == 1)
                        {
                            sprintf (output,
                                     "------ Storage items list of '%s' ------",
                                     pl_sd->status.name);
                            clif_displaymessage (fd, output);
                        }
                        if (stor->storage_[i].refine)
                            sprintf (output, "%d %s %+d (%s %+d, id: %d)",
                                     stor->storage_[i].amount,
                                     item_data->name,
                                     stor->storage_[i].refine,
                                     item_data->jname,
                                     stor->storage_[i].refine,
                                     stor->storage_[i].nameid);
                        else
                            sprintf (output, "%d %s (%s, id: %d)",
                                     stor->storage_[i].amount,
                                     item_data->name, item_data->jname,
                                     stor->storage_[i].nameid);
                        clif_displaymessage (fd, output);
                        memset (output, '\0', sizeof (output));
                        counter2 = 0;
                        for (j = 0; j < item_data->slot; j++)
                        {
                            if (stor->storage_[i].card[j])
                            {
                                if ((item_temp =
                                     itemdb_search (stor->
                                                    storage_[i].card[j])) !=
                                    NULL)
                                {
                                    if (output[0] == '\0')
                                        sprintf (outputtmp,
                                                 " -> (card(s): #%d %s (%s), ",
                                                 ++counter2, item_temp->name,
                                                 item_temp->jname);
                                    else
                                        sprintf (outputtmp, "#%d %s (%s), ",
                                                 ++counter2, item_temp->name,
                                                 item_temp->jname);
                                    strcat (output, outputtmp);
                                }
                            }
                        }
                        if (output[0] != '\0')
                        {
                            output[strlen (output) - 2] = ')';
                            output[strlen (output) - 1] = '\0';
                            clif_displaymessage (fd, output);
                        }
                    }
                }
                if (count == 0)
                    clif_displaymessage (fd,
                                         "No item found in the storage of this player.");
                else
                {
                    sprintf (output,
                             "%d item(s) found in %d kind(s) of items.",
                             counter, count);
                    clif_displaymessage (fd, output);
                }
            }
            else
            {
                clif_displaymessage (fd, "This player has no storage.");
                return -1;
            }
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @charcartlist <character>: Displays the items list of a player's cart.
 *------------------------------------------
 */
int
atcommand_character_cart_list (const int fd, struct map_session_data *sd,
                               const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd;
    struct item_data *item_data, *item_temp;
    int  i, j, count, counter, counter2;
    char character[100], output[200], outputtmp[200];

    memset (character, '\0', sizeof (character));
    memset (output, '\0', sizeof (output));
    memset (outputtmp, '\0', sizeof (outputtmp));

    if (!message || !*message || sscanf (message, "%99[^\n]", character) < 1)
    {
        clif_displaymessage (fd,
                             "Please, enter a player name (usage: @charitemlist <char name>).");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (pc_isGM (sd) >= pc_isGM (pl_sd))
        {                       // you can look items only lower or same level
            counter = 0;
            count = 0;
            for (i = 0; i < MAX_CART; i++)
            {
                if (pl_sd->status.cart[i].nameid > 0
                    && (item_data =
                        itemdb_search (pl_sd->status.cart[i].nameid)) != NULL)
                {
                    counter = counter + pl_sd->status.cart[i].amount;
                    count++;
                    if (count == 1)
                    {
                        sprintf (output,
                                 "------ Cart items list of '%s' ------",
                                 pl_sd->status.name);
                        clif_displaymessage (fd, output);
                    }
                    if (pl_sd->status.cart[i].refine)
                        sprintf (output, "%d %s %+d (%s %+d, id: %d)",
                                 pl_sd->status.cart[i].amount,
                                 item_data->name,
                                 pl_sd->status.cart[i].refine,
                                 item_data->jname,
                                 pl_sd->status.cart[i].refine,
                                 pl_sd->status.cart[i].nameid);
                    else
                        sprintf (output, "%d %s (%s, id: %d)",
                                 pl_sd->status.cart[i].amount,
                                 item_data->name, item_data->jname,
                                 pl_sd->status.cart[i].nameid);
                    clif_displaymessage (fd, output);
                    memset (output, '\0', sizeof (output));
                    counter2 = 0;
                    for (j = 0; j < item_data->slot; j++)
                    {
                        if (pl_sd->status.cart[i].card[j])
                        {
                            if ((item_temp =
                                 itemdb_search (pl_sd->status.
                                                cart[i].card[j])) != NULL)
                            {
                                if (output[0] == '\0')
                                    sprintf (outputtmp,
                                             " -> (card(s): #%d %s (%s), ",
                                             ++counter2, item_temp->name,
                                             item_temp->jname);
                                else
                                    sprintf (outputtmp, "#%d %s (%s), ",
                                             ++counter2, item_temp->name,
                                             item_temp->jname);
                                strcat (output, outputtmp);
                            }
                        }
                    }
                    if (output[0] != '\0')
                    {
                        output[strlen (output) - 2] = ')';
                        output[strlen (output) - 1] = '\0';
                        clif_displaymessage (fd, output);
                    }
                }
            }
            if (count == 0)
                clif_displaymessage (fd,
                                     "No item found in the cart of this player.");
            else
            {
                sprintf (output, "%d item(s) found in %d kind(s) of items.",
                         counter, count);
                clif_displaymessage (fd, output);
            }
        }
        else
        {
            clif_displaymessage (fd, "Your GM level don't authorise you to do this action on this player.");
            return -1;
        }
    }
    else
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    return 0;
}

/*==========================================
 * @killer by MouseJstr
 * enable killing players even when not in pvp
 *------------------------------------------
 */
int
atcommand_killer (const int fd, struct map_session_data *sd,
                  const char *UNUSED, const char *UNUSED)
{
    sd->special_state.killer = !sd->special_state.killer;

    if (sd->special_state.killer)
        clif_displaymessage (fd, "You be a killa...");
    else
        clif_displaymessage (fd, "You gonna be own3d");

    return 0;
}

/*==========================================
 * @killable by MouseJstr
 * enable other people killing you
 *------------------------------------------
 */
int
atcommand_killable (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *UNUSED)
{
    sd->special_state.killable = !sd->special_state.killable;

    if (sd->special_state.killable)
        clif_displaymessage (fd, "You gonna be own3d");
    else
        clif_displaymessage (fd, "You be a killa...");

    return 0;
}

/*==========================================
 * @charkillable by MouseJstr
 * enable another player to be killed
 *------------------------------------------
 */
int
atcommand_charkillable (const int fd, struct map_session_data *UNUSED,
                        const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd = NULL;

    if (!message || !*message)
        return -1;

    if ((pl_sd = map_nick2sd ((char *) message)) == NULL)
        return -1;

    pl_sd->special_state.killable = !pl_sd->special_state.killable;

    if (pl_sd->special_state.killable)
        clif_displaymessage (fd, "The player is now killable");
    else
        clif_displaymessage (fd, "The player is no longer killable");

    return 0;
}

/*==========================================
 * @skillon by MouseJstr
 * turn skills on for the map
 *------------------------------------------
 */
int
atcommand_skillon (const int fd, struct map_session_data *sd,
                   const char *UNUSED, const char *UNUSED)
{
    maps[sd->bl.m].flag.noskill = 0;
    clif_displaymessage (fd, "Map skills are on.");
    return 0;
}

/*==========================================
 * @skilloff by MouseJstr
 * Turn skills off on the map
 *------------------------------------------
 */
int
atcommand_skilloff (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *UNUSED)
{
    maps[sd->bl.m].flag.noskill = 1;
    clif_displaymessage (fd, "Map skills are off");
    return 0;
}

/*==========================================
 * @npcmove by MouseJstr
 *
 * move a npc
 *------------------------------------------
 */
int
atcommand_npcmove (const int UNUSED, struct map_session_data *sd,
                   const char *UNUSED, const char *message)
{
    char character[100];
    int  x = 0, y = 0;
    struct npc_data *nd = 0;

    if (sd == NULL)
        return -1;

    if (!message || !*message)
        return -1;

    memset (character, '\0', sizeof character);

    if (sscanf (message, "%d %d %99[^\n]", &x, &y, character) < 3)
        return -1;

    nd = npc_name2id (character);
    if (nd == NULL)
        return -1;

    npc_enable (character, 0);
    nd->bl.x = x;
    nd->bl.y = y;
    npc_enable (character, 1);

    return 0;
}

/*==========================================
 * @addwarp by MouseJstr
 *
 * Create a new static warp point.
 *------------------------------------------
 */
int
atcommand_addwarp (const int fd, struct map_session_data *sd,
                   const char *UNUSED, const char *message)
{
    char w1[64], w3[64], w4[64];
    char map[30], output[200];
    int  x, y, ret;

    if (!message || !*message)
        return -1;

    if (sscanf (message, "%99s %d %d[^\n]", map, &x, &y) < 3)
        return -1;

    sprintf (w1, "%s,%d,%d", sd->mapname, sd->bl.x, sd->bl.y);
    sprintf (w3, "%s%d%d%d%d", map, sd->bl.x, sd->bl.y, x, y);
    sprintf (w4, "1,1,%s.gat,%d,%d", map, x, y);

    ret = npc_parse_warp (w1, "warp", w3, w4);

    sprintf (output, "New warp NPC => %s", w3);

    clif_displaymessage (fd, output);

    return ret;
}

/*==========================================
 * @follow by [MouseJstr]
 *
 * Follow a player .. staying no more then 5 spaces away
 *------------------------------------------
 */
int
atcommand_follow (const int fd, struct map_session_data *UNUSED,
                  const char *UNUSED, const char *UNUSED)
{
#if 0
    struct map_session_data *pl_sd = NULL;

    if (!message || !*message)
        return -1;
    if ((pl_sd = map_nick2sd ((char *) message)) != NULL)
        pc_follow (sd, pl_sd->bl.id);
    else
        return 1;
#endif

    /*
     * Command disabled - it's incompatible with the TMW
     * client.
     */
    clif_displaymessage (fd, "@follow command not available");

    return 0;

}

/*==========================================
 * @chareffect by [MouseJstr]
 *
 * Create a effect localized on another character
 *------------------------------------------
 */
int
atcommand_chareffect (const int fd, struct map_session_data *UNUSED,
                      const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd = NULL;
    char target[255];
    int  type = 0;

    if (!message || !*message
        || sscanf (message, "%d %s", &type, target) != 2)
    {
        clif_displaymessage (fd, "usage: @chareffect <type+> <target>.");
        return -1;
    }

    if ((pl_sd = map_nick2sd ((char *) target)) == NULL)
        return -1;

    clif_specialeffect (&pl_sd->bl, type, 0);
    clif_displaymessage (fd, "Your effect has changed.");   // Your effect has changed.

    return 0;
}

/*==========================================
 * @dropall by [MouseJstr]
 *
 * Drop all your possession on the ground
 *------------------------------------------
 */
int
atcommand_dropall (const int UNUSED, struct map_session_data *sd,
                   const char *UNUSED, const char *UNUSED)
{
    int  i;
    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].amount)
        {
            if (sd->status.inventory[i].equip != 0)
                pc_unequipitem (sd, i, 0);
            pc_dropitem (sd, i, sd->status.inventory[i].amount);
        }
    }
    return 0;
}

/*==========================================
 * @chardropall by [MouseJstr]
 *
 * Throw all the characters possessions on the ground.  Normally
 * done in response to them being disrespectful of a GM
 *------------------------------------------
 */
int
atcommand_chardropall (const int fd, struct map_session_data *UNUSED,
                       const char *UNUSED, const char *message)
{
    int  i;
    struct map_session_data *pl_sd = NULL;

    if (!message || !*message)
        return -1;
    if ((pl_sd = map_nick2sd ((char *) message)) == NULL)
        return -1;
    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (pl_sd->status.inventory[i].amount)
        {
            if (pl_sd->status.inventory[i].equip != 0)
                pc_unequipitem (pl_sd, i, 0);
            pc_dropitem (pl_sd, i, pl_sd->status.inventory[i].amount);
        }
    }

    clif_displaymessage (pl_sd->fd, "Ever play 52 card pickup?");
    clif_displaymessage (fd, "It is done");
    //clif_displaymessage(fd, "It is offical.. your a jerk");

    return 0;
}

/*==========================================
 * @storeall by [MouseJstr]
 *
 * Put everything into storage to simplify your inventory to make
 * debugging easie
 *------------------------------------------
 */
int
atcommand_storeall (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *UNUSED)
{
    int  i;
    nullpo_retr (-1, sd);

    if (sd->state.storage_flag != 1)
    {                           //Open storage.
        switch (storage_storageopen (sd))
        {
            case 2:            //Try again
                clif_displaymessage (fd, "run this command again..");
                return 0;
            case 1:            //Failure
                clif_displaymessage (fd,
                                     "You can't open the storage currently.");
                return 1;
        }
    }
    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].amount)
        {
            if (sd->status.inventory[i].equip != 0)
                pc_unequipitem (sd, i, 0);
            storage_storageadd (sd, i, sd->status.inventory[i].amount);
        }
    }
    storage_storageclose (sd);

    clif_displaymessage (fd, "It is done");
    return 0;
}

/*==========================================
 * @charstoreall by [MouseJstr]
 *
 * A way to screw with players who piss you off
 *------------------------------------------
 */
int
atcommand_charstoreall (const int fd, struct map_session_data *sd,
                        const char *UNUSED, const char *message)
{
    int  i;
    struct map_session_data *pl_sd = NULL;

    if (!message || !*message)
        return -1;
    if ((pl_sd = map_nick2sd ((char *) message)) == NULL)
        return -1;

    if (storage_storageopen (pl_sd) == 1)
    {
        clif_displaymessage (fd,
                             "Had to open the characters storage window...");
        clif_displaymessage (fd, "run this command again..");
        return 0;
    }
    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (pl_sd->status.inventory[i].amount)
        {
            if (pl_sd->status.inventory[i].equip != 0)
                pc_unequipitem (pl_sd, i, 0);
            storage_storageadd (pl_sd, i, sd->status.inventory[i].amount);
        }
    }
    storage_storageclose (pl_sd);

    clif_displaymessage (pl_sd->fd,
                         "Everything you own has been put away for safe keeping.");
    clif_displaymessage (pl_sd->fd,
                         "go to the nearest kafka to retrieve it..");
    clif_displaymessage (pl_sd->fd, "   -- the management");

    clif_displaymessage (fd, "It is done");

    return 0;
}

/*==========================================
 * @skillid by [MouseJstr]
 *
 * lookup a skill by name
 *------------------------------------------
 */
int
atcommand_skillid (const int fd, struct map_session_data *UNUSED,
                   const char *UNUSED, const char *message)
{
    int  skillen = 0, idx = 0;
    if (!message || !*message)
        return -1;
    skillen = strlen (message);
    while (skill_names[idx].id != 0)
    {
        if ((strncasecmp (skill_names[idx].name, message, skillen) == 0) ||
            (strncasecmp (skill_names[idx].desc, message, skillen) == 0))
        {
            char output[255];
            sprintf (output, "skill %d: %s", skill_names[idx].id,
                     skill_names[idx].desc);
            clif_displaymessage (fd, output);
        }
        idx++;
    }
    return 0;
}

/*==========================================
 * @useskill by [MouseJstr]
 *
 * A way of using skills without having to find them in the skills menu
 *------------------------------------------
 */
int
atcommand_useskill (const int fd, struct map_session_data *sd,
                    const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd = NULL;
    int  skillnum;
    int  skilllv;
    int  inf;
    char target[255];

    if (!message || !*message)
        return -1;
    if (sscanf (message, "%d %d %s", &skillnum, &skilllv, target) != 3)
    {
        clif_displaymessage (fd,
                             "Usage: @useskill <skillnum> <skillv> <target>");
        return -1;
    }
    if ((pl_sd = map_nick2sd (target)) == NULL)
    {
        return -1;
    }

    inf = skill_get_inf (skillnum);

    if ((inf == 2) || (inf == 1))
        skill_use_pos (sd, pl_sd->bl.x, pl_sd->bl.y, skillnum, skilllv);
    else
        skill_use_id (sd, pl_sd->bl.id, skillnum, skilllv);

    return 0;
}

/*==========================================
 * It is made to rain.
 *------------------------------------------
 */
int
atcommand_rain (const int UNUSED, struct map_session_data *sd,
                const char *UNUSED, const char *UNUSED)
{
    int  effno = 0;
    effno = 161;
    nullpo_retr (-1, sd);
    if (effno < 0 || maps[sd->bl.m].flag.rain)
        return -1;

    maps[sd->bl.m].flag.rain = 1;
    clif_specialeffect (&sd->bl, effno, 2);
    return 0;
}

/*==========================================
 * It is made to snow.
 *------------------------------------------
 */
int
atcommand_snow (const int UNUSED, struct map_session_data *sd,
                const char *UNUSED, const char *UNUSED)
{
    int  effno = 0;
    effno = 162;
    nullpo_retr (-1, sd);
    if (effno < 0 || maps[sd->bl.m].flag.snow)
        return -1;

    maps[sd->bl.m].flag.snow = 1;
    clif_specialeffect (&sd->bl, effno, 2);
    return 0;
}

/*==========================================
 * Cherry tree snowstorm is made to fall. (Sakura)
 *------------------------------------------
 */
int
atcommand_sakura (const int UNUSED, struct map_session_data *sd,
                  const char *UNUSED, const char *UNUSED)
{
    int  effno = 0;
    effno = 163;
    nullpo_retr (-1, sd);
    if (effno < 0 || maps[sd->bl.m].flag.sakura)
        return -1;

    maps[sd->bl.m].flag.sakura = 1;
    clif_specialeffect (&sd->bl, effno, 2);
    return 0;
}

/*==========================================
 * Fog hangs over.
 *------------------------------------------
 */
int
atcommand_fog (const int UNUSED, struct map_session_data *sd,
               const char *UNUSED, const char *UNUSED)
{
    int  effno = 0;
    effno = 233;
    nullpo_retr (-1, sd);
    if (effno < 0 || maps[sd->bl.m].flag.fog)
        return -1;

    maps[sd->bl.m].flag.fog = 1;
    clif_specialeffect (&sd->bl, effno, 2);

    return 0;
}

/*==========================================
 * Fallen leaves fall.
 *------------------------------------------
 */
int
atcommand_leaves (const int UNUSED, struct map_session_data *sd,
                  const char *UNUSED, const char *UNUSED)
{
    int  effno = 0;
    effno = 333;
    nullpo_retr (-1, sd);
    if (effno < 0 || maps[sd->bl.m].flag.leaves)
        return -1;

    maps[sd->bl.m].flag.leaves = 1;
    clif_specialeffect (&sd->bl, effno, 2);
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int atcommand_summon (const int UNUSED, struct map_session_data *sd,
                      const char *UNUSED, const char *message)
{
    char name[100];
    int  mob_id = 0;
    int  x = 0;
    int  y = 0;
    int  id = 0;
    struct mob_data *md;
    unsigned int tick = gettick ();

    nullpo_retr (-1, sd);

    if (!message || !*message)
        return -1;
    if (sscanf (message, "%99s", name) < 1)
        return -1;

    if ((mob_id = atoi (name)) == 0)
        mob_id = mobdb_searchname (name);
    if (mob_id == 0)
        return -1;

    x = sd->bl.x + (MRAND (10) - 5);
    y = sd->bl.y + (MRAND (10) - 5);

    id = mob_once_spawn (sd, "this", x, y, "--ja--", mob_id, 1, "");
    if ((md = (struct mob_data *) map_id2bl (id)))
    {
        md->master_id = sd->bl.id;
        md->state.special_mob_ai = 1;
        md->mode = mob_db[md->mob_class].mode | 0x04;
        md->deletetimer = add_timer (tick + 60000, mob_timer_delete, id, 0);
        clif_misceffect (&md->bl, 344);
    }
    clif_skill_poseffect (&sd->bl, AM_CALLHOMUN, 1, x, y, tick);

    return 0;
}

/*==========================================
 * @adjcmdlvl by [MouseJstr]
 *
 * Temp adjust the GM level required to use a GM command
 *
 * Used during beta testing to allow players to use GM commands
 * for short periods of time
 *------------------------------------------
 */
int
atcommand_adjcmdlvl (const int fd, struct map_session_data *UNUSED,
                     const char *UNUSED, const char *message)
{
    int newlev;
    char cmd[100];

    if (!message || !*message || sscanf (message, "%d %s", &newlev, cmd) != 2)
    {
        clif_displaymessage (fd, "usage: @adjcmdlvl <lvl> <command>.");
        return -1;
    }

    for (int i = 0; i < ARRAY_SIZEOF(atcommand_info); i++)
        if (strcasecmp (cmd, atcommand_info[i].command + 1) == 0)
        {
            atcommand_info[i].level = newlev;
            clif_displaymessage (fd, "@command level changed.");
            return 0;
        }

    clif_displaymessage (fd, "@command not found.");
    return -1;
}

/*==========================================
 * @adjgmlvl by [MouseJstr]
 *
 * Create a temp GM
 *
 * Used during beta testing to allow players to use GM commands
 * for short periods of time
 *------------------------------------------
 */
int
atcommand_adjgmlvl (const int fd, struct map_session_data *UNUSED,
                    const char *UNUSED, const char *message)
{
    int  newlev;
    char user[100];
    struct map_session_data *pl_sd;

    if (!message || !*message
        || sscanf (message, "%d %s", &newlev, user) != 2)
    {
        clif_displaymessage (fd, "usage: @adjgmlvl <lvl> <user>.");
        return -1;
    }

    if ((pl_sd = map_nick2sd ((char *) user)) == NULL)
        return -1;

    pc_set_gm_level (pl_sd->status.account_id, newlev);

    return 0;
}

/*==========================================
 * @trade by [MouseJstr]
 *
 * Open a trade window with a remote player
 *
 * If I have to jump to a remote player one more time, I am
 * gonna scream!
 *------------------------------------------
 */
int
atcommand_trade (const int UNUSED, struct map_session_data *sd,
                 const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd = NULL;

    if (!message || !*message)
        return -1;
    if ((pl_sd = map_nick2sd ((char *) message)) != NULL)
    {
        trade_traderequest (sd, pl_sd->bl.id);
        return 0;
    }
    return -1;
}

/*===========================
 * @unmute [Valaris]
 *===========================
*/
int atcommand_unmute (const int UNUSED, struct map_session_data *sd,
                      const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd = NULL;
    if (!message || !*message)
        return -1;

    if ((pl_sd = map_nick2sd ((char *) message)) != NULL)
    {
        if (pl_sd->sc_data[SC_NOCHAT].timer != -1)
        {
            skill_status_change_end (&pl_sd->bl, SC_NOCHAT, -1);
            clif_displaymessage (sd->fd, "Player unmuted");
        }
        else
            clif_displaymessage (sd->fd, "Player is not muted");
    }

    return 0;
}

/* Magic atcommands by Fate */

static int magic_base = TMW_MAGIC;
#define magic_skills_nr 6
static const char *magic_skill_names[magic_skills_nr] =
    { "magic", "life", "war", "transmute", "nature", "astral" };

int
atcommand_magic_info (const int fd, struct map_session_data *UNUSED,
                      const char *UNUSED, const char *message)
{
    char character[100];
    char buf[200];
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));

    if (!message || !*message || sscanf (message, "%99s", character) < 1)
    {
        clif_displaymessage (fd, "Usage: @magicinfo <char_name>");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        int  i;

        sprintf (buf, "`%s' has the following magic skills:", character);
        clif_displaymessage (fd, buf);

        for (i = 0; i < magic_skills_nr; i++)
        {
            sprintf (buf, "%d in %s", pl_sd->status.skill[i + magic_base].lv,
                     magic_skill_names[i]);
            if (pl_sd->status.skill[i + magic_base].id == i + magic_base)
                clif_displaymessage (fd, buf);
        }

        return 0;
    }
    else
        clif_displaymessage (fd, "Character not found.");

    return -1;
}

static void set_skill (struct map_session_data *sd, int i, int level)
{
    sd->status.skill[i].id = level ? i : 0;
    sd->status.skill[i].lv = level;
}

int
atcommand_set_magic (const int fd, struct map_session_data *UNUSED,
                     const char *UNUSED, const char *message)
{
    char character[100];
    char magic_type[20];
    int  skill_index = -1;      // 0: all
    int  value;
    struct map_session_data *pl_sd;

    memset (character, '\0', sizeof (character));

    if (!message || !*message
        || sscanf (message, "%19s %i %99s", magic_type, &value,
                   character) < 1)
    {
        clif_displaymessage (fd,
                             "Usage: @setmagic <school> <value> <char-name>, where <school> is either `magic', one of the school names, or `all'.");
        return -1;
    }

    if (!strcasecmp ("all", magic_type))
        skill_index = 0;
    else
    {
        int  i;
        for (i = 0; i < magic_skills_nr; i++)
        {
            if (!strcasecmp (magic_skill_names[i], magic_type))
            {
                skill_index = i + magic_base;
                break;
            }
        }
    }

    if (skill_index == -1)
    {
        clif_displaymessage (fd,
                             "Incorrect school of magic.  Use `magic', `nature', `life', `war', `transmute', `ether', or `all'.");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        int  i;
        if (skill_index == 0)
            for (i = 0; i < magic_skills_nr; i++)
                set_skill (pl_sd, i + magic_base, value);
        else
            set_skill (pl_sd, skill_index, value);

        clif_skillinfoblock (pl_sd);
        return 0;
    }
    else
        clif_displaymessage (fd, "Character not found.");

    return -1;
}

int
atcommand_log (const int UNUSED, struct map_session_data *UNUSED,
               const char *UNUSED, const char *UNUSED)
{
    return 0;                   // only used for (implicit) logging
}

int
atcommand_tee (const int UNUSED, struct map_session_data *sd,
               const char *UNUSED, const char *message)
{
    char data[strlen (message) + 28];
    strcpy (data, sd->status.name);
    strcat (data, " : ");
    strcat (data, message);
    clif_message (&sd->bl, data);
    return 0;
}

int
atcommand_invisible (const int UNUSED, struct map_session_data *sd,
                     const char *UNUSED, const char *UNUSED)
{
    pc_invisibility (sd, 1);
    return 0;
}

int
atcommand_visible (const int UNUSED, struct map_session_data *sd,
                   const char *UNUSED, const char *UNUSED)
{
    pc_invisibility (sd, 0);
    return 0;
}

static int atcommand_jump_iterate (const int fd, struct map_session_data *sd,
                            const char *UNUSED, const char *UNUSED,
                            struct map_session_data *(*get_start) (void),
                            struct map_session_data *(*get_next) (struct
                                                                  map_session_data
                                                                  * current))
{
    char output[200];
    struct map_session_data *pl_sd;

    memset (output, '\0', sizeof (output));

    pl_sd = (struct map_session_data *) map_id2bl (sd->followtarget);

    if (pl_sd)
        pl_sd = get_next (pl_sd);

    if (!pl_sd)
        pl_sd = get_start ();

    if (pl_sd == sd)
    {
        pl_sd = get_next (pl_sd);
        if (!pl_sd)
            pl_sd = get_start ();
    }

    if (maps[pl_sd->bl.m].flag.nowarpto
        && battle_config.any_warp_GM_min_level > pc_isGM (sd))
    {
        clif_displaymessage (fd,
                             "You are not authorised to warp you to the map of this player.");
        return -1;
    }
    if (maps[sd->bl.m].flag.nowarp
        && battle_config.any_warp_GM_min_level > pc_isGM (sd))
    {
        clif_displaymessage (fd,
                             "You are not authorised to warp you from your actual map.");
        return -1;
    }
    pc_setpos (sd, maps[pl_sd->bl.m].name, pl_sd->bl.x, pl_sd->bl.y, 3);
    sprintf (output, "Jump to %s", pl_sd->status.name);
    clif_displaymessage (fd, output);

    sd->followtarget = pl_sd->bl.id;

    return 0;
}

int
atcommand_iterate_forward_over_players (const int fd,
                                        struct map_session_data *sd,
                                        const char *command,
                                        const char *message)
{
    return atcommand_jump_iterate (fd, sd, command, message,
                                   map_get_first_session,
                                   map_get_next_session);
}

int
atcommand_iterate_backwards_over_players (const int fd,
                                          struct map_session_data *sd,
                                          const char *command,
                                          const char *message)
{
    return atcommand_jump_iterate (fd, sd, command, message,
                                   map_get_last_session,
                                   map_get_prev_session);
}

int atcommand_wgm (const int fd, struct map_session_data *sd,
                   const char *UNUSED, const char *message)
{
    tmw_GmHackMsg ("%s: %s", sd->status.name, message);
    if (!pc_isGM (sd))
        clif_displaymessage (fd, "Message sent.");

    return 0;
}


int atcommand_skillpool_info (const int fd, struct map_session_data *UNUSED,
                              const char *UNUSED, const char *message)
{
    char character[100];
    struct map_session_data *pl_sd;

    if (!message || !*message || sscanf (message, "%99s", character) < 1)
    {
        clif_displaymessage (fd, "Usage: @sp-info <char_name>");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        char buf[200];
        int  pool_skills[MAX_SKILL_POOL];
        int  pool_skills_nr = skill_pool (pl_sd, pool_skills);
        int  i;

        sprintf (buf, "Active skills %d out of %d for %s:", pool_skills_nr,
                 skill_pool_max (pl_sd), character);
        clif_displaymessage (fd, buf);
        for (i = 0; i < pool_skills_nr; ++i)
        {
            sprintf (buf, " - %s [%d]: power %d", skill_name (pool_skills[i]),
                     pool_skills[i], skill_power (pl_sd, pool_skills[i]));
            clif_displaymessage (fd, buf);
        }

        sprintf (buf, "Learned skills out of %d for %s:",
                 skill_pool_skills_size, character);
        clif_displaymessage (fd, buf);

        for (i = 0; i < skill_pool_skills_size; ++i)
        {
            const char *name = skill_name (skill_pool_skills[i]);
            int  lvl = pl_sd->status.skill[skill_pool_skills[i]].lv;

            if (lvl)
            {
                sprintf (buf, " - %s [%d]: lvl %d", name,
                         skill_pool_skills[i], lvl);
                clif_displaymessage (fd, buf);
            }
        }

    }
    else
        clif_displaymessage (fd, "Character not found.");

    return 0;
}

int atcommand_skillpool_focus (const int fd, struct map_session_data *UNUSED,
                               const char *UNUSED, const char *message)
{
    char character[100];
    int  skill;
    struct map_session_data *pl_sd;

    if (!message || !*message
        || sscanf (message, "%d %99[^\n]", &skill, character) < 1)
    {
        clif_displaymessage (fd, "Usage: @sp-focus <skill-nr> <char_name>");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (skill_pool_activate (pl_sd, skill))
            clif_displaymessage (fd, "Activation failed.");
        else
            clif_displaymessage (fd, "Activation successful.");
    }
    else
        clif_displaymessage (fd, "Character not found.");

    return 0;
}

int atcommand_skillpool_unfocus (const int fd, struct map_session_data *UNUSED,
                                 const char *UNUSED, const char *message)
{
    char character[100];
    int  skill;
    struct map_session_data *pl_sd;

    if (!message || !*message
        || sscanf (message, "%d %99[^\n]", &skill, character) < 1)
    {
        clif_displaymessage (fd, "Usage: @sp-unfocus <skill-nr> <char_name>");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        if (skill_pool_deactivate (pl_sd, skill))
            clif_displaymessage (fd, "Deactivation failed.");
        else
            clif_displaymessage (fd, "Deactivation successful.");
    }
    else
        clif_displaymessage (fd, "Character not found.");

    return 0;
}

int atcommand_skill_learn (const int fd, struct map_session_data *UNUSED,
                           const char *UNUSED, const char *message)
{
    char character[100];
    int  skill, level;
    struct map_session_data *pl_sd;

    if (!message || !*message
        || sscanf (message, "%d %d %99[^\n]", &skill, &level, character) < 1)
    {
        clif_displaymessage (fd,
                             "Usage: @skill-learn <skill-nr> <level> <char_name>");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) != NULL)
    {
        set_skill (pl_sd, skill, level);
        clif_skillinfoblock (pl_sd);
    }
    else
        clif_displaymessage (fd, "Character not found.");

    return 0;
}

int atcommand_ipcheck (const int fd, struct map_session_data *UNUSED,
                       const char *UNUSED, const char *message)
{
    struct map_session_data *pl_sd;
    struct sockaddr_in sai;
    char output[200];
    char character[25];
    int i;
    socklen_t sa_len = sizeof (struct sockaddr);
    unsigned long ip;

    memset(character, '\0', sizeof(character));

    if (sscanf (message, "%24[^\n]", character) < 1)
    {
        clif_displaymessage (fd, "Usage: @ipcheck <char name>");
        return -1;
    }

    if ((pl_sd = map_nick2sd (character)) == NULL)
    {
        clif_displaymessage (fd, "Character not found.");
        return -1;
    }

    if (getpeername (pl_sd->fd, (struct sockaddr *)&sai, &sa_len))
    {
        clif_displaymessage (fd,
                             "Guru Meditation Error: getpeername() failed");
        return -1;
    }

    ip = sai.sin_addr.s_addr;

    // We now have the IP address of a character.
    // Loop over all logged in sessions looking for matches.

    for (i = 0; i < fd_max; i++)
    {
        if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
            && pl_sd->state.auth)
        {
            if (getpeername (pl_sd->fd, (struct sockaddr *)&sai, &sa_len))
                continue;

            // Is checking GM levels really needed here?
            if (ip == sai.sin_addr.s_addr)
            {
                snprintf (output, sizeof(output),
                         "Name: %s | Location: %s %d %d",
                         pl_sd->status.name, pl_sd->mapname,
                         pl_sd->bl.x, pl_sd->bl.y);
                clif_displaymessage (fd, output);
            }
        }
    }

    clif_displaymessage (fd, "End of list");
    return 0;
}
