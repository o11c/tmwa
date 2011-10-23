#ifndef MAIN_HPP
#define MAIN_HPP

# include "main.structs.hpp"

# include "../common/db.hpp"

extern map_data_local maps[];
extern sint32 map_num;
extern std::chrono::milliseconds autosave_interval;

extern char motd_txt[];

extern char whisper_server_name[24];

// global information
void map_setusers(sint32);
sint32 map_getusers(void) __attribute__((pure));
// block freeing
sint32 map_freeblock(BlockList *bl);
sint32 map_freeblock_lock(void);
sint32 map_freeblock_unlock(void);
// block related
bool map_addblock(BlockList *);
sint32 map_delblock(BlockList *);
typedef std::function<void (BlockList *)> MapForEachFunc;
void map_foreachinarea_impl(MapForEachFunc, sint32, sint32, sint32, sint32, sint32, BlockType);
template<class... Args>
void map_foreachinarea(void (&func)(BlockList *, Args...), sint32 m,
                       sint32 x_0, sint32 y_0, sint32 x_1, sint32 y_1, BlockType type,
                       Args... args)
{
    map_foreachinarea_impl(std::bind(func, std::placeholders::_1, args...),
                           m, x_0, y_0, x_1, y_1, type);
}

void map_foreachinmovearea_impl(MapForEachFunc, sint32, sint32, sint32, sint32, sint32, sint32, sint32, BlockType);
template<class... Args>
void map_foreachinmovearea(void (&func)(BlockList *, Args...), sint32 m,
                           sint32 x_0, sint32 y_0, sint32 x_1, sint32 y_1, sint32 dx, sint32 dy,
                           BlockType type, Args... args)
{
    map_foreachinmovearea_impl(std::bind(func, std::placeholders::_1, args...),
                           m, x_0, y_0, x_1, y_1, dx, dy, type);
}

/// Temporary objects (loot, etc)
//TODO - put this back here after splitting the headers
//BlockID map_addobject(BlockList *);
void map_delobject(BlockID, BlockType type);

void map_foreachobject_impl(MapForEachFunc);
template<class... Args>
void map_foreachobject(void (&func)(BlockList *, Args...), Args... args)
{
    map_foreachobject_impl(std::bind(func, std::placeholders::_1, args...));
}

void map_quit(MapSessionData *);
// npc
sint32 map_addnpc(sint32, struct npc_data *);

extern FILE *map_logfile;
void map_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
# define MAP_LOG(fmt, ...) map_log("%s", STR_PRINTF(fmt, ## __VA_ARGS__).c_str())
# define MAP_LOG_PC(sd, fmt, args...) MAP_LOG("PC%d %d:%d,%d " fmt, sd->status.char_id, sd->m, sd->x, sd->y, ## args)

// floor item methods
void map_clearflooritem_timer(timer_id, tick_t, BlockID);
inline void map_clearflooritem(BlockID id)
{
    map_clearflooritem_timer(NULL, DEFAULT, id);
}
BlockID map_addflooritem_any(struct item *, sint32 amount,
                             uint16 m, uint16 x, uint16 y,
                             MapSessionData **owners, interval_t *owner_protection,
                             interval_t lifetime, sint32 dispersal);
BlockID map_addflooritem(struct item *, sint32 amount,
                         uint16 m, uint16 x, uint16 y,
                         MapSessionData *, MapSessionData *, MapSessionData *);

// mappings between character id and names
void map_addchariddb(charid_t charid, const char *name);
const char *map_charid2nick(charid_t) __attribute__((pure));

MapSessionData *map_id2sd(BlockID) __attribute__((pure));
MapSessionData *map_id2authsd(BlockID) __attribute__((pure));
BlockList *map_id2bl(BlockID) __attribute__((pure));
sint32 map_mapname2mapid(const fixed_string<16>&) __attribute__((pure));
bool map_mapname2ipport(const fixed_string<16>&, IP_Address *, in_port_t *);
bool map_setipport(const fixed_string<16>& name, IP_Address ip, in_port_t port);

void map_addiddb(BlockList *);
void map_deliddb(BlockList *bl);

void map_addnickdb(MapSessionData *);
sint32 map_scriptcont(MapSessionData *sd, BlockID id);  /* Continues a script either on a spell or on an NPC */
MapSessionData *map_nick2sd(const char *) __attribute__((pure));
sint32 compare_item(struct item *a, struct item *b) __attribute__((pure));

// iterate over players
MapSessionData *map_get_first_session(void) __attribute__((pure));
MapSessionData *map_get_last_session(void) __attribute__((pure));
MapSessionData *map_get_next_session(MapSessionData *current) __attribute__((pure));
MapSessionData *map_get_prev_session(MapSessionData *current) __attribute__((pure));

// edit the gat data
MapCell map_getcell(sint32, sint32, sint32) __attribute__((pure));
__attribute__((deprecated))
void map_setcell(sint32, sint32, sint32, MapCell);

// get the general direction from block's location to the coordinates
Direction map_calc_dir(BlockList *src, sint32 x, sint32 y);

__attribute__((const))
const DMap<BlockID, BlockList *>& get_id_db();

#endif // MAIN_HPP
