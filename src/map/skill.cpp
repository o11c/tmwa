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

#define SKILLUNITTIMER_INVERVAL 100

#define STATE_BLIND 0x10

static int skill_get_time2(int id, int lv);
static int skill_delunitgroup(struct skill_unit_group *group);
static int skill_unitgrouptickset_delete(struct block_list *bl, int group_id);
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

static int rdamage;

/* スキルデータベース */
struct skill_db skill_db[MAX_SKILL_DB];

#define UNARMED_PLAYER_DAMAGE_MIN(bl)   (skill_power_bl((bl), TMW_BRAWLING) >> 4)   // +50 for 200
#define UNARMED_PLAYER_DAMAGE_MAX(bl)   (skill_power_bl((bl), TMW_BRAWLING))    // +200 for 200

int skill_get_hit(int id)
{
    return skill_db[id].hit;
}

int skill_get_inf(int id)
{
    return skill_db[id].inf;
}

int skill_get_pl(int id)
{
    return skill_db[id].pl;
}

int skill_get_max(int id)
{
    return skill_db[id].max;
}

int skill_get_max_raise(int id)
{
    return skill_db[id].max_raise;
}

int skill_get_range(int id, int lv)
{
    return (lv <= 0) ? 0 : skill_db[id].range[lv - 1];
}

int skill_get_sp(int id, int lv)
{
    return (lv <= 0) ? 0 : skill_db[id].sp[lv - 1];
}

int skill_get_num(int id, int lv)
{
    return (lv <= 0) ? 0 : skill_db[id].num[lv - 1];
}

int skill_get_delay(int id, int lv)
{
    return (lv <= 0) ? 0 : skill_db[id].delay[lv - 1];
}

int skill_get_time2(int id, int lv)
{
    return (lv <= 0) ? 0 : skill_db[id].upkeep_time2[lv - 1];
}

int skill_get_castdef(int id)
{
    return skill_db[id].cast_def_rate;
}

int skill_get_inf2(int id)
{
    return skill_db[id].inf2;
}

int skill_get_maxcount(int id)
{
    return skill_db[id].maxcount;
}

int skill_get_blewcount(int id, int lv)
{
    return (lv <= 0) ? 0 : skill_db[id].blewcount[lv - 1];
}

static int skill_get_castnodex(int id, int lv)
{
    return (lv <= 0) ? 0 : skill_db[id].castnodex[lv - 1];
}

/* プロトタイプ */
struct skill_unit_group *skill_unitsetting(struct block_list *src,
                                            int skillid, int skilllv, int x,
                                            int y, int flag);
static void skill_trap_splash(struct block_list *bl, va_list ap);
static void skill_count_target(struct block_list *bl, va_list ap);

/*==========================================
 * スキル追加効果
 *------------------------------------------
 */
int skill_additional_effect(struct block_list *src, struct block_list *bl,
                             int, int skilllv, int,
                             unsigned int)
{
    struct map_session_data *sd = NULL;
    struct mob_data *md = NULL;

    int luk;

    int sc_def_mdef, sc_def_vit, sc_def_int, sc_def_luk;

    nullpo_retr(0, src);
    nullpo_retr(0, bl);

    if (skilllv < 0)
        return 0;

    if (src->type == BL_PC)
    {
        nullpo_retr(0, sd = (struct map_session_data *) src);
    }
    else if (src->type == BL_MOB)
    {
        nullpo_retr(0, md = (struct mob_data *) src);  //未使用？
    }

    //対象の耐性
    luk = battle_get_luk(bl);
    sc_def_mdef = 100 - (3 + battle_get_mdef(bl) + luk / 3);
    sc_def_vit = 100 - (3 + battle_get_vit(bl) + luk / 3);
    sc_def_int = 100 - (3 + battle_get_int(bl) + luk / 3);
    sc_def_luk = 100 - (3 + luk);
    //自分の耐性
    luk = battle_get_luk(src);
    if (bl->type == BL_MOB)
    {
        if (sc_def_mdef > 50)
            sc_def_mdef = 50;
        if (sc_def_vit > 50)
            sc_def_vit = 50;
        if (sc_def_int > 50)
            sc_def_int = 50;
        if (sc_def_luk > 50)
            sc_def_luk = 50;
    }
    if (sc_def_mdef < 0)
        sc_def_mdef = 0;
    if (sc_def_vit < 0)
        sc_def_vit = 0;
    if (sc_def_int < 0)
        sc_def_int = 0;
    return 0;
}

/*=========================================================================
 スキル攻撃吹き飛ばし処理
-------------------------------------------------------------------------*/
static int skill_blown(struct block_list *src, struct block_list *target, int count)
{
    int dx = 0, dy = 0, nx, ny;
    int x = target->x, y = target->y;
    int ret, prev_state = MS_IDLE;
    int moveblock;
    struct map_session_data *sd = NULL;
    struct mob_data *md = NULL;
    struct skill_unit *su = NULL;

    nullpo_retr(0, src);
    nullpo_retr(0, target);

    if (target->type == BL_PC)
    {
        nullpo_retr(0, sd = (struct map_session_data *) target);
    }
    else if (target->type == BL_MOB)
    {
        nullpo_retr(0, md = (struct mob_data *) target);
    }
    else if (target->type == BL_SKILL)
    {
        nullpo_retr(0, su = (struct skill_unit *) target);
    }
    else
        return 0;

    if (!(count & 0x10000 && (sd || md || su)))
    {                           /* 指定なしなら位置関係から方向を求める */
        dx = target->x - src->x;
        dx = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
        dy = target->y - src->y;
        dy = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
    }
    if (dx == 0 && dy == 0)
    {
        int dir = battle_get_dir(target);
        if (dir >= 0 && dir < 8)
        {
            dx = -dirx[dir];
            dy = -diry[dir];
        }
    }

    ret = path_blownpos(target->m, x, y, dx, dy, count & 0xffff);
    nx = ret >> 16;
    ny = ret & 0xffff;
    moveblock = (x / BLOCK_SIZE != nx / BLOCK_SIZE
                 || y / BLOCK_SIZE != ny / BLOCK_SIZE);

    if (count & 0x20000)
    {
        battle_stopwalking(target, 1);
        if (sd)
        {
            sd->to_x = nx;
            sd->to_y = ny;
            sd->walktimer = 1;
            clif_walkok(sd);
            clif_movechar(sd);
        }
        else if (md)
        {
            md->to_x = nx;
            md->to_y = ny;
            prev_state = md->state.state;
            md->state.state = MS_WALK;
            clif_fixmobpos(md);
        }
    }
    else
        battle_stopwalking(target, 2);

    dx = nx - x;
    dy = ny - y;

    if (sd)                     /* 画面外に出たので消去 */
        map_foreachinmovearea(clif_pcoutsight, target->m, x - AREA_SIZE,
                               y - AREA_SIZE, x + AREA_SIZE, y + AREA_SIZE,
                               dx, dy, BL_NUL, sd);
    else if (md)
        map_foreachinmovearea(clif_moboutsight, target->m, x - AREA_SIZE,
                               y - AREA_SIZE, x + AREA_SIZE, y + AREA_SIZE,
                               dx, dy, BL_PC, md);

    if (su)
    {
        skill_unit_move_unit_group(su->group, target->m, dx, dy);
    }
    else
    {
//      struct status_change *sc_data=battle_get_sc_data(target);
        if (moveblock)
            map_delblock(target);
        target->x = nx;
        target->y = ny;
        if (moveblock)
            map_addblock(target);
/*ダンス中にエフェクトは移動しないらしい
                if (sc_data && sc_data[SC_DANCING].timer!=-1){ //対象がダンス中なのでエフェクトも移動
                        struct skill_unit_group *sg=(struct skill_unit_group *)sc_data[SC_DANCING].val2;
                        if (sg)
                                skill_unit_move_unit_group(sg,target->m,dx,dy);
                }
*/
    }

    if (sd)
    {                           /* 画面内に入ってきたので表示 */
        map_foreachinmovearea(clif_pcinsight, target->m, nx - AREA_SIZE,
                               ny - AREA_SIZE, nx + AREA_SIZE, ny + AREA_SIZE,
                               -dx, -dy, BL_NUL, sd);
        if (count & 0x20000)
            sd->walktimer = -1;
    }
    else if (md)
    {
        map_foreachinmovearea(clif_mobinsight, target->m, nx - AREA_SIZE,
                               ny - AREA_SIZE, nx + AREA_SIZE, ny + AREA_SIZE,
                               -dx, -dy, BL_PC, md);
        if (count & 0x20000)
            md->state.state = prev_state;
    }

    skill_unit_move(target, gettick(), (count & 0xffff) + 7); /* スキルユニットの判定 */

    return 0;
}

/*
 * =========================================================================
 * スキル攻撃効果処理まとめ
 * flagの説明。16進図
 *      00XRTTff
 *  ff  = magicで計算に渡される）
 *      TT      = パケットのtype部分(0でデフォルト）
 *  X   = パケットのスキルLv
 *  R   = 予約（skill_area_subで使用する）
 *-------------------------------------------------------------------------
 */

