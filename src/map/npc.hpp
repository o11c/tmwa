#ifndef NPC_HPP
#define NPC_HPP

#include "../common/mmo.hpp"
#include "../common/timer.structs.hpp"

#include "magic.structs.hpp"
#include "script.structs.hpp"

#define WARP_CLASS 45
#define INVISIBLE_CLASS 32767

sint32 npc_event_dequeue(MapSessionData *sd);
sint32 npc_event(MapSessionData *sd, const char *npcname, sint32);
sint32 npc_command(MapSessionData *sd, const char *npcname, const char *command);
sint32 npc_touch_areanpc(MapSessionData *, sint32, sint32, sint32);
sint32 npc_click(MapSessionData *, BlockID);
sint32 npc_scriptcont(MapSessionData *, BlockID);
enum class BuySell
{
    BUY,
    SELL,
};
bool npc_buysellsel(MapSessionData *, BlockID, BuySell);
sint32 npc_buylist(MapSessionData *, sint32, const uint16 *);
sint32 npc_selllist(MapSessionData *, sint32, const uint16 *);
sint32 npc_parse_warp(char *w1, const char *w2, char *w3, char *w4);

sint32 npc_enable(const char *name, sint32 flag);
struct npc_data *npc_name2id(const char *name) __attribute__((pure));

BlockID npc_get_new_npc_id(void);

/**
 * Spawns and installs a talk-only NPC
 *
 * \param message The message to speak.  If message is NULL, the NPC will not do anything at all.
 */
// message is strdup'd within
struct npc_data *npc_spawn_text(location_t loc, sint32 npc_class, const char *name, const char *message);

void npc_addsrcfile(char *);
sint32 do_init_npc(void);
sint32 npc_event_do_oninit(void);

sint32 npc_event_doall_l(const char *name, account_t rid, sint32 argc, ArgRec *argv);
sint32 npc_event_do_l(const char *name, account_t rid, sint32 argc, ArgRec *argv);
#define npc_event_doall(name) npc_event_doall_l(name, account_t(), 0, NULL)
#define npc_event_do(name) npc_event_do_l(name, account_t(), 0, NULL)

sint32 npc_timerevent_start(struct npc_data_script *nd);
sint32 npc_timerevent_stop(struct npc_data_script *nd);
interval_t npc_gettimerevent_tick(struct npc_data_script *nd);
void npc_settimerevent_tick(struct npc_data_script *nd, interval_t newtimer);

#endif // NPC_HPP
