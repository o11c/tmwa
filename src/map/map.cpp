#include "map.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <netdb.h>

#include "../common/core.hpp"
#include "../common/timer.hpp"
#include "../common/db.hpp"
#include "../common/grfio.hpp"
#include "../common/mt_rand.hpp"
#include "chrif.hpp"
#include "clif.hpp"
#include "intif.hpp"
#include "npc.hpp"
#include "pc.hpp"
#include "mob.hpp"
#include "itemdb.hpp"
#include "storage.hpp"
#include "skill.hpp"
#include "trade.hpp"
#include "party.hpp"
#include "battle.hpp"
#include "script.hpp"
#include "atcommand.hpp"
#include "../common/nullpo.hpp"
#include "../common/socket.hpp"
#include "magic.hpp"

static void map_helpscreen(void) __attribute__((noreturn));


static struct dbt *id_db = NULL;
static struct dbt *map_db = NULL;
static struct dbt *nick_db = NULL;
static struct dbt *charid_db = NULL;

static int users = 0;
static struct block_list *object[MAX_FLOORITEM];
obj_id_t first_free_object_id = 0, last_object_id = 0;

#define block_free_max 1048576
static void *block_free[block_free_max];
static int block_free_count = 0, block_free_lock = 0;

#define BL_LIST_MAX 1048576
static struct block_list *bl_list[BL_LIST_MAX];
static int bl_list_count = 0;

struct map_data maps[MAX_MAP_PER_SERVER];
int map_num = 0;

in_port_t map_port = 0;

int autosave_interval = DEFAULT_AUTOSAVE_INTERVAL;

struct charid2nick
{
    char nick[24];
};

char motd_txt[256] = "conf/motd.txt";

// can be modified in char-server configuration file
char wisp_server_name[24] = "Server";

/// Save the number of users reported by the char server
void map_setusers(int n)
{
    users = n;
}

/// Get the number of users previously reported by the char server
int map_getusers(void)
{
    return users;
}



/// Free a block
// prohibits calling free() until block_free_lock is 0
int map_freeblock(void *bl)
{
    if (!block_free_lock)
    {
        free(bl);
        bl = NULL;
        return 0;
    }
    map_log("Adding block %d due to %d locks", block_free_count, block_free_lock);
    if (block_free_count >= block_free_max)
        map_log("%s: MEMORY LEAK: too many free blocks!", __func__);
    else
        block_free[block_free_count++] = bl;
    return block_free_lock;
}

/// Temporarily prohibit freeing blocks.
int map_freeblock_lock(void)
{
    return ++block_free_lock;
}

/// Remove a prohibition on freeing blocks
// if this was the last lock, free the queued ones
int map_freeblock_unlock(void)
{
    if (!block_free_lock)
    {
        map_log("ERROR: Unlocked more times than locked");
        abort();
    }
    --block_free_lock;
    if (block_free_lock)
        return block_free_lock;

    map_log("Freeing %d deferred blocks", block_free_count);
    for (int i = 0; i < block_free_count; i++)
    {
        free(block_free[i]);
        block_free[i] = NULL;
    }
    block_free_count = 0;
    return 0;
}


/// this is a dummy bl->prev so that legitimate blocks always have non-NULL
static struct block_list bl_head;


/// link a new block
bool map_addblock(struct block_list *bl)
{
    nullpo_retr(0, bl);

    if (bl->prev)
    {
        map_log("%s: error: already linked", __func__);
        return 0;
    }

if (bl->m >= map_num || bl->x >= maps[bl->m].xs || bl->y >= maps[bl->m].ys)
    {
        map_log("%s: bad x/y/m: %hu/%hu/%hu", __func__, bl->x, bl->y, bl->m);
        return 1;
    }
    size_t b = bl->x / BLOCK_SIZE + (bl->y / BLOCK_SIZE) * maps[bl->m].bxs;
    if (bl->type == BL_MOB)
    {
        bl->next = maps[bl->m].block_mob[b];
        bl->prev = &bl_head;
        if (bl->next)
            bl->next->prev = bl;
        maps[bl->m].block_mob[b] = bl;
        maps[bl->m].block_mob_count[b]++;
    }
    else
    {
        bl->next = maps[bl->m].block[b];
        bl->prev = &bl_head;
        if (bl->next)
            bl->next->prev = bl;
        maps[bl->m].block[b] = bl;
        maps[bl->m].block_count[b]++;
        if (bl->type == BL_PC)
            maps[bl->m].users++;
    }

    return 0;
}

/// Remove a block from the list
// prev shouldn't be NULL
int map_delblock(struct block_list *bl)
{
    nullpo_retr(0, bl);

    // not in the blocklist
    if (!bl->prev)
    {
        map_log("Removing already unlinked block from list");
        if (bl->next)
            map_log("but it still links to other blocks!");
        return 0;
    }

    if (bl->type == BL_PC)
        maps[bl->m].users--;

    if (bl->next)
        bl->next->prev = bl->prev;
    // if this is the first in the list, need to update the true root blocks
    if (bl->prev == &bl_head)
    {
        size_t b = bl->x / BLOCK_SIZE + (bl->y / BLOCK_SIZE) * maps[bl->m].bxs;
        if (bl->type == BL_MOB)
        {
            maps[bl->m].block_mob[b] = bl->next;
            maps[bl->m].block_mob_count[b]--;
            if (maps[bl->m].block_mob_count[b] < 0)
            {
                map_log("???: Negative mobs on map %d.%d", bl->m, b);
                maps[bl->m].block_mob_count[b] = 0;
            }
        }
        else
        {
            maps[bl->m].block[b] = bl->next;
            maps[bl->m].block_count[b]--;
            if (maps[bl->m].block_count[b] < 0)
            {
                map_log("???: Negative normal blocks on %d.%d", bl->m, b);
                maps[bl->m].block_count[b] = 0;
            }
        }
    }
    else // not head of list
    {
        map_log("???: removing non-head block, possible synchronization problem - does this happen?");
        bl->prev->next = bl->next;
    }
    // make sure we aren't used again
    bl->next = NULL;
    bl->prev = NULL;

    return 0;
}

