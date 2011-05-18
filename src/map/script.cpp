#include "script.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <time.h>
#include <math.h>

#include "../common/socket.hpp"
#include "../common/timer.hpp"
#include "../common/lock.hpp"
#include "../common/mt_rand.hpp"

#include "atcommand.hpp"
#include "battle.hpp"
#include "chrif.hpp"
#include "clif.hpp"
#include "../common/db.hpp"
#include "intif.hpp"
#include "itemdb.hpp"
#include "../common/lock.hpp"
#include "map.hpp"
#include "mob.hpp"
#include "npc.hpp"
#include "party.hpp"
#include "pc.hpp"
#include "skill.hpp"
#include "storage.hpp"

#define SCRIPT_BLOCK_SIZE 256
enum
{ LABEL_NEXTLINE = 1, LABEL_START };
static unsigned char *script_buf;
static int script_pos, script_size;

char *str_buf;
int  str_pos, str_size;
static struct str_data_t
{
    int  type;
    int  str;
    int  backpatch;
    int  label;
    int  (*func) (struct script_state *);
    int  val;
    int  next;
}   *str_data;
int  str_num = LABEL_START, str_data_size;
int  str_hash[16];

static struct dbt *mapreg_db = NULL;
static struct dbt *mapregstr_db = NULL;
static int mapreg_dirty = -1;
char mapreg_txt[256] = "save/mapreg.txt";
#define MAPREG_AUTOSAVE_INTERVAL	(10*1000)

static struct dbt *scriptlabel_db = NULL;
static struct dbt *userfunc_db = NULL;

struct dbt *script_get_label_db (void)
{
    return scriptlabel_db;
}

struct dbt *script_get_userfunc_db (void)
{
    if (!userfunc_db)
        userfunc_db = strdb_init ();
    return userfunc_db;
}

static char epos[][100] =
{
    "Head", "Body", "Left hand", "Right hand", "Robe", "Shoes", "Accessory 1",
    "Accessory 2", "Head 2", "Head 3", "Not Equipped"
};

static struct Script_Config
{
    int  warn_func_no_comma;
    int  warn_cmd_no_comma;
    int  warn_func_mismatch_paramnum;
    int  warn_cmd_mismatch_paramnum;
    int  check_cmdcount;
    int  check_gotocount;
} script_config;
static int parse_cmd_if = 0;
static int parse_cmd;

/*==========================================
 * ローカルプロトタイプ宣言 (必要な物のみ)
 *------------------------------------------
 */
unsigned char *parse_subexpr (unsigned char *, int);
static int buildin_mes (struct script_state *st);
static int buildin_goto (struct script_state *st);
static int buildin_callsub (struct script_state *st);
static int buildin_callfunc (struct script_state *st);
static int buildin_return (struct script_state *st);
static int buildin_getarg (struct script_state *st);
static int buildin_next (struct script_state *st);
static int buildin_close (struct script_state *st);
static int buildin_close2 (struct script_state *st);
static int buildin_menu (struct script_state *st);
static int buildin_rand (struct script_state *st);
static int buildin_pow (struct script_state *st);
static int buildin_warp (struct script_state *st);
static int buildin_isat (struct script_state *st);
static int buildin_areawarp (struct script_state *st);
static int buildin_heal (struct script_state *st);
static int buildin_itemheal (struct script_state *st);
static int buildin_percentheal (struct script_state *st);
static int buildin_input (struct script_state *st);
static int buildin_setlook (struct script_state *st);
static int buildin_set (struct script_state *st);
static int buildin_setarray (struct script_state *st);
static int buildin_cleararray (struct script_state *st);
static int buildin_copyarray (struct script_state *st);
static int buildin_getarraysize (struct script_state *st);
static int buildin_deletearray (struct script_state *st);
static int buildin_getelementofarray (struct script_state *st);
static int buildin_if (struct script_state *st);
static int buildin_getitem (struct script_state *st);
static int buildin_getitem2 (struct script_state *st);
static int buildin_makeitem (struct script_state *st);
static int buildin_delitem (struct script_state *st);
static int buildin_countitem (struct script_state *st);
static int buildin_checkweight (struct script_state *st);
static int buildin_readparam (struct script_state *st);
static int buildin_getcharid (struct script_state *st);
static int buildin_getpartyname (struct script_state *st);
static int buildin_getpartymember (struct script_state *st);
static int buildin_strcharinfo (struct script_state *st);
static int buildin_getequipid (struct script_state *st);
static int buildin_getequipname (struct script_state *st);
static int buildin_getbrokenid (struct script_state *st); // [Valaris]
static int buildin_repair (struct script_state *st);  // [Valaris]
static int buildin_getequipisequiped (struct script_state *st);
static int buildin_getequipisenableref (struct script_state *st);
static int buildin_getequipisidentify (struct script_state *st);
static int buildin_getequiprefinerycnt (struct script_state *st);
static int buildin_getequipweaponlv (struct script_state *st);
static int buildin_getequippercentrefinery (struct script_state *st);
static int buildin_successrefitem (struct script_state *st);
static int buildin_failedrefitem (struct script_state *st);
static int buildin_statusup (struct script_state *st);
static int buildin_statusup2 (struct script_state *st);
static int buildin_bonus (struct script_state *st);
static int buildin_bonus2 (struct script_state *st);
static int buildin_bonus3 (struct script_state *st);
static int buildin_skill (struct script_state *st);
static int buildin_setskill (struct script_state *st);
static int buildin_getskilllv (struct script_state *st);
static int buildin_basicskillcheck (struct script_state *st);
static int buildin_getgmlevel (struct script_state *st);
static int buildin_end (struct script_state *st);
static int buildin_getopt2 (struct script_state *st);
static int buildin_setopt2 (struct script_state *st);
static int buildin_checkoption (struct script_state *st);
static int buildin_setoption (struct script_state *st);
static int buildin_savepoint (struct script_state *st);
static int buildin_gettimetick (struct script_state *st);
static int buildin_gettime (struct script_state *st);
static int buildin_gettimestr (struct script_state *st) __attribute__((deprecated));
static int buildin_openstorage (struct script_state *st);
static int buildin_itemskill (struct script_state *st);
static int buildin_monster (struct script_state *st);
static int buildin_areamonster (struct script_state *st);
static int buildin_killmonster (struct script_state *st);
static int buildin_killmonsterall (struct script_state *st);
static int buildin_doevent (struct script_state *st);
static int buildin_donpcevent (struct script_state *st);
static int buildin_addtimer (struct script_state *st);
static int buildin_deltimer (struct script_state *st);
static int buildin_addtimercount (struct script_state *st);
static int buildin_initnpctimer (struct script_state *st);
static int buildin_stopnpctimer (struct script_state *st);
static int buildin_startnpctimer (struct script_state *st);
static int buildin_setnpctimer (struct script_state *st);
static int buildin_getnpctimer (struct script_state *st);
static int buildin_announce (struct script_state *st);
static int buildin_mapannounce (struct script_state *st);
static int buildin_areaannounce (struct script_state *st);
static int buildin_getusers (struct script_state *st);
static int buildin_getmapusers (struct script_state *st);
static int buildin_getareausers (struct script_state *st);
static int buildin_getareadropitem (struct script_state *st);
static int buildin_enablenpc (struct script_state *st);
static int buildin_disablenpc (struct script_state *st);
static int buildin_hideoffnpc (struct script_state *st);
static int buildin_hideonnpc (struct script_state *st);
static int buildin_sc_start (struct script_state *st);
static int buildin_sc_start2 (struct script_state *st);
static int buildin_sc_end (struct script_state *st);
static int buildin_sc_check (struct script_state *st);    // [Fate]
static int buildin_getscrate (struct script_state *st);
static int buildin_debugmes (struct script_state *st);
static int buildin_resetlvl (struct script_state *st);
static int buildin_resetstatus (struct script_state *st);
static int buildin_resetskill (struct script_state *st);
static int buildin_changesex (struct script_state *st);
static int buildin_attachrid (struct script_state *st);
static int buildin_detachrid (struct script_state *st);
static int buildin_isloggedin (struct script_state *st);
static int buildin_setmapflagnosave (struct script_state *st);
static int buildin_setmapflag (struct script_state *st);
static int buildin_removemapflag (struct script_state *st);
static int buildin_pvpon (struct script_state *st);
static int buildin_pvpoff (struct script_state *st);
static int buildin_emotion (struct script_state *st);
static int buildin_getequipcardcnt (struct script_state *st);
static int buildin_successremovecards (struct script_state *st);
static int buildin_failedremovecards (struct script_state *st);
static int buildin_marriage (struct script_state *st);
static int buildin_wedding_effect (struct script_state *st);
static int buildin_divorce (struct script_state *st);
static int buildin_getitemname (struct script_state *st);
static int buildin_getspellinvocation (struct script_state *st);  // [Fate]
static int buildin_getanchorinvocation (struct script_state *st); // [Fate]
static int buildin_getexp (struct script_state *st);
static int buildin_getinventorylist (struct script_state *st);
static int buildin_getskilllist (struct script_state *st);
static int buildin_get_pool_skills (struct script_state *st); // [fate]
static int buildin_get_activated_pool_skills (struct script_state *st);   // [fate]
static int buildin_get_unactivated_pool_skills (struct script_state *st);   // [PO]
static int buildin_activate_pool_skill (struct script_state *st); // [fate]
static int buildin_deactivate_pool_skill (struct script_state *st);   // [fate]
static int buildin_check_pool_skill (struct script_state *st);    // [fate]
static int buildin_clearitem (struct script_state *st);
static int buildin_misceffect (struct script_state *st);
static int buildin_soundeffect (struct script_state *st);
static int buildin_mapwarp (struct script_state *st);
static int buildin_inittimer (struct script_state *st);
static int buildin_stoptimer (struct script_state *st);
static int buildin_cmdothernpc (struct script_state *st);
static int buildin_mobcount (struct script_state *st);
static int buildin_strmobinfo (struct script_state *st);  // Script for displaying mob info [Valaris]
static int buildin_specialeffect (struct script_state *st);   // special effect script [Valaris]
static int buildin_specialeffect2 (struct script_state *st);  // special effect script [Valaris]
static int buildin_nude (struct script_state *st);    // nude [Valaris]
static int buildin_gmcommand (struct script_state *st);   // [MouseJstr]
static int buildin_movenpc (struct script_state *st); // [MouseJstr]
static int buildin_npcwarp (struct script_state *st); // [remoitnane]
static int buildin_message (struct script_state *st); // [MouseJstr]
static int buildin_npctalk (struct script_state *st); // [Valaris]
static int buildin_hasitems (struct script_state *st);    // [Valaris]
static int buildin_getlook (struct script_state *st); //Lorky [Lupus]
static int buildin_getsavepoint (struct script_state *st);    //Lorky [Lupus]
static int buildin_getpartnerid (struct script_state *st);    // [Fate]
static int buildin_areatimer (struct script_state *st);   // [Jaxad0127]
static int buildin_isin (struct script_state *st);    // [Jaxad0127]
static int buildin_shop (struct script_state *st);    // [MadCamel]
static int buildin_isdead (struct script_state *st);  // [Jaxad0127]
static int buildin_fakenpcname (struct script_state *st); //[Kage]
static int buildin_unequip_by_id (struct script_state *st);   // [Freeyorp]
static int buildin_getx (struct script_state *st);  // [Kage]
static int buildin_gety (struct script_state *st);  // [Kage]

static void push_val (struct script_stack *stack, int type, int val);
static int run_func (struct script_state *st);

static int mapreg_setreg (int num, int val);
static int mapreg_setregstr (int num, const char *str);

struct builtin_function
{
    int  (*func) (struct script_state *);
    const char *name;
    const char *arg;
} buildin_func[] =
{
    {buildin_mes, "mes", "s"},
    {buildin_next, "next", ""},
    {buildin_close, "close", ""},
    {buildin_close2, "close2", ""},
    {buildin_menu, "menu", "*"},
    {buildin_goto, "goto", "l"},
    {buildin_callsub, "callsub", "i*"},
    {buildin_callfunc, "callfunc", "s*"},
    {buildin_return, "return", "*"},
    {buildin_getarg, "getarg", "i"},
    {buildin_input, "input", "*"},
    {buildin_warp, "warp", "sii"},
    {buildin_isat, "isat", "sii"},
    {buildin_areawarp, "areawarp", "siiiisii"},
    {buildin_setlook, "setlook", "ii"},
    {buildin_set, "set", "ii"},
    {buildin_setarray, "setarray", "ii*"},
    {buildin_cleararray, "cleararray", "iii"},
    {buildin_copyarray, "copyarray", "iii"},
    {buildin_getarraysize, "getarraysize", "i"},
    {buildin_deletearray, "deletearray", "ii"},
    {buildin_getelementofarray, "getelementofarray", "ii"},
    {buildin_if, "if", "i*"},
    {buildin_getitem, "getitem", "ii**"},
    {buildin_getitem2, "getitem2", "iiiiiiiii*"},
    {buildin_makeitem, "makeitem", "iisii"},
    {buildin_delitem, "delitem", "ii"},
    {buildin_heal, "heal", "ii"},
    {buildin_itemheal, "itemheal", "ii"},
    {buildin_percentheal, "percentheal", "ii"},
    {buildin_rand, "rand", "i*"},
    {buildin_pow, "pow", "ii"},
    {buildin_countitem, "countitem", "i"},
    {buildin_checkweight, "checkweight", "ii"},
    {buildin_readparam, "readparam", "i*"},
    {buildin_getcharid, "getcharid", "i*"},
    {buildin_getpartyname, "getpartyname", "i"},
    {buildin_getpartymember, "getpartymember", "i"},
    {buildin_strcharinfo, "strcharinfo", "i"},
    {buildin_getequipid, "getequipid", "i"},
    {buildin_getequipname, "getequipname", "i"},
    {buildin_getbrokenid, "getbrokenid", "i"},   // [Valaris]
    {buildin_repair, "repair", "i"}, // [Valaris]
    {buildin_getequipisequiped, "getequipisequiped", "i"},
    {buildin_getequipisenableref, "getequipisenableref", "i"},
    {buildin_getequipisidentify, "getequipisidentify", "i"},
    {buildin_getequiprefinerycnt, "getequiprefinerycnt", "i"},
    {buildin_getequipweaponlv, "getequipweaponlv", "i"},
    {buildin_getequippercentrefinery, "getequippercentrefinery", "i"},
    {buildin_successrefitem, "successrefitem", "i"},
    {buildin_failedrefitem, "failedrefitem", "i"},
    {buildin_statusup, "statusup", "i"},
    {buildin_statusup2, "statusup2", "ii"},
    {buildin_bonus, "bonus", "ii"},
    {buildin_bonus2, "bonus2", "iii"},
    {buildin_bonus3, "bonus3", "iiii"},
    {buildin_skill, "skill", "ii*"},
    {buildin_setskill, "setskill", "ii"},    // [Fate]
    {buildin_getskilllv, "getskilllv", "i"},
    {buildin_basicskillcheck, "basicskillcheck", "*"},
    {buildin_getgmlevel, "getgmlevel", "*"},
    {buildin_end, "end", ""},
    {buildin_getopt2, "getopt2", "i"},
    {buildin_setopt2, "setopt2", "i"},
    {buildin_end, "break", ""},
    {buildin_checkoption, "checkoption", "i"},
    {buildin_setoption, "setoption", "i"},
    {buildin_savepoint, "save", "sii"},
    {buildin_savepoint, "savepoint", "sii"},
    {buildin_gettimetick, "gettimetick", "i"},
    {buildin_gettime, "gettime", "i"},
    {buildin_gettimestr, "gettimestr", "si"},
    {buildin_openstorage, "openstorage", "*"},
    {buildin_itemskill, "itemskill", "iis"},
    {buildin_monster, "monster", "siisii*"},
    {buildin_areamonster, "areamonster", "siiiisii*"},
    {buildin_killmonster, "killmonster", "ss"},
    {buildin_killmonsterall, "killmonsterall", "s"},
    {buildin_doevent, "doevent", "s"},
    {buildin_donpcevent, "donpcevent", "s"},
    {buildin_addtimer, "addtimer", "is"},
    {buildin_deltimer, "deltimer", "s"},
    {buildin_addtimercount, "addtimercount", "si"},
    {buildin_initnpctimer, "initnpctimer", "*"},
    {buildin_stopnpctimer, "stopnpctimer", "*"},
    {buildin_startnpctimer, "startnpctimer", "*"},
    {buildin_setnpctimer, "setnpctimer", "*"},
    {buildin_getnpctimer, "getnpctimer", "i*"},
    {buildin_announce, "announce", "si"},
    {buildin_mapannounce, "mapannounce", "ssi"},
    {buildin_areaannounce, "areaannounce", "siiiisi"},
    {buildin_getusers, "getusers", "i"},
    {buildin_getmapusers, "getmapusers", "s"},
    {buildin_getareausers, "getareausers", "siiii"},
    {buildin_getareadropitem, "getareadropitem", "siiiii*"},
    {buildin_enablenpc, "enablenpc", "s"},
    {buildin_disablenpc, "disablenpc", "s"},
    {buildin_hideoffnpc, "hideoffnpc", "s"},
    {buildin_hideonnpc, "hideonnpc", "s"},
    {buildin_sc_start, "sc_start", "iii*"},
    {buildin_sc_start2, "sc_start2", "iiii*"},
    {buildin_sc_end, "sc_end", "i"},
    {buildin_sc_check, "sc_check", "i"},
    {buildin_getscrate, "getscrate", "ii*"},
    {buildin_debugmes, "debugmes", "s"},
    {buildin_resetlvl, "resetlvl", "i"},
    {buildin_resetstatus, "resetstatus", ""},
    {buildin_resetskill, "resetskill", ""},
    {buildin_changesex, "changesex", ""},
    {buildin_attachrid, "attachrid", "i"},
    {buildin_detachrid, "detachrid", ""},
    {buildin_isloggedin, "isloggedin", "i"},
    {buildin_setmapflagnosave, "setmapflagnosave", "ssii"},
    {buildin_setmapflag, "setmapflag", "si"},
    {buildin_removemapflag, "removemapflag", "si"},
    {buildin_pvpon, "pvpon", "s"},
    {buildin_pvpoff, "pvpoff", "s"},
    {buildin_emotion, "emotion", "i"},
    {buildin_getequipcardcnt, "getequipcardcnt", "i"},
    {buildin_successremovecards, "successremovecards", "i"},
    {buildin_failedremovecards, "failedremovecards", "ii"},
    {buildin_marriage, "marriage", "s"},
    {buildin_wedding_effect, "wedding", ""},
    {buildin_divorce, "divorce", "i"},
    {buildin_getitemname, "getitemname", "*"},
    {buildin_getspellinvocation, "getspellinvocation", "s"},
    {buildin_getanchorinvocation, "getanchorinvocation", "s"},
    {buildin_getpartnerid, "getpartnerid2", "i"},
    {buildin_getexp, "getexp", "ii"},
    {buildin_getinventorylist, "getinventorylist", ""},
    {buildin_getskilllist, "getskilllist", ""},
    {buildin_get_pool_skills, "getpoolskilllist", ""},
    {buildin_get_activated_pool_skills, "getactivatedpoolskilllist", ""},
    {buildin_get_unactivated_pool_skills, "getunactivatedpoolskilllist", ""},
    {buildin_activate_pool_skill, "poolskill", "i"},
    {buildin_deactivate_pool_skill, "unpoolskill", "i"},
    {buildin_check_pool_skill, "checkpoolskill", "i"},
    {buildin_clearitem, "clearitem", ""},
    {buildin_misceffect, "misceffect", "i*"},
    {buildin_soundeffect, "soundeffect", "si"},
    {buildin_strmobinfo, "strmobinfo", "ii"},    // display mob data [Valaris]
    {buildin_specialeffect, "specialeffect", "i"},   // npc skill effect [Valaris]
    {buildin_specialeffect2, "specialeffect2", "i"}, // skill effect on players[Valaris]
    {buildin_nude, "nude", ""},  // nude command [Valaris]
    {buildin_mapwarp, "mapwarp", "ssii"},    // Added by RoVeRT
    {buildin_inittimer, "inittimer", ""},
    {buildin_stoptimer, "stoptimer", ""},
    {buildin_cmdothernpc, "cmdothernpc", "ss"},
    {buildin_gmcommand, "gmcommand", "*"},   // [MouseJstr]
    {buildin_movenpc,"movenpc","siis"}, // [MouseJstr]
    {buildin_npcwarp, "npcwarp", "iis"}, // [remoitnane]
    {buildin_message, "message", "s*"},  // [MouseJstr]
    {buildin_npctalk, "npctalk", "*"},   // [Valaris]
    {buildin_hasitems, "hasitems", "*"}, // [Valaris]
    {buildin_mobcount, "mobcount", "ss"},
    {buildin_getlook, "getlook", "i"},
    {buildin_getsavepoint, "getsavepoint", "i"},
    {buildin_areatimer, "areatimer", "siiiiis"},
    {buildin_isin, "isin", "siiii"},
    {buildin_shop, "shop", "s"},
    {buildin_isdead, "isdead", "i"},
    {buildin_fakenpcname, "fakenpcname", "ssi"},
    {buildin_unequip_by_id, "unequipbyid", "i"}, // [Freeyorp]
    {buildin_getx, "getx", "i"}, // [Kage]
    {buildin_gety, "gety", "i"}, // [Kage]
        // End Additions
    {NULL, NULL, NULL},
};

enum
{
    C_NOP, C_POS, C_INT, C_PARAM, C_FUNC, C_STR, C_CONSTSTR, C_ARG,
    C_NAME, C_EOL, C_RETINFO,

    C_LOR, C_LAND, C_LE, C_LT, C_GE, C_GT, C_EQ, C_NE,  //operator
    C_XOR, C_OR, C_AND, C_ADD, C_SUB, C_MUL, C_DIV, C_MOD, C_NEG, C_LNOT,
    C_NOT, C_R_SHIFT, C_L_SHIFT
};

