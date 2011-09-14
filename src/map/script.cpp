#include "script.hpp"

#include <cmath>

#include "../common/lock.hpp"
#include "../common/mt_rand.hpp"
#include "../common/timer.hpp"
#include "../common/utils.hpp"

#include "atcommand.hpp"
#include "battle.hpp"
#include "chrif.hpp"
#include "clif.hpp"
#include "itemdb.hpp"
#include "magic-base.hpp"
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
static char *script_buf;
static int32_t script_pos, script_size;

char *str_buf;
int32_t str_pos, str_size;
static struct str_data_t
{
    int32_t type;
    int32_t str;
    int32_t backpatch;
    int32_t label;
    void (*func)(struct script_state *);
    int32_t val;
    int32_t next;
}   *str_data;
int32_t str_num = LABEL_START, str_data_size;
int32_t str_hash[16];

static struct dbt *mapreg_db = NULL;
static struct dbt *mapregstr_db = NULL;
static int32_t mapreg_dirty = -1;
char mapreg_txt[256] = "save/mapreg.txt";
#define MAPREG_AUTOSAVE_INTERVAL        (10*1000)

static struct dbt *scriptlabel_db = NULL;
static struct dbt *userfunc_db = NULL;

struct dbt *script_get_label_db(void)
{
    return scriptlabel_db;
}

struct dbt *script_get_userfunc_db(void)
{
    if (!userfunc_db)
        userfunc_db = strdb_init();
    return userfunc_db;
}


// scripts use a different set of enum constants (ugh)
// TODO change this to use the main enum class EQUIP
enum class EQ_SCR
{
    none,

    head, shield, hand2, hand1, gloves,
    shoes, misc1, misc2, torso, legs,

    count
};

constexpr earray<EPOS, EQ_SCR, EQ_SCR::count> equip =
{
    EPOS::NONE,

    EPOS::HELMET, EPOS::MISC1, EPOS::SHIELD, EPOS::WEAPON, EPOS::GLOVES,
    EPOS::SHOES, EPOS::CAPE, EPOS::MISC2, EPOS::CHEST, EPOS::LEGS,
};

constexpr earray<const char *, EQ_SCR, EQ_SCR::count> epos =
{
    "Not Equipped",

    "Head", "Shield", "Left hand", "Right hand", "Gloves",
    "Shoes", "Accessory 1", "Accessory 2", "Torso", "Legs"
};

static struct Script_Config
{
    int32_t warn_func_no_comma;
    int32_t warn_cmd_no_comma;
    int32_t warn_func_mismatch_paramnum;
    int32_t warn_cmd_mismatch_paramnum;
    int32_t check_cmdcount;
    int32_t check_gotocount;
} script_config;
static int32_t parse_cmd_if = 0;
static int32_t parse_cmd;

#define BUILTIN(name) static void builtin_ ## name (struct script_state *)
BUILTIN(mes);
BUILTIN(goto);
BUILTIN(callsub);
BUILTIN(callfunc);
BUILTIN(return);
BUILTIN(getarg);
BUILTIN(next);
BUILTIN(close);
BUILTIN(close2);
BUILTIN(menu);
BUILTIN(rand);
BUILTIN(pow);
BUILTIN(warp);
BUILTIN(isat);
BUILTIN(areawarp);
BUILTIN(heal);
BUILTIN(itemheal);
BUILTIN(percentheal);
BUILTIN(input);
BUILTIN(setlook);
BUILTIN(set);
BUILTIN(setarray);
BUILTIN(cleararray);
BUILTIN(copyarray);
BUILTIN(getarraysize);
BUILTIN(deletearray);
BUILTIN(getelementofarray);
BUILTIN(if);
BUILTIN(getitem);
BUILTIN(makeitem);
BUILTIN(delitem);
BUILTIN(countitem);
BUILTIN(checkweight);
BUILTIN(readparam);
BUILTIN(getcharid);
BUILTIN(getpartyname);
BUILTIN(getpartymember);
BUILTIN(strcharinfo);
BUILTIN(getequipid);
BUILTIN(getequipname);
BUILTIN(getequipisequiped);
BUILTIN(statusup);
BUILTIN(statusup2);
BUILTIN(bonus);
BUILTIN(skill);
BUILTIN(setskill);
BUILTIN(getskilllv);
BUILTIN(getgmlevel);
BUILTIN(end);
BUILTIN(getopt2);
BUILTIN(setopt2);
BUILTIN(checkoption);
BUILTIN(setoption);
BUILTIN(savepoint);
BUILTIN(gettimetick);
BUILTIN(gettime);
BUILTIN(gettimestr) __attribute__((deprecated));
BUILTIN(openstorage);
BUILTIN(monster);
BUILTIN(areamonster);
BUILTIN(killmonster);
BUILTIN(killmonsterall);
BUILTIN(doevent);
BUILTIN(donpcevent);
BUILTIN(addtimer);
BUILTIN(deltimer);
BUILTIN(initnpctimer);
BUILTIN(stopnpctimer);
BUILTIN(startnpctimer);
BUILTIN(setnpctimer);
BUILTIN(getnpctimer);
BUILTIN(announce);
BUILTIN(mapannounce);
BUILTIN(areaannounce);
BUILTIN(getusers);
BUILTIN(getmapusers);
BUILTIN(getareausers);
BUILTIN(getareadropitem);
BUILTIN(enablenpc);
BUILTIN(disablenpc);
BUILTIN(hideoffnpc);
BUILTIN(hideonnpc);
BUILTIN(sc_start);
BUILTIN(sc_start2);
BUILTIN(sc_end);
BUILTIN(sc_check);
BUILTIN(getscrate);
BUILTIN(debugmes);
BUILTIN(resetlvl);
BUILTIN(resetstatus);
BUILTIN(resetskill);
BUILTIN(changesex);
BUILTIN(attachrid);
BUILTIN(detachrid);
BUILTIN(isloggedin);
BUILTIN(setmapflagnosave);
BUILTIN(setmapflag);
BUILTIN(removemapflag);
BUILTIN(pvpon);
BUILTIN(pvpoff);
BUILTIN(emotion);
BUILTIN(marriage);
BUILTIN(divorce);
BUILTIN(getitemname);
BUILTIN(getspellinvocation);
BUILTIN(getanchorinvocation);
BUILTIN(getexp);
BUILTIN(getinventorylist);
BUILTIN(getskilllist);
BUILTIN(getpoolskilllist);
BUILTIN(getactivatedpoolskilllist);
BUILTIN(getunactivatedpoolskilllist);
BUILTIN(poolskill);
BUILTIN(unpoolskill);
BUILTIN(checkpoolskill);
BUILTIN(clearitem);
BUILTIN(misceffect);
BUILTIN(soundeffect);
BUILTIN(mapwarp);
BUILTIN(inittimer);
BUILTIN(stoptimer);
BUILTIN(cmdothernpc);
BUILTIN(mobcount);
BUILTIN(strmobinfo);
BUILTIN(specialeffect);
BUILTIN(specialeffect2);
BUILTIN(nude);
BUILTIN(gmcommand);
BUILTIN(movenpc);
BUILTIN(npcwarp);
BUILTIN(message);
BUILTIN(npctalk);
BUILTIN(hasitems);
BUILTIN(getlook);
BUILTIN(getsavepoint);
BUILTIN(getpartnerid2);
BUILTIN(areatimer);
BUILTIN(isin);
BUILTIN(shop);
BUILTIN(isdead);
BUILTIN(fakenpcname);
BUILTIN(unequipbyid);
BUILTIN(getx);
BUILTIN(gety);

static void push_val(struct script_stack *stack, int32_t type, int32_t val);
static int32_t run_func(struct script_state *st);

static int32_t mapreg_setreg(int32_t num, int32_t val);
static int32_t mapreg_setregstr(int32_t num, const char *str);