/// Runs a function for every block in the area
// if type is 0, all types, else BL_MOB, BL_PC, BL_SKILL, etc
void map_foreachinarea(void (*func) (struct block_list *, va_list), int m,
                        int x_0, int y_0, int x_1, int y_1, BlockType type, ...)
{
    if (m < 0)
        return;

    // fix bounds
    if (x_0 < 0)
        x_0 = 0;
    if (y_0 < 0)
        y_0 = 0;
    if (x_1 >= maps[m].xs)
        x_1 = maps[m].xs - 1;
    if (y_1 >= maps[m].ys)
        y_1 = maps[m].ys - 1;

    // save the count from before the changes
    int blockcount = bl_list_count;

    if (type == BL_NUL || type != BL_MOB)
        for (int by = y_0 / BLOCK_SIZE; by <= y_1 / BLOCK_SIZE; by++)
        {
            for (int bx = x_0 / BLOCK_SIZE; bx <= x_1 / BLOCK_SIZE; bx++)
            {
                int b = bx + by * maps[m].bxs;
                struct block_list *bl = maps[m].block[b];
                int c = maps[m].block_count[b];
                for (int i = 0; i < c && bl; i++, bl = bl->next)
                {
                    // if we're not doing every type, only allow the right type
                    if (type != BL_NUL && bl->type != type)
                        continue;
                    // check bounds
                    if (bl->x < x_0 || bl->x > x_1)
                        continue;
                    if (bl->y < y_0 || bl->y > y_1)
                        continue;
                    if (bl_list_count < BL_LIST_MAX)
                        bl_list[bl_list_count++] = bl;
                }
            } // for bx
        } // for by

    if (type == BL_NUL || type == BL_MOB)
        for (int by = y_0 / BLOCK_SIZE; by <= y_1 / BLOCK_SIZE; by++)
        {
            for (int bx = x_0 / BLOCK_SIZE; bx <= x_1 / BLOCK_SIZE; bx++)
            {
                int b = bx + by * maps[m].bxs;
                struct block_list *bl = maps[m].block_mob[b];
                int c = maps[m].block_mob_count[b];
                for (int i = 0; i < c && bl; i++, bl = bl->next)
                {
                    // check bounds
                    if (bl->x < x_0 || bl->x > x_1)
                        continue;
                    if (bl->y < y_0 || bl->y > y_1)
                        continue;
                    if (bl_list_count < BL_LIST_MAX)
                        bl_list[bl_list_count++] = bl;
                }
            } // for bx
        } // for by

    if (bl_list_count >= BL_LIST_MAX)
        map_log("%s: *WARNING* block count too many!", __func__);

    // don't unlink the blocks while calling the func
    // why does this matter?
    map_freeblock_lock();

    va_list ap;
    va_start(ap, type);
    for (int i = blockcount; i < bl_list_count; i++)
        // Check valid list elements only
        if (bl_list[i]->prev)
            func(bl_list[i], ap);
    va_end(ap);

    // actually unlink the blocks
    map_freeblock_unlock();


    // restore
    bl_list_count = blockcount;
}

/// Call a func for every block in the box of movement
// x_0, etc are player's current position +- how far you can see
// (which is set in battle_athena.conf, default 14 tiles)
// dx, dy in {-1, 0, 1}
// it only applies calls the function for the rim tiles
// func is clif{pc,mob}{in,out}sight or party_send_hp_check

// this function is always called twice:
// once with original location and ds (outsight)
// then with the new location and -ds (insight)

