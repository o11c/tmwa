#include "battle.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../common/timer.hpp"
#include "../common/nullpo.hpp"

#include "clif.hpp"
#include "itemdb.hpp"
#include "map.hpp"
#include "mob.hpp"
#include "pc.hpp"
#include "skill.hpp"
#include "../common/socket.hpp"
#include "../common/mt_rand.hpp"

static int battle_attr_fix (int damage, int atk_elem, int def_elem);
static int battle_stopattack (struct block_list *bl);
static int battle_get_class (struct block_list *bl);
static int battle_get_hit (struct block_list *bl);
static int battle_get_flee (struct block_list *bl);
static int battle_get_flee2 (struct block_list *bl);
static int battle_get_def2 (struct block_list *bl);
static int battle_get_mdef2 (struct block_list *bl);
static int battle_get_baseatk (struct block_list *bl);
static int battle_get_atk (struct block_list *bl);
static int battle_get_atk2 (struct block_list *bl);
static int battle_get_attack_element (struct block_list *bl);
static int battle_get_attack_element2 (struct block_list *bl);
static int battle_get_size (struct block_list *bl);


int  attr_fix_table[4][10][10];

struct Battle_Config battle_config;

/*==========================================
 * 自分をロックしている対象の数を返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_counttargeted (struct block_list *bl, struct block_list *src,
                          int target_lv)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_PC)
        return pc_counttargeted ((struct map_session_data *) bl, src,
                                 target_lv);
    else if (bl->type == BL_MOB)
        return mob_counttargeted ((struct mob_data *) bl, src, target_lv);
    return 0;
}

/*==========================================
 * 対象のClassを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_class (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return ((struct mob_data *) bl)->mob_class;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return 0;
    else
        return 0;
}

/// which way the object is facing
Direction battle_get_dir (struct block_list *bl)
{
    nullpo_retr (DIR_S, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return ((struct mob_data *) bl)->dir;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return ((struct map_session_data *) bl)->dir;
    else
        return DIR_S;
}

/*==========================================
 * 対象のレベルを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_lv (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return ((struct mob_data *) bl)->stats[MOB_LV];
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return ((struct map_session_data *) bl)->status.base_level;
    else
        return 0;
}

/*==========================================
 * 対象の射程を返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_range (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return mob_db[((struct mob_data *) bl)->mob_class].range;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return ((struct map_session_data *) bl)->attackrange;
    else
        return 0;
}

/*==========================================
 * 対象のHPを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_hp (struct block_list *bl)
{
    nullpo_retr (1, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return ((struct mob_data *) bl)->hp;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return ((struct map_session_data *) bl)->status.hp;
    else
        return 1;
}

/*==========================================
 * 対象のMHPを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_max_hp (struct block_list *bl)
{
    nullpo_retr (1, bl);
    if (bl->type == BL_PC && ((struct map_session_data *) bl))
        return ((struct map_session_data *) bl)->status.max_hp;
    else
    {
        int  max_hp = 1;
        if (bl->type == BL_MOB && ((struct mob_data *) bl))
        {
            max_hp = ((struct mob_data *) bl)->stats[MOB_MAX_HP];
            if (mob_db[((struct mob_data *) bl)->mob_class].mexp > 0)
            {
                if (battle_config.mvp_hp_rate != 100)
                    max_hp = (max_hp * battle_config.mvp_hp_rate) / 100;
            }
            else
            {
                if (battle_config.monster_hp_rate != 100)
                    max_hp = (max_hp * battle_config.monster_hp_rate) / 100;
            }
        }
        if (max_hp < 1)
            max_hp = 1;
        return max_hp;
    }
    return 1;
}

/*==========================================
 * 対象のStrを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_str (struct block_list *bl)
{
    int  str = 0;

    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && ((struct mob_data *) bl))
        str = ((struct mob_data *) bl)->stats[MOB_STR];
    else if (bl->type == BL_PC && ((struct map_session_data *) bl))
        return ((struct map_session_data *) bl)->paramc[0];

    if (str < 0)
        str = 0;
    return str;
}

/*==========================================
 * 対象のAgiを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */

int battle_get_agi (struct block_list *bl)
{
    int  agi = 0;

    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        agi = ((struct mob_data *) bl)->stats[MOB_AGI];
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        agi = ((struct map_session_data *) bl)->paramc[1];

    if (agi < 0)
        agi = 0;
    return agi;
}

/*==========================================
 * 対象のVitを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_vit (struct block_list *bl)
{
    int  vit = 0;

    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        vit = ((struct mob_data *) bl)->stats[MOB_VIT];
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        vit = ((struct map_session_data *) bl)->paramc[2];

    if (vit < 0)
        vit = 0;
    return vit;
}

/*==========================================
 * 対象のIntを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_int (struct block_list *bl)
{
    int  int_ = 0;
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        int_ = ((struct mob_data *) bl)->stats[MOB_INT];
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        int_ = ((struct map_session_data *) bl)->paramc[3];

    if (int_ < 0)
        int_ = 0;
    return int_;
}

/*==========================================
 * 対象のDexを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_dex (struct block_list *bl)
{
    int  dex = 0;

    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        dex = ((struct mob_data *) bl)->stats[MOB_DEX];
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        dex = ((struct map_session_data *) bl)->paramc[4];

    if (dex < 0)
        dex = 0;
    return dex;
}

/*==========================================
 * 対象のLukを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_luk (struct block_list *bl)
{
    int  luk = 0;

    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        luk = ((struct mob_data *) bl)->stats[MOB_LUK];
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        luk = ((struct map_session_data *) bl)->paramc[5];

    if (luk < 0)
        luk = 0;
    return luk;
}

/*==========================================
 * 対象のFleeを返す(汎用)
 * 戻りは整数で1以上
 *------------------------------------------
 */