/*==========================================
 * 文字列のハッシュを計算
 *------------------------------------------
 */
static int calc_hash (const unsigned char *p)
{
    int  h = 0;
    while (*p)
    {
        h = (h << 1) + (h >> 3) + (h >> 5) + (h >> 8);
        h += *p++;
    }
    return h & 15;
}

/*==========================================
 * str_dataの中に名前があるか検索する
 *------------------------------------------
 */
// 既存のであれば番号、無ければ-1
static int search_str (const char *p)
{
    int  i;
    i = str_hash[calc_hash ((const unsigned char *)p)];
    while (i)
    {
        if (strcmp (str_buf + str_data[i].str, p) == 0)
        {
            return i;
        }
        i = str_data[i].next;
    }
    return -1;
}

/*==========================================
 * str_dataに名前を登録
 *------------------------------------------
 */
// 既存のであれば番号、無ければ登録して新規番号
static int add_str (const char *p)
{
    int  i;
    char *lowcase;

    lowcase = strdup (p);
    for (i = 0; lowcase[i]; i++)
        lowcase[i] = tolower (lowcase[i]);
    if ((i = search_str (lowcase)) >= 0)
    {
        free (lowcase);
        return i;
    }
    free (lowcase);

    i = calc_hash ((const unsigned char *)p);
    if (str_hash[i] == 0)
    {
        str_hash[i] = str_num;
    }
    else
    {
        i = str_hash[i];
        for (;;)
        {
            if (strcmp (str_buf + str_data[i].str, p) == 0)
            {
                return i;
            }
            if (str_data[i].next == 0)
                break;
            i = str_data[i].next;
        }
        str_data[i].next = str_num;
    }
    if (str_num >= str_data_size)
    {
        str_data_size += 128;
        RECREATE (str_data, struct str_data_t, str_data_size);
        memset (str_data + (str_data_size - 128), '\0', 128);
    }
    while (str_pos + strlen (p) + 1 >= str_size)
    {
        str_size += 256;
        str_buf = (char *) realloc (str_buf, str_size);
        memset (str_buf + (str_size - 256), '\0', 256);
    }
    strcpy (str_buf + str_pos, p);
    str_data[str_num].type = C_NOP;
    str_data[str_num].str = str_pos;
    str_data[str_num].next = 0;
    str_data[str_num].func = NULL;
    str_data[str_num].backpatch = -1;
    str_data[str_num].label = -1;
    str_pos += strlen (p) + 1;
    return str_num++;
}

/*==========================================
 * スクリプトバッファサイズの確認と拡張
 *------------------------------------------
 */
static void check_script_buf (int size)
{
    if (script_pos + size >= script_size)
    {
        script_size += SCRIPT_BLOCK_SIZE;
        script_buf = (unsigned char *) realloc (script_buf, script_size);
        memset (script_buf + script_size - SCRIPT_BLOCK_SIZE, '\0',
                SCRIPT_BLOCK_SIZE);
    }
}

/*==========================================
 * スクリプトバッファに１バイト書き込む
 *------------------------------------------
 */
static void add_scriptb (int a)
{
    check_script_buf (1);
    script_buf[script_pos++] = a;
}

/*==========================================
 * スクリプトバッファにデータタイプを書き込む
 *------------------------------------------
 */
static void add_scriptc (int a)
{
    while (a >= 0x40)
    {
        add_scriptb ((a & 0x3f) | 0x40);
        a = (a - 0x40) >> 6;
    }
    add_scriptb (a & 0x3f);
}

/*==========================================
 * スクリプトバッファに整数を書き込む
 *------------------------------------------
 */
static void add_scripti (int a)
{
    while (a >= 0x40)
    {
        add_scriptb (a | 0xc0);
        a = (a - 0x40) >> 6;
    }
    add_scriptb (a | 0x80);
}

/*==========================================
 * スクリプトバッファにラベル/変数/関数を書き込む
 *------------------------------------------
 */
// 最大16Mまで
static void add_scriptl (int l)
{
    int  backpatch = str_data[l].backpatch;

    switch (str_data[l].type)
    {
        case C_POS:
            add_scriptc (C_POS);
            add_scriptb (str_data[l].label);
            add_scriptb (str_data[l].label >> 8);
            add_scriptb (str_data[l].label >> 16);
            break;
        case C_NOP:
            // ラベルの可能性があるのでbackpatch用データ埋め込み
            add_scriptc (C_NAME);
            str_data[l].backpatch = script_pos;
            add_scriptb (backpatch);
            add_scriptb (backpatch >> 8);
            add_scriptb (backpatch >> 16);
            break;
        case C_INT:
            add_scripti (str_data[l].val);
            break;
        default:
            // もう他の用途と確定してるので数字をそのまま
            add_scriptc (C_NAME);
            add_scriptb (l);
            add_scriptb (l >> 8);
            add_scriptb (l >> 16);
            break;
    }
}

/*==========================================
 * ラベルを解決する
 *------------------------------------------
 */
static void set_label (int l, int pos)
{
    int  i, next;

    str_data[l].type = C_POS;
    str_data[l].label = pos;
    for (i = str_data[l].backpatch; i >= 0 && i != 0x00ffffff;)
    {
        next = (*(int *) (script_buf + i)) & 0x00ffffff;
        script_buf[i - 1] = C_POS;
        script_buf[i] = pos;
        script_buf[i + 1] = pos >> 8;
        script_buf[i + 2] = pos >> 16;
        i = next;
    }
}

/*==========================================
 * スペース/コメント読み飛ばし
 *------------------------------------------
 */
static unsigned char *skip_space (unsigned char *p)
{
    while (1)
    {
        while (isspace (*p))
            p++;
        if (p[0] == '/' && p[1] == '/')
        {
            while (*p && *p != '\n')
                p++;
        }
        else if (p[0] == '/' && p[1] == '*')
        {
            p++;
            while (*p && (p[-1] != '*' || p[0] != '/'))
                p++;
            if (*p)
                p++;
        }
        else
            break;
    }
    return p;
}

/*==========================================
 * １単語スキップ
 *------------------------------------------
 */
static unsigned char *skip_word (unsigned char *p)
{
    // prefix
    if (*p == '$')
        p++;                    // MAP鯖内共有変数用
    if (*p == '@')
        p++;                    // 一時的変数用(like weiss)
    if (*p == '#')
        p++;                    // account変数用
    if (*p == '#')
        p++;                    // ワールドaccount変数用
    if (*p == 'l')
        p++;                    // 一時的変数用(like weiss)

    while (isalnum (*p) || *p == '_' || *p >= 0x81)
        if (*p >= 0x81 && p[1])
        {
            p += 2;
        }
        else
            p++;

    // postfix
    if (*p == '$')
        p++;                    // 文字列変数

    return p;
}

static script_ptr startptr;
static int startline;

/*==========================================
 * エラーメッセージ出力
 *------------------------------------------
 */
static void disp_error_message (const char *mes, script_ptr pos)
{
    int  line, c = 0, i;
    script_ptr p, linestart, lineend;

    for (line = startline, p = startptr; p && *p; line++)
    {
        linestart = p;
        lineend = (script_ptr)strchr ((char *)p, '\n');
        if (lineend)
        {
            c = *lineend;
            *lineend = 0;
        }
        if (lineend == NULL || pos < lineend)
        {
            printf ("%s line %d : ", mes, line);
            for (i = 0;
                 (linestart[i] != '\r') && (linestart[i] != '\n')
                 && linestart[i]; i++)
            {
                if (linestart + i != pos)
                    printf ("%c", linestart[i]);
                else
                    printf ("\'%c\'", linestart[i]);
            }
            printf ("\a\n");
            if (lineend)
                *lineend = c;
            return;
        }
        *lineend = c;
        p = lineend + 1;
    }
}

/*==========================================
 * 項の解析
 *------------------------------------------
 */
static unsigned char *parse_simpleexpr (unsigned char *p)
{
    int  i;
    p = skip_space (p);

    if (*p == ';' || *p == ',')
    {
        disp_error_message ("unexpected expr end", p);
        exit (1);
    }
    if (*p == '(')
    {

        p = parse_subexpr (p + 1, -1);
        p = skip_space (p);
        if ((*p++) != ')')
        {
            disp_error_message ("unmatch ')'", p);
            exit (1);
        }
    }
    else if (isdigit (*p) || ((*p == '-' || *p == '+') && isdigit (p[1])))
    {
        script_ptr np;
        i = strtoul ((char *)p, (char **)&np, 0);
        add_scripti (i);
        p = np;
    }
    else if (*p == '"')
    {
        add_scriptc (C_STR);
        p++;
        while (*p && *p != '"')
        {
            if (p[-1] <= 0x7e && *p == '\\')
                p++;
            else if (*p == '\n')
            {
                disp_error_message ("unexpected newline @ string", p);
                exit (1);
            }
            add_scriptb (*p++);
        }
        if (!*p)
        {
            disp_error_message ("unexpected eof @ string", p);
            exit (1);
        }
        add_scriptb (0);
        p++;                    //'"'
    }
    else
    {
        int  c, l;
        script_ptr p2;
        // label , register , function etc
        if (skip_word (p) == p)
        {
            disp_error_message ("unexpected character", p);
            exit (1);
        }
        p2 = skip_word (p);
        c = *p2;
        *p2 = 0;                // 名前をadd_strする
        l = add_str ((char *)p);

        parse_cmd = l;          // warn_*_mismatch_paramnumのために必要
        if (l == search_str ("if")) // warn_cmd_no_commaのために必要
            parse_cmd_if++;
/*
		// 廃止予定のl14/l15,およびプレフィックスｌの警告
		if(	strcmp(str_buf+str_data[l].str,"l14")==0 ||
			strcmp(str_buf+str_data[l].str,"l15")==0 ){
			disp_error_message("l14 and l15 is DEPRECATED. use @menu instead of l15.",p);
		}else if(str_buf[str_data[l].str]=='l'){
			disp_error_message("prefix 'l' is DEPRECATED. use prefix '@' instead.",p2);
		}
*/
        *p2 = c;
        p = p2;

        if (str_data[l].type != C_FUNC && c == '[')
        {
            // array(name[i] => getelementofarray(name,i) )
            add_scriptl (search_str ("getelementofarray"));
            add_scriptc (C_ARG);
            add_scriptl (l);
            p = parse_subexpr (p + 1, -1);
            p = skip_space (p);
            if ((*p++) != ']')
            {
                disp_error_message ("unmatch ']'", p);
                exit (1);
            }
            add_scriptc (C_FUNC);
        }
        else
            add_scriptl (l);

    }

    return p;
}

/*==========================================
 * 式の解析
 *------------------------------------------
 */
unsigned char *parse_subexpr (unsigned char *p, int limit)
{
    int  op, opl, len;
    script_ptr tmpp;

    p = skip_space (p);

    if (*p == '-')
    {
        tmpp = skip_space (p + 1);
        if (*tmpp == ';' || *tmpp == ',')
        {
            add_scriptl (LABEL_NEXTLINE);
            p++;
            return p;
        }
    }
    tmpp = p;
    if ((op = C_NEG, *p == '-') || (op = C_LNOT, *p == '!')
        || (op = C_NOT, *p == '~'))
    {
        p = parse_subexpr (p + 1, 100);
        add_scriptc (op);
    }
    else
        p = parse_simpleexpr (p);
    p = skip_space (p);
    while (((op = C_ADD, opl = 6, len = 1, *p == '+') ||
            (op = C_SUB, opl = 6, len = 1, *p == '-') ||
            (op = C_MUL, opl = 7, len = 1, *p == '*') ||
            (op = C_DIV, opl = 7, len = 1, *p == '/') ||
            (op = C_MOD, opl = 7, len = 1, *p == '%') ||
            (op = C_FUNC, opl = 8, len = 1, *p == '(') ||
            (op = C_LAND, opl = 1, len = 2, *p == '&' && p[1] == '&') ||
            (op = C_AND, opl = 5, len = 1, *p == '&') ||
            (op = C_LOR, opl = 0, len = 2, *p == '|' && p[1] == '|') ||
            (op = C_OR, opl = 4, len = 1, *p == '|') ||
            (op = C_XOR, opl = 3, len = 1, *p == '^') ||
            (op = C_EQ, opl = 2, len = 2, *p == '=' && p[1] == '=') ||
            (op = C_NE, opl = 2, len = 2, *p == '!' && p[1] == '=') ||
            (op = C_R_SHIFT, opl = 5, len = 2, *p == '>' && p[1] == '>') ||
            (op = C_GE, opl = 2, len = 2, *p == '>' && p[1] == '=') ||
            (op = C_GT, opl = 2, len = 1, *p == '>') ||
            (op = C_L_SHIFT, opl = 5, len = 2, *p == '<' && p[1] == '<') ||
            (op = C_LE, opl = 2, len = 2, *p == '<' && p[1] == '=') ||
            (op = C_LT, opl = 2, len = 1, *p == '<')) && opl > limit)
    {
        p += len;
        if (op == C_FUNC)
        {
            int  i = 0, func = parse_cmd;
            script_ptr plist[128];

            if (str_data[func].type != C_FUNC)
            {
                disp_error_message ("expect function", tmpp);
                exit (0);
            }

            add_scriptc (C_ARG);
            do
            {
                plist[i] = p;
                p = parse_subexpr (p, -1);
                p = skip_space (p);
                if (*p == ',')
                    p++;
                else if (*p != ')' && script_config.warn_func_no_comma)
                {
                    disp_error_message ("expect ',' or ')' at func params",
                                        p);
                }
                p = skip_space (p);
                i++;
            }
            while (*p && *p != ')' && i < 128);
            plist[i] = p;
            if (*(p++) != ')')
            {
                disp_error_message ("func request '(' ')'", p);
                exit (1);
            }

            if (str_data[func].type == C_FUNC
                && script_config.warn_func_mismatch_paramnum)
            {
                const char *arg = buildin_func[str_data[func].val].arg;
                int  j = 0;
                for (j = 0; arg[j]; j++)
                    if (arg[j] == '*')
                        break;
                if ((arg[j] == 0 && i != j) || (arg[j] == '*' && i < j))
                {
                    disp_error_message ("illegal number of parameters",
                                        plist[(i < j) ? i : j]);
                }
            }
        }
        else
        {
            p = parse_subexpr (p, opl);
        }
        add_scriptc (op);
        p = skip_space (p);
    }
    return p;                   /* return first untreated operator */
}

/*==========================================
 * 式の評価
 *------------------------------------------
 */
static unsigned char *parse_expr (unsigned char *p)
{
    switch (*p)
    {
        case ')':
        case ';':
        case ':':
        case '[':
        case ']':
        case '}':
            disp_error_message ("unexpected char", p);
            exit (1);
    }
    p = parse_subexpr (p, -1);
    return p;
}

/*==========================================
 * 行の解析
 *------------------------------------------
 */
static unsigned char *parse_line (unsigned char *p)
{
    int  i = 0, cmd;
    script_ptr plist[128];
    script_ptr p2;

    p = skip_space (p);
    if (*p == ';')
        return p;

    parse_cmd_if = 0;           // warn_cmd_no_commaのために必要

    // 最初は関数名
    p2 = p;
    p = parse_simpleexpr (p);
    p = skip_space (p);

    cmd = parse_cmd;
    if (str_data[cmd].type != C_FUNC)
    {
        disp_error_message ("expect command", p2);
//      exit(0);
    }

    add_scriptc (C_ARG);
    while (p && *p && *p != ';' && i < 128)
    {
        plist[i] = p;

        p = parse_expr (p);
        p = skip_space (p);
        // 引数区切りの,処理
        if (*p == ',')
            p++;
        else if (*p != ';' && script_config.warn_cmd_no_comma
                 && parse_cmd_if * 2 <= i)
        {
            disp_error_message ("expect ',' or ';' at cmd params", p);
        }
        p = skip_space (p);
        i++;
    }
    plist[i] = p;
    if (!p || *(p++) != ';')
    {
        disp_error_message ("need ';'", p);
        exit (1);
    }
    add_scriptc (C_FUNC);

    if (str_data[cmd].type == C_FUNC
        && script_config.warn_cmd_mismatch_paramnum)
    {
        const char *arg = buildin_func[str_data[cmd].val].arg;
        int  j = 0;
        for (j = 0; arg[j]; j++)
            if (arg[j] == '*')
                break;
        if ((arg[j] == 0 && i != j) || (arg[j] == '*' && i < j))
        {
            disp_error_message ("illegal number of parameters",
                                plist[(i < j) ? i : j]);
        }
    }

    return p;
}

/*==========================================
 * 組み込み関数の追加
 *------------------------------------------
 */
static void add_buildin_func (void)
{
    int  i, n;
    for (i = 0; buildin_func[i].func; i++)
    {
        n = add_str (buildin_func[i].name);
        str_data[n].type = C_FUNC;
        str_data[n].val = i;
        str_data[n].func = buildin_func[i].func;
    }
}

/*==========================================
 * 定数データベースの読み込み
 *------------------------------------------
 */
