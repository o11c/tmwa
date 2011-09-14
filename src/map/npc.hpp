#ifndef NPC_HPP
#define NPC_HPP

#include "../common/mmo.hpp"
#include "../common/timer.structs.hpp"

#include "magic.structs.hpp"
#include "script.structs.hpp"

#define START_NPC_NUM 110000000

#define WARP_CLASS 45
#define INVISIBLE_CLASS 32767

int32_t npc_event_dequeue(MapSessionData *sd);
int32_t npc_event(MapSessionData *sd, const char *npcname, int32_t);
int32_t npc_command(MapSessionData *sd, const char *npcname, const char *command);
int32_t npc_touch_areanpc(MapSessionData *, int32_t, int32_t, int32_t);
int32_t npc_click(MapSessionData *, int32_t);
int32_t npc_scriptcont(MapSessionData *, int32_t);
int32_t npc_buysellsel(MapSessionData *, int32_t, int32_t);
int32_t npc_buylist(MapSessionData *, int32_t, const uint16_t *);
int32_t npc_selllist(MapSessionData *, int32_t, const uint16_t *);
int32_t npc_parse_warp(char *w1, const char *w2, char *w3, char *w4);

int32_t npc_enable(const char *name, int32_t flag);
struct npc_data *npc_name2id(const char *name) __attribute__((pure));

int32_t npc_get_new_npc_id(void);

/**
 * Spawns and installs a talk-only NPC
 *
 * \param message The message to speak.  If message is NULL, the NPC will not do anything at all.
 */
// message is strdup'd within
struct npc_data *npc_spawn_text(location_t loc, int32_t npc_class, const char *name, const char *message);

void npc_addsrcfile(char *);
int32_t do_init_npc(void);
int32_t npc_event_do_oninit(void);
int32_t npc_do_ontimer(int32_t, MapSessionData *, bool);

int32_t npc_event_doall_l(const char *name, int32_t rid, int32_t argc, ArgRec *argv);
int32_t npc_event_do_l(const char *name, int32_t rid, int32_t argc, ArgRec *argv);
#define npc_event_doall(name) npc_event_doall_l(name, 0, 0, NULL)
#define npc_event_do(name) npc_event_do_l(name, 0, 0, NULL)

int32_t npc_timerevent_start(struct npc_data_script *nd);
int32_t npc_timerevent_stop(struct npc_data_script *nd);
int32_t npc_gettimerevent_tick(struct npc_data_script *nd);
int32_t npc_settimerevent_tick(struct npc_data_script *nd, int32_t newtimer);

#endif // NPC_HPP