int battle_get_flee (struct block_list *bl)
{
    int  flee = 1;
    struct status_change *sc_data;

    nullpo_retr (1, bl);
    sc_data = battle_get_sc_data (bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
        flee = ((struct map_session_data *) bl)->flee;
    else
        flee = battle_get_agi (bl) + battle_get_lv (bl);

    if (sc_data)
    {
        if (battle_is_unarmed (bl))
            flee += (skill_power_bl (bl, TMW_BRAWLING) >> 3);   // +25 for 200
        flee += skill_power_bl (bl, TMW_SPEED) >> 3;
    }
    if (flee < 1)
        flee = 1;
    return flee;
}

/*==========================================
 * 対象のHitを返す(汎用)
 * 戻りは整数で1以上
 *------------------------------------------
 */
int battle_get_hit (struct block_list *bl)
{
    int  hit = 1;
    struct status_change *sc_data;

    nullpo_retr (1, bl);
    sc_data = battle_get_sc_data (bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
        hit = ((struct map_session_data *) bl)->hit;
    else
        hit = battle_get_dex (bl) + battle_get_lv (bl);

    if (sc_data)
    {
        if (battle_is_unarmed (bl))
            hit += (skill_power_bl (bl, TMW_BRAWLING) >> 4);    // +12 for 200
    }
    if (hit < 1)
        hit = 1;
    return hit;
}

/*==========================================
 * 対象の完全回避を返す(汎用)
 * 戻りは整数で1以上
 *------------------------------------------
 */
int battle_get_flee2 (struct block_list *bl)
{
    int  flee2 = 1;
    struct status_change *sc_data;

    nullpo_retr (1, bl);
    sc_data = battle_get_sc_data (bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
    {
        flee2 = battle_get_luk (bl) + 10;
        flee2 +=
            ((struct map_session_data *) bl)->flee2 -
            (((struct map_session_data *) bl)->paramc[5] + 10);
    }
    else
        flee2 = battle_get_luk (bl) + 1;

    if (sc_data)
    {
        if (battle_is_unarmed (bl))
            flee2 += (skill_power_bl (bl, TMW_BRAWLING) >> 3);  // +25 for 200
        flee2 += skill_power_bl (bl, TMW_SPEED) >> 3;
    }
    if (flee2 < 1)
        flee2 = 1;
    return flee2;
}

/*==========================================
 * 対象のクリティカルを返す(汎用)
 * 戻りは整数で1以上
 *------------------------------------------
 */
static int battle_get_critical (struct block_list *bl)
{
    int  critical = 1;

    nullpo_retr (1, bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
    {
        critical = battle_get_luk (bl) * 2 + 10;
        critical +=
            ((struct map_session_data *) bl)->critical -
            ((((struct map_session_data *) bl)->paramc[5] * 3) + 10);
    }
    else
        critical = battle_get_luk (bl) * 3 + 1;

    if (critical < 1)
        critical = 1;
    return critical;
}

/*==========================================
 * base_atkの取得
 * 戻りは整数で1以上
 *------------------------------------------
 */
int battle_get_baseatk (struct block_list *bl)
{
    int  batk = 1;

    nullpo_retr (1, bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
        batk = ((struct map_session_data *) bl)->base_atk;  //設定されているbase_atk
    else
    {                           //それ以外なら
        int  str, dstr;
        str = battle_get_str (bl);  //STR
        dstr = str / 10;
        batk = dstr * dstr + str;   //base_atkを計算する
    }
    if (batk < 1)
        batk = 1;               //base_atkは最低でも1
    return batk;
}

/*==========================================
 * 対象のAtkを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_atk (struct block_list *bl)
{
    int  atk = 0;

    nullpo_retr (0, bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
        atk = ((struct map_session_data *) bl)->watk;
    else if (bl->type == BL_MOB && (struct mob_data *) bl)
        atk = ((struct mob_data *) bl)->stats[MOB_ATK1];

    if (atk < 0)
        atk = 0;
    return atk;
}

/*==========================================
 * 対象の左手Atkを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
static int battle_get_atk_ (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
    {
        int  atk = ((struct map_session_data *) bl)->watk_;

        return atk;
    }
    else
        return 0;
}

/*==========================================
 * 対象のAtk2を返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_atk2 (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
        return ((struct map_session_data *) bl)->watk2;
    else
    {
        int  atk2 = 0;
        if (bl->type == BL_MOB && (struct mob_data *) bl)
            atk2 = ((struct mob_data *) bl)->stats[MOB_ATK2];

        if (atk2 < 0)
            atk2 = 0;
        return atk2;
    }
    return 0;
}

/*==========================================
 * 対象の左手Atk2を返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
static int battle_get_atk_2 (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_PC)
        return ((struct map_session_data *) bl)->watk_2;
    else
        return 0;
}

/*==========================================
 * 対象のMAtk1を返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
static int battle_get_matk1 (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB)
    {
        int  matk, int_ = battle_get_int (bl);
        matk = int_ + (int_ / 5) * (int_ / 5);

        return matk;
    }
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return ((struct map_session_data *) bl)->matk1;
    else
        return 0;
}

/*==========================================
 * 対象のMAtk2を返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
static int battle_get_matk2 (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB)
    {
        int  matk, int_ = battle_get_int (bl);
        matk = int_ + (int_ / 7) * (int_ / 7);

        return matk;
    }
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return ((struct map_session_data *) bl)->matk2;
    else
        return 0;
}

/*==========================================
 * 対象のDefを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_def (struct block_list *bl)
{
    struct status_change *sc_data;
    int  def = 0, skilltimer = -1, skillid = 0;

    nullpo_retr (0, bl);
    sc_data = battle_get_sc_data (bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
    {
        def = ((struct map_session_data *) bl)->def;
        skilltimer = ((struct map_session_data *) bl)->skilltimer;
        skillid = ((struct map_session_data *) bl)->skillid;
    }
    else if (bl->type == BL_MOB && (struct mob_data *) bl)
    {
        def = ((struct mob_data *) bl)->stats[MOB_DEF];
        skilltimer = ((struct mob_data *) bl)->skilltimer;
        skillid = ((struct mob_data *) bl)->skillid;
    }

    if (def < 1000000)
    {
        if (sc_data)
        {
            if (sc_data[SC_POISON].timer != -1 && bl->type != BL_PC)
                def = def * 75 / 100;
        }
        //詠唱中は詠唱時減算率に基づいて減算
        if (skilltimer != -1)
        {
            int  def_rate = skill_get_castdef (skillid);
            if (def_rate != 0)
                def = (def * (100 - def_rate)) / 100;
        }
    }
    if (def < 0)
        def = 0;
    return def;
}

/*==========================================
 * 対象のMDefを返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_mdef (struct block_list *bl)
{
    struct status_change *sc_data;
    int  mdef = 0;

    nullpo_retr (0, bl);
    sc_data = battle_get_sc_data (bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
        mdef = ((struct map_session_data *) bl)->mdef;
    else if (bl->type == BL_MOB && (struct mob_data *) bl)
        mdef = ((struct mob_data *) bl)->stats[MOB_MDEF];

    if (mdef < 1000000)
    {
        if (sc_data)
        {
            //バリアー状態時はMDEF100
            if (mdef < 90 && sc_data[SC_MBARRIER].timer != -1)
            {
                mdef += sc_data[SC_MBARRIER].val1;
                if (mdef > 90)
                    mdef = 90;
            }
        }
    }
    if (mdef < 0)
        mdef = 0;
    return mdef;
}

/*==========================================
 * 対象のDef2を返す(汎用)
 * 戻りは整数で1以上
 *------------------------------------------
 */
int battle_get_def2 (struct block_list *bl)
{
    struct status_change *sc_data;
    int  def2 = 1;

    nullpo_retr (1, bl);
    sc_data = battle_get_sc_data (bl);
    if (bl->type == BL_PC)
        def2 = ((struct map_session_data *) bl)->def2;
    else if (bl->type == BL_MOB)
        def2 = ((struct mob_data *) bl)->stats[MOB_VIT];

    if (sc_data)
    {
        if (sc_data[SC_POISON].timer != -1 && bl->type != BL_PC)
            def2 = def2 * 75 / 100;
    }
    if (def2 < 1)
        def2 = 1;
    return def2;
}

/*==========================================
 * 対象のMDef2を返す(汎用)
 * 戻りは整数で0以上
 *------------------------------------------
 */
int battle_get_mdef2 (struct block_list *bl)
{
    int  mdef2 = 0;

    nullpo_retr (0, bl);
    if (bl->type == BL_MOB)
        mdef2 =
            ((struct mob_data *) bl)->stats[MOB_INT] +
            (((struct mob_data *) bl)->stats[MOB_VIT] >> 1);
    else if (bl->type == BL_PC)
        mdef2 =
            ((struct map_session_data *) bl)->mdef2 +
            (((struct map_session_data *) bl)->paramc[2] >> 1);
    if (mdef2 < 0)
        mdef2 = 0;
    return mdef2;
}

/*==========================================
 * 対象のSpeed(移動速度)を返す(汎用)
 * 戻りは整数で1以上
 * Speedは小さいほうが移動速度が速い
 *------------------------------------------
 */
int battle_get_speed (struct block_list *bl)
{
    nullpo_retr (1000, bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
        return ((struct map_session_data *) bl)->speed;
    else
    {
        int  speed = 1000;
        if (bl->type == BL_MOB && (struct mob_data *) bl)
            speed = ((struct mob_data *) bl)->stats[MOB_SPEED];

        if (speed < 1)
            speed = 1;
        return speed;
    }

    return 1000;
}

/*==========================================
 * 対象のaDelay(攻撃時ディレイ)を返す(汎用)
 * aDelayは小さいほうが攻撃速度が速い
 *------------------------------------------
 */
int battle_get_adelay (struct block_list *bl)
{
    nullpo_retr (4000, bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
        return (((struct map_session_data *) bl)->aspd << 1);
    else
    {
        struct status_change *sc_data = battle_get_sc_data (bl);
        int  adelay = 4000, aspd_rate = 100;
        if (bl->type == BL_MOB && (struct mob_data *) bl)
            adelay = ((struct mob_data *) bl)->stats[MOB_ADELAY];

        if (sc_data)
        {
            if (sc_data[SC_SPEEDPOTION0].timer != -1)
                aspd_rate -= sc_data[SC_SPEEDPOTION0].val1;
            // Fate's `haste' spell works the same as the above
            if (sc_data[SC_HASTE].timer != -1)
                aspd_rate -= sc_data[SC_HASTE].val1;
        }

        if (aspd_rate != 100)
            adelay = adelay * aspd_rate / 100;
        if (adelay < battle_config.monster_max_aspd << 1)
            adelay = battle_config.monster_max_aspd << 1;
        return adelay;
    }
    return 4000;
}

int battle_get_amotion (struct block_list *bl)
{
    nullpo_retr (2000, bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
        return ((struct map_session_data *) bl)->amotion;
    else
    {
        struct status_change *sc_data = battle_get_sc_data (bl);
        int  amotion = 2000, aspd_rate = 100;
        if (bl->type == BL_MOB && (struct mob_data *) bl)
            amotion = mob_db[((struct mob_data *) bl)->mob_class].amotion;

        if (sc_data)
        {
            if (sc_data[SC_SPEEDPOTION0].timer != -1)
                aspd_rate -= sc_data[SC_SPEEDPOTION0].val1;
            if (sc_data[SC_HASTE].timer != -1)
                aspd_rate -= sc_data[SC_HASTE].val1;
        }

        if (aspd_rate != 100)
            amotion = amotion * aspd_rate / 100;
        if (amotion < battle_config.monster_max_aspd)
            amotion = battle_config.monster_max_aspd;
        return amotion;
    }
    return 2000;
}

int battle_get_dmotion (struct block_list *bl)
{
    int  ret;

    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
    {
        ret = mob_db[((struct mob_data *) bl)->mob_class].dmotion;
        if (battle_config.monster_damage_delay_rate != 100)
            ret = ret * battle_config.monster_damage_delay_rate / 400;
    }
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
    {
        ret = ((struct map_session_data *) bl)->dmotion;
        if (battle_config.pc_damage_delay_rate != 100)
            ret = ret * battle_config.pc_damage_delay_rate / 400;
    }
    else
        return 2000;

    return ret;
}

int battle_get_element (struct block_list *bl)
{
    int  ret = 20;

    nullpo_retr (ret, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)   // 10の位＝Lv*2、１の位＝属性
        ret = ((struct mob_data *) bl)->def_ele;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        ret = 20 + ((struct map_session_data *) bl)->def_ele;   // 防御属性Lv1

    return ret;
}

int battle_get_attack_element (struct block_list *bl)
{
    int  ret = 0;

    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        ret = 0;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        ret = ((struct map_session_data *) bl)->atk_ele;

    return ret;
}

int battle_get_attack_element2 (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
    {
        int  ret = ((struct map_session_data *) bl)->atk_ele_;
        return ret;
    }
    return 0;
}

int battle_get_party_id (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_PC && (struct map_session_data *) bl)
        return ((struct map_session_data *) bl)->status.party_id;
    else if (bl->type == BL_MOB && (struct mob_data *) bl)
    {
        struct mob_data *md = (struct mob_data *) bl;
        if (md->master_id > 0)
            return -md->master_id;
        return -md->bl.id;
    }
    else if (bl->type == BL_SKILL && (struct skill_unit *) bl)
        return ((struct skill_unit *) bl)->group->party_id;
    else
        return 0;
}

int battle_get_race (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return mob_db[((struct mob_data *) bl)->mob_class].race;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return 7;
    else
        return 0;
}

int battle_get_size (struct block_list *bl)
{
    nullpo_retr (1, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return mob_db[((struct mob_data *) bl)->mob_class].size;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return 1;
    else
        return 1;
}

int battle_get_mode (struct block_list *bl)
{
    nullpo_retr (0x01, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return mob_db[((struct mob_data *) bl)->mob_class].mode;
    else
        return 0x01;            // とりあえず動くということで1
}

int battle_get_mexp (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
    {
        const struct mob_data *mob = (struct mob_data *) bl;
        const int retval =
            (mob_db[mob->mob_class].mexp *
             (int) (mob->stats[MOB_XP_BONUS])) >> MOB_XP_BONUS_SHIFT;
        fprintf (stderr, "Modifier of %x: -> %d\n", mob->stats[MOB_XP_BONUS],
                 retval);
        return retval;
    }
    else
        return 0;
}

int battle_get_stat (int stat_id /* SP_VIT or similar */ ,
                     struct block_list *bl)
{
    switch (stat_id)
    {
        case SP_STR:
            return battle_get_str (bl);
        case SP_AGI:
            return battle_get_agi (bl);
        case SP_DEX:
            return battle_get_dex (bl);
        case SP_VIT:
            return battle_get_vit (bl);
        case SP_INT:
            return battle_get_int (bl);
        case SP_LUK:
            return battle_get_luk (bl);
        default:
            return 0;
    }
}

// StatusChange系の所得
struct status_change *battle_get_sc_data (struct block_list *bl)
{
    nullpo_retr (NULL, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return ((struct mob_data *) bl)->sc_data;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return ((struct map_session_data *) bl)->sc_data;
    return NULL;
}

short *battle_get_sc_count (struct block_list *bl)
{
    nullpo_retr (NULL, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return &((struct mob_data *) bl)->sc_count;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return &((struct map_session_data *) bl)->sc_count;
    return NULL;
}

short *battle_get_opt1 (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return &((struct mob_data *) bl)->opt1;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return &((struct map_session_data *) bl)->opt1;
    else if (bl->type == BL_NPC && (struct npc_data *) bl)
        return &((struct npc_data *) bl)->opt1;
    return 0;
}

short *battle_get_opt2 (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return &((struct mob_data *) bl)->opt2;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return &((struct map_session_data *) bl)->opt2;
    else if (bl->type == BL_NPC && (struct npc_data *) bl)
        return &((struct npc_data *) bl)->opt2;
    return 0;
}

short *battle_get_opt3 (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return &((struct mob_data *) bl)->opt3;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return &((struct map_session_data *) bl)->opt3;
    else if (bl->type == BL_NPC && (struct npc_data *) bl)
        return &((struct npc_data *) bl)->opt3;
    return 0;
}

short *battle_get_option (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB && (struct mob_data *) bl)
        return &((struct mob_data *) bl)->option;
    else if (bl->type == BL_PC && (struct map_session_data *) bl)
        return &((struct map_session_data *) bl)->status.option;
    else if (bl->type == BL_NPC && (struct npc_data *) bl)
        return &((struct npc_data *) bl)->option;
    return 0;
}

//-------------------------------------------------------------------

// ダメージの遅延
struct battle_delay_damage_
{
    struct block_list *src, *target;
    int  damage;
    int  flag;
};
static void battle_delay_damage_sub (timer_id, tick_t, custom_id_t id, custom_data_t data)
{
    struct battle_delay_damage_ *dat = (struct battle_delay_damage_ *) data;
    if (dat && map_id2bl (id) == dat->src && dat->target->prev != NULL)
        battle_damage (dat->src, dat->target, dat->damage, dat->flag);
    free (dat);
}

int battle_delay_damage (unsigned int tick, struct block_list *src,
                         struct block_list *target, int damage, int flag)
{
    struct battle_delay_damage_ *dat;
    CREATE (dat, struct battle_delay_damage_, 1);

    nullpo_retr (0, src);
    nullpo_retr (0, target);

    dat->src = src;
    dat->target = target;
    dat->damage = damage;
    dat->flag = flag;
    add_timer (tick, battle_delay_damage_sub, src->id, (int) dat);
    return 0;
}

// 実際にHPを操作
int battle_damage (struct block_list *bl, struct block_list *target,
                   int damage, int flag)
{
    nullpo_retr (0, target);    //blはNULLで呼ばれることがあるので他でチェック

    if (damage == 0)
        return 0;

    if (target->prev == NULL)
        return 0;

    if (bl)
    {
        if (bl->prev == NULL)
            return 0;

//        if (bl->type == BL_PC)
//            sd = (struct map_session_data *) bl;
    }

    if (damage < 0)
        return battle_heal (bl, target, -damage, 0, flag);

    if (target->type == BL_MOB)
    {                           // MOB
        struct mob_data *md = (struct mob_data *) target;
        if (md && md->skilltimer != -1 && md->state.skillcastcancel)    // 詠唱妨害
            skill_castcancel (target, 0);
        return mob_damage (bl, md, damage, 0);
    }
    else if (target->type == BL_PC)
    {                           // PC

        struct map_session_data *tsd = (struct map_session_data *) target;

        if (tsd && tsd->skilltimer != -1)
        {                       // 詠唱妨害
            // フェンカードや妨害されないスキルかの検査
            if (!tsd->special_state.no_castcancel
                && tsd->state.skillcastcancel
                && !tsd->special_state.no_castcancel2)
                skill_castcancel (target, 0);
        }

        return pc_damage (bl, tsd, damage);

    }
    else if (target->type == BL_SKILL)
        return skill_unit_ondamaged ((struct skill_unit *) target, bl, damage,
                                     gettick ());
    return 0;
}

int battle_heal (struct block_list *bl, struct block_list *target, int hp,
                 int sp, int flag)
{
    nullpo_retr (0, target);    //blはNULLで呼ばれることがあるので他でチェック

    if (target->type == BL_PC
        && pc_isdead ((struct map_session_data *) target))
        return 0;
    if (hp == 0 && sp == 0)
        return 0;

    if (hp < 0)
        return battle_damage (bl, target, -hp, flag);

    if (target->type == BL_MOB)
        return mob_heal ((struct mob_data *) target, hp);
    else if (target->type == BL_PC)
        return pc_heal ((struct map_session_data *) target, hp, sp);
    return 0;
}

// 攻撃停止
int battle_stopattack (struct block_list *bl)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB)
        return mob_stopattack ((struct mob_data *) bl);
    else if (bl->type == BL_PC)
        return pc_stopattack ((struct map_session_data *) bl);
    return 0;
}

// 移動停止
int battle_stopwalking (struct block_list *bl, int type)
{
    nullpo_retr (0, bl);
    if (bl->type == BL_MOB)
        return mob_stop_walking ((struct mob_data *) bl, type);
    else if (bl->type == BL_PC)
        return pc_stop_walking ((struct map_session_data *) bl, type);
    return 0;
}

/*==========================================
 * ダメージの属性修正
 *------------------------------------------
 */
int battle_attr_fix (int damage, int atk_elem, int def_elem)
{
    int  def_type = def_elem % 10, def_lv = def_elem / 10 / 2;

    if (atk_elem < 0 || atk_elem > 9 || def_type < 0 || def_type > 9 ||
        def_lv < 1 || def_lv > 4)
    {                           // 属 性値がおかしいのでとりあえずそのまま返す
        if (battle_config.error_log)
            printf
                ("battle_attr_fix: unknown attr type: atk=%d def_type=%d def_lv=%d\n",
                 atk_elem, def_type, def_lv);
        return damage;
    }

    return damage * attr_fix_table[def_lv - 1][atk_elem][def_type] / 100;
}

/*==========================================
 * ダメージ最終計算
 *------------------------------------------
 */
int battle_calc_damage (struct block_list *, struct block_list *bl,
                        int damage, int div_, int, int,
                        int flag)
{
    struct mob_data *md = NULL;

    nullpo_retr (0, bl);

    // int class_ = battle_get_class (bl);
    if (bl->type == BL_MOB)
        md = (struct mob_data *) bl;

    if (battle_config.skill_min_damage || flag & BF_MISC)
    {
        if (div_ < 255)
        {
            if (damage > 0 && damage < div_)
                damage = div_;
        }
        else if (damage > 0 && damage < 3)
            damage = 3;
    }

    if (md != NULL && md->hp > 0 && damage > 0) // 反撃などのMOBスキル判定
        mobskill_event (md, flag);

    return damage;
}

/*==========================================
 * 修練ダメージ
 *------------------------------------------
 */
static int battle_addmastery (struct map_session_data *, struct block_list *,
                              int dmg, int) __attribute__((deprecated));
static int battle_addmastery (struct map_session_data *, struct block_list *,
                              int dmg, int)
{
    return dmg;
}

static struct Damage battle_calc_mob_weapon_attack (struct block_list *src,
                                                    struct block_list *target,
                                                    int skill_num,
                                                    int skill_lv, int)
{
    struct map_session_data *tsd = NULL;
    struct mob_data *md = (struct mob_data *) src, *tmd = NULL;
    int  hitrate, flee, cri = 0, atkmin, atkmax;
    int target_count = 1;
    int  def1 = battle_get_def (target);
    int  def2 = battle_get_def2 (target);
    int  t_vit = battle_get_vit (target);
    struct Damage wd;
    int  damage, damage2 = 0, type, div_, blewcount =
        skill_get_blewcount (skill_num, skill_lv);
    int  flag, ac_flag = 0, dmg_lv = 0;
    int  t_mode = 0, s_race = 0, s_ele = 0;

    //return前の処理があるので情報出力部のみ変更
    if (src == NULL || target == NULL || md == NULL)
    {
        nullpo_info (NLP_MARK);
        memset (&wd, 0, sizeof (wd));
        return wd;
    }

    s_race = battle_get_race (src);
    s_ele = battle_get_attack_element (src);

    // ターゲット
    if (target->type == BL_PC)
        tsd = (struct map_session_data *) target;
    else if (target->type == BL_MOB)
        tmd = (struct mob_data *) target;
    t_mode = battle_get_mode (target);

    flag = BF_SHORT | BF_WEAPON | BF_NORMAL;    // 攻撃の種類の設定

    // 回避率計算、回避判定は後で
    flee = battle_get_flee (target);
    if (battle_config.agi_penaly_type > 0
        || battle_config.vit_penaly_type > 0)
        target_count +=
            battle_counttargeted (target, src,
                                  battle_config.agi_penaly_count_lv);
    if (battle_config.agi_penaly_type > 0)
    {
        if (target_count >= battle_config.agi_penaly_count)
        {
            if (battle_config.agi_penaly_type == 1)
                flee =
                    (flee *
                     (100 -
                      (target_count -
                       (battle_config.agi_penaly_count -
                        1)) * battle_config.agi_penaly_num)) / 100;
            else if (battle_config.agi_penaly_type == 2)
                flee -=
                    (target_count -
                     (battle_config.agi_penaly_count -
                      1)) * battle_config.agi_penaly_num;
            if (flee < 1)
                flee = 1;
        }
    }
    hitrate = battle_get_hit (src) - flee + 80;

    type = 0;                   // normal
    div_ = 1;                   // single attack

//    luk = battle_get_luk (src);

    if (battle_config.enemy_str)
        damage = battle_get_baseatk (src);
    else
        damage = 0;
    atkmin = battle_get_atk (src);
    atkmax = battle_get_atk2 (src);
    if (mob_db[md->mob_class].range > 3)
        flag = (flag & ~BF_RANGEMASK) | BF_LONG;

    if (atkmin > atkmax)
        atkmin = atkmax;


    cri = battle_get_critical (src);
    cri -= battle_get_luk (target) * 3;
    if (battle_config.enemy_critical_rate != 100)
    {
        cri = cri * battle_config.enemy_critical_rate / 100;
        if (cri < 1)
            cri = 1;
    }

    if (ac_flag)
        cri = 1000;

    if (tsd && tsd->critical_def)
        cri = cri * (100 - tsd->critical_def) / 100;

    if (skill_num == 0 && skill_lv >= 0 && battle_config.enemy_critical && (MRAND (1000)) < cri)   // 判定（スキルの場合は無視）
        // 敵の判定
    {
        damage += atkmax;
        type = 0x0a;
    }
    else
    {
        int  vitbonusmax;

        if (atkmax > atkmin)
            damage += atkmin + MRAND ((atkmax - atkmin + 1));
        else
            damage += atkmin;

        if (skill_num > 0)
        {
            int  i;
            if ((i = skill_get_pl (skill_num)) > 0)
                s_ele = i;

            flag = (flag & ~BF_SKILLMASK) | BF_SKILL;
        }

        {
            // 対 象の防御力によるダメージの減少
            // ディバインプロテクション（ここでいいのかな？）
            if (def1 < 1000000)
            {                   //DEF, VIT無視
                int  t_def;
                target_count =
                    1 + battle_counttargeted (target, src,
                                              battle_config.vit_penaly_count_lv);
                if (battle_config.vit_penaly_type > 0)
                {
                    if (target_count >= battle_config.vit_penaly_count)
                    {
                        if (battle_config.vit_penaly_type == 1)
                        {
                            def1 =
                                (def1 *
                                 (100 -
                                  (target_count -
                                   (battle_config.vit_penaly_count -
                                    1)) * battle_config.vit_penaly_num)) /
                                100;
                            def2 =
                                (def2 *
                                 (100 -
                                  (target_count -
                                   (battle_config.vit_penaly_count -
                                    1)) * battle_config.vit_penaly_num)) /
                                100;
                            t_vit =
                                (t_vit *
                                 (100 -
                                  (target_count -
                                   (battle_config.vit_penaly_count -
                                    1)) * battle_config.vit_penaly_num)) /
                                100;
                        }
                        else if (battle_config.vit_penaly_type == 2)
                        {
                            def1 -=
                                (target_count -
                                 (battle_config.vit_penaly_count -
                                  1)) * battle_config.vit_penaly_num;
                            def2 -=
                                (target_count -
                                 (battle_config.vit_penaly_count -
                                  1)) * battle_config.vit_penaly_num;
                            t_vit -=
                                (target_count -
                                 (battle_config.vit_penaly_count -
                                  1)) * battle_config.vit_penaly_num;
                        }
                        if (def1 < 0)
                            def1 = 0;
                        if (def2 < 1)
                            def2 = 1;
                        if (t_vit < 1)
                            t_vit = 1;
                    }
                }
                t_def = def2 * 8 / 10;

                vitbonusmax = (t_vit / 20) * (t_vit / 20) - 1;
                if (battle_config.monster_defense_type)
                {
                    damage =
                        damage - (def1 * battle_config.monster_defense_type) -
                        t_def -
                        ((vitbonusmax < 1) ? 0 : MRAND ((vitbonusmax + 1)));
                }
                else
                {
                    damage =
                        damage * (100 - def1) / 100 - t_def -
                        ((vitbonusmax < 1) ? 0 : MRAND ((vitbonusmax + 1)));
                }
            }
        }
    }

    // 0未満だった場合1に補正
    if (damage < 1)
        damage = 1;

    // 回避修正
    if (hitrate < 1000000)
        hitrate = ((hitrate > 95) ? 95 : ((hitrate < 5) ? 5 : hitrate));
    if (type == 0 && MRAND (100) >= hitrate)
    {
        damage = damage2 = 0;
        dmg_lv = ATK_FLEE;
    }
    else
    {
        dmg_lv = ATK_DEF;
    }

    if (tsd)
    {
        int  cardfix = 100, i;
        cardfix = cardfix * (100 - tsd->subele[s_ele]) / 100;   // 属 性によるダメージ耐性
        cardfix = cardfix * (100 - tsd->subrace[s_race]) / 100; // 種族によるダメージ耐性
        if (mob_db[md->mob_class].mode & 0x20)
            cardfix = cardfix * (100 - tsd->subrace[10]) / 100;
        else
            cardfix = cardfix * (100 - tsd->subrace[11]) / 100;
        for (i = 0; i < tsd->add_def_class_count; i++)
        {
            if (tsd->add_def_classid[i] == md->mob_class)
            {
                cardfix = cardfix * (100 - tsd->add_def_classrate[i]) / 100;
                break;
            }
        }
        if (flag & BF_LONG)
            cardfix = cardfix * (100 - tsd->long_attack_def_rate) / 100;
        if (flag & BF_SHORT)
            cardfix = cardfix * (100 - tsd->near_attack_def_rate) / 100;
        damage = damage * cardfix / 100;
    }

    if (damage < 0)
        damage = 0;

    // 属 性の適用
    if (!((battle_config.mob_ghostring_fix == 1) && (battle_get_element (target) == 8) && (target->type == BL_PC))) // [MouseJstr]
        if (skill_num != 0 || s_ele != 0
            || !battle_config.mob_attack_attr_none)
            damage = battle_attr_fix (damage, s_ele, battle_get_element (target));

    // 完全回避の判定
    if (skill_num == 0 && skill_lv >= 0 && tsd != NULL
        && MRAND (1000) < battle_get_flee2 (target))
    {
        damage = 0;
        type = 0x0b;
        dmg_lv = ATK_LUCKY;
    }

    if (battle_config.enemy_perfect_flee)
    {
        if (skill_num == 0 && skill_lv >= 0 && tmd != NULL
            && MRAND (1000) < battle_get_flee2 (target))
        {
            damage = 0;
            type = 0x0b;
            dmg_lv = ATK_LUCKY;
        }
    }

//  if(def1 >= 1000000 && damage > 0)
    if (t_mode & 0x40 && damage > 0)
        damage = 1;

    if (tsd && tsd->special_state.no_weapon_damage)
        damage = 0;

    damage = battle_calc_damage (src, target, damage, div_, skill_num, skill_lv, flag);

    wd.damage = damage;
    wd.damage2 = 0;
    wd.type = type;
    wd.div_ = div_;
    wd.amotion = battle_get_amotion (src);
    wd.dmotion = battle_get_dmotion (target);
    wd.blewcount = blewcount;
    wd.flag = flag;
    wd.dmg_lv = dmg_lv;
    return wd;
}

int battle_is_unarmed (struct block_list *bl)
{
    if (!bl)
        return 0;
    if (bl->type == BL_PC)
    {
        struct map_session_data *sd = (struct map_session_data *) bl;

        return (sd->equip_index[EQUIP_SHIELD] == -1
                && sd->equip_index[EQUIP_WEAPON] == -1);
    }
    else
        return 0;
}

/*
 * =========================================================================
 * PCの武器による攻撃
 *-------------------------------------------------------------------------
 */
static struct Damage battle_calc_pc_weapon_attack (struct block_list *src,
                                                   struct block_list *target,
                                                   int skill_num,
                                                   int skill_lv, int)
{
    struct map_session_data *sd = (struct map_session_data *) src, *tsd =
        NULL;
    struct mob_data *tmd = NULL;
    int  hitrate, flee, cri = 0, atkmin, atkmax;
    int  dex, target_count = 1;
    int  def1 = battle_get_def (target);
    int  def2 = battle_get_def2 (target);
    int  t_vit = battle_get_vit (target);
    struct Damage wd;
    int  damage, damage2, type, div_, blewcount =
        skill_get_blewcount (skill_num, skill_lv);
    int  flag, dmg_lv = 0;
    int  t_mode = 0, t_race = 0, t_size = 1, s_race = 7, s_ele = 0;
    struct status_change *t_sc_data;
    int  atkmax_ = 0, atkmin_ = 0, s_ele_;  //二刀流用
    int  watk, watk_, cardfix, t_ele;
    int  da = 0, t_class, ac_flag = 0;
    int  idef_flag = 0, idef_flag_ = 0;
    int  target_distance;

    //return前の処理があるので情報出力部のみ変更
    if (src == NULL || target == NULL || sd == NULL)
    {
        nullpo_info (NLP_MARK);
        memset (&wd, 0, sizeof (wd));
        return wd;
    }

    // アタッカー
    s_race = battle_get_race (src); //種族
    s_ele = battle_get_attack_element (src);    //属性
    s_ele_ = battle_get_attack_element2 (src);  //左手属性

    sd->state.attack_type = BF_WEAPON;  //攻撃タイプは武器攻撃

    // ターゲット
    if (target->type == BL_PC)  //対象がPCなら
        tsd = (struct map_session_data *) target;   //tsdに代入(tmdはNULL)
    else if (target->type == BL_MOB)    //対象がMobなら
        tmd = (struct mob_data *) target;   //tmdに代入(tsdはNULL)
    t_race = battle_get_race (target);  //対象の種族
    t_ele = battle_get_elem_type (target);  //対象の属性
    t_size = battle_get_size (target);  //対象のサイズ
    t_mode = battle_get_mode (target);  //対象のMode
    t_sc_data = battle_get_sc_data (target);    //対象のステータス異常

//オートカウンター処理ここまで

    flag = BF_SHORT | BF_WEAPON | BF_NORMAL;    // 攻撃の種類の設定

    // 回避率計算、回避判定は後で
    flee = battle_get_flee (target);
    if (battle_config.agi_penaly_type > 0 || battle_config.vit_penaly_type > 0) //AGI、VITペナルティ設定が有効
        target_count += battle_counttargeted (target, src, battle_config.agi_penaly_count_lv);  //対象の数を算出
    if (battle_config.agi_penaly_type > 0)
    {
        if (target_count >= battle_config.agi_penaly_count)
        {                       //ペナルティ設定より対象が多い
            if (battle_config.agi_penaly_type == 1) //回避率がagi_penaly_num%ずつ減少
                flee =
                    (flee *
                     (100 -
                      (target_count -
                       (battle_config.agi_penaly_count -
                        1)) * battle_config.agi_penaly_num)) / 100;
            else if (battle_config.agi_penaly_type == 2)    //回避率がagi_penaly_num分減少
                flee -=
                    (target_count -
                     (battle_config.agi_penaly_count -
                      1)) * battle_config.agi_penaly_num;
            if (flee < 1)
                flee = 1;       //回避率は最低でも1
        }
    }
    hitrate = battle_get_hit (src) - flee + 80; //命中率計算

    {                           // [fate] Reduce hit chance by distance
        int  dx = abs (src->x - target->x);
        int  dy = abs (src->y - target->y);
        int  malus_dist;

        target_distance = MAX (dx, dy);
        malus_dist =
            MAX (0, target_distance - (skill_power (sd, AC_OWL) / 75));
        hitrate -= (malus_dist * (malus_dist + 1));
    }

    dex = battle_get_dex (src); //DEX
//    luk = battle_get_luk (src); //LUK
    watk = battle_get_atk (src);    //ATK
    watk_ = battle_get_atk_ (src);  //ATK左手

    type = 0;                   // normal
    div_ = 1;                   // single attack

    damage = damage2 = battle_get_baseatk (&sd->bl);    //damega,damega2初登場、base_atkの取得
    if (sd->attackrange > 2)
    {                           // [fate] ranged weapon?
        const int range_damage_bonus = 80;  // up to 31.25% bonus for long-range hit
        damage =
            damage * (256 +
                      ((range_damage_bonus * target_distance) /
                       sd->attackrange)) >> 8;
        damage2 =
            damage2 * (256 +
                       ((range_damage_bonus * target_distance) /
                        sd->attackrange)) >> 8;
    }

    atkmin = atkmin_ = dex;     //最低ATKはDEXで初期化？
    sd->state.arrow_atk = 0;    //arrow_atk初期化
    if (sd->equip_index[9] >= 0 && sd->inventory_data[sd->equip_index[9]])
        atkmin =
            atkmin * (80 +
                      sd->inventory_data[sd->equip_index[9]]->wlv * 20) / 100;
    if (sd->equip_index[8] >= 0 && sd->inventory_data[sd->equip_index[8]])
        atkmin_ =
            atkmin_ * (80 +
                       sd->inventory_data[sd->equip_index[8]]->wlv * 20) /
            100;
    if (sd->status.weapon == 11)
    {                           //武器が弓矢の場合
        atkmin = watk * ((atkmin < watk) ? atkmin : watk) / 100;    //弓用最低ATK計算
        flag = (flag & ~BF_RANGEMASK) | BF_LONG;    //遠距離攻撃フラグを有効
        if (sd->arrow_ele > 0)  //属性矢なら属性を矢の属性に変更
            s_ele = sd->arrow_ele;
        sd->state.arrow_atk = 1;    //arrow_atk有効化
    }

    // サイズ修正
    // ペコ騎乗していて、槍で攻撃した場合は中型のサイズ修正を100にする
    // ウェポンパーフェクション,ドレイクC
    if (sd->special_state.no_sizefix)
    {                           //ペコ騎乗していて、槍で中型を攻撃
        atkmax = watk;
        atkmax_ = watk_;
    }
    else
    {
        atkmax = (watk * sd->atkmods[t_size]) / 100;
        atkmin = (atkmin * sd->atkmods[t_size]) / 100;
        atkmax_ = (watk_ * sd->atkmods_[t_size]) / 100;
        atkmin_ = (atkmin_ * sd->atkmods[t_size]) / 100;
    }

    if (atkmin > atkmax && !(sd->state.arrow_atk))
        atkmin = atkmax;        //弓は最低が上回る場合あり
    if (atkmin_ > atkmax_)
        atkmin_ = atkmax_;

    if (sd->double_rate > 0 && da == 0 && skill_num == 0 && skill_lv >= 0)
        da = (MRAND (100) < sd->double_rate) ? 1 : 0;

    // 過剰精錬ボーナス
    if (sd->overrefine > 0)
        damage += MPRAND (1, sd->overrefine);
    if (sd->overrefine_ > 0)
        damage2 += MPRAND (1, sd->overrefine_);

    if (da == 0)
    {                           //ダブルアタックが発動していない
        // クリティカル計算
        cri = battle_get_critical (src);

        if (sd->state.arrow_atk)
            cri += sd->arrow_cri;
        if (sd->status.weapon == 16)
            // カタールの場合、クリティカルを倍に
            cri <<= 1;
        cri -= battle_get_luk (target) * 3;
        if (ac_flag)
            cri = 1000;

    }

    if (tsd && tsd->critical_def)
        cri = cri * (100 - tsd->critical_def) / 100;

    if (da == 0 && skill_num == 0 && skill_lv >= 0 && (MRAND (1000)) < cri)
    {
        damage += atkmax;
        damage2 += atkmax_;
        if (sd->atk_rate != 100)
        {
            damage = (damage * sd->atk_rate) / 100;
            damage2 = (damage2 * sd->atk_rate) / 100;
        }
        if (sd->state.arrow_atk)
            damage += sd->arrow_atk;
        type = 0x0a;

/*		if(def1 < 1000000) {
			if(sd->def_ratio_atk_ele & (1<<t_ele) || sd->def_ratio_atk_race & (1<<t_race)) {
				damage = (damage * (def1 + def2))/100;
				idef_flag = 1;
			}
			if(sd->def_ratio_atk_ele_ & (1<<t_ele) || sd->def_ratio_atk_race_ & (1<<t_race)) {
				damage2 = (damage2 * (def1 + def2))/100;
				idef_flag_ = 1;
			}
			if(t_mode & 0x20) {
				if(!idef_flag && sd->def_ratio_atk_race & (1<<10)) {
					damage = (damage * (def1 + def2))/100;
					idef_flag = 1;
				}
				if(!idef_flag_ && sd->def_ratio_atk_race_ & (1<<10)) {
					damage2 = (damage2 * (def1 + def2))/100;
					idef_flag_ = 1;
				}
			}
			else {
				if(!idef_flag && sd->def_ratio_atk_race & (1<<11)) {
					damage = (damage * (def1 + def2))/100;
					idef_flag = 1;
				}
				if(!idef_flag_ && sd->def_ratio_atk_race_ & (1<<11)) {
					damage2 = (damage2 * (def1 + def2))/100;
					idef_flag_ = 1;
				}
			}
		}*/
    }
    else
    {
        int  vitbonusmax;

        if (atkmax > atkmin)
            damage += atkmin + MRAND ((atkmax - atkmin + 1));
        else
            damage += atkmin;
        if (atkmax_ > atkmin_)
            damage2 += atkmin_ + MRAND ((atkmax_ - atkmin_ + 1));
        else
            damage2 += atkmin_;
        if (sd->atk_rate != 100)
        {
            damage = (damage * sd->atk_rate) / 100;
            damage2 = (damage2 * sd->atk_rate) / 100;
        }

        if (sd->state.arrow_atk)
        {
            if (sd->arrow_atk > 0)
                damage += MRAND ((sd->arrow_atk + 1));
            hitrate += sd->arrow_hit;
        }

        if (def1 < 1000000)
        {
            if (sd->def_ratio_atk_ele & (1 << t_ele)
                || sd->def_ratio_atk_race & (1 << t_race))
            {
                damage = (damage * (def1 + def2)) / 100;
                idef_flag = 1;
            }
            if (sd->def_ratio_atk_ele_ & (1 << t_ele)
                || sd->def_ratio_atk_race_ & (1 << t_race))
            {
                damage2 = (damage2 * (def1 + def2)) / 100;
                idef_flag_ = 1;
            }
            if (t_mode & 0x20)
            {
                if (!idef_flag && sd->def_ratio_atk_race & (1 << 10))
                {
                    damage = (damage * (def1 + def2)) / 100;
                    idef_flag = 1;
                }
                if (!idef_flag_ && sd->def_ratio_atk_race_ & (1 << 10))
                {
                    damage2 = (damage2 * (def1 + def2)) / 100;
                    idef_flag_ = 1;
                }
            }
            else
            {
                if (!idef_flag && sd->def_ratio_atk_race & (1 << 11))
                {
                    damage = (damage * (def1 + def2)) / 100;
                    idef_flag = 1;
                }
                if (!idef_flag_ && sd->def_ratio_atk_race_ & (1 << 11))
                {
                    damage2 = (damage2 * (def1 + def2)) / 100;
                    idef_flag_ = 1;
                }
            }
        }

        if (skill_num > 0)
        {
            int  i;
            if ((i = skill_get_pl (skill_num)) > 0)
                s_ele = s_ele_ = i;

            flag = (flag & ~BF_SKILLMASK) | BF_SKILL;
        }
        if (da == 2)
        {                       //三段掌が発動しているか
            type = 0x08;
            div_ = 255;         //三段掌用に…
        }

        {
            if (def1 < 1000000)
            {                   //DEF, VIT無視
                int  t_def;
                target_count =
                    1 + battle_counttargeted (target, src,
                                              battle_config.vit_penaly_count_lv);
                if (battle_config.vit_penaly_type > 0)
                {
                    if (target_count >= battle_config.vit_penaly_count)
                    {
                        if (battle_config.vit_penaly_type == 1)
                        {
                            def1 =
                                (def1 *
                                 (100 -
                                  (target_count -
                                   (battle_config.vit_penaly_count -
                                    1)) * battle_config.vit_penaly_num)) /
                                100;
                            def2 =
                                (def2 *
                                 (100 -
                                  (target_count -
                                   (battle_config.vit_penaly_count -
                                    1)) * battle_config.vit_penaly_num)) /
                                100;
                            t_vit =
                                (t_vit *
                                 (100 -
                                  (target_count -
                                   (battle_config.vit_penaly_count -
                                    1)) * battle_config.vit_penaly_num)) /
                                100;
                        }
                        else if (battle_config.vit_penaly_type == 2)
                        {
                            def1 -=
                                (target_count -
                                 (battle_config.vit_penaly_count -
                                  1)) * battle_config.vit_penaly_num;
                            def2 -=
                                (target_count -
                                 (battle_config.vit_penaly_count -
                                  1)) * battle_config.vit_penaly_num;
                            t_vit -=
                                (target_count -
                                 (battle_config.vit_penaly_count -
                                  1)) * battle_config.vit_penaly_num;
                        }
                        if (def1 < 0)
                            def1 = 0;
                        if (def2 < 1)
                            def2 = 1;
                        if (t_vit < 1)
                            t_vit = 1;
                    }
                }
                t_def = def2 * 8 / 10;
                vitbonusmax = (t_vit / 20) * (t_vit / 20) - 1;
                if (sd->ignore_def_ele & (1 << t_ele)
                    || sd->ignore_def_race & (1 << t_race))
                    idef_flag = 1;
                if (sd->ignore_def_ele_ & (1 << t_ele)
                    || sd->ignore_def_race_ & (1 << t_race))
                    idef_flag_ = 1;
                if (t_mode & 0x20)
                {
                    if (sd->ignore_def_race & (1 << 10))
                        idef_flag = 1;
                    if (sd->ignore_def_race_ & (1 << 10))
                        idef_flag_ = 1;
                }
                else
                {
                    if (sd->ignore_def_race & (1 << 11))
                        idef_flag = 1;
                    if (sd->ignore_def_race_ & (1 << 11))
                        idef_flag_ = 1;
                }

                if (!idef_flag)
                {
                    if (battle_config.player_defense_type)
                    {
                        damage =
                            damage -
                            (def1 * battle_config.player_defense_type) -
                            t_def -
                            ((vitbonusmax <
                              1) ? 0 : MRAND ((vitbonusmax + 1)));
                    }
                    else
                    {
                        damage =
                            damage * (100 - def1) / 100 - t_def -
                            ((vitbonusmax <
                              1) ? 0 : MRAND ((vitbonusmax + 1)));
                    }
                }
                if (!idef_flag_)
                {
                    if (battle_config.player_defense_type)
                    {
                        damage2 =
                            damage2 -
                            (def1 * battle_config.player_defense_type) -
                            t_def -
                            ((vitbonusmax <
                              1) ? 0 : MRAND ((vitbonusmax + 1)));
                    }
                    else
                    {
                        damage2 =
                            damage2 * (100 - def1) / 100 - t_def -
                            ((vitbonusmax <
                              1) ? 0 : MRAND ((vitbonusmax + 1)));
                    }
                }
            }
        }
    }
    damage += battle_get_atk2 (src);
    damage2 += battle_get_atk_2 (src);

    if (damage < 1)
        damage = 1;
    if (damage2 < 1)
        damage2 = 1;

    damage = battle_addmastery (sd, target, damage, 0);
    damage2 = battle_addmastery (sd, target, damage2, 1);

    if (sd->perfect_hit > 0)
    {
        if (MRAND (100) < sd->perfect_hit)
            hitrate = 1000000;
    }

    // 回避修正
    hitrate = (hitrate < 5) ? 5 : hitrate;
    if (type == 0 && MRAND (100) >= hitrate)
    {
        damage = damage2 = 0;
        dmg_lv = ATK_FLEE;
    }
    else
    {
        dmg_lv = ATK_DEF;
    }

//スキルによるダメージ補正ここまで

//カードによるダメージ追加処理ここから
    cardfix = 100;
    if (!sd->state.arrow_atk)
    {                           //弓矢以外
        if (!battle_config.left_cardfix_to_right)
        {                       //左手カード補正設定無し
            cardfix = cardfix * (100 + sd->addrace[t_race]) / 100;  // 種族によるダメージ修正
            cardfix = cardfix * (100 + sd->addele[t_ele]) / 100;    // 属性によるダメージ修正
            cardfix = cardfix * (100 + sd->addsize[t_size]) / 100;  // サイズによるダメージ修正
        }
        else
        {
            cardfix = cardfix * (100 + sd->addrace[t_race] + sd->addrace_[t_race]) / 100;   // 種族によるダメージ修正(左手による追加あり)
            cardfix = cardfix * (100 + sd->addele[t_ele] + sd->addele_[t_ele]) / 100;   // 属性によるダメージ修正(左手による追加あり)
            cardfix = cardfix * (100 + sd->addsize[t_size] + sd->addsize_[t_size]) / 100;   // サイズによるダメージ修正(左手による追加あり)
        }
    }
    else
    {                           //弓矢
        cardfix = cardfix * (100 + sd->addrace[t_race] + sd->arrow_addrace[t_race]) / 100;  // 種族によるダメージ修正(弓矢による追加あり)
        cardfix = cardfix * (100 + sd->addele[t_ele] + sd->arrow_addele[t_ele]) / 100;  // 属性によるダメージ修正(弓矢による追加あり)
        cardfix = cardfix * (100 + sd->addsize[t_size] + sd->arrow_addsize[t_size]) / 100;  // サイズによるダメージ修正(弓矢による追加あり)
    }
    if (t_mode & 0x20)
    {                           //ボス
        if (!sd->state.arrow_atk)
        {                       //弓矢攻撃以外なら
            if (!battle_config.left_cardfix_to_right)   //左手カード補正設定無し
                cardfix = cardfix * (100 + sd->addrace[10]) / 100;  //ボスモンスターに追加ダメージ
            else                //左手カード補正設定あり
                cardfix = cardfix * (100 + sd->addrace[10] + sd->addrace_[10]) / 100;   //ボスモンスターに追加ダメージ(左手による追加あり)
        }
        else                    //弓矢攻撃
            cardfix = cardfix * (100 + sd->addrace[10] + sd->arrow_addrace[10]) / 100;  //ボスモンスターに追加ダメージ(弓矢による追加あり)
    }
    else
    {                           //ボスじゃない
        if (!sd->state.arrow_atk)
        {                       //弓矢攻撃以外
            if (!battle_config.left_cardfix_to_right)   //左手カード補正設定無し
                cardfix = cardfix * (100 + sd->addrace[11]) / 100;  //ボス以外モンスターに追加ダメージ
            else                //左手カード補正設定あり
                cardfix = cardfix * (100 + sd->addrace[11] + sd->addrace_[11]) / 100;   //ボス以外モンスターに追加ダメージ(左手による追加あり)
        }
        else
            cardfix = cardfix * (100 + sd->addrace[11] + sd->arrow_addrace[11]) / 100;  //ボス以外モンスターに追加ダメージ(弓矢による追加あり)
    }
    //特定Class用補正処理(少女の日記→ボンゴン用？)
    t_class = battle_get_class (target);
    for (int i = 0; i < sd->add_damage_class_count; i++)
    {
        if (sd->add_damage_classid[i] == t_class)
        {
            cardfix = cardfix * (100 + sd->add_damage_classrate[i]) / 100;
            break;
        }
    }
    damage = damage * cardfix / 100;    //カード補正によるダメージ増加
//カードによるダメージ増加処理ここまで

//カードによるダメージ追加処理(左手)ここから
    cardfix = 100;
    if (!battle_config.left_cardfix_to_right)
    {                           //左手カード補正設定無し
        cardfix = cardfix * (100 + sd->addrace_[t_race]) / 100; // 種族によるダメージ修正左手
        cardfix = cardfix * (100 + sd->addele_[t_ele]) / 100;   // 属 性によるダメージ修正左手
        cardfix = cardfix * (100 + sd->addsize_[t_size]) / 100; // サイズによるダメージ修正左手
        if (t_mode & 0x20)      //ボス
            cardfix = cardfix * (100 + sd->addrace_[10]) / 100; //ボスモンスターに追加ダメージ左手
        else
            cardfix = cardfix * (100 + sd->addrace_[11]) / 100; //ボス以外モンスターに追加ダメージ左手
    }
    //特定Class用補正処理左手(少女の日記→ボンゴン用？)
    for (int i = 0; i < sd->add_damage_class_count_; i++)
    {
        if (sd->add_damage_classid_[i] == t_class)
        {
            cardfix = cardfix * (100 + sd->add_damage_classrate_[i]) / 100;
            break;
        }
    }
    damage2 = damage2 * cardfix / 100;

//カードによるダメージ減衰処理ここから
    if (tsd)
    {                           //対象がPCの場合
        cardfix = 100;
        cardfix = cardfix * (100 - tsd->subrace[s_race]) / 100; // 種族によるダメージ耐性
        cardfix = cardfix * (100 - tsd->subele[s_ele]) / 100;   // 属性によるダメージ耐性
        if (battle_get_mode (src) & 0x20)
            cardfix = cardfix * (100 - tsd->subrace[10]) / 100; //ボスからの攻撃はダメージ減少
        else
            cardfix = cardfix * (100 - tsd->subrace[11]) / 100; //ボス以外からの攻撃はダメージ減少
        //特定Class用補正処理左手(少女の日記→ボンゴン用？)
        for (int i = 0; i < tsd->add_def_class_count; i++)
        {
            if (tsd->add_def_classid[i] == 0)
            {
                cardfix = cardfix * (100 - tsd->add_def_classrate[i]) / 100;
                break;
            }
        }
        if (flag & BF_LONG)
            cardfix = cardfix * (100 - tsd->long_attack_def_rate) / 100;    //遠距離攻撃はダメージ減少(ホルンCとか)
        if (flag & BF_SHORT)
            cardfix = cardfix * (100 - tsd->near_attack_def_rate) / 100;    //近距離攻撃はダメージ減少(該当無し？)
        damage = damage * cardfix / 100;    //カード補正によるダメージ減少
        damage2 = damage2 * cardfix / 100;  //カード補正による左手ダメージ減少
    }
//カードによるダメージ減衰処理ここまで

//対象にステータス異常がある場合のダメージ減算処理ここから
    if (t_sc_data)
    {
        cardfix = 100;
    }
//対象にステータス異常がある場合のダメージ減算処理ここまで

    if (damage < 0)
        damage = 0;
    if (damage2 < 0)
        damage2 = 0;

    // 属 性の適用
    damage = battle_attr_fix (damage, s_ele, battle_get_element (target));
    damage2 = battle_attr_fix (damage2, s_ele_, battle_get_element (target));

    // 星のかけら、気球の適用
    damage += sd->star;
    damage2 += sd->star_;

    //左手のみ武器装備
    if (sd->weapontype1 == 0 && sd->weapontype2 > 0)
    {
        damage = damage2;
        damage2 = 0;
    }
    // 右手、左手修練の適用
    if (sd->status.weapon > 16)
    {                           // 二刀流か?
        int  dmg = damage, dmg2 = damage2;
        damage = damage * 50 / 100;
        if (dmg > 0 && damage < 1)
            damage = 1;
        damage2 = damage2 * 30 / 100;
        if (dmg2 > 0 && damage2 < 1)
            damage2 = 1;
    }
    else                        //二刀流でなければ左手ダメージは0
        damage2 = 0;

    // 右手,短剣のみ
    if (da == 1)
    {                           //ダブルアタックが発動しているか
        div_ = 2;
        damage += damage;
        type = 0x08;
    }

    if (sd->status.weapon == 16)
    {
        // カタール追撃ダメージ
        damage2 = damage / 100;
        if (damage > 0 && damage2 < 1)
            damage2 = 1;
    }

    // 完全回避の判定
    if (skill_num == 0 && skill_lv >= 0 && tsd != NULL && div_ < 255
        && MRAND (1000) < battle_get_flee2 (target))
    {
        damage = damage2 = 0;
        type = 0x0b;
        dmg_lv = ATK_LUCKY;
    }

    // 対象が完全回避をする設定がONなら
    if (battle_config.enemy_perfect_flee)
    {
        if (skill_num == 0 && skill_lv >= 0 && tmd != NULL && div_ < 255
            && MRAND (1000) < battle_get_flee2 (target))
        {
            damage = damage2 = 0;
            type = 0x0b;
            dmg_lv = ATK_LUCKY;
        }
    }

    //MobのModeに頑強フラグが立っているときの処理
    if (t_mode & 0x40)
    {
        if (damage > 0)
            damage = 1;
        if (damage2 > 0)
            damage2 = 1;
    }

    //bNoWeaponDamage(設定アイテム無し？)でグランドクロスじゃない場合はダメージが0
    if (tsd && tsd->special_state.no_weapon_damage)
        damage = damage2 = 0;

    if (damage > 0 || damage2 > 0)
    {
        if (damage2 < 1)        // ダメージ最終修正
            damage =
                battle_calc_damage (src, target, damage, div_, skill_num,
                                    skill_lv, flag);
        else if (damage < 1)    // 右手がミス？
            damage2 =
                battle_calc_damage (src, target, damage2, div_, skill_num,
                                    skill_lv, flag);
        else
        {                       // 両 手/カタールの場合はちょっと計算ややこしい
            int  d1 = damage + damage2, d2 = damage2;
            damage =
                battle_calc_damage (src, target, damage + damage2, div_,
                                    skill_num, skill_lv, flag);
            damage2 = (d2 * 100 / d1) * damage / 100;
            if (damage > 1 && damage2 < 1)
                damage2 = 1;
            damage -= damage2;
        }
    }

    /*              For executioner card [Valaris]              */
    if (src->type == BL_PC && sd->random_attack_increase_add > 0
        && sd->random_attack_increase_per > 0 && skill_num == 0)
    {
        if (MRAND (100) < sd->random_attack_increase_per)
        {
            if (damage > 0)
                damage *= sd->random_attack_increase_add / 100;
            if (damage2 > 0)
                damage2 *= sd->random_attack_increase_add / 100;
        }
    }
    /*                  End addition                    */

    wd.damage = damage;
    wd.damage2 = damage2;
    wd.type = type;
    wd.div_ = div_;
    wd.amotion = battle_get_amotion (src);
    wd.dmotion = battle_get_dmotion (target);
    wd.blewcount = blewcount;
    wd.flag = flag;
    wd.dmg_lv = dmg_lv;

    return wd;
}

/*==========================================
 * 武器ダメージ計算
 *------------------------------------------
 */
struct Damage battle_calc_weapon_attack (struct block_list *src,
                                         struct block_list *target,
                                         int skill_num, int skill_lv,
                                         int wflag)
{
    struct Damage wd;

    //return前の処理があるので情報出力部のみ変更
    if (src == NULL || target == NULL)
    {
        nullpo_info (NLP_MARK);
        memset (&wd, 0, sizeof (wd));
        return wd;
    }

    else if (src->type == BL_PC)
        wd = battle_calc_pc_weapon_attack (src, target, skill_num, skill_lv, wflag);    // weapon breaking [Valaris]
    else if (src->type == BL_MOB)
        wd = battle_calc_mob_weapon_attack (src, target, skill_num, skill_lv,
                                            wflag);
    else
        memset (&wd, 0, sizeof (wd));

    if (battle_config.equipment_breaking && src->type == BL_PC
        && (wd.damage > 0 || wd.damage2 > 0))
    {
        struct map_session_data *sd = (struct map_session_data *) src;
        if (sd->status.weapon && sd->status.weapon != 11)
        {
            int  breakrate = 1;
            if (wd.type == 0x0a)
                breakrate *= 2;
            if (MRAND (10000) <
                breakrate * battle_config.equipment_break_rate / 100
                || breakrate >= 10000)
            {
                pc_breakweapon (sd);
                memset (&wd, 0, sizeof (wd));
            }
        }
    }

    if (battle_config.equipment_breaking && target->type == BL_PC
        && (wd.damage > 0 || wd.damage2 > 0))
    {
        int  breakrate = 1;
        if (wd.type == 0x0a)
            breakrate *= 2;
        if (MRAND (10000) <
            breakrate * battle_config.equipment_break_rate / 100
            || breakrate >= 10000)
        {
            pc_breakarmor ((struct map_session_data *) target);
        }
    }

    return wd;
}

/*==========================================
 * 魔法ダメージ計算
 *------------------------------------------
 */
struct Damage battle_calc_magic_attack (struct block_list *bl,
                                        struct block_list *target,
                                        int skill_num, int skill_lv, int)
{
    int  mdef1 = battle_get_mdef (target);
    int  mdef2 = battle_get_mdef2 (target);
    int  matk1, matk2, damage = 0, div_ = 1, blewcount =
        skill_get_blewcount (skill_num, skill_lv), rdamage = 0;
    struct Damage md;
    int  aflag;
    int  normalmagic_flag = 1;
    int  ele = 0, race = 7, t_ele = 0, t_race = 7, t_mode =
        0, cardfix, t_class, i;
    struct map_session_data *sd = NULL, *tsd = NULL;

    //return前の処理があるので情報出力部のみ変更
    if (bl == NULL || target == NULL)
    {
        nullpo_info (NLP_MARK);
        memset (&md, 0, sizeof (md));
        return md;
    }

    matk1 = battle_get_matk1 (bl);
    matk2 = battle_get_matk2 (bl);
    ele = skill_get_pl (skill_num);
    race = battle_get_race (bl);
    t_ele = battle_get_elem_type (target);
    t_race = battle_get_race (target);
    t_mode = battle_get_mode (target);

#define MATK_FIX( a,b ) { matk1=matk1*(a)/(b); matk2=matk2*(a)/(b); }

    if (bl->type == BL_PC && (sd = (struct map_session_data *) bl))
    {
        sd->state.attack_type = BF_MAGIC;
        if (sd->matk_rate != 100)
            MATK_FIX (sd->matk_rate, 100);
        sd->state.arrow_atk = 0;
    }
    if (target->type == BL_PC)
        tsd = (struct map_session_data *) target;

    aflag = BF_MAGIC | BF_LONG | BF_SKILL;

    if (normalmagic_flag)
    {                           // 一般魔法ダメージ計算
        int  imdef_flag = 0;
        if (matk1 > matk2)
            damage = matk2 + MRAND ((matk1 - matk2 + 1));
        else
            damage = matk2;
        if (sd)
        {
            if (sd->ignore_mdef_ele & (1 << t_ele)
                || sd->ignore_mdef_race & (1 << t_race))
                imdef_flag = 1;
            if (t_mode & 0x20)
            {
                if (sd->ignore_mdef_race & (1 << 10))
                    imdef_flag = 1;
            }
            else
            {
                if (sd->ignore_mdef_race & (1 << 11))
                    imdef_flag = 1;
            }
        }
        if (!imdef_flag)
        {
            if (battle_config.magic_defense_type)
            {
                damage =
                    damage - (mdef1 * battle_config.magic_defense_type) -
                    mdef2;
            }
            else
            {
                damage = (damage * (100 - mdef1)) / 100 - mdef2;
            }
        }

        if (damage < 1)
            damage = 1;
    }

    if (sd)
    {
        cardfix = 100;
        cardfix = cardfix * (100 + sd->magic_addrace[t_race]) / 100;
        cardfix = cardfix * (100 + sd->magic_addele[t_ele]) / 100;
        if (t_mode & 0x20)
            cardfix = cardfix * (100 + sd->magic_addrace[10]) / 100;
        else
            cardfix = cardfix * (100 + sd->magic_addrace[11]) / 100;
        t_class = battle_get_class (target);
        for (i = 0; i < sd->add_magic_damage_class_count; i++)
        {
            if (sd->add_magic_damage_classid[i] == t_class)
            {
                cardfix =
                    cardfix * (100 + sd->add_magic_damage_classrate[i]) / 100;
                break;
            }
        }
        damage = damage * cardfix / 100;
    }

    if (tsd)
    {
        int  s_class = battle_get_class (bl);
        cardfix = 100;
        cardfix = cardfix * (100 - tsd->subele[ele]) / 100; // 属 性によるダメージ耐性
        cardfix = cardfix * (100 - tsd->subrace[race]) / 100;   // 種族によるダメージ耐性
        cardfix = cardfix * (100 - tsd->magic_subrace[race]) / 100;
        if (battle_get_mode (bl) & 0x20)
            cardfix = cardfix * (100 - tsd->magic_subrace[10]) / 100;
        else
            cardfix = cardfix * (100 - tsd->magic_subrace[11]) / 100;
        for (i = 0; i < tsd->add_mdef_class_count; i++)
        {
            if (tsd->add_mdef_classid[i] == s_class)
            {
                cardfix = cardfix * (100 - tsd->add_mdef_classrate[i]) / 100;
                break;
            }
        }
        cardfix = cardfix * (100 - tsd->magic_def_rate) / 100;
        damage = damage * cardfix / 100;
    }
    if (damage < 0)
        damage = 0;

    damage = battle_attr_fix (damage, ele, battle_get_element (target));    // 属 性修正

    div_ = skill_get_num (skill_num, skill_lv);

    if (div_ > 1)
        damage *= div_;

//  if(mdef1 >= 1000000 && damage > 0)
    if (t_mode & 0x40 && damage > 0)
        damage = 1;

    if (tsd && tsd->special_state.no_magic_damage)
    {
        if (battle_config.gtb_pvp_only != 0)
        {                       // [MouseJstr]
            if (maps[target->m].flag.pvp && target->type == BL_PC)
                damage = (damage * (100 - battle_config.gtb_pvp_only)) / 100;
        }
        else
            damage = 0;         // 黄 金蟲カード（魔法ダメージ０）
    }

    damage = battle_calc_damage (bl, target, damage, div_, skill_num, skill_lv, aflag); // 最終修正

    /* magic_damage_return by [AppleGirl] and [Valaris]     */
    if (target->type == BL_PC && tsd && tsd->magic_damage_return > 0)
    {
        rdamage += damage * tsd->magic_damage_return / 100;
        if (rdamage < 1)
            rdamage = 1;
        clif_damage (target, bl, gettick (), 0, 0, rdamage, 0, 0, 0);
        battle_damage (target, bl, rdamage, 0);
    }
    /*          end magic_damage_return         */

    md.damage = damage;
    md.div_ = div_;
    md.amotion = battle_get_amotion (bl);
    md.dmotion = battle_get_dmotion (target);
    md.damage2 = 0;
    md.type = 0;
    md.blewcount = blewcount;
    md.flag = aflag;

    return md;
}

/*==========================================
 * その他ダメージ計算
 *------------------------------------------
 */
struct Damage battle_calc_misc_attack (struct block_list *bl,
                                       struct block_list *target,
                                       int skill_num, int skill_lv, int)
{
    int ele, race, cardfix;
    struct map_session_data *sd = NULL, *tsd = NULL;
    int  damage = 0, div_ = 1, blewcount =
        skill_get_blewcount (skill_num, skill_lv);
    struct Damage md;
    int  damagefix = 1;

    int  aflag = BF_MISC | BF_LONG | BF_SKILL;

    //return前の処理があるので情報出力部のみ変更
    if (bl == NULL || target == NULL)
    {
        nullpo_info (NLP_MARK);
        memset (&md, 0, sizeof (md));
        return md;
    }

    if (bl->type == BL_PC && (sd = (struct map_session_data *) bl))
    {
        sd->state.attack_type = BF_MISC;
        sd->state.arrow_atk = 0;
    }

    if (target->type == BL_PC)
        tsd = (struct map_session_data *) target;

    ele = skill_get_pl (skill_num);
    race = battle_get_race (bl);

    if (damagefix)
    {
        if (damage < 1)
            damage = 1;

        if (tsd)
        {
            cardfix = 100;
            cardfix = cardfix * (100 - tsd->subele[ele]) / 100; // 属性によるダメージ耐性
            cardfix = cardfix * (100 - tsd->subrace[race]) / 100;   // 種族によるダメージ耐性
            cardfix = cardfix * (100 - tsd->misc_def_rate) / 100;
            damage = damage * cardfix / 100;
        }
        if (damage < 0)
            damage = 0;
        damage = battle_attr_fix (damage, ele, battle_get_element (target));    // 属性修正
    }

    div_ = skill_get_num (skill_num, skill_lv);
    if (div_ > 1)
        damage *= div_;

    if (damage > 0
        && (damage < div_
            || (battle_get_def (target) >= 1000000
                && battle_get_mdef (target) >= 1000000)))
    {
        damage = div_;
    }

    damage = battle_calc_damage (bl, target, damage, div_, skill_num, skill_lv, aflag); // 最終修正

    md.damage = damage;
    md.div_ = div_;
    md.amotion = battle_get_amotion (bl);
    md.dmotion = battle_get_dmotion (target);
    md.damage2 = 0;
    md.type = 0;
    md.blewcount = blewcount;
    md.flag = aflag;
    return md;

}

/*==========================================
 * ダメージ計算一括処理用
 *------------------------------------------
 */
struct Damage battle_calc_attack (int attack_type,
                                  struct block_list *bl,
                                  struct block_list *target, int skill_num,
                                  int skill_lv, int flag)
{
    struct Damage d;
    memset (&d, 0, sizeof (d));

    switch (attack_type)
    {
        case BF_WEAPON:
            return battle_calc_weapon_attack (bl, target, skill_num, skill_lv,
                                              flag);
        case BF_MAGIC:
            return battle_calc_magic_attack (bl, target, skill_num, skill_lv,
                                             flag);
        case BF_MISC:
            return battle_calc_misc_attack (bl, target, skill_num, skill_lv,
                                            flag);
        default:
            if (battle_config.error_log)
                printf ("battle_calc_attack: unknwon attack type ! %d\n",
                        attack_type);
            break;
    }
    return d;
}

/*==========================================
 * 通常攻撃処理まとめ
 *------------------------------------------
 */
int battle_weapon_attack (struct block_list *src, struct block_list *target,
                          unsigned int tick, int flag)
{
    struct map_session_data *sd = NULL;
    struct status_change *t_sc_data = battle_get_sc_data (target);
    short *opt1;
    int  race = 7, ele = 0;
    int  damage, rdamage = 0;
    struct Damage wd;

    nullpo_retr (0, src);
    nullpo_retr (0, target);

    if (src->type == BL_PC)
        sd = (struct map_session_data *) src;

    if (src->prev == NULL || target->prev == NULL)
        return 0;
    if (src->type == BL_PC && pc_isdead (sd))
        return 0;
    if (target->type == BL_PC
        && pc_isdead ((struct map_session_data *) target))
        return 0;

    opt1 = battle_get_opt1 (src);
    if (opt1 && *opt1 > 0)
    {
        battle_stopattack (src);
        return 0;
    }

    race = battle_get_race (target);
    ele = battle_get_elem_type (target);
    if (battle_check_target (src, target, BCT_ENEMY) > 0 &&
        battle_check_range (src, target, 0))
    {
        // 攻撃対象となりうるので攻撃
        if (sd && sd->status.weapon == 11)
        {
            if (sd->equip_index[10] >= 0)
            {
                if (battle_config.arrow_decrement)
                    pc_delitem (sd, sd->equip_index[10], 1, 0);
            }
            else
            {
                clif_arrow_fail (sd, 0);
                return 0;
            }
        }
        if (flag & 0x8000)
        {
            if (sd && battle_config.pc_attack_direction_change)
                sd->dir = sd->head_dir =
                    map_calc_dir (src, target->x, target->y);
            else if (src->type == BL_MOB
                     && battle_config.monster_attack_direction_change)
                ((struct mob_data *) src)->dir =
                    map_calc_dir (src, target->x, target->y);
        }
        else
            wd = battle_calc_weapon_attack (src, target, 0, 0, 0);

        // significantly increase injuries for hasted characters
        if (wd.damage > 0 && (t_sc_data[SC_HASTE].timer != -1))
        {
            wd.damage = (wd.damage * (16 + t_sc_data[SC_HASTE].val1)) >> 4;
        }

        if (wd.damage > 0
            && t_sc_data[SC_PHYS_SHIELD].timer != -1 && target->type == BL_PC)
        {
            int  reduction = t_sc_data[SC_PHYS_SHIELD].val1;
            if (reduction > wd.damage)
                reduction = wd.damage;

            wd.damage -= reduction;
            MAP_LOG_PC (((struct map_session_data *) target),
                        "MAGIC-ABSORB-DMG %d", reduction);
        }

        if ((damage = wd.damage + wd.damage2) > 0 && src != target)
        {
            if (wd.flag & BF_SHORT)
            {
                if (target->type == BL_PC)
                {
                    struct map_session_data *tsd =
                        (struct map_session_data *) target;
                    if (tsd && tsd->short_weapon_damage_return > 0)
                    {
                        rdamage +=
                            damage * tsd->short_weapon_damage_return / 100;
                        if (rdamage < 1)
                            rdamage = 1;
                    }
                }
            }
            else if (wd.flag & BF_LONG)
            {
                if (target->type == BL_PC)
                {
                    struct map_session_data *tsd =
                        (struct map_session_data *) target;
                    if (tsd && tsd->long_weapon_damage_return > 0)
                    {
                        rdamage +=
                            damage * tsd->long_weapon_damage_return / 100;
                        if (rdamage < 1)
                            rdamage = 1;
                    }
                }
            }

            if (rdamage > 0)
                clif_damage (src, src, tick, wd.amotion, 0, rdamage, 1, 4, 0);
        }

        if (wd.div_ == 255 && sd)
        {                       //三段掌
            int  delay =
                1000 - 4 * battle_get_agi (src) - 2 * battle_get_dex (src);
            sd->attackabletime = sd->canmove_tick = tick + delay;
        }
        else
        {
            clif_damage (src, target, tick, wd.amotion, wd.dmotion,
                         wd.damage, wd.div_, wd.type, wd.damage2);
            //二刀流左手とカタール追撃のミス表示(無理やり〜)
            if (sd && sd->status.weapon >= 16 && wd.damage2 == 0)
                clif_damage (src, target, tick + 10, wd.amotion, wd.dmotion,
                             0, 1, 0, 0);
        }
        map_freeblock_lock ();

        if (src->type == BL_PC)
        {
            int  weapon_index = sd->equip_index[9];
            int  weapon = 0;
            if (sd->inventory_data[weapon_index]
                && sd->status.inventory[weapon_index].equip & 0x2)
                weapon = sd->inventory_data[weapon_index]->nameid;

            map_log ("PC%d %d:%d,%d WPNDMG %s%d %d FOR %d WPN %d",
                     sd->status.char_id, src->m, src->x, src->y,
                     (target->type == BL_PC) ? "PC" : "MOB",
                     (target->type ==
                      BL_PC) ? ((struct map_session_data *) target)->
                     status.char_id : target->id,
                     (target->type ==
                      BL_PC) ? 0 : ((struct mob_data *) target)->mob_class,
                     wd.damage + wd.damage2, weapon);
        }

        if (target->type == BL_PC)
        {
            struct map_session_data *sd2 = (struct map_session_data *) target;
            map_log ("PC%d %d:%d,%d WPNINJURY %s%d %d FOR %d",
                     sd2->status.char_id, target->m, target->x, target->y,
                     (src->type == BL_PC) ? "PC" : "MOB",
                     (src->type ==
                      BL_PC) ? ((struct map_session_data *) src)->
                     status.char_id : src->id,
                     (src->type ==
                      BL_PC) ? 0 : ((struct mob_data *) src)->mob_class,
                     wd.damage + wd.damage2);
        }

        battle_damage (src, target, (wd.damage + wd.damage2), 0);
        if (target->prev != NULL &&
            (target->type != BL_PC
             || (target->type == BL_PC
                 && !pc_isdead ((struct map_session_data *) target))))
        {
            if (wd.damage > 0 || wd.damage2 > 0)
            {
                skill_additional_effect (src, target, 0, 0, BF_WEAPON, tick);
                if (sd)
                {
                    if (sd->weapon_coma_ele[ele] > 0
                        && MRAND (10000) < sd->weapon_coma_ele[ele])
                        battle_damage (src, target,
                                       battle_get_max_hp (target), 1);
                    if (sd->weapon_coma_race[race] > 0
                        && MRAND (10000) < sd->weapon_coma_race[race])
                        battle_damage (src, target,
                                       battle_get_max_hp (target), 1);
                    if (battle_get_mode (target) & 0x20)
                    {
                        if (sd->weapon_coma_race[10] > 0
                            && MRAND (10000) < sd->weapon_coma_race[10])
                            battle_damage (src, target,
                                           battle_get_max_hp (target), 1);
                    }
                    else
                    {
                        if (sd->weapon_coma_race[11] > 0
                            && MRAND (10000) < sd->weapon_coma_race[11])
                            battle_damage (src, target,
                                           battle_get_max_hp (target), 1);
                    }
                }
            }
        }
        if (sd)
        {
            if (sd->autospell_id > 0 && sd->autospell_lv > 0
                && MRAND (100) < sd->autospell_rate)
            {
                int  skilllv = sd->autospell_lv, i, f = 0, sp;
                i = MRAND (100);
                if (i >= 50)
                    skilllv -= 2;
                else if (i >= 15)
                    skilllv--;
                if (skilllv < 1)
                    skilllv = 1;
                sp = skill_get_sp (sd->autospell_id, skilllv) * 2 / 3;
                if (sd->status.sp >= sp)
                {
                    if (!f)
                        pc_heal (sd, 0, -sp);
                }
            }
            if (wd.flag & BF_WEAPON && src != target
                && (wd.damage > 0 || wd.damage2 > 0))
            {
                int  hp = 0, sp = 0;
                if (sd->hp_drain_rate && sd->hp_drain_per > 0 && wd.damage > 0
                    && MRAND (100) < sd->hp_drain_rate)
                {
                    hp += (wd.damage * sd->hp_drain_per) / 100;
                    if (sd->hp_drain_rate > 0 && hp < 1)
                        hp = 1;
                    else if (sd->hp_drain_rate < 0 && hp > -1)
                        hp = -1;
                }
                if (sd->hp_drain_rate_ && sd->hp_drain_per_ > 0
                    && wd.damage2 > 0 && MRAND (100) < sd->hp_drain_rate_)
                {
                    hp += (wd.damage2 * sd->hp_drain_per_) / 100;
                    if (sd->hp_drain_rate_ > 0 && hp < 1)
                        hp = 1;
                    else if (sd->hp_drain_rate_ < 0 && hp > -1)
                        hp = -1;
                }
                if (sd->sp_drain_rate && sd->sp_drain_per > 0 && wd.damage > 0
                    && MRAND (100) < sd->sp_drain_rate)
                {
                    sp += (wd.damage * sd->sp_drain_per) / 100;
                    if (sd->sp_drain_rate > 0 && sp < 1)
                        sp = 1;
                    else if (sd->sp_drain_rate < 0 && sp > -1)
                        sp = -1;
                }
                if (sd->sp_drain_rate_ && sd->sp_drain_per_ > 0
                    && wd.damage2 > 0 && MRAND (100) < sd->sp_drain_rate_)
                {
                    sp += (wd.damage2 * sd->sp_drain_per_) / 100;
                    if (sd->sp_drain_rate_ > 0 && sp < 1)
                        sp = 1;
                    else if (sd->sp_drain_rate_ < 0 && sp > -1)
                        sp = -1;
                }
                if (hp || sp)
                    pc_heal (sd, hp, sp);
            }
        }

        if (rdamage > 0)
            battle_damage (target, src, rdamage, 0);

        map_freeblock_unlock ();
    }
    return wd.dmg_lv;
}

int battle_check_undead (int race, int element)
{
    if (battle_config.undead_detect_type == 0)
    {
        if (element == 9)
            return 1;
    }
    else if (battle_config.undead_detect_type == 1)
    {
        if (race == 1)
            return 1;
    }
    else
    {
        if (element == 9 || race == 1)
            return 1;
    }
    return 0;
}

/*==========================================
 * 敵味方判定(1=肯定,0=否定,-1=エラー)
 * flag&0xf0000 = 0x00000:敵じゃないか判定（ret:1＝敵ではない）
 *				= 0x10000:パーティー判定（ret:1=パーティーメンバ)
 *				= 0x20000:全て(ret:1=敵味方両方)
 *				= 0x40000:敵か判定(ret:1=敵)
 *				= 0x50000:パーティーじゃないか判定(ret:1=パーティでない)
 *------------------------------------------
 */
int battle_check_target (struct block_list *src, struct block_list *target,
                         int flag)
{
    int s_p, t_p;
    struct block_list *ss = src;

    nullpo_retr (0, src);
    nullpo_retr (0, target);

    if (flag & 0x40000)
    {                           // 反転フラグ
        int  ret = battle_check_target (src, target, flag & 0x30000);
        if (ret != -1)
            return !ret;
        return -1;
    }

    if (flag & 0x20000)
    {
        if (target->type == BL_MOB || target->type == BL_PC)
            return 1;
        else
            return -1;
    }

    if (src->type == BL_SKILL && target->type == BL_SKILL)  // 対象がスキルユニットなら無条件肯定
        return -1;

    if (target->type == BL_PC
        && ((struct map_session_data *) target)->invincible_timer != -1)
        return -1;

    if (target->type == BL_SKILL)
    {
        switch (((struct skill_unit *) target)->group->unit_id)
        {
            case 0x8d:
            case 0x8f:
            case 0x98:
                return 0;
                break;
        }
    }

    // スキルユニットの場合、親を求める
    if (src->type == BL_SKILL)
    {
        int  inf2 =
            skill_get_inf2 (((struct skill_unit *) src)->group->skill_id);
        if ((ss =
             map_id2bl (((struct skill_unit *) src)->group->src_id)) == NULL)
            return -1;
        if (ss->prev == NULL)
            return -1;
        if (inf2 & 0x80 && (maps[src->m].flag.pvp || pc_iskiller ((struct map_session_data *) src, (struct map_session_data *) target)) &&   // [MouseJstr]
            !(target->type == BL_PC
              && pc_isinvisible ((struct map_session_data *) target)))
            return 0;
        if (ss == target)
        {
            if (inf2 & 0x100)
                return 0;
            if (inf2 & 0x200)
                return -1;
        }
    }
    // Mobでmaster_idがあってspecial_mob_aiなら、召喚主を求める
    if (src->type == BL_MOB)
    {
        struct mob_data *md = (struct mob_data *) src;
        if (md && md->master_id > 0)
        {
            if (md->master_id == target->id)    // 主なら肯定
                return 1;
            if (md->state.special_mob_ai)
            {
                if (target->type == BL_MOB)
                {               //special_mob_aiで対象がMob
                    struct mob_data *tmd = (struct mob_data *) target;
                    if (tmd)
                    {
                        if (tmd->master_id != md->master_id)    //召喚主が一緒でなければ否定
                            return 0;
                        else
                        {       //召喚主が一緒なので肯定したいけど自爆は否定
                            if (md->state.special_mob_ai > 2)
                                return 0;
                            else
                                return 1;
                        }
                    }
                }
            }
            if ((ss = map_id2bl (md->master_id)) == NULL)
                return -1;
        }
    }

    if (src == target || ss == target)  // 同じなら肯定
        return 1;

    if (target->type == BL_PC
        && pc_isinvisible ((struct map_session_data *) target))
        return -1;

    if (src->prev == NULL ||    // 死んでるならエラー
        (src->type == BL_PC && pc_isdead ((struct map_session_data *) src)))
        return -1;

    if ((ss->type == BL_PC && target->type == BL_MOB) ||
        (ss->type == BL_MOB && target->type == BL_PC))
        return 0;               // PCvsMOBなら否定

    s_p = battle_get_party_id (ss);

    t_p = battle_get_party_id (target);

    if (flag & 0x10000)
    {
        if (s_p && t_p && s_p == t_p)   // 同じパーティなら肯定（味方）
            return 1;
        else                    // パーティ検索なら同じパーティじゃない時点で否定
            return 0;
    }

//printf("ss:%d src:%d target:%d flag:0x%x %d %d ",ss->id,src->id,target->id,flag,src->type,target->type);
//printf("p:%d %d g:%d %d\n",s_p,t_p,s_g,t_g);

    if (ss->type == BL_PC && target->type == BL_PC)
    {                           // 両方PVPモードなら否定（敵）
        struct skill_unit *su = NULL;
        if (src->type == BL_SKILL)
            su = (struct skill_unit *) src;
        if (maps[ss->m].flag.pvp
            || pc_iskiller ((struct map_session_data *) ss,
                            (struct map_session_data *) target))
        {                       // [MouseJstr]
            if (su && su->group->target_flag == BCT_NOENEMY)
                return 1;
            else if (battle_config.pk_mode)
                return 1;       // prevent novice engagement in pk_mode [Valaris]
            else if (maps[ss->m].flag.pvp_noparty && s_p > 0 && t_p > 0
                     && s_p == t_p)
                return 1;
            return 0;
        }
    }

    return 1;                   // 該当しないので無関係人物（まあ敵じゃないので味方）
}

/*==========================================
 * 射程判定
 *------------------------------------------
 */
int battle_check_range (struct block_list *src, struct block_list *bl,
                        int range)
{

    int  dx, dy;
    struct walkpath_data wpd;
    int  arange;

    nullpo_retr (0, src);
    nullpo_retr (0, bl);

    dx = abs (bl->x - src->x);
    dy = abs (bl->y - src->y);
    arange = ((dx > dy) ? dx : dy);

    if (src->m != bl->m)        // 違うマップ
        return 0;

    if (range > 0 && range < arange)    // 遠すぎる
        return 0;

    if (arange < 2)             // 同じマスか隣接
        return 1;

//  if(bl->type == BL_SKILL && ((struct skill_unit *)bl)->group->unit_id == 0x8d)
//      return 1;

    // 障害物判定
    wpd.path_len = 0;
    wpd.path_pos = 0;
    wpd.path_half = 0;
    if (path_search (&wpd, src->m, src->x, src->y, bl->x, bl->y, 0x10001) !=
        -1)
        return 1;

    dx = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
    dy = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
    return (path_search (&wpd, src->m, src->x + dx, src->y + dy,
                         bl->x - dx, bl->y - dy, 0x10001) != -1) ? 1 : 0;
}

/*==========================================
 * 設定ファイルを読み込む
 *------------------------------------------
 */
int battle_config_read (const char *cfgName)
{
    int  i;
    char line[1024], w1[1024], w2[1024];
    FILE *fp;
    static int count = 0;

    if ((count++) == 0)
    {
        battle_config.warp_point_debug = 0;
        battle_config.enemy_critical = 0;
        battle_config.enemy_critical_rate = 100;
        battle_config.enemy_str = 1;
        battle_config.enemy_perfect_flee = 0;
        battle_config.cast_rate = 100;
        battle_config.delay_rate = 100;
        battle_config.delay_dependon_dex = 0;
        battle_config.sdelay_attack_enable = 0;
        battle_config.left_cardfix_to_right = 0;
        battle_config.pc_skill_add_range = 0;
        battle_config.skill_out_range_consume = 1;
        battle_config.mob_skill_add_range = 0;
        battle_config.pc_damage_delay = 1;
        battle_config.pc_damage_delay_rate = 100;
        battle_config.defnotenemy = 1;
        battle_config.random_monster_checklv = 1;
        battle_config.attr_recover = 1;
        battle_config.flooritem_lifetime = LIFETIME_FLOORITEM * 1000;
        battle_config.item_auto_get = 0;
        battle_config.drop_pickup_safety_zone = 20;
        battle_config.item_first_get_time = 3000;
        battle_config.item_second_get_time = 1000;
        battle_config.item_third_get_time = 1000;

        battle_config.drop_rate0item = 0;
        battle_config.base_exp_rate = 100;
        battle_config.job_exp_rate = 100;
        battle_config.pvp_exp = 1;
        battle_config.gtb_pvp_only = 0;
        battle_config.death_penalty_type = 0;
        battle_config.death_penalty_base = 0;
        battle_config.death_penalty_job = 0;
        battle_config.zeny_penalty = 0;
        battle_config.restart_hp_rate = 0;
        battle_config.restart_sp_rate = 0;
        battle_config.mvp_item_rate = 100;
        battle_config.mvp_exp_rate = 100;
        battle_config.mvp_hp_rate = 100;
        battle_config.monster_hp_rate = 100;
        battle_config.monster_max_aspd = 199;
        battle_config.gm_allskill = 0;
        battle_config.gm_allequip = 0;
        battle_config.gm_skilluncond = 0;
        battle_config.skillfree = 0;
        battle_config.skillup_limit = 0;
        battle_config.wp_rate = 100;
        battle_config.pp_rate = 100;
        battle_config.monster_active_enable = 1;
        battle_config.monster_damage_delay_rate = 100;
        battle_config.monster_loot_type = 0;
        battle_config.mob_skill_use = 1;
        battle_config.mob_count_rate = 100;
        battle_config.quest_skill_learn = 0;
        battle_config.quest_skill_reset = 1;
        battle_config.basic_skill_check = 1;
        battle_config.pc_invincible_time = 5000;
        battle_config.skill_min_damage = 0;
        battle_config.finger_offensive_type = 0;
        battle_config.heal_exp = 0;
        battle_config.resurrection_exp = 0;
        battle_config.shop_exp = 0;
        battle_config.combo_delay_rate = 100;
        battle_config.item_check = 1;
        battle_config.wedding_modifydisplay = 0;
        battle_config.natural_healhp_interval = 6000;
        battle_config.natural_healsp_interval = 8000;
        battle_config.natural_heal_skill_interval = 10000;
        battle_config.natural_heal_weight_rate = 50;
        battle_config.itemheal_regeneration_factor = 1;
        battle_config.item_name_override_grffile = 1;
        battle_config.arrow_decrement = 1;
        battle_config.max_aspd = 199;
        battle_config.max_hp = 32500;
        battle_config.max_sp = 32500;
        battle_config.max_lv = 99;  // [MouseJstr]
        battle_config.max_parameter = 99;
        battle_config.pc_skill_log = 0;
        battle_config.mob_skill_log = 0;
        battle_config.battle_log = 0;
        battle_config.save_log = 0;
        battle_config.error_log = 1;
        battle_config.etc_log = 1;
        battle_config.save_clothcolor = 0;
        battle_config.undead_detect_type = 0;
        battle_config.pc_auto_counter_type = 1;
        battle_config.monster_auto_counter_type = 1;
        battle_config.agi_penaly_type = 0;
        battle_config.agi_penaly_count = 3;
        battle_config.agi_penaly_num = 0;
        battle_config.agi_penaly_count_lv = ATK_FLEE;
        battle_config.vit_penaly_type = 0;
        battle_config.vit_penaly_count = 3;
        battle_config.vit_penaly_num = 0;
        battle_config.vit_penaly_count_lv = ATK_DEF;
        battle_config.player_defense_type = 0;
        battle_config.monster_defense_type = 0;
        battle_config.magic_defense_type = 0;
        battle_config.pc_skill_reiteration = 0;
        battle_config.monster_skill_reiteration = 0;
        battle_config.pc_skill_nofootset = 0;
        battle_config.monster_skill_nofootset = 0;
        battle_config.pc_cloak_check_type = 0;
        battle_config.monster_cloak_check_type = 0;
        battle_config.mob_changetarget_byskill = 0;
        battle_config.pc_attack_direction_change = 1;
        battle_config.monster_attack_direction_change = 1;
        battle_config.pc_undead_nofreeze = 0;
        battle_config.pc_land_skill_limit = 1;
        battle_config.monster_land_skill_limit = 1;
        battle_config.party_skill_penaly = 1;
        battle_config.monster_class_change_full_recover = 0;
        battle_config.produce_item_name_input = 1;
        battle_config.produce_potion_name_input = 1;
        battle_config.making_arrow_name_input = 1;
        battle_config.holywater_name_input = 1;
        battle_config.display_delay_skill_fail = 1;
        battle_config.chat_warpportal = 0;
        battle_config.mob_warpportal = 0;
        battle_config.dead_branch_active = 0;
        battle_config.show_steal_in_same_party = 0;
        battle_config.enable_upper_class = 0;
        battle_config.pc_attack_attr_none = 0;
        battle_config.mob_attack_attr_none = 1;
        battle_config.mob_ghostring_fix = 0;
        battle_config.gx_allhit = 0;
        battle_config.gx_cardfix = 0;
        battle_config.gx_dupele = 1;
        battle_config.gx_disptype = 1;
        battle_config.player_skill_partner_check = 1;
        battle_config.hide_GM_session = 0;
        battle_config.unit_movement_type = 0;
        battle_config.invite_request_check = 1;
        battle_config.skill_removetrap_type = 0;
        battle_config.disp_experience = 0;
        battle_config.item_rate_common = 100;
        battle_config.item_rate_equip = 100;
        battle_config.item_rate_card = 100;
        battle_config.item_rate_heal = 100; // Added by Valaris
        battle_config.item_rate_use = 100;  // End
        battle_config.item_drop_common_min = 1; // Added by TyrNemesis^
        battle_config.item_drop_common_max = 10000;
        battle_config.item_drop_equip_min = 1;
        battle_config.item_drop_equip_max = 10000;
        battle_config.item_drop_card_min = 1;
        battle_config.item_drop_card_max = 10000;
        battle_config.item_drop_mvp_min = 1;
        battle_config.item_drop_mvp_max = 10000;    // End Addition
        battle_config.item_drop_heal_min = 1;   // Added by Valaris
        battle_config.item_drop_heal_max = 10000;
        battle_config.item_drop_use_min = 1;
        battle_config.item_drop_use_max = 10000;    // End
        battle_config.prevent_logout = 1;   // Added by RoVeRT
        battle_config.maximum_level = 255;  // Added by Valaris
        battle_config.drops_by_luk = 0; // [Valaris]
        battle_config.equipment_breaking = 0;   // [Valaris]
        battle_config.equipment_break_rate = 100;   // [Valaris]
        battle_config.pk_mode = 0;  // [Valaris]
        battle_config.multi_level_up = 0;   // [Valaris]
        battle_config.backstab_bow_penalty = 0; // Akaru
        battle_config.night_at_start = 0;   // added by [Yor]
        battle_config.day_duration = 2 * 60 * 60 * 1000;    // added by [Yor] (2 hours)
        battle_config.night_duration = 30 * 60 * 1000;  // added by [Yor] (30 minutes)
        battle_config.show_mob_hp = 0;  // [Valaris]
        battle_config.hack_info_GM_level = 60;  // added by [Yor] (default: 60, GM level)
        battle_config.any_warp_GM_min_level = 20;   // added by [Yor]
        battle_config.packet_ver_flag = 63; // added by [Yor]
        battle_config.min_hair_style = 0;
        battle_config.max_hair_style = 20;
        battle_config.min_hair_color = 0;
        battle_config.max_hair_color = 9;
        battle_config.min_cloth_color = 0;
        battle_config.max_cloth_color = 4;

        battle_config.castrate_dex_scale = 150;

        battle_config.area_size = 14;

        battle_config.chat_lame_penalty = 2;
        battle_config.chat_spam_threshold = 10;
        battle_config.chat_spam_flood = 10;
        battle_config.chat_spam_ban = 1;
        battle_config.chat_spam_warn = 8;
        battle_config.chat_maxline = 255;

        battle_config.packet_spam_threshold = 2;
        battle_config.packet_spam_flood = 30;
        battle_config.packet_spam_kick = 1;

        battle_config.mask_ip_gms = 1;
    }

    fp = fopen_ (cfgName, "r");
    if (fp == NULL)
    {
        printf ("file not found: %s\n", cfgName);
        return 1;
    }
    while (fgets (line, 1020, fp))
    {
        const struct
        {
            char str[128];
            int *val;
        } data[] =
        {
            {"warp_point_debug", &battle_config.warp_point_debug},
            {"enemy_critical", &battle_config.enemy_critical},
            {"enemy_critical_rate", &battle_config.enemy_critical_rate},
            {"enemy_str", &battle_config.enemy_str},
            {"enemy_perfect_flee", &battle_config.enemy_perfect_flee},
            {"casting_rate", &battle_config.cast_rate},
            {"delay_rate", &battle_config.delay_rate},
            {"delay_dependon_dex", &battle_config.delay_dependon_dex},
            {"skill_delay_attack_enable", &battle_config.sdelay_attack_enable},
            {"left_cardfix_to_right", &battle_config.left_cardfix_to_right},
            {"player_skill_add_range", &battle_config.pc_skill_add_range},
            {"skill_out_range_consume", &battle_config.skill_out_range_consume},
            {"monster_skill_add_range", &battle_config.mob_skill_add_range},
            {"player_damage_delay", &battle_config.pc_damage_delay},
            {"player_damage_delay_rate", &battle_config.pc_damage_delay_rate},
            {"defunit_not_enemy", &battle_config.defnotenemy},
            {"random_monster_checklv", &battle_config.random_monster_checklv},
            {"attribute_recover", &battle_config.attr_recover},
            {"flooritem_lifetime", &battle_config.flooritem_lifetime},
            {"item_auto_get", &battle_config.item_auto_get},
            {"drop_pickup_safety_zone", &battle_config.drop_pickup_safety_zone},
            {"item_first_get_time", &battle_config.item_first_get_time},
            {"item_second_get_time", &battle_config.item_second_get_time},
            {"item_third_get_time", &battle_config.item_third_get_time},
            {"item_rate", &battle_config.item_rate},
            {"drop_rate0item", &battle_config.drop_rate0item},
            {"base_exp_rate", &battle_config.base_exp_rate},
            {"job_exp_rate", &battle_config.job_exp_rate},
            {"pvp_exp", &battle_config.pvp_exp},
            {"gtb_pvp_only", &battle_config.gtb_pvp_only},
            {"death_penalty_type", &battle_config.death_penalty_type},
            {"death_penalty_base", &battle_config.death_penalty_base},
            {"death_penalty_job", &battle_config.death_penalty_job},
            {"zeny_penalty", &battle_config.zeny_penalty},
            {"restart_hp_rate", &battle_config.restart_hp_rate},
            {"restart_sp_rate", &battle_config.restart_sp_rate},
            {"mvp_hp_rate", &battle_config.mvp_hp_rate},
            {"mvp_item_rate", &battle_config.mvp_item_rate},
            {"mvp_exp_rate", &battle_config.mvp_exp_rate},
            {"monster_hp_rate", &battle_config.monster_hp_rate},
            {"monster_max_aspd", &battle_config.monster_max_aspd},
            {"atcommand_spawn_quantity_limit", &battle_config.atc_spawn_quantity_limit},
            {"gm_all_skill", &battle_config.gm_allskill},
            {"gm_all_skill_add_abra", &battle_config.gm_allskill_addabra},
            {"gm_all_equipment", &battle_config.gm_allequip},
            {"gm_skill_unconditional", &battle_config.gm_skilluncond},
            {"player_skillfree", &battle_config.skillfree},
            {"player_skillup_limit", &battle_config.skillup_limit},
            {"weapon_produce_rate", &battle_config.wp_rate},
            {"potion_produce_rate", &battle_config.pp_rate},
            {"monster_active_enable", &battle_config.monster_active_enable},
            {"monster_damage_delay_rate", &battle_config.monster_damage_delay_rate},
            {"monster_loot_type", &battle_config.monster_loot_type},
            {"mob_skill_use", &battle_config.mob_skill_use},
            {"mob_count_rate", &battle_config.mob_count_rate},
            {"quest_skill_learn", &battle_config.quest_skill_learn},
            {"quest_skill_reset", &battle_config.quest_skill_reset},
            {"basic_skill_check", &battle_config.basic_skill_check},
            {"player_invincible_time", &battle_config.pc_invincible_time},
            {"skill_min_damage", &battle_config.skill_min_damage},
            {"finger_offensive_type", &battle_config.finger_offensive_type},
            {"heal_exp", &battle_config.heal_exp},
            {"resurrection_exp", &battle_config.resurrection_exp},
            {"shop_exp", &battle_config.shop_exp},
            {"combo_delay_rate", &battle_config.combo_delay_rate},
            {"item_check", &battle_config.item_check},
            {"wedding_modifydisplay", &battle_config.wedding_modifydisplay},
            {"natural_healhp_interval", &battle_config.natural_healhp_interval},
            {"natural_healsp_interval", &battle_config.natural_healsp_interval},
            {"natural_heal_skill_interval", &battle_config.natural_heal_skill_interval},
            {"natural_heal_weight_rate", &battle_config.natural_heal_weight_rate},
            {"itemheal_regeneration_factor", &battle_config.itemheal_regeneration_factor},
            {"item_name_override_grffile", &battle_config.item_name_override_grffile},
            {"arrow_decrement", &battle_config.arrow_decrement},
            {"max_aspd", &battle_config.max_aspd},
            {"max_hp", &battle_config.max_hp},
            {"max_sp", &battle_config.max_sp},
            {"max_lv", &battle_config.max_lv},
            {"max_parameter", &battle_config.max_parameter},
            {"player_skill_log", &battle_config.pc_skill_log},
            {"monster_skill_log", &battle_config.mob_skill_log},
            {"battle_log", &battle_config.battle_log},
            {"save_log", &battle_config.save_log},
            {"error_log", &battle_config.error_log},
            {"etc_log", &battle_config.etc_log},
            {"save_clothcolor", &battle_config.save_clothcolor},
            {"undead_detect_type", &battle_config.undead_detect_type},
            {"player_auto_counter_type", &battle_config.pc_auto_counter_type},
            {"monster_auto_counter_type", &battle_config.monster_auto_counter_type},
            {"agi_penaly_type", &battle_config.agi_penaly_type},
            {"agi_penaly_count", &battle_config.agi_penaly_count},
            {"agi_penaly_num", &battle_config.agi_penaly_num},
            {"agi_penaly_count_lv", &battle_config.agi_penaly_count_lv},
            {"vit_penaly_type", &battle_config.vit_penaly_type},
            {"vit_penaly_count", &battle_config.vit_penaly_count},
            {"vit_penaly_num", &battle_config.vit_penaly_num},
            {"vit_penaly_count_lv", &battle_config.vit_penaly_count_lv},
            {"player_defense_type", &battle_config.player_defense_type},
            {"monster_defense_type", &battle_config.monster_defense_type},
            {"magic_defense_type", &battle_config.magic_defense_type},
            {"player_skill_reiteration", &battle_config.pc_skill_reiteration},
            {"monster_skill_reiteration", &battle_config.monster_skill_reiteration},
            {"player_skill_nofootset", &battle_config.pc_skill_nofootset},
            {"monster_skill_nofootset", &battle_config.monster_skill_nofootset},
            {"player_cloak_check_type", &battle_config.pc_cloak_check_type},
            {"monster_cloak_check_type", &battle_config.monster_cloak_check_type},
            {"mob_changetarget_byskill", &battle_config.mob_changetarget_byskill},
            {"player_attack_direction_change", &battle_config.pc_attack_direction_change},
            {"monster_attack_direction_change", &battle_config.monster_attack_direction_change},
            {"player_land_skill_limit", &battle_config.pc_land_skill_limit},
            {"monster_land_skill_limit", &battle_config.monster_land_skill_limit},
            {"party_skill_penaly", &battle_config.party_skill_penaly},
            {"monster_class_change_full_recover", &battle_config.monster_class_change_full_recover},
            {"produce_item_name_input", &battle_config.produce_item_name_input},
            {"produce_potion_name_input", &battle_config.produce_potion_name_input},
            {"making_arrow_name_input", &battle_config.making_arrow_name_input},
            {"holywater_name_input", &battle_config.holywater_name_input},
            {"display_delay_skill_fail", &battle_config.display_delay_skill_fail},
            {"chat_warpportal", &battle_config.chat_warpportal},
            {"mob_warpportal", &battle_config.mob_warpportal},
            {"dead_branch_active", &battle_config.dead_branch_active},
            {"show_steal_in_same_party", &battle_config.show_steal_in_same_party},
            {"enable_upper_class", &battle_config.enable_upper_class},
            {"mob_attack_attr_none", &battle_config.mob_attack_attr_none},
            {"mob_ghostring_fix", &battle_config.mob_ghostring_fix},
            {"pc_attack_attr_none", &battle_config.pc_attack_attr_none},
            {"gx_allhit", &battle_config.gx_allhit},
            {"gx_cardfix", &battle_config.gx_cardfix},
            {"gx_dupele", &battle_config.gx_dupele},
            {"gx_disptype", &battle_config.gx_disptype},
            {"player_skill_partner_check", &battle_config.player_skill_partner_check},
            {"hide_GM_session", &battle_config.hide_GM_session},
            {"unit_movement_type", &battle_config.unit_movement_type},
            {"invite_request_check", &battle_config.invite_request_check},
            {"skill_removetrap_type", &battle_config.skill_removetrap_type},
            {"disp_experience", &battle_config.disp_experience},
            {"item_rate_common", &battle_config.item_rate_common},   // Added by RoVeRT
            {"item_rate_equip", &battle_config.item_rate_equip},
            {"item_rate_card", &battle_config.item_rate_card},   // End Addition
            {"item_rate_heal", &battle_config.item_rate_heal},   // Added by Valaris
            {"item_rate_use", &battle_config.item_rate_use}, // End
            {"item_drop_common_min", &battle_config.item_drop_common_min},   // Added by TyrNemesis^
            {"item_drop_common_max", &battle_config.item_drop_common_max},
            {"item_drop_equip_min", &battle_config.item_drop_equip_min},
            {"item_drop_equip_max", &battle_config.item_drop_equip_max},
            {"item_drop_card_min", &battle_config.item_drop_card_min},
            {"item_drop_card_max", &battle_config.item_drop_card_max},
            {"item_drop_mvp_min", &battle_config.item_drop_mvp_min},
            {"item_drop_mvp_max", &battle_config.item_drop_mvp_max}, // End Addition
            {"prevent_logout", &battle_config.prevent_logout},   // Added by RoVeRT
            {"alchemist_summon_reward", &battle_config.alchemist_summon_reward}, // [Valaris]
            {"maximum_level", &battle_config.maximum_level}, // [Valaris]
            {"drops_by_luk", &battle_config.drops_by_luk},   // [Valaris]
            {"monsters_ignore_gm", &battle_config.monsters_ignore_gm},   // [Valaris]
            {"equipment_breaking", &battle_config.equipment_breaking},   // [Valaris]
            {"equipment_break_rate", &battle_config.equipment_break_rate},   // [Valaris]
            {"pk_mode", &battle_config.pk_mode}, // [Valaris]
            {"multi_level_up", &battle_config.multi_level_up},   // [Valaris]
            {"backstab_bow_penalty", &battle_config.backstab_bow_penalty},
            {"night_at_start", &battle_config.night_at_start},   // added by [Yor]
            {"day_duration", &battle_config.day_duration},   // added by [Yor]
            {"night_duration", &battle_config.night_duration},   // added by [Yor]
            {"show_mob_hp", &battle_config.show_mob_hp}, // [Valaris]
            {"hack_info_GM_level", &battle_config.hack_info_GM_level},   // added by [Yor]
            {"any_warp_GM_min_level", &battle_config.any_warp_GM_min_level}, // added by [Yor]
            {"packet_ver_flag", &battle_config.packet_ver_flag}, // added by [Yor]
            {"min_hair_style", &battle_config.min_hair_style},   // added by [MouseJstr]
            {"max_hair_style", &battle_config.max_hair_style},   // added by [MouseJstr]
            {"min_hair_color", &battle_config.min_hair_color},   // added by [MouseJstr]
            {"max_hair_color", &battle_config.max_hair_color},   // added by [MouseJstr]
            {"min_cloth_color", &battle_config.min_cloth_color}, // added by [MouseJstr]
            {"max_cloth_color", &battle_config.max_cloth_color}, // added by [MouseJstr]
            {"castrate_dex_scale", &battle_config.castrate_dex_scale},   // added by [MouseJstr]
            {"area_size", &battle_config.area_size}, // added by [MouseJstr]
            {"chat_lame_penalty", &battle_config.chat_lame_penalty},
            {"chat_spam_threshold", &battle_config.chat_spam_threshold},
            {"chat_spam_flood", &battle_config.chat_spam_flood},
            {"chat_spam_ban", &battle_config.chat_spam_ban},
            {"chat_spam_warn", &battle_config.chat_spam_warn},
            {"chat_maxline", &battle_config.chat_maxline},
            {"packet_spam_threshold", &battle_config.packet_spam_threshold},
            {"packet_spam_flood", &battle_config.packet_spam_flood},
            {"packet_spam_kick", &battle_config.packet_spam_kick},
            {"mask_ip_gms", &battle_config.mask_ip_gms}
        };

        if (line[0] == '/' && line[1] == '/')
            continue;
        if (sscanf (line, "%[^:]:%s", w1, w2) != 2)
            continue;
        for (i = 0; i < sizeof (data) / (sizeof (data[0])); i++)
            if (strcasecmp (w1, data[i].str) == 0)
                *data[i].val = config_switch (w2);

        if (strcasecmp (w1, "import") == 0)
            battle_config_read (w2);
    }
    fclose_ (fp);

    if (--count == 0)
    {
        if (battle_config.flooritem_lifetime < 1000)
            battle_config.flooritem_lifetime = LIFETIME_FLOORITEM * 1000;
        if (battle_config.restart_hp_rate < 0)
            battle_config.restart_hp_rate = 0;
        else if (battle_config.restart_hp_rate > 100)
            battle_config.restart_hp_rate = 100;
        if (battle_config.restart_sp_rate < 0)
            battle_config.restart_sp_rate = 0;
        else if (battle_config.restart_sp_rate > 100)
            battle_config.restart_sp_rate = 100;
        if (battle_config.natural_healhp_interval < NATURAL_HEAL_INTERVAL)
            battle_config.natural_healhp_interval = NATURAL_HEAL_INTERVAL;
        if (battle_config.natural_healsp_interval < NATURAL_HEAL_INTERVAL)
            battle_config.natural_healsp_interval = NATURAL_HEAL_INTERVAL;
        if (battle_config.natural_heal_skill_interval < NATURAL_HEAL_INTERVAL)
            battle_config.natural_heal_skill_interval = NATURAL_HEAL_INTERVAL;
        if (battle_config.natural_heal_weight_rate < 50)
            battle_config.natural_heal_weight_rate = 50;
        if (battle_config.natural_heal_weight_rate > 101)
            battle_config.natural_heal_weight_rate = 101;
        battle_config.monster_max_aspd =
            2000 - battle_config.monster_max_aspd * 10;
        if (battle_config.monster_max_aspd < 10)
            battle_config.monster_max_aspd = 10;
        if (battle_config.monster_max_aspd > 1000)
            battle_config.monster_max_aspd = 1000;
        battle_config.max_aspd = 2000 - battle_config.max_aspd * 10;
        if (battle_config.max_aspd < 10)
            battle_config.max_aspd = 10;
        if (battle_config.max_aspd > 1000)
            battle_config.max_aspd = 1000;
        if (battle_config.max_hp > 1000000)
            battle_config.max_hp = 1000000;
        if (battle_config.max_hp < 100)
            battle_config.max_hp = 100;
        if (battle_config.max_sp > 1000000)
            battle_config.max_sp = 1000000;
        if (battle_config.max_sp < 100)
            battle_config.max_sp = 100;
        if (battle_config.max_parameter < 10)
            battle_config.max_parameter = 10;
        if (battle_config.max_parameter > 10000)
            battle_config.max_parameter = 10000;

        if (battle_config.agi_penaly_count < 2)
            battle_config.agi_penaly_count = 2;
        if (battle_config.vit_penaly_count < 2)
            battle_config.vit_penaly_count = 2;

        if (battle_config.item_drop_common_min < 1) // Added by TyrNemesis^
            battle_config.item_drop_common_min = 1;
        if (battle_config.item_drop_common_max > 10000)
            battle_config.item_drop_common_max = 10000;
        if (battle_config.item_drop_equip_min < 1)
            battle_config.item_drop_equip_min = 1;
        if (battle_config.item_drop_equip_max > 10000)
            battle_config.item_drop_equip_max = 10000;
        if (battle_config.item_drop_card_min < 1)
            battle_config.item_drop_card_min = 1;
        if (battle_config.item_drop_card_max > 10000)
            battle_config.item_drop_card_max = 10000;
        if (battle_config.item_drop_mvp_min < 1)
            battle_config.item_drop_mvp_min = 1;
        if (battle_config.item_drop_mvp_max > 10000)
            battle_config.item_drop_mvp_max = 10000;    // End Addition

        if (battle_config.night_at_start < 0)   // added by [Yor]
            battle_config.night_at_start = 0;
        else if (battle_config.night_at_start > 1)  // added by [Yor]
            battle_config.night_at_start = 1;
        if (battle_config.day_duration < 0) // added by [Yor]
            battle_config.day_duration = 0;
        if (battle_config.night_duration < 0)   // added by [Yor]
            battle_config.night_duration = 0;

        if (battle_config.hack_info_GM_level < 0)   // added by [Yor]
            battle_config.hack_info_GM_level = 0;
        else if (battle_config.hack_info_GM_level > 100)
            battle_config.hack_info_GM_level = 100;

        if (battle_config.any_warp_GM_min_level < 0)    // added by [Yor]
            battle_config.any_warp_GM_min_level = 0;
        else if (battle_config.any_warp_GM_min_level > 100)
            battle_config.any_warp_GM_min_level = 100;

        if (battle_config.chat_spam_ban < 0)
            battle_config.chat_spam_ban = 0;
        else if (battle_config.chat_spam_ban > 32767)
            battle_config.chat_spam_ban = 32767;

        if (battle_config.chat_spam_flood < 0)
            battle_config.chat_spam_flood = 0;
        else if (battle_config.chat_spam_flood > 32767)
            battle_config.chat_spam_flood = 32767;

        if (battle_config.chat_spam_warn < 0)
            battle_config.chat_spam_warn = 0;
        else if (battle_config.chat_spam_warn > 32767)
            battle_config.chat_spam_warn = 32767;

        if (battle_config.chat_spam_threshold < 0)
            battle_config.chat_spam_threshold = 0;
        else if (battle_config.chat_spam_threshold > 32767)
            battle_config.chat_spam_threshold = 32767;

        if (battle_config.chat_maxline < 1)
            battle_config.chat_maxline = 1;
        else if (battle_config.chat_maxline > 512)
            battle_config.chat_maxline = 512;

        if (battle_config.packet_spam_threshold < 0)
            battle_config.packet_spam_threshold = 0;
        else if (battle_config.packet_spam_threshold > 32767)
            battle_config.packet_spam_threshold = 32767;

        if (battle_config.packet_spam_flood < 0)
            battle_config.packet_spam_flood = 0;
        else if (battle_config.packet_spam_flood > 32767)
            battle_config.packet_spam_flood = 32767;

        if (battle_config.packet_spam_kick < 0)
            battle_config.packet_spam_kick = 0;
        else if (battle_config.packet_spam_kick > 1)
            battle_config.packet_spam_kick = 1;

        if (battle_config.mask_ip_gms < 0)
            battle_config.mask_ip_gms = 0;
        else if (battle_config.mask_ip_gms > 1)
            battle_config.mask_ip_gms = 1;

        // at least 1 client must be accepted
        if ((battle_config.packet_ver_flag & 63) == 0)  // added by [Yor]
            battle_config.packet_ver_flag = 63; // accept all clients

    }

    return 0;
}