static struct builtin_function_t
{
    void (*func)(struct script_state *);
    const char *name;
    const char *arg;
} builtin_functions[] =
{
#define BUILTIN_ARGS(name, args) {builtin_##name, #name, args}
    BUILTIN_ARGS(mes, "s"),
    BUILTIN_ARGS(next, ""),
    BUILTIN_ARGS(close, ""),
    BUILTIN_ARGS(close2, ""),
    BUILTIN_ARGS(menu, "sL*"),
    BUILTIN_ARGS(goto, "L"),
    BUILTIN_ARGS(callsub, "L*"),
    BUILTIN_ARGS(callfunc, "F*"),
    BUILTIN_ARGS(return, "*"),
    BUILTIN_ARGS(getarg, "i"),
    BUILTIN_ARGS(input, "N"),
    BUILTIN_ARGS(warp, "Mxy"),
    BUILTIN_ARGS(isat, "Mxy"),
    BUILTIN_ARGS(areawarp, "MxyxyMxy"),
    BUILTIN_ARGS(setlook, "ii"),
    BUILTIN_ARGS(set, "Ne"),
    BUILTIN_ARGS(setarray, "Ne*"),
    BUILTIN_ARGS(cleararray, "Nei"),
    BUILTIN_ARGS(copyarray, "NNi"),
    BUILTIN_ARGS(getarraysize, "N"),
    BUILTIN_ARGS(deletearray, "N*"),
    BUILTIN_ARGS(getelementofarray, "Ni"),
    BUILTIN_ARGS(if, "iF*"),
    BUILTIN_ARGS(getitem, "Ii**"),
    BUILTIN_ARGS(makeitem, "IiMxy"),
    BUILTIN_ARGS(delitem, "Ii"),
    BUILTIN_ARGS(heal, "ii"),
    BUILTIN_ARGS(itemheal, "ii"),
    BUILTIN_ARGS(percentheal, "ii"),
    BUILTIN_ARGS(rand, "i*"),
    BUILTIN_ARGS(pow, "ii"),
    BUILTIN_ARGS(countitem, "I"),
    BUILTIN_ARGS(checkweight, "Ii"),
    BUILTIN_ARGS(readparam, "i*"),
    BUILTIN_ARGS(getcharid, "i*"),
    BUILTIN_ARGS(getpartyname, "i"),
    BUILTIN_ARGS(getpartymember, "i"),
    BUILTIN_ARGS(strcharinfo, "i"),
    BUILTIN_ARGS(getequipid, "i"),
    BUILTIN_ARGS(getequipname, "i"),
    BUILTIN_ARGS(getequipisequiped, "i"),
    BUILTIN_ARGS(statusup, "i"),
    BUILTIN_ARGS(statusup2, "ii"),
    BUILTIN_ARGS(bonus, "ii"),
    BUILTIN_ARGS(skill, "ii*"),
    BUILTIN_ARGS(setskill, "ii"),
    BUILTIN_ARGS(getskilllv, "i"),
    BUILTIN_ARGS(getgmlevel, ""),
    BUILTIN_ARGS(end, ""),
    BUILTIN_ARGS(getopt2, ""),
    BUILTIN_ARGS(setopt2, "i"),
    // used quite a bit, in _mobs.txt and monsters.txt
    {builtin_end, "break", ""},
    BUILTIN_ARGS(checkoption, "i"),
    BUILTIN_ARGS(setoption, "i"),
    // believed unused
    {builtin_savepoint, "save", "sii"},
    BUILTIN_ARGS(savepoint, "Mxy"),
    BUILTIN_ARGS(gettimetick, "i"),
    BUILTIN_ARGS(gettime, "i"),
    BUILTIN_ARGS(gettimestr, "si"),
    BUILTIN_ARGS(openstorage, "*"),
    BUILTIN_ARGS(monster, "Mxysmi*"),
    BUILTIN_ARGS(areamonster, "Mxyxysmi*"),
    BUILTIN_ARGS(killmonster, "ME"),
    BUILTIN_ARGS(killmonsterall, "M"),
    BUILTIN_ARGS(doevent, "E"),
    BUILTIN_ARGS(donpcevent, "E"),
    BUILTIN_ARGS(addtimer, "tE"),
    BUILTIN_ARGS(deltimer, "E"),
    BUILTIN_ARGS(initnpctimer, ""),
    BUILTIN_ARGS(stopnpctimer, ""),
    BUILTIN_ARGS(startnpctimer, ""),
    BUILTIN_ARGS(setnpctimer, "i"),
    BUILTIN_ARGS(getnpctimer, "i"),
    BUILTIN_ARGS(announce, "si"),
    BUILTIN_ARGS(mapannounce, "Msi"),
    BUILTIN_ARGS(areaannounce, "Mxyxysi"),
    BUILTIN_ARGS(getusers, "i"),
    BUILTIN_ARGS(getmapusers, "M"),
    BUILTIN_ARGS(getareausers, "Mxyxy"),
    BUILTIN_ARGS(getareadropitem, "Mxyxyi*"),
    BUILTIN_ARGS(enablenpc, "s"),
    BUILTIN_ARGS(disablenpc, "s"),
    BUILTIN_ARGS(hideoffnpc, "s"),
    BUILTIN_ARGS(hideonnpc, "s"),
    BUILTIN_ARGS(sc_start, "iTi*"),
    BUILTIN_ARGS(sc_start2, "iTii*"),
    BUILTIN_ARGS(sc_end, "i"),
    BUILTIN_ARGS(sc_check, "i"),
    BUILTIN_ARGS(getscrate, "ii*"),
    BUILTIN_ARGS(debugmes, "s"),
    BUILTIN_ARGS(resetlvl, "i"),
    BUILTIN_ARGS(resetstatus, ""),
    BUILTIN_ARGS(resetskill, ""),
    BUILTIN_ARGS(changesex, ""),
    BUILTIN_ARGS(attachrid, "i"),
    BUILTIN_ARGS(detachrid, ""),
    BUILTIN_ARGS(isloggedin, "i"),
    BUILTIN_ARGS(setmapflagnosave, "MMxy"),
    BUILTIN_ARGS(setmapflag, "Mi"),
    BUILTIN_ARGS(removemapflag, "Mi"),
    BUILTIN_ARGS(pvpon, "M"),
    BUILTIN_ARGS(pvpoff, "M"),
    BUILTIN_ARGS(emotion, "i"),
    BUILTIN_ARGS(marriage, "P"),
    BUILTIN_ARGS(divorce, ""),
    BUILTIN_ARGS(getitemname, "I"),
    BUILTIN_ARGS(getspellinvocation, "s"),
    BUILTIN_ARGS(getanchorinvocation, "s"),
    BUILTIN_ARGS(getpartnerid2, ""),
    BUILTIN_ARGS(getexp, "ii"),
    BUILTIN_ARGS(getinventorylist, ""),
    BUILTIN_ARGS(getskilllist, ""),
    BUILTIN_ARGS(getpoolskilllist, ""),
    BUILTIN_ARGS(getactivatedpoolskilllist, ""),
    BUILTIN_ARGS(getunactivatedpoolskilllist, ""),
    BUILTIN_ARGS(poolskill, "i"),
    BUILTIN_ARGS(unpoolskill, "i"),
    BUILTIN_ARGS(checkpoolskill, "i"),
    BUILTIN_ARGS(clearitem, ""),
    BUILTIN_ARGS(misceffect, "i*"),
    BUILTIN_ARGS(soundeffect, "si"),
    BUILTIN_ARGS(strmobinfo, "im"),
    BUILTIN_ARGS(specialeffect, "i"),
    BUILTIN_ARGS(specialeffect2, "i"),
    BUILTIN_ARGS(nude, ""),
    BUILTIN_ARGS(mapwarp, "MMxy"),
    BUILTIN_ARGS(inittimer, ""),
    BUILTIN_ARGS(stoptimer, ""),
    BUILTIN_ARGS(cmdothernpc, "ss"),
    BUILTIN_ARGS(gmcommand, "s"),
    BUILTIN_ARGS(movenpc, "siis"),
    BUILTIN_ARGS(npcwarp, "xys"),
    BUILTIN_ARGS(message, "Ps"),
    BUILTIN_ARGS(npctalk, "s"),
    BUILTIN_ARGS(hasitems, ""),
    BUILTIN_ARGS(mobcount, "ME"),
    BUILTIN_ARGS(getlook, "i"),
    BUILTIN_ARGS(getsavepoint, "i"),
    BUILTIN_ARGS(areatimer, "MxyxytE"),
    BUILTIN_ARGS(isin, "Mxyxy"),
    BUILTIN_ARGS(shop, "s"),
    BUILTIN_ARGS(isdead, ""),
    BUILTIN_ARGS(fakenpcname, "ssi"),
    BUILTIN_ARGS(unequipbyid, "i"),
    BUILTIN_ARGS(getx, ""),
    BUILTIN_ARGS(gety, ""),
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
static int32_t calc_hash(const uint8_t *p) __attribute__((pure));
static int32_t calc_hash(const uint8_t *p)
{
    int32_t h = 0;
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
static int32_t search_str(const char *p) __attribute__((pure));
static int32_t search_str(const char *p)
{
    int32_t i;
    i = str_hash[calc_hash(sign_cast<const uint8_t *>(p))];
    while (i)
    {
        if (strcmp(str_buf + str_data[i].str, p) == 0)
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
static int32_t add_str(const char *p, size_t len)
{
    char *lowcase = static_cast<char *>(malloc(len + 1));
    memcpy(lowcase, p, len);
    lowcase[len] = '\0';

    // not really lower case anymore
/*
    for (int32_t i = 0; lowcase[i]; i++)
        lowcase[i] = tolower(lowcase[i]);
*/
    int32_t i = search_str(lowcase);
    if (i >= 0)
    {
        free(lowcase);
        return i;
    }

    p = lowcase;
    i = calc_hash(sign_cast<const uint8_t *>(p));
    if (str_hash[i] == 0)
    {
        str_hash[i] = str_num;
    }
    else
    {
        i = str_hash[i];
        for (;;)
        {
            if (strcmp(str_buf + str_data[i].str, p) == 0)
            {
                free(lowcase);
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
        RECREATE(str_data, struct str_data_t, str_data_size);
        memset(str_data + (str_data_size - 128), '\0', 128);
    }
    while (str_pos + strlen(p) + 1 >= str_size)
    {
        str_size += 256;
        RECREATE(str_buf, char, str_size);
        memset(str_buf + (str_size - 256), '\0', 256);
    }
    strcpy(str_buf + str_pos, p);
    str_data[str_num].type = C_NOP;
    str_data[str_num].str = str_pos;
    str_data[str_num].next = 0;
    str_data[str_num].func = NULL;
    str_data[str_num].backpatch = -1;
    str_data[str_num].label = -1;
    str_pos += strlen(p) + 1;
    free(lowcase);
    return str_num++;
}
static int32_t add_str(const char *p)
{
    return add_str(p, strlen(p));
}

/*==========================================
 * スクリプトバッファサイズの確認と拡張
 *------------------------------------------
 */
static void check_script_buf(int32_t size)
{
    if (script_pos + size >= script_size)
    {
        script_size += SCRIPT_BLOCK_SIZE;
        RECREATE(script_buf, char, script_size);
        memset(script_buf + script_size - SCRIPT_BLOCK_SIZE, '\0',
                SCRIPT_BLOCK_SIZE);
    }
}

/*==========================================
 * スクリプトバッファに１バイト書き込む
 *------------------------------------------
 */
static void add_scriptb(int32_t a)
{
    check_script_buf(1);
    script_buf[script_pos++] = a;
}

/*==========================================
 * スクリプトバッファにデータタイプを書き込む
 *------------------------------------------
 */
static void add_scriptc(int32_t a)
{
    while (a >= 0x40)
    {
        add_scriptb((a & 0x3f) | 0x40);
        a = (a - 0x40) >> 6;
    }
    add_scriptb(a & 0x3f);
}

/*==========================================
 * スクリプトバッファに整数を書き込む
 *------------------------------------------
 */
static void add_scripti(int32_t a)
{
    while (a >= 0x40)
    {
        add_scriptb(a | 0xc0);
        a = (a - 0x40) >> 6;
    }
    add_scriptb(a | 0x80);
}

/*==========================================
 * スクリプトバッファにラベル/変数/関数を書き込む
 *------------------------------------------
 */
// 最大16Mまで
static void add_scriptl(int32_t l)
{
    int32_t backpatch = str_data[l].backpatch;

    switch (str_data[l].type)
    {
        case C_POS:
            add_scriptc(C_POS);
            add_scriptb(str_data[l].label);
            add_scriptb(str_data[l].label >> 8);
            add_scriptb(str_data[l].label >> 16);
            break;
        case C_NOP:
            // ラベルの可能性があるのでbackpatch用データ埋め込み
            add_scriptc(C_NAME);
            str_data[l].backpatch = script_pos;
            add_scriptb(backpatch);
            add_scriptb(backpatch >> 8);
            add_scriptb(backpatch >> 16);
            break;
        case C_INT:
            add_scripti(str_data[l].val);
            break;
        default:
            // もう他の用途と確定してるので数字をそのまま
            add_scriptc(C_NAME);
            add_scriptb(l);
            add_scriptb(l >> 8);
            add_scriptb(l >> 16);
            break;
    }
}

/*==========================================
 * ラベルを解決する
 *------------------------------------------
 */
static void set_label(int32_t l, int32_t pos)
{
    int32_t i, next;

    str_data[l].type = C_POS;
    str_data[l].label = pos;
    for (i = str_data[l].backpatch; i >= 0 && i != 0x00ffffff;)
    {
        next = (*reinterpret_cast<int32_t *>(script_buf + i)) & 0x00ffffff;
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
static const char *skip_space(const char *p) __attribute__((pure));
static const char *skip_space(const char *p)
{
    while (1)
    {
        while (isspace(*p))
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
static const char *skip_word(const char *p) __attribute__((pure));
static const char *skip_word(const char *p)
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
    // TODO remove 'l'
    if (*p == 'l')
        p++;                    // 一時的変数用(like weiss)

    while (isalnum(*p) || *p == '_' || static_cast<uint8_t>(*p) >= 0x81)
        if (static_cast<uint8_t>(*p) >= 0x81 && p[1])
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

static const char *startptr;
static int32_t startline;

/*==========================================
 * エラーメッセージ出力
 *------------------------------------------
 */
static void disp_error_message(const char *mes, const char *pos)
{
    int32_t line;
    const char *p;

    for (line = startline, p = startptr; p && *p; line++)
    {
        const char *linestart = p;
        const char *lineend = strchr(p, '\n');
        if (lineend == NULL || pos < lineend)
        {
            printf("%s line %d : ", mes, line);
            for (int32_t i = 0;
                 (linestart[i] != '\r') && (linestart[i] != '\n')
                 && linestart[i]; i++)
            {
                if (linestart + i != pos)
                    printf("%c", linestart[i]);
                else
                    printf("\'%c\'", linestart[i]);
            }
            printf("\a\n");
            return;
        }
        p = lineend + 1;
    }
}

static const char *parse_subexpr(const char *, int32_t);
static const char *parse_simpleexpr(const char *p)
{
    int32_t i;
    p = skip_space(p);

    if (*p == ';' || *p == ',')
    {
        disp_error_message("unexpected expr end", p);
        exit(1);
    }
    if (*p == '(')
    {

        p = parse_subexpr(p + 1, -1);
        p = skip_space(p);
        if ((*p++) != ')')
        {
            disp_error_message("unmatch ')'", p);
            exit(1);
        }
    }
    else if (isdigit(*p) || ((*p == '-' || *p == '+') && isdigit(p[1])))
    {
        char *np;
        i = strtoul(p, &np, 0);
        add_scripti(i);
        p = np;
    }
    else if (*p == '"')
    {
        add_scriptc(C_STR);
        p++;
        while (*p && *p != '"')
        {
            if (p[-1] <= 0x7e && *p == '\\')
                p++;
            else if (*p == '\n')
            {
                disp_error_message("unexpected newline @ string", p);
                exit(1);
            }
            add_scriptb(*p++);
        }
        if (!*p)
        {
            disp_error_message("unexpected eof @ string", p);
            exit(1);
        }
        add_scriptb(0);
        p++;                    //'"'
    }
    else
    {
        // label , register , function etc
        if (skip_word(p) == p)
        {
            disp_error_message("unexpected character", p);
            exit(1);
        }
        const char *p2 = skip_word(p);
        int32_t l = add_str(p, p2 - p);

        parse_cmd = l;          // warn_*_mismatch_paramnumのために必要
        if (l == search_str("if")) // warn_cmd_no_commaのために必要
            parse_cmd_if++;
        p = p2;

        if (str_data[l].type != C_FUNC && *p == '[')
        {
            // array(name[i] => getelementofarray(name,i) )
            add_scriptl(search_str("getelementofarray"));
            add_scriptc(C_ARG);
            add_scriptl(l);
            p = parse_subexpr(p + 1, -1);
            p = skip_space(p);
            if ((*p++) != ']')
            {
                disp_error_message("unmatch ']'", p);
                exit(1);
            }
            add_scriptc(C_FUNC);
        }
        else
            add_scriptl(l);

    }

    return p;
}

/*==========================================
 * 式の解析
 *------------------------------------------
 */
const char *parse_subexpr(const char *p, int32_t limit)
{
    int32_t op, opl, len;

    p = skip_space(p);

    if (*p == '-')
    {
        const char *tmpp = skip_space(p + 1);
        if (*tmpp == ';' || *tmpp == ',')
        {
            add_scriptl(LABEL_NEXTLINE);
            p++;
            return p;
        }
    }
    const char *tmpp = p;
    if ((op = C_NEG, *p == '-') || (op = C_LNOT, *p == '!')
        || (op = C_NOT, *p == '~'))
    {
        p = parse_subexpr(p + 1, 100);
        add_scriptc(op);
    }
    else
        p = parse_simpleexpr(p);
    p = skip_space(p);
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
            int32_t i = 0, func = parse_cmd;
            const char *plist[128];

            if (str_data[func].type != C_FUNC)
            {
                disp_error_message("expect function", tmpp);
                exit(0);
            }

            add_scriptc(C_ARG);
            while (*p && *p != ')' && i < 128)
            {
                plist[i] = p;
                p = parse_subexpr(p, -1);
                p = skip_space(p);
                if (*p == ',')
                    p++;
                else if (*p != ')' && script_config.warn_func_no_comma)
                {
                    disp_error_message("expect ',' or ')' at func params", p);
                }
                p = skip_space(p);
                i++;
            }
            plist[i] = p;
            if (*(p++) != ')')
            {
                disp_error_message("func request '(' ')'", p);
                exit(1);
            }

            if (str_data[func].type == C_FUNC
                && script_config.warn_func_mismatch_paramnum)
            {
                const char *arg = builtin_functions[str_data[func].val].arg;
                int32_t j = 0;
                for (j = 0; arg[j]; j++)
                    if (arg[j] == '*')
                        break;
                if ((arg[j] == 0 && i != j) || (arg[j] == '*' && i < j))
                {
                    disp_error_message("illegal number of parameters",
                                        plist[(i < j) ? i : j]);
                }
            }
        }
        else // not op == C_FUNC
        {
            p = parse_subexpr(p, opl);
        }
        add_scriptc(op);
        p = skip_space(p);
    }
    return p;                   /* return first untreated operator */
}

/*==========================================
 * 式の評価
 *------------------------------------------
 */
static const char *parse_expr(const char *p)
{
    switch (*p)
    {
        case ')':
        case ';':
        case ':':
        case '[':
        case ']':
        case '}':
            disp_error_message("unexpected char", p);
            exit(1);
    }
    p = parse_subexpr(p, -1);
    return p;
}

/*==========================================
 * 行の解析
 *------------------------------------------
 */
static const char *parse_line(const char *p)
{
    int32_t i = 0, cmd;
    const char *plist[128];

    p = skip_space(p);
    if (*p == ';')
        return p;

    parse_cmd_if = 0;           // warn_cmd_no_commaのために必要

    // 最初は関数名
    const char *p2 = p;
    p = parse_simpleexpr(p);
    p = skip_space(p);

    cmd = parse_cmd;
    if (str_data[cmd].type != C_FUNC)
    {
        disp_error_message("expect command", p2);
//      exit(0);
    }

    add_scriptc(C_ARG);
    while (p && *p && *p != ';' && i < 128)
    {
        plist[i] = p;

        p = parse_expr(p);
        p = skip_space(p);
        // 引数区切りの,処理
        if (*p == ',')
            p++;
        else if (*p != ';' && script_config.warn_cmd_no_comma
                 && parse_cmd_if * 2 <= i)
        {
            disp_error_message("expect ',' or ';' at cmd params", p);
        }
        p = skip_space(p);
        i++;
    }
    plist[i] = p;
    if (!p || *(p++) != ';')
    {
        disp_error_message("need ';'", p);
        exit(1);
    }
    add_scriptc(C_FUNC);

    if (str_data[cmd].type == C_FUNC
        && script_config.warn_cmd_mismatch_paramnum)
    {
        const char *arg = builtin_functions[str_data[cmd].val].arg;
        int32_t j = 0;
        for (j = 0; arg[j]; j++)
            if (arg[j] == '*')
                break;
        if ((arg[j] == 0 && i != j) || (arg[j] == '*' && i < j))
        {
            disp_error_message("illegal number of parameters",
                                plist[(i < j) ? i : j]);
        }
    }

    return p;
}

/*==========================================
 * 組み込み関数の追加
 *------------------------------------------
 */
static void add_builtin_functions(void)
{
    int32_t i, n;
    for (i = 0; builtin_functions[i].func; i++)
    {
        n = add_str(builtin_functions[i].name);
        str_data[n].type = C_FUNC;
        str_data[n].val = i;
        str_data[n].func = builtin_functions[i].func;
    }
}

/*==========================================
 * 定数データベースの読み込み
 *------------------------------------------
 */
static void read_constdb(void)
{
    FILE *fp;
    char line[1024], name[1024];
    int32_t val, n, i, type;

    fp = fopen_("db/const.txt", "r");
    if (fp == NULL)
    {
        printf("can't read db/const.txt\n");
        return;
    }
    while (fgets(line, 1020, fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;
        type = 0;
        if (sscanf(line, "%[A-Za-z0-9_],%d,%d", name, &val, &type) >= 2 ||
            sscanf(line, "%[A-Za-z0-9_] %d %d", name, &val, &type) >= 2)
        {
            for (i = 0; name[i]; i++)
                name[i] = tolower(name[i]);
            n = add_str(name);
            if (type == 0)
                str_data[n].type = C_INT;
            else
                str_data[n].type = C_PARAM;
            str_data[n].val = val;
        }
    }
    fclose_(fp);
}

/*==========================================
 * スクリプトの解析
 *------------------------------------------
 */
const char *parse_script(const char *src, int32_t line)
{
    static bool first = 1;

    if (first)
    {
        add_builtin_functions();
        read_constdb();
    }
    first = 0;
    CREATE(script_buf, char, SCRIPT_BLOCK_SIZE);
    script_pos = 0;
    script_size = SCRIPT_BLOCK_SIZE;
    str_data[LABEL_NEXTLINE].type = C_NOP;
    str_data[LABEL_NEXTLINE].backpatch = -1;
    str_data[LABEL_NEXTLINE].label = -1;
    for (int32_t i = LABEL_START; i < str_num; i++)
    {
        if (str_data[i].type == C_POS || str_data[i].type == C_NAME)
        {
            str_data[i].type = C_NOP;
            str_data[i].backpatch = -1;
            str_data[i].label = -1;
        }
    }

    // 外部用label dbの初期化
    if (scriptlabel_db)
        // not strdb_final
        db_final(scriptlabel_db);
    scriptlabel_db = strdb_init();

    // globals for for error message
    startptr = src;
    startline = line;

    const char *p = src;
    p = skip_space(p);
    if (*p != '{')
    {
        disp_error_message("not found '{'", p);
        return NULL;
    }
    for (p++; p && *p && *p != '}';)
    {
        p = skip_space(p);
        const char *p_skipw = skip_word(p);
        // labelだけ特殊処理
        const char *tmpp = skip_space(p_skipw);
        if (*tmpp == ':')
        {
            int32_t l = add_str(p, p_skipw - p);
            if (str_data[l].label != -1)
            {
                disp_error_message("dup label ", p);
                exit(1);
            }
            set_label(l, script_pos);
            // Yes, we are adding a non-NUL-terminated string as the key to the DB
            // the only use of scriptlabel_db is in npc.cpp, doing a foreach
            // since there is no db_search, and there's a NUL out there *somewhere*
            // (but usually it will stop before the ':'), nothing goes wrong
            strdb_insert(scriptlabel_db, p, script_pos);   // 外部用label db登録
            p = tmpp + 1;
            continue;
        }

        // 他は全部一緒くた
        p = parse_line(p);
        p = skip_space(p);
        add_scriptc(C_EOL);

        set_label(LABEL_NEXTLINE, script_pos);
        str_data[LABEL_NEXTLINE].type = C_NOP;
        str_data[LABEL_NEXTLINE].backpatch = -1;
        str_data[LABEL_NEXTLINE].label = -1;
    }

    add_scriptc(C_NOP);

    script_size = script_pos;
    RECREATE(script_buf, char, script_pos + 1);

    // 未解決のラベルを解決
    for (int32_t i = LABEL_START; i < str_num; i++)
    {
        if (str_data[i].type == C_NOP)
        {
            int32_t j, next;
            str_data[i].type = C_NAME;
            str_data[i].label = i;
            for (j = str_data[i].backpatch; j >= 0 && j != 0x00ffffff;)
            {
                next = (*reinterpret_cast<int32_t *>(script_buf + j)) & 0x00ffffff;
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
static MapSessionData *script_rid2sd(struct script_state *st)
{
    MapSessionData *sd = map_id2sd(st->rid);
    if (!sd)
    {
        printf("script_rid2sd: fatal error ! player not attached!\n");
    }
    return sd;
}

/*==========================================
 * 変数の読み取り
 *------------------------------------------
 */
static int32_t get_val(struct script_state *st, struct script_data *data)
{
    MapSessionData *sd = NULL;
    if (data->type == C_NAME)
    {
        char *name = str_buf + str_data[data->u.num & 0x00ffffff].str;
        char prefix = *name;
        char postfix = name[strlen(name) - 1];

        if (prefix != '$')
        {
            if ((sd = script_rid2sd(st)) == NULL)
                printf("get_val error name?:%s\n", name);
        }
        if (postfix == '$')
        {

            data->type = C_CONSTSTR;
            if (prefix == '@' || prefix == 'l')
            {
                // TODO remove 'l'
                if (sd)
                    data->u.str = pc_readregstr(sd, data->u.num);
            }
            else if (prefix == '$')
            {
                data->u.str = static_cast<char *>(numdb_search(mapregstr_db, data->u.num).p);
            }
            else
            {
                printf("script: get_val: illegal scope string variable.\n");
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
                    data->u.num = pc_readparam(sd, static_cast<SP>(str_data[data->u.num & 0x00ffffff].val));
            }
            else if (prefix == '@' || prefix == 'l')
            {
                // TODO remove 'l'
                if (sd)
                    data->u.num = pc_readreg(sd, data->u.num);
            }
            else if (prefix == '$')
            {
                data->u.num = numdb_search(mapreg_db, data->u.num).i;
            }
            else if (prefix == '#')
            {
                if (name[1] == '#')
                {
                    if (sd)
                        data->u.num = pc_readaccountreg2(sd, name);
                }
                else
                {
                    if (sd)
                        data->u.num = pc_readaccountreg(sd, name);
                }
            }
            else
            {
                if (sd)
                    data->u.num = pc_readglobalreg(sd, name);
            }
        }
    }
    return 0;
}

/*==========================================
 * 変数の読み取り2
 *------------------------------------------
 */
static const void *get_val2(struct script_state *st, int32_t num)
{
    struct script_data dat;
    dat.type = C_NAME;
    dat.u.num = num;
    get_val(st, &dat);
    if (dat.type == C_INT)
        return reinterpret_cast<void *>(dat.u.num);
    else
        return dat.u.str;
}

/*==========================================
 * 変数設定用
 *------------------------------------------
 */
static int32_t set_reg(MapSessionData *sd, int32_t num, const char *name, const void *v)
{
    const char prefix = *name;
    const char postfix = name[strlen(name) - 1];

    if (postfix == '$')
    {
        const char *str = static_cast<const char *>(v);
        if (prefix == '@' || prefix == 'l')
        {
            // TODO remove 'l'
            pc_setregstr(sd, num, str);
        }
        else if (prefix == '$')
        {
            mapreg_setregstr(num, str);
        }
        else
        {
            printf("script: set_reg: illegal scope string variable !");
        }
    }
    else
    {
        // 数値
        int32_t val = reinterpret_cast<int32_t>(v);
        if (str_data[num & 0x00ffffff].type == C_PARAM)
        {
            pc_setparam(sd, static_cast<SP>(str_data[num & 0x00ffffff].val), val);
        }
        else if (prefix == '@' || prefix == 'l')
        {
            // TODO remove 'l'
            pc_setreg(sd, num, val);
        }
        else if (prefix == '$')
        {
            mapreg_setreg(num, val);
        }
        else if (prefix == '#')
        {
            if (name[1] == '#')
                pc_setaccountreg2(sd, name, val);
            else
                pc_setaccountreg(sd, name, val);
        }
        else
        {
            pc_setglobalreg(sd, name, val);
        }
    }
    return 0;
}

/*==========================================
 * 文字列への変換
 *------------------------------------------
 */
static const char *conv_str(struct script_state *st, struct script_data *data)
{
    get_val(st, data);
    if (data->type == C_INT)
    {
        char *buf;
        CREATE(buf, char, 16);
        sprintf(buf, "%d", data->u.num);
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
static int32_t conv_num(struct script_state *st, struct script_data *data)
{
    const char *p;
    get_val(st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        p = data->u.str;
        data->u.num = atoi(p);
        if (data->type == C_STR)
            free(const_cast<char *>(p));
        data->type = C_INT;
    }
    return data->u.num;
}

/*==========================================
 * スタックへ数値をプッシュ
 *------------------------------------------
 */
void push_val(struct script_stack *stack, int32_t type, int32_t val)
{
    if (stack->sp >= stack->sp_max)
    {
        stack->sp_max += 64;
        RECREATE(stack->stack_data, struct script_data, stack->sp_max);
        memset(stack->stack_data + (stack->sp_max - 64), 0,
               64 * sizeof(struct script_data));
    }
//  if (battle_config.etc_log)
//      printf("push (%d,%d)-> %d\n",type,val,stack->sp);
    stack->stack_data[stack->sp].type = type;
    stack->stack_data[stack->sp].u.num = val;
    stack->sp++;
}

/*==========================================
 * スタックへ文字列をプッシュ
 *------------------------------------------
 */
static void push_str(struct script_stack *stack, int32_t type, const char *str)
{
    if (stack->sp >= stack->sp_max)
    {
        stack->sp_max += 64;
        RECREATE(stack->stack_data, struct script_data, stack->sp_max);
        memset(stack->stack_data + (stack->sp_max - 64), '\0',
               64 * sizeof(struct script_data));
    }
//  if (battle_config.etc_log)
//      printf("push (%d,%x)-> %d\n",type,str,stack->sp);
    stack->stack_data[stack->sp].type = type;
    stack->stack_data[stack->sp].u.str = str;
    stack->sp++;
}

/*==========================================
 * スタックへ複製をプッシュ
 *------------------------------------------
 */
static void push_copy(struct script_stack *stack, int32_t pos)
{
    switch (stack->stack_data[pos].type)
    {
        case C_CONSTSTR:
            push_str(stack, C_CONSTSTR, stack->stack_data[pos].u.str);
            break;
        case C_STR:
            push_str(stack, C_STR, strdup(stack->stack_data[pos].u.str));
            break;
        default:
            push_val(stack, stack->stack_data[pos].type,
                      stack->stack_data[pos].u.num);
            break;
    }
}

/*==========================================
 * スタックからポップ
 *------------------------------------------
 */
static void pop_stack(struct script_stack *stack, int32_t start, int32_t end)
{
    int32_t i;
    for (i = start; i < end; i++)
    {
        if (stack->stack_data[i].type == C_STR)
        {
            free(const_cast<char *>(stack->stack_data[i].u.str));
        }
    }
    if (stack->sp > end)
    {
        memmove(&stack->stack_data[start], &stack->stack_data[end],
                 sizeof(stack->stack_data[0]) * (stack->sp - end));
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
void builtin_mes(struct script_state *st)
{
    conv_str(st, &(st->stack->stack_data[st->start + 2]));
    clif_scriptmes(script_rid2sd(st), st->oid,
                    st->stack->stack_data[st->start + 2].u.str);
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_goto(struct script_state *st)
{
    int32_t pos;

    if (st->stack->stack_data[st->start + 2].type != C_POS)
    {
        printf("script: goto: not label !\n");
        st->state = END;
        return;
    }

    pos = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    st->pos = pos;
    st->state = GOTO;
}

/*==========================================
 * ユーザー定義関数の呼び出し
 *------------------------------------------
 */
void builtin_callfunc(struct script_state *st)
{
    char *scr;
    const char *str = conv_str(st, &(st->stack->stack_data[st->start + 2]));

    if ((scr = static_cast<char *>(strdb_search(script_get_userfunc_db(), str).p)))
    {
        int32_t i, j;
        for (i = st->start + 3, j = 0; i < st->end; i++, j++)
            push_copy(st->stack, i);

        push_val(st->stack, C_INT, j); // 引数の数をプッシュ
        push_val(st->stack, C_INT, st->defsp); // 現在の基準スタックポインタをプッシュ
        push_val(st->stack, C_INT, reinterpret_cast<int32_t>(st->script));  // 現在のスクリプトをプッシュ
        push_val(st->stack, C_RETINFO, st->pos);   // 現在のスクリプト位置をプッシュ

        st->pos = 0;
        st->script = scr;
        st->defsp = st->start + 4 + j;
        st->state = GOTO;
    }
    else
    {
        printf("script:callfunc: function not found! [%s]\n", str);
        st->state = END;
    }
}

/*==========================================
 * サブルーティンの呼び出し
 *------------------------------------------
 */
void builtin_callsub(struct script_state *st)
{
    int32_t pos = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    int32_t i, j;
    for (i = st->start + 3, j = 0; i < st->end; i++, j++)
        push_copy(st->stack, i);

    push_val(st->stack, C_INT, j); // 引数の数をプッシュ
    push_val(st->stack, C_INT, st->defsp); // 現在の基準スタックポインタをプッシュ
    push_val(st->stack, C_INT, reinterpret_cast<int32_t>(st->script));  // 現在のスクリプトをプッシュ
    push_val(st->stack, C_RETINFO, st->pos);   // 現在のスクリプト位置をプッシュ

    st->pos = pos;
    st->defsp = st->start + 4 + j;
    st->state = GOTO;
}

/*==========================================
 * 引数の所得
 *------------------------------------------
 */
void builtin_getarg(struct script_state *st)
{
    int32_t num = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    int32_t max, stsp;
    if (st->defsp < 4
        || st->stack->stack_data[st->defsp - 1].type != C_RETINFO)
    {
        printf("script:getarg without callfunc or callsub!\n");
        st->state = END;
        return;
    }
    max = conv_num(st, &(st->stack->stack_data[st->defsp - 4]));
    stsp = st->defsp - max - 4;
    if (num >= max)
    {
        printf("script:getarg arg1(%d) out of range(%d) !\n", num, max);
        st->state = END;
        return;
    }
    push_copy(st->stack, stsp + num);
}

/*==========================================
 * サブルーチン/ユーザー定義関数の終了
 *------------------------------------------
 */
void builtin_return(struct script_state *st)
{
    if (st->end > st->start + 2)
    {                           // 戻り値有り
        push_copy(st->stack, st->start + 2);
    }
    st->state = RETFUNC;
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_next(struct script_state *st)
{
    st->state = STOP;
    clif_scriptnext(script_rid2sd(st), st->oid);
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_close(struct script_state *st)
{
    st->state = END;
    clif_scriptclose(script_rid2sd(st), st->oid);
}

void builtin_close2(struct script_state *st)
{
    st->state = STOP;
    clif_scriptclose(script_rid2sd(st), st->oid);
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_menu(struct script_state *st)
{
    int32_t menu_choices = 0;
    int32_t finished_menu_items = 0;   // [fate] set to 1 after we hit the first empty string

    MapSessionData *sd;

    sd = script_rid2sd(st);

    // We don't need to do this iteration if the player cancels, strictly speaking.
    for (int32_t i = st->start + 2; i < st->end; i += 2)
    {
        conv_str(st, &(st->stack->stack_data[i]));
        size_t choice_len = strlen(st->stack->stack_data[i].u.str);

        if (choice_len && !finished_menu_items)
            ++menu_choices;
        else
            finished_menu_items = 1;
    }

    if (sd->state.menu_or_input == 0)
    {
        st->state = RERUNLINE;
        sd->state.menu_or_input = 1;

        std::vector<std::string> choices;
        for (int32_t i = st->start + 2; menu_choices > 0; i += 2, --menu_choices)
        {
            choices.push_back(st->stack->stack_data[i].u.str);
        }
        clif_scriptmenu(script_rid2sd(st), st->oid, choices);
    }
    else if (sd->npc_menu == 0xff)
    {                           // cansel
        sd->state.menu_or_input = 0;
        st->state = END;
    }
    else
    {
        pc_setreg(sd, add_str("@menu"), sd->npc_menu);
        sd->state.menu_or_input = 0;
        if (sd->npc_menu > 0 && sd->npc_menu <= menu_choices)
        {
            int32_t pos;
            if (st->stack->
                stack_data[st->start + sd->npc_menu * 2 + 1].type != C_POS)
            {
                st->state = END;
                return;
            }
            pos =
                conv_num(st,
                          &(st->
                            stack->stack_data[st->start + sd->npc_menu * 2 +
                                              1]));
            st->pos = pos;
            st->state = GOTO;
        }
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_rand(struct script_state *st)
{
    int32_t range, min, max;

    if (st->end > st->start + 3)
    {
        min = conv_num(st, &(st->stack->stack_data[st->start + 2]));
        max = conv_num(st, &(st->stack->stack_data[st->start + 3]));
        if (max < min)
        {
            int32_t tmp;
            tmp = min;
            min = max;
            max = tmp;
        }
        range = max - min + 1;
        push_val(st->stack, C_INT, (range <= 0 ? 0 : MRAND(range)) + min);
    }
    else
    {
        range = conv_num(st, &(st->stack->stack_data[st->start + 2]));
        push_val(st->stack, C_INT, range <= 0 ? 0 : MRAND(range));
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_pow(struct script_state *st)
{
    int32_t a, b;

    a = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    b = conv_num(st, &(st->stack->stack_data[st->start + 3]));

    push_val(st->stack, C_INT, pow(a * 0.001, b));
}

/*==========================================
 * Check whether the PC is at the specified location
 *------------------------------------------
 */
void builtin_isat(struct script_state *st)
{
    int32_t x, y;
    const char *str;
    MapSessionData *sd = script_rid2sd(st);

    str = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    x = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    y = conv_num(st, &(st->stack->stack_data[st->start + 4]));

    if (!sd)
        return;

    push_val(st->stack, C_INT,
              (x == sd->x)
              && (y == sd->y) && (!strcmp(str, &maps[sd->m].name)));
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_warp(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);

    const char *str = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    int16_t x = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    int16_t y = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    if (strcmp(str, "Random") == 0)
        pc_randomwarp(sd, BeingRemoveType::WARP);
    else if (strcmp(str, "SavePoint") == 0)
    {
        if (maps[sd->m].flag.noreturn)    // 蝶禁止
            return;

        pc_setpos(sd, sd->status.save_point, BeingRemoveType::WARP);
    }
    else if (strcmp(str, "Save") == 0)
    {
        if (maps[sd->m].flag.noreturn)    // 蝶禁止
            return;

        pc_setpos(sd, sd->status.save_point, BeingRemoveType::WARP);
    }
    else
    {
        fixed_string<16> fstr;
        fstr.copy_from(str);
        pc_setpos(sd, Point{fstr, x, y}, BeingRemoveType::ZERO);
    }
}

/*==========================================
 * エリア指定ワープ
 *------------------------------------------
 */
static void builtin_areawarp_sub(BlockList *bl, Point point)
{
    if (strcmp(&point.map, "Random") == 0)
        pc_randomwarp(static_cast<MapSessionData *>(bl), BeingRemoveType::WARP);
    else
        pc_setpos(static_cast<MapSessionData *>(bl), point, BeingRemoveType::ZERO);
}

void builtin_areawarp(struct script_state *st)
{
    fixed_string<16> src_map;
    src_map.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    int32_t x_0 = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    int32_t y_0 = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    int32_t x_1 = conv_num(st, &(st->stack->stack_data[st->start + 5]));
    int32_t y_1 = conv_num(st, &(st->stack->stack_data[st->start + 6]));
    fixed_string<16> dst_map;
    dst_map.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 7])));
    int16_t x = conv_num(st, &(st->stack->stack_data[st->start + 8]));
    int16_t y = conv_num(st, &(st->stack->stack_data[st->start + 9]));

    int32_t m = map_mapname2mapid(src_map);
    if (m < 0)
        return;

    map_foreachinarea(builtin_areawarp_sub,
                      m, x_0, y_0, x_1, y_1, BL_PC, Point{dst_map, x, y});
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_heal(struct script_state *st)
{
    int32_t hp, sp;

    hp = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    sp = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    pc_heal(script_rid2sd(st), hp, sp);
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_itemheal(struct script_state *st)
{
    int32_t hp, sp;

    hp = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    sp = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    pc_itemheal(script_rid2sd(st), hp, sp);
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_percentheal(struct script_state *st)
{
    int32_t hp, sp;

    hp = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    sp = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    pc_percentheal(script_rid2sd(st), hp, sp);
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_input(struct script_state *st)
{
    MapSessionData *sd = NULL;
    int32_t num =
        (st->end >
         st->start + 2) ? st->stack->stack_data[st->start + 2].u.num : 0;
    const char *name =
        (st->end >
         st->start + 2) ? str_buf + str_data[num & 0x00ffffff].str : "";
//  char prefix=*name;
    char postfix = name[strlen(name) - 1];

    sd = script_rid2sd(st);
    if (sd->state.menu_or_input)
    {
        sd->state.menu_or_input = 0;
        if (postfix == '$')
        {
            // 文字列
            if (st->end > st->start + 2)
            {                   // 引数1個
                set_reg(sd, num, name, sd->npc_str);
            }
            else
            {
                printf("builtin_input: string discarded !!\n");
            }
        }
        else
        {

            //commented by Lupus (check Value Number Input fix in clif.c)
            //** Fix by fritz :X keeps people from abusing old input bugs
            if (sd->npc_amount < 0) //** If input amount is less then 0
            {
                clif_tradecancelled(sd);   // added "Deal has been cancelled" message by Valaris
                builtin_close(st); //** close
            }

            // 数値
            if (st->end > st->start + 2)
            {                   // 引数1個
                set_reg(sd, num, name, reinterpret_cast<void *>(sd->npc_amount));
            }
            else
            {
                // ragemu互換のため
                pc_setreg(sd, add_str("l14"), sd->npc_amount);
            }
        }
    }
    else
    {
        st->state = RERUNLINE;
        if (postfix == '$')
            clif_scriptinputstr(sd, st->oid);
        else
            clif_scriptinput(sd, st->oid);
        sd->state.menu_or_input = 1;
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_if(struct script_state *st)
{
    int32_t sel, i;

    sel = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    if (!sel)
        return;

    // 関数名をコピー
    push_copy(st->stack, st->start + 3);
    // 間に引数マーカを入れて
    push_val(st->stack, C_ARG, 0);
    // 残りの引数をコピー
    for (i = st->start + 4; i < st->end; i++)
    {
        push_copy(st->stack, i);
    }
    run_func(st);
}

/*==========================================
 * 変数設定
 *------------------------------------------
 */
void builtin_set(struct script_state *st)
{
    MapSessionData *sd = NULL;
    int32_t num = st->stack->stack_data[st->start + 2].u.num;
    char *name = str_buf + str_data[num & 0x00ffffff].str;
    char prefix = *name;
    char postfix = name[strlen(name) - 1];

    if (st->stack->stack_data[st->start + 2].type != C_NAME)
    {
        printf("script: builtin_set: not name\n");
        return;
    }

    if (prefix != '$')
        sd = script_rid2sd(st);

    if (postfix == '$')
    {
        // 文字列
        const char *str = conv_str(st, &(st->stack->stack_data[st->start + 3]));
        set_reg(sd, num, name, str);
    }
    else
    {
        // 数値
        int32_t val = conv_num(st, &(st->stack->stack_data[st->start + 3]));
        set_reg(sd, num, name, reinterpret_cast<void *>(val));
    }
}

/*==========================================
 * 配列変数設定
 *------------------------------------------
 */
void builtin_setarray(struct script_state *st)
{
    MapSessionData *sd = NULL;
    int32_t num = st->stack->stack_data[st->start + 2].u.num;
    char *name = str_buf + str_data[num & 0x00ffffff].str;
    char prefix = *name;
    char postfix = name[strlen(name) - 1];
    int32_t i, j;

    if (prefix != '$' && prefix != '@')
    {
        printf("builtin_setarray: illegal scope !\n");
        return;
    }
    if (prefix != '$')
        sd = script_rid2sd(st);

    for (j = 0, i = st->start + 3; i < st->end && j < 128; i++, j++)
    {
        // This is ugly but it works
        union
        {
            const void *p;
            intptr_t i;
        } v;
        if (postfix == '$')
            v.p = conv_str(st, &(st->stack->stack_data[i]));
        else
            v.i = conv_num(st, &(st->stack->stack_data[i]));
        set_reg(sd, num + (j << 24), name, v.p);
    }
}

/*==========================================
 * 配列変数クリア
 *------------------------------------------
 */
void builtin_cleararray(struct script_state *st)
{
    MapSessionData *sd = NULL;
    int32_t num = st->stack->stack_data[st->start + 2].u.num;
    char *name = str_buf + str_data[num & 0x00ffffff].str;
    char prefix = *name;
    char postfix = name[strlen(name) - 1];
    int32_t sz = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    int32_t i;

    if (prefix != '$' && prefix != '@')
    {
        printf("builtin_cleararray: illegal scope !\n");
        return;
    }
    if (prefix != '$')
        sd = script_rid2sd(st);

    // This is ugly but it works
    union
    {
        const void *p;
        intptr_t i;
    } v;
    if (postfix == '$')
        v.p = conv_str(st, &(st->stack->stack_data[st->start + 3]));
    else
        v.i = conv_num(st, &(st->stack->stack_data[st->start + 3]));

    for (i = 0; i < sz; i++)
        set_reg(sd, num + (i << 24), name, v.p);
}

/*==========================================
 * 配列変数コピー
 *------------------------------------------
 */
void builtin_copyarray(struct script_state *st)
{
    MapSessionData *sd = NULL;
    int32_t num = st->stack->stack_data[st->start + 2].u.num;
    char *name = str_buf + str_data[num & 0x00ffffff].str;
    char prefix = *name;
    char postfix = name[strlen(name) - 1];
    int32_t num2 = st->stack->stack_data[st->start + 3].u.num;
    char *name2 = str_buf + str_data[num2 & 0x00ffffff].str;
    char prefix2 = *name2;
    char postfix2 = name2[strlen(name2) - 1];
    int32_t sz = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    int32_t i;

    if (prefix != '$' && prefix != '@' && prefix2 != '$' && prefix2 != '@')
    {
        printf("builtin_copyarray: illegal scope !\n");
        return;
    }
    if ((postfix == '$' || postfix2 == '$') && postfix != postfix2)
    {
        printf("builtin_copyarray: type mismatch !\n");
        return;
    }
    if (prefix != '$' || prefix2 != '$')
        sd = script_rid2sd(st);

    for (i = 0; i < sz; i++)
        set_reg(sd, num + (i << 24), name, get_val2(st, num2 + (i << 24)));
}

/*==========================================
 * 配列変数のサイズ所得
 *------------------------------------------
 */
static int32_t getarraysize(struct script_state *st, int32_t num, int32_t postfix)
{
    int32_t i = (num >> 24), c = i;
    for (; i < 128; i++)
    {
        const void *v = get_val2(st, num + (i << 24));
        if (postfix == '$' && *static_cast<const char *>(v))
            c = i;
        if (postfix != '$' && reinterpret_cast<int32_t>(v))
            c = i;
    }
    return c + 1;
}

void builtin_getarraysize(struct script_state *st)
{
    int32_t num = st->stack->stack_data[st->start + 2].u.num;
    char *name = str_buf + str_data[num & 0x00ffffff].str;
    char prefix = *name;
    char postfix = name[strlen(name) - 1];

    if (prefix != '$' && prefix != '@')
    {
        printf("builtin_copyarray: illegal scope !\n");
        return;
    }

    push_val(st->stack, C_INT, getarraysize(st, num, postfix));
}

/*==========================================
 * 配列変数から要素削除
 *------------------------------------------
 */
void builtin_deletearray(struct script_state *st)
{
    MapSessionData *sd = NULL;
    int32_t num = st->stack->stack_data[st->start + 2].u.num;
    char *name = str_buf + str_data[num & 0x00ffffff].str;
    char prefix = *name;
    char postfix = name[strlen(name) - 1];
    int32_t count = 1;
    int32_t i, sz = getarraysize(st, num, postfix) - (num >> 24) - count + 1;

    if ((st->end > st->start + 3))
        count = conv_num(st, &(st->stack->stack_data[st->start + 3]));

    if (prefix != '$' && prefix != '@')
    {
        printf("builtin_deletearray: illegal scope !\n");
        return;
    }
    if (prefix != '$')
        sd = script_rid2sd(st);

    for (i = 0; i < sz; i++)
    {
        set_reg(sd, num + (i << 24), name,
                 get_val2(st, num + ((i + count) << 24)));
    }
    for (; i < (128 - (num >> 24)); i++)
    {
        if (postfix != '$')
            set_reg(sd, num + (i << 24), name, 0);
        if (postfix == '$')
            set_reg(sd, num + (i << 24), name, "");
    }
}

/*==========================================
 * 指定要素を表す値(キー)を所得する
 *------------------------------------------
 */
void builtin_getelementofarray(struct script_state *st)
{
    if (st->stack->stack_data[st->start + 2].type == C_NAME)
    {
        int32_t i = conv_num(st, &(st->stack->stack_data[st->start + 3]));
        if (i > 127 || i < 0)
        {
            printf
                ("script: getelementofarray (operator[]): param2 illegal number %d\n",
                 i);
            push_val(st->stack, C_INT, 0);
        }
        else
        {
            push_val(st->stack, C_NAME,
                      (i << 24) | st->stack->stack_data[st->start + 2].u.num);
        }
    }
    else
    {
        printf("script: getelementofarray (operator[]): param1 not name !\n");
        push_val(st->stack, C_INT, 0);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_setlook(struct script_state *st)
{
    LOOK type = static_cast<LOOK>(conv_num(st, &(st->stack->stack_data[st->start + 2])));
    int32_t val = conv_num(st, &(st->stack->stack_data[st->start + 3]));

    pc_changelook(script_rid2sd(st), type, val);
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_countitem(struct script_state *st)
{
    int32_t nameid = 0, count = 0, i;
    MapSessionData *sd;

    struct script_data *data;

    sd = script_rid2sd(st);

    data = &(st->stack->stack_data[st->start + 2]);
    get_val(st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str(st, data);
        struct item_data *item_data;
        if ((item_data = itemdb_searchname(name)) != NULL)
            nameid = item_data->nameid;
    }
    else
        nameid = conv_num(st, data);

    if (nameid >= 500)          //if no such ID then skip this iteration
        for (i = 0; i < MAX_INVENTORY; i++)
        {
            if (sd->status.inventory[i].nameid == nameid)
                count += sd->status.inventory[i].amount;
        }
    else
    {
        map_log("wrong map_logitem ID : countitem(%i)\n", nameid);
    }
    push_val(st->stack, C_INT, count);
}

/*==========================================
 * 重量チェック
 *------------------------------------------
 */
void builtin_checkweight(struct script_state *st)
{
    int32_t nameid = 0, amount;
    MapSessionData *sd;
    struct script_data *data;

    sd = script_rid2sd(st);

    data = &(st->stack->stack_data[st->start + 2]);
    get_val(st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str(st, data);
        struct item_data *item_data = itemdb_searchname(name);
        if (item_data)
            nameid = item_data->nameid;
    }
    else
        nameid = conv_num(st, data);

    amount = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    if (amount <= 0 || nameid < 500)
    {                           //if get wrong item ID or amount<=0, don't count weight of non existing items
        push_val(st->stack, C_INT, 0);
    }

    sd = script_rid2sd(st);
    if (itemdb_weight(nameid) * amount + sd->weight > sd->max_weight)
    {
        push_val(st->stack, C_INT, 0);
    }
    else
    {
        push_val(st->stack, C_INT, 1);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_getitem(struct script_state *st)
{
    int32_t nameid, amount;
    struct item item_tmp;
    MapSessionData *sd;
    struct script_data *data;

    sd = script_rid2sd(st);

    data = &(st->stack->stack_data[st->start + 2]);
    get_val(st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str(st, data);
        struct item_data *item_data = itemdb_searchname(name);
        nameid = 727;           //Default to iten
        if (item_data != NULL)
            nameid = item_data->nameid;
    }
    else
        nameid = conv_num(st, data);

    if ((amount =
         conv_num(st, &(st->stack->stack_data[st->start + 3]))) <= 0)
    {
        return;               //return if amount <=0, skip the useles iteration
    }

    if (nameid > 0)
    {
        memset(&item_tmp, 0, sizeof(item_tmp));
        item_tmp.nameid = nameid;
        if (st->end > st->start + 5)    //アイテムを指定したIDに渡す
            sd = map_id2sd(conv_num(st, &(st->stack->stack_data[st->start + 5])));
        if (sd == NULL)         //アイテムを渡す相手がいなかったらお帰り
            return;
        PickupFail flag = pc_additem(sd, &item_tmp, amount);
        if (flag != PickupFail::OKAY)
        {
            clif_additem(sd, 0, 0, flag);
            map_addflooritem(&item_tmp, amount, sd->m, sd->x, sd->y,
                              NULL, NULL, NULL);
        }
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_makeitem(struct script_state *st)
{
    int32_t nameid, amount;
    int32_t x, y, m;
    struct item item_tmp;
    MapSessionData *sd;
    struct script_data *data;

    sd = script_rid2sd(st);

    data = &(st->stack->stack_data[st->start + 2]);
    get_val(st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str(st, data);
        struct item_data *item_data = itemdb_searchname(name);
        nameid = 512;           //Apple Item ID
        if (item_data)
            nameid = item_data->nameid;
    }
    else
        nameid = conv_num(st, data);

    amount = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    fixed_string<16> mapname;
    mapname.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 4])));
    x = conv_num(st, &(st->stack->stack_data[st->start + 5]));
    y = conv_num(st, &(st->stack->stack_data[st->start + 6]));

    if (sd && strcmp(&mapname, "this") == 0)
        m = sd->m;
    else
        m = map_mapname2mapid(mapname);

    if (nameid > 0)
    {
        memset(&item_tmp, 0, sizeof(item_tmp));
        item_tmp.nameid = nameid;

        map_addflooritem(&item_tmp, amount, m, x, y, NULL, NULL, NULL);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_delitem(struct script_state *st)
{
    int32_t nameid = 0, amount, i;
    MapSessionData *sd;
    struct script_data *data;

    sd = script_rid2sd(st);

    data = &(st->stack->stack_data[st->start + 2]);
    get_val(st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str(st, data);
        struct item_data *item_data = itemdb_searchname(name);
        //nameid=512;
        if (item_data)
            nameid = item_data->nameid;
    }
    else
        nameid = conv_num(st, data);

    amount = conv_num(st, &(st->stack->stack_data[st->start + 3]));

    if (nameid < 500 || amount <= 0)
    {                           //by Lupus. Don't run FOR if u got wrong item ID or amount<=0
        //printf("wrong item ID or amount<=0 : delitem %i,\n",nameid,amount);
        return;
    }
    sd = script_rid2sd(st);

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
                pc_delitem(sd, i, amount, 0);
                break;
            }
            else
            {
                amount -= sd->status.inventory[i].amount;
                if (amount == 0)
                    amount = sd->status.inventory[i].amount;
                pc_delitem(sd, i, amount, 0);
                break;
            }
        }
    }
}

/*==========================================
 *キャラ関係のパラメータ取得
 *------------------------------------------
 */
void builtin_readparam(struct script_state *st)
{
    int32_t type;
    MapSessionData *sd;

    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    if (st->end > st->start + 3)
        sd = map_nick2sd(conv_str
                          (st, &(st->stack->stack_data[st->start + 3])));
    else
        sd = script_rid2sd(st);

    if (sd == NULL)
    {
        push_val(st->stack, C_INT, -1);
        return;
    }

    push_val(st->stack, C_INT, pc_readparam(sd, static_cast<SP>(type)));
}

/*==========================================
 *キャラ関係のID取得
 *------------------------------------------
 */
void builtin_getcharid(struct script_state *st)
{
    int32_t num;
    MapSessionData *sd;

    num = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    if (st->end > st->start + 3)
        sd = map_nick2sd(conv_str
                          (st, &(st->stack->stack_data[st->start + 3])));
    else
        sd = script_rid2sd(st);
    if (sd == NULL)
    {
        push_val(st->stack, C_INT, -1);
        return;
    }
    if (num == 0)
        push_val(st->stack, C_INT, sd->status.char_id);
    if (num == 1)
        push_val(st->stack, C_INT, sd->status.party_id);
    if (num == 2)
        push_val(st->stack, C_INT, 0 /*guild_id*/);
    if (num == 3)
        push_val(st->stack, C_INT, sd->status.account_id);
}

/*==========================================
 *指定IDのPT名取得
 *------------------------------------------
 */
static char *builtin_getpartyname_sub(int32_t party_id)
{
    struct party *p;

    p = NULL;
    p = party_search(party_id);

    if (p != NULL)
    {
        char *buf;
        CREATE(buf, char, 24);
        strcpy(buf, p->name);
        return buf;
    }

    return 0;
}

void builtin_getpartyname(struct script_state *st)
{
    char *name;
    int32_t party_id;

    party_id = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    name = builtin_getpartyname_sub(party_id);
    if (name != 0)
        push_str(st->stack, C_STR, name);
    else
        push_str(st->stack, C_CONSTSTR, "null");
}

/*==========================================
 *指定IDのPT人数とメンバーID取得
 *------------------------------------------
 */
void builtin_getpartymember(struct script_state *st)
{
    struct party *p;
    int32_t i, j = 0;

    p = NULL;
    p = party_search(conv_num(st, &(st->stack->stack_data[st->start + 2])));

    if (p != NULL)
    {
        for (i = 0; i < MAX_PARTY; i++)
        {
            if (p->member[i].account_id)
            {
//              printf("name:%s %d\n",p->member[i].name,i);
                mapreg_setregstr(add_str("$@partymembername$") + (i << 24),
                                  p->member[i].name);
                j++;
            }
        }
    }
    mapreg_setreg(add_str("$@partymembercount"), j);
}

/*==========================================
 * キャラクタの名前
 *------------------------------------------
 */
void builtin_strcharinfo(struct script_state *st)
{
    MapSessionData *sd;
    int32_t num;

    sd = script_rid2sd(st);
    num = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    if (num == 0)
    {
        char *buf;
        CREATE(buf, char, 24);
        strncpy(buf, sd->status.name, 23);
        push_str(st->stack, C_STR, buf);
    }
    if (num == 1)
    {
        char *buf;
        buf = builtin_getpartyname_sub(sd->status.party_id);
        if (buf != 0)
            push_str(st->stack, C_STR, buf);
        else
            push_str(st->stack, C_CONSTSTR, "");
    }
    if (num == 2)
    {
        // was: guild name
        push_str(st->stack, C_CONSTSTR, "");
    }
}

/*==========================================
 * GetEquipID(Pos);     Pos: 1-10
 *------------------------------------------
 */
void builtin_getequipid(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    if (sd == NULL)
    {
        printf("getequipid: sd == NULL\n");
        return;
    }
    EQ_SCR num = static_cast<EQ_SCR>(conv_num(st, &(st->stack->stack_data[st->start + 2])));
    int32_t i = pc_checkequip(sd, equip[num]);
    if (i >= 0)
    {
        struct item_data *item = sd->inventory_data[i];
        if (item)
            push_val(st->stack, C_INT, item->nameid);
        else
            push_val(st->stack, C_INT, 0);
    }
    else
    {
        push_val(st->stack, C_INT, -1);
    }
}

/*==========================================
 * 装備名文字列（精錬メニュー用）
 *------------------------------------------
 */
void builtin_getequipname(struct script_state *st)
{
    char *buf;
    CREATE(buf, char, 64);

    MapSessionData *sd = script_rid2sd(st);
    EQ_SCR num = static_cast<EQ_SCR>(conv_num(st, &(st->stack->stack_data[st->start + 2])));
    int32_t i = pc_checkequip(sd, equip[num]);
    if (i >= 0)
    {
        struct item_data *item = sd->inventory_data[i];
        if (item)
            sprintf(buf, "%s-[%s]", epos[num], item->jname);
        else
            sprintf(buf, "%s-[%s]", epos[num], epos[EQ_SCR::none]);
    }
    else
    {
        sprintf(buf, "%s-[%s]", epos[num], epos[num]);
    }
    push_str(st->stack, C_STR, buf);
}

/*==========================================
 * 装備チェック
 *------------------------------------------
 */
void builtin_getequipisequiped(struct script_state *st)
{
    EQ_SCR num = static_cast<EQ_SCR>(conv_num(st, &(st->stack->stack_data[st->start + 2])));
    MapSessionData *sd = script_rid2sd(st);
    int32_t i = pc_checkequip(sd, equip[num]);
    if (i >= 0)
    {
        push_val(st->stack, C_INT, 1);
    }
    else
    {
        push_val(st->stack, C_INT, 0);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_statusup(struct script_state *st)
{
    SP type = static_cast<SP>(conv_num(st, &(st->stack->stack_data[st->start + 2])));
    MapSessionData *sd = script_rid2sd(st);
    pc_statusup(sd, static_cast<SP>(type));
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_statusup2(struct script_state *st)
{
    SP type = static_cast<SP>(conv_num(st, &(st->stack->stack_data[st->start + 2])));
    int32_t val = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    MapSessionData *sd = script_rid2sd(st);
    pc_statusup2(sd, type, val);
}

/*==========================================
 * 装備品による能力値ボーナス
 *------------------------------------------
 */
void builtin_bonus(struct script_state *st)
{
    SP type = static_cast<SP>(conv_num(st, &(st->stack->stack_data[st->start + 2])));
    int32_t val = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    MapSessionData *sd = script_rid2sd(st);
    pc_bonus(sd, type, val);
}

/*==========================================
 * スキル所得
 *------------------------------------------
 */
void builtin_skill(struct script_state *st)
{
    int32_t id, level, flag = 1;
    MapSessionData *sd;

    id = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    level = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    if (st->end > st->start + 4)
        flag = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    sd = script_rid2sd(st);
    pc_skill(sd, id, level, flag);
    clif_skillinfoblock(sd);
}

/*==========================================
 * [Fate] Sets the skill level permanently
 *------------------------------------------
 */
void builtin_setskill(struct script_state *st)
{
    int32_t id, level;
    MapSessionData *sd;

    id = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    level = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    sd = script_rid2sd(st);

    sd->status.skill[id].id = level ? id : 0;
    sd->status.skill[id].lv = level;
    clif_skillinfoblock(sd);
}

/*==========================================
 * スキルレベル所得
 *------------------------------------------
 */
void builtin_getskilllv(struct script_state *st)
{
    int32_t id = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    push_val(st->stack, C_INT, pc_checkskill(script_rid2sd(st), id));
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_getgmlevel(struct script_state *st)
{
    push_val(st->stack, C_INT, pc_isGM(script_rid2sd(st)));
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_end(struct script_state *st)
{
    st->state = END;
}

/*==========================================
 * [Freeyorp] Return the current opt2
 *------------------------------------------
 */

void builtin_getopt2(struct script_state *st)
{
    MapSessionData *sd;

    sd = script_rid2sd(st);

    push_val(st->stack, C_INT, sd->opt2);
}

/*==========================================
 * [Freeyorp] Sets opt2
 *------------------------------------------
 */

void builtin_setopt2(struct script_state *st)
{
    int32_t new_opt2;
    MapSessionData *sd;

    new_opt2 = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd(st);
    if (new_opt2 == sd->opt2)
        return;
    sd->opt2 = new_opt2;
    clif_changeoption(sd);
    pc_calcstatus(sd, 0);
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_checkoption(struct script_state *st)
{
    int32_t type;
    MapSessionData *sd;

    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd(st);

    if (sd->status.option & type)
    {
        push_val(st->stack, C_INT, 1);
    }
    else
    {
        push_val(st->stack, C_INT, 0);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_setoption(struct script_state *st)
{
    int32_t type;
    MapSessionData *sd;

    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    sd = script_rid2sd(st);
    pc_setoption(sd, type);
}

/*==========================================
 *      セーブポイントの保存
 *------------------------------------------
 */
void builtin_savepoint(struct script_state *st)
{
    fixed_string<16> str;
    str.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    int16_t x = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    int16_t y = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    pc_setsavepoint(script_rid2sd(st), Point{str, x, y});
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
void builtin_gettimetick(struct script_state *st)   /* Asgard Version */
{
    int32_t type;
    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));

    switch (type)
    {
        /* Number of seconds elapsed today (0-86399, 00:00:00-23:59:59). */
        case 1:
        {
            time_t timer;
            struct tm *t;

            time(&timer);
            t = gmtime(&timer);
            push_val(st->stack, C_INT,
                      ((t->tm_hour) * 3600 + (t->tm_min) * 60 + t->tm_sec));
            break;
        }
        /* Seconds since Unix epoch. */
        case 2:
            push_val(st->stack, C_INT, time(NULL));
            break;
        /* System tick (uint32_t, and yes, it will wrap). */
        case 0:
        default:
            push_val(st->stack, C_INT, gettick());
            break;
    }
}

/*==========================================
 * GetTime(Type);
 * 1: Sec     2: Min     3: Hour
 * 4: WeekDay     5: MonthDay     6: Month
 * 7: Year
 *------------------------------------------
 */
void builtin_gettime(struct script_state *st)   /* Asgard Version */
{
    int32_t type;
    time_t timer;
    struct tm *t;

    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));

    time(&timer);
    t = gmtime(&timer);

    switch (type)
    {
        case 1:                //Sec (0~59)
            push_val(st->stack, C_INT, t->tm_sec);
            break;
        case 2:                //Min (0~59)
            push_val(st->stack, C_INT, t->tm_min);
            break;
        case 3:                //Hour (0~23)
            push_val(st->stack, C_INT, t->tm_hour);
            break;
        case 4:                //WeekDay (0~6)
            push_val(st->stack, C_INT, t->tm_wday);
            break;
        case 5:                //MonthDay (01~31)
            push_val(st->stack, C_INT, t->tm_mday);
            break;
        case 6:                //Month (01~12)
            push_val(st->stack, C_INT, t->tm_mon + 1);
            break;
        case 7:                //Year (20xx)
            push_val(st->stack, C_INT, t->tm_year + 1900);
            break;
        default:               //(format error)
            push_val(st->stack, C_INT, -1);
            break;
    }
}

/*==========================================
 * GetTimeStr("TimeFMT", Length);
 *------------------------------------------
 */
void builtin_gettimestr(struct script_state *st)
{
    char *tmpstr;
    const char *fmtstr;
    int32_t maxlen;
    time_t now = time(NULL);

    fmtstr = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    maxlen = conv_num(st, &(st->stack->stack_data[st->start + 3]));

    CREATE(tmpstr, char, maxlen + 1);
    strftime(tmpstr, maxlen, fmtstr, gmtime(&now));
    tmpstr[maxlen] = '\0';

    push_str(st->stack, C_STR, tmpstr);
}

/*==========================================
 * カプラ倉庫を開く
 *------------------------------------------
 */
void builtin_openstorage(struct script_state *st)
{
//  int32_t sync = 0;
//  if (st->end >= 3) sync = conv_num(st,& (st->stack->stack_data[st->start+2]));
    MapSessionData *sd = script_rid2sd(st);

//  if (sync) {
    st->state = STOP;
    sd->npc_flags.storage = 1;
//  } else st->state = END;

    storage_storageopen(sd);
}

/*==========================================
 * NPCで経験値上げる
 *------------------------------------------
 */
void builtin_getexp(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t base = 0, job = 0;

    base = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    job = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    if (base < 0 || job < 0)
        return;
    if (sd)
        pc_gainexp_reason(sd, base, job, PC_GAINEXP_REASON::SCRIPT);
}

/*==========================================
 * モンスター発生
 *------------------------------------------
 */
void builtin_monster(struct script_state *st)
{
    fixed_string<16> map;
    map.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    uint16_t x = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    uint16_t y = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    const char *str = conv_str(st, &(st->stack->stack_data[st->start + 5]));
    int32_t mob_class = conv_num(st, &(st->stack->stack_data[st->start + 6]));
    int32_t amount = conv_num(st, &(st->stack->stack_data[st->start + 7]));
    const char *event = "";
    if (st->end > st->start + 8)
        event = conv_str(st, &(st->stack->stack_data[st->start + 8]));

    mob_once_spawn(map_id2sd(st->rid), {map, x, y}, str, mob_class, amount, event);
}

/*==========================================
 * モンスター発生
 *------------------------------------------
 */
void builtin_areamonster(struct script_state *st)
{
    int32_t mob_class, amount, x_0, y_0, x_1, y_1;
    const char *str, *event = "";

    fixed_string<16> map;
    map.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    x_0 = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    y_0 = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    x_1 = conv_num(st, &(st->stack->stack_data[st->start + 5]));
    y_1 = conv_num(st, &(st->stack->stack_data[st->start + 6]));
    str = conv_str(st, &(st->stack->stack_data[st->start + 7]));
    mob_class = conv_num(st, &(st->stack->stack_data[st->start + 8]));
    amount = conv_num(st, &(st->stack->stack_data[st->start + 9]));
    if (st->end > st->start + 10)
        event = conv_str(st, &(st->stack->stack_data[st->start + 10]));

    mob_once_spawn_area(map_id2sd(st->rid), map, x_0, y_0, x_1, y_1, str, mob_class,
                         amount, event);
}

/*==========================================
 * モンスター削除
 *------------------------------------------
 */
static void builtin_killmonster_sub(BlockList *bl, const char *event, bool allflag)
{
    struct mob_data *md = static_cast<struct mob_data *>(bl);
    if (!allflag)
    {
        if (strcmp(event, md->npc_event) == 0)
            mob_delete(md);
        return;
    }
    else
    {
        if (md->spawndelay_1 == -1 && md->spawndelay2 == -1)
            mob_delete(md);
        return;
    }
}

void builtin_killmonster(struct script_state *st)
{
    const char *event;
    int32_t m;
    fixed_string<16> mapname;
    mapname.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    event = conv_str(st, &(st->stack->stack_data[st->start + 3]));
    bool allflag = strcmp(event, "All") == 0;

    if ((m = map_mapname2mapid(mapname)) < 0)
        return;
    map_foreachinarea(builtin_killmonster_sub,
                      m, 0, 0, maps[m].xs, maps[m].ys, BL_MOB, event, allflag);
}

static void builtin_killmonsterall_sub(BlockList *bl)
{
    mob_delete(static_cast<struct mob_data *>(bl));
}

void builtin_killmonsterall(struct script_state *st)
{
    int32_t m;
    fixed_string<16> mapname;
    mapname.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));

    if ((m = map_mapname2mapid(mapname)) < 0)
        return;
    map_foreachinarea(builtin_killmonsterall_sub,
                      m, 0, 0, maps[m].xs, maps[m].ys, BL_MOB);
}

/*==========================================
 * イベント実行
 *------------------------------------------
 */
void builtin_doevent(struct script_state *st)
{
    const char *event;
    event = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    npc_event(map_id2sd(st->rid), event, 0);
}

/*==========================================
 * NPC主体イベント実行
 *------------------------------------------
 */
void builtin_donpcevent(struct script_state *st)
{
    const char *event;
    event = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    npc_event_do(event);
}

/*==========================================
 * イベントタイマー追加
 *------------------------------------------
 */
void builtin_addtimer(struct script_state *st)
{
    const char *event;
    int32_t tick;
    tick = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    event = conv_str(st, &(st->stack->stack_data[st->start + 3]));
    pc_addeventtimer(script_rid2sd(st), tick, event);
}

/*==========================================
 * イベントタイマー削除
 *------------------------------------------
 */
void builtin_deltimer(struct script_state *st)
{
    const char *event;
    event = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    pc_deleventtimer(script_rid2sd(st), event);
}

/*==========================================
 * NPCタイマー初期化
 *------------------------------------------
 */
void builtin_initnpctimer(struct script_state *st)
{
    struct npc_data_script *nd = static_cast<struct npc_data_script *>(map_id2bl(st->oid));

    npc_settimerevent_tick(nd, 0);
    npc_timerevent_start(nd);
}

/*==========================================
 * NPCタイマー開始
 *------------------------------------------
 */
void builtin_startnpctimer(struct script_state *st)
{
    struct npc_data_script *nd = static_cast<struct npc_data_script *>(map_id2bl(st->oid));

    npc_timerevent_start(nd);
}

/*==========================================
 * NPCタイマー停止
 *------------------------------------------
 */
void builtin_stopnpctimer(struct script_state *st)
{
    struct npc_data_script *nd = static_cast<struct npc_data_script *>(map_id2bl(st->oid));

    npc_timerevent_stop(nd);
}

/*==========================================
 * NPCタイマー情報所得
 *------------------------------------------
 */
void builtin_getnpctimer(struct script_state *st)
{
    int32_t type = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    int32_t val = 0;
    struct npc_data_script *nd = static_cast<struct npc_data_script *>(map_id2bl(st->oid));

    switch (type)
    {
        case 0:
            val = npc_gettimerevent_tick(nd);
            break;
        case 1:
            val = (nd->scr.nexttimer >= 0);
            break;
        case 2:
            val = nd->scr.timeramount;
            break;
    }
    push_val(st->stack, C_INT, val);
}

/*==========================================
 * NPCタイマー値設定
 *------------------------------------------
 */
void builtin_setnpctimer(struct script_state *st)
{
    int32_t tick = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    struct npc_data_script *nd = static_cast<struct npc_data_script *>(map_id2bl(st->oid));

    npc_settimerevent_tick(nd, tick);
}

/*==========================================
 * 天の声アナウンス
 *------------------------------------------
 */
void builtin_announce(struct script_state *st)
{
    const char *str;
    int32_t flag;
    str = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    flag = conv_num(st, &(st->stack->stack_data[st->start + 3]));

    if (flag & 0x0f)
    {
        BlockList *bl = (flag & 0x08)
                ? map_id2bl(st->oid)
                : script_rid2sd(st);
        clif_GMmessage(bl, str, strlen(str) + 1, flag);
    }
    else
        intif_GMmessage(str, strlen(str) + 1);
}

/*==========================================
 * 天の声アナウンス（特定マップ）
 *------------------------------------------
 */
static void builtin_mapannounce_sub(BlockList *bl, const char *str, size_t len, int32_t flag)
{
    clif_GMmessage(bl, str, len, flag | 3);
}

void builtin_mapannounce(struct script_state *st)
{
    const char *str;
    int32_t flag, m;

    fixed_string<16> mapname;
    mapname.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    str = conv_str(st, &(st->stack->stack_data[st->start + 3]));
    flag = conv_num(st, &(st->stack->stack_data[st->start + 4]));

    if ((m = map_mapname2mapid(mapname)) < 0)
        return;
    map_foreachinarea(builtin_mapannounce_sub,
                      m, 0, 0, maps[m].xs, maps[m].ys, BL_PC, str,
                      strlen(str) + 1, flag & 0x10);
}

/*==========================================
 * 天の声アナウンス（特定エリア）
 *------------------------------------------
 */
void builtin_areaannounce(struct script_state *st)
{
    const char *str;
    int32_t flag, m;
    int32_t x_0, y_0, x_1, y_1;

    fixed_string<16> map;
    map.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    x_0 = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    y_0 = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    x_1 = conv_num(st, &(st->stack->stack_data[st->start + 5]));
    y_1 = conv_num(st, &(st->stack->stack_data[st->start + 6]));
    str = conv_str(st, &(st->stack->stack_data[st->start + 7]));
    flag = conv_num(st, &(st->stack->stack_data[st->start + 8]));

    if ((m = map_mapname2mapid(map)) < 0)
        return;

    map_foreachinarea(builtin_mapannounce_sub,
                      m, x_0, y_0, x_1, y_1, BL_PC, str, strlen(str) + 1,
                      flag & 0x10);
}

/*==========================================
 * ユーザー数所得
 *------------------------------------------
 */
void builtin_getusers(struct script_state *st)
{
    int32_t flag = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    BlockList *bl = map_id2bl((flag & 0x08) ? st->oid : st->rid);
    int32_t val = 0;
    switch (flag & 0x07)
    {
        case 0:
            val = maps[bl->m].users;
            break;
        case 1:
            val = map_getusers();
            break;
    }
    push_val(st->stack, C_INT, val);
}

/*==========================================
 * マップ指定ユーザー数所得
 *------------------------------------------
 */
void builtin_getmapusers(struct script_state *st)
{
    int32_t m;
    fixed_string<16> str;
    str.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    if ((m = map_mapname2mapid(str)) < 0)
    {
        push_val(st->stack, C_INT, -1);
        return;
    }
    push_val(st->stack, C_INT, maps[m].users);
}

/*==========================================
 * エリア指定ユーザー数所得
 *------------------------------------------
 */
static void builtin_getareausers_sub(BlockList *, int32_t *users)
{
    ++*users;
}

void builtin_getareausers(struct script_state *st)
{
    int32_t m, x_0, y_0, x_1, y_1, users = 0;
    fixed_string<16> str;
    str.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    x_0 = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    y_0 = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    x_1 = conv_num(st, &(st->stack->stack_data[st->start + 5]));
    y_1 = conv_num(st, &(st->stack->stack_data[st->start + 6]));
    if ((m = map_mapname2mapid(str)) < 0)
    {
        push_val(st->stack, C_INT, -1);
        return;
    }
    map_foreachinarea(builtin_getareausers_sub,
                      m, x_0, y_0, x_1, y_1, BL_PC, &users);
    push_val(st->stack, C_INT, users);
}

/*==========================================
 * エリア指定ドロップアイテム数所得
 *------------------------------------------
 */
static void builtin_getareadropitem_sub(BlockList *bl, int32_t item, int32_t *amount)
{
    struct flooritem_data *drop = static_cast<struct flooritem_data *>(bl);

    if (drop->item_data.nameid == item)
        (*amount) += drop->item_data.amount;
}

static void builtin_getareadropitem_sub_anddelete(BlockList *bl, int32_t item, int32_t *amount)
{
    struct flooritem_data *drop = static_cast<struct flooritem_data *>(bl);

    if (drop->item_data.nameid == item)
    {
        (*amount) += drop->item_data.amount;
        clif_clearflooritem(drop, -1);
        map_delobject(drop->id, drop->type);
    }
}

void builtin_getareadropitem(struct script_state *st)
{
    int32_t m, x_0, y_0, x_1, y_1, item, amount = 0, delitems = 0;
    struct script_data *data;

    fixed_string<16> str;
    str.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    x_0 = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    y_0 = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    x_1 = conv_num(st, &(st->stack->stack_data[st->start + 5]));
    y_1 = conv_num(st, &(st->stack->stack_data[st->start + 6]));

    data = &(st->stack->stack_data[st->start + 7]);
    get_val(st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str(st, data);
        struct item_data *item_data = itemdb_searchname(name);
        item = 512;
        if (item_data)
            item = item_data->nameid;
    }
    else
        item = conv_num(st, data);

    if (st->end > st->start + 8)
        delitems = conv_num(st, &(st->stack->stack_data[st->start + 8]));

    if ((m = map_mapname2mapid(str)) < 0)
    {
        push_val(st->stack, C_INT, -1);
        return;
    }
    if (delitems)
        map_foreachinarea(builtin_getareadropitem_sub_anddelete,
                          m, x_0, y_0, x_1, y_1, BL_ITEM, item, &amount);
    else
        map_foreachinarea(builtin_getareadropitem_sub,
                          m, x_0, y_0, x_1, y_1, BL_ITEM, item, &amount);

    push_val(st->stack, C_INT, amount);
}

/*==========================================
 * NPCの有効化
 *------------------------------------------
 */
void builtin_enablenpc(struct script_state *st)
{
    const char *str;
    str = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    npc_enable(str, 1);
}

/*==========================================
 * NPCの無効化
 *------------------------------------------
 */
void builtin_disablenpc(struct script_state *st)
{
    const char *str;
    str = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    npc_enable(str, 0);
}

/*==========================================
 * 隠れているNPCの表示
 *------------------------------------------
 */
void builtin_hideoffnpc(struct script_state *st)
{
    const char *str;
    str = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    npc_enable(str, 2);
}

/*==========================================
 * NPCをハイディング
 *------------------------------------------
 */
void builtin_hideonnpc(struct script_state *st)
{
    const char *str;
    str = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    npc_enable(str, 4);
}

/*==========================================
 * 状態異常にかかる
 *------------------------------------------
 */
void builtin_sc_start(struct script_state *st)
{
    BlockList *bl;
    int32_t type, tick, val1;
    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    tick = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    val1 = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    if (st->end > st->start + 5)    //指定したキャラを状態異常にする
        bl = map_id2bl(conv_num
                        (st, &(st->stack->stack_data[st->start + 5])));
    else
        bl = map_id2bl(st->rid);
    skill_status_change_start(bl, type, val1, tick);
}

/*==========================================
 * 状態異常にかかる(確率指定)
 *------------------------------------------
 */
void builtin_sc_start2(struct script_state *st)
{
    BlockList *bl;
    int32_t type, tick, val1, per;
    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    tick = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    val1 = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    per = conv_num(st, &(st->stack->stack_data[st->start + 5]));
    if (st->end > st->start + 6)    //指定したキャラを状態異常にする
        bl = map_id2bl(conv_num
                        (st, &(st->stack->stack_data[st->start + 6])));
    else
        bl = map_id2bl(st->rid);
    if (MRAND(10000) < per)
        skill_status_change_start(bl, type, val1, tick);
}

/*==========================================
 * 状態異常が直る
 *------------------------------------------
 */
void builtin_sc_end(struct script_state *st)
{
    BlockList *bl;
    int32_t type;
    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    bl = map_id2bl(st->rid);
    skill_status_change_end(bl, type, NULL);
//  if (battle_config.etc_log)
//      printf("sc_end : %d %d\n",st->rid,type);
}

void builtin_sc_check(struct script_state *st)
{
    BlockList *bl;
    int32_t type;
    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    bl = map_id2bl(st->rid);

    push_val(st->stack, C_INT, skill_status_change_active(bl, type));
}

/*==========================================
 * 状態異常耐性を計算した確率を返す
 *------------------------------------------
 */
void builtin_getscrate(struct script_state *st)
{
    BlockList *bl;
    int32_t sc_def = 100, sc_def_vit2;
    int32_t type, rate, luk;

    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    rate = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    if (st->end > st->start + 4)    //指定したキャラの耐性を計算する
        bl = map_id2bl(conv_num
                        (st, &(st->stack->stack_data[st->start + 6])));
    else
        bl = map_id2bl(st->rid);

    luk = battle_get_luk(bl);
    sc_def_vit2 = 100 - (3 + battle_get_vit(bl) + luk / 3);

    if (type == SC_POISON)
        sc_def = sc_def_vit2;

    rate = rate * sc_def / 100;
    push_val(st->stack, C_INT, rate);
}

/*==========================================
 *
 *------------------------------------------
 */
void builtin_debugmes(struct script_state *st)
{
    conv_str(st, &(st->stack->stack_data[st->start + 2]));
    printf("script debug : %d %d : %s\n", st->rid, st->oid,
            st->stack->stack_data[st->start + 2].u.str);
}

/*==========================================
 * Added - AppleGirl For Advanced Classes, (Updated for Cleaner Script Purposes)
 *------------------------------------------
 */
void builtin_resetlvl(struct script_state *st)
{
    MapSessionData *sd;

    int32_t type = conv_num(st, &(st->stack->stack_data[st->start + 2]));

    sd = script_rid2sd(st);
    pc_resetlvl(sd, type);
}

/*==========================================
 * ステータスリセット
 *------------------------------------------
 */
void builtin_resetstatus(struct script_state *st)
{
    MapSessionData *sd;
    sd = script_rid2sd(st);
    pc_resetstate(sd);
}

/*==========================================
 * スキルリセット
 *------------------------------------------
 */
void builtin_resetskill(struct script_state *st)
{
    MapSessionData *sd;
    sd = script_rid2sd(st);
    pc_resetskill(sd);
}

/*==========================================
 * 性別変換
 *------------------------------------------
 */
void builtin_changesex(struct script_state *st)
{
    MapSessionData *sd = NULL;
    sd = script_rid2sd(st);

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
    chrif_char_ask_name(-1, sd->status.name, CharOperation::CHANGE_SEX);
    chrif_save(sd);
}

/*==========================================
 * RIDのアタッチ
 *------------------------------------------
 */
void builtin_attachrid(struct script_state *st)
{
    st->rid = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    push_val(st->stack, C_INT, (map_id2sd(st->rid) != NULL));
}

/*==========================================
 * RIDのデタッチ
 *------------------------------------------
 */
void builtin_detachrid(struct script_state *st)
{
    st->rid = 0;
}

/*==========================================
 * 存在チェック
 *------------------------------------------
 */
void builtin_isloggedin(struct script_state *st)
{
    push_val(st->stack, C_INT,
              map_id2sd(conv_num
                         (st,
                          &(st->stack->stack_data[st->start + 2]))) != NULL);
}

/*==========================================
 * Note: Fixed to correspond with const.txt
 *------------------------------------------
 */
enum
{
    MF_NOMEMO = 0, MF_NOTELEPORT = 1, MF_NOSAVE = 2, MF_NOBRANCH = 3,
    MF_NOPENALTY = 4, MF_PVP = 5, MF_PVP_NOPARTY = 6,
    MF_NOZENYPENALTY = 10,
    // not in const.txt but could be useful
    MF_NOTRADE, MF_NOWARP, MF_NOPVP,
};

void builtin_setmapflagnosave(struct script_state *st)
{
    int32_t m, x, y;
    fixed_string<16> str, str2;
    str.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    str2.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 3])));
    x = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    y = conv_num(st, &(st->stack->stack_data[st->start + 5]));
    m = map_mapname2mapid(str);
    if (m >= 0)
    {
        maps[m].flag.nosave = 1;
        maps[m].save.map = str2;
        maps[m].save.x = x;
        maps[m].save.y = y;
    }
}

void builtin_setmapflag(struct script_state *st)
{
    int32_t m, i;
    fixed_string<16> str;
    str.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    i = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    m = map_mapname2mapid(str);
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
}

void builtin_removemapflag(struct script_state *st)
{
    int32_t m, i;

    fixed_string<16> str;
    str.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    i = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    m = map_mapname2mapid(str);
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
}

void builtin_pvpon(struct script_state *st)
{
    int32_t m;

    fixed_string<16> str;
    str.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    m = map_mapname2mapid(str);
    if (m >= 0 && !maps[m].flag.pvp && !maps[m].flag.nopvp)
    {
        maps[m].flag.pvp = 1;
        if (battle_config.pk_mode)  // disable ranking functions if pk_mode is on [Valaris]
            return;

        for (MapSessionData *pl_sd : auth_sessions)
        {
            if (m == pl_sd->m && pl_sd->pvp_timer == NULL)
            {
                pl_sd->pvp_timer = add_timer(gettick() + 200, pc_calc_pvprank_timer, pl_sd->id);
                pl_sd->pvp_rank = 0;
                pl_sd->pvp_lastusers = 0;
                pl_sd->pvp_point = 5;
            }
        }
    }
}

void builtin_pvpoff(struct script_state *st)
{
    int32_t m;

    fixed_string<16> str;
    str.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    m = map_mapname2mapid(str);
    if (m >= 0 && maps[m].flag.pvp && maps[m].flag.nopvp)
    {
        maps[m].flag.pvp = 0;
        if (battle_config.pk_mode)  // disable ranking options if pk_mode is on [Valaris]
            return;

        for (MapSessionData *pl_sd : auth_sessions)
        {
            if (m == pl_sd->m)
            {
                if (pl_sd->pvp_timer)
                {
                    delete_timer(pl_sd->pvp_timer);
                    pl_sd->pvp_timer = NULL;
                }
            }
        }
    }
}

/*==========================================
 *      NPCエモーション
 *------------------------------------------
 */

void builtin_emotion(struct script_state *st)
{
    int32_t type;
    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    if (type < 0 || type > 100)
        return;
    clif_emotion(map_id2bl(st->oid), type);
}

void builtin_mapwarp(struct script_state *st)   // Added by RoVeRT
{
    fixed_string<16> src_map;
    src_map.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    int32_t m = map_mapname2mapid(src_map);
    if (m < 0)
        return;

    int32_t x_0 = 0;
    int32_t y_0 = 0;
    int32_t x_1 = maps[m].xs;
    int32_t y_1 = maps[m].ys;
    fixed_string<16> dst_map;
    dst_map.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 3])));
    int16_t x = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    int16_t y = conv_num(st, &(st->stack->stack_data[st->start + 5]));

    map_foreachinarea(builtin_areawarp_sub,
                      m, x_0, y_0, x_1, y_1, BL_PC, Point{dst_map, x, y});
}

void builtin_cmdothernpc(struct script_state *st)   // Added by RoVeRT
{
    const char *npc, *command;

    npc = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    command = conv_str(st, &(st->stack->stack_data[st->start + 3]));

    npc_command(map_id2sd(st->rid), npc, command);
}

void builtin_inittimer(struct script_state *st) // Added by RoVeRT
{
//  struct npc_data *nd=(struct npc_data*)map_id2bl(st->oid);

//  nd->lastaction=nd->timer=gettick();
    npc_do_ontimer(st->oid, map_id2sd(st->rid), 1);
}

void builtin_stoptimer(struct script_state *st) // Added by RoVeRT
{
//  struct npc_data *nd=(struct npc_data*)map_id2bl(st->oid);

//  nd->lastaction=nd->timer=-1;
    npc_do_ontimer(st->oid, map_id2sd(st->rid), 0);
}

static void builtin_mobcount_sub(BlockList *bl, const char *event, int32_t *c)    // Added by RoVeRT
{
    if (strcmp(event, static_cast<struct mob_data *>(bl)->npc_event) == 0)
        ++*c;
}

void builtin_mobcount(struct script_state *st)  // Added by RoVeRT
{
    const char *event;
    int32_t m, c = 0;
    fixed_string<16> mapname;
    mapname.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    event = conv_str(st, &(st->stack->stack_data[st->start + 3]));

    if ((m = map_mapname2mapid(mapname)) < 0)
    {
        push_val(st->stack, C_INT, -1);
        return;
    }
    map_foreachinarea(builtin_mobcount_sub,
                      m, 0, 0, maps[m].xs, maps[m].ys, BL_MOB, event, &c);

    push_val(st->stack, C_INT, (c - 1));
}

void builtin_marriage(struct script_state *st)
{
    const char *partner = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    MapSessionData *sd = script_rid2sd(st);
    MapSessionData *p_sd = map_nick2sd(partner);

    if (sd == NULL || p_sd == NULL || pc_marriage(sd, p_sd) < 0)
    {
        push_val(st->stack, C_INT, 0);
        return;
    }
    push_val(st->stack, C_INT, 1);
}

void builtin_divorce(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);

    st->state = STOP;           // rely on pc_divorce to restart

    sd->npc_flags.divorce = 1;

    if (sd == NULL || pc_divorce(sd) < 0)
    {
        push_val(st->stack, C_INT, 0);
        return;
    }

    push_val(st->stack, C_INT, 1);
}

/*================================================
 * Script for Displaying MOB Information [Valaris]
 *------------------------------------------------
 */
void builtin_strmobinfo(struct script_state *st)
{

    int32_t num = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    int32_t mob_class = conv_num(st, &(st->stack->stack_data[st->start + 3]));

    if (num <= 0 || num >= 8 || (mob_class >= 0 && mob_class <= 1000) || mob_class > 2000)
        return;

    if (num == 1)
    {
        char *buf;
        buf = mob_db[mob_class].name;
        push_str(st->stack, C_STR, buf);
        return;
    }
    else if (num == 2)
    {
        char *buf;
        buf = mob_db[mob_class].jname;
        push_str(st->stack, C_STR, buf);
        return;
    }
    else if (num == 3)
        push_val(st->stack, C_INT, mob_db[mob_class].lv);
    else if (num == 4)
        push_val(st->stack, C_INT, mob_db[mob_class].max_hp);
    else if (num == 5)
        push_val(st->stack, C_INT, mob_db[mob_class].max_sp);
    else if (num == 6)
        push_val(st->stack, C_INT, mob_db[mob_class].base_exp);
    else if (num == 7)
        push_val(st->stack, C_INT, mob_db[mob_class].job_exp);
}

/*==========================================
 * IDからItem名
 *------------------------------------------
 */
void builtin_getitemname(struct script_state *st)
{
    struct item_data *i_data;
    char *item_name;
    struct script_data *data;

    data = &(st->stack->stack_data[st->start + 2]);
    get_val(st, data);
    if (data->type == C_STR || data->type == C_CONSTSTR)
    {
        const char *name = conv_str(st, data);
        i_data = itemdb_searchname(name);
    }
    else
    {
        int32_t item_id = conv_num(st, data);
        i_data = itemdb_search(item_id);
    }

    CREATE(item_name, char, 24);
    if (i_data)
        strncpy(item_name, i_data->jname, 23);
    else
        strncpy(item_name, "Unknown Item", 23);

    push_str(st->stack, C_STR, item_name);
}

void builtin_getspellinvocation(struct script_state *st)
{
    POD_string name = NULL;
    name.assign(conv_str(st, &(st->stack->stack_data[st->start + 2])));

    POD_string invocation = magic_find_invocation(name);
    if (!invocation)
        invocation.assign("...");

    push_str(st->stack, C_STR, strdup(invocation.c_str()));
    name.free();
    invocation.free();
}

void builtin_getanchorinvocation(struct script_state *st)
{
    POD_string name = NULL;
    name.assign(conv_str(st, &(st->stack->stack_data[st->start + 2])));

    POD_string invocation = magic_find_anchor_invocation(name);
    if (!invocation)
        invocation.assign("...");

    push_str(st->stack, C_STR, strdup(invocation.c_str()));
    name.free();
    invocation.free();
}

void builtin_getpartnerid2(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);

    push_val(st->stack, C_INT, sd->status.partner_id);
}

/*==========================================
 * PCの所持品情報読み取り
 *------------------------------------------
 */
void builtin_getinventorylist(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    if (!sd)
        return;

    int32_t j = 0;
    for (int32_t i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].nameid > 0
            && sd->status.inventory[i].amount > 0)
        {
            pc_setreg(sd, add_str("@inventorylist_id") + (j << 24),
                      sd->status.inventory[i].nameid);
            pc_setreg(sd, add_str("@inventorylist_amount") + (j << 24),
                      sd->status.inventory[i].amount);
            pc_setreg(sd, add_str("@inventorylist_equip") + (j << 24),
                      static_cast<uint16_t>(sd->status.inventory[i].equip));
            j++;
        }
    }
    pc_setreg(sd, add_str("@inventorylist_count"), j);
}

void builtin_getskilllist(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t i, j = 0;
    if (!sd)
        return;
    for (i = 0; i < MAX_SKILL; i++)
    {
        if (sd->status.skill[i].id > 0 && sd->status.skill[i].lv > 0)
        {
            pc_setreg(sd, add_str("@skilllist_id") + (j << 24),
                       sd->status.skill[i].id);
            pc_setreg(sd, add_str("@skilllist_lv") + (j << 24),
                       sd->status.skill[i].lv);
            pc_setreg(sd, add_str("@skilllist_flag") + (j << 24),
                       sd->status.skill[i].flags);
            j++;
        }
    }
    pc_setreg(sd, add_str("@skilllist_count"), j);
}

void builtin_getactivatedpoolskilllist(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t pool_skills[MAX_SKILL_POOL];
    int32_t pool_size = skill_pool(sd, pool_skills);
    int32_t i, count = 0;

    if (!sd)
        return;

    for (i = 0; i < pool_size; i++)
    {
        int32_t skill_id = pool_skills[i];

        if (sd->status.skill[skill_id].id == skill_id)
        {
            pc_setreg(sd, add_str("@skilllist_id") + (count << 24),
                       sd->status.skill[skill_id].id);
            pc_setreg(sd, add_str("@skilllist_lv") + (count << 24),
                       sd->status.skill[skill_id].lv);
            pc_setreg(sd, add_str("@skilllist_flag") + (count << 24),
                       sd->status.skill[skill_id].flags);
            pc_setregstr(sd, add_str("@skilllist_name$") + (count << 24),
                          skill_name(skill_id));
            ++count;
        }
    }
    pc_setreg(sd, add_str("@skilllist_count"), count);
}

void builtin_getunactivatedpoolskilllist(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t i, count = 0;

    if (!sd)
        return;

    for (i = 0; i < skill_pool_skills_size; i++)
    {
        int32_t skill_id = skill_pool_skills[i];

        if (sd->status.skill[skill_id].id == skill_id && !(sd->status.skill[skill_id].flags & SKILL_POOL_ACTIVATED))
        {
            pc_setreg(sd, add_str("@skilllist_id") + (count << 24),
                       sd->status.skill[skill_id].id);
            pc_setreg(sd, add_str("@skilllist_lv") + (count << 24),
                       sd->status.skill[skill_id].lv);
            pc_setreg(sd, add_str("@skilllist_flag") + (count << 24),
                       sd->status.skill[skill_id].flags);
            pc_setregstr(sd, add_str("@skilllist_name$") + (count << 24),
                          skill_name(skill_id));
            ++count;
        }
    }
    pc_setreg(sd, add_str("@skilllist_count"), count);
}

void builtin_getpoolskilllist(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t i, count = 0;

    if (!sd)
        return;

    for (i = 0; i < skill_pool_skills_size; i++)
    {
        int32_t skill_id = skill_pool_skills[i];

        if (sd->status.skill[skill_id].id == skill_id)
        {
            pc_setreg(sd, add_str("@skilllist_id") + (count << 24),
                       sd->status.skill[skill_id].id);
            pc_setreg(sd, add_str("@skilllist_lv") + (count << 24),
                       sd->status.skill[skill_id].lv);
            pc_setreg(sd, add_str("@skilllist_flag") + (count << 24),
                       sd->status.skill[skill_id].flags);
            pc_setregstr(sd, add_str("@skilllist_name$") + (count << 24),
                          skill_name(skill_id));
            ++count;
        }
    }
    pc_setreg(sd, add_str("@skilllist_count"), count);
}

void builtin_poolskill(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t skill_id = conv_num(st, &(st->stack->stack_data[st->start + 2]));

    skill_pool_activate(sd, skill_id);
    clif_skillinfoblock(sd);
}

void builtin_unpoolskill(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t skill_id = conv_num(st, &(st->stack->stack_data[st->start + 2]));

    skill_pool_deactivate(sd, skill_id);
    clif_skillinfoblock(sd);
}

void builtin_checkpoolskill(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t skill_id = conv_num(st, &(st->stack->stack_data[st->start + 2]));

    push_val(st->stack, C_INT, skill_pool_is_activated(sd, skill_id));
}

void builtin_clearitem(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    int32_t i;
    if (sd == NULL)
        return;
    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].amount)
            pc_delitem(sd, i, sd->status.inventory[i].amount, 0);
    }
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
void builtin_misceffect(struct script_state *st)
{
    int32_t type;
    int32_t id = 0;
    const char *name = NULL;
    BlockList *bl = NULL;

    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));

    if (st->end > st->start + 3)
    {
        struct script_data *sdata = &(st->stack->stack_data[st->start + 3]);

        get_val(st, sdata);

        if (sdata->type == C_STR || sdata->type == C_CONSTSTR)
            name = conv_str(st, sdata);
        else
            id = conv_num(st, sdata);
    }

    if (name)
    {
        MapSessionData *sd = map_nick2sd(name);
        if (sd)
            bl = sd;
    }
    else if (id)
        bl = map_id2bl(id);
    else if (st->oid)
        bl = map_id2bl(st->oid);
    else
    {
        MapSessionData *sd = script_rid2sd(st);
        if (sd)
            bl = sd;
    }

    if (bl)
        clif_misceffect(bl, type);
}

/*==========================================
 * サウンドエフェクト
 *------------------------------------------
 */
void builtin_soundeffect(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    const char *name;
    int32_t type = 0;

    name = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    type = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    if (sd)
    {
        if (st->oid)
            clif_soundeffect(sd, map_id2bl(st->oid), name, type);
        else
        {
            clif_soundeffect(sd, sd, name, type);
        }
    }
}

/*==========================================
 * Special effects [Valaris]
 *------------------------------------------
 */
void builtin_specialeffect(struct script_state *st)
{
    BlockList *bl = map_id2bl(st->oid);

    if (bl == NULL)
        return;

    clif_specialeffect(bl,
                        conv_num(st,
                                  &(st->stack->stack_data[st->start + 2])),
                        0);
}

void builtin_specialeffect2(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);

    if (sd == NULL)
        return;

    clif_specialeffect(sd,
                        conv_num(st,
                                  &(st->stack->stack_data[st->start + 2])),
                        0);
}

/*==========================================
 * Nude [Valaris]
 *------------------------------------------
 */

void builtin_nude(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);

    if (!sd)
        return;

    for (EQUIP i : EQUIPs)
        if (sd->equip_index[i] >= 0)
            pc_unequipitem(sd, sd->equip_index[i], i != EQUIP::MISC2);
    pc_calcstatus(sd, 0);
}

/*==========================================
 * UnequipById [Freeyorp]
 *------------------------------------------
 */

void builtin_unequipbyid(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    if (sd == NULL)
        return;

    EQUIP slot_id = static_cast<EQUIP>(conv_num(st, &(st->stack->stack_data[st->start + 2])));

    if (sd->equip_index[slot_id] >= 0)
        pc_unequipitem(sd, sd->equip_index[slot_id], slot_id != EQUIP::MISC2);

    pc_calcstatus(sd, 0);
}

/*==========================================
 * gmcommand [MouseJstr]
 *
 * suggested on the forums...
 *------------------------------------------
 */

void builtin_gmcommand(struct script_state *st)
{
    MapSessionData *sd;
    const char *cmd;

    sd = script_rid2sd(st);
    cmd = conv_str(st, &(st->stack->stack_data[st->start + 2]));

    is_atcommand(sd->fd, sd, cmd, 99);
}

/*==========================================
 * movenpc [MouseJstr]
 *------------------------------------------
 */
void builtin_movenpc(struct script_state *st)
{
//     MapSessionData *sd = script_rid2sd(st);

//     const char *map = conv_str (st, &(st->stack->stack_data[st->start + 2]));
    int32_t x = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    int32_t y = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    const char *npc = conv_str(st, &(st->stack->stack_data[st->start + 5]));

    struct npc_data *nd = npc_name2id(npc);
    if (!nd)
        return;

    npc_enable(npc, 0);
    nd->x = x;
    nd->y = y;
    npc_enable(npc, 1);
}

/*==========================================
 * npcwarp [remoitnane]
 * Move NPC to a new position on the same map.
 *------------------------------------------
 */
void builtin_npcwarp(struct script_state *st)
{
    int32_t x, y;
    const char *npc;
    struct npc_data *nd = NULL;

    x = conv_num(st, &(st->stack->stack_data[st->start + 2]));
    y = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    npc = conv_str(st, &(st->stack->stack_data[st->start + 4]));
    nd = npc_name2id(npc);

    if (!nd)
        return;

    int16_t m = nd->m;

    /* Crude sanity checks. */
    if (m < 0 || !nd->prev
            || x < 0 || x > maps[m].xs -1
            || y < 0 || y > maps[m].ys - 1)
        return;

    npc_enable(npc, 0);
    map_delblock(nd); /* [Freeyorp] */
    nd->x = x;
    nd->y = y;
    map_addblock(nd);
    npc_enable(npc, 1);
}

/*==========================================
 * message [MouseJstr]
 *------------------------------------------
 */

void builtin_message(struct script_state *st)
{
    const char *msg, *player;
    MapSessionData *pl_sd = NULL;

    player = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    msg = conv_str(st, &(st->stack->stack_data[st->start + 3]));

    if ((pl_sd = map_nick2sd(player)) == NULL)
        return;
    clif_displaymessage(pl_sd->fd, msg);
}

/*==========================================
 * npctalk(sends message to surrounding
 * area) [Valaris]
 *------------------------------------------
 */

void builtin_npctalk(struct script_state *st)
{
    const char *str;
    char message[255];

    struct npc_data *nd = static_cast<struct npc_data *>(map_id2bl(st->oid));
    str = conv_str(st, &(st->stack->stack_data[st->start + 2]));

    if (nd)
    {
        memcpy(message, nd->name, 24);
        strcat(message, " : ");
        strcat(message, str);
        clif_message(nd, message);
    }
}

/*==========================================
 * hasitems(checks to see if player has any
 * items on them, if so will return a 1)
 * [Valaris]
 *------------------------------------------
 */

void builtin_hasitems(struct script_state *st)
{
    int32_t i;
    MapSessionData *sd;

    sd = script_rid2sd(st);

    for (i = 0; i < MAX_INVENTORY; i++)
    {
        if (sd->status.inventory[i].amount)
        {
            push_val(st->stack, C_INT, 1);
            return;
        }
    }

    push_val(st->stack, C_INT, 0);
}

/*==========================================
  * getlook char info. getlook(arg)
  *------------------------------------------
  */
void builtin_getlook(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);

    LOOK type = static_cast<LOOK>(conv_num(st, &(st->stack->stack_data[st->start + 2])));
    int32_t val = -1;
    switch (type)
    {
        case LOOK::HAIR:        //1
            val = sd->status.hair;
            break;
        case LOOK::WEAPON:      //2
            val = sd->status.weapon;
            break;
        case LOOK::LEGS: //3
            val = sd->status.legs;
            break;
        case LOOK::HEAD:    //4
            val = sd->status.head;
            break;
        case LOOK::CHEST:    //5
            val = sd->status.chest;
            break;
        case LOOK::HAIR_COLOR:  //6
            val = sd->status.hair_color;
            break;
        case LOOK::SHIELD:      //8
            val = sd->status.shield;
            break;
        case LOOK::SHOES:       //9
            break;
    }

    push_val(st->stack, C_INT, val);
}

/*==========================================
  *     get char save point. argument: 0- map name, 1- x, 2- y
  *------------------------------------------
*/
void builtin_getsavepoint(struct script_state *st)
{
    int32_t x, y, type;
    char *mapname;
    MapSessionData *sd;

    sd = script_rid2sd(st);

    type = conv_num(st, &(st->stack->stack_data[st->start + 2]));

    x = sd->status.save_point.x;
    y = sd->status.save_point.y;
    switch (type)
    {
        case 0:
            CREATE(mapname, char, 16);
            sd->status.save_point.map.write_to(mapname);
            push_str(st->stack, C_STR, mapname);
            break;
        case 1:
            push_val(st->stack, C_INT, x);
            break;
        case 2:
            push_val(st->stack, C_INT, y);
            break;
    }
}

/*==========================================
 *     areatimer
 *------------------------------------------
 */
static void builtin_areatimer_sub(BlockList *bl, int32_t tick, const char *event)
{
    pc_addeventtimer(static_cast<MapSessionData *>(bl), tick, event);
}

void builtin_areatimer(struct script_state *st)
{
    int32_t tick, m;
    const char *event;
    int32_t x_0, y_0, x_1, y_1;

    fixed_string<16> mapname;
    mapname.copy_from(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    x_0 = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    y_0 = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    x_1 = conv_num(st, &(st->stack->stack_data[st->start + 5]));
    y_1 = conv_num(st, &(st->stack->stack_data[st->start + 6]));
    tick = conv_num(st, &(st->stack->stack_data[st->start + 7]));
    event = conv_str(st, &(st->stack->stack_data[st->start + 8]));

    if ((m = map_mapname2mapid(mapname)) < 0)
        return;

    map_foreachinarea(builtin_areatimer_sub,
                      m, x_0, y_0, x_1, y_1, BL_PC, tick, event);
}

/*==========================================
 * Check whether the PC is in the specified rectangle
 *------------------------------------------
 */
void builtin_isin(struct script_state *st)
{
    int32_t x_1, y_1, x2, y2;
    const char *str;
    MapSessionData *sd = script_rid2sd(st);

    str = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    x_1 = conv_num(st, &(st->stack->stack_data[st->start + 3]));
    y_1 = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    x2 = conv_num(st, &(st->stack->stack_data[st->start + 5]));
    y2 = conv_num(st, &(st->stack->stack_data[st->start + 6]));

    if (!sd)
        return;

    push_val(st->stack, C_INT,
              (sd->x >= x_1 && sd->x <= x2)
              && (sd->y >= y_1 && sd->y <= y2)
              && (!strcmp(str, &maps[sd->m].name)));
}

// Trigger the shop on a (hopefully) nearby shop NPC
void builtin_shop(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);
    struct npc_data *nd;

    if (!sd)
        return;

    nd = npc_name2id(conv_str(st, &(st->stack->stack_data[st->start + 2])));
    if (!nd)
        return;

    builtin_close(st);
    clif_npcbuysell(sd, nd->id);
}

/*==========================================
 * Check whether the PC is dead
 *------------------------------------------
 */
void builtin_isdead(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);

    push_val(st->stack, C_INT, pc_isdead(sd));
}

/*========================================
 * Changes a NPC name, and sprite
 *----------------------------------------
 */
void builtin_fakenpcname(struct script_state *st)
{
    const char *name, *newname;
    int32_t newsprite;
    struct npc_data *nd;

    name = conv_str(st, &(st->stack->stack_data[st->start + 2]));
    newname = conv_str(st, &(st->stack->stack_data[st->start + 3]));
    newsprite = conv_num(st, &(st->stack->stack_data[st->start + 4]));
    nd = npc_name2id(name);
    if (!nd)
        return;
    strncpy(nd->name, newname, sizeof(nd->name)-1);
    nd->name[sizeof(nd->name)-1] = '\0';
    nd->npc_class = newsprite;

    // Refresh this npc
    npc_enable(name, 0);
    npc_enable(name, 1);
}

/*============================
 * Gets the PC's x pos
 *----------------------------
 */

void builtin_getx(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);

    push_val(st->stack, C_INT, sd->x);
}

/*============================
 * Gets the PC's y pos
 *----------------------------
 */
void builtin_gety(struct script_state *st)
{
    MapSessionData *sd = script_rid2sd(st);

    push_val(st->stack, C_INT, sd->y);
}

//
// 実行部main
//
/*==========================================
 * コマンドの読み取り
 *------------------------------------------
 */
static int32_t get_com(const char *script, int32_t *pos)
{
    int32_t i, j;
    if (static_cast<uint8_t>(script[*pos]) >= 0x80)
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
static int32_t get_num(const char *script, int32_t *pos)
{
    int32_t i, j;
    i = 0;
    j = 0;
    while (static_cast<uint8_t>(script[*pos]) >= 0xc0)
    {
        // why is this mask 0x7f instead of 0x3f?
        i += (script[(*pos)++] & 0x7f) << j;
        j += 6;
    }
    return i + ((script[(*pos)++] & 0x7f) << j);
}

/*==========================================
 * スタックから値を取り出す
 *------------------------------------------
 */
static int32_t pop_val(struct script_state *st)
{
    if (st->stack->sp <= 0)
        return 0;
    st->stack->sp--;
    get_val(st, &(st->stack->stack_data[st->stack->sp]));
    if (st->stack->stack_data[st->stack->sp].type == C_INT)
        return st->stack->stack_data[st->stack->sp].u.num;
    return 0;
}

#define isstr(c) ((c).type==C_STR || (c).type==C_CONSTSTR)

/*==========================================
 * 加算演算子
 *------------------------------------------
 */
static void op_add(struct script_state *st)
{
    st->stack->sp--;
    get_val(st, &(st->stack->stack_data[st->stack->sp]));
    get_val(st, &(st->stack->stack_data[st->stack->sp - 1]));

    if (isstr(st->stack->stack_data[st->stack->sp])
        || isstr(st->stack->stack_data[st->stack->sp - 1]))
    {
        conv_str(st, &(st->stack->stack_data[st->stack->sp]));
        conv_str(st, &(st->stack->stack_data[st->stack->sp - 1]));
    }
    if (st->stack->stack_data[st->stack->sp].type == C_INT)
    {                           // ii
        st->stack->stack_data[st->stack->sp - 1].u.num +=
            st->stack->stack_data[st->stack->sp].u.num;
    }
    else
    {                           // ssの予定
        char *buf;
        CREATE(buf, char,
               strlen(st->stack->stack_data[st->stack->sp - 1].u.str)
                   + strlen(st->stack->stack_data[st->stack->sp].u.str) + 1);
        strcpy(buf, st->stack->stack_data[st->stack->sp - 1].u.str);
        strcat(buf, st->stack->stack_data[st->stack->sp].u.str);
        if (st->stack->stack_data[st->stack->sp - 1].type == C_STR)
            free(const_cast<char *>(st->stack->stack_data[st->stack->sp - 1].u.str));
        if (st->stack->stack_data[st->stack->sp].type == C_STR)
            free(const_cast<char *>(st->stack->stack_data[st->stack->sp].u.str));
        st->stack->stack_data[st->stack->sp - 1].type = C_STR;
        st->stack->stack_data[st->stack->sp - 1].u.str = buf;
    }
}

/*==========================================
 * 二項演算子(文字列)
 *------------------------------------------
 */
static void op_2str(struct script_state *st, int32_t op, int32_t sp1, int32_t sp2)
{
    const char *s1 = st->stack->stack_data[sp1].u.str,
        *s2 = st->stack->stack_data[sp2].u.str;
    int32_t a = 0;

    switch (op)
    {
        case C_EQ:
            a = (strcmp(s1, s2) == 0);
            break;
        case C_NE:
            a = (strcmp(s1, s2) != 0);
            break;
        case C_GT:
            a = (strcmp(s1, s2) > 0);
            break;
        case C_GE:
            a = (strcmp(s1, s2) >= 0);
            break;
        case C_LT:
            a = (strcmp(s1, s2) < 0);
            break;
        case C_LE:
            a = (strcmp(s1, s2) <= 0);
            break;
        default:
            printf("illegal string operater\n");
            break;
    }

    push_val(st->stack, C_INT, a);

    if (st->stack->stack_data[sp1].type == C_STR)
        free(const_cast<char *>(s1));
    if (st->stack->stack_data[sp2].type == C_STR)
        free(const_cast<char *>(s2));
}

/*==========================================
 * 二項演算子(数値)
 *------------------------------------------
 */
static void op_2num(struct script_state *st, int32_t op, int32_t i1, int32_t i2)
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
    push_val(st->stack, C_INT, i1);
}

/*==========================================
 * 二項演算子
 *------------------------------------------
 */
static void op_2(struct script_state *st, int32_t op)
{
    int32_t i1, i2;
    const char *s1 = NULL, *s2 = NULL;

    i2 = pop_val(st);
    if (isstr(st->stack->stack_data[st->stack->sp]))
        s2 = st->stack->stack_data[st->stack->sp].u.str;

    i1 = pop_val(st);
    if (isstr(st->stack->stack_data[st->stack->sp]))
        s1 = st->stack->stack_data[st->stack->sp].u.str;

    if (s1 != NULL && s2 != NULL)
    {
        // ss => op_2str
        op_2str(st, op, st->stack->sp, st->stack->sp + 1);
    }
    else if (s1 == NULL && s2 == NULL)
    {
        // ii => op_2num
        op_2num(st, op, i1, i2);
    }
    else
    {
        // si,is => error
        printf("script: op_2: int32_t&str, str&int32_t not allow.");
        push_val(st->stack, C_INT, 0);
    }
}

/*==========================================
 * 単項演算子
 *------------------------------------------
 */
static void op_1num(struct script_state *st, int32_t op)
{
    int32_t i1;
    i1 = pop_val(st);
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
    push_val(st->stack, C_INT, i1);
}

/*==========================================
 * 関数の実行
 *------------------------------------------
 */
int32_t run_func(struct script_state *st)
{
    int32_t io, start_sp, end_sp, func;

    end_sp = st->stack->sp;
    for (io = end_sp - 1; io >= 0 && st->stack->stack_data[io].type != C_ARG;
         io--);
    if (io == 0)
    {
        map_log("functimap_logon not found\n");
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
        printf("run_func: not function and command! \n");
//      st->stack->sp=0;
        st->state = END;
        return 0;
    }
    if (str_data[func].func)
    {
        str_data[func].func(st);
    }
    else
    {
        map_log("run_fumap_lognc : %s? (%d(%d))\n", str_buf + str_data[func].str,
                func, str_data[func].type);
        push_val(st->stack, C_INT, 0);
    }

    pop_stack(st->stack, start_sp, end_sp);

    if (st->state == RETFUNC)
    {
        // ユーザー定義関数からの復帰
        int32_t olddefsp = st->defsp;
        int32_t ii;

        pop_stack(st->stack, st->defsp, start_sp); // 復帰に邪魔なスタック削除
        if (st->defsp < 4
            || st->stack->stack_data[st->defsp - 1].type != C_RETINFO)
        {
            printf
                ("script:run_func(return) return without callfunc or callsub!\n");
            st->state = END;
            return 0;
        }
        ii = conv_num(st, &(st->stack->stack_data[st->defsp - 4])); // 引数の数所得
        st->pos = conv_num(st, &(st->stack->stack_data[st->defsp - 1]));   // スクリプト位置の復元
        int32_t tmp = conv_num(st, &(st->stack->stack_data[st->defsp - 2]));   // スクリプトを復元
        st->script = reinterpret_cast<char *>(tmp);
        st->defsp = conv_num(st, &(st->stack->stack_data[st->defsp - 3])); // 基準スタックポインタを復元

        pop_stack(st->stack, olddefsp - 4 - ii, olddefsp);  // 要らなくなったスタック(引数と復帰用データ)削除

        st->state = GOTO;
    }

    return 0;
}

/*==========================================
 * スクリプトの実行メイン部分
 *------------------------------------------
 */
static int32_t run_script_main(const char *script, int32_t pos, int32_t, int32_t,
                           struct script_state *st, const char *rootscript)
{
    int32_t c, rerun_pos;
    int32_t cmdcount = script_config.check_cmdcount;
    int32_t gotocount = script_config.check_gotocount;
    struct script_stack *stack = st->stack;

    st->defsp = stack->sp;
    st->script = script;

    rerun_pos = st->pos;
    for (st->state = 0; st->state == 0;)
    {
        switch (c = get_com(script, &st->pos))
        {
            case C_EOL:
                if (stack->sp != st->defsp)
                {
                    map_log("stack.sp(%d) != demap_logfault(%d)\n", stack->sp,
                            st->defsp);
                    stack->sp = st->defsp;
                }
                rerun_pos = st->pos;
                break;
            case C_INT:
                push_val(stack, C_INT, get_num(script, &st->pos));
                break;
            case C_POS:
            case C_NAME:
                push_val(stack, c, (*reinterpret_cast<const int32_t *>(script + st->pos)) & 0xffffff);
                st->pos += 3;
                break;
            case C_ARG:
                push_val(stack, c, 0);
                break;
            case C_STR:
                push_str(stack, C_CONSTSTR, script + st->pos);
                while (script[st->pos++]);
                break;
            case C_FUNC:
                run_func(st);
                if (st->state == GOTO)
                {
                    rerun_pos = st->pos;
                    script = st->script;
                    st->state = 0;
                    if (gotocount > 0 && (--gotocount) <= 0)
                    {
                        printf("run_script: infinity loop !\n");
                        st->state = END;
                    }
                }
                break;

            case C_ADD:
                op_add(st);
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
                op_2(st, c);
                break;

            case C_NEG:
            case C_NOT:
            case C_LNOT:
                op_1num(st, c);
                break;

            case C_NOP:
                st->state = END;
                break;

            default:
                map_log("unknown commanmap_logd : %d @ %d\n", c, pos);
                st->state = END;
                break;
        }
        if (cmdcount > 0 && (--cmdcount) <= 0)
        {
            printf("run_script: infinity loop !\n");
            st->state = END;
        }
    }
    switch (st->state)
    {
        case STOP:
            break;
        case END:
        {
            MapSessionData *sd = map_id2sd(st->rid);
            st->pos = -1;
            if (sd && sd->npc_id == st->oid)
                npc_event_dequeue(sd);
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
        MapSessionData *sd = map_id2sd(st->rid);
        if (sd /* && sd->npc_stackbuf==NULL */ )
        {
            if (sd->npc_stackbuf)
                free(sd->npc_stackbuf);
            CREATE(sd->npc_stackbuf, struct script_data, stack->sp_max);
            memcpy(sd->npc_stackbuf, stack->stack_data, sizeof(struct script_data) * stack->sp_max);
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
int32_t run_script(const char *script, int32_t pos, int32_t rid, int32_t oid)
{
    return run_script_l(script, pos, rid, oid, 0, NULL);
}

int32_t run_script_l(const char *script, int32_t pos, int32_t rid, int32_t oid, int32_t args_nr, ArgRec *args)
{
    struct script_stack stack;
    struct script_state st;
    MapSessionData *sd = map_id2sd(rid);
    const char *rootscript = script;
    int32_t i;
    if (script == NULL || pos < 0)
        return -1;

    if (sd && sd->npc_stackbuf && sd->npc_scriptroot == rootscript)
    {
        // 前回のスタックを復帰
        script = sd->npc_script;
        stack.sp = sd->npc_stack;
        stack.sp_max = sd->npc_stackmax;
        CREATE(stack.stack_data, struct script_data, stack.sp_max);
        memcpy(stack.stack_data, sd->npc_stackbuf,
               sizeof(struct script_data) * stack.sp_max);
        free(sd->npc_stackbuf);
        sd->npc_stackbuf = NULL;
    }
    else
    {
        // スタック初期化
        stack.sp = 0;
        stack.sp_max = 64;
        CREATE(stack.stack_data, struct script_data, stack.sp_max);
    }
    st.stack = &stack;
    st.pos = pos;
    st.rid = rid;
    st.oid = oid;
    for (i = 0; i < args_nr; i++)
    {
        if (args[i].name[strlen(args[i].name) - 1] == '$')
            pc_setregstr(sd, add_str(args[i].name), args[i].s);
        else
            pc_setreg(sd, add_str(args[i].name), args[i].i);
    }
    run_script_main(script, pos, rid, oid, &st, rootscript);

    free(stack.stack_data);
    stack.stack_data = NULL;
    return st.pos;
}

/*==========================================
 * マップ変数の変更
 *------------------------------------------
 */
int32_t mapreg_setreg(int32_t num, int32_t val)
{
    if (val != 0)
        numdb_insert(mapreg_db, num, val);
    else
        numdb_erase(mapreg_db, num);

    mapreg_dirty = 1;
    return 0;
}

/*==========================================
 * 文字列型マップ変数の変更
 *------------------------------------------
 */
int32_t mapreg_setregstr(int32_t num, const char *str)
{
    char *p;

    if ((p = static_cast<char *>(numdb_search(mapregstr_db, num).p)) != NULL)
        free(p);

    if (str == NULL || *str == 0)
    {
        numdb_erase(mapregstr_db, num);
        mapreg_dirty = 1;
        return 0;
    }
    CREATE(p, char, strlen(str) + 1);
    strcpy(p, str);
    numdb_insert(mapregstr_db, num, static_cast<void *>(p));
    mapreg_dirty = 1;
    return 0;
}

/*==========================================
 * 永続的マップ変数の読み込み
 *------------------------------------------
 */
static int32_t script_load_mapreg(void)
{
    FILE *fp;
    char line[1024];

    if ((fp = fopen_(mapreg_txt, "rt")) == NULL)
        return -1;

    while (fgets(line, sizeof(line), fp))
    {
        char buf1[256], buf2[1024], *p;
        int32_t n, v, s, i;
        if (sscanf(line, "%255[^,],%d\t%n", buf1, &i, &n) != 2 &&
            (i = 0, sscanf(line, "%[^\t]\t%n", buf1, &n) != 1))
            continue;
        if (buf1[strlen(buf1) - 1] == '$')
        {
            if (sscanf(line + n, "%[^\n\r]", buf2) != 1)
            {
                printf("%s: %s broken data !\n", mapreg_txt, buf1);
                continue;
            }
            CREATE(p, char, strlen(buf2));
            strcpy(p, buf2);
            s = add_str(buf1);
            numdb_insert(mapregstr_db, (i << 24) | s, static_cast<void *>(p));
        }
        else
        {
            if (sscanf(line + n, "%d", &v) != 1)
            {
                printf("%s: %s broken data !\n", mapreg_txt, buf1);
                continue;
            }
            s = add_str(buf1);
            numdb_insert(mapreg_db, (i << 24) | s, v);
        }
    }
    fclose_(fp);
    mapreg_dirty = 0;
    return 0;
}

/*==========================================
 * 永続的マップ変数の書き込み
 *------------------------------------------
 */
static void script_save_mapreg_intsub(db_key_t key, db_val_t data, FILE *fp)
{
    int32_t num = key.i & 0x00ffffff, i = key.i >> 24;
    const char *name = str_buf + str_data[num].str;
    if (name[1] != '@')
    {
        if (i == 0)
            fprintf(fp, "%s\t%d\n", name, static_cast<int32_t>(data.i));
        else
            fprintf(fp, "%s,%d\t%d\n", name, i, static_cast<int32_t>(data.i));
    }
}

static void script_save_mapreg_strsub(db_key_t key, db_val_t data, FILE *fp)
{
    int32_t num = key.i & 0x00ffffff, i = key.i >> 24;
    char *name = str_buf + str_data[num].str;
    if (name[1] != '@')
    {
        if (i == 0)
            fprintf(fp, "%s\t%s\n", name, static_cast<char *>(data.p));
        else
            fprintf(fp, "%s,%d\t%s\n", name, i, static_cast<char *>(data.p));
    }
}

static int32_t script_save_mapreg(void)
{
    FILE *fp;
    int32_t lock;

    if ((fp = lock_fopen(mapreg_txt, &lock)) == NULL)
        return -1;
    numdb_foreach(mapreg_db, script_save_mapreg_intsub, fp);
    numdb_foreach(mapregstr_db, script_save_mapreg_strsub, fp);
    lock_fclose(fp, mapreg_txt, &lock);
    mapreg_dirty = 0;
    return 0;
}

static void script_autosave_mapreg(timer_id, tick_t)
{
    if (mapreg_dirty)
        script_save_mapreg();
}

int32_t script_config_read(const char *cfgName)
{
    int32_t i;
    char line[1024], w1[1024], w2[1024];
    FILE *fp;

    script_config.warn_func_no_comma = 1;
    script_config.warn_cmd_no_comma = 1;
    script_config.warn_func_mismatch_paramnum = 1;
    script_config.warn_cmd_mismatch_paramnum = 1;
    script_config.check_cmdcount = 8192;
    script_config.check_gotocount = 512;

    fp = fopen_(cfgName, "r");
    if (fp == NULL)
    {
        printf("file not found: %s\n", cfgName);
        return 1;
    }
    while (fgets(line, 1020, fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;
        i = sscanf(line, "%[^:]: %[^\r\n]", w1, w2);
        if (i != 2)
            continue;
        if (strcasecmp(w1, "import") == 0)
        {
            script_config_read(w2);
        }
    }
    fclose_(fp);

    return 0;
}

/*==========================================
 * 終了
 *------------------------------------------
 */

static void mapregstr_db_final(db_key_t, db_val_t data)
{
    free(data.p);
}

static void userfunc_db_final(db_key_t key, db_val_t data)
{
    free(const_cast<char *>(key.s));
    free(data.p);
}

int32_t do_final_script(void)
{
    if (mapreg_dirty >= 0)
        script_save_mapreg();
    if (script_buf)
        free(script_buf);

    if (mapreg_db)
        // not numdb_final
        db_final(mapreg_db);
    if (mapregstr_db)
        strdb_final(mapregstr_db, mapregstr_db_final);
    if (scriptlabel_db)
        // not strdb_final
        db_final(scriptlabel_db);
    if (userfunc_db)
        strdb_final(userfunc_db, userfunc_db_final);

    if (str_data)
        free(str_data);
    if (str_buf)
        free(str_buf);

    return 0;
}

/*==========================================
 * 初期化
 *------------------------------------------
 */
int32_t do_init_script(void)
{
    mapreg_db = numdb_init();
    mapregstr_db = numdb_init();
    script_load_mapreg();

    add_timer_interval(gettick() + MAPREG_AUTOSAVE_INTERVAL,
                       MAPREG_AUTOSAVE_INTERVAL, script_autosave_mapreg);

    scriptlabel_db = strdb_init();
    return 0;
}