int skill_attack(int attack_type, struct block_list *src,
                  struct block_list *dsrc, struct block_list *bl, int skillid,
                  int skilllv, unsigned int tick, int flag)
{
    struct Damage dmg;
    int type, lv, damage;

    rdamage = 0;
    nullpo_retr(0, src);
    nullpo_retr(0, dsrc);
    nullpo_retr(0, bl);

//何もしない判定ここから
    if (dsrc->m != bl->m)       //対象が同じマップにいなければ何もしない
        return 0;
    if (src->prev == NULL || dsrc->prev == NULL || bl->prev == NULL)    //prevよくわからない※
        return 0;
    if (src->type == BL_PC && pc_isdead((struct map_session_data *) src))  //術者？がPCですでに死んでいたら何もしない
        return 0;
    if (dsrc->type == BL_PC && pc_isdead((struct map_session_data *) dsrc))    //術者？がPCですでに死んでいたら何もしない
        return 0;
    if (bl->type == BL_PC && pc_isdead((struct map_session_data *) bl))    //対象がPCですでに死んでいたら何もしない
        return 0;

//何もしない判定ここまで

    type = -1;
    lv = (flag >> 20) & 0xf;
    dmg = battle_calc_attack(attack_type, src, bl, skillid, skilllv, flag & 0xff); //ダメージ計算

    damage = dmg.damage + dmg.damage2;

    if (lv == 15)
        lv = -1;

    if (flag & 0xff00)
        type = (flag & 0xff00) >> 8;

    if (damage <= 0 || damage < dmg.div_)   //吹き飛ばし判定？※
        dmg.blewcount = 0;

//使用者がPCの場合の処理ここから
    if (src->type == BL_PC)
    {
        struct map_session_data *sd = (struct map_session_data *) src;
        nullpo_retr(0, sd);
    }
//使用者がPCの場合の処理ここまで
//武器スキル？ここから
    //AppleGirl Was Here
    if (attack_type & BF_MAGIC && damage > 0 && src != bl && src == dsrc)
    {                           //Blah Blah
        if (bl->type == BL_PC)
        {                       //Blah Blah
            struct map_session_data *tsd = (struct map_session_data *) bl;
            if (tsd->magic_damage_return > 0)
            {                   //More Blah
                rdamage += damage * tsd->magic_damage_return / 100;
                if (rdamage < 1)
                    rdamage = 1;
            }
        }
    }
    //Stop Here
    if (attack_type & BF_WEAPON && damage > 0 && src != bl && src == dsrc)
    {                           //武器スキル＆ダメージあり＆使用者と対象者が違う＆src=dsrc
        if (dmg.flag & BF_SHORT)
        {                       //近距離攻撃時？※
            if (bl->type == BL_PC)
            {                   //対象がPCの時
                struct map_session_data *tsd = (struct map_session_data *) bl;
                nullpo_retr(0, tsd);
                if (tsd->short_weapon_damage_return > 0)
                {               //近距離攻撃跳ね返し？※
                    rdamage += damage * tsd->short_weapon_damage_return / 100;
                    if (rdamage < 1)
                        rdamage = 1;
                }
            }
        }
        else if (dmg.flag & BF_LONG)
        {                       //遠距離攻撃時？※
            if (bl->type == BL_PC)
            {                   //対象がPCの時
                struct map_session_data *tsd = (struct map_session_data *) bl;
                nullpo_retr(0, tsd);
                if (tsd->long_weapon_damage_return > 0)
                {               //遠距離攻撃跳ね返し？※
                    rdamage += damage * tsd->long_weapon_damage_return / 100;
                    if (rdamage < 1)
                        rdamage = 1;
                }
            }
        }
        if (rdamage > 0)
            clif_damage(src, src, tick, dmg.amotion, 0, rdamage, 1, 4, 0);
    }
//武器スキル？ここまで

    clif_skill_damage(dsrc, bl, tick, dmg.amotion, dmg.dmotion, damage,
                       dmg.div_, skillid, (lv != 0) ? lv : skilllv,
                       (skillid == 0) ? 5 : type);
    if (dmg.blewcount > 0)
    {
        skill_blown(dsrc, bl, dmg.blewcount);
        if (bl->type == BL_MOB)
            clif_fixmobpos((struct mob_data *) bl);
        else
            clif_fixpos(bl);
    }

    map_freeblock_lock();
    battle_damage(src, bl, damage, 0);
    /* ダメージがあるなら追加効果判定 */
    if (bl->prev != NULL)
    {
        struct map_session_data *sd = (struct map_session_data *) bl;
        nullpo_retr(0, sd);
        if (bl->type != BL_PC || (sd && !pc_isdead(sd)))
        {
            if (damage > 0)
                skill_additional_effect(src, bl, skillid, skilllv,
                                         attack_type, tick);
            if (bl->type == BL_MOB && src != bl)    /* スキル使用条件のMOBスキル */
            {
                struct mob_data *md = (struct mob_data *) bl;
                nullpo_retr(0, md);
                if (battle_config.mob_changetarget_byskill == 1)
                {
                    int target;
                    target = md->target_id;
                    if (src->type == BL_PC)
                        md->target_id = src->id;
                    mobskill_use(md, tick, MSC_SKILLUSED | (skillid << 16));
                    md->target_id = target;
                }
                else
                    mobskill_use(md, tick, MSC_SKILLUSED | (skillid << 16));
            }
        }
    }

    if (src->type == BL_PC && dmg.flag & BF_WEAPON && src != bl && src == dsrc
        && damage > 0)
    {
        struct map_session_data *sd = (struct map_session_data *) src;
        int hp = 0, sp = 0;
        nullpo_retr(0, sd);
        if (sd->hp_drain_rate && sd->hp_drain_per > 0 && dmg.damage > 0
            && MRAND(100) < sd->hp_drain_rate)
        {
            hp += (dmg.damage * sd->hp_drain_per) / 100;
            if (sd->hp_drain_rate > 0 && hp < 1)
                hp = 1;
            else if (sd->hp_drain_rate < 0 && hp > -1)
                hp = -1;
        }
        if (sd->hp_drain_rate_ && sd->hp_drain_per_ > 0 && dmg.damage2 > 0
            && MRAND(100) < sd->hp_drain_rate_)
        {
            hp += (dmg.damage2 * sd->hp_drain_per_) / 100;
            if (sd->hp_drain_rate_ > 0 && hp < 1)
                hp = 1;
            else if (sd->hp_drain_rate_ < 0 && hp > -1)
                hp = -1;
        }
        if (sd->sp_drain_rate > 0 && sd->sp_drain_per > 0 && dmg.damage > 0
            && MRAND(100) < sd->sp_drain_rate)
        {
            sp += (dmg.damage * sd->sp_drain_per) / 100;
            if (sd->sp_drain_rate > 0 && sp < 1)
                sp = 1;
            else if (sd->sp_drain_rate < 0 && sp > -1)
                sp = -1;
        }
        if (sd->sp_drain_rate_ > 0 && sd->sp_drain_per_ > 0 && dmg.damage2 > 0
            && MRAND(100) < sd->sp_drain_rate_)
        {
            sp += (dmg.damage2 * sd->sp_drain_per_) / 100;
            if (sd->sp_drain_rate_ > 0 && sp < 1)
                sp = 1;
            else if (sd->sp_drain_rate_ < 0 && sp > -1)
                sp = -1;
        }
        if (hp || sp)
            pc_heal(sd, hp, sp);
    }

    if (rdamage > 0)
        battle_damage(bl, src, rdamage, 0);

    map_freeblock_unlock();

    return (dmg.damage + dmg.damage2);  /* 与ダメを返す */
}

/*==========================================
 * スキル範囲攻撃用(map_foreachinareaから呼ばれる)
 * flagについて：16進図を確認
 * MSB <- 00fTffff ->LSB
 *      T       =ターゲット選択用(BCT_*)
 *  ffff=自由に使用可能
 *  0   =予約。0に固定
 *------------------------------------------
 */
typedef int (*SkillFunc) (struct block_list *, struct block_list *, int, int,
                          unsigned int, int);

/*==========================================
 *
 *------------------------------------------
 */
static void skill_timerskill_(timer_id, tick_t tick, custom_id_t id, custom_data_t data)
{
    struct map_session_data *sd = NULL;
    struct mob_data *md = NULL;
    struct block_list *src = map_id2bl(id), *target;
    struct skill_timerskill *skl = NULL;

    nullpo_retv(src);

    if (src->prev == NULL)
        return;

    if (src->type == BL_PC)
    {
        nullpo_retv(sd = (struct map_session_data *) src);
        skl = &sd->skilltimerskill[data];
    }
    else if (src->type == BL_MOB)
    {
        nullpo_retv(md = (struct mob_data *) src);
        skl = &md->skilltimerskill[data];
    }

    else
        return;

    nullpo_retv(skl);

    skl->timer = -1;
    if (skl->target_id)
    {
        target = map_id2bl(skl->target_id);
        if (target == NULL)
            return;
        if (target->prev == NULL)
            return;
        if (src->m != target->m)
            return;
        if (sd && pc_isdead(sd))
            return;
        if (target->type == BL_PC
            && pc_isdead((struct map_session_data *) target))
            return;

        skill_attack(skl->type, src, src, target, skl->skill_id,
                      skl->skill_lv, tick, skl->flag);
    }
}

/*==========================================
 *
 *------------------------------------------
 */
int skill_addtimerskill(struct block_list *src, unsigned int tick,
                         int target, int x, int y, int skill_id, int skill_lv,
                         int type, int flag)
{
    int i;

    nullpo_retr(1, src);

    if (src->type == BL_PC)
    {
        struct map_session_data *sd = (struct map_session_data *) src;
        nullpo_retr(1, sd);
        for (i = 0; i < MAX_SKILLTIMERSKILL; i++)
        {
            if (sd->skilltimerskill[i].timer == -1)
            {
                sd->skilltimerskill[i].timer =
                    add_timer(tick, skill_timerskill_, src->id, i);
                sd->skilltimerskill[i].src_id = src->id;
                sd->skilltimerskill[i].target_id = target;
                sd->skilltimerskill[i].skill_id = skill_id;
                sd->skilltimerskill[i].skill_lv = skill_lv;
                sd->skilltimerskill[i].map = src->m;
                sd->skilltimerskill[i].x = x;
                sd->skilltimerskill[i].y = y;
                sd->skilltimerskill[i].type = type;
                sd->skilltimerskill[i].flag = flag;

                return 0;
            }
        }
        return 1;
    }
    else if (src->type == BL_MOB)
    {
        struct mob_data *md = (struct mob_data *) src;
        nullpo_retr(1, md);
        for (i = 0; i < MAX_MOBSKILLTIMERSKILL; i++)
        {
            if (md->skilltimerskill[i].timer == -1)
            {
                md->skilltimerskill[i].timer =
                    add_timer(tick, skill_timerskill_, src->id, i);
                md->skilltimerskill[i].src_id = src->id;
                md->skilltimerskill[i].target_id = target;
                md->skilltimerskill[i].skill_id = skill_id;
                md->skilltimerskill[i].skill_lv = skill_lv;
                md->skilltimerskill[i].map = src->m;
                md->skilltimerskill[i].x = x;
                md->skilltimerskill[i].y = y;
                md->skilltimerskill[i].type = type;
                md->skilltimerskill[i].flag = flag;

                return 0;
            }
        }
        return 1;
    }

    return 1;
}