static void read_constdb (void)
{
    FILE *fp;
    char line[1024], name[1024];
    int  val, n, i, type;

    fp = fopen_ ("db/const.txt", "r");
    if (fp == NULL)
    {
        printf ("can't read db/const.txt\n");
        return;
    }
    while (fgets (line, 1020, fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;
        type = 0;
        if (sscanf (line, "%[A-Za-z0-9_],%d,%d", name, &val, &type) >= 2 ||
            sscanf (line, "%[A-Za-z0-9_] %d %d", name, &val, &type) >= 2)
        {
            for (i = 0; name[i]; i++)
                name[i] = tolower (name[i]);
            n = add_str (name);
            if (type == 0)
                str_data[n].type = C_INT;
            else
                str_data[n].type = C_PARAM;
            str_data[n].val = val;
        }
    }
    fclose_ (fp);
}

/*==========================================
 * スクリプトの解析
 *------------------------------------------
 */
unsigned char *parse_script (unsigned char *src, int line)
{
    unsigned char *p, *tmpp;
    int  i;
    static int first = 1;

    if (first)
    {
        add_buildin_func ();
        read_constdb ();
    }
    first = 0;
    script_buf = (unsigned char *) calloc (SCRIPT_BLOCK_SIZE, 1);
    script_pos = 0;
    script_size = SCRIPT_BLOCK_SIZE;
    str_data[LABEL_NEXTLINE].type = C_NOP;
    str_data[LABEL_NEXTLINE].backpatch = -1;
    str_data[LABEL_NEXTLINE].label = -1;
    for (i = LABEL_START; i < str_num; i++)
    {
        if (str_data[i].type == C_POS || str_data[i].type == C_NAME)
        {
            str_data[i].type = C_NOP;
            str_data[i].backpatch = -1;
            str_data[i].label = -1;
        }
    }

    // 外部用label dbの初期化
    if (scriptlabel_db != NULL)
        strdb_final (scriptlabel_db, NULL);
    scriptlabel_db = strdb_init ();

    // for error message
    startptr = src;
    startline = line;

    p = src;
    p = skip_space (p);
    if (*p != '{')
    {
        disp_error_message ("not found '{'", p);
        return NULL;
    }
    for (p++; p && *p && *p != '}';)
    {
        p = skip_space (p);
        // labelだけ特殊処理
        tmpp = skip_space (skip_word (p));
        if (*tmpp == ':')
        {
            int  l, c;

            c = *skip_word (p);
            *skip_word (p) = 0;
            l = add_str ((char *)p);
            if (str_data[l].label != -1)
            {
                *skip_word (p) = c;
                disp_error_message ("dup label ", p);
                exit (1);
            }
            set_label (l, script_pos);
            strdb_insert (scriptlabel_db, (const char*)p, script_pos);   // 外部用label db登録
            *skip_word (p) = c;
            p = tmpp + 1;
            continue;
        }

        // 他は全部一緒くた
        p = parse_line (p);
        p = skip_space (p);
        add_scriptc (C_EOL);

        set_label (LABEL_NEXTLINE, script_pos);
        str_data[LABEL_NEXTLINE].type = C_NOP;
        str_data[LABEL_NEXTLINE].backpatch = -1;
        str_data[LABEL_NEXTLINE].label = -1;
    }

    add_scriptc (C_NOP);

    script_size = script_pos;
    script_buf = (unsigned char *) realloc (script_buf, script_pos + 1);

    // 未解決のラベルを解決
    for (i = LABEL_START; i < str_num; i++)
    {
        if (str_data[i].type == C_NOP)
        {
            int  j, next;
            str_data[i].type = C_NAME;
            str_data[i].label = i;
            for (j = str_data[i].backpatch; j >= 0 && j != 0x00ffffff;)
            {
                next = (*(int *) (script_buf + j)) & 0x00ffffff;
                script_buf[j] = i;
                script_buf[j + 1] = i >> 8;
                script_buf[j + 2] = i >> 16;
                j = next;
            }
        }
    }


    return script_buf;
}

//
// 実行系
//
enum
{ STOP = 1, END, RERUNLINE, GOTO, RETFUNC };

/*==========================================
 * ridからsdへの解決
 *------------------------------------------
 */
static struct map_session_data *script_rid2sd (struct script_state *st)
{
    struct map_session_data *sd = map_id2sd (st->rid);
    if (!sd)
    {
        printf ("script_rid2sd: fatal error ! player not attached!\n");
    }
    return sd;
}

/*==========================================
 * 変数の読み取り
 *------------------------------------------
 */
static int get_val (struct script_state *st, struct script_data *data)
{
    struct map_session_data *sd = NULL;
    if (data->type == C_NAME)
    {
        char *name = str_buf + str_data[data->u.num & 0x00ffffff].str;
        char prefix = *name;
        char postfix = name[strlen (name) - 1];

        if (prefix != '$')
        {
            if ((sd = script_rid2sd (st)) == NULL)
                printf ("get_val error name?:%s\n", name);
        }
        if (postfix == '$')
        {

            data->type = C_CONSTSTR;
            if (prefix == '@' || prefix == 'l')
            {
                if (sd)
                    data->u.str = pc_readregstr (sd, data->u.num);
            }
            else if (prefix == '$')
            {
                data->u.str =
                    (char *) numdb_search (mapregstr_db, data->u.num).p;
            }
            else
            {
                printf ("script: get_val: illegal scope string variable.\n");
                data->u.str = "!!ERROR!!";
            }
            if (data->u.str == NULL)
                data->u.str = "";

        }
        else
        {

            data->type = C_INT;
            if (str_data[data->u.num & 0x00ffffff].type == C_INT)
            {
                data->u.num = str_data[data->u.num & 0x00ffffff].val;
            }
            else if (str_data[data->u.num & 0x00ffffff].type == C_PARAM)
            {
                if (sd)
                    data->u.num =
                        pc_readparam (sd,
                                      str_data[data->u.num & 0x00ffffff].val);
            }
            else if (prefix == '@' || prefix == 'l')
            {
                if (sd)
                    data->u.num = pc_readreg (sd, data->u.num);
            }
            else if (prefix == '$')
            {
                data->u.num = numdb_search (mapreg_db, data->u.num).i;
            }
            else if (prefix == '#')
            {
                if (name[1] == '#')
                {
                    if (sd)
                        data->u.num = pc_readaccountreg2 (sd, name);
                }
                else
                {
                    if (sd)
                        data->u.num = pc_readaccountreg (sd, name);
                }
            }
            else
            {
                if (sd)
                    data->u.num = pc_readglobalreg (sd, name);
            }
        }
    }
    return 0;
}

/*==========================================
 * 変数の読み取り2
 *------------------------------------------
 */
static void *get_val2 (struct script_state *st, int num)
{
    struct script_data dat;
    dat.type = C_NAME;
    dat.u.num = num;
    get_val (st, &dat);
    if (dat.type == C_INT)
        return (void *) dat.u.num;
    else
        return (void *) dat.u.str;
}

/*==========================================
 * 変数設定用
 *------------------------------------------
 */
static int set_reg (struct map_session_data *sd, int num, const char *name, const void *v)
{
    const char prefix = *name;
    const char postfix = name[strlen (name) - 1];

    if (postfix == '$')
    {
        char *str = (char *) v;
        if (prefix == '@' || prefix == 'l')
        {
            pc_setregstr (sd, num, str);
        }
        else if (prefix == '$')
        {
            mapreg_setregstr (num, str);
        }
        else
        {
            printf ("script: set_reg: illegal scope string variable !");
        }
    }
    else
    {
        // 数値
        int  val = (int) v;
        if (str_data[num & 0x00ffffff].type == C_PARAM)
        {
            pc_setparam (sd, str_data[num & 0x00ffffff].val, val);
        }
        else if (prefix == '@' || prefix == 'l')
        {
            pc_setreg (sd, num, val);
        }
        else if (prefix == '$')
        {
            mapreg_setreg (num, val);
        }
        else if (prefix == '#')
        {
            if (name[1] == '#')
                pc_setaccountreg2 (sd, name, val);
            else
                pc_setaccountreg (sd, name, val);
        }
        else
        {
            pc_setglobalreg (sd, name, val);
        }
    }
    return 0;
}

/*==========================================
 * 文字列への変換
 *------------------------------------------
 */
static const char *conv_str (struct script_state *st, struct script_data *data)
{
    get_val (st, data);
    if (data->type == C_INT)
    {
        char *buf;
        buf = (char *) calloc (16, 1);
        sprintf (buf, "%d", data->u.num);
        data->type = C_STR;
        data->u.str = buf;
#if 1
    }
    else if (data->type == C_NAME)
    {
        // テンポラリ。本来無いはず
        data->type = C_CONSTSTR;
        data->u.str = str_buf + str_data[data->u.num].str;
#endif
    }
    return data->u.str;
}

/*==========================================
 * 数値へ変換
 *------------------------------------------
 */
static int conv_num (struct script_state *st, struct script_data *data)
{
    const char *p;
    get_val (st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        p = data->u.str;
        data->u.num = atoi (p);
        if (data->type == C_STR)
            free ((char *)p);
        data->type = C_INT;
    }
    return data->u.num;
}

/*==========================================
 * スタックへ数値をプッシュ
 *------------------------------------------
 */
void push_val (struct script_stack *stack, int type, int val)
{
    if (stack->sp >= stack->sp_max)
    {
        stack->sp_max += 64;
        stack->stack_data = (struct script_data *)
            realloc (stack->stack_data, sizeof (stack->stack_data[0]) *
                                        stack->sp_max);
        memset (stack->stack_data + (stack->sp_max - 64), 0,
                64 * sizeof (*(stack->stack_data)));
    }
//  if(battle_config.etc_log)
//      printf("push (%d,%d)-> %d\n",type,val,stack->sp);
    stack->stack_data[stack->sp].type = type;
    stack->stack_data[stack->sp].u.num = val;
    stack->sp++;
}

/*==========================================
 * スタックへ文字列をプッシュ
 *------------------------------------------
 */
static void push_str (struct script_stack *stack, int type, const char *str)
{
    if (stack->sp >= stack->sp_max)
    {
        stack->sp_max += 64;
        stack->stack_data = (struct script_data *)
            realloc (stack->stack_data, sizeof (stack->stack_data[0]) *
                                        stack->sp_max);
        memset (stack->stack_data + (stack->sp_max - 64), '\0',
                64 * sizeof (*(stack->stack_data)));
    }
//  if(battle_config.etc_log)
//      printf("push (%d,%x)-> %d\n",type,str,stack->sp);
    stack->stack_data[stack->sp].type = type;
    stack->stack_data[stack->sp].u.str = str;
    stack->sp++;
}

/*==========================================
 * スタックへ複製をプッシュ
 *------------------------------------------
 */
static void push_copy (struct script_stack *stack, int pos)
{
    switch (stack->stack_data[pos].type)
    {
        case C_CONSTSTR:
            push_str (stack, C_CONSTSTR, stack->stack_data[pos].u.str);
            break;
        case C_STR:
            push_str (stack, C_STR, strdup (stack->stack_data[pos].u.str));
            break;
        default:
            push_val (stack, stack->stack_data[pos].type,
                      stack->stack_data[pos].u.num);
            break;
    }
}

/*==========================================
 * スタックからポップ
 *------------------------------------------
 */
static void pop_stack (struct script_stack *stack, int start, int end)
{
    int  i;
    for (i = start; i < end; i++)
    {
        if (stack->stack_data[i].type == C_STR)
        {
            free ((char *)stack->stack_data[i].u.str);
        }
    }
    if (stack->sp > end)
    {
        memmove (&stack->stack_data[start], &stack->stack_data[end],
                 sizeof (stack->stack_data[0]) * (stack->sp - end));
    }
    stack->sp -= end - start;
}

//
// 埋め込み関数
//
/*==========================================
 *
 *------------------------------------------
 */
int buildin_mes (struct script_state *st)
{
    conv_str (st, &(st->stack->stack_data[st->start + 2]));
    clif_scriptmes (script_rid2sd (st), st->oid,
                    st->stack->stack_data[st->start + 2].u.str);
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_goto (struct script_state *st)
{
    int  pos;

    if (st->stack->stack_data[st->start + 2].type != C_POS)
    {
        printf ("script: goto: not label !\n");
        st->state = END;
        return 0;
    }

    pos = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    st->pos = pos;
    st->state = GOTO;
    return 0;
}

/*==========================================
 * ユーザー定義関数の呼び出し
 *------------------------------------------
 */
int buildin_callfunc (struct script_state *st)
{
    script_ptr scr;
    const char *str = conv_str (st, &(st->stack->stack_data[st->start + 2]));

    if ((scr = (script_ptr)strdb_search (script_get_userfunc_db (), str).p))
    {
        int  i, j;
        for (i = st->start + 3, j = 0; i < st->end; i++, j++)
            push_copy (st->stack, i);

        push_val (st->stack, C_INT, j); // 引数の数をプッシュ
        push_val (st->stack, C_INT, st->defsp); // 現在の基準スタックポインタをプッシュ
        push_val (st->stack, C_INT, (int) st->script);  // 現在のスクリプトをプッシュ
        push_val (st->stack, C_RETINFO, st->pos);   // 現在のスクリプト位置をプッシュ

        st->pos = 0;
        st->script = scr;
        st->defsp = st->start + 4 + j;
        st->state = GOTO;
    }
    else
    {
        printf ("script:callfunc: function not found! [%s]\n", str);
        st->state = END;
    }
    return 0;
}

/*==========================================
 * サブルーティンの呼び出し
 *------------------------------------------
 */
int buildin_callsub (struct script_state *st)
{
    int  pos = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    int  i, j;
    for (i = st->start + 3, j = 0; i < st->end; i++, j++)
        push_copy (st->stack, i);

    push_val (st->stack, C_INT, j); // 引数の数をプッシュ
    push_val (st->stack, C_INT, st->defsp); // 現在の基準スタックポインタをプッシュ
    push_val (st->stack, C_INT, (int) st->script);  // 現在のスクリプトをプッシュ
    push_val (st->stack, C_RETINFO, st->pos);   // 現在のスクリプト位置をプッシュ

    st->pos = pos;
    st->defsp = st->start + 4 + j;
    st->state = GOTO;
    return 0;
}

/*==========================================
 * 引数の所得
 *------------------------------------------
 */
int buildin_getarg (struct script_state *st)
{
    int  num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    int  max, stsp;
    if (st->defsp < 4
        || st->stack->stack_data[st->defsp - 1].type != C_RETINFO)
    {
        printf ("script:getarg without callfunc or callsub!\n");
        st->state = END;
        return 0;
    }
    max = conv_num (st, &(st->stack->stack_data[st->defsp - 4]));
    stsp = st->defsp - max - 4;
    if (num >= max)
    {
        printf ("script:getarg arg1(%d) out of range(%d) !\n", num, max);
        st->state = END;
        return 0;
    }
    push_copy (st->stack, stsp + num);
    return 0;
}

/*==========================================
 * サブルーチン/ユーザー定義関数の終了
 *------------------------------------------
 */
int buildin_return (struct script_state *st)
{
    if (st->end > st->start + 2)
    {                           // 戻り値有り
        push_copy (st->stack, st->start + 2);
    }
    st->state = RETFUNC;
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_next (struct script_state *st)
{
    st->state = STOP;
    clif_scriptnext (script_rid2sd (st), st->oid);
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_close (struct script_state *st)
{
    st->state = END;
    clif_scriptclose (script_rid2sd (st), st->oid);
    return 0;
}

int buildin_close2 (struct script_state *st)
{
    st->state = STOP;
    clif_scriptclose (script_rid2sd (st), st->oid);
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_menu (struct script_state *st)
{
    char *buf;
    int  i, len = 0;            // [fate] len is the total # of bytes we need to transmit the string choices
    int  menu_choices = 0;
    int  finished_menu_items = 0;   // [fate] set to 1 after we hit the first empty string

    struct map_session_data *sd;

    sd = script_rid2sd (st);

    // We don't need to do this iteration if the player cancels, strictly speaking.
    for (i = st->start + 2; i < st->end; i += 2)
    {
        int  choice_len;
        conv_str (st, &(st->stack->stack_data[i]));
        choice_len = strlen (st->stack->stack_data[i].u.str);
        len += choice_len + 1;  // count # of bytes we'll need for packet.  Only used if menu_or_input = 0.

        if (choice_len && !finished_menu_items)
            ++menu_choices;
        else
            finished_menu_items = 1;
    }

    if (sd->state.menu_or_input == 0)
    {
        st->state = RERUNLINE;
        sd->state.menu_or_input = 1;

        buf = (char *) calloc (len + 1, 1);
        buf[0] = 0;
        for (i = st->start + 2; menu_choices > 0; i += 2, --menu_choices)
        {
            strcat (buf, st->stack->stack_data[i].u.str);
            strcat (buf, ":");
        }
        clif_scriptmenu (script_rid2sd (st), st->oid, buf);
        free (buf);
    }
    else if (sd->npc_menu == 0xff)
    {                           // cansel
        sd->state.menu_or_input = 0;
        st->state = END;
    }
    else
    {                           // goto動作
        // ragemu互換のため
        pc_setreg (sd, add_str ("l15"), sd->npc_menu);
        pc_setreg (sd, add_str ("@menu"), sd->npc_menu);
        sd->state.menu_or_input = 0;
        if (sd->npc_menu > 0 && sd->npc_menu <= menu_choices)
        {
            int  pos;
            if (st->stack->
                stack_data[st->start + sd->npc_menu * 2 + 1].type != C_POS)
            {
                st->state = END;
                return 0;
            }
            pos =
                conv_num (st,
                          &(st->
                            stack->stack_data[st->start + sd->npc_menu * 2 +
                                              1]));
            st->pos = pos;
            st->state = GOTO;
        }
    }
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_rand (struct script_state *st)
{
    int  range, min, max;

    if (st->end > st->start + 3)
    {
        min = conv_num (st, &(st->stack->stack_data[st->start + 2]));
        max = conv_num (st, &(st->stack->stack_data[st->start + 3]));
        if (max < min)
        {
            int  tmp;
            tmp = min;
            min = max;
            max = tmp;
        }
        range = max - min + 1;
        push_val (st->stack, C_INT, (range <= 0 ? 0 : MRAND (range)) + min);
    }
    else
    {
        range = conv_num (st, &(st->stack->stack_data[st->start + 2]));
        push_val (st->stack, C_INT, range <= 0 ? 0 : MRAND (range));
    }
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_pow (struct script_state *st)
{
    int  a, b;

    a = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    b = conv_num (st, &(st->stack->stack_data[st->start + 3]));

    push_val (st->stack, C_INT, pow (a * 0.001, b));

    return 0;
}

/*==========================================
 * Check whether the PC is at the specified location
 *------------------------------------------
 */
int buildin_isat (struct script_state *st)
{
    int  x, y;
    const char *str;
    struct map_session_data *sd = script_rid2sd (st);

    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    y = conv_num (st, &(st->stack->stack_data[st->start + 4]));

    if (!sd)
        return 1;

    push_val (st->stack, C_INT,
              (x == sd->bl.x)
              && (y == sd->bl.y) && (!strcmp (str, maps[sd->bl.m].name)));

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_warp (struct script_state *st)
{
    int  x, y;
    const char *str;
    struct map_session_data *sd = script_rid2sd (st);

    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    y = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    if (strcmp (str, "Random") == 0)
        pc_randomwarp (sd, 3);
    else if (strcmp (str, "SavePoint") == 0)
    {
        if (maps[sd->bl.m].flag.noreturn)    // 蝶禁止
            return 0;

        pc_setpos (sd, sd->status.save_point.map,
                   sd->status.save_point.x, sd->status.save_point.y, 3);
    }
    else if (strcmp (str, "Save") == 0)
    {
        if (maps[sd->bl.m].flag.noreturn)    // 蝶禁止
            return 0;

        pc_setpos (sd, sd->status.save_point.map,
                   sd->status.save_point.x, sd->status.save_point.y, 3);
    }
    else
        pc_setpos (sd, str, x, y, 0);
    return 0;
}

/*==========================================
 * エリア指定ワープ
 *------------------------------------------
 */
static void buildin_areawarp_sub (struct block_list *bl, va_list ap)
{
    int  x, y;
    char *map;
    map = va_arg (ap, char *);
    x = va_arg (ap, int);
    y = va_arg (ap, int);
    if (strcmp (map, "Random") == 0)
        pc_randomwarp ((struct map_session_data *) bl, 3);
    else
        pc_setpos ((struct map_session_data *) bl, map, x, y, 0);
}

int buildin_areawarp (struct script_state *st)
{
    int  x, y, m;
    const char *str;
    const char *mapname;
    int  x_0, y_0, x_1, y_1;

    mapname = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x_0 = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    y_0 = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    x_1 = conv_num (st, &(st->stack->stack_data[st->start + 5]));
    y_1 = conv_num (st, &(st->stack->stack_data[st->start + 6]));
    str = conv_str (st, &(st->stack->stack_data[st->start + 7]));
    x = conv_num (st, &(st->stack->stack_data[st->start + 8]));
    y = conv_num (st, &(st->stack->stack_data[st->start + 9]));

    if ((m = map_mapname2mapid (mapname)) < 0)
        return 0;

    map_foreachinarea (buildin_areawarp_sub,
                       m, x_0, y_0, x_1, y_1, BL_PC, str, x, y);
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_heal (struct script_state *st)
{
    int  hp, sp;

    hp = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sp = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    pc_heal (script_rid2sd (st), hp, sp);
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_itemheal (struct script_state *st)
{
    int  hp, sp;

    hp = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sp = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    pc_itemheal (script_rid2sd (st), hp, sp);
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_percentheal (struct script_state *st)
{
    int  hp, sp;

    hp = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sp = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    pc_percentheal (script_rid2sd (st), hp, sp);
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_input (struct script_state *st)
{
    struct map_session_data *sd = NULL;
    int  num =
        (st->end >
         st->start + 2) ? st->stack->stack_data[st->start + 2].u.num : 0;
    const char *name =
        (st->end >
         st->start + 2) ? str_buf + str_data[num & 0x00ffffff].str : "";
//  char prefix=*name;
    char postfix = name[strlen (name) - 1];

    sd = script_rid2sd (st);
    if (sd->state.menu_or_input)
    {
        sd->state.menu_or_input = 0;
        if (postfix == '$')
        {
            // 文字列
            if (st->end > st->start + 2)
            {                   // 引数1個
                set_reg (sd, num, name, (void *) sd->npc_str);
            }
            else
            {
                printf ("buildin_input: string discarded !!\n");
            }
        }
        else
        {

            //commented by Lupus (check Value Number Input fix in clif.c)
            //** Fix by fritz :X keeps people from abusing old input bugs
            if (sd->npc_amount < 0) //** If input amount is less then 0
            {
                clif_tradecancelled (sd);   // added "Deal has been cancelled" message by Valaris
                buildin_close (st); //** close
            }

            // 数値
            if (st->end > st->start + 2)
            {                   // 引数1個
                set_reg (sd, num, name, (void *) sd->npc_amount);
            }
            else
            {
                // ragemu互換のため
                pc_setreg (sd, add_str ("l14"), sd->npc_amount);
            }
        }
    }
    else
    {
        st->state = RERUNLINE;
        if (postfix == '$')
            clif_scriptinputstr (sd, st->oid);
        else
            clif_scriptinput (sd, st->oid);
        sd->state.menu_or_input = 1;
    }
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_if (struct script_state *st)
{
    int  sel, i;

    sel = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    if (!sel)
        return 0;

    // 関数名をコピー
    push_copy (st->stack, st->start + 3);
    // 間に引数マーカを入れて
    push_val (st->stack, C_ARG, 0);
    // 残りの引数をコピー
    for (i = st->start + 4; i < st->end; i++)
    {
        push_copy (st->stack, i);
    }
    run_func (st);

    return 0;
}

/*==========================================
 * 変数設定
 *------------------------------------------
 */
int buildin_set (struct script_state *st)
{
    struct map_session_data *sd = NULL;
    int  num = st->stack->stack_data[st->start + 2].u.num;
    char *name = str_buf + str_data[num & 0x00ffffff].str;
    char prefix = *name;
    char postfix = name[strlen (name) - 1];

    if (st->stack->stack_data[st->start + 2].type != C_NAME)
    {
        printf ("script: buildin_set: not name\n");
        return 0;
    }

    if (prefix != '$')
        sd = script_rid2sd (st);

    if (postfix == '$')
    {
        // 文字列
        const char *str = conv_str (st, &(st->stack->stack_data[st->start + 3]));
        set_reg (sd, num, name, (void *) str);
    }
    else
    {
        // 数値
        int  val = conv_num (st, &(st->stack->stack_data[st->start + 3]));
        set_reg (sd, num, name, (void *) val);
    }

    return 0;
}

/*==========================================
 * 配列変数設定
 *------------------------------------------
 */
int buildin_setarray (struct script_state *st)
{
    struct map_session_data *sd = NULL;
    int  num = st->stack->stack_data[st->start + 2].u.num;
    char *name = str_buf + str_data[num & 0x00ffffff].str;
    char prefix = *name;
    char postfix = name[strlen (name) - 1];
    int  i, j;

    if (prefix != '$' && prefix != '@')
    {
        printf ("buildin_setarray: illegal scope !\n");
        return 0;
    }
    if (prefix != '$')
        sd = script_rid2sd (st);

    for (j = 0, i = st->start + 3; i < st->end && j < 128; i++, j++)
    {
        // This is ugly but it works
        union
        {
            const void *p;
            intptr_t i;
        } v;
        if (postfix == '$')
            v.p = conv_str (st, &(st->stack->stack_data[i]));
        else
            v.i = conv_num (st, &(st->stack->stack_data[i]));
        set_reg (sd, num + (j << 24), name, v.p);
    }
    return 0;
}

/*==========================================
 * 配列変数クリア
 *------------------------------------------
 */
int buildin_cleararray (struct script_state *st)
{
    struct map_session_data *sd = NULL;
    int  num = st->stack->stack_data[st->start + 2].u.num;
    char *name = str_buf + str_data[num & 0x00ffffff].str;
    char prefix = *name;
    char postfix = name[strlen (name) - 1];
    int  sz = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    int  i;

    if (prefix != '$' && prefix != '@')
    {
        printf ("buildin_cleararray: illegal scope !\n");
        return 0;
    }
    if (prefix != '$')
        sd = script_rid2sd (st);

    // This is ugly but it works
    union
    {
        const void *p;
        intptr_t i;
    } v;
    if (postfix == '$')
        v.p = conv_str (st, &(st->stack->stack_data[st->start + 3]));
    else
        v.i = conv_num (st, &(st->stack->stack_data[st->start + 3]));

    for (i = 0; i < sz; i++)
        set_reg (sd, num + (i << 24), name, v.p);
    return 0;
}

/*==========================================
 * 配列変数コピー
 *------------------------------------------
 */
int buildin_copyarray (struct script_state *st)
{
    struct map_session_data *sd = NULL;
    int  num = st->stack->stack_data[st->start + 2].u.num;
    char *name = str_buf + str_data[num & 0x00ffffff].str;
    char prefix = *name;
    char postfix = name[strlen (name) - 1];
    int  num2 = st->stack->stack_data[st->start + 3].u.num;
    char *name2 = str_buf + str_data[num2 & 0x00ffffff].str;
    char prefix2 = *name2;
    char postfix2 = name2[strlen (name2) - 1];
    int  sz = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    int  i;

    if (prefix != '$' && prefix != '@' && prefix2 != '$' && prefix2 != '@')
    {
        printf ("buildin_copyarray: illegal scope !\n");
        return 0;
    }
    if ((postfix == '$' || postfix2 == '$') && postfix != postfix2)
    {
        printf ("buildin_copyarray: type mismatch !\n");
        return 0;
    }
    if (prefix != '$' || prefix2 != '$')
        sd = script_rid2sd (st);

    for (i = 0; i < sz; i++)
        set_reg (sd, num + (i << 24), name, get_val2 (st, num2 + (i << 24)));
    return 0;
}

/*==========================================
 * 配列変数のサイズ所得
 *------------------------------------------
 */
static int getarraysize (struct script_state *st, int num, int postfix)
{
    int  i = (num >> 24), c = i;
    for (; i < 128; i++)
    {
        void *v = get_val2 (st, num + (i << 24));
        if (postfix == '$' && *((char *) v))
            c = i;
        if (postfix != '$' && (int) v)
            c = i;
    }
    return c + 1;
}

int buildin_getarraysize (struct script_state *st)
{
    int  num = st->stack->stack_data[st->start + 2].u.num;
    char *name = str_buf + str_data[num & 0x00ffffff].str;
    char prefix = *name;
    char postfix = name[strlen (name) - 1];

    if (prefix != '$' && prefix != '@')
    {
        printf ("buildin_copyarray: illegal scope !\n");
        return 0;
    }

    push_val (st->stack, C_INT, getarraysize (st, num, postfix));
    return 0;
}

/*==========================================
 * 配列変数から要素削除
 *------------------------------------------
 */
int buildin_deletearray (struct script_state *st)
{
    struct map_session_data *sd = NULL;
    int  num = st->stack->stack_data[st->start + 2].u.num;
    char *name = str_buf + str_data[num & 0x00ffffff].str;
    char prefix = *name;
    char postfix = name[strlen (name) - 1];
    int  count = 1;
    int  i, sz = getarraysize (st, num, postfix) - (num >> 24) - count + 1;

    if ((st->end > st->start + 3))
        count = conv_num (st, &(st->stack->stack_data[st->start + 3]));

    if (prefix != '$' && prefix != '@')
    {
        printf ("buildin_deletearray: illegal scope !\n");
        return 0;
    }
    if (prefix != '$')
        sd = script_rid2sd (st);

    for (i = 0; i < sz; i++)
    {
        set_reg (sd, num + (i << 24), name,
                 get_val2 (st, num + ((i + count) << 24)));
    }
    for (; i < (128 - (num >> 24)); i++)
    {
        if (postfix != '$')
            set_reg (sd, num + (i << 24), name, 0);
        if (postfix == '$')
            set_reg (sd, num + (i << 24), name, "");
    }
    return 0;
}

/*==========================================
 * 指定要素を表す値(キー)を所得する
 *------------------------------------------
 */
int buildin_getelementofarray (struct script_state *st)
{
    if (st->stack->stack_data[st->start + 2].type == C_NAME)
    {
        int  i = conv_num (st, &(st->stack->stack_data[st->start + 3]));
        if (i > 127 || i < 0)
        {
            printf
                ("script: getelementofarray (operator[]): param2 illegal number %d\n",
                 i);
            push_val (st->stack, C_INT, 0);
        }
        else
        {
            push_val (st->stack, C_NAME,
                      (i << 24) | st->stack->stack_data[st->start + 2].u.num);
        }
    }
    else
    {
        printf
            ("script: getelementofarray (operator[]): param1 not name !\n");
        push_val (st->stack, C_INT, 0);
    }
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_setlook (struct script_state *st)
{
    int  type, val;

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    val = conv_num (st, &(st->stack->stack_data[st->start + 3]));

    pc_changelook (script_rid2sd (st), type, val);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_countitem (struct script_state *st)
{
    int  nameid = 0, count = 0, i;
    struct map_session_data *sd;

    struct script_data *data;

    sd = script_rid2sd (st);

    data = &(st->stack->stack_data[st->start + 2]);
    get_val (st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str (st, data);
        struct item_data *item_data;
        if ((item_data = itemdb_searchname (name)) != NULL)
            nameid = item_data->nameid;
    }
    else
        nameid = conv_num (st, data);

    if (nameid >= 500)          //if no such ID then skip this iteration
        for (i = 0; i < MAX_INVENTORY; i++)
        {
            if (sd->status.inventory[i].nameid == nameid)
                count += sd->status.inventory[i].amount;
        }
    else
    {
        if (battle_config.error_log)
            printf ("wrong item ID : countitem(%i)\n", nameid);
    }
    push_val (st->stack, C_INT, count);

    return 0;
}

/*==========================================
 * 重量チェック
 *------------------------------------------
 */
int buildin_checkweight (struct script_state *st)
{
    int  nameid = 0, amount;
    struct map_session_data *sd;
    struct script_data *data;

    sd = script_rid2sd (st);

    data = &(st->stack->stack_data[st->start + 2]);
    get_val (st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str (st, data);
        struct item_data *item_data = itemdb_searchname (name);
        if (item_data)
            nameid = item_data->nameid;
    }
    else
        nameid = conv_num (st, data);

    amount = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    if (amount <= 0 || nameid < 500)
    {                           //if get wrong item ID or amount<=0, don't count weight of non existing items
        push_val (st->stack, C_INT, 0);
    }

    sd = script_rid2sd (st);
    if (itemdb_weight (nameid) * amount + sd->weight > sd->max_weight)
    {
        push_val (st->stack, C_INT, 0);
    }
    else
    {
        push_val (st->stack, C_INT, 1);
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_getitem (struct script_state *st)
{
    int  nameid, amount, flag = 0;
    struct item item_tmp;
    struct map_session_data *sd;
    struct script_data *data;

    sd = script_rid2sd (st);

    data = &(st->stack->stack_data[st->start + 2]);
    get_val (st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str (st, data);
        struct item_data *item_data = itemdb_searchname (name);
        nameid = 727;           //Default to iten
        if (item_data != NULL)
            nameid = item_data->nameid;
    }
    else
        nameid = conv_num (st, data);

    if ((amount =
         conv_num (st, &(st->stack->stack_data[st->start + 3]))) <= 0)
    {
        return 0;               //return if amount <=0, skip the useles iteration
    }
    //Violet Box, Blue Box, etc - random item pick
    if (nameid < 0)
    {                           // ランダム
        nameid = itemdb_searchrandomid (-nameid);
        flag = 1;
    }

    if (nameid > 0)
    {
        memset (&item_tmp, 0, sizeof (item_tmp));
        item_tmp.nameid = nameid;
        if (!flag)
            item_tmp.identify = 1;
        else
            item_tmp.identify = !itemdb_isequip3 (nameid);
        if (st->end > st->start + 5)    //アイテムを指定したIDに渡す
            sd = map_id2sd (conv_num
                            (st, &(st->stack->stack_data[st->start + 5])));
        if (sd == NULL)         //アイテムを渡す相手がいなかったらお帰り
            return 0;
        if ((flag = pc_additem (sd, &item_tmp, amount)))
        {
            clif_additem (sd, 0, 0, flag);
            map_addflooritem (&item_tmp, amount, sd->bl.m, sd->bl.x, sd->bl.y,
                              NULL, NULL, NULL);
        }
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_getitem2 (struct script_state *st)
{
    int  nameid, amount, flag = 0;
    int  iden, ref, attr, c1, c2, c3, c4;
    struct item item_tmp;
    struct map_session_data *sd;
    struct script_data *data;

    sd = script_rid2sd (st);

    data = &(st->stack->stack_data[st->start + 2]);
    get_val (st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str (st, data);
        struct item_data *item_data = itemdb_searchname (name);
        nameid = 512;           //Apple item ID
        if (item_data)
            nameid = item_data->nameid;
    }
    else
        nameid = conv_num (st, data);

    amount = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    iden = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    ref = conv_num (st, &(st->stack->stack_data[st->start + 5]));
    attr = conv_num (st, &(st->stack->stack_data[st->start + 6]));
    c1 = conv_num (st, &(st->stack->stack_data[st->start + 7]));
    c2 = conv_num (st, &(st->stack->stack_data[st->start + 8]));
    c3 = conv_num (st, &(st->stack->stack_data[st->start + 9]));
    c4 = conv_num (st, &(st->stack->stack_data[st->start + 10]));
    if (st->end > st->start + 11)   //アイテムを指定したIDに渡す
        sd = map_id2sd (conv_num
                        (st, &(st->stack->stack_data[st->start + 11])));
    if (sd == NULL)             //アイテムを渡す相手がいなかったらお帰り
        return 0;

    if (nameid < 0)
    {                           // ランダム
        nameid = itemdb_searchrandomid (-nameid);
        flag = 1;
    }

    if (nameid > 0)
    {
        memset (&item_tmp, 0, sizeof (item_tmp));
        struct item_data *item_data;
        item_data = itemdb_search (nameid);
        if (item_data->type == 4 || item_data->type == 5)
        {
            if (ref > 10)
                ref = 10;
        }
        else if (item_data->type == 7)
        {
            iden = 1;
            ref = 0;
        }
        else
        {
            iden = 1;
            ref = attr = 0;
        }

        item_tmp.nameid = nameid;
        if (!flag)
            item_tmp.identify = iden;
        else if (item_data->type == 4 || item_data->type == 5)
            item_tmp.identify = 0;
        item_tmp.refine = ref;
        item_tmp.attribute = attr;
        item_tmp.card[0] = c1;
        item_tmp.card[1] = c2;
        item_tmp.card[2] = c3;
        item_tmp.card[3] = c4;
        if ((flag = pc_additem (sd, &item_tmp, amount)))
        {
            clif_additem (sd, 0, 0, flag);
            map_addflooritem (&item_tmp, amount, sd->bl.m, sd->bl.x, sd->bl.y,
                              NULL, NULL, NULL);
        }
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_makeitem (struct script_state *st)
{
    int  nameid, amount, flag = 0;
    int  x, y, m;
    const char *mapname;
    struct item item_tmp;
    struct map_session_data *sd;
    struct script_data *data;

    sd = script_rid2sd (st);

    data = &(st->stack->stack_data[st->start + 2]);
    get_val (st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str (st, data);
        struct item_data *item_data = itemdb_searchname (name);
        nameid = 512;           //Apple Item ID
        if (item_data)
            nameid = item_data->nameid;
    }
    else
        nameid = conv_num (st, data);

    amount = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    mapname = conv_str (st, &(st->stack->stack_data[st->start + 4]));
    x = conv_num (st, &(st->stack->stack_data[st->start + 5]));
    y = conv_num (st, &(st->stack->stack_data[st->start + 6]));

    if (sd && strcmp (mapname, "this") == 0)
        m = sd->bl.m;
    else
        m = map_mapname2mapid (mapname);

    if (nameid < 0)
    {                           // ランダム
        nameid = itemdb_searchrandomid (-nameid);
        flag = 1;
    }

    if (nameid > 0)
    {
        memset (&item_tmp, 0, sizeof (item_tmp));
        item_tmp.nameid = nameid;
        if (!flag)
            item_tmp.identify = 1;
        else
            item_tmp.identify = !itemdb_isequip3 (nameid);

//      clif_additem(sd,0,0,flag);
        map_addflooritem (&item_tmp, amount, m, x, y, NULL, NULL, NULL);
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_delitem (struct script_state *st)
{
    int  nameid = 0, amount, i;
    struct map_session_data *sd;
    struct script_data *data;

    sd = script_rid2sd (st);

    data = &(st->stack->stack_data[st->start + 2]);
    get_val (st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str (st, data);
        struct item_data *item_data = itemdb_searchname (name);
        //nameid=512;
        if (item_data)
            nameid = item_data->nameid;
    }
    else
        nameid = conv_num (st, data);

    amount = conv_num (st, &(st->stack->stack_data[st->start + 3]));

    if (nameid < 500 || amount <= 0)
    {                           //by Lupus. Don't run FOR if u got wrong item ID or amount<=0
        //printf("wrong item ID or amount<=0 : delitem %i,\n",nameid,amount);
        return 0;
    }
    sd = script_rid2sd (st);

    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid <= 0
            || sd->inventory_data[i] == NULL
            || sd->inventory_data[i]->type != 7
            || sd->status.inventory[i].amount <= 0)
            continue;
    }
    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid == nameid)
        {
            if (sd->status.inventory[i].amount >= amount)
            {
                pc_delitem (sd, i, amount, 0);
                break;
            }
            else
            {
                amount -= sd->status.inventory[i].amount;
                if (amount == 0)
                    amount = sd->status.inventory[i].amount;
                pc_delitem (sd, i, amount, 0);
                break;
            }
        }
    }

    return 0;
}

/*==========================================
 *キャラ関係のパラメータ取得
 *------------------------------------------
 */
int buildin_readparam (struct script_state *st)
{
    int  type;
    struct map_session_data *sd;

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    if (st->end > st->start + 3)
        sd = map_nick2sd (conv_str
                          (st, &(st->stack->stack_data[st->start + 3])));
    else
        sd = script_rid2sd (st);

    if (sd == NULL)
    {
        push_val (st->stack, C_INT, -1);
        return 0;
    }

    push_val (st->stack, C_INT, pc_readparam (sd, type));

    return 0;
}

/*==========================================
 *キャラ関係のID取得
 *------------------------------------------
 */
int buildin_getcharid (struct script_state *st)
{
    int  num;
    struct map_session_data *sd;

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    if (st->end > st->start + 3)
        sd = map_nick2sd (conv_str
                          (st, &(st->stack->stack_data[st->start + 3])));
    else
        sd = script_rid2sd (st);
    if (sd == NULL)
    {
        push_val (st->stack, C_INT, -1);
        return 0;
    }
    if (num == 0)
        push_val (st->stack, C_INT, sd->status.char_id);
    if (num == 1)
        push_val (st->stack, C_INT, sd->status.party_id);
    if (num == 2)
        push_val (st->stack, C_INT, 0 /*guild_id*/);
    if (num == 3)
        push_val (st->stack, C_INT, sd->status.account_id);
    return 0;
}

/*==========================================
 *指定IDのPT名取得
 *------------------------------------------
 */
static char *buildin_getpartyname_sub (int party_id)
{
    struct party *p;

    p = NULL;
    p = party_search (party_id);

    if (p != NULL)
    {
        char *buf;
        buf = (char *) calloc (24, 1);
        strcpy (buf, p->name);
        return buf;
    }

    return 0;
}

int buildin_getpartyname (struct script_state *st)
{
    char *name;
    int  party_id;

    party_id = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    name = buildin_getpartyname_sub (party_id);
    if (name != 0)
        push_str (st->stack, C_STR, name);
    else
        push_str (st->stack, C_CONSTSTR, "null");

    return 0;
}

/*==========================================
 *指定IDのPT人数とメンバーID取得
 *------------------------------------------
 */
int buildin_getpartymember (struct script_state *st)
{
    struct party *p;
    int  i, j = 0;

    p = NULL;
    p = party_search (conv_num (st, &(st->stack->stack_data[st->start + 2])));

    if (p != NULL)
    {
        for (i = 0; i < MAX_PARTY; i++)
        {
            if (p->member[i].account_id)
            {
//              printf("name:%s %d\n",p->member[i].name,i);
                mapreg_setregstr (add_str ("$@partymembername$") + (i << 24),
                                  p->member[i].name);
                j++;
            }
        }
    }
    mapreg_setreg (add_str ("$@partymembercount"), j);

    return 0;
}

/*==========================================
 * キャラクタの名前
 *------------------------------------------
 */
int buildin_strcharinfo (struct script_state *st)
{
    struct map_session_data *sd;
    int  num;

    sd = script_rid2sd (st);
    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    if (num == 0)
    {
        char *buf;
        buf = (char *) calloc (24, 1);
        strncpy (buf, sd->status.name, 23);
        push_str (st->stack, C_STR, buf);
    }
    if (num == 1)
    {
        char *buf;
        buf = buildin_getpartyname_sub (sd->status.party_id);
        if (buf != 0)
            push_str (st->stack, C_STR, buf);
        else
            push_str (st->stack, C_CONSTSTR, "");
    }
    if (num == 2)
    {
        // was: guild name
        push_str (st->stack, C_CONSTSTR, "");
    }

    return 0;
}

unsigned int equip[10] =
    { 0x0100, 0x0010, 0x0020, 0x0002, 0x0004, 0x0040, 0x0008, 0x0080, 0x0200,
    0x0001
};

/*==========================================
 * GetEquipID(Pos);     Pos: 1-10
 *------------------------------------------
 */
int buildin_getequipid (struct script_state *st)
{
    int  i, num;
    struct map_session_data *sd;
    struct item_data *item;

    sd = script_rid2sd (st);
    if (sd == NULL)
    {
        printf ("getequipid: sd == NULL\n");
        return 0;
    }
    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    i = pc_checkequip (sd, equip[num - 1]);
    if (i >= 0)
    {
        item = sd->inventory_data[i];
        if (item)
            push_val (st->stack, C_INT, item->nameid);
        else
            push_val (st->stack, C_INT, 0);
    }
    else
    {
        push_val (st->stack, C_INT, -1);
    }
    return 0;
}

/*==========================================
 * 装備名文字列（精錬メニュー用）
 *------------------------------------------
 */
int buildin_getequipname (struct script_state *st)
{
    int  i, num;
    struct map_session_data *sd;
    struct item_data *item;
    char *buf;

    buf = (char *) calloc (64, 1);
    sd = script_rid2sd (st);
    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    i = pc_checkequip (sd, equip[num - 1]);
    if (i >= 0)
    {
        item = sd->inventory_data[i];
        if (item)
            sprintf (buf, "%s-[%s]", epos[num-1], item->jname);
        else
            sprintf (buf, "%s-[%s]", epos[num-1], epos[10]);
    }
    else
    {
        sprintf (buf, "%s-[%s]", epos[num-1], epos[num-1]);
    }
    push_str (st->stack, C_STR, buf);

    return 0;
}

/*==========================================
 * getbrokenid [Valaris]
 *------------------------------------------
 */
int buildin_getbrokenid (struct script_state *st)
{
    int  i, num, id = 0, brokencounter = 0;
    struct map_session_data *sd;

    sd = script_rid2sd (st);

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].broken == 1)
        {
            brokencounter++;
            if (num == brokencounter)
            {
                id = sd->status.inventory[i].nameid;
                break;
            }
        }
    }

    push_val (st->stack, C_INT, id);

    return 0;
}

/*==========================================
 * repair [Valaris]
 *------------------------------------------
 */
int buildin_repair (struct script_state *st)
{
    int  i, num;
    int  repaircounter = 0;
    struct map_session_data *sd;

    sd = script_rid2sd (st);

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].broken == 1)
        {
            repaircounter++;
            if (num == repaircounter)
            {
                sd->status.inventory[i].broken = 0;
                clif_equiplist (sd);
                clif_misceffect (&sd->bl, 3);
                clif_displaymessage (sd->fd, "Item has been repaired.");
                break;
            }
        }
    }

    return 0;
}

/*==========================================
 * 装備チェック
 *------------------------------------------
 */
int buildin_getequipisequiped (struct script_state *st)
{
    int  i, num;
    struct map_session_data *sd;

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    i = pc_checkequip (sd, equip[num - 1]);
    if (i >= 0)
    {
        push_val (st->stack, C_INT, 1);
    }
    else
    {
        push_val (st->stack, C_INT, 0);
    }

    return 0;
}

/*==========================================
 * 装備品精錬可能チェック
 *------------------------------------------
 */
int buildin_getequipisenableref (struct script_state *st)
{
    int  i, num;
    struct map_session_data *sd;

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    i = pc_checkequip (sd, equip[num - 1]);
    if (i >= 0 && num < 7 && sd->inventory_data[i]
        && (num != 1 || sd->inventory_data[i]->def > 1
            || (sd->inventory_data[i]->def == 1
                && sd->inventory_data[i]->equip_script == NULL)
            || (sd->inventory_data[i]->def <= 0
                && sd->inventory_data[i]->equip_script != NULL)))
    {
        push_val (st->stack, C_INT, 1);
    }
    else
    {
        push_val (st->stack, C_INT, 0);
    }

    return 0;
}

/*==========================================
 * 装備品鑑定チェック
 *------------------------------------------
 */
int buildin_getequipisidentify (struct script_state *st)
{
    int  i, num;
    struct map_session_data *sd;

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    i = pc_checkequip (sd, equip[num - 1]);
    if (i >= 0)
        push_val (st->stack, C_INT, sd->status.inventory[i].identify);
    else
        push_val (st->stack, C_INT, 0);

    return 0;
}

/*==========================================
 * 装備品精錬度
 *------------------------------------------
 */
int buildin_getequiprefinerycnt (struct script_state *st)
{
    int  i, num;
    struct map_session_data *sd;

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    i = pc_checkequip (sd, equip[num - 1]);
    if (i >= 0)
        push_val (st->stack, C_INT, sd->status.inventory[i].refine);
    else
        push_val (st->stack, C_INT, 0);

    return 0;
}

/*==========================================
 * 装備品武器LV
 *------------------------------------------
 */
int buildin_getequipweaponlv (struct script_state *st)
{
    int  i, num;
    struct map_session_data *sd;

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    i = pc_checkequip (sd, equip[num - 1]);
    if (i >= 0 && sd->inventory_data[i])
        push_val (st->stack, C_INT, sd->inventory_data[i]->wlv);
    else
        push_val (st->stack, C_INT, 0);

    return 0;
}

/*==========================================
 * 装備品精錬成功率
 *------------------------------------------
 */
int buildin_getequippercentrefinery (struct script_state *st)
{
    int  i, num;
    struct map_session_data *sd;

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    i = pc_checkequip (sd, equip[num - 1]);
    if (i >= 0)
        push_val (st->stack, C_INT,
                  pc_percentrefinery (sd, &sd->status.inventory[i]));
    else
        push_val (st->stack, C_INT, 0);

    return 0;
}

/*==========================================
 * 精錬成功
 *------------------------------------------
 */
int buildin_successrefitem (struct script_state *st)
{
    int  i, num, ep;
    struct map_session_data *sd;

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    i = pc_checkequip (sd, equip[num - 1]);
    if (i >= 0)
    {
        ep = sd->status.inventory[i].equip;

        sd->status.inventory[i].refine++;
        pc_unequipitem (sd, i, 0);
        clif_delitem (sd, i, 1);
        clif_additem (sd, i, 1, 0);
        pc_equipitem (sd, i, ep);
        clif_misceffect (&sd->bl, 3);
    }

    return 0;
}

/*==========================================
 * 精錬失敗
 *------------------------------------------
 */
int buildin_failedrefitem (struct script_state *st)
{
    int  i, num;
    struct map_session_data *sd;

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    i = pc_checkequip (sd, equip[num - 1]);
    if (i >= 0)
    {
        sd->status.inventory[i].refine = 0;
        pc_unequipitem (sd, i, 0);
        // 精錬失敗エフェクトのパケット
        pc_delitem (sd, i, 1, 0);
        // 他の人にも失敗を通知
        clif_misceffect (&sd->bl, 2);
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_statusup (struct script_state *st)
{
    int  type;
    struct map_session_data *sd;

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    pc_statusup (sd, type);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_statusup2 (struct script_state *st)
{
    int  type, val;
    struct map_session_data *sd;

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    val = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    sd = script_rid2sd (st);
    pc_statusup2 (sd, type, val);

    return 0;
}

/*==========================================
 * 装備品による能力値ボーナス
 *------------------------------------------
 */
int buildin_bonus (struct script_state *st)
{
    int  type, val;
    struct map_session_data *sd;

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    val = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    sd = script_rid2sd (st);
    pc_bonus (sd, type, val);

    return 0;
}

/*==========================================
 * 装備品による能力値ボーナス
 *------------------------------------------
 */
int buildin_bonus2 (struct script_state *st)
{
    int  type, type2, val;
    struct map_session_data *sd;

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    type2 = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    val = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    sd = script_rid2sd (st);
    pc_bonus2 (sd, type, type2, val);

    return 0;
}

/*==========================================
 * 装備品による能力値ボーナス
 *------------------------------------------
 */
int buildin_bonus3 (struct script_state *st)
{
    int  type, type2, type3, val;
    struct map_session_data *sd;

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    type2 = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    type3 = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    val = conv_num (st, &(st->stack->stack_data[st->start + 5]));
    sd = script_rid2sd (st);
    pc_bonus3 (sd, type, type2, type3, val);

    return 0;
}

/*==========================================
 * スキル所得
 *------------------------------------------
 */
int buildin_skill (struct script_state *st)
{
    int  id, level, flag = 1;
    struct map_session_data *sd;

    id = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    level = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    if (st->end > st->start + 4)
        flag = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    sd = script_rid2sd (st);
    pc_skill (sd, id, level, flag);
    clif_skillinfoblock (sd);

    return 0;
}

/*==========================================
 * [Fate] Sets the skill level permanently
 *------------------------------------------
 */
int buildin_setskill (struct script_state *st)
{
    int  id, level;
    struct map_session_data *sd;

    id = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    level = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    sd = script_rid2sd (st);

    sd->status.skill[id].id = level ? id : 0;
    sd->status.skill[id].lv = level;
    clif_skillinfoblock (sd);
    return 0;
}

/*==========================================
 * スキルレベル所得
 *------------------------------------------
 */
int buildin_getskilllv (struct script_state *st)
{
    int  id = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    push_val (st->stack, C_INT, pc_checkskill (script_rid2sd (st), id));
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_basicskillcheck (struct script_state *st)
{
    push_val (st->stack, C_INT, battle_config.basic_skill_check);
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_getgmlevel (struct script_state *st)
{
    push_val (st->stack, C_INT, pc_isGM (script_rid2sd (st)));
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_end (struct script_state *st)
{
    st->state = END;
    return 0;
}

/*==========================================
 * [Freeyorp] Return the current opt2
 *------------------------------------------
 */

int buildin_getopt2 (struct script_state *st)
{
    struct map_session_data *sd;

    sd = script_rid2sd (st);

    push_val (st->stack, C_INT, sd->opt2);

    return 0;
}

/*==========================================
 * [Freeyorp] Sets opt2
 *------------------------------------------
 */

int buildin_setopt2 (struct script_state *st)
{
    int  new_opt2;
    struct map_session_data *sd;

    new_opt2 = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    if (new_opt2 == sd->opt2)
        return 0;
    sd->opt2 = new_opt2;
    clif_changeoption (&sd->bl);
    pc_calcstatus (sd, 0);

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_checkoption (struct script_state *st)
{
    int  type;
    struct map_session_data *sd;

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);

    if (sd->status.option & type)
    {
        push_val (st->stack, C_INT, 1);
    }
    else
    {
        push_val (st->stack, C_INT, 0);
    }

    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_setoption (struct script_state *st)
{
    int  type;
    struct map_session_data *sd;

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    pc_setoption (sd, type);

    return 0;
}

/*==========================================
 *	セーブポイントの保存
 *------------------------------------------
 */
int buildin_savepoint (struct script_state *st)
{
    int  x, y;
    const char *str;

    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    y = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    pc_setsavepoint (script_rid2sd (st), str, x, y);
    return 0;
}

/*==========================================
 * gettimetick(type)
 *
 * type The type of time measurement.
 *  Specify 0 for the system tick, 1 for
 *  seconds elapsed today, or 2 for seconds
 *  since Unix epoch. Defaults to 0 for any
 *  other value.
 *------------------------------------------
 */
int buildin_gettimetick (struct script_state *st)   /* Asgard Version */
{
    int  type;
    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));

    switch (type)
    {
        /* Number of seconds elapsed today (0-86399, 00:00:00-23:59:59). */
        case 1:
        {
            time_t timer;
            struct tm *t;

            time (&timer);
            t = gmtime (&timer);
            push_val (st->stack, C_INT,
                      ((t->tm_hour) * 3600 + (t->tm_min) * 60 + t->tm_sec));
            break;
        }
        /* Seconds since Unix epoch. */
        case 2:
            push_val (st->stack, C_INT, (int) time (NULL));
            break;
        /* System tick (unsigned int, and yes, it will wrap). */
        case 0:
        default:
            push_val (st->stack, C_INT, gettick ());
            break;
    }
    return 0;
}

/*==========================================
 * GetTime(Type);
 * 1: Sec     2: Min     3: Hour
 * 4: WeekDay     5: MonthDay     6: Month
 * 7: Year
 *------------------------------------------
 */
int buildin_gettime (struct script_state *st)   /* Asgard Version */
{
    int  type;
    time_t timer;
    struct tm *t;

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));

    time (&timer);
    t = gmtime (&timer);

    switch (type)
    {
        case 1:                //Sec(0~59)
            push_val (st->stack, C_INT, t->tm_sec);
            break;
        case 2:                //Min(0~59)
            push_val (st->stack, C_INT, t->tm_min);
            break;
        case 3:                //Hour(0~23)
            push_val (st->stack, C_INT, t->tm_hour);
            break;
        case 4:                //WeekDay(0~6)
            push_val (st->stack, C_INT, t->tm_wday);
            break;
        case 5:                //MonthDay(01~31)
            push_val (st->stack, C_INT, t->tm_mday);
            break;
        case 6:                //Month(01~12)
            push_val (st->stack, C_INT, t->tm_mon + 1);
            break;
        case 7:                //Year(20xx)
            push_val (st->stack, C_INT, t->tm_year + 1900);
            break;
        default:               //(format error)
            push_val (st->stack, C_INT, -1);
            break;
    }
    return 0;
}

/*==========================================
 * GetTimeStr("TimeFMT", Length);
 *------------------------------------------
 */
int buildin_gettimestr (struct script_state *st)
{
    char *tmpstr;
    const char *fmtstr;
    int  maxlen;
    time_t now = time (NULL);

    fmtstr = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    maxlen = conv_num (st, &(st->stack->stack_data[st->start + 3]));

    tmpstr = (char *) calloc (maxlen + 1, 1);
    strftime (tmpstr, maxlen, fmtstr, gmtime (&now));
    tmpstr[maxlen] = '\0';

    push_str (st->stack, C_STR, tmpstr);
    return 0;
}

/*==========================================
 * カプラ倉庫を開く
 *------------------------------------------
 */
int buildin_openstorage (struct script_state *st)
{
//  int sync = 0;
//  if (st->end >= 3) sync = conv_num(st,& (st->stack->stack_data[st->start+2]));
    struct map_session_data *sd = script_rid2sd (st);

//  if (sync) {
    st->state = STOP;
    sd->npc_flags.storage = 1;
//  } else st->state = END;

    storage_storageopen (sd);
    return 0;
}

/*==========================================
 * アイテムによるスキル発動
 *------------------------------------------
 */
int buildin_itemskill (struct script_state *st)
{
    int  id, lv;
    const char *str;
    struct map_session_data *sd = script_rid2sd (st);

    id = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    lv = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    str = conv_str (st, &(st->stack->stack_data[st->start + 4]));

    // 詠唱中にスキルアイテムは使用できない
    if (sd->skilltimer != -1)
        return 0;

    sd->skillitem = id;
    sd->skillitemlv = lv;
    clif_item_skill (sd, id, lv, str);
    return 0;
}

/*==========================================
 * NPCで経験値上げる
 *------------------------------------------
 */
int buildin_getexp (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    int  base = 0, job = 0;

    base = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    job = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    if (base < 0 || job < 0)
        return 0;
    if (sd)
        pc_gainexp_reason (sd, base, job, PC_GAINEXP_REASON_SCRIPT);

    return 0;
}

/*==========================================
 * モンスター発生
 *------------------------------------------
 */
int buildin_monster (struct script_state *st)
{
    int  mob_class, amount, x, y;
    const char *str, *map, *event = "";

    map = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    y = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    str = conv_str (st, &(st->stack->stack_data[st->start + 5]));
    mob_class = conv_num (st, &(st->stack->stack_data[st->start + 6]));
    amount = conv_num (st, &(st->stack->stack_data[st->start + 7]));
    if (st->end > st->start + 8)
        event = conv_str (st, &(st->stack->stack_data[st->start + 8]));

    mob_once_spawn (map_id2sd (st->rid), map, x, y, str, mob_class, amount,
                    event);
    return 0;
}

/*==========================================
 * モンスター発生
 *------------------------------------------
 */
int buildin_areamonster (struct script_state *st)
{
    int  mob_class, amount, x_0, y_0, x_1, y_1;
    const char *str, *map, *event = "";

    map = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x_0 = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    y_0 = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    x_1 = conv_num (st, &(st->stack->stack_data[st->start + 5]));
    y_1 = conv_num (st, &(st->stack->stack_data[st->start + 6]));
    str = conv_str (st, &(st->stack->stack_data[st->start + 7]));
    mob_class = conv_num (st, &(st->stack->stack_data[st->start + 8]));
    amount = conv_num (st, &(st->stack->stack_data[st->start + 9]));
    if (st->end > st->start + 10)
        event = conv_str (st, &(st->stack->stack_data[st->start + 10]));

    mob_once_spawn_area (map_id2sd (st->rid), map, x_0, y_0, x_1, y_1, str, mob_class,
                         amount, event);
    return 0;
}

/*==========================================
 * モンスター削除
 *------------------------------------------
 */
static void buildin_killmonster_sub (struct block_list *bl, va_list ap)
{
    char *event = va_arg (ap, char *);
    int  allflag = va_arg (ap, int);

    if (!allflag)
    {
        if (strcmp (event, ((struct mob_data *) bl)->npc_event) == 0)
            mob_delete ((struct mob_data *) bl);
        return;
    }
    else if (allflag)
    {
        if (((struct mob_data *) bl)->spawndelay_1 == -1
            && ((struct mob_data *) bl)->spawndelay2 == -1)
            mob_delete ((struct mob_data *) bl);
        return;
    }
}

int buildin_killmonster (struct script_state *st)
{
    const char *mapname, *event;
    int  m, allflag = 0;
    mapname = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    event = conv_str (st, &(st->stack->stack_data[st->start + 3]));
    if (strcmp (event, "All") == 0)
        allflag = 1;

    if ((m = map_mapname2mapid (mapname)) < 0)
        return 0;
    map_foreachinarea (buildin_killmonster_sub,
                       m, 0, 0, maps[m].xs, maps[m].ys, BL_MOB, event, allflag);
    return 0;
}

static void buildin_killmonsterall_sub (struct block_list *bl, va_list)
{
    mob_delete ((struct mob_data *) bl);
}

int buildin_killmonsterall (struct script_state *st)
{
    const char *mapname;
    int  m;
    mapname = conv_str (st, &(st->stack->stack_data[st->start + 2]));

    if ((m = map_mapname2mapid (mapname)) < 0)
        return 0;
    map_foreachinarea (buildin_killmonsterall_sub,
                       m, 0, 0, maps[m].xs, maps[m].ys, BL_MOB);
    return 0;
}

/*==========================================
 * イベント実行
 *------------------------------------------
 */
int buildin_doevent (struct script_state *st)
{
    const char *event;
    event = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    npc_event (map_id2sd (st->rid), event, 0);
    return 0;
}

/*==========================================
 * NPC主体イベント実行
 *------------------------------------------
 */
int buildin_donpcevent (struct script_state *st)
{
    const char *event;
    event = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    npc_event_do (event);
    return 0;
}

/*==========================================
 * イベントタイマー追加
 *------------------------------------------
 */
int buildin_addtimer (struct script_state *st)
{
    const char *event;
    int  tick;
    tick = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    event = conv_str (st, &(st->stack->stack_data[st->start + 3]));
    pc_addeventtimer (script_rid2sd (st), tick, event);
    return 0;
}

/*==========================================
 * イベントタイマー削除
 *------------------------------------------
 */
int buildin_deltimer (struct script_state *st)
{
    const char *event;
    event = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    pc_deleventtimer (script_rid2sd (st), event);
    return 0;
}

/*==========================================
 * イベントタイマーのカウント値追加
 *------------------------------------------
 */
int buildin_addtimercount (struct script_state *st)
{
    const char *event;
    int  tick;
    event = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    tick = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    pc_addeventtimercount (script_rid2sd (st), event, tick);
    return 0;
}

/*==========================================
 * NPCタイマー初期化
 *------------------------------------------
 */
int buildin_initnpctimer (struct script_state *st)
{
    struct npc_data *nd;
    if (st->end > st->start + 2)
        nd = npc_name2id (conv_str
                          (st, &(st->stack->stack_data[st->start + 2])));
    else
        nd = (struct npc_data *) map_id2bl (st->oid);

    npc_settimerevent_tick (nd, 0);
    npc_timerevent_start (nd);
    return 0;
}

/*==========================================
 * NPCタイマー開始
 *------------------------------------------
 */
int buildin_startnpctimer (struct script_state *st)
{
    struct npc_data *nd;
    if (st->end > st->start + 2)
        nd = npc_name2id (conv_str
                          (st, &(st->stack->stack_data[st->start + 2])));
    else
        nd = (struct npc_data *) map_id2bl (st->oid);

    npc_timerevent_start (nd);
    return 0;
}

/*==========================================
 * NPCタイマー停止
 *------------------------------------------
 */
int buildin_stopnpctimer (struct script_state *st)
{
    struct npc_data *nd;
    if (st->end > st->start + 2)
        nd = npc_name2id (conv_str
                          (st, &(st->stack->stack_data[st->start + 2])));
    else
        nd = (struct npc_data *) map_id2bl (st->oid);

    npc_timerevent_stop (nd);
    return 0;
}

/*==========================================
 * NPCタイマー情報所得
 *------------------------------------------
 */
int buildin_getnpctimer (struct script_state *st)
{
    struct npc_data *nd;
    int  type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    int  val = 0;
    if (st->end > st->start + 3)
        nd = npc_name2id (conv_str
                          (st, &(st->stack->stack_data[st->start + 3])));
    else
        nd = (struct npc_data *) map_id2bl (st->oid);

    switch (type)
    {
        case 0:
            val = npc_gettimerevent_tick (nd);
            break;
        case 1:
            val = (nd->u.scr.nexttimer >= 0);
            break;
        case 2:
            val = nd->u.scr.timeramount;
            break;
    }
    push_val (st->stack, C_INT, val);
    return 0;
}

/*==========================================
 * NPCタイマー値設定
 *------------------------------------------
 */
int buildin_setnpctimer (struct script_state *st)
{
    int  tick;
    struct npc_data *nd;
    tick = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    if (st->end > st->start + 3)
        nd = npc_name2id (conv_str
                          (st, &(st->stack->stack_data[st->start + 3])));
    else
        nd = (struct npc_data *) map_id2bl (st->oid);

    npc_settimerevent_tick (nd, tick);
    return 0;
}

/*==========================================
 * 天の声アナウンス
 *------------------------------------------
 */
int buildin_announce (struct script_state *st)
{
    const char *str;
    int  flag;
    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    flag = conv_num (st, &(st->stack->stack_data[st->start + 3]));

    if (flag & 0x0f)
    {
        struct block_list *bl = (flag & 0x08) ? map_id2bl (st->oid) :
            (struct block_list *) script_rid2sd (st);
        clif_GMmessage (bl, str, strlen (str) + 1, flag);
    }
    else
        intif_GMmessage (str, strlen (str) + 1);
    return 0;
}

/*==========================================
 * 天の声アナウンス（特定マップ）
 *------------------------------------------
 */
static void buildin_mapannounce_sub (struct block_list *bl, va_list ap)
{
    char *str;
    int  len, flag;
    str = va_arg (ap, char *);
    len = va_arg (ap, int);
    flag = va_arg (ap, int);
    clif_GMmessage (bl, str, len, flag | 3);
}

int buildin_mapannounce (struct script_state *st)
{
    const char *mapname, *str;
    int  flag, m;

    mapname = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    str = conv_str (st, &(st->stack->stack_data[st->start + 3]));
    flag = conv_num (st, &(st->stack->stack_data[st->start + 4]));

    if ((m = map_mapname2mapid (mapname)) < 0)
        return 0;
    map_foreachinarea (buildin_mapannounce_sub,
                       m, 0, 0, maps[m].xs, maps[m].ys, BL_PC, str,
                       strlen (str) + 1, flag & 0x10);
    return 0;
}

/*==========================================
 * 天の声アナウンス（特定エリア）
 *------------------------------------------
 */
int buildin_areaannounce (struct script_state *st)
{
    const char *map, *str;
    int  flag, m;
    int  x_0, y_0, x_1, y_1;

    map = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x_0 = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    y_0 = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    x_1 = conv_num (st, &(st->stack->stack_data[st->start + 5]));
    y_1 = conv_num (st, &(st->stack->stack_data[st->start + 6]));
    str = conv_str (st, &(st->stack->stack_data[st->start + 7]));
    flag = conv_num (st, &(st->stack->stack_data[st->start + 8]));

    if ((m = map_mapname2mapid (map)) < 0)
        return 0;

    map_foreachinarea (buildin_mapannounce_sub,
                       m, x_0, y_0, x_1, y_1, BL_PC, str, strlen (str) + 1,
                       flag & 0x10);
    return 0;
}

/*==========================================
 * ユーザー数所得
 *------------------------------------------
 */
int buildin_getusers (struct script_state *st)
{
    int  flag = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    struct block_list *bl = map_id2bl ((flag & 0x08) ? st->oid : st->rid);
    int  val = 0;
    switch (flag & 0x07)
    {
        case 0:
            val = maps[bl->m].users;
            break;
        case 1:
            val = map_getusers ();
            break;
    }
    push_val (st->stack, C_INT, val);
    return 0;
}

/*==========================================
 * マップ指定ユーザー数所得
 *------------------------------------------
 */
int buildin_getmapusers (struct script_state *st)
{
    const char *str;
    int  m;
    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    if ((m = map_mapname2mapid (str)) < 0)
    {
        push_val (st->stack, C_INT, -1);
        return 0;
    }
    push_val (st->stack, C_INT, maps[m].users);
    return 0;
}

/*==========================================
 * エリア指定ユーザー数所得
 *------------------------------------------
 */
static void buildin_getareausers_sub (struct block_list *, va_list ap)
{
    int *users = va_arg (ap, int *);
    (*users)++;
}

int buildin_getareausers (struct script_state *st)
{
    const char *str;
    int  m, x_0, y_0, x_1, y_1, users = 0;
    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x_0 = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    y_0 = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    x_1 = conv_num (st, &(st->stack->stack_data[st->start + 5]));
    y_1 = conv_num (st, &(st->stack->stack_data[st->start + 6]));
    if ((m = map_mapname2mapid (str)) < 0)
    {
        push_val (st->stack, C_INT, -1);
        return 0;
    }
    map_foreachinarea (buildin_getareausers_sub,
                       m, x_0, y_0, x_1, y_1, BL_PC, &users);
    push_val (st->stack, C_INT, users);
    return 0;
}

/*==========================================
 * エリア指定ドロップアイテム数所得
 *------------------------------------------
 */
static void buildin_getareadropitem_sub (struct block_list *bl, va_list ap)
{
    int  item = va_arg (ap, int);
    int *amount = va_arg (ap, int *);
    struct flooritem_data *drop = (struct flooritem_data *) bl;

    if (drop->item_data.nameid == item)
        (*amount) += drop->item_data.amount;
}

static void buildin_getareadropitem_sub_anddelete (struct block_list *bl, va_list ap)
{
    int  item = va_arg (ap, int);
    int *amount = va_arg (ap, int *);
    struct flooritem_data *drop = (struct flooritem_data *) bl;

    if (drop->item_data.nameid == item) {
        (*amount) += drop->item_data.amount;
        clif_clearflooritem(drop, 0);
        map_delobject(drop->bl.id, drop->bl.type);
    }
}

int buildin_getareadropitem (struct script_state *st)
{
    const char *str;
    int  m, x_0, y_0, x_1, y_1, item, amount = 0, delitems = 0;
    struct script_data *data;

    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x_0 = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    y_0 = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    x_1 = conv_num (st, &(st->stack->stack_data[st->start + 5]));
    y_1 = conv_num (st, &(st->stack->stack_data[st->start + 6]));

    data = &(st->stack->stack_data[st->start + 7]);
    get_val (st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str (st, data);
        struct item_data *item_data = itemdb_searchname (name);
        item = 512;
        if (item_data)
            item = item_data->nameid;
    }
    else
        item = conv_num (st, data);

    if (st->end > st->start + 8)
        delitems = conv_num (st, &(st->stack->stack_data[st->start + 8]));

    if ((m = map_mapname2mapid (str)) < 0)
    {
        push_val (st->stack, C_INT, -1);
        return 0;
    }
    if (delitems)
        map_foreachinarea (buildin_getareadropitem_sub_anddelete,
                           m, x_0, y_0, x_1, y_1, BL_ITEM, item, &amount);
    else
        map_foreachinarea (buildin_getareadropitem_sub,
                           m, x_0, y_0, x_1, y_1, BL_ITEM, item, &amount);

    push_val (st->stack, C_INT, amount);
    return 0;
}

/*==========================================
 * NPCの有効化
 *------------------------------------------
 */
int buildin_enablenpc (struct script_state *st)
{
    const char *str;
    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    npc_enable (str, 1);
    return 0;
}

/*==========================================
 * NPCの無効化
 *------------------------------------------
 */
int buildin_disablenpc (struct script_state *st)
{
    const char *str;
    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    npc_enable (str, 0);
    return 0;
}

/*==========================================
 * 隠れているNPCの表示
 *------------------------------------------
 */
int buildin_hideoffnpc (struct script_state *st)
{
    const char *str;
    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    npc_enable (str, 2);
    return 0;
}

/*==========================================
 * NPCをハイディング
 *------------------------------------------
 */
int buildin_hideonnpc (struct script_state *st)
{
    const char *str;
    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    npc_enable (str, 4);
    return 0;
}

/*==========================================
 * 状態異常にかかる
 *------------------------------------------
 */
int buildin_sc_start (struct script_state *st)
{
    struct block_list *bl;
    int  type, tick, val1;
    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    tick = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    val1 = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    if (st->end > st->start + 5)    //指定したキャラを状態異常にする
        bl = map_id2bl (conv_num
                        (st, &(st->stack->stack_data[st->start + 5])));
    else
        bl = map_id2bl (st->rid);
    if (bl->type == BL_PC
        && ((struct map_session_data *) bl)->state.potionpitcher_flag)
        bl = map_id2bl (((struct map_session_data *) bl)->skilltarget);
    skill_status_change_start (bl, type, val1, 0, 0, 0, tick, 0);
    return 0;
}

/*==========================================
 * 状態異常にかかる(確率指定)
 *------------------------------------------
 */
int buildin_sc_start2 (struct script_state *st)
{
    struct block_list *bl;
    int  type, tick, val1, per;
    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    tick = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    val1 = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    per = conv_num (st, &(st->stack->stack_data[st->start + 5]));
    if (st->end > st->start + 6)    //指定したキャラを状態異常にする
        bl = map_id2bl (conv_num
                        (st, &(st->stack->stack_data[st->start + 6])));
    else
        bl = map_id2bl (st->rid);
    if (bl->type == BL_PC
        && ((struct map_session_data *) bl)->state.potionpitcher_flag)
        bl = map_id2bl (((struct map_session_data *) bl)->skilltarget);
    if (MRAND (10000) < per)
        skill_status_change_start (bl, type, val1, 0, 0, 0, tick, 0);
    return 0;
}

/*==========================================
 * 状態異常が直る
 *------------------------------------------
 */
int buildin_sc_end (struct script_state *st)
{
    struct block_list *bl;
    int  type;
    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    bl = map_id2bl (st->rid);
    if (bl->type == BL_PC
        && ((struct map_session_data *) bl)->state.potionpitcher_flag)
        bl = map_id2bl (((struct map_session_data *) bl)->skilltarget);
    skill_status_change_end (bl, type, -1);
//  if(battle_config.etc_log)
//      printf("sc_end : %d %d\n",st->rid,type);
    return 0;
}

int buildin_sc_check (struct script_state *st)
{
    struct block_list *bl;
    int  type;
    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    bl = map_id2bl (st->rid);
    if (bl->type == BL_PC
        && ((struct map_session_data *) bl)->state.potionpitcher_flag)
        bl = map_id2bl (((struct map_session_data *) bl)->skilltarget);

    push_val (st->stack, C_INT, skill_status_change_active (bl, type));

    return 0;
}

/*==========================================
 * 状態異常耐性を計算した確率を返す
 *------------------------------------------
 */
int buildin_getscrate (struct script_state *st)
{
    struct block_list *bl;
    int  sc_def = 100, sc_def_vit2;
    int  type, rate, luk;

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    rate = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    if (st->end > st->start + 4)    //指定したキャラの耐性を計算する
        bl = map_id2bl (conv_num
                        (st, &(st->stack->stack_data[st->start + 6])));
    else
        bl = map_id2bl (st->rid);

    luk = battle_get_luk (bl);
    sc_def_vit2 = 100 - (3 + battle_get_vit (bl) + luk / 3);

    if (type == SC_POISON)
        sc_def = sc_def_vit2;

    rate = rate * sc_def / 100;
    push_val (st->stack, C_INT, rate);

    return 0;

}

/*==========================================
 *
 *------------------------------------------
 */
int buildin_debugmes (struct script_state *st)
{
    conv_str (st, &(st->stack->stack_data[st->start + 2]));
    printf ("script debug : %d %d : %s\n", st->rid, st->oid,
            st->stack->stack_data[st->start + 2].u.str);
    return 0;
}

/*==========================================
 * Added - AppleGirl For Advanced Classes, (Updated for Cleaner Script Purposes)
 *------------------------------------------
 */
int buildin_resetlvl (struct script_state *st)
{
    struct map_session_data *sd;

    int  type = conv_num (st, &(st->stack->stack_data[st->start + 2]));

    sd = script_rid2sd (st);
    pc_resetlvl (sd, type);
    return 0;
}

/*==========================================
 * ステータスリセット
 *------------------------------------------
 */
int buildin_resetstatus (struct script_state *st)
{
    struct map_session_data *sd;
    sd = script_rid2sd (st);
    pc_resetstate (sd);
    return 0;
}

/*==========================================
 * スキルリセット
 *------------------------------------------
 */
int buildin_resetskill (struct script_state *st)
{
    struct map_session_data *sd;
    sd = script_rid2sd (st);
    pc_resetskill (sd);
    return 0;
}

/*==========================================
 * 性別変換
 *------------------------------------------
 */
int buildin_changesex (struct script_state *st)
{
    struct map_session_data *sd = NULL;
    sd = script_rid2sd (st);

    if (sd->status.sex == 0)
    {
        sd->status.sex = 1;
        sd->sex = 1;
    }
    else if (sd->status.sex == 1)
    {
        sd->status.sex = 0;
        sd->sex = 0;
    }
    chrif_char_ask_name (-1, sd->status.name, CharOperation::CHANGE_SEX);
    chrif_save (sd);
    return 0;
}

/*==========================================
 * RIDのアタッチ
 *------------------------------------------
 */
int buildin_attachrid (struct script_state *st)
{
    st->rid = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    push_val (st->stack, C_INT, (map_id2sd (st->rid) != NULL));
    return 0;
}

/*==========================================
 * RIDのデタッチ
 *------------------------------------------
 */
int buildin_detachrid (struct script_state *st)
{
    st->rid = 0;
    return 0;
}

/*==========================================
 * 存在チェック
 *------------------------------------------
 */
int buildin_isloggedin (struct script_state *st)
{
    push_val (st->stack, C_INT,
              map_id2sd (conv_num
                         (st,
                          &(st->stack->stack_data[st->start + 2]))) != NULL);
    return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
enum
{
    MF_NOMEMO, MF_NOTELEPORT, MF_NOSAVE, MF_NOBRANCH, MF_NOPENALTY,
    MF_NOZENYPENALTY, MF_PVP, MF_PVP_NOPARTY,
    MF_NOTRADE, MF_NOSKILL, MF_NOWARP, MF_NOPVP,
};

int buildin_setmapflagnosave (struct script_state *st)
{
    int  m, x, y;
    const char *str, *str2;

    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    str2 = conv_str (st, &(st->stack->stack_data[st->start + 3]));
    x = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    y = conv_num (st, &(st->stack->stack_data[st->start + 5]));
    m = map_mapname2mapid (str);
    if (m >= 0)
    {
        maps[m].flag.nosave = 1;
        memcpy (maps[m].save.map, str2, 16);
        maps[m].save.x = x;
        maps[m].save.y = y;
    }

    return 0;
}

int buildin_setmapflag (struct script_state *st)
{
    int  m, i;
    const char *str;

    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    i = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    m = map_mapname2mapid (str);
    if (m >= 0)
    {
        switch (i)
        {
            case MF_NOMEMO:
                maps[m].flag.nomemo = 1;
                break;
            case MF_NOTELEPORT:
                maps[m].flag.noteleport = 1;
                break;
            case MF_NOBRANCH:
                maps[m].flag.nobranch = 1;
                break;
            case MF_NOPENALTY:
                maps[m].flag.nopenalty = 1;
                break;
            case MF_PVP_NOPARTY:
                maps[m].flag.pvp_noparty = 1;
                break;
            case MF_NOZENYPENALTY:
                maps[m].flag.nozenypenalty = 1;
                break;
            case MF_NOTRADE:
                maps[m].flag.notrade = 1;
                break;
            case MF_NOWARP:
                maps[m].flag.nowarp = 1;
                break;
            case MF_NOPVP:
                maps[m].flag.nopvp = 1;
                break;
        }
    }

    return 0;
}

int buildin_removemapflag (struct script_state *st)
{
    int  m, i;
    const char *str;

    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    i = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    m = map_mapname2mapid (str);
    if (m >= 0)
    {
        switch (i)
        {
            case MF_NOMEMO:
                maps[m].flag.nomemo = 0;
                break;
            case MF_NOTELEPORT:
                maps[m].flag.noteleport = 0;
                break;
            case MF_NOSAVE:
                maps[m].flag.nosave = 0;
                break;
            case MF_NOBRANCH:
                maps[m].flag.nobranch = 0;
                break;
            case MF_NOPENALTY:
                maps[m].flag.nopenalty = 0;
                break;
            case MF_PVP_NOPARTY:
                maps[m].flag.pvp_noparty = 0;
                break;
            case MF_NOZENYPENALTY:
                maps[m].flag.nozenypenalty = 0;
                break;
            case MF_NOWARP:
                maps[m].flag.nowarp = 0;
                break;
            case MF_NOPVP:
                maps[m].flag.nopvp = 0;
                break;
        }
    }

    return 0;
}

int buildin_pvpon (struct script_state *st)
{
    int  m, i;
    const char *str;
    struct map_session_data *pl_sd = NULL;

    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    m = map_mapname2mapid (str);
    if (m >= 0 && !maps[m].flag.pvp && !maps[m].flag.nopvp)
    {
        maps[m].flag.pvp = 1;
        if (battle_config.pk_mode)  // disable ranking functions if pk_mode is on [Valaris]
            return 0;

        for (i = 0; i < fd_max; i++)
        {                       //人数分ループ
            if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
                && pl_sd->state.auth)
            {
                if (m == pl_sd->bl.m && pl_sd->pvp_timer == -1)
                {
                    pl_sd->pvp_timer =
                        add_timer (gettick () + 200, pc_calc_pvprank_timer,
                                   pl_sd->bl.id, 0);
                    pl_sd->pvp_rank = 0;
                    pl_sd->pvp_lastusers = 0;
                    pl_sd->pvp_point = 5;
                }
            }
        }
    }

    return 0;
}

int buildin_pvpoff (struct script_state *st)
{
    int  m, i;
    const char *str;
    struct map_session_data *pl_sd = NULL;

    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    m = map_mapname2mapid (str);
    if (m >= 0 && maps[m].flag.pvp && maps[m].flag.nopvp)
    {
        maps[m].flag.pvp = 0;
        if (battle_config.pk_mode)  // disable ranking options if pk_mode is on [Valaris]
            return 0;

        for (i = 0; i < fd_max; i++)
        {                       //人数分ループ
            if (session[i] && (pl_sd = (struct map_session_data *)session[i]->session_data)
                && pl_sd->state.auth)
            {
                if (m == pl_sd->bl.m)
                {
                    if (pl_sd->pvp_timer != -1)
                    {
                        delete_timer (pl_sd->pvp_timer,
                                      pc_calc_pvprank_timer);
                        pl_sd->pvp_timer = -1;
                    }
                }
            }
        }
    }

    return 0;
}

/*==========================================
 *	NPCエモーション
 *------------------------------------------
 */

int buildin_emotion (struct script_state *st)
{
    int  type;
    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    if (type < 0 || type > 100)
        return 0;
    clif_emotion (map_id2bl (st->oid), type);
    return 0;
}

/* =====================================================================
 * カードの数を得る
 * ---------------------------------------------------------------------
 */
int buildin_getequipcardcnt (struct script_state *st)
{
    int  i, num;
    struct map_session_data *sd;
    int  c = 4;

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    i = pc_checkequip (sd, equip[num - 1]);
    if (sd->status.inventory[i].card[0] == 0x00ff)
    {                           // 製造武器はカードなし
        push_val (st->stack, C_INT, 0);
        return 0;
    }
    do
    {
        if ((sd->status.inventory[i].card[c - 1] > 4000) &&
            (sd->status.inventory[i].card[c - 1] < 5000))
        {

            push_val (st->stack, C_INT, (c));
            return 0;
        }
    }
    while (c--);
    push_val (st->stack, C_INT, 0);
    return 0;
}

/* ================================================================
 * カード取り外し成功
 * ----------------------------------------------------------------
 */
int buildin_successremovecards (struct script_state *st)
{
    int  i, num, cardflag = 0, flag;
    struct map_session_data *sd;
    struct item item_tmp;
    int  c = 4;

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd (st);
    i = pc_checkequip (sd, equip[num - 1]);
    if (sd->status.inventory[i].card[0] == 0x00ff)
    {                           // 製造武器は処理しない
        return 0;
    }
    do
    {
        if ((sd->status.inventory[i].card[c - 1] > 4000) &&
            (sd->status.inventory[i].card[c - 1] < 5000))
        {

            cardflag = 1;
            item_tmp.id = 0, item_tmp.nameid =
                sd->status.inventory[i].card[c - 1];
            item_tmp.equip = 0, item_tmp.identify = 1, item_tmp.refine = 0;
            item_tmp.attribute = 0;
            item_tmp.card[0] = 0, item_tmp.card[1] = 0, item_tmp.card[2] =
                0, item_tmp.card[3] = 0;

            if ((flag = pc_additem (sd, &item_tmp, 1)))
            {                   // 持てないならドロップ
                clif_additem (sd, 0, 0, flag);
                map_addflooritem (&item_tmp, 1, sd->bl.m, sd->bl.x, sd->bl.y,
                                  NULL, NULL, NULL);
            }
        }
    }
    while (c--);

    if (cardflag == 1)
    {                           // カードを取り除いたアイテム所得
        flag = 0;
        item_tmp.id = 0, item_tmp.nameid = sd->status.inventory[i].nameid;
        item_tmp.equip = 0, item_tmp.identify = 1, item_tmp.refine =
            sd->status.inventory[i].refine;
        item_tmp.attribute = sd->status.inventory[i].attribute;
        item_tmp.card[0] = 0, item_tmp.card[1] = 0, item_tmp.card[2] =
            0, item_tmp.card[3] = 0;
        pc_delitem (sd, i, 1, 0);
        if ((flag = pc_additem (sd, &item_tmp, 1)))
        {                       // もてないならドロップ
            clif_additem (sd, 0, 0, flag);
            map_addflooritem (&item_tmp, 1, sd->bl.m, sd->bl.x, sd->bl.y,
                              NULL, NULL, NULL);
        }
        clif_misceffect (&sd->bl, 3);
        return 0;
    }
    return 0;
}

/* ================================================================
 * カード取り外し失敗 slot,type
 * type=0: 両方損失、1:カード損失、2:武具損失、3:損失無し
 * ----------------------------------------------------------------
 */
int buildin_failedremovecards (struct script_state *st)
{
    int  i, num, cardflag = 0, flag, typefail;
    struct map_session_data *sd;
    struct item item_tmp;
    int  c = 4;

    num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    typefail = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    sd = script_rid2sd (st);
    i = pc_checkequip (sd, equip[num - 1]);
    if (sd->status.inventory[i].card[0] == 0x00ff)
    {                           // 製造武器は処理しない
        return 0;
    }
    do
    {
        if ((sd->status.inventory[i].card[c - 1] > 4000) &&
            (sd->status.inventory[i].card[c - 1] < 5000))
        {

            cardflag = 1;

            if (typefail == 2)
            {                   // 武具のみ損失なら、カードは受け取らせる
                item_tmp.id = 0, item_tmp.nameid =
                    sd->status.inventory[i].card[c - 1];
                item_tmp.equip = 0, item_tmp.identify = 1, item_tmp.refine =
                    0;
                item_tmp.attribute = 0;
                item_tmp.card[0] = 0, item_tmp.card[1] = 0, item_tmp.card[2] =
                    0, item_tmp.card[3] = 0;
                if ((flag = pc_additem (sd, &item_tmp, 1)))
                {
                    clif_additem (sd, 0, 0, flag);
                    map_addflooritem (&item_tmp, 1, sd->bl.m, sd->bl.x,
                                      sd->bl.y, NULL, NULL, NULL);
                }
            }
        }
    }
    while (c--);

    if (cardflag == 1)
    {

        if (typefail == 0 || typefail == 2)
        {                       // 武具損失
            pc_delitem (sd, i, 1, 0);
            clif_misceffect (&sd->bl, 2);
            return 0;
        }
        if (typefail == 1)
        {                       // カードのみ損失（武具を返す）
            flag = 0;
            item_tmp.id = 0, item_tmp.nameid = sd->status.inventory[i].nameid;
            item_tmp.equip = 0, item_tmp.identify = 1, item_tmp.refine =
                sd->status.inventory[i].refine;
            item_tmp.attribute = sd->status.inventory[i].attribute;
            item_tmp.card[0] = 0, item_tmp.card[1] = 0, item_tmp.card[2] =
                0, item_tmp.card[3] = 0;
            pc_delitem (sd, i, 1, 0);
            if ((flag = pc_additem (sd, &item_tmp, 1)))
            {
                clif_additem (sd, 0, 0, flag);
                map_addflooritem (&item_tmp, 1, sd->bl.m, sd->bl.x, sd->bl.y,
                                  NULL, NULL, NULL);
            }
        }
        clif_misceffect (&sd->bl, 2);
        return 0;
    }
    return 0;
}

int buildin_mapwarp (struct script_state *st)   // Added by RoVeRT
{
    int  x, y, m;
    const char *str;
    const char *mapname;
    int  x_0, y_0, x_1, y_1;

    mapname = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x_0 = 0;
    y_0 = 0;
    x_1 = maps[map_mapname2mapid (mapname)].xs;
    y_1 = maps[map_mapname2mapid (mapname)].ys;
    str = conv_str (st, &(st->stack->stack_data[st->start + 3]));
    x = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    y = conv_num (st, &(st->stack->stack_data[st->start + 5]));

    if ((m = map_mapname2mapid (mapname)) < 0)
        return 0;

    map_foreachinarea (buildin_areawarp_sub,
                       m, x_0, y_0, x_1, y_1, BL_PC, str, x, y);
    return 0;
}

int buildin_cmdothernpc (struct script_state *st)   // Added by RoVeRT
{
    const char *npc, *command;

    npc = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    command = conv_str (st, &(st->stack->stack_data[st->start + 3]));

    npc_command (map_id2sd (st->rid), npc, command);
    return 0;
}

int buildin_inittimer (struct script_state *st) // Added by RoVeRT
{
//  struct npc_data *nd=(struct npc_data*)map_id2bl(st->oid);

//  nd->lastaction=nd->timer=gettick();
    npc_do_ontimer (st->oid, map_id2sd (st->rid), 1);

    return 0;
}

int buildin_stoptimer (struct script_state *st) // Added by RoVeRT
{
//  struct npc_data *nd=(struct npc_data*)map_id2bl(st->oid);

//  nd->lastaction=nd->timer=-1;
    npc_do_ontimer (st->oid, map_id2sd (st->rid), 0);

    return 0;
}

static void buildin_mobcount_sub (struct block_list *bl, va_list ap)    // Added by RoVeRT
{
    char *event = va_arg (ap, char *);
    int *c = va_arg (ap, int *);

    if (strcmp (event, ((struct mob_data *) bl)->npc_event) == 0)
        (*c)++;
}

int buildin_mobcount (struct script_state *st)  // Added by RoVeRT
{
    const char *mapname, *event;
    int  m, c = 0;
    mapname = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    event = conv_str (st, &(st->stack->stack_data[st->start + 3]));

    if ((m = map_mapname2mapid (mapname)) < 0)
    {
        push_val (st->stack, C_INT, -1);
        return 0;
    }
    map_foreachinarea (buildin_mobcount_sub,
                       m, 0, 0, maps[m].xs, maps[m].ys, BL_MOB, event, &c);

    push_val (st->stack, C_INT, (c - 1));

    return 0;
}

int buildin_marriage (struct script_state *st)
{
    const char *partner = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    struct map_session_data *sd = script_rid2sd (st);
    struct map_session_data *p_sd = map_nick2sd (partner);

    if (sd == NULL || p_sd == NULL || pc_marriage (sd, p_sd) < 0)
    {
        push_val (st->stack, C_INT, 0);
        return 0;
    }
    push_val (st->stack, C_INT, 1);
    return 0;
}

int buildin_wedding_effect (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);

    if (sd == NULL)
        return 0;
    clif_wedding_effect (&sd->bl);
    return 0;
}

int buildin_divorce (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);

    st->state = STOP;           // rely on pc_divorce to restart

    sd->npc_flags.divorce = 1;

    if (sd == NULL || pc_divorce (sd) < 0)
    {
        push_val (st->stack, C_INT, 0);
        return 0;
    }

    push_val (st->stack, C_INT, 1);
    return 0;
}

/*================================================
 * Script for Displaying MOB Information [Valaris]
 *------------------------------------------------
 */
int buildin_strmobinfo (struct script_state *st)
{

    int  num = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    int  mob_class = conv_num (st, &(st->stack->stack_data[st->start + 3]));

    if (num <= 0 || num >= 8 || (mob_class >= 0 && mob_class <= 1000) || mob_class > 2000)
        return 0;

    if (num == 1)
    {
        char *buf;
        buf = mob_db[mob_class].name;
        push_str (st->stack, C_STR, buf);
        return 0;
    }
    else if (num == 2)
    {
        char *buf;
        buf = mob_db[mob_class].jname;
        push_str (st->stack, C_STR, buf);
        return 0;
    }
    else if (num == 3)
        push_val (st->stack, C_INT, mob_db[mob_class].lv);
    else if (num == 4)
        push_val (st->stack, C_INT, mob_db[mob_class].max_hp);
    else if (num == 5)
        push_val (st->stack, C_INT, mob_db[mob_class].max_sp);
    else if (num == 6)
        push_val (st->stack, C_INT, mob_db[mob_class].base_exp);
    else if (num == 7)
        push_val (st->stack, C_INT, mob_db[mob_class].job_exp);
    return 0;
}

/*==========================================
 * IDからItem名
 *------------------------------------------
 */
int buildin_getitemname (struct script_state *st)
{
    struct item_data *i_data;
    char *item_name;
    struct script_data *data;

    data = &(st->stack->stack_data[st->start + 2]);
    get_val (st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str (st, data);
        i_data = itemdb_searchname (name);
    }
    else
    {
        int  item_id = conv_num (st, data);
        i_data = itemdb_search (item_id);
    }

    item_name = (char *) calloc (24, 1);
    if (i_data)
        strncpy (item_name, i_data->jname, 23);
    else
        strncpy (item_name, "Unknown Item", 23);

    push_str (st->stack, C_STR, item_name);

    return 0;
}

int buildin_getspellinvocation (struct script_state *st)
{
    const char *name;
    const char *invocation;

    name = conv_str (st, &(st->stack->stack_data[st->start + 2]));

    invocation = magic_find_invocation (name);
    if (!invocation)
        invocation = "...";

    push_str (st->stack, C_STR, strdup (invocation));
    return 0;
}

int buildin_getanchorinvocation (struct script_state *st)
{
    const char *name;
    const char *invocation;

    name = conv_str (st, &(st->stack->stack_data[st->start + 2]));

    invocation = magic_find_anchor_invocation (name);
    if (!invocation)
        invocation = "...";

    push_str (st->stack, C_STR, strdup (invocation));
    return 0;
}

int buildin_getpartnerid (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);

    push_val (st->stack, C_INT, sd->status.partner_id);
    return 0;
}

/*==========================================
 * PCの所持品情報読み取り
 *------------------------------------------
 */
int buildin_getinventorylist (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    int  i, j = 0;
    if (!sd)
        return 0;
    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid > 0
            && sd->status.inventory[i].amount > 0)
        {
            pc_setreg (sd, add_str ("@inventorylist_id") + (j << 24),
                       sd->status.inventory[i].nameid);
            pc_setreg (sd, add_str ("@inventorylist_amount") + (j << 24),
                       sd->status.inventory[i].amount);
            pc_setreg (sd, add_str ("@inventorylist_equip") + (j << 24),
                       sd->status.inventory[i].equip);
            pc_setreg (sd, add_str ("@inventorylist_refine") + (j << 24),
                       sd->status.inventory[i].refine);
            pc_setreg (sd, add_str ("@inventorylist_identify") + (j << 24),
                       sd->status.inventory[i].identify);
            pc_setreg (sd, add_str ("@inventorylist_attribute") + (j << 24),
                       sd->status.inventory[i].attribute);
            pc_setreg (sd, add_str ("@inventorylist_card1") + (j << 24),
                       sd->status.inventory[i].card[0]);
            pc_setreg (sd, add_str ("@inventorylist_card2") + (j << 24),
                       sd->status.inventory[i].card[1]);
            pc_setreg (sd, add_str ("@inventorylist_card3") + (j << 24),
                       sd->status.inventory[i].card[2]);
            pc_setreg (sd, add_str ("@inventorylist_card4") + (j << 24),
                       sd->status.inventory[i].card[3]);
            j++;
        }
    }
    pc_setreg (sd, add_str ("@inventorylist_count"), j);
    return 0;
}

int buildin_getskilllist (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    int  i, j = 0;
    if (!sd)
        return 0;
    for (i = 0; i < MAX_SKILL; i++)
    {
        if (sd->status.skill[i].id > 0 && sd->status.skill[i].lv > 0)
        {
            pc_setreg (sd, add_str ("@skilllist_id") + (j << 24),
                       sd->status.skill[i].id);
            pc_setreg (sd, add_str ("@skilllist_lv") + (j << 24),
                       sd->status.skill[i].lv);
            pc_setreg (sd, add_str ("@skilllist_flag") + (j << 24),
                       sd->status.skill[i].flags);
            j++;
        }
    }
    pc_setreg (sd, add_str ("@skilllist_count"), j);
    return 0;
}

int buildin_get_activated_pool_skills (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    int  pool_skills[MAX_SKILL_POOL];
    int  pool_size = skill_pool (sd, pool_skills);
    int  i, count = 0;

    if (!sd)
        return 0;

    for (i = 0; i < pool_size; i++)
    {
        int  skill_id = pool_skills[i];

        if (sd->status.skill[skill_id].id == skill_id)
        {
            pc_setreg (sd, add_str ("@skilllist_id") + (count << 24),
                       sd->status.skill[skill_id].id);
            pc_setreg (sd, add_str ("@skilllist_lv") + (count << 24),
                       sd->status.skill[skill_id].lv);
            pc_setreg (sd, add_str ("@skilllist_flag") + (count << 24),
                       sd->status.skill[skill_id].flags);
            pc_setregstr (sd, add_str ("@skilllist_name$") + (count << 24),
                          skill_name (skill_id));
            ++count;
        }
    }
    pc_setreg (sd, add_str ("@skilllist_count"), count);

    return 0;
}

extern int skill_pool_skills[];
extern int skill_pool_skills_size;

int buildin_get_unactivated_pool_skills (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    int  i, count = 0;

    if (!sd)
        return 0;

    for (i = 0; i < skill_pool_skills_size; i++)
    {
        int  skill_id = skill_pool_skills[i];

        if (sd->status.skill[skill_id].id == skill_id && !(sd->status.skill[skill_id].flags & SKILL_POOL_ACTIVATED))
        {
            pc_setreg (sd, add_str ("@skilllist_id") + (count << 24),
                       sd->status.skill[skill_id].id);
            pc_setreg (sd, add_str ("@skilllist_lv") + (count << 24),
                       sd->status.skill[skill_id].lv);
            pc_setreg (sd, add_str ("@skilllist_flag") + (count << 24),
                       sd->status.skill[skill_id].flags);
            pc_setregstr (sd, add_str ("@skilllist_name$") + (count << 24),
                          skill_name (skill_id));
            ++count;
        }
    }
    pc_setreg (sd, add_str ("@skilllist_count"), count);

    return 0;
}

int buildin_get_pool_skills (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    int  i, count = 0;

    if (!sd)
        return 0;

    for (i = 0; i < skill_pool_skills_size; i++)
    {
        int  skill_id = skill_pool_skills[i];

        if (sd->status.skill[skill_id].id == skill_id)
        {
            pc_setreg (sd, add_str ("@skilllist_id") + (count << 24),
                       sd->status.skill[skill_id].id);
            pc_setreg (sd, add_str ("@skilllist_lv") + (count << 24),
                       sd->status.skill[skill_id].lv);
            pc_setreg (sd, add_str ("@skilllist_flag") + (count << 24),
                       sd->status.skill[skill_id].flags);
            pc_setregstr (sd, add_str ("@skilllist_name$") + (count << 24),
                          skill_name (skill_id));
            ++count;
        }
    }
    pc_setreg (sd, add_str ("@skilllist_count"), count);

    return 0;
}

int buildin_activate_pool_skill (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    int  skill_id = conv_num (st, &(st->stack->stack_data[st->start + 2]));

    skill_pool_activate (sd, skill_id);
    clif_skillinfoblock (sd);

    return 0;
}

int buildin_deactivate_pool_skill (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    int  skill_id = conv_num (st, &(st->stack->stack_data[st->start + 2]));

    skill_pool_deactivate (sd, skill_id);
    clif_skillinfoblock (sd);

    return 0;
}

int buildin_check_pool_skill (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    int  skill_id = conv_num (st, &(st->stack->stack_data[st->start + 2]));

    push_val (st->stack, C_INT, skill_pool_is_activated (sd, skill_id));

    return 0;
}

int buildin_clearitem (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    int  i;
    if (sd == NULL)
        return 0;
    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].amount)
            pc_delitem (sd, i, sd->status.inventory[i].amount, 0);
    }
    return 0;
}

/*==========================================
 * NPCから発生するエフェクト
 * misceffect(effect, [target])
 *
 * effect The effect type/ID.
 * target The player name or being ID on
 *  which to display the effect. If not
 *  specified, it attempts to default to
 *  the current NPC or invoking PC.
 *------------------------------------------
 */
int buildin_misceffect (struct script_state *st)
{
    int  type;
    int  id = 0;
    const char *name = NULL;
    struct block_list *bl = NULL;

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));

    if (st->end > st->start + 3)
    {
        struct script_data *sdata = &(st->stack->stack_data[st->start + 3]);

        get_val (st, sdata);

        if (sdata->type == C_STR || sdata->type == C_CONSTSTR)
            name = conv_str (st, sdata);
        else
            id = conv_num (st, sdata);
    }

    if (name)
    {
        struct map_session_data *sd = map_nick2sd (name);
        if (sd)
            bl = &sd->bl;
    }
    else if (id)
        bl = map_id2bl (id);
    else if (st->oid)
        bl = map_id2bl (st->oid);
    else
    {
        struct map_session_data *sd = script_rid2sd (st);
        if (sd)
            bl = &sd->bl;
    }

    if (bl)
        clif_misceffect (bl, type);

    return 0;
}

/*==========================================
 * サウンドエフェクト
 *------------------------------------------
 */
int buildin_soundeffect (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    const char *name;
    int  type = 0;

    name = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    type = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    if (sd)
    {
        if (st->oid)
            clif_soundeffect (sd, map_id2bl (st->oid), name, type);
        else
        {
            clif_soundeffect (sd, &sd->bl, name, type);
        }
    }
    return 0;
}

/*==========================================
 * Special effects [Valaris]
 *------------------------------------------
 */
int buildin_specialeffect (struct script_state *st)
{
    struct block_list *bl = map_id2bl (st->oid);

    if (bl == NULL)
        return 0;

    clif_specialeffect (bl,
                        conv_num (st,
                                  &(st->stack->stack_data[st->start + 2])),
                        0);

    return 0;
}

int buildin_specialeffect2 (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);

    if (sd == NULL)
        return 0;

    clif_specialeffect (&sd->bl,
                        conv_num (st,
                                  &(st->stack->stack_data[st->start + 2])),
                        0);

    return 0;
}

/*==========================================
 * Nude [Valaris]
 *------------------------------------------
 */

int buildin_nude (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    int  i;

    if (sd == NULL)
        return 0;

    for (i = 0; i < 11; i++)
        if (sd->equip_index[i] >= 0)
            pc_unequipitem (sd, sd->equip_index[i], i);
    pc_calcstatus (sd, 0);

    return 0;
}

/*==========================================
 * UnequipById [Freeyorp]
 *------------------------------------------
 */

int buildin_unequip_by_id (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    if (sd == NULL)
        return 0;

    int  slot_id = conv_num (st, &(st->stack->stack_data[st->start + 2]));

    if (slot_id >= 0 && slot_id < 11 && sd->equip_index[slot_id] >= 0)
        pc_unequipitem (sd, sd->equip_index[slot_id], slot_id);

    pc_calcstatus (sd, 0);

    return 0;
}

/*==========================================
 * gmcommand [MouseJstr]
 *
 * suggested on the forums...
 *------------------------------------------
 */

int buildin_gmcommand (struct script_state *st)
{
    struct map_session_data *sd;
    const char *cmd;

    sd = script_rid2sd (st);
    cmd = conv_str (st, &(st->stack->stack_data[st->start + 2]));

    is_atcommand (sd->fd, sd, cmd, 99);

    return 0;
}

/*==========================================
 * movenpc [MouseJstr]
 *------------------------------------------
 */

int buildin_movenpc (struct script_state *)
{
#if 0
    struct map_session_data *sd;
    const char *map, *npc;
    int  x, y;

    sd = script_rid2sd (st);

    map = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    y = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    npc = conv_str (st, &(st->stack->stack_data[st->start + 5]));

    // TODO actually implement this - the next function might be useful
#endif
    return 0;
}

/*==========================================
 * npcwarp [remoitnane]
 * Move NPC to a new position on the same map.
 *------------------------------------------
 */
int buildin_npcwarp (struct script_state *st)
{
    int  x, y;
    const char *npc;
    struct npc_data *nd = NULL;

    x = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    y = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    npc = conv_str (st, &(st->stack->stack_data[st->start + 4]));
    nd = npc_name2id (npc);

    if (!nd)
        return -1;

    short m = nd->bl.m;

    /* Crude sanity checks. */
    if (m < 0 || !nd->bl.prev
            || x < 0 || x > maps[m].xs -1
            || y < 0 || y > maps[m].ys - 1)
        return -1;

    npc_enable (npc, 0);
    map_delblock(&nd->bl); /* [Freeyorp] */
    nd->bl.x = x;
    nd->bl.y = y;
    map_addblock(&nd->bl);
    npc_enable (npc, 1);

    return 0;
}

/*==========================================
 * message [MouseJstr]
 *------------------------------------------
 */

int buildin_message (struct script_state *st)
{
    const char *msg, *player;
    struct map_session_data *pl_sd = NULL;

    player = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    msg = conv_str (st, &(st->stack->stack_data[st->start + 3]));

    if ((pl_sd = map_nick2sd ((char *) player)) == NULL)
        return 1;
    clif_displaymessage (pl_sd->fd, msg);

    return 0;
}

/*==========================================
 * npctalk (sends message to surrounding
 * area) [Valaris]
 *------------------------------------------
 */

int buildin_npctalk (struct script_state *st)
{
    const char *str;
    char message[255];

    struct npc_data *nd = (struct npc_data *) map_id2bl (st->oid);
    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));

    if (nd)
    {
        memcpy (message, nd->name, 24);
        strcat (message, " : ");
        strcat (message, str);
        clif_message (&(nd->bl), message);
    }

    return 0;
}

/*==========================================
 * hasitems (checks to see if player has any
 * items on them, if so will return a 1)
 * [Valaris]
 *------------------------------------------
 */

int buildin_hasitems (struct script_state *st)
{
    int  i;
    struct map_session_data *sd;

    sd = script_rid2sd (st);

    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].amount)
        {
            push_val (st->stack, C_INT, 1);
            return 0;
        }
    }

    push_val (st->stack, C_INT, 0);

    return 0;
}

