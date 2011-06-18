#include "mob.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../common/timer.hpp"
#include "../common/socket.hpp"
#include "../common/db.hpp"
#include "../common/nullpo.hpp"
#include "../common/mt_rand.hpp"
#include "map.hpp"
#include "clif.hpp"
#include "intif.hpp"
#include "pc.hpp"
#include "itemdb.hpp"
#include "skill.hpp"
#include "battle.hpp"
#include "party.hpp"
#include "npc.hpp"

#ifndef max
#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif

#define MIN_MOBTHINKTIME 100

#define MOB_LAZYMOVEPERC 50     // Move probability in the negligent mode MOB (rate of 1000 minute)
#define MOB_LAZYWARPPERC 20     // Warp probability in the negligent mode MOB (rate of 1000 minute)

static int mob_walktoxy(struct mob_data *md, int x, int y, int easy);
static int mob_changestate(struct mob_data *md, int state, int type);
static int mob_deleteslave(struct mob_data *md);


struct mob_db mob_db[2001];

/*==========================================
 * Local prototype declaration   (only required thing)
 *------------------------------------------
 */
static int distance(int, int, int, int);
static int mob_makedummymobdb(int);
// last argument is actually uint8_t, but is often 0
static void mob_timer(timer_id, tick_t, uint32_t, int);
static int mob_unlocktarget(struct mob_data *md, int tick);

/*==========================================
 * Mob is searched with a name.
 *------------------------------------------
 */
int mobdb_searchname(const char *str)
{
    int i;

    for (i = 0; i < sizeof(mob_db) / sizeof(mob_db[0]); i++)
    {
        if (strcasecmp(mob_db[i].name, str) == 0
            || strcmp(mob_db[i].jname, str) == 0
            || memcmp(mob_db[i].name, str, 24) == 0
            || memcmp(mob_db[i].jname, str, 24) == 0)
            return i;
    }

    return 0;
}

/*==========================================
 * Id Mob is checked.
 *------------------------------------------
 */
int mobdb_checkid(const int id)
{
    if (id <= 0 || id >= (sizeof(mob_db) / sizeof(mob_db[0]))
        || mob_db[id].name[0] == '\0')
        return 0;

    return id;
}

static void mob_init(struct mob_data *md);

/*==========================================
 * The minimum data set for MOB spawning
 *------------------------------------------
 */
static int mob_spawn_dataset(struct mob_data *md, const char *mobname, int mob_class)
{
    nullpo_ret(md);

    if (strcmp(mobname, "--en--") == 0)
        memcpy(md->name, mob_db[mob_class].name, 24);
    else if (strcmp(mobname, "--ja--") == 0)
        memcpy(md->name, mob_db[mob_class].jname, 24);
    else
        memcpy(md->name, mobname, 24);

    md->prev = NULL;
    md->next = NULL;
    md->n = 0;
    md->base_class = md->mob_class = mob_class;
    md->id = npc_get_new_npc_id();

    memset(&md->state, 0, sizeof(md->state));
    md->timer = NULL;
    md->target_id = 0;
    md->attacked_id = 0;

    mob_init(md);

    return 0;
}

// Mutation values indicate how `valuable' a change to each stat is, XP wise.
// For one 256th of change, we give out that many 1024th fractions of XP change
// (i.e., 1024 means a 100% XP increase for a single point of adjustment, 4 means 100% XP bonus for doubling the value)
static int mutation_value[MOB_XP_BONUS] = {
    2,                          // MOB_LV
    3,                          // MOB_MAX_HP
    1,                          // MOB_STR
    2,                          // MOB_AGI
    1,                          // MOB_VIT
    0,                          // MOB_INT
    2,                          // MOB_DEX
    2,                          // MOB_LUK
    1,                          // MOB_ATK1
    1,                          // MOB_ATK2
    2,                          // MOB_ADELAY
    2,                          // MOB_DEF
    2,                          // MOB_MDEF
    2,                          // MOB_SPEED
};

// The mutation scale indicates how far `up' we can go, with 256 indicating 100%  Note that this may stack with multiple
// calls to `mutate'.
static int mutation_scale[MOB_XP_BONUS] = {
    16,                         // MOB_LV
    256,                        // MOB_MAX_HP
    32,                         // MOB_STR
    48,                         // MOB_AGI
    48,                         // MOB_VIT
    48,                         // MOB_INT
    48,                         // MOB_DEX
    64,                         // MOB_LUK
    48,                         // MOB_ATK1
    48,                         // MOB_ATK2
    80,                         // MOB_ADELAY
    48,                         // MOB_DEF
    48,                         // MOB_MDEF
    80,                         // MOB_SPEED
};

// The table below indicates the `average' value for each of the statistics, or -1 if there is none.
// This average is used to determine XP modifications for mutations.  The experience point bonus is
// based on mutation_value and mutation_base as follows:
// (1) first, compute the percentage change of the attribute (p0)
// (2) second, determine the absolute stat change
// (3) third, compute the percentage stat change relative to mutation_base (p1)
// (4) fourth, compute the XP mofication based on the smaller of (p0, p1).
static int mutation_base[MOB_XP_BONUS] = {
    30,                         // MOB_LV
    -1,                         // MOB_MAX_HP
    20,                         // MOB_STR
    20,                         // MOB_AGI
    20,                         // MOB_VIT
    20,                         // MOB_INT
    20,                         // MOB_DEX
    20,                         // MOB_LUK
    -1,                         // MOB_ATK1
    -1,                         // MOB_ATK2
    -1,                         // MOB_ADELAY
    -1,                         // MOB_DEF
    20,                         // MOB_MDEF
    -1,                         // MOB_SPEED
};

/*========================================
 * Mutates a MOB.  For large `direction' values, calling this multiple times will give bigger XP boni.
 *----------------------------------------
 */
static void mob_mutate(struct mob_data *md, int stat, int intensity)   // intensity: positive: strengthen, negative: weaken.  256 = 100%.
{
    int old_stat;
    int new_stat;
    int real_intensity;        // relative intensity
    const int mut_base = mutation_base[stat];
    int sign = 1;

    if (!md || stat < 0 || stat >= MOB_XP_BONUS || intensity == 0)
        return;

    while (intensity > mutation_scale[stat])
    {
        mob_mutate(md, stat, mutation_scale[stat]);    // give better XP assignments
        intensity -= mutation_scale[stat];
    }
    while (intensity < -mutation_scale[stat])
    {
        mob_mutate(md, stat, mutation_scale[stat]);    // give better XP assignments
        intensity += mutation_scale[stat];
    }

    if (!intensity)
        return;

    // MOB_ADELAY and MOB_SPEED are special because going DOWN is good here.
    if (stat == MOB_ADELAY || stat == MOB_SPEED)
        sign = -1;

    // Now compute the new stat
    old_stat = md->stats[stat];
    new_stat = old_stat + ((old_stat * sign * intensity) / 256);

    if (new_stat < 0)
        new_stat = 0;

    if (old_stat == 0)
        real_intensity = 0;
    else
        real_intensity = (((new_stat - old_stat) << 8) / old_stat);

    if (mut_base != -1)
    {
        // Now compute the mutation intensity relative to an absolute value.
        // Take the lesser of the two effects.
        int real_intensity2 = (((new_stat - old_stat) << 8) / mut_base);

        if (real_intensity < 0)
            if (real_intensity2 > real_intensity)
                real_intensity = real_intensity2;

        if (real_intensity > 0)
            if (real_intensity2 < real_intensity)
                real_intensity = real_intensity2;
    }

    real_intensity *= sign;

    md->stats[stat] = new_stat;

    // Adjust XP value
    md->stats[MOB_XP_BONUS] += mutation_value[stat] * real_intensity;
    if (md->stats[MOB_XP_BONUS] <= 0)
        md->stats[MOB_XP_BONUS] = 1;

    // Sanitise
    if (md->stats[MOB_ATK1] > md->stats[MOB_ATK2])
    {
        int swap = md->stats[MOB_ATK2];
        md->stats[MOB_ATK2] = md->stats[MOB_ATK1];
        md->stats[MOB_ATK1] = swap;
    }
}

// This calculates the exp of a given mob
static int mob_gen_exp(struct mob_db *mob)
{
    if (mob->max_hp <= 1)
        return 1;
    double mod_def = 100 - mob->def;
    if (mod_def < 1)
        mod_def = 1;
    double effective_hp =
        ((50 - mob->luk) * mob->max_hp / 50.0) +
        (2 * mob->luk * mob->max_hp / mod_def);
    double attack_factor =
        (mob->atk1 + mob->atk2 + mob->str / 3.0 + mob->dex / 2.0 +
         mob->luk) * (1872.0 / mob->adelay) / 4;
    double dodge_factor =
        pow(mob->lv + mob->agi + mob->luk / 2.0, 4.0 / 3.0);
    double persuit_factor =
        (3 + mob->range) * (mob->mode % 2) * 1000 / mob->speed;
    double aggression_factor = (mob->mode & 4) == 4 ? 10.0 / 9.0 : 1.0;
    int xp =
        floor(effective_hp
                * pow(sqrt(attack_factor) + sqrt(dodge_factor) + sqrt(persuit_factor) + 55, 3)
                * aggression_factor / 2000000.0 * battle_config.base_exp_rate / 100.);
    if (xp < 1)
        xp = 1;
    printf("Exp for mob '%s' generated: %d\n", mob->name, xp);
    return xp;
}

static void mob_init(struct mob_data *md)
{
    int i;
    const int mob_class = md->mob_class;
    const int mutations_nr = mob_db[mob_class].mutations_nr;
    const int mutation_power = mob_db[mob_class].mutation_power;

    md->stats[MOB_LV] = mob_db[mob_class].lv;
    md->stats[MOB_MAX_HP] = mob_db[mob_class].max_hp;
    md->stats[MOB_STR] = mob_db[mob_class].str;
    md->stats[MOB_AGI] = mob_db[mob_class].agi;
    md->stats[MOB_VIT] = mob_db[mob_class].vit;
    md->stats[MOB_INT] = mob_db[mob_class].int_;
    md->stats[MOB_DEX] = mob_db[mob_class].dex;
    md->stats[MOB_LUK] = mob_db[mob_class].luk;
    md->stats[MOB_ATK1] = mob_db[mob_class].atk1;
    md->stats[MOB_ATK2] = mob_db[mob_class].atk2;
    md->stats[MOB_ADELAY] = mob_db[mob_class].adelay;
    md->stats[MOB_DEF] = mob_db[mob_class].def;
    md->stats[MOB_MDEF] = mob_db[mob_class].mdef;
    md->stats[MOB_SPEED] = mob_db[mob_class].speed;
    md->stats[MOB_XP_BONUS] = MOB_XP_BONUS_BASE;

    for (i = 0; i < mutations_nr; i++)
    {
        int stat_nr = MRAND(MOB_XP_BONUS + 1);
        int strength;

        if (stat_nr >= MOB_XP_BONUS)
            stat_nr = MOB_MAX_HP;

        strength =
            ((MRAND((mutation_power >> 1)) +
              (MRAND((mutation_power >> 1))) +
              2) * mutation_scale[stat_nr]) / 100;

        strength = MRAND(2) ? strength : -strength;

        if (strength < -240)
            strength = -240;    /* Don't go too close to zero */

        mob_mutate(md, stat_nr, strength);
    }
}

/*==========================================
 * The MOB appearance for one time(for scripts)
 *------------------------------------------
 */