/*==========================================
 *
 *------------------------------------------
 */
int skill_cleartimerskill(struct block_list *src)
{
    int i;

    nullpo_retr(0, src);

    if (src->type == BL_PC)
    {
        struct map_session_data *sd = (struct map_session_data *) src;
        nullpo_retr(0, sd);
        for (i = 0; i < MAX_SKILLTIMERSKILL; i++)
        {
            if (sd->skilltimerskill[i].timer != -1)
            {
                delete_timer(sd->skilltimerskill[i].timer, skill_timerskill_);
                sd->skilltimerskill[i].timer = -1;
            }
        }
    }
    else if (src->type == BL_MOB)
    {
        struct mob_data *md = (struct mob_data *) src;
        nullpo_retr(0, md);
        for (i = 0; i < MAX_MOBSKILLTIMERSKILL; i++)
        {
            if (md->skilltimerskill[i].timer != -1)
            {
                delete_timer(md->skilltimerskill[i].timer, skill_timerskill_);
                md->skilltimerskill[i].timer = -1;
            }
        }
    }

    return 0;
}

/* 範囲スキル使用処理小分けここまで
 * -------------------------------------------------------------------------
 */

/*==========================================
 * スキルユニット設定処理
 *------------------------------------------
 */
struct skill_unit_group *skill_unitsetting(struct block_list *src,
                                            int skillid, int skilllv, int x,
                                            int y, int)
{
    struct skill_unit_group *group;
    int i, count = 1, limit = 10000, val1 = 0, val2 = 0;
    int target = BCT_ENEMY, interval = 1000, range = 0;

    nullpo_retr(0, src);

    nullpo_retr(NULL, group = skill_initunitgroup(src, count, skillid, skilllv, 0));
    group->limit = limit;
    group->val1 = val1;
    group->val2 = val2;
    group->target_flag = target;
    group->interval = interval;
    group->range = range;
    for (i = 0; i < count; i++)
    {
        struct skill_unit *unit;
        int ux = x, uy = y, val_1 = skilllv, val_2 = 0, limit_ =
            group->limit, alive = 1;
        int range_ = group->range;

        if (alive)
        {
            nullpo_retr(NULL, unit = skill_initunit(group, i, ux, uy));
            unit->val1 = val_1;
            unit->val2 = val_2;
            unit->limit = limit_;
            unit->range = range_;
        }
    }
    return group;
}

/*==========================================
 * スキルユニットの発動イベント
 *------------------------------------------
 */
static int skill_unit_onplace(struct skill_unit *src, struct block_list *bl,
                        unsigned int tick)
{
    struct skill_unit_group *sg;
    struct block_list *ss;
    struct skill_unit_group_tickset *ts;
    struct map_session_data *srcsd = NULL;
    int diff, goflag, splash_count = 0;

    nullpo_retr(0, src);
    nullpo_retr(0, bl);

    if (bl->prev == NULL || !src->alive
        || (bl->type == BL_PC && pc_isdead((struct map_session_data *) bl)))
        return 0;

    nullpo_retr(0, sg = src->group);
    nullpo_retr(0, ss = map_id2bl(sg->src_id));

    if (ss->type == BL_PC)
        nullpo_retr(0, srcsd = (struct map_session_data *) ss);

    if (bl->type != BL_PC && bl->type != BL_MOB)
        return 0;
    nullpo_retr(0, ts = skill_unitgrouptickset_search(bl, sg->group_id));
    diff = DIFF_TICK(tick, ts->tick);
    goflag = (diff > sg->interval || diff < 0);

    if (!goflag)
        return 0;
    ts->tick = tick;
    ts->group_id = sg->group_id;

    switch (sg->unit_id)
    {
        case 0x83:             /* サンクチュアリ */
        {
            int race = battle_get_race(bl);
            int damage_flag =
                (battle_check_undead(race, battle_get_elem_type(bl))
                 || race == 6) ? 1 : 0;

            if (battle_get_hp(bl) >= battle_get_max_hp(bl) && !damage_flag)
                break;

            if ((sg->val1--) <= 0)
            {
                skill_delunitgroup(sg);
                return 0;
            }
            if (!damage_flag)
            {
                int heal = sg->val2;
                if (bl->type == BL_PC
                    && ((struct map_session_data *) bl)->
                    special_state.no_magic_damage)
                    heal = 0;   /* 黄金蟲カード（ヒール量０） */
                battle_heal(NULL, bl, heal, 0, 0);
            }
            else
                skill_attack(BF_MAGIC, ss, &src->bl, bl, sg->skill_id,
                              sg->skill_lv, tick, 0);
        }
            break;

        case 0x84:             /* マグヌスエクソシズム */
        {
            int race = battle_get_race(bl);
            int damage_flag =
                (battle_check_undead(race, battle_get_elem_type(bl))
                 || race == 6) ? 1 : 0;

            if (!damage_flag)
                return 0;
            skill_attack(BF_MAGIC, ss, &src->bl, bl, sg->skill_id,
                          sg->skill_lv, tick, 0);
        }
            break;

        case 0x85:             /* ニューマ */
        case 0x7e:             /* セイフティウォール */
            break;

        case 0x86:             /* ロードオブヴァーミリオン(＆ストームガスト ＆グランドクロス) */
            skill_attack(BF_MAGIC, ss, &src->bl, bl, sg->skill_id,
                          sg->skill_lv, tick, 0);
            break;

        case 0x7f:             /* ファイヤーウォール */
            if ((src->val2--) > 0)
                skill_attack(BF_MAGIC, ss, &src->bl, bl,
                              sg->skill_id, sg->skill_lv, tick, 0);
            if (src->val2 <= 0)
                skill_delunit(src);
            break;

        case 0x87:             /* ファイアーピラー(発動前) */
            skill_delunit(src);
            skill_unitsetting(ss, sg->skill_id, sg->skill_lv, src->bl.x,
                               src->bl.y, 1);
            break;

        case 0x88:             /* ファイアーピラー(発動後) */
            if (DIFF_TICK(tick, sg->tick) < 150)
                skill_attack(BF_MAGIC, ss, &src->bl, bl, sg->skill_id,
                              sg->skill_lv, tick, 0);
            break;

        case 0x90:             /* スキッドトラップ */
        {
            int i, c = skill_get_blewcount(sg->skill_id, sg->skill_lv);
            for (i = 0; i < c; i++)
                skill_blown(&src->bl, bl, 1 | 0x30000);
            sg->unit_id = 0x8c;
            clif_changelook(&src->bl, LOOK_BASE, sg->unit_id);
            sg->limit = DIFF_TICK(tick, sg->tick) + 1500;
        }
            break;

        case 0x93:             /* ランドマイン */
            skill_attack(BF_MISC, ss, &src->bl, bl, sg->skill_id,
                          sg->skill_lv, tick, 0);
            sg->unit_id = 0x8c;
            clif_changelook(&src->bl, LOOK_BASE, 0x88);
            sg->limit = DIFF_TICK(tick, sg->tick) + 1500;
            break;

        case 0x8f:             /* ブラストマイン */
        case 0x94:             /* ショックウェーブトラップ */
        case 0x95:             /* サンドマン */
        case 0x96:             /* フラッシャー */
        case 0x97:             /* フリージングトラップ */
        case 0x98:             /* クレイモアートラップ */
            map_foreachinarea(skill_count_target, src->bl.m,
                               src->bl.x - src->range, src->bl.y - src->range,
                               src->bl.x + src->range, src->bl.y + src->range,
                               BL_NUL, &src->bl, &splash_count);
            map_foreachinarea(skill_trap_splash, src->bl.m,
                               src->bl.x - src->range, src->bl.y - src->range,
                               src->bl.x + src->range, src->bl.y + src->range,
                               BL_NUL, &src->bl, tick, splash_count);
            sg->unit_id = 0x8c;
            clif_changelook(&src->bl, LOOK_BASE, sg->unit_id);
            sg->limit = DIFF_TICK(tick, sg->tick) + 1500;
            break;

        case 0x91:             /* アンクルスネア */
        {
            struct status_change *sc_data = battle_get_sc_data(bl);
            if (sg->val2 == 0 && sc_data)
            {
                int moveblock = (bl->x / BLOCK_SIZE != src->bl.x / BLOCK_SIZE
                                  || bl->y / BLOCK_SIZE !=
                                  src->bl.y / BLOCK_SIZE);
                int sec = skill_get_time2(sg->skill_id,
                                            sg->skill_lv) -
                    battle_get_agi(bl) * 0.1;
                if (battle_get_mode(bl) & 0x20)
                    sec = sec / 5;
                battle_stopwalking(bl, 1);

                if (moveblock)
                    map_delblock(bl);
                bl->x = src->bl.x;
                bl->y = src->bl.y;
                if (moveblock)
                    map_addblock(bl);
                if (bl->type == BL_MOB)
                    clif_fixmobpos((struct mob_data *) bl);
                else
                    clif_fixpos(bl);
                sg->limit = DIFF_TICK(tick, sg->tick) + sec;
                sg->val2 = bl->id;
            }
        }
            break;

        case 0x80:             /* ワープポータル(発動後) */
            if (bl->type == BL_PC)
            {
                struct map_session_data *sd = (struct map_session_data *) bl;
                if (sd && src->bl.m == bl->m && src->bl.x == bl->x
                    && src->bl.y == bl->y && src->bl.x == sd->to_x
                    && src->bl.y == sd->to_y)
                {
                    if ((sg->val1--) > 0)
                    {
                        pc_setpos(sd, sg->valstr, sg->val2 >> 16,
                                    sg->val2 & 0xffff, 3);
                        if (sg->src_id == bl->id
                            || (strcmp(maps[src->bl.m].name, sg->valstr)
                                == 0 && src->bl.x == (sg->val2 >> 16)
                                && src->bl.y == (sg->val2 & 0xffff)))
                            skill_delunitgroup(sg);
                    }
                    else
                        skill_delunitgroup(sg);
                }
            }
            else if (bl->type == BL_MOB && battle_config.mob_warpportal)
            {
                int m = map_mapname2mapid(sg->valstr);
                mob_warp((struct mob_data *) bl, m, sg->val2 >> 16,
                          sg->val2 & 0xffff, 3);
            }
            break;

        case 0x8e:             /* クァグマイア */
        case 0x92:             /* ベノムダスト */
        case 0x9a:             /* ボルケーノ */
        case 0x9b:             /* デリュージ */
        case 0x9c:             /* バイオレントゲイル */
        case 0x9e:             /* 子守唄 */
        case 0x9f:             /* ニヨルドの宴 */
        case 0xa0:             /* 永遠の混沌 */
        case 0xa1:             /* 戦太鼓の響き */
        case 0xa2:             /* ニーベルングの指輪 */
        case 0xa3:             /* ロキの叫び */
        case 0xa4:             /* 深淵の中に */
        case 0xa5:             /* 不死身のジークフリード */
        case 0xa6:             /* 不協和音 */
        case 0xa7:             /* 口笛 */
        case 0xa8:             /* 夕陽のアサシンクロス */
        case 0xa9:             /* ブラギの詩 */
        case 0xab:             /* 自分勝手なダンス */
        case 0xac:             /* ハミング */
        case 0xad:             /* 私を忘れないで… */
        case 0xae:             /* 幸運のキス */
        case 0xaf:             /* サービスフォーユー */
        case 0xb4:
        case 0xaa:             /* イドゥンの林檎 */
            break;

        case 0xb1:             /* デモンストレーション */
            skill_attack(BF_WEAPON, ss, &src->bl, bl, sg->skill_id,
                          sg->skill_lv, tick, 0);
            if (bl->type == BL_PC && MRAND(100) < sg->skill_lv
                && battle_config.equipment_breaking)
                pc_breakweapon((struct map_session_data *) bl);
            break;
        case 0x99:             /* トーキーボックス */
            if (sg->src_id == bl->id)   //自分が踏んでも発動しない
                break;
            if (sg->val2 == 0)
            {
                clif_talkiebox(&src->bl, sg->valstr);
                sg->unit_id = 0x8c;
                clif_changelook(&src->bl, LOOK_BASE, sg->unit_id);
                sg->limit = DIFF_TICK(tick, sg->tick) + 5000;
                sg->val2 = -1;  //踏んだ
            }
            break;
        case 0xb2:             /* あなたを_会いたいです */
        case 0xb3:             /* ゴスペル */
        case 0xb6:             /* フォグウォール */
            //とりあえず何もしない
            break;

        case 0xb7:             /* スパイダーウェッブ */
            if (sg->val2 == 0)
            {
                int moveblock = (bl->x / BLOCK_SIZE != src->bl.x / BLOCK_SIZE
                                  || bl->y / BLOCK_SIZE !=
                                  src->bl.y / BLOCK_SIZE);
                skill_additional_effect(ss, bl, sg->skill_id, sg->skill_lv,
                                         BF_MISC, tick);
                if (moveblock)
                    map_delblock(bl);
                bl->x = (&src->bl)->x;
                bl->y = (&src->bl)->y;
                if (moveblock)
                    map_addblock(bl);
                if (bl->type == BL_MOB)
                    clif_fixmobpos((struct mob_data *) bl);
                else
                    clif_fixpos(bl);
                sg->limit =
                    DIFF_TICK(tick,
                               sg->tick) + skill_get_time2(sg->skill_id,
                                                            sg->skill_lv);
                sg->val2 = bl->id;
            }
            break;

/*      default:
                if (battle_config.error_log)
                        printf("skill_unit_onplace: Unknown skill unit id=%d block=%d\n",sg->unit_id,bl->id);
                break;*/
    }
    if (bl->type == BL_MOB && ss != bl) /* スキル使用条件のMOBスキル */
    {
        if (battle_config.mob_changetarget_byskill == 1)
        {
            int target = ((struct mob_data *) bl)->target_id;
            if (ss->type == BL_PC)
                ((struct mob_data *) bl)->target_id = ss->id;
            mobskill_use((struct mob_data *) bl, tick,
                          MSC_SKILLUSED | (sg->skill_id << 16));
            ((struct mob_data *) bl)->target_id = target;
        }
        else
            mobskill_use((struct mob_data *) bl, tick,
                          MSC_SKILLUSED | (sg->skill_id << 16));
    }

    return 0;
}

