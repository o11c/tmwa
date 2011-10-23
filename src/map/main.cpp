#include "main.hpp"

#include <sys/time.h>
#include <netdb.h>
#include <cassert>

#include "../common/core.hpp"
#include "../common/mt_rand.hpp"
#include "../common/nullpo.hpp"
#include "../common/timer.hpp"
#include "../common/utils.hpp"

#include "atcommand.hpp"
#include "battle.hpp"
#include "chrif.hpp"
#include "clif.hpp"
#include "grfio.hpp"
#include "itemdb.hpp"
#include "magic.hpp"
#include "magic-stmt.hpp"
#include "mob.hpp"
#include "npc.hpp"
#include "party.hpp"
#include "pc.hpp"
#include "script.hpp"
#include "skill.hpp"
#include "storage.hpp"
#include "trade.hpp"

static void map_helpscreen() __attribute__((noreturn));

static DMap<BlockID, BlockList *> id_db;
const DMap<BlockID, BlockList *>& get_id_db() { return id_db; }
static std::map<std::string, map_data *> map_db;
static std::map<std::string, MapSessionData *> nick_db;
static std::map<charid_t, struct charid2nick> charid_db;

static sint32 users = 0;
// NOTE: the ID range for temporary objects must be the lowest
// TODO: why not 1?
constexpr BlockID MIN_TEMPORARY_OBJECT = BlockID(2);
// TODO: it might allow more efficient code if this were a power of 2
constexpr BlockID MAX_TEMPORARY_OBJECT = BlockID(500000);
static BlockID next_object_id = MIN_TEMPORARY_OBJECT;
static BlockID first_free_object_id = MIN_TEMPORARY_OBJECT;

#define block_free_max 1048576
static BlockList *block_free[block_free_max];
static sint32 block_free_count = 0, block_free_lock = 0;

#define BL_LIST_MAX 1048576
static BlockList *bl_list[BL_LIST_MAX];
static sint32 bl_list_count = 0;

map_data_local maps[MAX_MAP_PER_SERVER];
sint32 map_num = 0;

in_port_t map_port = 0;

constexpr std::chrono::minutes DEFAULT_AUTOSAVE_INTERVAL(1);
std::chrono::milliseconds autosave_interval = DEFAULT_AUTOSAVE_INTERVAL;

struct charid2nick
{
    char nick[24];
};

char motd_txt[256] = "conf/motd.txt";

// can be modified in char-server configuration file
char whisper_server_name[24] = "Server";

/// Save the number of users reported by the char server
void map_setusers(sint32 n)
{
    users = n;
}

/// Get the number of users previously reported by the char server
sint32 map_getusers(void)
{
    return users;
}



/// Free a block
// prohibits deleting until block_free_lock is 0
sint32 map_freeblock(BlockList *bl)
{
    if (!block_free_lock)
    {
        delete bl;
        return 0;
    }
    // TODO: set a breakpoint to set a watchpoint
    map_log("Adding block %d due to %d locks", block_free_count, block_free_lock);
    if (block_free_count >= block_free_max)
        map_log("%s: MEMORY LEAK: too many free blocks!", __func__);
    else
        block_free[block_free_count++] = bl;
    return block_free_lock;
}


// TODO: figure out if this is necessary
/// Temporarily prohibit freeing blocks.
sint32 map_freeblock_lock(void)
{
    return ++block_free_lock;
}

/// Remove a prohibition on freeing blocks
// if this was the last lock, free the queued ones
sint32 map_freeblock_unlock(void)
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
    for (sint32 i = 0; i < block_free_count; i++)
    {
        //TODO: set a breakpoint to unset the watchpoint
        delete block_free[i];
        block_free[i] = NULL;
    }
    block_free_count = 0;
    return 0;
}


/// this is a dummy bl->prev so that legitimate blocks always have non-NULL
static BlockList bl_head(BL_NUL, BlockID());