void map_foreachinmovearea(void (*func) (struct block_list *, va_list), int m,
                            int x_0, int y_0, int x_1, int y_1, int dx, int dy,
                            BlockType type, ...)
{
    int blockcount = bl_list_count;

    if (!dx && !dy)
    {
        map_log("%s: no delta", __func__);
        exit(1);
    }
    // if nondiagonal movement, the area is a rectangle
    if (!dx || !dy)
    {
        if (!dx)
        {
            if (dy < 0)
                y_0 = y_1 + dy + 1;
            else
                y_1 = y_0 + dy - 1;
        }
        else if (!dy)
        {
            if (dx < 0)
                x_0 = x_1 + dx + 1;
            else
                x_1 = x_0 + dx - 1;
        }
        if (x_0 < 0)
            x_0 = 0;
        if (y_0 < 0)
            y_0 = 0;
        if (x_1 >= maps[m].xs)
            x_1 = maps[m].xs - 1;
        if (y_1 >= maps[m].ys)
            y_1 = maps[m].ys - 1;
        for (int by = y_0 / BLOCK_SIZE; by <= y_1 / BLOCK_SIZE; by++)
        {
            for (int bx = x_0 / BLOCK_SIZE; bx <= x_1 / BLOCK_SIZE; bx++)
            {
                struct block_list *bl = maps[m].block[bx + by * maps[m].bxs];
                int c = maps[m].block_count[bx + by * maps[m].bxs];
                for (int i = 0; i < c && bl; i++, bl = bl->next)
                {
                    if (type && bl->type != type)
                        continue;
                    if (bl->x < x_0 || bl->x > x_1)
                        continue;
                    if (bl->y < y_0 || bl->y > y_1)
                        continue;
                    if (bl_list_count < BL_LIST_MAX)
                        bl_list[bl_list_count++] = bl;
                }

                bl = maps[m].block_mob[bx + by * maps[m].bxs];
                c = maps[m].block_mob_count[bx + by * maps[m].bxs];
                for (int i = 0; i < c && bl; i++, bl = bl->next)
                {
                    if (type && bl->type != type)
                        continue;
                    if (bl->x < x_0 || bl->x > x_1)
                        continue;
                    if (bl->y < y_0 || bl->y > y_1)
                        continue;
                    if (bl_list_count < BL_LIST_MAX)
                        bl_list[bl_list_count++] = bl;
                }
            } // for bx
        } // for by
    }
    else // dx && dy
    {
        // diagonal movement - the area is an L shape
        if (x_0 < 0)
            x_0 = 0;
        if (y_0 < 0)
            y_0 = 0;
        if (x_1 >= maps[m].xs)
            x_1 = maps[m].xs - 1;
        if (y_1 >= maps[m].ys)
            y_1 = maps[m].ys - 1;

        for (int by = y_0 / BLOCK_SIZE; by <= y_1 / BLOCK_SIZE; by++)
        {
            for (int bx = x_0 / BLOCK_SIZE; bx <= x_1 / BLOCK_SIZE; bx++)
            {
                int b = bx + by * maps[m].bxs;
                struct block_list *bl = maps[m].block[b];
                int c = maps[m].block_count[b];
                for (int i = 0; i < c && bl; i++, bl = bl->next)
                {
                    if (type && bl->type != type)
                        continue;
                    if (bl->x < x_0 || bl->x > x_1)
                        continue;
                    if (bl->y < y_0 || bl->y > y_1)
                        continue;
                    if (    (dx > 0 && bl->x < x_0 + dx) ||
                            (dx < 0 && bl->x > x_1 + dx) ||
                            (dy > 0 && bl->y < y_0 + dy) ||
                            (dy < 0 && bl->y > y_1 + dy))
                        if (bl_list_count < BL_LIST_MAX)
                            bl_list[bl_list_count++] = bl;
                }

                bl = maps[m].block_mob[b];
                c = maps[m].block_mob_count[b];
                for (int i = 0; i < c && bl; i++, bl = bl->next)
                {
                    if (type && bl->type != type)
                        continue;
                    if (bl->x < x_0 || bl->x > x_1)
                        continue;
                    if (bl->y < y_0 || bl->y > y_1)
                        continue;
                    if (    (dx > 0 && bl->x < x_0 + dx) ||
                            (dx < 0 && bl->x > x_1 + dx) ||
                            (dy > 0 && bl->y < y_0 + dy) ||
                            (dy < 0 && bl->y > y_1 + dy))
                        if (bl_list_count < BL_LIST_MAX)
                            bl_list[bl_list_count++] = bl;
                }
            } // for bx
        } // for by
    } // dx && dy

    if (bl_list_count >= BL_LIST_MAX)
        map_log("%s: *WARNING* block count too many!", __func__);

    // prevent freeing blocks
    map_freeblock_lock();

    va_list ap;
    va_start(ap, type);

    for (int i = blockcount; i < bl_list_count; i++)
        // only act on valid blocks
        if (bl_list[i]->prev)
            func(bl_list[i], ap);

    va_end(ap);

    // free the blocks
    map_freeblock_unlock();

    bl_list_count = blockcount;
}

/// Add a temporary object on the floor (loot, etc)
obj_id_t map_addobject(struct block_list *bl)
{
    if (!bl)
    {
        map_log("%s: nullpo!", __func__);
        return 0;
    }
    if (first_free_object_id < 2 || first_free_object_id >= MAX_FLOORITEM)
        first_free_object_id = 2;
    for (; first_free_object_id < MAX_FLOORITEM; first_free_object_id++)
        if (!object[first_free_object_id])
            break;
    if (first_free_object_id >= MAX_FLOORITEM)
    {
        map_log("no free object id");
        return 0;
    }
    if (last_object_id < first_free_object_id)
        last_object_id = first_free_object_id;
    object[first_free_object_id] = bl;
    numdb_insert(id_db, first_free_object_id, (void *)bl);
    return first_free_object_id;
}

// The only external use of this function is skill_delunit
// TODO understand why that is
void map_delobjectnofree(obj_id_t id, BlockType type)
{
    if (!object[id])
        return;

    if (object[id]->type != type)
    {
        map_log("Incorrect type: expected %d, got %d", type, object[id]->type);
        SEGFAULT();
    }

    map_delblock(object[id]);
    numdb_erase(id_db, id);
//  map_freeblock(object[id]);
    object[id] = NULL;

    if (first_free_object_id > id)
        first_free_object_id = id;

    while (last_object_id > 2 && object[last_object_id] == NULL)
        last_object_id--;
}

/// Free an object WITH deletion
void map_delobject(obj_id_t id, BlockType type)
{
    struct block_list *obj = object[id];

    if (!obj)
        return;

    map_delobjectnofree(id, type);
    map_freeblock(obj);
}