/*==========================================
  * getlook char info. getlook(arg)
  *------------------------------------------
  */
int buildin_getlook (struct script_state *st)
{
    int  type, val;
    struct map_session_data *sd;
    sd = script_rid2sd (st);

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));
    val = -1;
    switch (type)
    {
        case LOOK_HAIR:        //1
            val = sd->status.hair;
            break;
        case LOOK_WEAPON:      //2
            val = sd->status.weapon;
            break;
        case LOOK_HEAD_BOTTOM: //3
            val = sd->status.head_bottom;
            break;
        case LOOK_HEAD_TOP:    //4
            val = sd->status.head_top;
            break;
        case LOOK_HEAD_MID:    //5
            val = sd->status.head_mid;
            break;
        case LOOK_HAIR_COLOR:  //6
            val = sd->status.hair_color;
            break;
        case LOOK_CLOTHES_COLOR:   //7
            val = sd->status.clothes_color;
            break;
        case LOOK_SHIELD:      //8
            val = sd->status.shield;
            break;
        case LOOK_SHOES:       //9
            break;
    }

    push_val (st->stack, C_INT, val);
    return 0;
}

/*==========================================
  *     get char save point. argument: 0- map name, 1- x, 2- y
  *------------------------------------------
*/
int buildin_getsavepoint (struct script_state *st)
{
    int  x, y, type;
    char *mapname;
    struct map_session_data *sd;

    sd = script_rid2sd (st);

    type = conv_num (st, &(st->stack->stack_data[st->start + 2]));

    x = sd->status.save_point.x;
    y = sd->status.save_point.y;
    switch (type)
    {
        case 0:
            mapname = (char*)calloc (24, 1);
            strncpy (mapname, sd->status.save_point.map, 23);
            push_str (st->stack, C_STR, mapname);
            break;
        case 1:
            push_val (st->stack, C_INT, x);
            break;
        case 2:
            push_val (st->stack, C_INT, y);
            break;
    }
    return 0;
}

