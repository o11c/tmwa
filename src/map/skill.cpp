#include "skill.hpp"

#include "../common/mt_rand.hpp"
#include "../common/nullpo.hpp"
#include "../common/timer.hpp"

#include "battle.hpp"
#include "clif.hpp"
#include "magic-stmt.hpp"
#include "map.hpp"
#include "pc.hpp"

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
int skill_castcancel(BlockList *bl)
{
    nullpo_ret(bl);

    if (bl->type == BL_PC)
    {
        MapSessionData *sd = static_cast<MapSessionData *>(bl);
        unsigned long tick = gettick();
        nullpo_ret(sd);
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
int skill_status_change_active(BlockList *bl, int type)
{
    struct status_change *sc_data;

    nullpo_ret(bl);
    if (bl->type != BL_PC && bl->type != BL_MOB)
    {
        map_log("%s: neither MOB nor PC !\n", __func__);
        return 0;
    }

    nullpo_ret(sc_data = battle_get_sc_data(bl));

    return sc_data[type].timer != NULL;
}

int skill_status_change_end(BlockList *bl, int type, timer_id tid)
{
    struct status_change *sc_data;
    int opt_flag = 0, calc_flag = 0;
    short *sc_count, *option, *opt1, *opt2, *opt3;

    nullpo_ret(bl);
    if (bl->type != BL_PC && bl->type != BL_MOB)
    {
        map_log("%s: neither MOB nor PC !\n", __func__);
        return 0;
    }
    nullpo_ret(sc_data = battle_get_sc_data(bl));
    nullpo_ret(sc_count = battle_get_sc_count(bl));
    nullpo_ret(option = battle_get_option(bl));
    nullpo_ret(opt1 = battle_get_opt1(bl));
    nullpo_ret(opt2 = battle_get_opt2(bl));
    nullpo_ret(opt3 = battle_get_opt3(bl));

    if ((*sc_count) > 0 && sc_data[type].timer
        && (sc_data[type].timer == tid || tid == NULL))
    {

        if (tid == NULL)          // タイマから呼ばれていないならタイマ削除をする
            delete_timer(sc_data[type].timer);

        /* 該当の異常を正常に戻す */
        sc_data[type].timer = NULL;
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
                if (sc_data[SC_POISON].timer)
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
            pc_calcstatus(static_cast<MapSessionData *>(bl), 0);  /* ステータス再計算 */
    }

    return 0;
}

void skill_update_heal_animation(MapSessionData *sd)
{
    const int mask = 0x100;
    int was_active;
    int is_active;

    nullpo_retv(sd);
    was_active = sd->opt2 & mask;
    is_active = sd->quick_regeneration_hp.amount > 0;

    if ((was_active && is_active) || (!was_active && !is_active))
        return;               // no update

    if (is_active)
        sd->opt2 |= mask;
    else
        sd->opt2 &= ~mask;

    clif_changeoption(sd);
}

/*==========================================
 * ステータス異常終了タイマー
 *------------------------------------------
 */
static void skill_status_change_timer(timer_id tid, tick_t tick, uint32_t id, int type)
{
    BlockList *bl;
    MapSessionData *sd = NULL;
    struct status_change *sc_data;
    //short *sc_count; //使ってない？

    if ((bl = map_id2bl(id)) == NULL)
        return;               //該当IDがすでに消滅しているというのはいかにもありそうなのでスルーしてみる
    nullpo_retv(sc_data = battle_get_sc_data(bl));

    if (bl->type == BL_PC)
        sd = static_cast<MapSessionData *>(bl);

    //sc_count=battle_get_sc_count(bl); //使ってない？

    if (sc_data[type].spell_invocation)
    {                           // Must report termination
        spell_effect_report_termination(sc_data[type].spell_invocation,
                                         bl->id, type, 0);
        sc_data[type].spell_invocation = 0;
    }

    switch (type)
    {                           /* 特殊な処理になる場合 */
        case SC_POISON:
            if (sc_data[SC_SLOWPOISON].timer == NULL)
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
                            if ((md = static_cast<struct mob_data *>(bl)) == NULL)
                                break;
                            hp = 3 + hp / 200;
                            md->hp -= hp;
                        }
                    }
                    sc_data[type].timer = add_timer(1000 + tick, skill_status_change_timer, bl->id, type);
                }
            }
            else
                sc_data[type].timer = add_timer(2000 + tick, skill_status_change_timer, bl->id, type);
            break;

        case SC_FLYING_BACKPACK:
            clif_updatestatus(sd, SP::WEIGHT);
            break;

    }

    skill_status_change_end(bl, type, tid);
}