/// Execute a function for each temporary object of the given type
void map_foreachobject(void (*func) (struct block_list *, va_list), BlockType type,
                        ...)
{
    int blockcount = bl_list_count;

    for (int i = 2; i <= last_object_id; i++)
    {
        if (object[i])
        {
            if (type && object[i]->type != type)
                continue;
            if (bl_list_count >= BL_LIST_MAX)
                map_log("%s: too many block !", __func__);
            else
                bl_list[bl_list_count++] = object[i];
        }
    }

    map_freeblock_lock();

    va_list ap;
    va_start(ap, type);
    for (int i = blockcount; i < bl_list_count; i++)
        if (bl_list[i]->prev || bl_list[i]->next)
            func(bl_list[i], ap);
    va_end(ap);

    map_freeblock_unlock();

    bl_list_count = blockcount;
}

/// Delete floor items
void map_clearflooritem_timer(timer_id tid, tick_t, custom_id_t id, custom_data_t data)
{
    struct flooritem_data *fitem = (struct flooritem_data *) object[id];
    if (!fitem || fitem->bl.type != BL_ITEM)
    {
        map_log("%s: error: no such item", __func__);
        return;
    }
    if (!data.i && fitem->cleartimer != tid)
    {
        map_log("%s: error: bad data", __func__);
        return;
    }
    if (data.i)
        delete_timer(fitem->cleartimer, map_clearflooritem_timer);
    clif_clearflooritem(fitem, 0);
    map_delobject(fitem->bl.id, BL_ITEM);
}

/// drop an object on a random point near the object
// return (y << 16 ) | x for the chosen point
// TODO rewrite this to avoid the double loop - use an LFSR?
static uint32_t map_searchrandfreecell(uint16_t m, uint16_t x, uint16_t y, int range)
{
    int free_cell = 0;
    for (int i = -range; i <= range; i++)
    {
        if (i + y < 0 || i + y >= maps[m].ys)
            continue;
        for (int j = -range; j <= range; j++)
        {
            if (j + x < 0 || j + x >= maps[m].xs)
                continue;
            int c = read_gat(m, j + x, i + y);
            // must be walkable to drop stuff there
            if (c & 1)
                continue;
            free_cell++;
        }
    }
    if (!free_cell)
        return -1;
    // choose a cell at random, and repeat the logic
    free_cell = MRAND(free_cell);
    for (int i = -range; i <= range; i++)
    {
        if (i + y < 0 || i + y >= maps[m].ys)
            continue;
        for (int j = -range; j <= range; j++)
        {
            if (j + x < 0 || j + x >= maps[m].xs)
                continue;
            int c = read_gat(m, j + x, i + y);
            if (c & 1)
                continue;
            // the chosen cell
            if (!free_cell)
            {
                x += j;
                y += i;
                i = range + 1;
                break;
            }
            free_cell--;
        }
    }

    return x + (y << 16);
}

/// put items in a 3x3 square around the point
// items may initially only be picked by the first owner
// this decays with owner_protection
// after lifetime it disapeears
// dispersal: the actual range over which they may be scattered
int map_addflooritem_any(struct item *item_data, int amount, uint16_t m, uint16_t x,
                          uint16_t y, struct map_session_data **owners,
                          int *owner_protection, int lifetime, int dispersal)
{
    nullpo_retr(0, item_data);

    uint32_t xy = map_searchrandfreecell(m, x, y, dispersal);
    if (xy == -1)
        return 0;
    int r = mt_random();

    struct flooritem_data *fitem;
    CREATE(fitem, struct flooritem_data, 1);
    fitem->bl.type = BL_ITEM;
    fitem->bl.prev = fitem->bl.next = NULL;
    fitem->bl.m = m;
    fitem->bl.x = xy & 0xffff;
    fitem->bl.y = (xy >> 16) & 0xffff;
    fitem->first_get_id = 0;
    fitem->first_get_tick = 0;
    fitem->second_get_id = 0;
    fitem->second_get_tick = 0;
    fitem->third_get_id = 0;
    fitem->third_get_tick = 0;

    // this is kind of ugly
    fitem->bl.id = map_addobject(&fitem->bl);
    if (!fitem->bl.id)
    {
        free(fitem);
        return 0;
    }

    tick_t tick = gettick();

    if (owners[0])
        fitem->first_get_id = owners[0]->bl.id;
    fitem->first_get_tick = tick + owner_protection[0];

    if (owners[1])
        fitem->second_get_id = owners[1]->bl.id;
    fitem->second_get_tick = tick + owner_protection[1];

    if (owners[2])
        fitem->third_get_id = owners[2]->bl.id;
    fitem->third_get_tick = tick + owner_protection[2];

    fitem->item_data= *item_data;
    fitem->item_data.amount = amount;
    fitem->subx = (r & 3) * 3 + 3;
    fitem->suby = ((r >> 2) & 3) * 3 + 3;
    fitem->cleartimer = add_timer(gettick() + lifetime, map_clearflooritem_timer,
                                   fitem->bl.id, 0);

    map_addblock(&fitem->bl);
    clif_dropflooritem(fitem);

    return fitem->bl.id;
}

/// Add an item such that only the given players can pick it up, at first
int map_addflooritem(struct item *item_data, int amount, uint16_t m, uint16_t x, uint16_t y,
                      struct map_session_data *first_sd,
                      struct map_session_data *second_sd,
                      struct map_session_data *third_sd)
{
    struct map_session_data *owners[3] = { first_sd, second_sd, third_sd };
    int owner_protection[3];