int mob_once_spawn(MapSessionData *sd, const fixed_string<16>& mapname,
                    int x, int y, const char *mobname, int mob_class, int amount,
                    const char *event)
{
    struct mob_data *md = NULL;
    int m, count, r = mob_class;

    if (sd && strcmp(&mapname, "this") == 0)
        m = sd->m;
    else
        m = map_mapname2mapid(mapname);

    if (m < 0 || amount <= 0 || mob_class <= 1000 || mob_class > 2000)  // 値が異常なら召喚を止める
        return 0;

    if (sd)
    {
        if (x <= 0)
            x = sd->x;
        if (y <= 0)
            y = sd->y;
    }
    else if (x <= 0 || y <= 0)
    {
        printf("mob_once_spawn: ??\n");
    }

    for (count = 0; count < amount; count++)
    {
        md = new mob_data;
        if (mob_db[mob_class].mode & 0x02)
        {
            CREATE(md->lootitem, struct item, LOOTITEM_SIZE);
        }
        else
            md->lootitem = NULL;

        mob_spawn_dataset(md, mobname, mob_class);
        md->m = m;
        md->x = x;
        md->y = y;
        if (r < 0 && battle_config.dead_branch_active == 1)
            md->mode = 0x1 + 0x4 + 0x80;    //移動してアクティブで反撃する
        md->m_0 = m;
        md->x_0 = x;
        md->y_0 = y;
        md->xs = 0;
        md->ys = 0;
        md->spawndelay_1 = -1;   // Only once is a flag.
        md->spawndelay2 = -1;   // Only once is a flag.

        memcpy(md->npc_event, event, sizeof(md->npc_event));

        map_addiddb(md);
        mob_spawn(md->id);

    }
    return (amount > 0) ? md->id : 0;
}

/*==========================================
 * The MOB appearance for one time(& area specification for scripts)
 *------------------------------------------
 */
int mob_once_spawn_area(MapSessionData *sd, const fixed_string<16>& mapname,
                         int x_0, int y_0, int x_1, int y_1,
                         const char *mobname, int mob_class, int amount,
                         const char *event)
{
    int x, y, i, c, max, lx = -1, ly = -1, id = 0;
    int m;

    if (strcmp(&mapname, "this") == 0)
        m = sd->m;
    else
        m = map_mapname2mapid(mapname);

    max = (y_1 - y_0 + 1) * (x_1 - x_0 + 1) * 3;
    if (max > 1000)
        max = 1000;

    if (m < 0 || amount <= 0 || (mob_class >= 0 && mob_class <= 1000) || mob_class > 2000)  // A summon is stopped if a value is unusual
        return 0;

    for (i = 0; i < amount; i++)
    {
        int j = 0;
        do
        {
            x = MPRAND(x_0, (x_1 - x_0 + 1));
            y = MPRAND(y_0, (y_1 - y_0 + 1));
        }
        while (((c = map_getcell(m, x, y)) == 1 || c == 5) && (++j) < max);
        if (j >= max)
        {
            if (lx >= 0)
            {                   // Since reference went wrong, the place which boiled before is used.
                x = lx;
                y = ly;
            }
            else
                return 0;       // Since reference of the place which boils first went wrong, it stops.
        }
        id = mob_once_spawn(sd, mapname, x, y, mobname, mob_class, 1, event);
        lx = x;
        ly = y;
    }
    return id;
}

/*==========================================
 * Is MOB in the state in which the present movement is possible or not?
 *------------------------------------------
 */
static int mob_can_move(struct mob_data *md)
{
    nullpo_ret(md);

    if (md->canmove_tick > gettick() || (md->opt1 > 0 && md->opt1 != 6)
        || md->option & 2)
        return 0;
    return 1;
}

/*==========================================
 * Time calculation concerning one step next to mob
 *------------------------------------------
 */
static int calc_next_walk_step(struct mob_data *md)
{
    nullpo_ret(md);

    if (md->walkpath.path_pos >= md->walkpath.path_len)
        return -1;
    if (static_cast<int>(md->walkpath.path[md->walkpath.path_pos]) & 1)
        return battle_get_speed(md) * 14 / 10;
    return battle_get_speed(md);
}

static int mob_walktoxy_sub(struct mob_data *md);

/*==========================================
 * Mob Walk processing
 *------------------------------------------
 */