/*==========================================
 * ステータス異常開始
 *------------------------------------------
 */
int skill_status_change_start(BlockList *bl, int type, int val1, tick_t tick)
{
    return skill_status_effect(bl, type, val1, tick, 0);
}

int skill_status_effect(BlockList *bl, int type, int val1, tick_t tick, int spell_invocation)
{
    MapSessionData *sd = NULL;
    struct status_change *sc_data;
    short *sc_count, *option, *opt1, *opt2, *opt3;
    int opt_flag = 0, calc_flag = 0;
    SP updateflag = SP::NONE;
    int scdef = 0;

    nullpo_ret(bl);
    nullpo_ret(sc_data = battle_get_sc_data(bl));
    nullpo_ret(sc_count = battle_get_sc_count(bl));
    nullpo_ret(option = battle_get_option(bl));
    nullpo_ret(opt1 = battle_get_opt1(bl));
    nullpo_ret(opt2 = battle_get_opt2(bl));
    nullpo_ret(opt3 = battle_get_opt3(bl));

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
        sd = static_cast<MapSessionData *>(bl);
    }
    else if (bl->type == BL_MOB)
    {
    }
    else
    {
        map_log("%s: neither MOB nor PC !\n", __func__);
        return 0;
    }

    if (sc_data[type].timer)
    {                           /* すでに同じ異常になっている場合タイマ解除 */
        if (sc_data[type].val1 > val1 && type != SC_SPEEDPOTION0 && type != SC_ATKPOT)
            return 0;
        if (type == SC_POISON)
            return 0;           /* 継ぎ足しができない状態異常である時は状態異常を行わない */
        (*sc_count)--;
        delete_timer(sc_data[type].timer);
        sc_data[type].timer = NULL;
    }

    switch (type)
    {
        case SC_SLOWPOISON:
            if (sc_data[SC_POISON].timer == NULL)
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
            {
                int sc_def =
                    100 - (battle_get_vit(bl) + battle_get_luk(bl) / 5);
                tick = tick * sc_def / 100;
            }
            tick = 1000;
            break;
        case SC_HASTE:
            calc_flag = 1;
        case SC_PHYS_SHIELD:
            break;
        case SC_FLYING_BACKPACK:
            updateflag = SP::WEIGHT;
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
            if (sc_data[SC_SLOWPOISON].timer == NULL)
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

    if (bl->type == BL_PC && updateflag != SP::NONE)
        clif_updatestatus(sd, updateflag); /* ステータスをクライアントに送る */

    return 0;
}

/*==========================================
 * ステータス異常全解除
 *------------------------------------------
 */