/*==========================================
 *     areatimer
 *------------------------------------------
 */
static void buildin_areatimer_sub (struct block_list *bl, va_list ap)
{
    int  tick;
    char *event;
    tick = va_arg (ap, int);
    event = va_arg (ap, char *);
    pc_addeventtimer ((struct map_session_data *) bl, tick, event);
}

int buildin_areatimer (struct script_state *st)
{
    int  tick, m;
    const char *event;
    const char *mapname;
    int  x_0, y_0, x_1, y_1;

    mapname = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x_0 = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    y_0 = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    x_1 = conv_num (st, &(st->stack->stack_data[st->start + 5]));
    y_1 = conv_num (st, &(st->stack->stack_data[st->start + 6]));
    tick = conv_num (st, &(st->stack->stack_data[st->start + 7]));
    event = conv_str (st, &(st->stack->stack_data[st->start + 8]));

    if ((m = map_mapname2mapid (mapname)) < 0)
        return 0;

    map_foreachinarea (buildin_areatimer_sub,
                       m, x_0, y_0, x_1, y_1, BL_PC, tick, event);
    return 0;
}

/*==========================================
 * Check whether the PC is in the specified rectangle
 *------------------------------------------
 */
int buildin_isin (struct script_state *st)
{
    int  x_1, y_1, x2, y2;
    const char *str;
    struct map_session_data *sd = script_rid2sd (st);

    str = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    x_1 = conv_num (st, &(st->stack->stack_data[st->start + 3]));
    y_1 = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    x2 = conv_num (st, &(st->stack->stack_data[st->start + 5]));
    y2 = conv_num (st, &(st->stack->stack_data[st->start + 6]));

    if (!sd)
        return 1;

    push_val (st->stack, C_INT,
              (sd->bl.x >= x_1 && sd->bl.x <= x2)
              && (sd->bl.y >= y_1 && sd->bl.y <= y2)
              && (!strcmp (str, maps[sd->bl.m].name)));

    return 0;
}