static int mob_walk(struct mob_data *md, unsigned int tick, uint8_t data)
{
    int moveblock;
    int i, ctype;
    static int dirx[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };
    static int diry[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
    int x, y, dx, dy;

    nullpo_ret(md);

    md->state.state = MS_IDLE;
    if (md->walkpath.path_pos >= md->walkpath.path_len
        || md->walkpath.path_pos != data)
        return 0;

    md->walkpath.path_half ^= 1;
    if (md->walkpath.path_half == 0)
    {
        md->walkpath.path_pos++;
        if (md->state.change_walk_target)
        {
            mob_walktoxy_sub(md);
            return 0;
        }
    }
    else
    {
        if (static_cast<int>(md->walkpath.path[md->walkpath.path_pos]) >= 8)
            return 1;

        x = md->x;
        y = md->y;
        ctype = map_getcell(md->m, x, y);
        if (ctype == 1 || ctype == 5)
        {
            mob_stop_walking(md, 1);
            return 0;
        }
        md->dir = md->walkpath.path[md->walkpath.path_pos];
        dx = dirx[static_cast<int>(md->dir)];
        dy = diry[static_cast<int>(md->dir)];

        ctype = map_getcell(md->m, x + dx, y + dy);
        if (ctype == 1 || ctype == 5)
        {
            mob_walktoxy_sub(md);
            return 0;
        }

        moveblock = (x / BLOCK_SIZE != (x + dx) / BLOCK_SIZE
                     || y / BLOCK_SIZE != (y + dy) / BLOCK_SIZE);

        md->state.state = MS_WALK;
        map_foreachinmovearea(clif_moboutsight, md->m, x - AREA_SIZE,
                              y - AREA_SIZE, x + AREA_SIZE, y + AREA_SIZE,
                              dx, dy, BL_PC, md);

        x += dx;
        y += dy;
        if (md->min_chase > 13)
            md->min_chase--;

        if (moveblock)
            map_delblock(md);
        md->x = x;
        md->y = y;
        if (moveblock)
            map_addblock(md);

        map_foreachinmovearea(clif_mobinsight, md->m, x - AREA_SIZE,
                              y - AREA_SIZE, x + AREA_SIZE, y + AREA_SIZE,
                              -dx, -dy, BL_PC, md);
        md->state.state = MS_IDLE;
    }
    if ((i = calc_next_walk_step(md)) > 0)
    {
        i = i >> 1;
        if (i < 1 && md->walkpath.path_half == 0)
            i = 1;
        md->timer = add_timer(tick + i, mob_timer, md->id, static_cast<int>(md->walkpath.path_pos));
        md->state.state = MS_WALK;

        if (md->walkpath.path_pos >= md->walkpath.path_len)
            clif_fixmobpos(md);    // When mob stops, retransmission current of a position.
    }
    return 0;
}

/*==========================================
 * Check if mob should be attempting to attack
 *------------------------------------------
 */
static int mob_check_attack(struct mob_data *md)
{
    BlockList *tbl = NULL;
    MapSessionData *tsd = NULL;
    struct mob_data *tmd = NULL;

    int mode, race, range;

    nullpo_ret(md);

    md->min_chase = 13;
    md->state.state = MS_IDLE;

    if (md->opt1 > 0 || md->option & 2)
        return 0;

    if ((tbl = map_id2bl(md->target_id)) == NULL)
    {
        md->target_id = 0;
        md->state.targettype = NONE_ATTACKABLE;
        return 0;
    }

    if (tbl->type == BL_PC)
        tsd = static_cast<MapSessionData *>(tbl);
    else if (tbl->type == BL_MOB)
        tmd = static_cast<struct mob_data *>(tbl);
    else
        return 0;

    if (tsd)
    {
        if (pc_isdead(tsd) || tsd->invincible_timer
            || pc_isinvisible(tsd) || md->m != tbl->m || tbl->prev == NULL
            || distance(md->x, md->y, tbl->x, tbl->y) >= 13)
        {
            md->target_id = 0;
            md->state.targettype = NONE_ATTACKABLE;
            return 0;
        }
    }
    if (tmd)
    {
        if (md->m != tbl->m || tbl->prev == NULL
            || distance(md->x, md->y, tbl->x, tbl->y) >= 13)
        {
            md->target_id = 0;
            md->state.targettype = NONE_ATTACKABLE;
            return 0;
        }
    }

    if (!md->mode)
        mode = mob_db[md->mob_class].mode;
    else
        mode = md->mode;

    race = mob_db[md->mob_class].race;
    if (!(mode & 0x80))
    {
        md->target_id = 0;
        md->state.targettype = NONE_ATTACKABLE;
        return 0;
    }
    if (tsd && !(mode & 0x20) && ((pc_ishiding(tsd)
                                    || tsd->state.gangsterparadise)
                                   && race != 4 && race != 6))
    {
        md->target_id = 0;
        md->state.targettype = NONE_ATTACKABLE;
        return 0;
    }

    range = mob_db[md->mob_class].range;
    if (mode & 1)
        range++;
    if (distance(md->x, md->y, tbl->x, tbl->y) > range)
        return 0;

    return 1;
}

/*==========================================
 * Attack processing of mob
 *------------------------------------------
 */
static int mob_attack(struct mob_data *md, unsigned int tick, int)
{
    BlockList *tbl = NULL;

    nullpo_ret(md);

    if ((tbl = map_id2bl(md->target_id)) == NULL)
        return 0;

    if (!mob_check_attack(md))
        return 0;

    if (battle_config.monster_attack_direction_change)
        md->dir = map_calc_dir(md, tbl->x, tbl->y);   // 向き設定

    //clif_fixmobpos(md);

    md->target_lv = battle_weapon_attack(md, tbl, tick);

    md->attackabletime = tick + battle_get_adelay(md);

    md->timer = add_timer(md->attackabletime, mob_timer, md->id, 0);
    md->state.state = MS_ATTACK;

    return 0;
}

/*==========================================
 * The attack of PC which is attacking id is stopped.
 * The callback function of clif_foreachclient
 *------------------------------------------
 */
static void mob_stopattacked(MapSessionData *sd, uint32_t id)
{
    nullpo_retv(sd);

    if (sd->attacktarget == id)
        pc_stopattack(sd);
}

/*==========================================
 * The timer in which the mob's states changes
 *------------------------------------------
 */
int mob_changestate(struct mob_data *md, int state, int type)
{
    unsigned int tick;
    int i;

    nullpo_ret(md);

    if (md->timer)
        delete_timer(md->timer);
    md->timer = NULL;
    md->state.state = state;

    switch (state)
    {
        case MS_WALK:
            if ((i = calc_next_walk_step(md)) > 0)
            {
                i = i >> 2;
                md->timer =
                    add_timer(gettick() + i, mob_timer, md->id, 0);
            }
            else
                md->state.state = MS_IDLE;
            break;
        case MS_ATTACK:
            tick = gettick();
            i = DIFF_TICK(md->attackabletime, tick);
            if (i > 0 && i < 2000)
                md->timer =
                    add_timer(md->attackabletime, mob_timer, md->id, 0);
            else if (type)
            {
                md->attackabletime = tick + battle_get_amotion(md);
                md->timer =
                    add_timer(md->attackabletime, mob_timer, md->id, 0);
            }
            else
            {
                md->attackabletime = tick + 1;
                md->timer =
                    add_timer(md->attackabletime, mob_timer, md->id, 0);
            }
            break;
        case MS_DELAY:
            md->timer =
                add_timer(gettick() + type, mob_timer, md->id, 0);
            break;
        case MS_DEAD:
            skill_castcancel(md);
            md->last_deadtime = gettick();
            // Since it died, all aggressors' attack to this mob is stopped.
            for (MapSessionData *sd : auth_sessions)
                mob_stopattacked(sd, md->id);
            skill_status_change_clear(md, 2); // The abnormalities in status are canceled.
            if (md->deletetimer)
                delete_timer(md->deletetimer);
            md->deletetimer = NULL;
            md->hp = md->target_id = md->attacked_id = 0;
            md->state.targettype = NONE_ATTACKABLE;
            break;
    }

    return 0;
}

/*==========================================
 * timer processing of mob(timer function)
 * It branches to a walk and an attack.
 *------------------------------------------
 */
static void mob_timer(timer_id, tick_t tick, uint32_t id, int data)
{
    struct mob_data *md;
    BlockList *bl = map_id2bl(id);

    if (!bl || !bl->type || bl->type != BL_MOB)
        return;

    nullpo_retv(md = static_cast<struct mob_data *>(bl));

    if (!md->type || md->type != BL_MOB)
        return;

    md->timer = NULL;
    if (md->prev == NULL || md->state.state == MS_DEAD)
        return;

    map_freeblock_lock();
    switch (md->state.state)
    {
        case MS_WALK:
            mob_check_attack(md);
            mob_walk(md, tick, data);
            break;
        case MS_ATTACK:
            mob_attack(md, tick, data);
            break;
        case MS_DELAY:
            mob_changestate(md, MS_IDLE, 0);
            break;
        default:
            map_log("mob_timer map_log: %d ?\n", md->state.state);
            break;
    }
    map_freeblock_unlock();
    return;
}

/*==========================================
 *
 *------------------------------------------
 */
static int mob_walktoxy_sub(struct mob_data *md)
{
    struct walkpath_data wpd;

    nullpo_ret(md);

    if (path_search
        (&wpd, md->m, md->x, md->y, md->to_x, md->to_y,
         md->state.walk_easy))
        return 1;
    memcpy(&md->walkpath, &wpd, sizeof(wpd));

    md->state.change_walk_target = 0;
    mob_changestate(md, MS_WALK, 0);
    clif_movemob(md);

    return 0;
}

/*==========================================
 * mob move start
 *------------------------------------------
 */
int mob_walktoxy(struct mob_data *md, int x, int y, int easy)
{
    struct walkpath_data wpd;

    nullpo_ret(md);

    if (md->state.state == MS_WALK
        && path_search(&wpd, md->m, md->x, md->y, x, y, easy))
        return 1;

    md->state.walk_easy = easy;
    md->to_x = x;
    md->to_y = y;
    if (md->state.state == MS_WALK)
    {
        md->state.change_walk_target = 1;
    }
    else
    {
        return mob_walktoxy_sub(md);
    }

    return 0;
}

/*==========================================
 * mob spawn with delay(timer function)
 *------------------------------------------
 */
static void mob_delayspawn(timer_id, tick_t, int m)
{
    mob_spawn(m);
}

/*==========================================
 * spawn timing calculation
 *------------------------------------------
 */
static int mob_setdelayspawn(int id)
{
    unsigned int spawntime, spawntime1, spawntime2, spawntime3;
    struct mob_data *md;
    BlockList *bl;

    if ((bl = map_id2bl(id)) == NULL)
        return -1;

    if (!bl || !bl->type || bl->type != BL_MOB)
        return -1;

    nullpo_retr(-1, md = static_cast<struct mob_data *>(bl));

    if (!md || md->type != BL_MOB)
        return -1;

    // Processing of MOB which is not revitalized
    if (md->spawndelay_1 == -1 && md->spawndelay2 == -1 && md->n == 0)
    {
        map_deliddb(md);
        map_freeblock(md);     // Instead of [ of free ]
        return 0;
    }

    spawntime1 = md->last_spawntime + md->spawndelay_1;
    spawntime2 = md->last_deadtime + md->spawndelay2;
    spawntime3 = gettick() + 5000;
    // spawntime = max(spawntime1,spawntime2,spawntime3);
    if (DIFF_TICK(spawntime1, spawntime2) > 0)
    {
        spawntime = spawntime1;
    }
    else
    {
        spawntime = spawntime2;
    }
    if (DIFF_TICK(spawntime3, spawntime) > 0)
    {
        spawntime = spawntime3;
    }

    add_timer(spawntime, mob_delayspawn, id);
    return 0;
}

/*==========================================
 * Mob spawning. Initialization is also variously here.
 *------------------------------------------
 */
int mob_spawn(int id)
{
    int x = 0, y = 0, i = 0, c;
    unsigned int tick = gettick();
    struct mob_data *md;
    BlockList *bl;

    nullpo_retr(-1, bl = map_id2bl(id));

    if (!bl || !bl->type || bl->type != BL_MOB)
        return -1;

    nullpo_retr(-1, md = static_cast<struct mob_data *>(bl));

    if (!md || !md->type || md->type != BL_MOB)
        return -1;

    md->last_spawntime = tick;
    if (md->prev != NULL)
    {
//      clif_being_remove(md,3);
        map_delblock(md);
    }
    else
        md->mob_class = md->base_class;

    md->m = md->m_0;
    do
    {
        if (md->x_0 == 0 && md->y_0 == 0)
        {
            x = MPRAND(1, (maps[md->m].xs - 2));
            y = MPRAND(1, (maps[md->m].ys - 2));
        }
        else
        {
            x = MPRAND(md->x_0, (md->xs + 1)) - md->xs / 2;
            y = MPRAND(md->y_0, (md->ys + 1)) - md->ys / 2;
        }
        i++;
    }
    while (((c = map_getcell(md->m, x, y)) == 1 || c == 5) && i < 50);

    if (i >= 50)
    {
        add_timer(tick + 5000, mob_delayspawn, id);
        return 1;
    }

    md->to_x = md->x = x;
    md->to_y = md->y = y;
    md->dir = Direction::S;

    map_addblock(md);

    memset(&md->state, 0, sizeof(md->state));
    md->attacked_id = 0;
    md->target_id = 0;
    md->move_fail_count = 0;
    mob_init(md);

    if (!md->stats[MOB_SPEED])
        md->stats[MOB_SPEED] = mob_db[md->mob_class].speed;
    md->def_ele = mob_db[md->mob_class].element;
    md->master_id = 0;
    md->master_dist = 0;

    md->state.state = MS_IDLE;
    md->timer = NULL;
    md->last_thinktime = tick;
    md->next_walktime = tick + MPRAND(5000, 50);
    md->attackabletime = tick;
    md->canmove_tick = tick;

    md->deletetimer = NULL;

    memset(md->dmglog, 0, sizeof(md->dmglog));
    if (md->lootitem)
        memset(md->lootitem, 0, sizeof(md->lootitem));
    md->lootitem_count = 0;

    for (i = 0; i < MAX_STATUSCHANGE; i++)
    {
        md->sc_data[i].timer = NULL;
        md->sc_data[i].val1 = 0;
    }
    md->sc_count = 0;
    md->opt1 = md->opt2 = md->opt3 = md->option = 0;

    md->hp = battle_get_max_hp(md);
    if (md->hp <= 0)
    {
        mob_makedummymobdb(md->mob_class);
        md->hp = battle_get_max_hp(md);
    }

    clif_spawnmob(md);

    return 0;
}

/*==========================================
 * Distance calculation between two points
 *------------------------------------------
 */
static int distance(int x_0, int y_0, int x_1, int y_1)
{
    int dx, dy;

    dx = abs(x_0 - x_1);
    dy = abs(y_0 - y_1);
    return dx > dy ? dx : dy;
}

/*==========================================
 * The stop of MOB's attack
 *------------------------------------------
 */
int mob_stopattack(struct mob_data *md)
{
    md->target_id = 0;
    md->state.targettype = NONE_ATTACKABLE;
    md->attacked_id = 0;
    return 0;
}

/*==========================================
 * The stop of MOB's walking
 *------------------------------------------
 */
int mob_stop_walking(struct mob_data *md, int type)
{
    nullpo_ret(md);

    if (md->state.state == MS_WALK || md->state.state == MS_IDLE)
    {
        int dx = 0, dy = 0;

        md->walkpath.path_len = 0;
        if (type & 4)
        {
            dx = md->to_x - md->x;
            if (dx < 0)
                dx = -1;
            else if (dx > 0)
                dx = 1;
            dy = md->to_y - md->y;
            if (dy < 0)
                dy = -1;
            else if (dy > 0)
                dy = 1;
        }
        md->to_x = md->x + dx;
        md->to_y = md->y + dy;
        if (dx != 0 || dy != 0)
        {
            mob_walktoxy_sub(md);
            return 0;
        }
        mob_changestate(md, MS_IDLE, 0);
    }
    if (type & 0x01)
        clif_fixmobpos(md);
    if (type & 0x02)
    {
        int delay = battle_get_dmotion(md);
        unsigned int tick = gettick();
        if (md->canmove_tick < tick)
            md->canmove_tick = tick + delay;
    }

    return 0;
}

/*==========================================
 * Reachability to a Specification ID existence place
 *------------------------------------------
 */
static int mob_can_reach(struct mob_data *md, BlockList *bl, int range)
{
    int dx, dy;
    struct walkpath_data wpd;
    int i;

    nullpo_ret(md);
    nullpo_ret(bl);

    dx = abs(bl->x - md->x);
    dy = abs(bl->y - md->y);

    if (bl && bl->type == BL_PC && battle_config.monsters_ignore_gm == 1)
    {                           // option to have monsters ignore GMs [Valaris]
        MapSessionData *sd = static_cast<MapSessionData *>(bl);
        if (sd && pc_isGM(sd))
            return 0;
    }

    if (md->m != bl->m)      // 違うャbプ
        return 0;

    if (range > 0 && range < ((dx > dy) ? dx : dy)) // 遠すぎる
        return 0;

    if (md->x == bl->x && md->y == bl->y) // 同じャX
        return 1;

    // Obstacle judging
    wpd.path_len = 0;
    wpd.path_pos = 0;
    wpd.path_half = 0;
    if (path_search(&wpd, md->m, md->x, md->y, bl->x, bl->y, 0) !=
        -1)
        return 1;

    if (bl->type != BL_PC && bl->type != BL_MOB)
        return 0;

    // It judges whether it can adjoin or not.
    dx = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
    dy = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
    if (path_search
        (&wpd, md->m, md->x, md->y, bl->x - dx, bl->y - dy, 0) != -1)
        return 1;
    for (i = 0; i < 9; i++)
    {
        if (path_search
            (&wpd, md->m, md->x, md->y, bl->x - 1 + i / 3,
             bl->y - 1 + i % 3, 0) != -1)
            return 1;
    }
    return 0;
}

/*==========================================
 * Determination for an attack of a monster
 *------------------------------------------
 */
int mob_target(struct mob_data *md, BlockList *bl, int dist)
{
    MapSessionData *sd;
    short *option;
    int mode, race;

    nullpo_ret(md);
    nullpo_ret(bl);

    option = battle_get_option(bl);
    race = mob_db[md->mob_class].race;

    if (!md->mode)
    {
        mode = mob_db[md->mob_class].mode;
    }
    else
    {
        mode = md->mode;
    }
    if (!(mode & 0x80))
    {
        md->target_id = 0;
        return 0;
    }
    // Nothing will be carried out if there is no mind of changing TAGE by TAGE ending.
    if ((md->target_id > 0 && md->state.targettype == ATTACKABLE)
        && (!(mode & 0x04) || MRAND(100) > 25))
        return 0;

    if (mode & 0x20 ||          // Coercion is exerted if it is MVPMOB.
        ((option && !(*option & 0x06)) || race == 4 || race == 6))
    {
        if (bl->type == BL_PC)
        {
            nullpo_ret(sd = static_cast<MapSessionData *>(bl));
            if (sd->invincible_timer || pc_isinvisible(sd))
                return 0;
            if (!(mode & 0x20) && race != 4 && race != 6
                && sd->state.gangsterparadise)
                return 0;
        }

        md->target_id = bl->id; // Since there was no disturbance, it locks on to target.
        if (bl->type == BL_PC || bl->type == BL_MOB)
            md->state.targettype = ATTACKABLE;
        else
            md->state.targettype = NONE_ATTACKABLE;
        md->min_chase = dist + 13;
        if (md->min_chase > 26)
            md->min_chase = 26;
    }
    return 0;
}

/*==========================================
 * The ?? routine of an active monster
 *------------------------------------------
 */
static void mob_ai_sub_hard_activesearch(BlockList *bl, struct mob_data *smd, int *pcc)
{
    MapSessionData *tsd = NULL;
    struct mob_data *tmd = NULL;
    int mode, race, dist;

    nullpo_retv(bl);
    nullpo_retv(smd);
    nullpo_retv(pcc);

    if (bl->type == BL_PC)
        tsd = static_cast<MapSessionData *>(bl);
    else if (bl->type == BL_MOB)
        tmd = static_cast<struct mob_data *>(bl);
    else
        return;

    //敵味方判定
    if (battle_check_target(smd, bl) == 0)
        return;

    if (!smd->mode)
        mode = mob_db[smd->mob_class].mode;
    else
        mode = smd->mode;

    // アクティブでターゲット射程内にいるなら、ロックする
    if (mode & 0x04)
    {
        race = mob_db[smd->mob_class].race;
        //対象がPCの場合
        if (tsd &&
            !pc_isdead(tsd) &&
            tsd->m == smd->m &&
            tsd->invincible_timer == NULL &&
            !pc_isinvisible(tsd) &&
            (dist =
             distance(smd->x, smd->y, tsd->x, tsd->y)) < 9)
        {
            if (mode & 0x20 ||
                ((!pc_ishiding(tsd) && !tsd->state.gangsterparadise)
                  || race == 4 || race == 6))
            {                   // 妨害がないか判定
                if (mob_can_reach(smd, bl, 12) &&  // 到達可能性判定
                    MRAND(1000) < 1000 / (++(*pcc)))
                {               // 範囲内PCで等確率にする
                    smd->target_id = tsd->id;
                    smd->state.targettype = ATTACKABLE;
                    smd->min_chase = 13;
                }
            }
        }
        //対象がMobの場合
        else if (tmd &&
                 tmd->m == smd->m &&
                 (dist =
                  distance(smd->x, smd->y, tmd->x, tmd->y)) < 9)
        {
            if (mob_can_reach(smd, bl, 12) &&  // 到達可能性判定
                MRAND(1000) < 1000 / (++(*pcc)))
            {                   // 範囲内で等確率にする
                smd->target_id = bl->id;
                smd->state.targettype = ATTACKABLE;
                smd->min_chase = 13;
            }
        }
    }
}

/*==========================================
 * loot monster item search
 *------------------------------------------
 */
static void mob_ai_sub_hard_lootsearch(BlockList *bl,
                                       struct mob_data *md,
                                       int *itc)
{
    int mode, dist;

    nullpo_retv(bl);
    nullpo_retv(md);
    nullpo_retv(itc);

    if (!md->mode)
    {
        mode = mob_db[md->mob_class].mode;
    }
    else
    {
        mode = md->mode;
    }

    if (!md->target_id && mode & 0x02)
    {
        if (!md->lootitem
            || (battle_config.monster_loot_type == 1
                && md->lootitem_count >= LOOTITEM_SIZE))
            return;
        if (bl->m == md->m
            && (dist = distance(md->x, md->y, bl->x, bl->y)) < 9)
        {
            if (mob_can_reach(md, bl, 12) &&   // Reachability judging
                MRAND(1000) < 1000 / (++(*itc)))
            {                   // It is made a probability, such as within the limits PC.
                md->target_id = bl->id;
                md->state.targettype = NONE_ATTACKABLE;
                md->min_chase = 13;
            }
        }
    }
}

/*==========================================
 * The ?? routine of a link monster
 *------------------------------------------
 */
static void mob_ai_sub_hard_linksearch(BlockList *bl,
                                       struct mob_data *md,
                                       MapSessionData *target)
{

    nullpo_retv(bl);
    struct mob_data *tmd = static_cast<struct mob_data *>(bl);
    nullpo_retv(md);
    nullpo_retv(target);

    if (md->attacked_id > 0 && mob_db[md->mob_class].mode & 0x08)
    {
        if (tmd->mob_class == md->mob_class && tmd->m == md->m
            && (!tmd->target_id || md->state.targettype == NONE_ATTACKABLE))
        {
            if (mob_can_reach(tmd, target, 12))
            {                   // Reachability judging
                tmd->target_id = md->attacked_id;
                tmd->state.targettype = ATTACKABLE;
                tmd->min_chase = 13;
            }
        }
    }
}

/*==========================================
 * Processing of slave monsters
 *------------------------------------------
 */
static int mob_ai_sub_hard_slavemob(struct mob_data *md, unsigned int tick)
{
    struct mob_data *mmd = NULL;
    BlockList *bl;
    int mode, race, old_dist;

    nullpo_ret(md);

    if ((bl = map_id2bl(md->master_id)) != NULL)
        mmd = static_cast<struct mob_data *>(bl);

    mode = mob_db[md->mob_class].mode;

    // It is not main monster/leader.
    if (!mmd || mmd->type != BL_MOB || mmd->id != md->master_id)
        return 0;

    // Since it is in the map on which the master is not, teleport is carried out and it pursues.
    if (mmd->m != md->m)
    {
        mob_warp(md, mmd->m, mmd->x, mmd->y, BeingRemoveType::WARP);
        md->state.master_check = 1;
        return 0;
    }

    // Distance with between slave and master is measured.
    old_dist = md->master_dist;
    md->master_dist = distance(md->x, md->y, mmd->x, mmd->y);

    // Since the master was in near immediately before, teleport is carried out and it pursues.
    if (old_dist < 10 && md->master_dist > 18)
    {
        mob_warp(md, -1, mmd->x, mmd->y, BeingRemoveType::WARP);
        md->state.master_check = 1;
        return 0;
    }

    // Although there is the master, since it is somewhat far, it approaches.
    if ((!md->target_id || md->state.targettype == NONE_ATTACKABLE)
        && mob_can_move(md)
        && (md->walkpath.path_pos >= md->walkpath.path_len
            || md->walkpath.path_len == 0) && md->master_dist < 15)
    {
        int i = 0, dx, dy, ret;
        if (md->master_dist > 4)
        {
            do
            {
                if (i <= 5)
                {
                    dx = mmd->x - md->x;
                    dy = mmd->y - md->y;
                    if (dx < 0)
                        dx += (MPRAND(1, ((dx < -3) ? 3 : -dx)));
                    else if (dx > 0)
                        dx -= (MPRAND(1, ((dx > 3) ? 3 : dx)));
                    if (dy < 0)
                        dy += (MPRAND(1, ((dy < -3) ? 3 : -dy)));
                    else if (dy > 0)
                        dy -= (MPRAND(1, ((dy > 3) ? 3 : dy)));
                }
                else
                {
                    dx = mmd->x - md->x + MRAND(7) - 3;
                    dy = mmd->y - md->y + MRAND(7) - 3;
                }

                ret = mob_walktoxy(md, md->x + dx, md->y + dy, 0);
                i++;
            }
            while (ret && i < 10);
        }
        else
        {
            do
            {
                dx = MRAND(9) - 5;
                dy = MRAND(9) - 5;
                if (dx == 0 && dy == 0)
                {
                    dx = (MRAND(1)) ? 1 : -1;
                    dy = (MRAND(1)) ? 1 : -1;
                }
                dx += mmd->x;
                dy += mmd->y;

                ret = mob_walktoxy(md, mmd->x + dx, mmd->y + dy, 0);
                i++;
            }
            while (ret && i < 10);
        }

        md->next_walktime = tick + 500;
        md->state.master_check = 1;
    }

    // There is the master, the master locks a target and he does not lock.
    if ((mmd->target_id > 0 && mmd->state.targettype == ATTACKABLE)
        && (!md->target_id || md->state.targettype == NONE_ATTACKABLE))
    {
        MapSessionData *sd = map_id2sd(mmd->target_id);
        if (sd && !pc_isdead(sd) && sd->invincible_timer == NULL
            && !pc_isinvisible(sd))
        {

            race = mob_db[md->mob_class].race;
            if (mode & 0x20 ||
                ((!pc_ishiding(sd) && !sd->state.gangsterparadise)
                  || race == 4 || race == 6))
            {                   // 妨害がないか判定

                md->target_id = sd->id;
                md->state.targettype = ATTACKABLE;
                md->min_chase =
                    5 + distance(md->x, md->y, sd->x, sd->y);
                md->state.master_check = 1;
            }
        }
    }

    // There is the master, the master locks a target and he does not lock.
/*      if ( (md->target_id>0 && mmd->state.targettype == ATTACKABLE) && (!mmd->target_id || mmd->state.targettype == NONE_ATTACKABLE) ){
                MapSessionData *sd=map_id2sd(md->target_id);
                if (sd!=NULL && !pc_isdead(sd) && sd->invincible_timer == -1 && !pc_isinvisible(sd)){

                        race=mob_db[mmd->mob_class].race;
                        if (mode&0x20 ||
                                (sd->sc_data[SC_TRICKDEAD].timer == -1 &&
                                (!(sd->status.option&0x06) || race==4 || race==6)
                                ) ){    // It judges whether there is any disturbance.

                                mmd->target_id=sd->id;
                                mmd->state.targettype = ATTACKABLE;
                                mmd->min_chase=5+distance(mmd->x,mmd->y,sd->x,sd->y);
                        }
                }
        }*/

    return 0;
}

/*==========================================
 * A lock of target is stopped and mob moves to a standby state.
 *------------------------------------------
 */
static int mob_unlocktarget(struct mob_data *md, int tick)
{
    nullpo_ret(md);

    md->target_id = 0;
    md->state.targettype = NONE_ATTACKABLE;
    md->next_walktime = tick + MPRAND(3000, 3000);
    return 0;
}

/*==========================================
 * Random walk
 *------------------------------------------
 */
static int mob_randomwalk(struct mob_data *md, int tick)
{
    const int retrycount = 20;
    int speed;

    nullpo_ret(md);

    speed = battle_get_speed(md);
    if (DIFF_TICK(md->next_walktime, tick) < 0)
    {
        int i, x, y, c, d = 12 - md->move_fail_count;
        if (d < 5)
            d = 5;
        for (i = 0; i < retrycount; i++)
        {                       // Search of a movable place
            int r = mt_random();
            x = md->x + r % (d * 2 + 1) - d;
            y = md->y + r / (d * 2 + 1) % (d * 2 + 1) - d;
            if ((c = map_getcell(md->m, x, y)) != 1 && c != 5
                && mob_walktoxy(md, x, y, 1) == 0)
            {
                md->move_fail_count = 0;
                break;
            }
            if (i + 1 >= retrycount)
            {
                md->move_fail_count++;
                if (md->move_fail_count > 1000)
                {
                    map_log("MOB cant move. random spawn %d, mob_class = %d\n",
                            md->id, md->mob_class);
                    md->move_fail_count = 0;
                    mob_spawn(md->id);
                }
            }
        }
        for (i = c = 0; i < md->walkpath.path_len; i++)
        {                       // The next walk start time is calculated.
            if (static_cast<int>(md->walkpath.path[i]) & 1)
                c += speed * 14 / 10;
            else
                c += speed;
        }
        md->next_walktime = tick + MPRAND(3000, 3000) + c;
        return 1;
    }
    return 0;
}

/*==========================================
 * AI of MOB whose is near a Player
 *------------------------------------------
 */
static void mob_ai_sub_hard(BlockList *bl, tick_t tick)
{
    struct mob_data *md, *tmd = NULL;
    MapSessionData *tsd = NULL;
    BlockList *tbl = NULL;
    struct flooritem_data *fitem;
    int i, dx, dy, ret, dist;
    int attack_type = 0;
    int mode, race;

    nullpo_retv(bl);
    nullpo_retv(md = static_cast<struct mob_data *>(bl));

    if (DIFF_TICK(tick, md->last_thinktime) < MIN_MOBTHINKTIME)
        return;
    md->last_thinktime = tick;

    if (md->prev == NULL)
    {                           // Under a skill aria and death
        if (DIFF_TICK(tick, md->next_walktime) > MIN_MOBTHINKTIME)
            md->next_walktime = tick;
        return;
    }

    if (!md->mode)
        mode = mob_db[md->mob_class].mode;
    else
        mode = md->mode;

    race = mob_db[md->mob_class].race;

    // Abnormalities
    if ((md->opt1 > 0 && md->opt1 != 6) || md->state.state == MS_DELAY)
        return;

    if (!(mode & 0x80) && md->target_id > 0)
        md->target_id = 0;

    if (md->attacked_id > 0 && mode & 0x08)
    {                           // Link monster
        MapSessionData *asd = map_id2sd(md->attacked_id);
        if (asd)
        {
            if (asd->invincible_timer == NULL && !pc_isinvisible(asd))
            {
                map_foreachinarea(mob_ai_sub_hard_linksearch, md->m,
                                  md->x - 13, md->y - 13,
                                  md->x + 13, md->y + 13,
                                  BL_MOB, md, asd);
            }
        }
    }

    // It checks to see it was attacked first (if active, it is target change at 25% of probability).
    if (mode > 0 && md->attacked_id > 0
        && (!md->target_id || md->state.targettype == NONE_ATTACKABLE
            || (mode & 0x04 && MRAND(100) < 25)))
    {
        BlockList *abl = map_id2bl(md->attacked_id);
        MapSessionData *asd = NULL;
        if (abl)
        {
            if (abl->type == BL_PC)
                asd = static_cast<MapSessionData *>(abl);
            if (asd == NULL || md->m != abl->m || abl->prev == NULL
                || asd->invincible_timer || pc_isinvisible(asd)
                || (dist =
                    distance(md->x, md->y, abl->x, abl->y)) >= 32
                || battle_check_target(bl, abl) == 0)
                md->attacked_id = 0;
            else
            {
                md->target_id = md->attacked_id;    // set target
                md->state.targettype = ATTACKABLE;
                attack_type = 1;
                md->attacked_id = 0;
                md->min_chase = dist + 13;
                if (md->min_chase > 26)
                    md->min_chase = 26;
            }
        }
    }

    md->state.master_check = 0;
    // Processing of slave monster
    if (md->master_id > 0 && md->state.special_mob_ai == 0)
        mob_ai_sub_hard_slavemob(md, tick);

    // アクティヴモンスターの策敵 (?? of a bitter taste TIVU monster)
    if ((!md->target_id || md->state.targettype == NONE_ATTACKABLE)
        && mode & 0x04 && !md->state.master_check
        && battle_config.monster_active_enable == 1)
    {
        i = 0;
        if (md->state.special_mob_ai)
        {
            map_foreachinarea(mob_ai_sub_hard_activesearch, md->m,
                              md->x - AREA_SIZE * 2,
                              md->y - AREA_SIZE * 2,
                              md->x + AREA_SIZE * 2,
                              md->y + AREA_SIZE * 2, BL_NUL, md, &i);
        }
        else
        {
            map_foreachinarea(mob_ai_sub_hard_activesearch, md->m,
                              md->x - AREA_SIZE * 2,
                              md->y - AREA_SIZE * 2,
                              md->x + AREA_SIZE * 2,
                              md->y + AREA_SIZE * 2, BL_PC, md, &i);
        }
    }

    // The item search of a route monster
    if (!md->target_id && mode & 0x02 && !md->state.master_check)
    {
        i = 0;
        map_foreachinarea(mob_ai_sub_hard_lootsearch, md->m,
                          md->x - AREA_SIZE * 2, md->y - AREA_SIZE * 2,
                          md->x + AREA_SIZE * 2, md->y + AREA_SIZE * 2,
                          BL_ITEM, md, &i);
    }

    // It will attack, if the candidate for an attack is.
    if (md->target_id > 0)
    {
        if ((tbl = map_id2bl(md->target_id)))
        {
            if (tbl->type == BL_PC)
                tsd = static_cast<MapSessionData *>(tbl);
            else if (tbl->type == BL_MOB)
                tmd = static_cast<struct mob_data *>(tbl);
            if (tsd || tmd)
            {
                if (tbl->m != md->m || tbl->prev == NULL
                    || (dist =
                        distance(md->x, md->y, tbl->x,
                                  tbl->y)) >= md->min_chase)
                    mob_unlocktarget(md, tick);    // 別マップか、視界外
                else if (tsd && !(mode & 0x20)
                         && ((pc_ishiding(tsd)
                               || tsd->state.gangsterparadise) && race != 4
                              && race != 6))
                    mob_unlocktarget(md, tick);    // スキルなどによる策敵妨害
                else if (!battle_check_range
                         (md, tbl, mob_db[md->mob_class].range))
                {
                    // 攻撃範囲外なので移動
                    if (!(mode & 1))
                    {           // 移動しないモード
                        mob_unlocktarget(md, tick);
                        return;
                    }
                    if (!mob_can_move(md)) // 動けない状態にある
                        return;
                    if (md->timer
                        && md->state.state != MS_ATTACK
                        && (DIFF_TICK(md->next_walktime, tick) < 0
                            || distance(md->to_x, md->to_y, tbl->x, tbl->y) < 2))
                        return;   // 既に移動中
                    if (!mob_can_reach
                        (md, tbl, (md->min_chase > 13) ? md->min_chase : 13))
                        mob_unlocktarget(md, tick);    // 移動できないのでタゲ解除（IWとか？）
                    else
                    {
                        // 追跡
                        md->next_walktime = tick + 500;
                        i = 0;
                        do
                        {
                            if (i == 0)
                            {   // 最初はAEGISと同じ方法で検索
                                dx = tbl->x - md->x;
                                dy = tbl->y - md->y;
                                if (dx < 0)
                                    dx++;
                                else if (dx > 0)
                                    dx--;
                                if (dy < 0)
                                    dy++;
                                else if (dy > 0)
                                    dy--;
                            }
                            else
                            {   // だめならAthena式(ランダム)
                                dx = tbl->x - md->x + MRAND(3) - 1;
                                dy = tbl->y - md->y + MRAND(3) - 1;
                            }
                            /*                      if (path_search(&md->walkpath,md->m,md->x,md->y,md->x+dx,md->y+dy,0)){
                             * dx=tsd->x - md->x;
                             * dy=tsd->y - md->y;
                             * if (dx<0) dx--;
                             * else if (dx>0) dx++;
                             * if (dy<0) dy--;
                             * else if (dy>0) dy++;
                             * } */
                            ret =
                                mob_walktoxy(md, md->x + dx,
                                              md->y + dy, 0);
                            i++;
                        }
                        while (ret && i < 5);

                        if (ret)
                        {       // 移動不可能な所からの攻撃なら2歩下る
                            if (dx < 0)
                                dx = 2;
                            else if (dx > 0)
                                dx = -2;
                            if (dy < 0)
                                dy = 2;
                            else if (dy > 0)
                                dy = -2;
                            mob_walktoxy(md, md->x + dx, md->y + dy,
                                          0);
                        }
                    }
                }
                else
                {               // 攻撃射程範囲内
                    if (md->state.state == MS_WALK)
                        mob_stop_walking(md, 1);   // 歩行中なら停止
                    if (md->state.state == MS_ATTACK)
                        return;   // 既に攻撃中
                    mob_changestate(md, MS_ATTACK, attack_type);

/*                                      if (mode&0x08){ // リンクモンスター
                                        map_foreachinarea(mob_ai_sub_hard_linksearch,md->m,
                                                md->x-13,md->y-13,
                                                md->x+13,md->y+13,
                                                        BL_MOB,md,tsd);
                                }*/
                }
                return;
            }
            else
            {                   // ルートモンスター処理
                if (tbl == NULL || tbl->type != BL_ITEM || tbl->m != md->m
                    || (dist =
                        distance(md->x, md->y, tbl->x,
                                  tbl->y)) >= md->min_chase || !md->lootitem)
                {
                    // 遠すぎるかアイテムがなくなった
                    mob_unlocktarget(md, tick);
                    if (md->state.state == MS_WALK)
                        mob_stop_walking(md, 1);   // 歩行中なら停止
                }
                else if (dist)
                {
                    if (!(mode & 1))
                    {           // 移動しないモード
                        mob_unlocktarget(md, tick);
                        return;
                    }
                    if (!mob_can_move(md)) // 動けない状態にある
                        return;

                    if (md->timer
                        && md->state.state != MS_ATTACK
                        && (DIFF_TICK(md->next_walktime, tick) < 0
                            || distance(md->to_x, md->to_y, tbl->x,
                                         tbl->y) <= 0))
                        return;   // 既に移動中
                    md->next_walktime = tick + 500;
                    dx = tbl->x - md->x;
                    dy = tbl->y - md->y;
/*                              if (path_search(&md->walkpath,md->m,md->x,md->y,md->x+dx,md->y+dy,0)){
                                                dx=tbl->x - md->x;
                                                dy=tbl->y - md->y;
                                }*/
                    ret = mob_walktoxy(md, md->x + dx, md->y + dy, 0);
                    if (ret)
                        mob_unlocktarget(md, tick);    // 移動できないのでタゲ解除（IWとか？）
                }
                else
                {               // アイテムまでたどり着いた
                    if (md->state.state == MS_ATTACK)
                        return;   // 攻撃中
                    if (md->state.state == MS_WALK)
                        mob_stop_walking(md, 1);   // 歩行中なら停止
                    fitem = static_cast<struct flooritem_data *>(tbl);
                    if (md->lootitem_count < LOOTITEM_SIZE)
                        memcpy(&md->lootitem[md->lootitem_count++],
                                &fitem->item_data, sizeof(md->lootitem[0]));
                    else if (battle_config.monster_loot_type == 1
                             && md->lootitem_count >= LOOTITEM_SIZE)
                    {
                        mob_unlocktarget(md, tick);
                        return;
                    }
                    else
                    {
                        for (i = 0; i < LOOTITEM_SIZE - 1; i++)
                            memcpy(&md->lootitem[i], &md->lootitem[i + 1],
                                    sizeof(md->lootitem[0]));
                        memcpy(&md->lootitem[LOOTITEM_SIZE - 1],
                                &fitem->item_data, sizeof(md->lootitem[0]));
                    }
                    map_clearflooritem(tbl->id);
                    mob_unlocktarget(md, tick);
                }
                return;
            }
        }
        else
        {
            mob_unlocktarget(md, tick);
            if (md->state.state == MS_WALK)
                mob_stop_walking(md, 4);   // 歩行中なら停止
            return;
        }
    }

    // 歩行処理
    if (mode & 1 && mob_can_move(md) &&    // 移動可能MOB&動ける状態にある
        (md->master_id == 0 || md->state.special_mob_ai
         || md->master_dist > 10))
    {                           //取り巻きMOBじゃない

        if (DIFF_TICK(md->next_walktime, tick) > +7000 &&
            (md->walkpath.path_len == 0
             || md->walkpath.path_pos >= md->walkpath.path_len))
        {
            md->next_walktime = tick + 3000 * MRAND(2000);
        }

        // Random movement
        if (mob_randomwalk(md, tick))
            return;
    }
}

/*==========================================
 * Serious processing for mob in PC field of view(foreachclient)
 *------------------------------------------
 */
static void mob_ai_sub_foreachclient(MapSessionData *sd, tick_t tick)
{
    nullpo_retv(sd);

    map_foreachinarea(mob_ai_sub_hard, sd->m,
                      sd->x - AREA_SIZE * 2, sd->y - AREA_SIZE * 2,
                      sd->x + AREA_SIZE * 2, sd->y + AREA_SIZE * 2,
                      BL_MOB, tick);
}

/*==========================================
 * Serious processing for mob in PC field of view   (interval timer function)
 *------------------------------------------
 */
static void mob_ai_hard(timer_id, tick_t tick)
{
    for (MapSessionData *sd : auth_sessions)
        mob_ai_sub_foreachclient(sd, tick);
}

/*==========================================
 * Negligent mode MOB AI(PC is not in near)
 *------------------------------------------
 */
static void mob_ai_sub_lazy(db_key_t, db_val_t data, tick_t tick)
{
    struct mob_data *md = static_cast<struct mob_data *>(data.p);

    nullpo_retv(md);

    if (md == NULL)
        return;

    if (!md->type || md->type != BL_MOB)
        return;

    if (DIFF_TICK(tick, md->last_thinktime) < MIN_MOBTHINKTIME * 10)
        return;
    md->last_thinktime = tick;

    if (md->prev == NULL)
    {
        if (DIFF_TICK(tick, md->next_walktime) > MIN_MOBTHINKTIME * 10)
            md->next_walktime = tick;
        return;
    }

    if (DIFF_TICK(md->next_walktime, tick) < 0 &&
        (mob_db[md->mob_class].mode & 1) && mob_can_move(md))
    {

        if (maps[md->m].users > 0)
        {
            // Since PC is in the same map, somewhat better negligent processing is carried out.

            // It sometimes moves.
            if (MRAND(1000) < MOB_LAZYMOVEPERC)
                mob_randomwalk(md, tick);

            // MOB which is not not the summons MOB but BOSS, either sometimes reboils.
            else if (MRAND(1000) < MOB_LAZYWARPPERC && md->x_0 <= 0
                     && md->master_id != 0
                     && !(mob_db[md->mob_class].mode & 0x20))
                mob_spawn(md->id);

        }
        else
        {
            // Since PC is not even in the same map, suitable processing is carried out even if it takes.

            // MOB which is not BOSS which is not Summons MOB, either -- a case -- sometimes -- leaping
            if (MRAND(1000) < MOB_LAZYWARPPERC && md->x_0 <= 0
                && md->master_id != 0
                && !(mob_db[md->mob_class].mode & 0x20))
                mob_warp(md, -1, -1, -1, BeingRemoveType::NEGATIVE);
        }

        md->next_walktime = tick + MPRAND(5000, 10000);
    }
}

/*==========================================
 * Negligent processing for mob outside PC field of view   (interval timer function)
 *------------------------------------------
 */
static void mob_ai_lazy(timer_id, tick_t tick)
{
    map_foreachiddb(std::bind(mob_ai_sub_lazy,
                              std::placeholders::_1,
                              std::placeholders::_2,
                              tick));
}

/*==========================================
 * The structure object for item drop with delay
 * Since it is only two being able to pass [ int ] a timer function
 * Data is put in and passed to this structure object.
 *------------------------------------------
 */
struct delay_item_drop
{
    int m, x, y;
    int nameid, amount;
    MapSessionData *first_sd, *second_sd, *third_sd;
};

struct delay_item_drop2
{
    int m, x, y;
    struct item item_data;
    MapSessionData *first_sd, *second_sd, *third_sd;
};

/*==========================================
 * item drop with delay(timer function)
 *------------------------------------------
 */
static void mob_delay_item_drop(timer_id, tick_t, struct delay_item_drop *ditem)
{
    struct item temp_item;
    int flag;

    nullpo_retv(ditem);

    memset(&temp_item, 0, sizeof(temp_item));
    temp_item.nameid = ditem->nameid;
    temp_item.amount = ditem->amount;
    temp_item.identify = !itemdb_isequip3(temp_item.nameid);

    if (battle_config.item_auto_get == 1)
    {
        if (ditem->first_sd
            && (flag =
                pc_additem(ditem->first_sd, &temp_item, ditem->amount)))
        {
            clif_additem(ditem->first_sd, 0, 0, flag);
            map_addflooritem(&temp_item, 1, ditem->m, ditem->x, ditem->y,
                              ditem->first_sd, ditem->second_sd,
                              ditem->third_sd);
        }
        free(ditem);
        return;
    }

    map_addflooritem(&temp_item, 1, ditem->m, ditem->x, ditem->y,
                      ditem->first_sd, ditem->second_sd, ditem->third_sd);

    free(ditem);
}

/*==========================================
 * item drop(timer function)-lootitem with delay
 *------------------------------------------
 */
static void mob_delay_item_drop2(timer_id, tick_t, struct delay_item_drop2 *ditem)
{
    int flag;

    nullpo_retv(ditem);

    if (battle_config.item_auto_get == 1)
    {
        if (ditem->first_sd
            && (flag =
                pc_additem(ditem->first_sd, &ditem->item_data,
                            ditem->item_data.amount)))
        {
            clif_additem(ditem->first_sd, 0, 0, flag);
            map_addflooritem(&ditem->item_data, ditem->item_data.amount,
                              ditem->m, ditem->x, ditem->y, ditem->first_sd,
                              ditem->second_sd, ditem->third_sd);
        }
        free(ditem);
        return;
    }

    map_addflooritem(&ditem->item_data, ditem->item_data.amount, ditem->m,
                      ditem->x, ditem->y, ditem->first_sd, ditem->second_sd,
                      ditem->third_sd);

    free(ditem);
}

/*==========================================
 * mob data is erased.
 *------------------------------------------
 */
int mob_delete(struct mob_data *md)
{
    nullpo_retr(1, md);

    if (md->prev == NULL)
        return 1;
    mob_changestate(md, MS_DEAD, 0);
    clif_being_remove(md, BeingRemoveType::DEAD);
    map_delblock(md);
    mob_deleteslave(md);
    mob_setdelayspawn(md->id);
    return 0;
}

int mob_catch_delete(struct mob_data *md)
{
    nullpo_retr(1, md);

    if (md->prev == NULL)
        return 1;
    mob_changestate(md, MS_DEAD, 0);
    clif_being_remove(md, BeingRemoveType::WARP);
    map_delblock(md);
    mob_setdelayspawn(md->id);
    return 0;
}

/// Timer for a summoned mob to expire
void mob_timer_delete(timer_id, tick_t, int id)
{
    BlockList *bl = map_id2bl(id);
    struct mob_data *md;

    nullpo_retv(bl);

    md = static_cast<struct mob_data *>(bl);
    mob_catch_delete(md);
}

/*==========================================
 *
 *------------------------------------------
 */
static void mob_deleteslave_sub(BlockList *bl, uint32_t id)
{
    nullpo_retv(bl);
    struct mob_data *md = static_cast<struct mob_data *>(bl);

    if (md->master_id > 0 && md->master_id == id)
        mob_damage(NULL, md, md->hp, 1);
}

/*==========================================
 *
 *------------------------------------------
 */
int mob_deleteslave(struct mob_data *md)
{
    nullpo_ret(md);

    map_foreachinarea(mob_deleteslave_sub, md->m,
                      0, 0, maps[md->m].xs, maps[md->m].ys,
                      BL_MOB, md->id);
    return 0;
}

#define DAMAGE_BONUS_COUNT 6    // max. number of players to account for
static const double damage_bonus_factor[DAMAGE_BONUS_COUNT + 1] = {
    1.0, 1.0, 2.0, 2.5, 2.75, 2.9, 3.0
};

/*==========================================
 * It is the damage of sd to damage to md.
 *------------------------------------------
 */
int mob_damage(BlockList *src, struct mob_data *md, int damage,
                int type)
{
    int minpos, mindmg;
    MapSessionData *sd = NULL, *tmpsd[DAMAGELOG_SIZE];
    struct
    {
        struct party *p;
        int id, base_exp, job_exp;
    } pt[DAMAGELOG_SIZE];
    int pnum = 0;
    int max_hp;
    unsigned int tick = gettick();
    MapSessionData *mvp_sd = NULL, *second_sd = NULL, *third_sd =
        NULL;
    double tdmg;

    nullpo_ret(md);        //srcはNULLで呼ばれる場合もあるので、他でチェック

    if (src && src->id == md->master_id
        && md->mode & MOB_MODE_TURNS_AGAINST_BAD_MASTER)
    {
        /* If the master hits a monster, have the monster turn against him */
        md->master_id = 0;
        md->mode = 0x85;        /* Regular war mode */
        md->target_id = src->id;
        md->attacked_id = src->id;
    }

    max_hp = battle_get_max_hp(md);

    if (src && src->type == BL_PC)
    {
        sd = static_cast<MapSessionData *>(src);
        mvp_sd = sd;
    }

//  if (battle_config.battle_log)
//      printf("mob_damage %d %d %d\n",md->hp,max_hp,damage);
    if (md->prev == NULL)
    {
        map_log("mob_damap_logmage : BlockError!!\n");
        return 0;
    }

    if (md->state.state == MS_DEAD || md->hp <= 0)
    {
        if (md->prev != NULL)
        {
            mob_changestate(md, MS_DEAD, 0);
            clif_being_remove(md, BeingRemoveType::DEAD);
            map_delblock(md);
            mob_setdelayspawn(md->id);
        }
        return 0;
    }

    mob_stop_walking(md, 3);

    if (md->hp > max_hp)
        md->hp = max_hp;

    // The amount of overkill rounds to hp.
    if (damage > md->hp)
        damage = md->hp;

    if (!(type & 2))
    {
        if (sd != NULL)
        {
            int i;
            for (i = 0, minpos = 0, mindmg = 0x7fffffff; i < DAMAGELOG_SIZE;
                 i++)
            {
                if (md->dmglog[i].id == sd->id)
                    break;
                if (md->dmglog[i].id == 0)
                {
                    minpos = i;
                    mindmg = 0;
                }
                else if (md->dmglog[i].dmg < mindmg)
                {
                    minpos = i;
                    mindmg = md->dmglog[i].dmg;
                }
            }
            if (i < DAMAGELOG_SIZE)
                md->dmglog[i].dmg += damage;
            else
            {
                md->dmglog[minpos].id = sd->id;
                md->dmglog[minpos].dmg = damage;
            }

            if (md->attacked_id <= 0 && md->state.special_mob_ai == 0)
                md->attacked_id = sd->id;
        }
        if (src && src->type == BL_MOB
            && static_cast<struct mob_data *>(src)->state.special_mob_ai)
        {
            struct mob_data *md2 = static_cast<struct mob_data *>(src);
            BlockList *master_bl = map_id2bl(md2->master_id);
            if (master_bl && master_bl->type == BL_PC)
            {
                MAP_LOG_PC(static_cast<MapSessionData *>(master_bl),
                            "MOB-TO-MOB-DMG FROM MOB%d %d TO MOB%d %d FOR %d",
                            md2->id, md2->mob_class, md->id, md->mob_class,
                            damage);
            }

            nullpo_ret(md2);
            int i;
            for (i = 0, minpos = 0, mindmg = 0x7fffffff; i < DAMAGELOG_SIZE;
                 i++)
            {
                if (md->dmglog[i].id == md2->master_id)
                    break;
                if (md->dmglog[i].id == 0)
                {
                    minpos = i;
                    mindmg = 0;
                }
                else if (md->dmglog[i].dmg < mindmg)
                {
                    minpos = i;
                    mindmg = md->dmglog[i].dmg;
                }
            }
            if (i < DAMAGELOG_SIZE)
                md->dmglog[i].dmg += damage;
            else
            {
                md->dmglog[minpos].id = md2->master_id;
                md->dmglog[minpos].dmg = damage;

                if (md->attacked_id <= 0 && md->state.special_mob_ai == 0)
                    md->attacked_id = md2->master_id;
            }
        }

    }

    md->hp -= damage;

    if (md->hp > 0)
    {
        return 0;
    }

    map_log("MOB%d DEAD", md->id);

    // ----- ここから死亡処理 -----

    map_freeblock_lock();
    mob_changestate(md, MS_DEAD, 0);

    memset(tmpsd, 0, sizeof(tmpsd));
    memset(pt, 0, sizeof(pt));

    max_hp = battle_get_max_hp(md);

    if (src && src->type == BL_MOB)
        mob_unlocktarget(static_cast<struct mob_data *>(src), tick);

    // map外に消えた人は計算から除くので
    // overkill分は無いけどsumはmax_hpとは違う

    tdmg = 0;
    int count = 0;
    for (int i = 0, mvp_damage = 0; i < DAMAGELOG_SIZE; i++)
    {
        if (md->dmglog[i].id == 0)
            continue;
        tmpsd[i] = map_id2sd(md->dmglog[i].id);
        if (tmpsd[i] == NULL)
            continue;
        count++;
        if (tmpsd[i]->m != md->m || pc_isdead(tmpsd[i]))
            continue;

        tdmg += md->dmglog[i].dmg;
        if (mvp_damage < md->dmglog[i].dmg)
        {
            third_sd = second_sd;
            second_sd = mvp_sd;
            mvp_sd = tmpsd[i];
            mvp_damage = md->dmglog[i].dmg;
        }
    }

    // [MouseJstr]
    if ((maps[md->m].flag.pvp == 0) || (battle_config.pvp_exp == 1))
    {
        // 経験値の分配
        for (int i = 0; i < DAMAGELOG_SIZE; i++)
        {

            int pid, base_exp, job_exp, flag = 1;
            double per;
            struct party *p;
            if (tmpsd[i] == NULL || tmpsd[i]->m != md->m)
                continue;
/* jAthena's exp formula
                per = ((double)md->dmglog[i].dmg)*(9.+(double)((count > 6)? 6:count))/10./((double)max_hp) * dmg_rate;
                temp = ((double)mob_db[md->mob_class].base_exp * (double)battle_config.base_exp_rate / 100. * per);
                base_exp = (temp > 2147483647.)? 0x7fffffff:(int)temp;
                if (mob_db[md->mob_class].base_exp > 0 && base_exp < 1) base_exp = 1;
                if (base_exp < 0) base_exp = 0;
                temp = ((double)mob_db[md->mob_class].job_exp * (double)battle_config.job_exp_rate / 100. * per);
                job_exp = (temp > 2147483647.)? 0x7fffffff:(int)temp;
                if (mob_db[md->mob_class].job_exp > 0 && job_exp < 1) job_exp = 1;
                if (job_exp < 0) job_exp = 0;
*/
//eAthena's exp formula rather than jAthena's
//      per=(double)md->dmglog[i].dmg*256*(9+(double)((count > 6)? 6:count))/10/(double)max_hp;
            // [Fate] The above is the old formula.  We do a more involved computation below.
            per = md->dmglog[i].dmg * 256.0/ max_hp;   // 256 = 100% of the score
            per *= damage_bonus_factor[count > DAMAGE_BONUS_COUNT ? DAMAGE_BONUS_COUNT : count];    // Bonus for party attack
            if (per > 512)
                per = 512;      // [Fate] Retained from before.  The maximum a single individual can get is double the original value.
            if (per < 1)
                per = 1;

            base_exp =
                ((mob_db[md->mob_class].base_exp *
                  md->stats[MOB_XP_BONUS]) >> MOB_XP_BONUS_SHIFT) * per / 256;
            if (base_exp < 1)
                base_exp = 1;
            if (sd && md && battle_config.pk_mode == 1
                && (mob_db[md->mob_class].lv - sd->status.base_level >= 20))
            {
                base_exp *= 1.15;   // pk_mode additional exp if monster >20 levels [Valaris]
            }
            if (md->state.special_mob_ai >= 1
                && battle_config.alchemist_summon_reward != 1)
                base_exp = 0;   // Added [Valaris]
            job_exp = mob_db[md->mob_class].job_exp * per / 256;
            if (job_exp < 1)
                job_exp = 1;
            if (sd && md && battle_config.pk_mode == 1
                && (mob_db[md->mob_class].lv - sd->status.base_level >= 20))
            {
                job_exp *= 1.15;    // pk_mode additional exp if monster >20 levels [Valaris]
            }
            if (md->state.special_mob_ai >= 1
                && battle_config.alchemist_summon_reward != 1)
                job_exp = 0;    // Added [Valaris]

            if ((pid = tmpsd[i]->status.party_id) > 0)
            {                   // パーティに入っている
                int j = 0;
                for (j = 0; j < pnum; j++)  // 公平パーティリストにいるかどうか
                    if (pt[j].id == pid)
                        break;
                if (j == pnum)
                {               // いないときは公平かどうか確認
                    if ((p = party_search(pid)) != NULL && p->exp != 0)
                    {
                        pt[pnum].id = pid;
                        pt[pnum].p = p;
                        pt[pnum].base_exp = base_exp;
                        pt[pnum].job_exp = job_exp;
                        pnum++;
                        flag = 0;
                    }
                }
                else
                {               // いるときは公平
                    pt[j].base_exp += base_exp;
                    pt[j].job_exp += job_exp;
                    flag = 0;
                }
            }
            if (flag)           // 各自所得
                pc_gainexp(tmpsd[i], base_exp, job_exp);
        }
        // 公平分配
        for (int i = 0; i < pnum; i++)
            party_exp_share(pt[i].p, md->m, pt[i].base_exp,
                             pt[i].job_exp);

        // item drop
        if (!(type & 1))
        {
            for (int i = 0; i < 8; i++)
            {
                struct delay_item_drop *ditem;
                int drop_rate;

                if (md->state.special_mob_ai >= 1 && battle_config.alchemist_summon_reward != 1)    // Added [Valaris]
                    break;      // End

                if (mob_db[md->mob_class].dropitem[i].nameid <= 0)
                    continue;
                drop_rate = mob_db[md->mob_class].dropitem[i].p;
                if (drop_rate <= 0 && battle_config.drop_rate0item == 1)
                    drop_rate = 1;
                if (battle_config.drops_by_luk > 0 && sd && md)
                    drop_rate += (sd->status.luk * battle_config.drops_by_luk) / 100;   // drops affected by luk [Valaris]
                if (sd && md && battle_config.pk_mode == 1
                    && (mob_db[md->mob_class].lv - sd->status.base_level >= 20))
                    drop_rate *= 1.25;  // pk_mode increase drops if 20 level difference [Valaris]
                if (drop_rate <= MRAND(10000))
                    continue;

                CREATE(ditem, struct delay_item_drop, 1);
                ditem->nameid = mob_db[md->mob_class].dropitem[i].nameid;
                ditem->amount = 1;
                ditem->m = md->m;
                ditem->x = md->x;
                ditem->y = md->y;
                ditem->first_sd = mvp_sd;
                ditem->second_sd = second_sd;
                ditem->third_sd = third_sd;
                add_timer(tick + 500 + i, mob_delay_item_drop, ditem);
            }
            if (sd && sd->state.attack_type == BF_WEAPON)
            {
                if (sd->get_zeny_num > 0)
                    pc_getzeny(sd,
                                mob_db[md->mob_class].lv * 10 +
                                MRAND((sd->get_zeny_num + 1)));
            }
            if (md->lootitem)
            {
                for (int i = 0; i < md->lootitem_count; i++)
                {
                    struct delay_item_drop2 *ditem;

                    CREATE(ditem, struct delay_item_drop2, 1);
                    memcpy(&ditem->item_data, &md->lootitem[i],
                            sizeof(md->lootitem[0]));
                    ditem->m = md->m;
                    ditem->x = md->x;
                    ditem->y = md->y;
                    ditem->first_sd = mvp_sd;
                    ditem->second_sd = second_sd;
                    ditem->third_sd = third_sd;
                    add_timer(tick + 540 + i, mob_delay_item_drop2, ditem);
                }
            }
        }
    }                           // [MouseJstr]

    // SCRIPT実行
    if (md->npc_event[0])
    {
        if (sd == NULL)
        {
            if (mvp_sd != NULL)
                sd = mvp_sd;
            else
            {
                for (MapSessionData *tmp_sd : auth_sessions)
                {
                    if (md->m == tmp_sd->m)
                    {
                        sd = tmp_sd;
                        break;
                    }
                }
            }
        }
        if (sd)
            npc_event(sd, md->npc_event, 0);
    }

    clif_being_remove(md, BeingRemoveType::DEAD);
    map_delblock(md);
    mob_deleteslave(md);
    mob_setdelayspawn(md->id);
    map_freeblock_unlock();

    return 0;
}

/*==========================================
 * mob回復
 *------------------------------------------
 */
int mob_heal(struct mob_data *md, int heal)
{
    int max_hp = battle_get_max_hp(md);

    nullpo_ret(md);

    md->hp += heal;
    if (max_hp < md->hp)
        md->hp = max_hp;

    return 0;
}

/*==========================================
 * Added by RoVeRT
 *------------------------------------------
 */
static void mob_warpslave_sub(BlockList *bl, uint32_t id, uint16_t x, uint16_t y)
{
    struct mob_data *md = static_cast<struct mob_data *>(bl);
    if (md->master_id == id)
    {
        mob_warp(md, -1, x, y, BeingRemoveType::QUIT);
    }
}

/*==========================================
 * Added by RoVeRT
 *------------------------------------------
 */
static int mob_warpslave(struct mob_data *md, int x, int y)
{
//printf("warp slave\n");
    map_foreachinarea(mob_warpslave_sub, md->m,
                      x - AREA_SIZE, y - AREA_SIZE,
                      x + AREA_SIZE, y + AREA_SIZE, BL_MOB,
                      md->id, md->x, md->y);
    return 0;
}

/*==========================================
 * mobワープ
 *------------------------------------------
 */
int mob_warp(struct mob_data *md, int m, int x, int y, BeingRemoveType type)
{
    int i = 0, c, xs = 0, ys = 0, bx = x, by = y;

    nullpo_ret(md);

    if (md->prev == NULL)
        return 0;

    if (m < 0)
        m = md->m;

    if (type != BeingRemoveType::NEGATIVE)
    {
        if (maps[md->m].flag.monster_noteleport)
            return 0;
        clif_being_remove(md, type);
    }
    map_delblock(md);

    if (bx > 0 && by > 0)
    {                           // 位置指定の場合周囲９セルを探索
        xs = ys = 9;
    }

    while ((x < 0 || y < 0 || ((c = read_gat(m, x, y)) == 1 || c == 5))
           && (i++) < 1000)
    {
        if (xs > 0 && ys > 0 && i < 250)
        {                       // 指定位置付近の探索
            x = MPRAND(bx, xs) - xs / 2;
            y = MPRAND(by, ys) - ys / 2;
        }
        else
        {                       // 完全ランダム探索
            x = MPRAND(1, (maps[m].xs - 2));
            y = MPRAND(1, (maps[m].ys - 2));
        }
    }
    md->dir = Direction::S;
    if (i < 1000)
    {
        md->x = md->to_x = x;
        md->y = md->to_y = y;
        md->m = m;
    }
    else
    {
        m = md->m;
        map_log("MOB %d warp failed, mob_class = %d\n", md->id, md->mob_class);
    }

    md->target_id = 0;          // タゲを解除する
    md->state.targettype = NONE_ATTACKABLE;
    md->attacked_id = 0;
    mob_changestate(md, MS_IDLE, 0);

    if (type != BeingRemoveType::NEGATIVE && type != BeingRemoveType::ZERO && i == 1000)
    {
        map_log("MOB %d warp to (%d,%d), mob_class = %d\n", md->id, x, y,
                md->mob_class);
    }

    map_addblock(md);
    if (type != BeingRemoveType::NEGATIVE && type != BeingRemoveType::ZERO)
    {
        clif_spawnmob(md);
        mob_warpslave(md, md->x, md->y);
    }

    return 0;
}

/*==========================================
 * 自分をロックしているPCの数を数える(foreachclient)
 *------------------------------------------
 */
static void mob_counttargeted_sub(BlockList *bl, uint32_t id, int *c,
                                  BlockList *src, AttackResult target_lv)
{
    nullpo_retv(bl);

    if (id == bl->id || (src && id == src->id))
        return;
    if (bl->type == BL_PC)
    {
        MapSessionData *sd = static_cast<MapSessionData *>(bl);
        if (sd && sd->attacktarget == id && sd->attacktimer
            && sd->attacktarget_lv >= target_lv)
            (*c)++;
    }
    else if (bl->type == BL_MOB)
    {
        struct mob_data *md = static_cast<struct mob_data *>(bl);
        if (md && md->target_id == id && md->timer
            && md->state.state == MS_ATTACK && md->target_lv >= target_lv)
            (*c)++;
    }
}

/*==========================================
 * 自分をロックしているPCの数を数える
 *------------------------------------------
 */
int mob_counttargeted(struct mob_data *md, BlockList *src,
                      AttackResult target_lv)
{
    int c = 0;

    nullpo_ret(md);

    map_foreachinarea(mob_counttargeted_sub, md->m,
                      md->x - AREA_SIZE, md->y - AREA_SIZE,
                      md->x + AREA_SIZE, md->y + AREA_SIZE, BL_NUL,
                      md->id, &c, src, target_lv);
    return c;
}

//
// 初期化
//
/*==========================================
 * Since un-setting [ mob ] up was used, it is an initial provisional value setup.
 *------------------------------------------
 */
static int mob_makedummymobdb(int mob_class)
{
    int i;

    sprintf(mob_db[mob_class].name, "mob%d", mob_class);
    sprintf(mob_db[mob_class].jname, "mob%d", mob_class);
    mob_db[mob_class].lv = 1;
    mob_db[mob_class].max_hp = 1000;
    mob_db[mob_class].max_sp = 1;
    mob_db[mob_class].base_exp = 2;
    mob_db[mob_class].job_exp = 1;
    mob_db[mob_class].range = 1;
    mob_db[mob_class].atk1 = 7;
    mob_db[mob_class].atk2 = 10;
    mob_db[mob_class].def = 0;
    mob_db[mob_class].mdef = 0;
    mob_db[mob_class].str = 1;
    mob_db[mob_class].agi = 1;
    mob_db[mob_class].vit = 1;
    mob_db[mob_class].int_ = 1;
    mob_db[mob_class].dex = 6;
    mob_db[mob_class].luk = 2;
    mob_db[mob_class].range2 = 10;
    mob_db[mob_class].range3 = 10;
    mob_db[mob_class].size = 0;
    mob_db[mob_class].race = 0;
    mob_db[mob_class].element = 0;
    mob_db[mob_class].mode = 0;
    mob_db[mob_class].speed = 300;
    mob_db[mob_class].adelay = 1000;
    mob_db[mob_class].amotion = 500;
    mob_db[mob_class].dmotion = 500;
    mob_db[mob_class].dropitem[0].nameid = 909; // Jellopy
    mob_db[mob_class].dropitem[0].p = 1000;
    for (i = 1; i < 8; i++)
    {
        mob_db[mob_class].dropitem[i].nameid = 0;
        mob_db[mob_class].dropitem[i].p = 0;
    }
    return 0;
}

/*==========================================
 * db/mob_db.txt reading
 *------------------------------------------
 */
static int mob_readdb(void)
{
    FILE *fp;
    char line[1024];
    const char *filename[] = { "db/mob_db.txt", "db/mob_db2.txt" };

    memset(mob_db, 0, sizeof(mob_db));

    for (int io = 0; io < 2; io++)
    {

        fp = fopen_(filename[io], "r");
        if (fp == NULL)
        {
            if (io > 0)
                continue;
            return -1;
        }
        while (fgets(line, 1020, fp))
        {
            int mob_class;
            char *str[57], *p, *np;

            if (line[0] == '/' && line[1] == '/')
                continue;

            int ii;
            for (ii = 0, p = line; ii < 57; ii++)
            {
                while (*p == '\t' || *p == ' ')
                    p++;
                if ((np = strchr(p, ',')) != NULL)
                {
                    str[ii] = p;
                    *np = 0;
                    p = np + 1;
                }
                else
                    str[ii] = p;
            }

            mob_class = atoi(str[0]);
            if (mob_class <= 1000 || mob_class > 2000)
                continue;

            memcpy(mob_db[mob_class].name, str[1], 24);
            memcpy(mob_db[mob_class].jname, str[2], 24);
            mob_db[mob_class].lv = atoi(str[3]);
            mob_db[mob_class].max_hp = atoi(str[4]);
            mob_db[mob_class].max_sp = atoi(str[5]);

            mob_db[mob_class].base_exp = atoi(str[6]);
            if (mob_db[mob_class].base_exp < 0)
                mob_db[mob_class].base_exp = 0;
            else if (mob_db[mob_class].base_exp > 0
                     && (mob_db[mob_class].base_exp *
                         battle_config.base_exp_rate / 100 > 1000000000
                         || mob_db[mob_class].base_exp *
                         battle_config.base_exp_rate / 100 < 0))
                mob_db[mob_class].base_exp = 1000000000;
            else
                mob_db[mob_class].base_exp *= battle_config.base_exp_rate / 100;

            mob_db[mob_class].job_exp = atoi(str[7]);
            if (mob_db[mob_class].job_exp < 0)
                mob_db[mob_class].job_exp = 0;
            else if (mob_db[mob_class].job_exp > 0
                     && (mob_db[mob_class].job_exp * battle_config.job_exp_rate /
                         100 > 1000000000
                         || mob_db[mob_class].job_exp *
                         battle_config.job_exp_rate / 100 < 0))
                mob_db[mob_class].job_exp = 1000000000;
            else
                mob_db[mob_class].job_exp *= battle_config.job_exp_rate / 100;

            mob_db[mob_class].range = atoi(str[8]);
            mob_db[mob_class].atk1 = atoi(str[9]);
            mob_db[mob_class].atk2 = atoi(str[10]);
            mob_db[mob_class].def = atoi(str[11]);
            mob_db[mob_class].mdef = atoi(str[12]);
            mob_db[mob_class].str = atoi(str[13]);
            mob_db[mob_class].agi = atoi(str[14]);
            mob_db[mob_class].vit = atoi(str[15]);
            mob_db[mob_class].int_ = atoi(str[16]);
            mob_db[mob_class].dex = atoi(str[17]);
            mob_db[mob_class].luk = atoi(str[18]);
            mob_db[mob_class].range2 = atoi(str[19]);
            mob_db[mob_class].range3 = atoi(str[20]);
            mob_db[mob_class].size = atoi(str[21]);
            mob_db[mob_class].race = atoi(str[22]);
            mob_db[mob_class].element = atoi(str[23]);
            mob_db[mob_class].mode = atoi(str[24]);
            mob_db[mob_class].speed = atoi(str[25]);
            mob_db[mob_class].adelay = atoi(str[26]);
            mob_db[mob_class].amotion = atoi(str[27]);
            mob_db[mob_class].dmotion = atoi(str[28]);

            for (ii = 0; ii < 8; ii++)
            {
                int rate = 0, type, ratemin, ratemax;
                mob_db[mob_class].dropitem[ii].nameid = atoi(str[29 + ii * 2]);
                type = itemdb_type(mob_db[mob_class].dropitem[ii].nameid);
                if (type == 0)
                {               // Added [Valaris]
                    rate = battle_config.item_rate_heal;
                    ratemin = battle_config.item_drop_heal_min;
                    ratemax = battle_config.item_drop_heal_max;
                }
                else if (type == 2)
                {
                    rate = battle_config.item_rate_use;
                    ratemin = battle_config.item_drop_use_min;
                    ratemax = battle_config.item_drop_use_max;  // End
                }
                else if (type == 4 || type == 5 || type == 8)
                {
                    rate = battle_config.item_rate_equip;
                    ratemin = battle_config.item_drop_equip_min;
                    ratemax = battle_config.item_drop_equip_max;
                }
                else if (type == 6)
                {
                    rate = battle_config.item_rate_card;
                    ratemin = battle_config.item_drop_card_min;
                    ratemax = battle_config.item_drop_card_max;
                }
                else
                {
                    rate = battle_config.item_rate_common;
                    ratemin = battle_config.item_drop_common_min;
                    ratemax = battle_config.item_drop_common_max;
                }
                rate = (rate / 100) * atoi(str[30 + ii * 2]);
                rate =
                    (rate < ratemin) ? ratemin : (rate >
                                                  ratemax) ? ratemax : rate;
                mob_db[mob_class].dropitem[ii].p = rate;
            }
            // str[45 .. 52] removed (mvp items/experience)
            mob_db[mob_class].mutations_nr = atoi(str[55]);
            mob_db[mob_class].mutation_power = atoi(str[56]);

            if (mob_db[mob_class].base_exp == 0)
                mob_db[mob_class].base_exp = mob_gen_exp(&mob_db[mob_class]);
        }
        fclose_(fp);
        printf("read %s done\n", filename[io]);
    }
    return 0;
}

/*==========================================
 * Circumference initialization of mob
 *------------------------------------------
 */
int do_init_mob(void)
{
    mob_readdb();

    add_timer_interval(gettick() + MIN_MOBTHINKTIME, MIN_MOBTHINKTIME, mob_ai_hard);
    add_timer_interval(gettick() + MIN_MOBTHINKTIME * 10, MIN_MOBTHINKTIME * 10, mob_ai_lazy);

    return 0;
}