int skill_status_change_clear(BlockList *bl, int type)
{
    struct status_change *sc_data;
    short *sc_count, *option, *opt1, *opt2, *opt3;
    int i;

    nullpo_ret(bl);
    nullpo_ret(sc_data = battle_get_sc_data(bl));
    nullpo_ret(sc_count = battle_get_sc_count(bl));
    nullpo_ret(option = battle_get_option(bl));
    nullpo_ret(opt1 = battle_get_opt1(bl));
    nullpo_ret(opt2 = battle_get_opt2(bl));
    nullpo_ret(opt3 = battle_get_opt3(bl));

    if (*sc_count == 0)
        return 0;
    for (i = 0; i < MAX_STATUSCHANGE; i++)
    {
        if (sc_data[i].timer)
        {
            skill_status_change_end(bl, i, NULL);
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

static SP scan_stat(char *statname)
{
    if (!strcasecmp(statname, "str"))
        return SP::STR;
    if (!strcasecmp(statname, "dex"))
        return SP::DEX;
    if (!strcasecmp(statname, "agi"))
        return SP::AGI;
    if (!strcasecmp(statname, "vit"))
        return SP::VIT;
    if (!strcasecmp(statname, "int"))
        return SP::INT;
    if (!strcasecmp(statname, "luk"))
        return SP::LUK;
    if (strcasecmp(statname, "none"))
        fprintf(stderr, "Unknown stat `%s'\n", statname);

    return SP::NONE;
}

static char *set_skill_name(int idx, const char *name)
{
    for (int i = 0; skill_names[i].id; i++)
        if (skill_names[i].id == idx)
        {
            char *dup = strdup(name);
            skill_names[i].desc = dup;
            return dup;
        }
    return NULL;
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

        char *s = set_skill_name(i, split[17]);
        if (!s)
            continue;
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



int skill_pool_skills[MAX_POOL_SKILLS];
int skill_pool_skills_size = 0;

static int skill_pool_size(MapSessionData *sd);

void skill_pool_register(int id)
{
    if (skill_pool_skills_size + 1 >= MAX_POOL_SKILLS)
    {
        fprintf(stderr,
                "Too many pool skills! Increase MAX_POOL_SKILLS and recompile.");
        return;
    }

    skill_pool_skills[skill_pool_skills_size++] = id;
}

const char *skill_name(int skill)
{
    if (skill > 0 && skill < MAX_SKILL)
        return skill_names[skill].desc;
    else
        return NULL;
}

int skill_pool(MapSessionData *sd, int *skills)
{
    int i, count = 0;

    for (i = 0; count < MAX_SKILL_POOL && i < skill_pool_skills_size; i++)
    {
        int skill_id = skill_pool_skills[i];
        if (sd->status.skill[skill_id].flags & SKILL_POOL_ACTIVATED)
        {
            if (skills)
                skills[count] = skill_id;
            ++count;
        }
    }

    return count;
}

int skill_pool_size(MapSessionData *sd)
{
    return skill_pool(sd, NULL);
}

int skill_pool_max(MapSessionData *sd)
{
    return sd->status.skill[TMW_SKILLPOOL].lv;
}

int skill_pool_activate(MapSessionData *sd, int skill_id)
{
    if (sd->status.skill[skill_id].flags & SKILL_POOL_ACTIVATED)
        return 0;               // Already there
        else if (sd->status.skill[skill_id].id == skill_id  // knows the skill
             && (skill_pool_size(sd) < skill_pool_max(sd)))
    {
        sd->status.skill[skill_id].flags |= SKILL_POOL_ACTIVATED;
        pc_calcstatus(sd, 0);
        MAP_LOG_PC(sd, "SKILL-ACTIVATE %d %d %d", skill_id,
                   sd->status.skill[skill_id].lv, skill_power(sd,
                                                              skill_id));
        return 0;
    }

    return 1;                   // failed
}

int skill_pool_is_activated(MapSessionData *sd, int skill_id)
{
    return sd->status.skill[skill_id].flags & SKILL_POOL_ACTIVATED;
}

int skill_pool_deactivate(MapSessionData *sd, int skill_id)
{
    if (sd->status.skill[skill_id].flags & SKILL_POOL_ACTIVATED)
    {
        sd->status.skill[skill_id].flags &= ~SKILL_POOL_ACTIVATED;
        MAP_LOG_PC(sd, "SKILL-DEACTIVATE %d", skill_id);
        pc_calcstatus(sd, 0);
        return 0;
    }

    return 1;
}

int skill_power(MapSessionData *sd, int skill_id)
{
    SP stat = skill_db[skill_id].stat;

    if (stat == SP::NONE || !skill_pool_is_activated(sd, skill_id))
        return 0;

    int stat_value = battle_get_stat(stat, sd);
    int skill_value = sd->status.skill[skill_id].lv;

    if ((skill_value * 10) - 1 > stat_value)
        skill_value += (stat_value / 10);
    else
        skill_value *= 2;

    return (skill_value * stat_value) / 10;
}

int skill_power_bl(BlockList *bl, int skill)
{
    if (bl->type == BL_PC)
        return skill_power(static_cast<MapSessionData *>(bl), skill);
    else
        return 0;
}
