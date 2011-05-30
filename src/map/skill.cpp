#include "skill.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../common/timer.hpp"
#include "../common/nullpo.hpp"
#include "../common/mt_rand.hpp"
#include "magic.hpp"

#include "battle.hpp"
#include "clif.hpp"
#include "intif.hpp"
#include "itemdb.hpp"
#include "map.hpp"
#include "mob.hpp"
#include "party.hpp"
#include "pc.hpp"
#include "script.hpp"
#include "../common/socket.hpp"

static void skill_status_change_timer(timer_id, tick_t, custom_id_t, custom_data_t);

struct skill_name_db skill_names[] =
{
    {NV_EMOTE,          "EMOTE",        "Emote_Skill"},
    {NV_TRADE,          "TRADE",        "Trade_Skill"},
    {NV_PARTY,          "PARTY",        "Party_Skill"},
    {AC_OWL,            "MALLARD",      "Mallard's_Eye"},
    {TMW_SKILLPOOL,     "POOL",         "Skill_Pool"},

    {TMW_MAGIC,         "MAGIC",        "General_Magic"},
    {TMW_MAGIC_LIFE,    "LIFE",         "Life_Magic"},
    {TMW_MAGIC_WAR,     "WAR",          "War_Magic"},
    {TMW_MAGIC_TRANSMUTE,"TRANSMUTE",   "Transmutation_Magic"},
    {TMW_MAGIC_NATURE,  "NATURE",       "Nature_Magic" },
    {TMW_MAGIC_ETHER,   "ASTRAL",       "Astral_Magic"},

    {TMW_BRAWLING,      "BRAWL",        "Brawling"},
    {TMW_LUCKY_COUNTER, "LUCKY",        "Lucky_Counter"},
    {TMW_SPEED,         "SPEED",        "Speed"},
    {TMW_RESIST_POISON, "RESIST_POISON","Resist_Poison"},
    {TMW_ASTRAL_SOUL,   "ASTRAL_SOUL",  "Astral_Soul"},
    {TMW_RAGING,        "RAGE",         "Raging"},
    {0, 0, 0}
};