/*==========================================
 * スキルユニットから離脱する(もしくはしている)場合
 *------------------------------------------
 */
static int skill_unit_onout(struct skill_unit *src, struct block_list *bl,
                      unsigned int tick)
{
    struct skill_unit_group *sg;

    nullpo_retr(0, src);
    nullpo_retr(0, bl);
    nullpo_retr(0, sg = src->group);

    if (bl->prev == NULL || !src->alive)
        return 0;

    if (bl->type != BL_PC && bl->type != BL_MOB)
        return 0;

    switch (sg->unit_id)
    {
        case 0x91:
            if (map_id2bl(sg->val2) != bl)
                break;
            // else fall through
        case 0xb5:
        case 0xb8:
        case 0xb6:
        case 0xb7:
            sg->limit = DIFF_TICK(tick, sg->tick) + 1000;
            break;
    }
    skill_unitgrouptickset_delete(bl, sg->group_id);
    return 0;
}

/*==========================================
 * スキルユニットの削除イベント
 *------------------------------------------
 */
static int skill_unit_ondelete(struct skill_unit *src, struct block_list *bl,
                         unsigned int tick)
{
    struct skill_unit_group *sg;

    nullpo_retr(0, src);
    nullpo_retr(0, bl);
    nullpo_retr(0, sg = src->group);

    if (bl->prev == NULL || !src->alive)
        return 0;

    if (bl->type != BL_PC && bl->type != BL_MOB)
        return 0;

    switch (sg->unit_id)
    {
        case 0x85:             /* ニューマ */
        case 0x7e:             /* セイフティウォール */
        case 0x8e:             /* クァグマイヤ */
        case 0x9a:             /* ボルケーノ */
        case 0x9b:             /* デリュージ */
        case 0x9c:             /* バイオレントゲイル */
        case 0x9e:             /* 子守唄 */
        case 0x9f:             /* ニヨルドの宴 */
        case 0xa0:             /* 永遠の混沌 */
        case 0xa1:             /* 戦太鼓の響き */
        case 0xa2:             /* ニーベルングの指輪 */
        case 0xa3:             /* ロキの叫び */
        case 0xa4:             /* 深淵の中に */
        case 0xa5:             /* 不死身のジークフリード */
        case 0xa6:             /* 不協和音 */
        case 0xa7:             /* 口笛 */
        case 0xa8:             /* 夕陽のアサシンクロス */
        case 0xa9:             /* ブラギの詩 */
        case 0xaa:             /* イドゥンの林檎 */
        case 0xab:             /* 自分勝手なダンス */
        case 0xac:             /* ハミング */
        case 0xad:             /* 私を忘れないで… */
        case 0xae:             /* 幸運のキス */
        case 0xaf:             /* サービスフォーユー */
        case 0xb4:
            return skill_unit_onout(src, bl, tick);

/*      default:
                if (battle_config.error_log)
                        printf("skill_unit_ondelete: Unknown skill unit id=%d block=%d\n",sg->unit_id,bl->id);
                break;*/
    }
    skill_unitgrouptickset_delete(bl, sg->group_id);
    return 0;
}

/*==========================================
 * スキルユニットの限界イベント
 *------------------------------------------
 */
static int skill_unit_onlimit(struct skill_unit *src, unsigned int)
{
    struct skill_unit_group *sg;

    nullpo_retr(0, src);
    nullpo_retr(0, sg = src->group);

    switch (sg->unit_id)
    {
        case 0x81:             /* ワープポータル(発動前) */
        {
            struct skill_unit_group *group =
                skill_unitsetting(map_id2bl(sg->src_id), sg->skill_id,
                                   sg->skill_lv,
                                   src->bl.x, src->bl.y, 1);
            if (group == NULL)
                return 0;
            CREATE(group->valstr, char, 24);
            memcpy(group->valstr, sg->valstr, 24);
            group->val2 = sg->val2;
        }
            break;

        case 0x8d:             /* アイスウォール */
            map_setcell(src->bl.m, src->bl.x, src->bl.y, src->val2);
            clif_changemapcell(src->bl.m, src->bl.x, src->bl.y, src->val2,
                                1);
            break;
        case 0xb2:             /* あなたに会いたい */
        {
            struct map_session_data *sd = NULL;
            struct map_session_data *p_sd = NULL;
            if ((sd =
                 (struct map_session_data *) (map_id2bl(sg->src_id))) ==
                NULL)
                return 0;
            if ((p_sd = pc_get_partner(sd)) == NULL)
                return 0;

            pc_setpos(p_sd, maps[src->bl.m].name, src->bl.x, src->bl.y, 3);
        }
            break;
    }
    return 0;
}