    owner_protection[0] = battle_config.item_first_get_time;
    owner_protection[1] = owner_protection[0] + battle_config.item_second_get_time;
    owner_protection[2] = owner_protection[1] + battle_config.item_third_get_time;

    return map_addflooritem_any(item_data, amount, m, x, y,
                                 owners, owner_protection,
                                 battle_config.flooritem_lifetime, 1);
}

/// Add an charid->charname mapping
// if it is already in the table and flagged as "please reply"
// then send the reply to that session
void map_addchariddb(charid_t charid, const char *name)
{
    struct charid2nick *p = (struct charid2nick *)numdb_search(charid_db, charid).p;
    if (!p)
    {
        // if not in the database, it will need to be added it
        CREATE(p, struct charid2nick, 1);
    }
    else
        // is this really necessary?
        numdb_erase(charid_db, charid);

    STRZCPY(p->nick, name);
    numdb_insert(charid_db, charid, (void *)p);
}

/// Add block to DB
void map_addiddb(struct block_list *bl)
{
    nullpo_retv(bl);
    numdb_insert(id_db, bl->id, (void *)bl);
}

/// Delete block from DB
void map_deliddb(struct block_list *bl)
{
    nullpo_retv(bl);
    numdb_erase(id_db, bl->id);
}

/// Add mapping name to session
void map_addnickdb(struct map_session_data *sd)
{
    nullpo_retv(sd);
    strdb_insert(nick_db, sd->status.name, (void *)sd);
}

/// A player quits from the map server
void map_quit(struct map_session_data *sd)
{
    nullpo_retv(sd);

    if (sd->trade_partner)
        trade_tradecancel(sd);

    if (sd->party_invite)
        party_reply_invite(sd, sd->party_invite_account, 0);

    party_send_logout(sd);

    pc_cleareventtimer(sd);

    skill_castcancel(&sd->bl, 0);

    skill_status_change_clear(&sd->bl, 1);
    pc_stop_walking(sd, 0);
    pc_stopattack(sd);
    pc_delinvincibletimer(sd);

    pc_calcstatus(sd, 4);

    clif_clearchar_area(&sd->bl, 2);

    if (pc_isdead(sd))
        pc_setrestartvalue(sd, 2);
    pc_makesavestatus(sd);

    //The storage closing routines will save the char if needed. [Skotlex]
    if (!sd->state.storage_flag)
        chrif_save(sd);
    else if (sd->state.storage_flag == 1)
        storage_storage_quit(sd);

    free(sd->npc_stackbuf);

    map_delblock(&sd->bl);

    numdb_erase(id_db, sd->bl.id);
    strdb_erase(nick_db, sd->status.name);
    numdb_erase(charid_db, (numdb_key_t)sd->status.char_id);
}

// return the session of the given id
// TODO figure out what kind of ID it is and use a typedef
// I think it might be a charid_t but I'm not sure
struct map_session_data *map_id2sd(unsigned int id)
{
    for (int i = 0; i < fd_max; i++)
    {
        if (!session[i])
            continue;
        struct map_session_data *sd = (struct map_session_data *)session[i]->session_data;
        if (sd && sd->bl.id == id)
            return sd;
    }
    return NULL;
}

/// get name of a character
const char *map_charid2nick(charid_t id)
{
    struct charid2nick *p = (struct charid2nick *)numdb_search(charid_db, id).p;

    if (!p)
        return NULL;
    return p->nick;
}

/// Operations to iterate over active map sessions

static struct map_session_data *map_get_session(int i)
{
    if (i < 0 || i > fd_max || !session[i])
        return NULL;
    struct map_session_data *d = (struct map_session_data *)session[i]->session_data;
    if (d && d->state.auth)
        return d;

    return NULL;
}

static struct map_session_data *map_get_session_forward(int start)
{
    // this loop usually isn't traversed many times
    for (int i = start; i < fd_max; i++)
    {
        struct map_session_data *d = map_get_session(i);
        if (d)
            return d;
    }

    return NULL;
}

static struct map_session_data *map_get_session_backward(int start)
{
    // this loop usually isn't traversed many times
    for (int i = start; i >= 0; i--)
    {
        struct map_session_data *d = map_get_session(i);
        if (d)
            return d;
    }

    return NULL;
}

struct map_session_data *map_get_first_session(void)
{
    return map_get_session_forward(0);
}

struct map_session_data *map_get_next_session(struct map_session_data *d)
{
    return map_get_session_forward(d->fd + 1);
}

struct map_session_data *map_get_last_session(void)
{
    return map_get_session_backward(fd_max);
}

struct map_session_data *map_get_prev_session(struct map_session_data *d)
{
    return map_get_session_backward(d->fd - 1);
}

/// get session by name
struct map_session_data *map_nick2sd(const char *nick)
{
    if (!nick)
        return NULL;
    size_t nicklen = strlen(nick);

    int quantity = 0;
    struct map_session_data *sd = NULL;

    for (int i = 0; i < fd_max; i++)
    {
        if (!session[i])
            continue;
        struct map_session_data *pl_sd = (struct map_session_data *)session[i]->session_data;
        if (pl_sd && pl_sd->state.auth)
        {
            // Without case sensitive check (increase the number of similar character names found)
            if (strncasecmp(pl_sd->status.name, nick, nicklen) == 0)
            {
                // Strict comparison (if found, we finish the function immediatly with correct value)
                if (strcmp(pl_sd->status.name, nick) == 0)
                    return pl_sd;
                quantity++;
                sd = pl_sd;
            }
        }
    }
    // Here, the exact character name is not found
    // We return the found index of a similar account ONLY if there is 1 similar character
    if (quantity == 1)
        return sd;