// Trigger the shop on a (hopefully) nearby shop NPC
int buildin_shop (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);
    struct npc_data *nd;

    if (!sd)
        return 1;

    nd = npc_name2id (conv_str (st, &(st->stack->stack_data[st->start + 2])));
    if (!nd)
        return 1;

    buildin_close (st);
    clif_npcbuysell (sd, nd->bl.id);
    return 0;
}

/*==========================================
 * Check whether the PC is dead
 *------------------------------------------
 */
int buildin_isdead (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);

    push_val (st->stack, C_INT, pc_isdead (sd));
    return 0;
}

/*========================================
 * Changes a NPC name, and sprite
 *----------------------------------------
 */
int buildin_fakenpcname (struct script_state *st)
{
    const char *name, *newname;
    int  newsprite;
    struct npc_data *nd;

    name = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    newname = conv_str (st, &(st->stack->stack_data[st->start + 3]));
    newsprite = conv_num (st, &(st->stack->stack_data[st->start + 4]));
    nd = npc_name2id (name);
    if (!nd)
        return 1;
    strncpy (nd->name, newname, sizeof(nd->name)-1);
    nd->name[sizeof(nd->name)-1] = '\0';
    nd->npc_class = newsprite;

    // Refresh this npc
    npc_enable (name, 0);
    npc_enable (name, 1);

    return 0;
}