static const int dirx[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
static const int diry[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

/* スキルデータベース */
struct skill_db skill_db[MAX_SKILL];

#define UNARMED_PLAYER_DAMAGE_MIN(bl)   (skill_power_bl((bl), TMW_BRAWLING) >> 4)   // +50 for 200
#define UNARMED_PLAYER_DAMAGE_MAX(bl)   (skill_power_bl((bl), TMW_BRAWLING))    // +200 for 200

int skill_get_max_raise(int id)
{
    return skill_db[id].max_raise;
}

/*---------------------------------------------------------------------------- */

/*==========================================
 * スキル詠唱キャンセル
 *------------------------------------------
 */
int skill_castcancel(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->type == BL_PC)
    {
        struct map_session_data *sd = reinterpret_cast<struct map_session_data *>(bl);
        unsigned long tick = gettick();
        nullpo_retr(0, sd);
        sd->canact_tick = tick;
        sd->canmove_tick = tick;

        return 0;
    }
    else if (bl->type == BL_MOB)
    {
        return 0;
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * ステータス異常
 *----------------------------------------------------------------------------
 */

/*==========================================
 * ステータス異常終了
 *------------------------------------------
 */
int skill_status_change_active(struct block_list *bl, int type)
{
    struct status_change *sc_data;

    nullpo_retr(0, bl);
    if (bl->type != BL_PC && bl->type != BL_MOB)
    {
        map_log("%s: neither MOB nor PC !\n", __func__);
        return 0;
    }

    nullpo_retr(0, sc_data = battle_get_sc_data(bl));

    return sc_data[type].timer != -1;
}

int skill_status_change_end(struct block_list *bl, int type, int tid)
{
    struct status_change *sc_data;
    int opt_flag = 0, calc_flag = 0;
    short *sc_count, *option, *opt1, *opt2, *opt3;

    nullpo_retr(0, bl);
    if (bl->type != BL_PC && bl->type != BL_MOB)
    {
        map_log("%s: neither MOB nor PC !\n", __func__);
        return 0;
    }
    nullpo_retr(0, sc_data = battle_get_sc_data(bl));
    nullpo_retr(0, sc_count = battle_get_sc_count(bl));
    nullpo_retr(0, option = battle_get_option(bl));
    nullpo_retr(0, opt1 = battle_get_opt1(bl));
    nullpo_retr(0, opt2 = battle_get_opt2(bl));
    nullpo_retr(0, opt3 = battle_get_opt3(bl));

    if ((*sc_count) > 0 && sc_data[type].timer != -1
        && (sc_data[type].timer == tid || tid == -1))
    {

        if (tid == -1)          // タイマから呼ばれていないならタイマ削除をする
            delete_timer(sc_data[type].timer, skill_status_change_timer);

        /* 該当の異常を正常に戻す */
        sc_data[type].timer = -1;
        (*sc_count)--;

        switch (type)
        {                       /* 異常の種類ごとの処理 */
            case SC_SPEEDPOTION0:  /* 増速ポーション */
            case SC_ATKPOT:    /* attack potion [Valaris] */
            case SC_PHYS_SHIELD:
            case SC_HASTE:
                calc_flag = 1;
                break;
                /* option2 */
            case SC_POISON:    /* 毒 */
                calc_flag = 1;
                break;
        }

        if (bl->type == BL_PC && type < SC_SENDMAX)
            clif_status_change(bl, type, 0);   /* アイコン消去 */

        switch (type)
        {                       /* 正常に戻るときなにか処理が必要 */
            case SC_POISON:
                *opt2 &= ~1;
                opt_flag = 1;
                break;

            case SC_SLOWPOISON:
                if (sc_data[SC_POISON].timer != -1)
                    *opt2 |= 0x1;
                *opt2 &= ~0x200;
                opt_flag = 1;
                break;

            case SC_SPEEDPOTION0:
                *opt2 &= ~0x20;
                opt_flag = 1;
                break;

            case SC_ATKPOT:
                *opt2 &= ~0x80;
                opt_flag = 1;
                break;
        }

        if (opt_flag)           /* optionの変更を伝える */
            clif_changeoption(bl);

        if (bl->type == BL_PC && calc_flag)
            pc_calcstatus(reinterpret_cast<struct map_session_data *>(bl), 0);  /* ステータス再計算 */
    }

    return 0;
}

int skill_update_heal_animation(struct map_session_data *sd)
{
    const int mask = 0x100;
    int was_active;
    int is_active;

    nullpo_retr(0, sd);
    was_active = sd->opt2 & mask;
    is_active = sd->quick_regeneration_hp.amount > 0;

    if ((was_active && is_active) || (!was_active && !is_active))
        return 0;               // no update

    if (is_active)
        sd->opt2 |= mask;
    else
        sd->opt2 &= ~mask;

    return clif_changeoption(&sd->bl);
}

/*==========================================
 * ステータス異常終了タイマー
 *------------------------------------------
 */
void skill_status_change_timer(timer_id tid, tick_t tick, custom_id_t id, custom_data_t data)
{
    int type = data.i;
    struct block_list *bl;
    struct map_session_data *sd = NULL;
    struct status_change *sc_data;
    //short *sc_count; //使ってない？

    if ((bl = map_id2bl(id)) == NULL)
        return;               //該当IDがすでに消滅しているというのはいかにもありそうなのでスルーしてみる
    nullpo_retv(sc_data = battle_get_sc_data(bl));

    if (bl->type == BL_PC)
        sd = reinterpret_cast<struct map_session_data *>(bl);

    //sc_count=battle_get_sc_count(bl); //使ってない？

    if (sc_data[type].timer != tid)
    {
        map_log("%s: %d != %d\n", __func__, tid, sc_data[type].timer);
    }

    if (sc_data[type].spell_invocation)
    {                           // Must report termination
        spell_effect_report_termination(sc_data[type].spell_invocation,
                                         bl->id, type, 0);
        sc_data[type].spell_invocation = 0;
    }

    switch (type)
    {                           /* 特殊な処理になる場合 */
        case SC_POISON:
            if (sc_data[SC_SLOWPOISON].timer == -1)
            {
                const int resist_poison =
                    skill_power_bl(bl, TMW_RESIST_POISON) >> 3;
                if (resist_poison)
                    sc_data[type].val1 -= MRAND(resist_poison + 1);

                if ((--sc_data[type].val1) > 0)
                {

                    int hp = battle_get_max_hp(bl);
                    if (battle_get_hp(bl) > hp >> 4)
                    {
                        if (bl->type == BL_PC)
                        {
                            hp = 3 + hp * 3 / 200;
                            pc_heal(sd, -hp, 0);
                        }
                        else if (bl->type == BL_MOB)
                        {
                            struct mob_data *md;
                            if ((md = reinterpret_cast<struct mob_data *>(bl)) == NULL)
                                break;
                            hp = 3 + hp / 200;
                            md->hp -= hp;
                        }
                    }
                    sc_data[type].timer =
                        add_timer(1000 + tick, skill_status_change_timer,
                                   bl->id, data);
                }
            }
            else
                sc_data[type].timer =
                    add_timer(2000 + tick, skill_status_change_timer, bl->id,
                               data);
            break;

        case SC_BROKNWEAPON:
        case SC_BROKNARMOR:
            if (sc_data[type].timer == tid)
                sc_data[type].timer =
                    add_timer(1000 * 600 + tick, skill_status_change_timer,
                               bl->id, data);
            return;

        case SC_FLYING_BACKPACK:
            clif_updatestatus(sd, SP_WEIGHT);
            break;

    }

    skill_status_change_end(bl, type, tid);
}

/*==========================================
 * ステータス異常開始
 *------------------------------------------
 */
int skill_status_change_start(struct block_list *bl, int type, int val1,
                               int val2, int val3, int val4, int tick,
                               int flag)
{
    return skill_status_effect(bl, type, val1, val2, val3, val4, tick, flag,
                                0);
}

int skill_status_effect(struct block_list *bl, int type, int val1, int val2,
                         int val3, int val4, int tick, int flag,
                         int spell_invocation)
{
    struct map_session_data *sd = NULL;
    struct status_change *sc_data;
    short *sc_count, *option, *opt1, *opt2, *opt3;
    int opt_flag = 0, calc_flag = 0, updateflag =
        0;
    int scdef = 0;

    nullpo_retr(0, bl);
    nullpo_retr(0, sc_data = battle_get_sc_data(bl));
    nullpo_retr(0, sc_count = battle_get_sc_count(bl));
    nullpo_retr(0, option = battle_get_option(bl));
    nullpo_retr(0, opt1 = battle_get_opt1(bl));
    nullpo_retr(0, opt2 = battle_get_opt2(bl));
    nullpo_retr(0, opt3 = battle_get_opt3(bl));

    switch (type)
    {
        case SC_POISON:
            scdef = 3 + battle_get_vit(bl) + battle_get_luk(bl) / 3;
            break;
        default:
            scdef = 0;
    }
    if (scdef >= 100)
        return 0;
    if (bl->type == BL_PC)
    {
        sd = reinterpret_cast<struct map_session_data *>(bl);
    }
    else if (bl->type == BL_MOB)
    {
    }
    else
    {
        map_log("%s: neither MOB nor PC !\n", __func__);
        return 0;
    }

    if (sc_data[type].timer != -1)
    {                           /* すでに同じ異常になっている場合タイマ解除 */
        if (sc_data[type].val1 > val1 && type != SC_SPEEDPOTION0 && type != SC_ATKPOT)
            return 0;
        if (type == SC_POISON)
            return 0;           /* 継ぎ足しができない状態異常である時は状態異常を行わない */
        (*sc_count)--;
        delete_timer(sc_data[type].timer, skill_status_change_timer);
        sc_data[type].timer = -1;
    }

    switch (type)
    {
        case SC_SLOWPOISON:
            if (sc_data[SC_POISON].timer == -1)
                return 0;
            break;
        case SC_SPEEDPOTION0:  /* 増速ポーション */
            *opt2 |= 0x20;
            calc_flag = 1;
            tick = 1000 * tick;
//          val2 = 5*(2+type-SC_SPEEDPOTION0);
            break;

            /* atk & matk potions [Valaris] */
        case SC_ATKPOT:
            *opt2 |= 0x80;
            calc_flag = 1;
            tick = 1000 * tick;
            break;

            /* option2 */
        case SC_POISON:        /* 毒 */
            calc_flag = 1;
            if (!(flag & 2))
            {
                int sc_def =
                    100 - (battle_get_vit(bl) + battle_get_luk(bl) / 5);
                tick = tick * sc_def / 100;
            }
            val3 = tick / 1000;
            if (val3 < 1)
                val3 = 1;
            tick = 1000;
            break;
        case SC_HASTE:
            calc_flag = 1;
        case SC_PHYS_SHIELD:
            break;
        case SC_FLYING_BACKPACK:
            updateflag = SP_WEIGHT;
            break;
        default:
            map_log("UnknownStatusChange [%d]\n", type);
            return 0;
    }

    if (bl->type == BL_PC && type < SC_SENDMAX)
        clif_status_change(bl, type, 1);   /* アイコン表示 */

    /* optionの変更 */
    switch (type)
    {
        case SC_POISON:
            if (sc_data[SC_SLOWPOISON].timer == -1)
            {
                *opt2 |= 0x1;
                opt_flag = 1;
            }
            break;
        case SC_SLOWPOISON:
            *opt2 &= ~0x1;
            *opt2 |= 0x200;
            opt_flag = 1;
            break;
    }

    if (opt_flag)               /* optionの変更 */
        clif_changeoption(bl);

    (*sc_count)++;              /* ステータス異常の数 */

    sc_data[type].val1 = val1;
    sc_data[type].val2 = val2;
    sc_data[type].val3 = val3;
    sc_data[type].val4 = val4;
    if (sc_data[type].spell_invocation) // Supplant by newer spell
        spell_effect_report_termination(sc_data[type].spell_invocation,
                                         bl->id, type, 1);

    sc_data[type].spell_invocation = spell_invocation;

    /* タイマー設定 */
    sc_data[type].timer =
        add_timer(gettick() + tick, skill_status_change_timer, bl->id,
                   type);

    if (bl->type == BL_PC && calc_flag)
        pc_calcstatus(sd, 0);  /* ステータス再計算 */

    if (bl->type == BL_PC && updateflag)
        clif_updatestatus(sd, updateflag); /* ステータスをクライアントに送る */

    return 0;
}

/*==========================================
 * ステータス異常全解除
 *------------------------------------------
 */
int skill_status_change_clear(struct block_list *bl, int type)
{
    struct status_change *sc_data;
    short *sc_count, *option, *opt1, *opt2, *opt3;
    int i;

    nullpo_retr(0, bl);
    nullpo_retr(0, sc_data = battle_get_sc_data(bl));
    nullpo_retr(0, sc_count = battle_get_sc_count(bl));
    nullpo_retr(0, option = battle_get_option(bl));
    nullpo_retr(0, opt1 = battle_get_opt1(bl));
    nullpo_retr(0, opt2 = battle_get_opt2(bl));
    nullpo_retr(0, opt3 = battle_get_opt3(bl));

    if (*sc_count == 0)
        return 0;
    for (i = 0; i < MAX_STATUSCHANGE; i++)
    {
        if (sc_data[i].timer != -1)
        {                       /* 異常があるならタイマーを削除する */
/*
                        delete_timer(sc_data[i].timer, skill_status_change_timer);
                        sc_data[i].timer = -1;

                        if (!type && i < SC_SENDMAX)
                                clif_status_change(bl, i, 0);
*/

            skill_status_change_end(bl, i, -1);
        }
    }
    *sc_count = 0;
    *opt1 = 0;
    *opt2 = 0;
    *opt3 = 0;
    *option &= OPTION_MASK;

    if (!type || type & 2)
        clif_changeoption(bl);

    return 0;
}

/*----------------------------------------------------------------------------
 * アイテム合成
 *----------------------------------------------------------------------------
 */

/*----------------------------------------------------------------------------
 * 初期化系
 */

static int scan_stat(char *statname)
{
    if (!strcasecmp(statname, "str"))
        return SP_STR;
    if (!strcasecmp(statname, "dex"))
        return SP_DEX;
    if (!strcasecmp(statname, "agi"))
        return SP_AGI;
    if (!strcasecmp(statname, "vit"))
        return SP_VIT;
    if (!strcasecmp(statname, "int"))
        return SP_INT;
    if (!strcasecmp(statname, "luk"))
        return SP_LUK;
    if (!strcasecmp(statname, "none"))
        return 0;

    else
        fprintf(stderr, "Unknown stat `%s'\n", statname);
    return 0;
}

/// read skill_db.txt
static int skill_readdb(void)
{
    int i, j;
    FILE *fp;
    char line[1024], *p;

    /* The main skill database */
    memset(skill_db, 0, sizeof(skill_db));
    fp = fopen_("db/skill_db.txt", "r");
    if (fp == NULL)
    {
        printf("can't read db/skill_db.txt\n");
        return 1;
    }
    while (fgets(line, sizeof(line), fp))
    {
        char *split[18], *split2[MAX_SKILL_LEVEL];
        if (line[0] == '/' && line[1] == '/')
            continue;
        for (j = 0, p = line; j < 18 && p; j++)
        {
            while (*p == '\t' || *p == ' ')
                p++;
            split[j] = p;
            p = strchr(p, ',');
            if (p)
                *p++ = 0;
        }
        if (split[17] == NULL || j < 18)
        {
            fprintf(stderr,
                    "Incomplete skill db data online: '%s' has only %d of 18 entries\n",
                    split[0], j);
            continue;
        }

        i = atoi(split[0]);
        if (i < 0 || i > MAX_SKILL)
            continue;

        // split[1]: ranges
//         skill_db[i].hit = atoi(split[2]);
//         skill_db[i].inf = atoi(split[3]);
//         skill_db[i].pl = atoi(split[4]);
//         skill_db[i].nk = atoi(split[5]);
        skill_db[i].max_raise = atoi(split[6]);
//         skill_db[i].max = atoi(split[7]);

        // split[8]:
//         skill_db[i].castcancel = strcasecmp(split[9], "yes") == 0;
//         skill_db[i].cast_def_rate = atoi(split[10]);
//         skill_db[i].inf2 = atoi(split[11]);
//         skill_db[i].maxcount = atoi(split[12]);
        // split[13]: skill type (weapon/magic/misc)
        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[14]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }

        if (!strcasecmp(split[15], "passive"))
        {
            skill_pool_register(i);
            skill_db[i].poolflags = SKILL_POOL_FLAG;
        }
        else if (!strcasecmp(split[15], "active"))
        {
            skill_pool_register(i);
            skill_db[i].poolflags = SKILL_POOL_FLAG | SKILL_POOL_ACTIVE;
        }
        else
            skill_db[i].poolflags = 0;

        skill_db[i].stat = scan_stat(split[16]);

        char *s = strdup(split[17]);
        skill_names[i].desc = s;
        char *stest;
        // replace "_" by " "
        while ((stest = strchr(s, '_')))
            *stest = ' ';
        if ((stest = strchr(s, '\t'))
            || (stest = strchr(s, ' '))
            || (stest = strchr(s, '\n')))
            *stest = '\000';
    }
    fclose_(fp);
    printf("read db/skill_db.txt done\n");

    return 0;
}

/*==========================================
 * スキル関係初期化処理
 *------------------------------------------
 */
void do_init_skill(void)
{
    skill_readdb();
}