    // Exact character name is not found and 0 or more than 1 similar characters have been found ==> we say not found
    return NULL;
}

struct block_list *map_id2bl(unsigned int id)
{
    if (id < ARRAY_SIZEOF(object))
        return object[id];
    return (struct block_list *)numdb_search(id_db, id).p;
}

/// Run func for whole ID db
void map_foreachiddb(db_func_t func, ...)
{
    va_list ap;
    va_start(ap, func);
    numdb_foreach(id_db, func, ap);
    va_end(ap);
}

/// Add an NPC to a map
// there was some Japanese comment about warps
// return the index within that maps NPC list
int map_addnpc(int m, struct npc_data *nd)
{
    if (m < 0 || m >= map_num)
        return -1;
    int i;
    for (i = 0; i < maps[m].npc_num && i < MAX_NPC_PER_MAP; i++)
        if (!maps[m].npc[i])
            break;
    if (i == MAX_NPC_PER_MAP)
    {
        map_log("too many NPCs in one map %s\n", maps[m].name);
        return -1;
    }
    if (i == maps[m].npc_num)
    {
        maps[m].npc_num++;
    }

    nullpo_retr(0, nd);

    maps[m].npc[i] = nd;
    nd->n = i;
    numdb_insert(id_db, nd->bl.id, (void *)nd);

    return i;
}

// get a map index from map name
int map_mapname2mapid(const char *name)
{
    struct map_data *md = (struct map_data *)strdb_search(map_db, name).p;
    if (md == NULL || md->gat == NULL)
        return -1;
    return md->m;
}

/// Get IP/port of a map on another server
bool map_mapname2ipport(const char *name, in_addr_t *ip, in_port_t *port)
{
    struct map_data *mdos = (struct map_data *)strdb_search(map_db, name).p;
    if (mdos == NULL || mdos->gat)
        return 0;
    *ip = mdos->ip;
    *port = mdos->port;
    return 1;
}

Direction map_calc_dir(struct block_list *src, int x, int y)
{
    nullpo_retr(DIR_S, src);

    int dx = x - src->x;
    int dy = y - src->y;
    if (dx == 0 && dy == 0)
        // 0
        return DIR_S;
    if (dx >= 0 && dy >= 0)
    {
        if (dx * 3 - 1 < dy)
            return DIR_S;
        if (dx > dy * 3)
            return DIR_E;
        return DIR_SE;
    }
    if (dx >= 0 && dy <= 0)
    {
        if (dx * 3 - 1 < -dy)
            return DIR_N;
        if (dx > -dy * 3)
            return DIR_E;
        return DIR_NE;
    }
    if (dx <= 0 && dy <= 0)
    {
        if (dx * 3 + 1 > dy)
            return DIR_N;
        if (dx < dy * 3)
            return DIR_W;
        return DIR_NW;
    }
    if (dx <= 0 && dy >= 0)
    {
        if (-dx * 3 - 1 < dy)
            return DIR_S;
        if (-dx > dy * 3)
            return DIR_W;
        return DIR_SW;
    }
    // unreachable
    return DIR_S;
}

uint8_t map_getcell(int m, int x, int y)
{
    if (x < 0 || x >= maps[m].xs - 1 || y < 0 || y >= maps[m].ys - 1)
        return 1;
    return maps[m].gat[x + y * maps[m].xs];
}

void map_setcell(int m, int x, int y, uint8_t t)
{
    if (x < 0 || x >= maps[m].xs || y < 0 || y >= maps[m].ys)
        return;
    maps[m].gat[x + y * maps[m].xs] = t;
}

/// know what to do for maps on other map-servers
bool map_setipport(const char *name, in_addr_t ip, in_port_t port)
{
    struct map_data *md = (struct map_data *)strdb_search(map_db, name).p;
    if (!md)
    {
        // not exist -> add new data
        CREATE(md, struct map_data, 1);
        STRZCPY(md->name, name);
        md->gat = NULL;
        md->ip = ip;
        md->port = port;
        strdb_insert(map_db, md->name, (void *)md);
        return 0;
    }
    if (md->gat)
    {
        if (ip != clif_getip() || port != clif_getport())
        {
            map_log("%s: map server told us one of our maps is not ours!", __func__);
            return 1;
        }
        return 0;
    }

    md->ip = ip;
    md->port = port;
    return 0;
}

/// Read a map
static bool map_readmap(int m, const char *filename)
{
    printf("\rLoading Maps [%d/%d]: %-50s  ", m, map_num, filename);
    fflush(stdout);

    // read & convert fn
    uint8_t *gat = (uint8_t *)grfio_read(filename);
    if (!gat)
        return 0;

    maps[m].m = m;
    int xs = maps[m].xs = *(uint16_t *) (gat);
    int ys = maps[m].ys = *(uint16_t *) (gat + 2);
    printf("%i %i", xs, ys);
    fflush(stdout);

    maps[m].npc_num = 0;
    maps[m].users = 0;
    memset(&maps[m].flag, 0, sizeof(maps[m].flag));
    if (battle_config.pk_mode)
        maps[m].flag.pvp = 1;

    // instead of copying, just use it
    maps[m].gat = gat + 4;

    maps[m].bxs = (xs + BLOCK_SIZE - 1) / BLOCK_SIZE;
    maps[m].bys = (ys + BLOCK_SIZE - 1) / BLOCK_SIZE;

    size_t size = maps[m].bxs * maps[m].bys;

    CREATE(maps[m].block, struct block_list *, size);
    CREATE(maps[m].block_mob, struct block_list *, size);
    CREATE(maps[m].block_count, int, size);
    CREATE(maps[m].block_mob_count, int, size);

    strdb_insert(map_db, maps[m].name, (void *)&maps[m]);

    return 1;
}

