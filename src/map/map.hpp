#ifndef MAP_HPP
#define MAP_HPP

# include "map.structs.hpp"

# include "../common/db.hpp"

extern map_data_local maps[];
extern int32_t map_num;
extern int32_t autosave_interval;

extern char motd_txt[];

extern char whisper_server_name[24];

// global information
void map_setusers(int32_t);
int32_t map_getusers(void) __attribute__((pure));
// block freeing
int32_t map_freeblock(BlockList *bl);
int32_t map_freeblock_lock(void);
int32_t map_freeblock_unlock(void);
// block related
bool map_addblock(BlockList *);
int32_t map_delblock(BlockList *);
typedef std::function<void (BlockList *)> MapForEachFunc;
void map_foreachinarea_impl(MapForEachFunc, int32_t, int32_t, int32_t, int32_t, int32_t, BlockType);
template<class... Args>
void map_foreachinarea(void (&func)(BlockList *, Args...), int32_t m,
                       int32_t x_0, int32_t y_0, int32_t x_1, int32_t y_1, BlockType type,
                       Args... args)
{
    map_foreachinarea_impl(std::bind(func, std::placeholders::_1, args...),
                           m, x_0, y_0, x_1, y_1, type);
}

void map_foreachinmovearea_impl(MapForEachFunc, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, BlockType);
template<class... Args>
void map_foreachinmovearea(void (&func)(BlockList *, Args...), int32_t m,
                           int32_t x_0, int32_t y_0, int32_t x_1, int32_t y_1, int32_t dx, int32_t dy,
                           BlockType type, Args... args)
{
    map_foreachinmovearea_impl(std::bind(func, std::placeholders::_1, args...),
                           m, x_0, y_0, x_1, y_1, dx, dy, type);
}

/// Temporary objects (loot, etc)
typedef uint32_t obj_id_t;
obj_id_t map_addobject(BlockList *);
void map_delobject(obj_id_t, BlockType type);

void map_foreachobject_impl(MapForEachFunc);
template<class... Args>
void map_foreachobject(void (&func)(BlockList *, Args...), Args... args)
{
    map_foreachobject_impl(std::bind(func, std::placeholders::_1, args...));
}

void map_quit(MapSessionData *);
// npc
int32_t map_addnpc(int32_t, struct npc_data *);

extern FILE *map_logfile;
void map_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

#define MAP_LOG_PC(sd, fmt, args...) map_log("PC%d %d:%d,%d " fmt, sd->status.char_id, sd->m, sd->x, sd->y, ## args)

// floor item methods
void map_clearflooritem_timer(timer_id, tick_t, uint32_t);
inline void map_clearflooritem(uint32_t id)
{
    map_clearflooritem_timer(NULL, 0, id);
}
int32_t map_addflooritem_any(struct item *, int32_t amount, uint16_t m, uint16_t x, uint16_t y,
                           MapSessionData **owners,
                           int32_t *owner_protection,
                          int32_t lifetime, int32_t dispersal);
int32_t map_addflooritem(struct item *, int32_t amount, uint16_t m, uint16_t x, uint16_t y,
                       MapSessionData *, MapSessionData *,
                      MapSessionData *);

// mappings between character id and names
void map_addchariddb(charid_t charid, const char *name);
const char *map_charid2nick(charid_t) __attribute__((pure));

MapSessionData *map_id2sd(uint32_t) __attribute__((pure));
MapSessionData *map_id2authsd(uint32_t) __attribute__((pure));
BlockList *map_id2bl(uint32_t) __attribute__((pure));
int32_t map_mapname2mapid(const fixed_string<16>&) __attribute__((pure));
bool map_mapname2ipport(const fixed_string<16>&, IP_Address *, in_port_t *);
bool map_setipport(const fixed_string<16>& name, IP_Address ip, in_port_t port);

void map_addiddb(BlockList *);
void map_deliddb(BlockList *bl);
void map_foreachiddb(DB_Func func);

void map_addnickdb(MapSessionData *);
int32_t map_scriptcont(MapSessionData *sd, int32_t id);  /* Continues a script either on a spell or on an NPC */
MapSessionData *map_nick2sd(const char *) __attribute__((pure));
int32_t compare_item(struct item *a, struct item *b) __attribute__((pure));

// iterate over players
MapSessionData *map_get_first_session(void) __attribute__((pure));
MapSessionData *map_get_last_session(void) __attribute__((pure));
MapSessionData *map_get_next_session(MapSessionData *current) __attribute__((pure));
MapSessionData *map_get_prev_session(MapSessionData *current) __attribute__((pure));

// edit the gat data
uint8_t map_getcell(int32_t, int32_t, int32_t) __attribute__((pure));
void map_setcell(int32_t, int32_t, int32_t, uint8_t);

// get the general direction from block's location to the coordinates
Direction map_calc_dir(BlockList *src, int32_t x, int32_t y);

#endif // MAP_HPP
