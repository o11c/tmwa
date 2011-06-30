#ifndef MOB_HPP
#define MOB_HPP

#include "mob.structs.hpp"

#include "map.structs.hpp"
#include "clif.structs.hpp"

#include "../common/mmo.hpp"
#include "../common/timer.structs.hpp"

extern struct mob_db mob_db[];

int mobdb_searchname(const char *str) __attribute__((pure));
int mobdb_checkid(const int id) __attribute__((pure));
int mob_once_spawn(MapSessionData *sd, const fixed_string<16>& mapname,
                     int x, int y, const char *mobname, int class_, int amount,
                    const char *event);
int mob_once_spawn_area(MapSessionData *sd, const fixed_string<16>& mapname, int x_0,
                          int y_0, int x_1, int y_1, const char *mobname,
                         int class_, int amount, const char *event);

int mob_target(struct mob_data *md, BlockList *bl, int dist);
int mob_stop_walking(struct mob_data *md, int type);
int mob_stopattack(struct mob_data *);
int mob_spawn(int);
int mob_damage(BlockList *, struct mob_data *, int, int);
int mob_heal(struct mob_data *, int);
int do_init_mob(void);

int mob_delete(struct mob_data *md);
int mob_catch_delete(struct mob_data *md);
void mob_timer_delete(timer_id, tick_t, int);

int mob_counttargeted(struct mob_data *md, BlockList *src,
                      AttackResult target_lv);

int mob_warp(struct mob_data *md, int m, int x, int y, BeingRemoveType type);

#endif // MOB_HPP