/// Read all maps
static void map_readallmap(void)
{
    int maps_removed = 0;
    char fn[256] = "";

    for (int i = 0; i < map_num; i++)
    {
        // if (strstr(maps[i].name, ".gat") != NULL)
        sprintf(fn, "data/%s", maps[i].name);
        if (!map_readmap(i, fn))
        {
            i--;
            maps_removed++;
        }
    }

    printf("\rMaps Loaded: %d %60s\n", map_num, "");
    printf("Maps Removed: %d \n", maps_removed);
}

/// Add a map to load
static void map_addmap(const char *mapname)
{
    if (strcasecmp(mapname, "clear") == 0)
    {
        map_num = 0;
        return;
    }

    if (map_num >= MAX_MAP_PER_SERVER)
    {
        map_log("%s: too many maps\n", __func__);
        return;
    }
    STRZCPY(maps[map_num].name, mapname);
    map_num++;
}

extern char *gm_logfile_name;

#define LOGFILE_SECONDS_PER_CHUNK_SHIFT 10

FILE *map_logfile = NULL;
char *map_logfile_name = NULL;
static long map_logfile_index;

static void map_close_logfile(void)
{
    if (map_logfile)
    {
        char *filenameop_buf = (char*)malloc(strlen(map_logfile_name) + 50);
        sprintf(filenameop_buf, "gzip -f %s.%ld", map_logfile_name,
                 map_logfile_index);

        fclose(map_logfile);

        if (!system(filenameop_buf))
            perror(filenameop_buf);

        free(filenameop_buf);
    }
}

static void map_start_logfile(long suffix)
{
    char *filename_buf = (char*)malloc(strlen(map_logfile_name) + 50);
    map_logfile_index = suffix >> LOGFILE_SECONDS_PER_CHUNK_SHIFT;

    sprintf(filename_buf, "%s.%ld", map_logfile_name, map_logfile_index);
    map_logfile = fopen(filename_buf, "w+");
    if (!map_logfile)
        perror(map_logfile_name);

    free(filename_buf);
}

static void map_set_logfile(const char *filename)
{
    map_logfile_name = strdup(filename);
    struct timeval tv;
    gettimeofday(&tv, NULL);

    map_start_logfile(tv.tv_sec);
    atexit(map_close_logfile);
    map_log("log-start v3");
}

void map_log(const char *format, ...)
{
    if (!map_logfile)
        return;

    struct timeval tv;
    gettimeofday(&tv, NULL);

    if ((tv.tv_sec >> LOGFILE_SECONDS_PER_CHUNK_SHIFT) != map_logfile_index)
    {
        map_close_logfile();
        map_start_logfile(tv.tv_sec);
    }
    if (!map_logfile)
        return;

    va_list args;
    va_start(args, format);
    fprintf(map_logfile, "%ld.%06ld ", (long) tv.tv_sec, (long) tv.tv_usec);
    vfprintf(map_logfile, format, args);
    fputc('\n', map_logfile);
    va_end(args);
}

/// Read conf/map_athena.conf
static void map_config_read(const char *cfgName)
{
    FILE *fp = fopen_(cfgName, "r");
    if (!fp)
    {
        printf("Map configuration file not found at: %s\n", cfgName);
        exit(1);
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '/' && line[1] == '/')
            continue;
        char w1[1024], w2[1024];
        if (sscanf(line, "%[^:]: %[^\r\n]", w1, w2) == 2)
        {
            if (strcasecmp(w1, "userid") == 0)
            {
                chrif_setuserid(w2);
                continue;
            }
            if (strcasecmp(w1, "passwd") == 0)
            {
                chrif_setpasswd(w2);
                continue;
            }
            if (strcasecmp(w1, "char_ip") == 0)
            {
                struct hostent *h = gethostbyname(w2);
                if (h)
                {
                    printf("Character server IP address : %s -> %hhu.%hhu.%hhu.%hhu\n", w2,
                            h->h_addr[0], h->h_addr[1],
                            h->h_addr[2], h->h_addr[3]);
                    sprintf(w2, "%hhu.%hhu.%hhu.%hhu",
                             h->h_addr[0], h->h_addr[1],
                             h->h_addr[2], h->h_addr[3]);
                }
                chrif_setip(w2);
                continue;
            }
            if (strcasecmp(w1, "char_port") == 0)
            {
                chrif_setport(atoi(w2));
                continue;
            }
            if (strcasecmp(w1, "map_ip") == 0)
            {
                struct hostent *h = gethostbyname(w2);
                if (h)
                {
                    printf("Map server IP address : %s -> %hhu.%hhu.%hhu.%hhu\n", w2,
                            h->h_addr[0], h->h_addr[1],
                            h->h_addr[2], h->h_addr[3]);
                    sprintf(w2, "%hhu.%hhu.%hhu.%hhu",
                             h->h_addr[0], h->h_addr[1],
                             h->h_addr[2], h->h_addr[3]);
                }
                clif_setip(w2);
                continue;
            }
            if (strcasecmp(w1, "map_port") == 0)
            {
                map_port = atoi(w2);
                clif_setport(map_port);
                continue;
            }
            if (strcasecmp(w1, "map") == 0)
            {
                map_addmap(w2);
                continue;
            }
            if (strcasecmp(w1, "npc") == 0)
            {
                npc_addsrcfile(w2);
                continue;
            }
            if (strcasecmp(w1, "autosave_time") == 0)
            {
                autosave_interval = atoi(w2) * 1000;
                if (autosave_interval <= 0)
                    autosave_interval = DEFAULT_AUTOSAVE_INTERVAL;
                continue;
            }
            if (strcasecmp(w1, "motd_txt") == 0)
            {
                strcpy(motd_txt, w2);
                continue;
            }
            if (strcasecmp(w1, "mapreg_txt") == 0)
            {
                strcpy(mapreg_txt, w2);
                continue;
            }
            if (strcasecmp(w1, "gm_log") == 0)
            {
                gm_logfile_name = strdup(w2);
                continue;
            }
            if (strcasecmp(w1, "log_file") == 0)
            {
                map_set_logfile(w2);
                continue;
            }
            if (strcasecmp(w1, "import") == 0)
            {
                map_config_read(w2);
                continue;
            }
        }
    }
    fclose_(fp);
}