/*==========================================
 * スキルユニットのダメージイベント
 *------------------------------------------
 */
int skill_unit_ondamaged(struct skill_unit *src, struct block_list *bl,
                          int damage, unsigned int)
{
    struct skill_unit_group *sg;

    nullpo_retr(0, src);
    nullpo_retr(0, sg = src->group);

    switch (sg->unit_id)
    {
        case 0x8d:             /* アイスウォール */
            src->val1 -= damage;
            break;
        case 0x8f:             /* ブラストマイン */
        case 0x98:             /* クレイモアートラップ */
            skill_blown(bl, &src->bl, 2);  //吹き飛ばしてみる
            break;
        default:
            damage = 0;
            break;
    }
    return damage;
}

/*---------------------------------------------------------------------------- */

/*==========================================
 * 詠唱時間計算
 *------------------------------------------
 */
int skill_castfix(struct block_list *bl, int time_)
{
    struct mob_data *md;        // [Valaris]
    int dex;
    int castrate = 100;
    int skill = 0;
    int lv = 0;
    int castnodex;

    nullpo_retr(0, bl);

    if (bl->type == BL_MOB)
    {                           // Crash fix [Valaris]
        md = (struct mob_data *) bl;
        skill = md->skillid;
        lv = md->skilllv;
    }

    dex = battle_get_dex(bl);

    if (skill > MAX_SKILL_DB || skill < 0)
        return 0;

    castnodex = skill_get_castnodex(skill, lv);

    if (time_ == 0)
        return 0;
    if (castnodex > 0 && bl->type == BL_PC)
        castrate = ((struct map_session_data *) bl)->castrate;
    else if (castnodex <= 0 && bl->type == BL_PC)
    {
        castrate = ((struct map_session_data *) bl)->castrate;
        time_ =
            time_ * castrate * (battle_config.castrate_dex_scale -
                               dex) / (battle_config.castrate_dex_scale *
                                       100);
        time_ = time_ * battle_config.cast_rate / 100;
    }
    return (time_ > 0) ? time_ : 0;
}

/*==========================================
 * ディレイ計算
 *------------------------------------------
 */
int skill_delayfix(struct block_list *bl, int time_)
{
    nullpo_retr(0, bl);

    if (time_ <= 0)
        return 0;

    if (bl->type == BL_PC)
    {
        if (battle_config.delay_dependon_dex)   /* dexの影響を計算する */
            time_ =
                time_ * (battle_config.castrate_dex_scale -
                        battle_get_dex(bl)) /
                battle_config.castrate_dex_scale;
        time_ = time_ * battle_config.delay_rate / 100;
    }
    return (time_ > 0) ? time_ : 0;
}

/*==========================================
 * スキル詠唱キャンセル
 *------------------------------------------
 */
int skill_castcancel(struct block_list *bl, int)
{
    int inf;

    nullpo_retr(0, bl);

    if (bl->type == BL_PC)
    {
        struct map_session_data *sd = (struct map_session_data *) bl;
        unsigned long tick = gettick();
        nullpo_retr(0, sd);
        sd->canact_tick = tick;
        sd->canmove_tick = tick;

        return 0;
    }
    else if (bl->type == BL_MOB)
    {
        struct mob_data *md = (struct mob_data *) bl;
        nullpo_retr(0, md);
        if (md->skilltimer != -1)
        {
            if ((inf = skill_get_inf(md->skillid)) == 2 || inf == 32)
                delete_timer(md->skilltimer, mobskill_castend_pos);
            else
                delete_timer(md->skilltimer, mobskill_castend_id);
            md->skilltimer = -1;
        }
        return 0;
    }
    return 1;
}

/*==========================================
 * イドゥンの林檎の回復処理(foreachinarea)
 *------------------------------------------
 */
static void skill_idun_heal(struct block_list *bl, va_list ap)
{
    struct skill_unit *unit;
    struct skill_unit_group *sg;
    int heal;

    nullpo_retv(bl);
    nullpo_retv(unit = va_arg(ap, struct skill_unit *));
    nullpo_retv(sg = unit->group);

    heal =
        30 + sg->skill_lv * 5 + ((sg->val1) >> 16) * 5 +
        ((sg->val1) & 0xfff) / 2;

    if (bl->type == BL_SKILL || bl->id == sg->src_id)
        return;

    if (bl->type == BL_PC || bl->type == BL_MOB)
    {
        battle_heal(NULL, bl, heal, 0, 0);
    }
}

/*==========================================
 * 指定範囲内でsrcに対して有効なターゲットのblの数を数える(foreachinarea)
 *------------------------------------------
 */
void skill_count_target(struct block_list *bl, va_list ap)
{
    struct block_list *src;
    int *c;

    nullpo_retv(bl);

    if ((src = va_arg(ap, struct block_list *)) == NULL)
        return;
    if ((c = va_arg(ap, int *)) == NULL)
        return;
    if (battle_check_target(src, bl, BCT_ENEMY) > 0)
        (*c)++;
}

/*==========================================
 * トラップ範囲処理(foreachinarea)
 *------------------------------------------
 */