/*============================
 * Gets the PC's x pos
 *----------------------------
 */

int buildin_getx (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);

    push_val (st->stack, C_INT, sd->bl.x);
    return 0;
}

/*============================
 * Gets the PC's y pos
 *----------------------------
 */
int buildin_gety (struct script_state *st)
{
    struct map_session_data *sd = script_rid2sd (st);

    push_val (st->stack, C_INT, sd->bl.y);
    return 0;
}

//
// 実行部main
//
/*==========================================
 * コマンドの読み取り
 *------------------------------------------
 */
static int get_com (unsigned char *script, int *pos)
{
    int  i, j;
    if (script[*pos] >= 0x80)
    {
        return C_INT;
    }
    i = 0;
    j = 0;
    while (script[*pos] >= 0x40)
    {
        i = script[(*pos)++] << j;
        j += 6;
    }
    return i + (script[(*pos)++] << j);
}


/*==========================================
 * 数値の所得
 *------------------------------------------
 */
static int get_num (unsigned char *script, int *pos)
{
    int  i, j;
    i = 0;
    j = 0;
    while (script[*pos] >= 0xc0)
    {
        i += (script[(*pos)++] & 0x7f) << j;
        j += 6;
    }
    return i + ((script[(*pos)++] & 0x7f) << j);
}