static void cleanup_sub(struct block_list *bl, va_list)
{
    nullpo_retv(bl);

    switch (bl->type)
    {
        case BL_PC:
            map_delblock(bl);  // There is something better...
            break;
        case BL_NPC:
            npc_delete((struct npc_data *) bl);
            break;
        case BL_MOB:
            mob_delete((struct mob_data *) bl);
            break;
        case BL_ITEM:
            map_clearflooritem(bl->id);
            break;
        case BL_SPELL:
            spell_free_invocation((struct invocation *) bl);
            break;
    }
}

/// server is shutting down
// TODO: don't bother freeing anything as process is about to end
static void do_final(void)
{
    for (int map_id = 0; map_id < map_num; map_id++)
    {
        if (maps[map_id].m)
            map_foreachinarea(cleanup_sub, map_id, 0, 0, maps[map_id].xs,
                               maps[map_id].ys, BL_NUL, 0);
    }

    for (int i = 0; i < fd_max; i++)
        delete_session(i);

    numdb_final(id_db, NULL);
    strdb_final(map_db, NULL);
    strdb_final(nick_db, NULL);
    numdb_final(charid_db, NULL);

    do_final_script();
    do_final_itemdb();
    do_final_storage();
}

/// --help was passed
// FIXME this should produce output
void map_helpscreen(void)
{
    exit(1);
}

int compare_item(struct item *a, struct item *b)
{
    return ((a->nameid == b->nameid) &&
            (a->identify == b->identify) &&
            (a->refine == b->refine) &&
            (a->attribute == b->attribute) &&
            (a->card[0] == b->card[0]) &&
            (a->card[1] == b->card[1]) &&
            (a->card[2] == b->card[2]) && (a->card[3] == b->card[3]));
}

// TODO move shutdown stuff here
void term_func(void)
{
    do_final();
}

/// parse command-line arguments
void do_init(int argc, char *argv[])
{
    const char *MAP_CONF_NAME = "conf/map_athena.conf";
    const char *BATTLE_CONF_FILENAME = "conf/battle_athena.conf";
    const char *ATCOMMAND_CONF_FILENAME = "conf/atcommand_athena.conf";
    const char *SCRIPT_CONF_NAME = "conf/script_athena.conf";

    for (int i = 1; i < argc; i++)
    {

        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0
            || strcmp(argv[i], "-?") == 0 || strcmp(argv[i], "/?") == 0)
            map_helpscreen();
        else if (strcmp(argv[i], "--map_config") == 0)
            MAP_CONF_NAME = argv[i + 1];
        else if (strcmp(argv[i], "--battle_config") == 0)
            BATTLE_CONF_FILENAME = argv[i + 1];
        else if (strcmp(argv[i], "--atcommand_config") == 0)
            ATCOMMAND_CONF_FILENAME = argv[i + 1];
        else if (strcmp(argv[i], "--script_config") == 0)
            SCRIPT_CONF_NAME = argv[i + 1];
    }

    map_config_read(MAP_CONF_NAME);
    battle_config_read(BATTLE_CONF_FILENAME);
    atcommand_config_read(ATCOMMAND_CONF_FILENAME);
    script_config_read(SCRIPT_CONF_NAME);

    id_db = numdb_init();
    map_db = strdb_init();
    nick_db = strdb_init();
    charid_db = numdb_init();

    map_readallmap();

    do_init_chrif();
    do_init_clif();
    do_init_itemdb();
    do_init_mob();
    do_init_script();
    do_init_npc();
    do_init_pc();
    do_init_party();
    do_init_storage();
    do_init_skill();
    do_init_magic();

    // NPC::OnInit labels
    npc_event_do_oninit();

    if (battle_config.pk_mode)
        printf("The server is running in \033[1;31mPK Mode\033[0m.\n");

    printf("The map-server is \033[1;32mready\033[0m (Server is listening on the port %d).\n\n",
            map_port);
}

int map_scriptcont(struct map_session_data *sd, int id)
{
    struct block_list *bl = map_id2bl(id);

    if (!bl)
        return 0;

    switch (bl->type)
    {
        case BL_NPC:
            return npc_scriptcont(sd, id);
        case BL_SPELL:
            spell_execute_script((struct invocation *) bl);
            break;
    }

    return 0;
}
