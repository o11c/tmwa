#ifndef MOB_HPP
#define MOB_HPP

#include "mob.structs.hpp"

#include "map.structs.hpp"
#include "clif.structs.hpp"

#include "../common/mmo.hpp"
#include "../common/timer.structs.hpp"

extern struct mob_db mob_db[];

int32_t mobdb_searchname(const char *str) __attribute__((pure));
int32_t mobdb_checkid(const int32_t id) __attribute__((pure));
int32_t mob_once_spawn(MapSessionData *sd, Point point, const char *mobname,
                   int32_t mob_class, int32_t amount, const char *event);
int32_t mob_once_spawn_area(MapSessionData *sd, const fixed_string<16>& mapname, int32_t x_0,
                          int32_t y_0, int32_t x_1, int32_t y_1, const char *mobname,
                         int32_t class_, int32_t amount, const char *event);

int32_t mob_target(struct mob_data *md, BlockList *bl, int32_t dist);
int32_t mob_stop_walking(struct mob_data *md, int32_t type);
int32_t mob_stopattack(struct mob_data *);
int32_t mob_spawn(int32_t);
int32_t mob_damage(BlockList *, struct mob_data *, int32_t, int32_t);
int32_t mob_heal(struct mob_data *, int32_t);
int32_t do_init_mob(void);

int32_t mob_delete(struct mob_data *md);
int32_t mob_catch_delete(struct mob_data *md);
void mob_timer_delete(timer_id, tick_t, int32_t);

int32_t mob_counttargeted(struct mob_data *md, BlockList *src,
                      AttackResult target_lv);

int32_t mob_warp(struct mob_data *md, int32_t m, int32_t x, int32_t y, BeingRemoveType type);

#endif // MOB_HPP