/// link a new block
bool map_addblock(BlockList *bl)
{
    nullpo_ret(bl);

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
sint32 map_delblock(BlockList *bl)
{
    nullpo_ret(bl);

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
void map_foreachinarea_impl(MapForEachFunc func, sint32 m,
                            sint32 x_0, sint32 y_0, sint32 x_1, sint32 y_1, BlockType type)
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
    sint32 blockcount = bl_list_count;

    if (type == BL_NUL || type != BL_MOB)
        for (sint32 by = y_0 / BLOCK_SIZE; by <= y_1 / BLOCK_SIZE; by++)
        {
            for (sint32 bx = x_0 / BLOCK_SIZE; bx <= x_1 / BLOCK_SIZE; bx++)
            {
                sint32 b = bx + by * maps[m].bxs;
                BlockList *bl = maps[m].block[b];
                sint32 c = maps[m].block_count[b];
                for (sint32 i = 0; i < c && bl; i++, bl = bl->next)
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
        for (sint32 by = y_0 / BLOCK_SIZE; by <= y_1 / BLOCK_SIZE; by++)
        {
            for (sint32 bx = x_0 / BLOCK_SIZE; bx <= x_1 / BLOCK_SIZE; bx++)
            {
                sint32 b = bx + by * maps[m].bxs;
                BlockList *bl = maps[m].block_mob[b];
                sint32 c = maps[m].block_mob_count[b];
                for (sint32 i = 0; i < c && bl; i++, bl = bl->next)
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

    for (sint32 i = blockcount; i < bl_list_count; i++)
        // Check valid list elements only
        if (bl_list[i]->prev)
            func(bl_list[i]);

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

void map_foreachinmovearea_impl(MapForEachFunc func, sint32 m,
                                sint32 x_0, sint32 y_0, sint32 x_1, sint32 y_1,
                                sint32 dx, sint32 dy, BlockType type)
{
    sint32 blockcount = bl_list_count;

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
        for (sint32 by = y_0 / BLOCK_SIZE; by <= y_1 / BLOCK_SIZE; by++)
        {
            for (sint32 bx = x_0 / BLOCK_SIZE; bx <= x_1 / BLOCK_SIZE; bx++)
            {
                BlockList *bl = maps[m].block[bx + by * maps[m].bxs];
                sint32 c = maps[m].block_count[bx + by * maps[m].bxs];
                for (sint32 i = 0; i < c && bl; i++, bl = bl->next)
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
                for (sint32 i = 0; i < c && bl; i++, bl = bl->next)
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

        for (sint32 by = y_0 / BLOCK_SIZE; by <= y_1 / BLOCK_SIZE; by++)
        {
            for (sint32 bx = x_0 / BLOCK_SIZE; bx <= x_1 / BLOCK_SIZE; bx++)
            {
                sint32 b = bx + by * maps[m].bxs;
                BlockList *bl = maps[m].block[b];
                sint32 c = maps[m].block_count[b];
                for (sint32 i = 0; i < c && bl; i++, bl = bl->next)
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
                for (sint32 i = 0; i < c && bl; i++, bl = bl->next)
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

    for (sint32 i = blockcount; i < bl_list_count; i++)
        // only act on valid blocks
        if (bl_list[i]->prev)
            func(bl_list[i]);

    // free the blocks
    map_freeblock_unlock();

    bl_list_count = blockcount;
}

/// Add a temporary object on the floor (loot, etc)
BlockID map_addobject(BlockList *bl)
{
    nullpo_retr(BlockID(), bl);
    if (first_free_object_id >= MAX_TEMPORARY_OBJECT)
        first_free_object_id = MIN_TEMPORARY_OBJECT;
    if (id_db.replace(first_free_object_id, bl))
    {
        map_log("Error: too many or long lived temporary objects!");
        abort();
    }
    return first_free_object_id++;
}

/// Free an object WITH deletion
void map_delobject(BlockID id, BlockType type)
{
    assert(type == BL_ITEM || type == BL_SPELL);
    assert (MIN_TEMPORARY_OBJECT <= id && id < MAX_TEMPORARY_OBJECT);

    BlockList *bl = id_db.take(id);
    assert(bl != NULL);

    if (bl->type != type)
    {
        map_log("Incorrect type: expected %d, got %d", type, bl->type);
        abort();
    }

    map_delblock(bl);
    map_freeblock(bl);
}

/// Execute a function for each temporary object of the given type
// NOTE: this function depends on the fact the temporary IDs are the lowest
void map_foreachobject_impl(MapForEachFunc func)
{
    // (why) *is* this (and the freeblock_lock) needed?
    std::deque<BlockList *> temp_blocks;

    for (const auto& pair: id_db)
    {
        BlockList *bl = pair.second;
        //assert pair.first == bl->id
        if (bl->id >= MAX_TEMPORARY_OBJECT)
            break;

        temp_blocks.push_back(bl);
    }

    map_freeblock_lock();
    for (BlockList *bl : temp_blocks)
        if (bl->prev || bl->next)
            func(bl);
    map_freeblock_unlock();
}

/// Delete floor items
void map_clearflooritem_timer(timer_id tid, tick_t, BlockID id)
{
    bool flag = tid == NULL;
    struct flooritem_data *fitem = static_cast<struct flooritem_data *>(id_db.get(id));
    if (!fitem || fitem->type != BL_ITEM)
    {
        map_log("%s: error: no such item", __func__);
        abort();
    }
    if (!flag && fitem->cleartimer != tid)
    {
        map_log("%s: error: bad data", __func__);
        abort();
    }
    if (flag)
        delete_timer(fitem->cleartimer);
    clif_clearflooritem(fitem, -1);
    map_delobject(fitem->id, BL_ITEM);
}

/// drop an object on a random point near the object
// return (y << 16 ) | x for the chosen point
// TODO rewrite this to avoid the double loop - use an LFSR?
// TODO I think I did this with while working in the magic system ...
static std::pair<uint16, uint16> map_searchrandfreecell(uint16 m, uint16 x, uint16 y, sint32 range)
{
    sint32 free_cell = 0;
    for (sint32 i = -range; i <= range; i++)
    {
        if (i + y < 0 || i + y >= maps[m].ys)
            continue;
        for (sint32 j = -range; j <= range; j++)
        {
            if (j + x < 0 || j + x >= maps[m].xs)
                continue;
            // must be walkable to drop stuff there
            if (read_gat(m, j + x, i + y) & MapCell::SOLID)
                continue;
            free_cell++;
        }
    }
    if (!free_cell)
        abort();
    // choose a cell at random, and repeat the logic
    free_cell = MRAND(free_cell);
    for (sint32 i = -range; i <= range; i++)
    {
        if (i + y < 0 || i + y >= maps[m].ys)
            continue;
        for (sint32 j = -range; j <= range; j++)
        {
            if (j + x < 0 || j + x >= maps[m].xs)
                continue;
            if (read_gat(m, j + x, i + y) & MapCell::SOLID)
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

    return {x, y};
}

/// put items in a 3x3 square around the point
// items may initially only be picked by the first owner
// this decays with owner_protection
// after lifetime it disapeears
// dispersal: the actual range over which they may be scattered
BlockID map_addflooritem_any(struct item *item_data, sint32 amount,
                             uint16 m, uint16 x, uint16 y,
                             MapSessionData **owners, interval_t *owner_protection,
                             interval_t lifetime, sint32 dispersal)
{
    nullpo_retr(BlockID(), item_data);

    std::pair<uint16, uint16> xy = map_searchrandfreecell(m, x, y, dispersal);
    sint32 r = mt_random();

    struct flooritem_data *fitem = new flooritem_data(m, xy.first, xy.second);

    tick_t tick = gettick();

    if (owners[0])
        fitem->first_get_id = owners[0]->id;
    fitem->first_get_tick = tick + owner_protection[0];

    if (owners[1])
        fitem->second_get_id = owners[1]->id;
    fitem->second_get_tick = tick + owner_protection[1];

    if (owners[2])
        fitem->third_get_id = owners[2]->id;
    fitem->third_get_tick = tick + owner_protection[2];

    fitem->item_data= *item_data;
    fitem->item_data.amount = amount;
    fitem->subx = (r & 3) * 3 + 3;
    fitem->suby = ((r >> 2) & 3) * 3 + 3;
    fitem->cleartimer = add_timer(gettick() + lifetime, map_clearflooritem_timer, fitem->id);

    map_addblock(fitem);
    clif_dropflooritem(fitem);

    return fitem->id;
}

/// Add an item such that only the given players can pick it up, at first
BlockID map_addflooritem(struct item *item_data, sint32 amount,
                         uint16 m, uint16 x, uint16 y,
                         MapSessionData *first_sd, MapSessionData *second_sd,
                         MapSessionData *third_sd)
{
    MapSessionData *owners[3] = { first_sd, second_sd, third_sd };
    interval_t owner_protection[3];

    owner_protection[0] = std::chrono::milliseconds(battle_config.item_first_get_time);
    owner_protection[1] = owner_protection[0] + std::chrono::milliseconds(battle_config.item_second_get_time);
    owner_protection[2] = owner_protection[1] + std::chrono::milliseconds(battle_config.item_third_get_time);

    return map_addflooritem_any(item_data, amount, m, x, y,
                                owners, owner_protection,
                                std::chrono::milliseconds(battle_config.flooritem_lifetime), 1);
}

/// Add an charid->charname mapping
// if it is already in the table and flagged as "please reply"
// then send the reply to that session
void map_addchariddb(charid_t charid, const char *name)
{
    struct charid2nick p;
    STRZCPY(p.nick, name);
    charid_db[charid] = p;
}

/// Add block to DB
void map_addiddb(BlockList *bl)
{
    nullpo_retv(bl);
    id_db.set(bl->id, bl);
}

/// Delete block from DB
void map_deliddb(BlockList *bl)
{
    nullpo_retv(bl);
    id_db.set(bl->id, NULL);
}

/// Add mapping name to session
void map_addnickdb(MapSessionData *sd)
{
    nullpo_retv(sd);
    if (!nick_db.insert({sd->status.name, sd}).second)
        abort();
}

/// A player quits from the map server
void map_quit(MapSessionData *sd)
{
    nullpo_retv(sd);

    if (sd->trade_partner)
        trade_tradecancel(sd);

    if (sd->party_invite)
        party_reply_invite(sd, sd->party_invite_account, 0);

    party_send_logout(sd);

    pc_cleareventtimer(sd);

    skill_castcancel(sd);

    skill_status_change_clear(sd, 1);
    pc_stop_walking(sd, 0);
    pc_stopattack(sd);
    pc_delinvincibletimer(sd);

    pc_calcstatus(sd, 4);

    clif_being_remove(sd, BeingRemoveType::QUIT);

    if (pc_isdead(sd))
        pc_setrestartvalue(sd, 2);
    pc_makesavestatus(sd);

    //The storage closing routines will save the char if needed. [Skotlex]
    if (!sd->state.storage_flag)
        chrif_save(sd);
    else if (sd->state.storage_flag == 1)
        storage_storage_quit(sd);

    map_delblock(sd);

    id_db.set(sd->id, NULL);
    nick_db.erase(sd->status.name);
    charid_db.erase(sd->status.char_id);
}

// return the session data of the given account ID
MapSessionData *map_id2sd(BlockID id)
{
    for (MapSessionData *sd : all_sessions)
        if (sd->id == id)
            return sd;
    return NULL;
}
MapSessionData *map_id2authsd(BlockID id)
{
    for (MapSessionData *sd : auth_sessions)
        if (sd->id == id)
            return sd;
    return NULL;
}

/// get name of a character
const char *map_charid2nick(charid_t id)
{
    auto it = charid_db.find(id);
    if (it == charid_db.end())
        return NULL;

    return it->second.nick;
}

/// Operations to iterate over active map sessions

static MapSessionData *map_get_session(sint32 i)
{
    if (i < 0 || i > fd_max || !session[i])
        return NULL;
    MapSessionData *d = static_cast<MapSessionData *>(session[i]->session_data);
    if (d && d->state.auth)
        return d;

    return NULL;
}

static MapSessionData *map_get_session_forward(sint32 start) __attribute__((pure));
static MapSessionData *map_get_session_forward(sint32 start)
{
    // this loop usually isn't traversed many times
    for (sint32 i = start; i < fd_max; i++)
    {
        MapSessionData *d = map_get_session(i);
        if (d)
            return d;
    }

    return NULL;
}

static MapSessionData *map_get_session_backward(sint32 start)
{
    // this loop usually isn't traversed many times
    for (sint32 i = start; i >= 0; i--)
    {
        MapSessionData *d = map_get_session(i);
        if (d)
            return d;
    }

    return NULL;
}

MapSessionData *map_get_first_session(void)
{
    return map_get_session_forward(0);
}

MapSessionData *map_get_next_session(MapSessionData *d)
{
    return map_get_session_forward(d->fd + 1);
}

MapSessionData *map_get_last_session(void)
{
    return map_get_session_backward(fd_max);
}

MapSessionData *map_get_prev_session(MapSessionData *d)
{
    return map_get_session_backward(d->fd - 1);
}

/// get session by name
MapSessionData *map_nick2sd(const char *nick)
{
    if (!nick)
        return NULL;
    size_t nicklen = strlen(nick);

    sint32 quantity = 0;
    MapSessionData *sd = NULL;

    for (MapSessionData *pl_sd : auth_sessions)
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
    // Here, the exact character name is not found
    // We return the found index of a similar account ONLY if there is 1 similar character
    if (quantity == 1)
        return sd;

    // Exact character name is not found and 0 or more than 1 similar characters have been found ==> we say not found
    return NULL;
}

BlockList *map_id2bl(BlockID id)
{
    return id_db.get(id);
}

/// Add an NPC to a map
// there was some Japanese comment about warps
// return the index within that maps NPC list
sint32 map_addnpc(sint32 m, struct npc_data *nd)
{
    if (m < 0 || m >= map_num)
        return -1;
    sint32 i;
    for (i = 0; i < maps[m].npc_num && i < MAX_NPC_PER_MAP; i++)
        if (!maps[m].npc[i])
            break;
    if (i == MAX_NPC_PER_MAP)
    {
        map_log("too many NPCs in one map %s\n", &maps[m].name);
        return -1;
    }
    if (i == maps[m].npc_num)
    {
        maps[m].npc_num++;
    }

    nullpo_ret(nd);

    maps[m].npc[i] = nd;
    nd->n = i;
    id_db.set(nd->id, nd);

    return i;
}

// get a map index from map name
// TODO convert to return a map_data_local directly
sint32 map_mapname2mapid(const fixed_string<16>& name)
{
    auto it = map_db.find(&name);
    if (it == map_db.end())
        return -1;
    map_data *md = it->second;
    if (md == NULL || md->gat == NULL)
        return -1;
    return static_cast<map_data_local *>(md)->m;
}

/// Get IP/port of a map on another server
bool map_mapname2ipport(const fixed_string<16>& name, IP_Address *ip, in_port_t *port)
{
    auto it = map_db.find(&name);
    if (it == map_db.end())
        return 0;
    map_data *md = it->second;
    if (md == NULL || md->gat)
        return 0;
    map_data_remote *mdos = static_cast<map_data_remote *>(md);
    *ip = mdos->ip;
    *port = mdos->port;
    return 1;
}

Direction map_calc_dir(BlockList *src, sint32 x, sint32 y)
{
    nullpo_retr(Direction::S, src);

    sint32 dx = x - src->x;
    sint32 dy = y - src->y;
    if (dx == 0 && dy == 0)
        // 0
        return Direction::S;
    if (dx >= 0 && dy >= 0)
    {
        if (dx * 3 - 1 < dy)
            return Direction::S;
        if (dx > dy * 3)
            return Direction::E;
        return Direction::SE;
    }
    if (dx >= 0 && dy <= 0)
    {
        if (dx * 3 - 1 < -dy)
            return Direction::N;
        if (dx > -dy * 3)
            return Direction::E;
        return Direction::NE;
    }
    if (dx <= 0 && dy <= 0)
    {
        if (dx * 3 + 1 > dy)
            return Direction::N;
        if (dx < dy * 3)
            return Direction::W;
        return Direction::NW;
    }
    if (dx <= 0 && dy >= 0)
    {
        if (-dx * 3 - 1 < dy)
            return Direction::S;
        if (-dx > dy * 3)
            return Direction::W;
        return Direction::SW;
    }
    // unreachable
    return Direction::S;
}

MapCell map_getcell(sint32 m, sint32 x, sint32 y)
{
    if (x < 0 || x >= maps[m].xs - 1 || y < 0 || y >= maps[m].ys - 1)
        return MapCell::ALL;
    return maps[m].gat[x + y * maps[m].xs];
}

void map_setcell(sint32 m, sint32 x, sint32 y, MapCell t)
{
    if (x < 0 || x >= maps[m].xs || y < 0 || y >= maps[m].ys)
        return;
    maps[m].gat[x + y * maps[m].xs] = t;
}

/// know what to do for maps on other map-servers
bool map_setipport(const fixed_string<16>& name, IP_Address ip, in_port_t port)
{
    auto it = map_db.find(&name);

    if (it == map_db.end())
    {
        // not exist -> add new data
        map_data_remote *mdr = new map_data_remote;
        mdr->name = name;
        mdr->gat = NULL;
        mdr->ip = ip;
        mdr->port = port;
        map_db.insert({ &name, mdr });
        return 0;
    }

    map_data *md = it->second;
    if (md->gat)
    {
        if (ip != clif_getip() || port != clif_getport())
        {
            map_log("%s: map server told us one of our maps is not ours!",
                    __func__);
            return 1;
        }
        return 0;
    }

    map_data_remote *mdr = static_cast<map_data_remote *>(md);
    mdr->ip = ip;
    mdr->port = port;
    return 0;
}

/// Read a map
static bool map_readmap(sint32 m, const char *filename)
{
    printf("\rLoading Maps [%d/%d]: %-50s  ", m, map_num, filename);
    fflush(stdout);

    // read & convert fn
    uint8 *gat = static_cast<uint8 *>(grfio_read(filename));
    if (!gat)
        return 0;

    maps[m].m = m;
    sint32 xs = maps[m].xs = *reinterpret_cast<uint16 *>(gat);
    sint32 ys = maps[m].ys = *reinterpret_cast<uint16 *>(gat + 2);
    printf("%i %i", xs, ys);
    fflush(stdout);

    maps[m].npc_num = 0;
    maps[m].users = 0;
    memset(&maps[m].flag, 0, sizeof(maps[m].flag));
    if (battle_config.pk_mode)
        maps[m].flag.pvp = 1;

    // instead of copying, just use it
    // TODO: what about instances? I might need to, after all
    // TODO: look into cow mmap
    maps[m].gat = reinterpret_cast<MapCell *>(gat + 4);

    maps[m].bxs = (xs + BLOCK_SIZE - 1) / BLOCK_SIZE;
    maps[m].bys = (ys + BLOCK_SIZE - 1) / BLOCK_SIZE;

    size_t size = maps[m].bxs * maps[m].bys;

    CREATE(maps[m].block, BlockList *, size);
    CREATE(maps[m].block_mob, BlockList *, size);
    CREATE(maps[m].block_count, sint32, size);
    CREATE(maps[m].block_mob_count, sint32, size);

    map_db.insert({ &maps[m].name, &maps[m] });

    return 1;
}

/// Read all maps
static void map_readallmap(void)
{
    sint32 maps_removed = 0;
    char fn[256] = "";

    for (sint32 i = 0; i < map_num; i++)
    {
        // if (strstr(maps[i].name, ".gat") != NULL)
        sprintf(fn, "data/%s", &maps[i].name);
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
static void map_addmap(const fixed_string<16>& mapname)
{
    if (strcasecmp(&mapname, "clear") == 0)
    {
        map_num = 0;
        return;
    }

    if (map_num >= MAX_MAP_PER_SERVER)
    {
        map_log("%s: too many maps\n", __func__);
        return;
    }
    maps[map_num].name = mapname;
    map_num++;
}

#define LOGFILE_SECONDS_PER_CHUNK_SHIFT 10

FILE *map_logfile = NULL;
char *map_logfile_name = NULL;
static sint32 map_logfile_index;

static void map_close_logfile(void)
{
    if (map_logfile)
    {
        // TODO do this properly
        char *filenameop_buf = static_cast<char *>(malloc(strlen(map_logfile_name) + 50));
        sprintf(filenameop_buf, "gzip -f %s.%d", map_logfile_name,
                 map_logfile_index);

        fclose(map_logfile);

        if (!system(filenameop_buf))
            perror(filenameop_buf);

        free(filenameop_buf);
    }
}

static void map_start_logfile(sint32 suffix)
{
    char *filename_buf = static_cast<char *>(malloc(strlen(map_logfile_name) + 50));
    map_logfile_index = suffix >> LOGFILE_SECONDS_PER_CHUNK_SHIFT;

    sprintf(filename_buf, "%s.%d", map_logfile_name, map_logfile_index);
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
    fprintf(map_logfile, "%d.%06d ", static_cast<sint32>(tv.tv_sec), static_cast<sint32>(tv.tv_usec));
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
                chrif_setip(IP_Address(w2));
                continue;
            }
            if (strcasecmp(w1, "char_port") == 0)
            {
                chrif_setport(atoi(w2));
                continue;
            }
            if (strcasecmp(w1, "map_ip") == 0)
            {
                IP_Address ip(w2);
                struct hostent *h = gethostbyname(w2);
                if (h)
                {
                    printf("Map server IP address : %s -> %s\n", w2,
                           ip.to_string().c_str());
                }
                clif_setip(ip);
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
                fixed_string<16> mapname;
                mapname.copy_from(w2);
                map_addmap(mapname);
                continue;
            }
            if (strcasecmp(w1, "npc") == 0)
            {
                npc_addsrcfile(w2);
                continue;
            }
            if (strcasecmp(w1, "autosave_time") == 0)
            {
                autosave_interval = std::chrono::seconds(atoi(w2));
                if (autosave_interval <= std::chrono::seconds::zero())
                    autosave_interval = DEFAULT_AUTOSAVE_INTERVAL;
                continue;
            }
            if (strcasecmp(w1, "motd_txt") == 0)
            {
                strcpy(motd_txt, w2);
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

/// server is shutting down
// don't bother freeing anything as process is about to end
static void do_final(void)
{
    // Saves variables
    do_final_script();
}

/// --help was passed
// FIXME this should produce output
void map_helpscreen(void)
{
    exit(1);
}

sint32 compare_item(struct item *a, struct item *b)
{
    return a->nameid == b->nameid;
}

// TODO move shutdown stuff here
void term_func(void)
{
    do_final();
}

/// parse command-line arguments
void do_init(sint32 argc, char *argv[])
{
    const char *MAP_CONF_NAME = "conf/map_athena.conf";
    const char *BATTLE_CONF_FILENAME = "conf/battle_athena.conf";
    const char *ATCOMMAND_CONF_FILENAME = "conf/atcommand_athena.conf";

    for (sint32 i = 1; i < argc; i++)
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
    }

    map_config_read(MAP_CONF_NAME);
    battle_config_read(BATTLE_CONF_FILENAME);
    atcommand_config_read(ATCOMMAND_CONF_FILENAME);

    map_readallmap();

    do_init_chrif();
    do_init_clif();
    pre_init_script();
    do_init_itemdb();
    do_init_mob();
    do_init_script();
    do_init_npc();
    do_init_pc();
    do_init_party();
    do_init_skill();
    do_init_magic();

    // NPC::OnInit labels
    npc_event_do_oninit();

    if (battle_config.pk_mode)
        printf("The server is running in \033[1;31mPK Mode\033[0m.\n");

    printf("The map-server is \033[1;32mready\033[0m (Server is listening on the port %d).\n\n",
            map_port);
}

sint32 map_scriptcont(MapSessionData *sd, BlockID id)
{
    BlockList *bl = map_id2bl(id);

    if (!bl)
        return 0;

    switch (bl->type)
    {
        case BL_NPC:
            return npc_scriptcont(sd, id);
        case BL_SPELL:
            spell_execute_script(static_cast<invocation_t *>(bl));
            break;
    }

    return 0;
}
