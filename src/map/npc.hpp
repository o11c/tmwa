#ifndef NPC_HPP
#define NPC_HPP

#include "../common/mmo.hpp"
#include "../common/timer.structs.hpp"

#define START_NPC_NUM 110000000

#define WARP_CLASS 45
#define INVISIBLE_CLASS 32767

int npc_event_dequeue(MapSessionData *sd);
int npc_event(MapSessionData *sd, const char *npcname, int);
int npc_command(MapSessionData *sd, const char *npcname, const char *command);
int npc_touch_areanpc(MapSessionData *, int, int, int);
int npc_click(MapSessionData *, int);
int npc_scriptcont(MapSessionData *, int);
int npc_buysellsel(MapSessionData *, int, int);
int npc_buylist(MapSessionData *, int, const unsigned short *);
int npc_selllist(MapSessionData *, int, const unsigned short *);
int npc_parse_warp(char *w1, const char *w2, char *w3, char *w4);

int npc_enable(const char *name, int flag);
struct npc_data *npc_name2id(const char *name) __attribute__((pure));

int npc_get_new_npc_id(void);

/**
 * Spawns and installs a talk-only NPC
 *
 * \param message The message to speak.  If message is NULL, the NPC will not do anything at all.
 */
// message is strdup'd within
struct npc_data *npc_spawn_text(int m, int x, int y, int class_, const char *name, const char *message);

void npc_addsrcfile(char *);
int do_init_npc(void);
int npc_event_do_oninit(void);
int npc_do_ontimer(int, MapSessionData *, bool);

int npc_event_doall_l(const char *name, int rid, int argc,
                       struct argrec *argv);
int npc_event_do_l(const char *name, int rid, int argc,
                    struct argrec *argv);
#define npc_event_doall(name) npc_event_doall_l(name, 0, 0, NULL)
#define npc_event_do(name) npc_event_do_l(name, 0, 0, NULL)

int npc_timerevent_start(struct npc_data_script *nd);
int npc_timerevent_stop(struct npc_data_script *nd);
int npc_gettimerevent_tick(struct npc_data_script *nd);
int npc_settimerevent_tick(struct npc_data_script *nd, int newtimer);

#endif // NPC_HPP