/*==========================================
 * スタックから値を取り出す
 *------------------------------------------
 */
static int pop_val (struct script_state *st)
{
    if (st->stack->sp <= 0)
        return 0;
    st->stack->sp--;
    get_val (st, &(st->stack->stack_data[st->stack->sp]));
    if (st->stack->stack_data[st->stack->sp].type == C_INT)
        return st->stack->stack_data[st->stack->sp].u.num;
    return 0;
}

#define isstr(c) ((c).type==C_STR || (c).type==C_CONSTSTR)

/*==========================================
 * 加算演算子
 *------------------------------------------
 */
static void op_add (struct script_state *st)
{
    st->stack->sp--;
    get_val (st, &(st->stack->stack_data[st->stack->sp]));
    get_val (st, &(st->stack->stack_data[st->stack->sp - 1]));

    if (isstr (st->stack->stack_data[st->stack->sp])
        || isstr (st->stack->stack_data[st->stack->sp - 1]))
    {
        conv_str (st, &(st->stack->stack_data[st->stack->sp]));
        conv_str (st, &(st->stack->stack_data[st->stack->sp - 1]));
    }
    if (st->stack->stack_data[st->stack->sp].type == C_INT)
    {                           // ii
        st->stack->stack_data[st->stack->sp - 1].u.num +=
            st->stack->stack_data[st->stack->sp].u.num;
    }
    else
    {                           // ssの予定
        char *buf;
        buf = (char *)
            calloc (strlen (st->stack->stack_data[st->stack->sp - 1].u.str) +
                    strlen (st->stack->stack_data[st->stack->sp].u.str) + 1,
                    1);
        strcpy (buf, st->stack->stack_data[st->stack->sp - 1].u.str);
        strcat (buf, st->stack->stack_data[st->stack->sp].u.str);
        if (st->stack->stack_data[st->stack->sp - 1].type == C_STR)
            free ((char *)st->stack->stack_data[st->stack->sp - 1].u.str);
        if (st->stack->stack_data[st->stack->sp].type == C_STR)
            free ((char *)st->stack->stack_data[st->stack->sp].u.str);
        st->stack->stack_data[st->stack->sp - 1].type = C_STR;
        st->stack->stack_data[st->stack->sp - 1].u.str = buf;
    }
}

/*==========================================
 * 二項演算子(文字列)
 *------------------------------------------
 */
static void op_2str (struct script_state *st, int op, int sp1, int sp2)
{
    const char *s1 = st->stack->stack_data[sp1].u.str,
        *s2 = st->stack->stack_data[sp2].u.str;
    int  a = 0;

    switch (op)
    {
        case C_EQ:
            a = (strcmp (s1, s2) == 0);
            break;
        case C_NE:
            a = (strcmp (s1, s2) != 0);
            break;
        case C_GT:
            a = (strcmp (s1, s2) > 0);
            break;
        case C_GE:
            a = (strcmp (s1, s2) >= 0);
            break;
        case C_LT:
            a = (strcmp (s1, s2) < 0);
            break;
        case C_LE:
            a = (strcmp (s1, s2) <= 0);
            break;
        default:
            printf ("illegal string operater\n");
            break;
    }

    push_val (st->stack, C_INT, a);

    if (st->stack->stack_data[sp1].type == C_STR)
        free ((char *)s1);
    if (st->stack->stack_data[sp2].type == C_STR)
        free ((char *)s2);
}

/*==========================================
 * 二項演算子(数値)
 *------------------------------------------
 */
static void op_2num (struct script_state *st, int op, int i1, int i2)
{
    switch (op)
    {
        case C_SUB:
            i1 -= i2;
            break;
        case C_MUL:
            i1 *= i2;
            break;
        case C_DIV:
            i1 /= i2;
            break;
        case C_MOD:
            i1 %= i2;
            break;
        case C_AND:
            i1 &= i2;
            break;
        case C_OR:
            i1 |= i2;
            break;
        case C_XOR:
            i1 ^= i2;
            break;
        case C_LAND:
            i1 = i1 && i2;
            break;
        case C_LOR:
            i1 = i1 || i2;
            break;
        case C_EQ:
            i1 = i1 == i2;
            break;
        case C_NE:
            i1 = i1 != i2;
            break;
        case C_GT:
            i1 = i1 > i2;
            break;
        case C_GE:
            i1 = i1 >= i2;
            break;
        case C_LT:
            i1 = i1 < i2;
            break;
        case C_LE:
            i1 = i1 <= i2;
            break;
        case C_R_SHIFT:
            i1 = i1 >> i2;
            break;
        case C_L_SHIFT:
            i1 = i1 << i2;
            break;
    }
    push_val (st->stack, C_INT, i1);
}

/*==========================================
 * 二項演算子
 *------------------------------------------
 */
static void op_2 (struct script_state *st, int op)
{
    int  i1, i2;
    const char *s1 = NULL, *s2 = NULL;

    i2 = pop_val (st);
    if (isstr (st->stack->stack_data[st->stack->sp]))
        s2 = st->stack->stack_data[st->stack->sp].u.str;

    i1 = pop_val (st);
    if (isstr (st->stack->stack_data[st->stack->sp]))
        s1 = st->stack->stack_data[st->stack->sp].u.str;

    if (s1 != NULL && s2 != NULL)
    {
        // ss => op_2str
        op_2str (st, op, st->stack->sp, st->stack->sp + 1);
    }
    else if (s1 == NULL && s2 == NULL)
    {
        // ii => op_2num
        op_2num (st, op, i1, i2);
    }
    else
    {
        // si,is => error
        printf ("script: op_2: int&str, str&int not allow.");
        push_val (st->stack, C_INT, 0);
    }
}

/*==========================================
 * 単項演算子
 *------------------------------------------
 */
static void op_1num (struct script_state *st, int op)
{
    int  i1;
    i1 = pop_val (st);
    switch (op)
    {
        case C_NEG:
            i1 = -i1;
            break;
        case C_NOT:
            i1 = ~i1;
            break;
        case C_LNOT:
            i1 = !i1;
            break;
    }
    push_val (st->stack, C_INT, i1);
}

/*==========================================
 * 関数の実行
 *------------------------------------------
 */
int run_func (struct script_state *st)
{
    int io, start_sp, end_sp, func;

    end_sp = st->stack->sp;
    for (io = end_sp - 1; io >= 0 && st->stack->stack_data[io].type != C_ARG;
         io--);
    if (io == 0)
    {
        if (battle_config.error_log)
            printf ("function not found\n");
//      st->stack->sp=0;
        st->state = END;
        return 0;
    }
    start_sp = io - 1;
    st->start = io - 1;
    st->end = end_sp;

    func = st->stack->stack_data[st->start].u.num;
    if (st->stack->stack_data[st->start].type != C_NAME
        || str_data[func].type != C_FUNC)
    {
        printf ("run_func: not function and command! \n");
//      st->stack->sp=0;
        st->state = END;
        return 0;
    }
    if (str_data[func].func)
    {
        str_data[func].func (st);
    }
    else
    {
        if (battle_config.error_log)
            printf ("run_func : %s? (%d(%d))\n", str_buf + str_data[func].str,
                    func, str_data[func].type);
        push_val (st->stack, C_INT, 0);
    }

    pop_stack (st->stack, start_sp, end_sp);

    if (st->state == RETFUNC)
    {
        // ユーザー定義関数からの復帰
        int  olddefsp = st->defsp;
        int  ii;

        pop_stack (st->stack, st->defsp, start_sp); // 復帰に邪魔なスタック削除
        if (st->defsp < 4
            || st->stack->stack_data[st->defsp - 1].type != C_RETINFO)
        {
            printf
                ("script:run_func(return) return without callfunc or callsub!\n");
            st->state = END;
            return 0;
        }
        ii = conv_num (st, &(st->stack->stack_data[st->defsp - 4])); // 引数の数所得
        st->pos = conv_num (st, &(st->stack->stack_data[st->defsp - 1]));   // スクリプト位置の復元
        int tmp = conv_num (st, &(st->stack->stack_data[st->defsp - 2]));   // スクリプトを復元
        st->script = (script_ptr) tmp;
        st->defsp = conv_num (st, &(st->stack->stack_data[st->defsp - 3])); // 基準スタックポインタを復元

        pop_stack (st->stack, olddefsp - 4 - ii, olddefsp);  // 要らなくなったスタック(引数と復帰用データ)削除

        st->state = GOTO;
    }

    return 0;
}

/*==========================================
 * スクリプトの実行メイン部分
 *------------------------------------------
 */
static int run_script_main (script_ptr script, int pos, int, int,
                     struct script_state *st, script_ptr rootscript)
{
    int  c, rerun_pos;
    int  cmdcount = script_config.check_cmdcount;
    int  gotocount = script_config.check_gotocount;
    struct script_stack *stack = st->stack;

    st->defsp = stack->sp;
    st->script = script;

    rerun_pos = st->pos;
    for (st->state = 0; st->state == 0;)
    {
        switch (c = get_com (script, &st->pos))
        {
            case C_EOL:
                if (stack->sp != st->defsp)
                {
                    if (battle_config.error_log)
                        printf ("stack.sp(%d) != default(%d)\n", stack->sp,
                                st->defsp);
                    stack->sp = st->defsp;
                }
                rerun_pos = st->pos;
                break;
            case C_INT:
                push_val (stack, C_INT, get_num (script, &st->pos));
                break;
            case C_POS:
            case C_NAME:
                push_val (stack, c, (*(int *) (script + st->pos)) & 0xffffff);
                st->pos += 3;
                break;
            case C_ARG:
                push_val (stack, c, 0);
                break;
            case C_STR:
                push_str (stack, C_CONSTSTR, (char *)script + st->pos);
                while (script[st->pos++]);
                break;
            case C_FUNC:
                run_func (st);
                if (st->state == GOTO)
                {
                    rerun_pos = st->pos;
                    script = st->script;
                    st->state = 0;
                    if (gotocount > 0 && (--gotocount) <= 0)
                    {
                        printf ("run_script: infinity loop !\n");
                        st->state = END;
                    }
                }
                break;

            case C_ADD:
                op_add (st);
                break;

            case C_SUB:
            case C_MUL:
            case C_DIV:
            case C_MOD:
            case C_EQ:
            case C_NE:
            case C_GT:
            case C_GE:
            case C_LT:
            case C_LE:
            case C_AND:
            case C_OR:
            case C_XOR:
            case C_LAND:
            case C_LOR:
            case C_R_SHIFT:
            case C_L_SHIFT:
                op_2 (st, c);
                break;

            case C_NEG:
            case C_NOT:
            case C_LNOT:
                op_1num (st, c);
                break;

            case C_NOP:
                st->state = END;
                break;

            default:
                if (battle_config.error_log)
                    printf ("unknown command : %d @ %d\n", c, pos);
                st->state = END;
                break;
        }
        if (cmdcount > 0 && (--cmdcount) <= 0)
        {
            printf ("run_script: infinity loop !\n");
            st->state = END;
        }
    }
    switch (st->state)
    {
        case STOP:
            break;
        case END:
        {
            struct map_session_data *sd = map_id2sd (st->rid);
            st->pos = -1;
            if (sd && sd->npc_id == st->oid)
                npc_event_dequeue (sd);
        }
            break;
        case RERUNLINE:
        {
            st->pos = rerun_pos;
        }
            break;
    }

    if (st->state != END)
    {
        // 再開するためにスタック情報を保存
        struct map_session_data *sd = map_id2sd (st->rid);
        if (sd /* && sd->npc_stackbuf==NULL */ )
        {
            if (sd->npc_stackbuf)
                free (sd->npc_stackbuf);
            sd->npc_stackbuf = (char *)
                calloc (sizeof (stack->stack_data[0]) * stack->sp_max, 1);
            memcpy (sd->npc_stackbuf, stack->stack_data,
                    sizeof (stack->stack_data[0]) * stack->sp_max);
            sd->npc_stack = stack->sp;
            sd->npc_stackmax = stack->sp_max;
            sd->npc_script = script;
            sd->npc_scriptroot = rootscript;
        }
    }

    return 0;
}

/*==========================================
 * スクリプトの実行
 *------------------------------------------
 */
int run_script (unsigned char *script, int pos, int rid, int oid)
{
    return run_script_l (script, pos, rid, oid, 0, NULL);
}

int run_script_l (script_ptr script, int pos, int rid, int oid,
                  int args_nr, argrec_t * args)
{
    struct script_stack stack;
    struct script_state st;
    struct map_session_data *sd = map_id2sd (rid);
    script_ptr rootscript = script;
    int  i;
    if (script == NULL || pos < 0)
        return -1;

    if (sd && sd->npc_stackbuf && sd->npc_scriptroot == rootscript)
    {
        // 前回のスタックを復帰
        script = sd->npc_script;
        stack.sp = sd->npc_stack;
        stack.sp_max = sd->npc_stackmax;
        stack.stack_data = (struct script_data *)
            calloc (stack.sp_max, sizeof (stack.stack_data[0]));
        memcpy (stack.stack_data, sd->npc_stackbuf,
                sizeof (stack.stack_data[0]) * stack.sp_max);
        free (sd->npc_stackbuf);
        sd->npc_stackbuf = NULL;
    }
    else
    {
        // スタック初期化
        stack.sp = 0;
        stack.sp_max = 64;
        stack.stack_data = (struct script_data *)
            calloc (stack.sp_max, sizeof (stack.stack_data[0]));
    }
    st.stack = &stack;
    st.pos = pos;
    st.rid = rid;
    st.oid = oid;
    for (i = 0; i < args_nr; i++)
    {
        if (args[i].name[strlen (args[i].name) - 1] == '$')
            pc_setregstr (sd, add_str (args[i].name), args[i].v.s);
        else
            pc_setreg (sd, add_str (args[i].name), args[i].v.i);
    }
    run_script_main (script, pos, rid, oid, &st, rootscript);

    free (stack.stack_data);
    stack.stack_data = NULL;
    return st.pos;
}

/*==========================================
 * マップ変数の変更
 *------------------------------------------
 */
int mapreg_setreg (int num, int val)
{
    if (val != 0)
        numdb_insert (mapreg_db, num, val);
    else
        numdb_erase (mapreg_db, num);

    mapreg_dirty = 1;
    return 0;
}

/*==========================================
 * 文字列型マップ変数の変更
 *------------------------------------------
 */
int mapreg_setregstr (int num, const char *str)
{
    char *p;

    if ((p = (char *)numdb_search (mapregstr_db, num).p) != NULL)
        free (p);

    if (str == NULL || *str == 0)
    {
        numdb_erase (mapregstr_db, num);
        mapreg_dirty = 1;
        return 0;
    }
    p = (char *) calloc (strlen (str) + 1, 1);
    strcpy (p, str);
    numdb_insert (mapregstr_db, num, (void *)p);
    mapreg_dirty = 1;
    return 0;
}

/*==========================================
 * 永続的マップ変数の読み込み
 *------------------------------------------
 */
static int script_load_mapreg (void)
{
    FILE *fp;
    char line[1024];

    if ((fp = fopen_ (mapreg_txt, "rt")) == NULL)
        return -1;

    while (fgets (line, sizeof (line), fp))
    {
        char buf1[256], buf2[1024], *p;
        int  n, v, s, i;
        if (sscanf (line, "%255[^,],%d\t%n", buf1, &i, &n) != 2 &&
            (i = 0, sscanf (line, "%[^\t]\t%n", buf1, &n) != 1))
            continue;
        if (buf1[strlen (buf1) - 1] == '$')
        {
            if (sscanf (line + n, "%[^\n\r]", buf2) != 1)
            {
                printf ("%s: %s broken data !\n", mapreg_txt, buf1);
                continue;
            }
            p = (char *) calloc (strlen (buf2) + 1, 1);
            strcpy (p, buf2);
            s = add_str (buf1);
            numdb_insert (mapregstr_db, (i << 24) | s, (void *)p);
        }
        else
        {
            if (sscanf (line + n, "%d", &v) != 1)
            {
                printf ("%s: %s broken data !\n", mapreg_txt, buf1);
                continue;
            }
            s = add_str (buf1);
            numdb_insert (mapreg_db, (i << 24) | s, v);
        }
    }
    fclose_ (fp);
    mapreg_dirty = 0;
    return 0;
}

/*==========================================
 * 永続的マップ変数の書き込み
 *------------------------------------------
 */
static void script_save_mapreg_intsub (db_key_t key, db_val_t data, va_list ap)
{
    FILE *fp = va_arg (ap, FILE *);
    int  num = key.i & 0x00ffffff, i = key.i >> 24;
    const char *name = str_buf + str_data[num].str;
    if (name[1] != '@')
    {
        if (i == 0)
            fprintf (fp, "%s\t%d\n", name, (int) data.i);
        else
            fprintf (fp, "%s,%d\t%d\n", name, i, (int) data.i);
    }
}

static void script_save_mapreg_strsub (db_key_t key, db_val_t data, va_list ap)
{
    FILE *fp = va_arg (ap, FILE *);
    int  num = key.i & 0x00ffffff, i = key.i >> 24;
    char *name = str_buf + str_data[num].str;
    if (name[1] != '@')
    {
        if (i == 0)
            fprintf (fp, "%s\t%s\n", name, (char *) data.p);
        else
            fprintf (fp, "%s,%d\t%s\n", name, i, (char *) data.p);
    }
}

static int script_save_mapreg (void)
{
    FILE *fp;
    int  lock;

    if ((fp = lock_fopen (mapreg_txt, &lock)) == NULL)
        return -1;
    numdb_foreach (mapreg_db, script_save_mapreg_intsub, fp);
    numdb_foreach (mapregstr_db, script_save_mapreg_strsub, fp);
    lock_fclose (fp, mapreg_txt, &lock);
    mapreg_dirty = 0;
    return 0;
}

static void script_autosave_mapreg (timer_id, tick_t, custom_id_t,
                                    custom_data_t)
{
    if (mapreg_dirty)
        script_save_mapreg ();
}

/*==========================================
 *
 *------------------------------------------
 */
static int set_posword (char *p)
{
    char *np, *str[15];
    int  i = 0;
    for (i = 0; i < 11; i++)
    {
        if ((np = strchr (p, ',')) != NULL)
        {
            str[i] = p;
            *np = 0;
            p = np + 1;
        }
        else
        {
            str[i] = p;
            p += strlen (p);
        }
        if (str[i])
            strcpy (epos[i], str[i]);
    }
    return 0;
}

int script_config_read (const char *cfgName)
{
    int  i;
    char line[1024], w1[1024], w2[1024];
    FILE *fp;

    script_config.warn_func_no_comma = 1;
    script_config.warn_cmd_no_comma = 1;
    script_config.warn_func_mismatch_paramnum = 1;
    script_config.warn_cmd_mismatch_paramnum = 1;
    script_config.check_cmdcount = 8192;
    script_config.check_gotocount = 512;

    fp = fopen_ (cfgName, "r");
    if (fp == NULL)
    {
        printf ("file not found: %s\n", cfgName);
        return 1;
    }
    while (fgets (line, 1020, fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;
        i = sscanf (line, "%[^:]: %[^\r\n]", w1, w2);
        if (i != 2)
            continue;
        if (strcasecmp (w1, "refine_posword") == 0)
        {
            set_posword (w2);
        }
        if (strcasecmp (w1, "import") == 0)
        {
            script_config_read (w2);
        }
    }
    fclose_ (fp);

    return 0;
}

/*==========================================
 * 終了
 *------------------------------------------
 */

static void mapregstr_db_final (db_key_t, db_val_t data, va_list)
{
    free (data.p);
}

static void userfunc_db_final (db_key_t key, db_val_t data, va_list)
{
    free ((char*)key.s);
    free (data.p);
}

int do_final_script (void)
{
    if (mapreg_dirty >= 0)
        script_save_mapreg ();
    if (script_buf)
        free (script_buf);

    if (mapreg_db)
        numdb_final (mapreg_db, NULL);
    if (mapregstr_db)
        strdb_final (mapregstr_db, mapregstr_db_final);
    if (scriptlabel_db)
        strdb_final (scriptlabel_db, NULL);
    if (userfunc_db)
        strdb_final (userfunc_db, userfunc_db_final);

    if (str_data)
        free (str_data);
    if (str_buf)
        free (str_buf);

    return 0;
}

/*==========================================
 * 初期化
 *------------------------------------------
 */
int do_init_script (void)
{
    mapreg_db = numdb_init ();
    mapregstr_db = numdb_init ();
    script_load_mapreg ();

    add_timer_interval (gettick () + MAPREG_AUTOSAVE_INTERVAL,
                        script_autosave_mapreg, 0, 0,
                        MAPREG_AUTOSAVE_INTERVAL);

    scriptlabel_db = strdb_init ();
    return 0;
}
