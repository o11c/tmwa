#ifndef MOB_HPP
#define MOB_HPP

#include "mob.structs.hpp"

#include "main.structs.hpp"
#include "clif.structs.hpp"

#include "../common/mmo.hpp"
#include "../common/timer.structs.hpp"

extern struct mob_db mob_db[];

sint32 mobdb_searchname(const char *str) __attribute__((pure));
sint32 mobdb_checkid(const sint32 id) __attribute__((pure));
BlockID mob_once_spawn(MapSessionData *sd, Point point, const char *mobname,
                       sint32 mob_class, sint32 amount, const char *event);
BlockID mob_once_spawn_area(MapSessionData *sd, const fixed_string<16>& mapname,
                            sint32 x_0, sint32 y_0, sint32 x_1, sint32 y_1,
                            const char *mobname, sint32 class_, sint32 amount,
                            const char *event);

sint32 mob_target(struct mob_data *md, BlockList *bl, sint32 dist);
sint32 mob_stop_walking(struct mob_data *md, sint32 type);
sint32 mob_stopattack(struct mob_data *);
sint32 mob_spawn(BlockID);
sint32 mob_damage(BlockList *, struct mob_data *, sint32, sint32);
sint32 mob_heal(struct mob_data *, sint32);
sint32 do_init_mob(void);

sint32 mob_delete(struct mob_data *md);
bool mob_catch_delete(struct mob_data *md);
void mob_timer_delete(timer_id, tick_t, BlockID);

sint32 mob_counttargeted(struct mob_data *md, BlockList *src,
                      AttackResult target_lv);

sint32 mob_warp(struct mob_data *md, sint32 m, sint32 x, sint32 y, BeingRemoveType type);

#endif // MOB_HPP