void skill_trap_splash(struct block_list *bl, va_list ap)
{
    struct block_list *src;
    int tick;
    int splash_count;
    struct skill_unit *unit;
    struct skill_unit_group *sg;
    struct block_list *ss;
    int i;

    nullpo_retv(bl);
    nullpo_retv(src = va_arg(ap, struct block_list *));
    nullpo_retv(unit = (struct skill_unit *) src);
    nullpo_retv(sg = unit->group);
    nullpo_retv(ss = map_id2bl(sg->src_id));

    tick = va_arg(ap, int);
    splash_count = va_arg(ap, int);

    if (battle_check_target(src, bl, BCT_ENEMY) > 0)
    {
        switch (sg->unit_id)
        {
            case 0x95:         /* サンドマン */
            case 0x96:         /* フラッシャー */
            case 0x94:         /* ショックウェーブトラップ */
                skill_additional_effect(ss, bl, sg->skill_id, sg->skill_lv,
                                         BF_MISC, tick);
                break;
            case 0x8f:         /* ブラストマイン */
            case 0x98:         /* クレイモアートラップ */
                for (i = 0; i < splash_count; i++)
                {
                    skill_attack(BF_MISC, ss, src, bl, sg->skill_id,
                                  sg->skill_lv, tick,
                                  (sg->val2) ? 0x0500 : 0);
                }
            case 0x97:         /* フリージングトラップ */
                skill_attack(BF_WEAPON, ss, src, bl, sg->skill_id,
                              sg->skill_lv, tick, (sg->val2) ? 0x0500 : 0);
                break;
            default:
                break;
        }
    }
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
        if (battle_config.error_log)
            printf("skill_status_change_active: neither MOB nor PC !\n");
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
        if (battle_config.error_log)
            printf("skill_status_change_end: neither MOB nor PC !\n");
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
            pc_calcstatus((struct map_session_data *) bl, 0);  /* ステータス再計算 */
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
    int type = data;
    struct block_list *bl;
    struct map_session_data *sd = NULL;
    struct status_change *sc_data;
    //short *sc_count; //使ってない？

    if ((bl = map_id2bl(id)) == NULL)
        return;               //該当IDがすでに消滅しているというのはいかにもありそうなのでスルーしてみる
    nullpo_retv(sc_data = battle_get_sc_data(bl));

    if (bl->type == BL_PC)
        sd = (struct map_session_data *) bl;

    //sc_count=battle_get_sc_count(bl); //使ってない？

    if (sc_data[type].timer != tid)
    {
        if (battle_config.error_log)
            printf("skill_status_change_timer %d != %d\n", tid,
                    sc_data[type].timer);
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
                            pc_heal((struct map_session_data *) bl, -hp, 0);
                        }
                        else if (bl->type == BL_MOB)
                        {
                            struct mob_data *md;
                            if ((md = ((struct mob_data *) bl)) == NULL)
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
    if (bl->type == BL_SKILL)
        return 0;
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
        sd = (struct map_session_data *) bl;

        if (type == SC_POISON)
        {                       /* カードによる耐性 */
            if (sd && MRAND(10000) < sd->reseff[4])
            {
                if (battle_config.battle_log)
                    printf("PC %d skill_sc_start: cardによる異常耐性発動\n",
                            sd->bl.id);
                return 0;
            }
        }
    }
    else if (bl->type == BL_MOB)
    {
    }
    else
    {
        if (battle_config.error_log)
            printf("skill_status_change_start: neither MOB nor PC !\n");
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
            if (battle_config.error_log)
                printf("UnknownStatusChange [%d]\n", type);
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

/* クローキング検査（周りに移動不可能地帯があるか） */
int skill_check_cloaking(struct block_list *bl)
{
    static int dx[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static int dy[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    int end = 1, i;

    nullpo_retr(0, bl);

    if (bl->type == BL_PC && battle_config.pc_cloak_check_type & 1)
        return 0;
    if (bl->type == BL_MOB && battle_config.monster_cloak_check_type & 1)
        return 0;
    for (i = 0; i < sizeof(dx) / sizeof(dx[0]); i++)
    {
        int c = map_getcell(bl->m, bl->x + dx[i], bl->y + dy[i]);
        if (c == 1 || c == 5)
            end = 0;
    }
    return end;
}

/*
 *----------------------------------------------------------------------------
 * スキルユニット
 *----------------------------------------------------------------------------
 */

/*==========================================
 * スキルユニット初期化
 *------------------------------------------
 */
struct skill_unit *skill_initunit(struct skill_unit_group *group, int idx,
                                   int x, int y)
{
    struct skill_unit *unit;

    nullpo_retr(NULL, group);
    nullpo_retr(NULL, unit = &group->unit[idx]);

    if (!unit->alive)
        group->alive_count++;

    unit->bl.id = map_addobject(&unit->bl);
    unit->bl.type = BL_SKILL;
    unit->bl.m = group->map;
    unit->bl.x = x;
    unit->bl.y = y;
    unit->group = group;
    unit->val1 = unit->val2 = 0;
    unit->alive = 1;

    map_addblock(&unit->bl);
    clif_skill_setunit(unit);
    return unit;
}

static void skill_unit_timer_sub_ondelete(struct block_list *bl, va_list ap);
/*==========================================
 * スキルユニット削除
 *------------------------------------------
 */
int skill_delunit(struct skill_unit *unit)
{
    struct skill_unit_group *group;
    int range;

    nullpo_retr(0, unit);
    if (!unit->alive)
        return 0;
    nullpo_retr(0, group = unit->group);

    /* onlimitイベント呼び出し */
    skill_unit_onlimit(unit, gettick());

    /* ondeleteイベント呼び出し */
    range = group->range;
    map_foreachinarea(skill_unit_timer_sub_ondelete, unit->bl.m,
                       unit->bl.x - range, unit->bl.y - range,
                       unit->bl.x + range, unit->bl.y + range, BL_NUL, &unit->bl,
                       gettick());

    clif_skill_delunit(unit);

    unit->group = NULL;
    unit->alive = 0;
    map_delobjectnofree(unit->bl.id, BL_SKILL);
    if (group->alive_count > 0 && (--group->alive_count) <= 0)
        skill_delunitgroup(group);

    return 0;
}

/*==========================================
 * スキルユニットグループ初期化
 *------------------------------------------
 */
static int skill_unit_group_newid = 10;
struct skill_unit_group *skill_initunitgroup(struct block_list *src,
                                              int count, int skillid,
                                              int skilllv, int unit_id)
{
    int i;
    struct skill_unit_group *group = NULL, *list = NULL;
    int maxsug = 0;

    nullpo_retr(NULL, src);

    if (src->type == BL_PC)
    {
        list = ((struct map_session_data *) src)->skillunit;
        maxsug = MAX_SKILLUNITGROUP;
    }
    else if (src->type == BL_MOB)
    {
        list = ((struct mob_data *) src)->skillunit;
        maxsug = MAX_MOBSKILLUNITGROUP;
    }
    if (list)
    {
        for (i = 0; i < maxsug; i++)    /* 空いているもの検索 */
            if (list[i].group_id == 0)
            {
                group = &list[i];
                break;
            }

        if (group == NULL)
        {                       /* 空いてないので古いもの検索 */
            int j = 0;
            unsigned maxdiff = 0, x, tick = gettick();
            for (i = 0; i < maxsug; i++)
                if ((x = DIFF_TICK(tick, list[i].tick)) > maxdiff)
                {
                    maxdiff = x;
                    j = i;
                }
            skill_delunitgroup(&list[j]);
            group = &list[j];
        }
    }

    if (group == NULL)
    {
        printf("skill_initunitgroup: error unit group !\n");
        exit(1);
    }

    group->src_id = src->id;
    group->party_id = battle_get_party_id(src);
    group->group_id = skill_unit_group_newid++;
    if (skill_unit_group_newid <= 0)
        skill_unit_group_newid = 10;
    CREATE(group->unit, struct skill_unit, count);
    group->unit_count = count;
    group->val1 = group->val2 = 0;
    group->skill_id = skillid;
    group->skill_lv = skilllv;
    group->unit_id = unit_id;
    group->map = src->m;
    group->range = 0;
    group->limit = 10000;
    group->interval = 1000;
    group->tick = gettick();
    group->valstr = NULL;
    return group;
}

/*==========================================
 * スキルユニットグループ削除
 *------------------------------------------
 */
int skill_delunitgroup(struct skill_unit_group *group)
{
    int i;

    nullpo_retr(0, group);
    if (group->unit_count <= 0)
        return 0;

    group->alive_count = 0;
    if (group->unit != NULL)
    {
        for (i = 0; i < group->unit_count; i++)
            if (group->unit[i].alive)
                skill_delunit(&group->unit[i]);
    }
    if (group->valstr != NULL)
    {
        map_freeblock(group->valstr);
        group->valstr = NULL;
    }

    map_freeblock(group->unit);    /* free()の替わり */
    group->unit = NULL;
    group->src_id = 0;
    group->group_id = 0;
    group->unit_count = 0;
    return 0;
}

/*==========================================
 * スキルユニットグループ全削除
 *------------------------------------------
 */
int skill_clear_unitgroup(struct block_list *src)
{
    struct skill_unit_group *group = NULL;
    int maxsug = 0;

    nullpo_retr(0, src);

    if (src->type == BL_PC)
    {
        group = ((struct map_session_data *) src)->skillunit;
        maxsug = MAX_SKILLUNITGROUP;
    }
    else if (src->type == BL_MOB)
    {
        group = ((struct mob_data *) src)->skillunit;
        maxsug = MAX_MOBSKILLUNITGROUP;
    }
    if (group)
    {
        int i;
        for (i = 0; i < maxsug; i++)
            if (group[i].group_id > 0 && group[i].src_id == src->id)
                skill_delunitgroup(&group[i]);
    }
    return 0;
}

/*==========================================
 * スキルユニットグループの被影響tick検索
 *------------------------------------------
 */
struct skill_unit_group_tickset *skill_unitgrouptickset_search(struct
                                                                block_list
                                                                *bl,
                                                                int group_id)
{
    int i, j = 0, k, s = group_id % MAX_SKILLUNITGROUPTICKSET;
    struct skill_unit_group_tickset *set = NULL;

    nullpo_retr(0, bl);

    if (bl->type == BL_PC)
    {
        set = ((struct map_session_data *) bl)->skillunittick;
    }
    else
    {
        set = ((struct mob_data *) bl)->skillunittick;
    }
    if (set == NULL)
        return 0;
    for (i = 0; i < MAX_SKILLUNITGROUPTICKSET; i++)
        if (set[(k = (i + s) % MAX_SKILLUNITGROUPTICKSET)].group_id ==
            group_id)
            return &set[k];
        else if (set[k].group_id == 0)
            j = k;

    return &set[j];
}

/*==========================================
 * スキルユニットグループの被影響tick削除
 *------------------------------------------
 */
int skill_unitgrouptickset_delete(struct block_list *bl, int group_id)
{
    int i, s = group_id % MAX_SKILLUNITGROUPTICKSET;
    struct skill_unit_group_tickset *set = NULL, *ts;

    nullpo_retr(0, bl);

    if (bl->type == BL_PC)
    {
        set = ((struct map_session_data *) bl)->skillunittick;
    }
    else
    {
        set = ((struct mob_data *) bl)->skillunittick;
    }

    if (set != NULL)
    {

        for (i = 0; i < MAX_SKILLUNITGROUPTICKSET; i++)
            if ((ts =
                 &set[(i + s) % MAX_SKILLUNITGROUPTICKSET])->group_id ==
                group_id)
                ts->group_id = 0;

    }
    return 0;
}

/*==========================================
 * スキルユニットタイマー発動処理用(foreachinarea)
 *------------------------------------------
 */
static void skill_unit_timer_sub_onplace(struct block_list *bl, va_list ap)
{
    struct block_list *src;
    struct skill_unit *su;
    unsigned int tick;

    nullpo_retv(bl);
    src = va_arg(ap, struct block_list *);

    tick = va_arg(ap, unsigned int);
    su = (struct skill_unit *) src;

    if (su && su->alive)
    {
        struct skill_unit_group *sg;
        sg = su->group;
        if (sg && battle_check_target(src, bl, sg->target_flag) > 0)
            skill_unit_onplace(su, bl, tick);
    }
}

/*==========================================
 * スキルユニットタイマー削除処理用(foreachinarea)
 *------------------------------------------
 */
void skill_unit_timer_sub_ondelete(struct block_list *bl, va_list ap)
{
    struct block_list *src;
    struct skill_unit *su;
    unsigned int tick;

    nullpo_retv(bl);
    src = va_arg(ap, struct block_list *);

    tick = va_arg(ap, unsigned int);
    su = (struct skill_unit *) src;

    if (su && su->alive)
    {
        struct skill_unit_group *sg;
        sg = su->group;
        if (sg && battle_check_target(src, bl, sg->target_flag) > 0)
            skill_unit_ondelete(su, bl, tick);
    }
}

/*==========================================
 * スキルユニットタイマー処理用(foreachobject)
 *------------------------------------------
 */
static void skill_unit_timer_sub(struct block_list *bl, va_list ap)
{
    struct skill_unit *unit;
    struct skill_unit_group *group;
    int range;
    unsigned int tick;

    nullpo_retv(bl);
    nullpo_retv(unit = (struct skill_unit *) bl);
    nullpo_retv(group = unit->group);
    tick = va_arg(ap, unsigned int);

    if (!unit->alive)
        return;

    range = (unit->range != 0) ? unit->range : group->range;

    /* onplaceイベント呼び出し */
    if (unit->alive && unit->range >= 0)
    {
        map_foreachinarea(skill_unit_timer_sub_onplace, bl->m,
                           bl->x - range, bl->y - range, bl->x + range,
                           bl->y + range, BL_NUL, bl, tick);
        if (group->unit_id == 0xaa
            && DIFF_TICK(tick, group->tick) >= 6000 * group->val2)
        {
            map_foreachinarea(skill_idun_heal, bl->m,
                               bl->x - range, bl->y - range, bl->x + range,
                               bl->y + range, BL_NUL, unit);
            group->val2++;
        }
    }
    /* 時間切れ削除 */
    if (unit->alive &&
        (DIFF_TICK(tick, group->tick) >= group->limit
         || DIFF_TICK(tick, group->tick) >= unit->limit))
    {
        switch (group->unit_id)
        {

            case 0x8f:         /* ブラストマイン */
                group->unit_id = 0x8c;
                clif_changelook(bl, LOOK_BASE, group->unit_id);
                group->limit = DIFF_TICK(tick + 1500, group->tick);
                unit->limit = DIFF_TICK(tick + 1500, group->tick);
                break;
            case 0x90:         /* スキッドトラップ */
            case 0x91:         /* アンクルスネア */
            case 0x93:         /* ランドマイン */
            case 0x94:         /* ショックウェーブトラップ */
            case 0x95:         /* サンドマン */
            case 0x96:         /* フラッシャー */
            case 0x97:         /* フリージングトラップ */
            case 0x98:         /* クレイモアートラップ */
            case 0x99:         /* トーキーボックス */
            {
                struct block_list *src = map_id2bl(group->src_id);
                if (group->unit_id == 0x91 && group->val2);
                else
                {
                    if (src && src->type == BL_PC)
                    {
                        struct item item_tmp;
                        memset(&item_tmp, 0, sizeof(item_tmp));
                        item_tmp.nameid = 1065;
                        item_tmp.identify = 1;
                        map_addflooritem(&item_tmp, 1, bl->m, bl->x, bl->y, NULL, NULL, NULL);  // 罠返還
                    }
                }
            }
            default:
                skill_delunit(unit);
        }
    }

    if (group->unit_id == 0x8d)
    {
        unit->val1 -= 5;
        if (unit->val1 <= 0 && unit->limit + group->tick > tick + 700)
            unit->limit = DIFF_TICK(tick + 700, group->tick);
    }
}

/*==========================================
 * スキルユニットタイマー処理
 *------------------------------------------
 */
static void skill_unit_timer(timer_id, tick_t tick, custom_id_t, custom_data_t)
{
    map_freeblock_lock();

    map_foreachobject(skill_unit_timer_sub, BL_SKILL, tick);

    map_freeblock_unlock();
}

/*==========================================
 * スキルユニット移動時処理用(foreachinarea)
 *------------------------------------------
 */
static void skill_unit_out_all_sub(struct block_list *bl, va_list ap)
{
    struct skill_unit *unit;
    struct skill_unit_group *group;
    struct block_list *src;
    int range;
    unsigned int tick;

    nullpo_retv(bl);
    nullpo_retv(src = va_arg(ap, struct block_list *));
    nullpo_retv(unit = (struct skill_unit *) bl);
    nullpo_retv(group = unit->group);

    tick = va_arg(ap, unsigned int);

    if (!unit->alive || src->prev == NULL)
        return;

    range = (unit->range != 0) ? unit->range : group->range;

    if (range < 0 || battle_check_target(bl, src, group->target_flag) <= 0)
        return;

    if (src->x >= bl->x - range && src->x <= bl->x + range &&
        src->y >= bl->y - range && src->y <= bl->y + range)
        skill_unit_onout(unit, src, tick);
}

/*==========================================
 * スキルユニット移動時処理
 *------------------------------------------
 */
int skill_unit_out_all(struct block_list *bl, unsigned int tick, int range)
{
    nullpo_retr(0, bl);

    if (bl->prev == NULL)
        return 0;

    if (range < 7)
        range = 7;
    map_foreachinarea(skill_unit_out_all_sub,
                       bl->m, bl->x - range, bl->y - range, bl->x + range,
                       bl->y + range, BL_SKILL, bl, tick);

    return 0;
}

/*==========================================
 * スキルユニット移動時処理用(foreachinarea)
 *------------------------------------------
 */
static void skill_unit_move_sub(struct block_list *bl, va_list ap)
{
    struct skill_unit *unit;
    struct skill_unit_group *group;
    struct block_list *src;
    int range;
    unsigned int tick;

    nullpo_retv(bl);
    nullpo_retv(unit = (struct skill_unit *) bl);
    nullpo_retv(src = va_arg(ap, struct block_list *));

    tick = va_arg(ap, unsigned int);

    if (!unit->alive || src->prev == NULL)
        return;

    if ((group = unit->group) == NULL)
        return;
    range = (unit->range != 0) ? unit->range : group->range;

    if (range < 0 || battle_check_target(bl, src, group->target_flag) <= 0)
        return;

    if (src->x >= bl->x - range && src->x <= bl->x + range &&
        src->y >= bl->y - range && src->y <= bl->y + range)
        skill_unit_onplace(unit, src, tick);
    else
        skill_unit_onout(unit, src, tick);
}

/*==========================================
 * スキルユニット移動時処理
 *------------------------------------------
 */
int skill_unit_move(struct block_list *bl, unsigned int tick, int range)
{
    nullpo_retr(0, bl);

    if (bl->prev == NULL)
        return 0;

    if (range < 7)
        range = 7;
    map_foreachinarea(skill_unit_move_sub,
                       bl->m, bl->x - range, bl->y - range, bl->x + range,
                       bl->y + range, BL_SKILL, bl, tick);

    return 0;
}

/*==========================================
 * スキルユニット自体の移動時処理(foreachinarea)
 *------------------------------------------
 */
static void skill_unit_move_unit_group_sub(struct block_list *bl, va_list ap)
{
    struct skill_unit *unit;
    struct skill_unit_group *group;
    struct block_list *src;
    int range;
    unsigned int tick;

    nullpo_retv(bl);
    nullpo_retv(src = va_arg(ap, struct block_list *));
    nullpo_retv(unit = (struct skill_unit *) src);
    nullpo_retv(group = unit->group);

    tick = va_arg(ap, unsigned int);

    if (!unit->alive || bl->prev == NULL)
        return;

    range = (unit->range != 0) ? unit->range : group->range;

    if (range < 0 || battle_check_target(src, bl, group->target_flag) <= 0)
        return;
    if (bl->x >= src->x - range && bl->x <= src->x + range &&
        bl->y >= src->y - range && bl->y <= src->y + range)
        skill_unit_onplace(unit, bl, tick);
    else
        skill_unit_onout(unit, bl, tick);
}

/*==========================================
 * スキルユニット自体の移動時処理
 * 引数はグループと移動量
 *------------------------------------------
 */
int skill_unit_move_unit_group(struct skill_unit_group *group, int m, int dx,
                                int dy)
{
    nullpo_retr(0, group);

    if (group->unit_count <= 0)
        return 0;

    if (group->unit != NULL)
    {
        if (!battle_config.unit_movement_type)
        {
            int i;
            for (i = 0; i < group->unit_count; i++)
            {
                struct skill_unit *unit = &group->unit[i];
                if (unit->alive && !(m == unit->bl.m && dx == 0 && dy == 0))
                {
                    int range = unit->range;
                    map_delblock(&unit->bl);
                    unit->bl.m = m;
                    unit->bl.x += dx;
                    unit->bl.y += dy;
                    map_addblock(&unit->bl);
                    clif_skill_setunit(unit);
                    if (range > 0)
                    {
                        if (range < 7)
                            range = 7;
                        map_foreachinarea(skill_unit_move_unit_group_sub,
                                           unit->bl.m, unit->bl.x - range,
                                           unit->bl.y - range,
                                           unit->bl.x + range,
                                           unit->bl.y + range, BL_NUL, &unit->bl,
                                           gettick());
                    }
                }
            }
        }
        else
        {
            int i, j, *r_flag, *s_flag, *m_flag;
            struct skill_unit *unit1;
            struct skill_unit *unit2;
            r_flag = (int *) malloc(sizeof(int) * group->unit_count);
            s_flag = (int *) malloc(sizeof(int) * group->unit_count);
            m_flag = (int *) malloc(sizeof(int) * group->unit_count);
            memset(r_flag, 0, sizeof(int) * group->unit_count);   // 継承フラグ
            memset(s_flag, 0, sizeof(int) * group->unit_count);   // 継承フラグ
            memset(m_flag, 0, sizeof(int) * group->unit_count);   // 継承フラグ

            //先にフラグを全部決める
            for (i = 0; i < group->unit_count; i++)
            {
                int move_check = 0;    // かぶりフラグ
                unit1 = &group->unit[i];
                for (j = 0; j < group->unit_count; j++)
                {
                    unit2 = &group->unit[j];
                    if (unit1->bl.m == m && unit1->bl.x + dx == unit2->bl.x
                        && unit1->bl.y + dy == unit2->bl.y)
                    {
                        //移動先にユニットがかぶってたら
                        s_flag[i] = 1;  // 移動前のユニットナンバーの継承フラグon
                        r_flag[j] = 1;  // かぶるユニットナンバーの残留フラグon
                        move_check = 1; //ユニットがかぶった。
                        break;
                    }
                }
                if (!move_check)    // ユニットがかぶってなかったら
                    m_flag[i] = 1;  // 移動前ユニットナンバーの移動フラグon
            }

            //フラグに基づいてユニット移動
            for (i = 0; i < group->unit_count; i++)
            {
                unit1 = &group->unit[i];
                if (m_flag[i])
                {               // 移動フラグがonで
                    if (!r_flag[i])
                    {           // 残留フラグがoffなら
                        //単純移動(rangeも継承の必要無し)
                        int range = unit1->range;
                        map_delblock(&unit1->bl);
                        unit1->bl.m = m;
                        unit1->bl.x += dx;
                        unit1->bl.y += dy;
                        map_addblock(&unit1->bl);
                        clif_skill_setunit(unit1);
                        if (range > 0)
                        {
                            if (range < 7)
                                range = 7;
                            map_foreachinarea(skill_unit_move_unit_group_sub,
                                               unit1->bl.m,
                                               unit1->bl.x - range,
                                               unit1->bl.y - range,
                                               unit1->bl.x + range,
                                               unit1->bl.y + range, BL_NUL,
                                               &unit1->bl, gettick());
                        }
                    }
                    else
                    {           // 残留フラグがonなら
                        //空ユニットになるので、継承可能なユニットを探す
                        for (j = 0; j < group->unit_count; j++)
                        {
                            unit2 = &group->unit[j];
                            if (s_flag[j] && !r_flag[j])
                            {
                                // 継承移動(range継承付き)
                                int range = unit1->range;
                                map_delblock(&unit2->bl);
                                unit2->bl.m = m;
                                unit2->bl.x = unit1->bl.x + dx;
                                unit2->bl.y = unit1->bl.y + dy;
                                unit2->range = unit1->range;
                                map_addblock(&unit2->bl);
                                clif_skill_setunit(unit2);
                                if (range > 0)
                                {
                                    if (range < 7)
                                        range = 7;
                                    map_foreachinarea
                                        (skill_unit_move_unit_group_sub,
                                         unit2->bl.m, unit2->bl.x - range,
                                         unit2->bl.y - range,
                                         unit2->bl.x + range,
                                         unit2->bl.y + range, BL_NUL, &unit2->bl,
                                         gettick());
                                }
                                s_flag[j] = 0;  // 継承完了したのでoff
                                break;
                            }
                        }
                    }
                }
            }
            free(r_flag);
            free(s_flag);
            free(m_flag);
        }
    }
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

/*==========================================
 * スキル関係ファイル読み込み
 * skill_db.txt スキルデータ
 * skill_cast_db.txt スキルの詠唱時間とディレイデータ
 *------------------------------------------
 */
static int skill_readdb(void)
{
    int i, j, k, l;
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
    while (fgets(line, 1020, fp))
    {
        char *split[50], *split2[MAX_SKILL_LEVEL];
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
            fprintf(stderr, "Incomplete skill db data online (%d entries)\n",
                     j);
            continue;
        }

        i = atoi(split[0]);
        if (i < 0 || i > MAX_SKILL_DB)
            continue;

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[1]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].range[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);
        skill_db[i].hit = atoi(split[2]);
        skill_db[i].inf = atoi(split[3]);
        skill_db[i].pl = atoi(split[4]);
        skill_db[i].nk = atoi(split[5]);
        skill_db[i].max_raise = atoi(split[6]);
        skill_db[i].max = atoi(split[7]);

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[8]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].num[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);

        if (strcasecmp(split[9], "yes") == 0)
            skill_db[i].castcancel = 1;
        else
            skill_db[i].castcancel = 0;
        skill_db[i].cast_def_rate = atoi(split[9]);
        skill_db[i].inf2 = atoi(split[10]);
        skill_db[i].maxcount = atoi(split[11]);
        if (strcasecmp(split[13], "weapon") == 0)
            skill_db[i].skill_type = BF_WEAPON;
        else if (strcasecmp(split[12], "magic") == 0)
            skill_db[i].skill_type = BF_MAGIC;
        else if (strcasecmp(split[12], "misc") == 0)
            skill_db[i].skill_type = BF_MISC;
        else
            skill_db[i].skill_type = 0;
        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[14]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].blewcount[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);

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

    fp = fopen_("db/skill_require_db.txt", "r");
    if (fp == NULL)
    {
        printf("can't read db/skill_require_db.txt\n");
        return 1;
    }
    while (fgets(line, 1020, fp))
    {
        char *split[51], *split2[MAX_SKILL_LEVEL];
        if (line[0] == '/' && line[1] == '/')
            continue;
        for (j = 0, p = line; j < 30 && p; j++)
        {
            while (*p == '\t' || *p == ' ')
                p++;
            split[j] = p;
            p = strchr(p, ',');
            if (p)
                *p++ = 0;
        }
        if (split[29] == NULL || j < 30)
            continue;

        i = atoi(split[0]);
        if (i < 0 || i > MAX_SKILL_DB)
            continue;

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[1]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].hp[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[2]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].mhp[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[3]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].sp[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[4]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].hp_rate[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[5]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].sp_rate[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[6]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].zeny[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[7]; j < 32 && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < 32 && split2[k]; k++)
        {
            l = atoi(split2[k]);
            if (l == 99)
            {
                skill_db[i].weapon = 0xffffffff;
                break;
            }
            else
                skill_db[i].weapon |= 1 << l;
        }

        if (strcasecmp(split[8], "hiding") == 0)
            skill_db[i].state = ST_HIDING;
        else if (strcasecmp(split[8], "cloaking") == 0)
            skill_db[i].state = ST_CLOAKING;
        else if (strcasecmp(split[8], "hidden") == 0)
            skill_db[i].state = ST_HIDDEN;
        else if (strcasecmp(split[8], "shield") == 0)
            skill_db[i].state = ST_SHIELD;
        else if (strcasecmp(split[8], "sight") == 0)
            skill_db[i].state = ST_SIGHT;
        else if (strcasecmp(split[8], "explosionspirits") == 0)
            skill_db[i].state = ST_EXPLOSIONSPIRITS;
        else if (strcasecmp(split[8], "recover_weight_rate") == 0)
            skill_db[i].state = ST_RECOV_WEIGHT_RATE;
        else if (strcasecmp(split[8], "move_enable") == 0)
            skill_db[i].state = ST_MOVE_ENABLE;
        else if (strcasecmp(split[8], "water") == 0)
            skill_db[i].state = ST_WATER;
        else
            skill_db[i].state = ST_NONE;

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[9]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        skill_db[i].itemid[0] = atoi(split[10]);
        skill_db[i].amount[0] = atoi(split[11]);
        skill_db[i].itemid[1] = atoi(split[12]);
        skill_db[i].amount[1] = atoi(split[13]);
        skill_db[i].itemid[2] = atoi(split[14]);
        skill_db[i].amount[2] = atoi(split[15]);
        skill_db[i].itemid[3] = atoi(split[16]);
        skill_db[i].amount[3] = atoi(split[17]);
        skill_db[i].itemid[4] = atoi(split[18]);
        skill_db[i].amount[4] = atoi(split[19]);
        skill_db[i].itemid[5] = atoi(split[20]);
        skill_db[i].amount[5] = atoi(split[21]);
        skill_db[i].itemid[6] = atoi(split[22]);
        skill_db[i].amount[6] = atoi(split[23]);
        skill_db[i].itemid[7] = atoi(split[24]);
        skill_db[i].amount[7] = atoi(split[25]);
        skill_db[i].itemid[8] = atoi(split[26]);
        skill_db[i].amount[8] = atoi(split[27]);
        skill_db[i].itemid[9] = atoi(split[28]);
        skill_db[i].amount[9] = atoi(split[29]);
    }
    fclose_(fp);
    printf("read db/skill_require_db.txt done\n");

    /* ? */
    fp = fopen_("db/skill_cast_db.txt", "r");
    if (fp == NULL)
    {
        printf("can't read db/skill_cast_db.txt\n");
        return 1;
    }
    while (fgets(line, 1020, fp))
    {
        char *split[50], *split2[MAX_SKILL_LEVEL];
        memset(split, 0, sizeof(split));  // [Valaris] thanks to fov
        if (line[0] == '/' && line[1] == '/')
            continue;
        for (j = 0, p = line; j < 5 && p; j++)
        {
            while (*p == '\t' || *p == ' ')
                p++;
            split[j] = p;
            p = strchr(p, ',');
            if (p)
                *p++ = 0;
        }
        if (split[4] == NULL || j < 5)
            continue;

        i = atoi(split[0]);
        if (i < 0 || i > MAX_SKILL_DB)
            continue;

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[1]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].cast[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[2]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].delay[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[3]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].upkeep_time[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[4]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].upkeep_time2[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);
    }
    fclose_(fp);
    printf("read db/skill_cast_db.txt done\n");

    fp = fopen_("db/skill_castnodex_db.txt", "r");
    if (fp == NULL)
    {
        printf("can't read db/skill_castnodex_db.txt\n");
        return 1;
    }
    while (fgets(line, 1020, fp))
    {
        char *split[50], *split2[MAX_SKILL_LEVEL];
        memset(split, 0, sizeof(split));
        if (line[0] == '/' && line[1] == '/')
            continue;
        for (j = 0, p = line; j < 2 && p; j++)
        {
            while (*p == '\t' || *p == ' ')
                p++;
            split[j] = p;
            p = strchr(p, ',');
            if (p)
                *p++ = 0;
        }

        i = atoi(split[0]);
        if (i < 0 || i > MAX_SKILL_DB)
            continue;

        memset(split2, 0, sizeof(split2));
        for (j = 0, p = split[1]; j < MAX_SKILL_LEVEL && p; j++)
        {
            split2[j] = p;
            p = strchr(p, ':');
            if (p)
                *p++ = 0;
        }
        for (k = 0; k < MAX_SKILL_LEVEL; k++)
            skill_db[i].castnodex[k] =
                (split2[k]) ? atoi(split2[k]) : atoi(split2[0]);
    }
    fclose_(fp);
    printf("read db/skill_castnodex_db.txt done\n");

    return 0;
}

void skill_reload(void)
{
    /*
     *
     * <empty skill database>
     * <?>
     *
     */

    do_init_skill();
}

/*==========================================
 * スキル関係初期化処理
 *------------------------------------------
 */
int do_init_skill(void)
{
    skill_readdb();

    add_timer_interval(gettick() + SKILLUNITTIMER_INVERVAL,
                        skill_unit_timer, 0, 0, SKILLUNITTIMER_INVERVAL);

    return 0;
}
