#ifndef CLIF_H
#define CLIF_H

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../common/socket.hpp"

#include "../lib/fixed_string.hpp"

#include "map.hpp"

#include <iterator>

/// What the client uses to connect to us
void clif_setip(const char *);
void clif_setport(in_port_t);
in_addr_t clif_getip(void);
in_port_t clif_getport(void);

unsigned int clif_countusers(void);
void clif_setwaitclose(int);

void clif_authok(MapSessionData *);
void clif_authfail_fd(int, int);
void clif_charselectok(int);
void clif_dropflooritem(struct flooritem_data *);
void clif_clearflooritem(struct flooritem_data *, int);

// these need better names
// the only one that is fully accurate is DEAD
enum class BeingRemoveType
{
    NEGATIVE = -1,
    ZERO = 0,
    DEAD = 1,
    QUIT = 2,
    WARP = 3,
    DISGUISE = 9,
};
class BlockList;

void clif_being_remove(BlockList *, BeingRemoveType);
void clif_being_remove_id(uint32_t, BeingRemoveType, int fd);
void clif_spawnpc(MapSessionData *);  //area
void clif_spawnnpc(struct npc_data *); // area
void clif_spawn_fake_npc_for_player(MapSessionData *sd, int fake_npc_id);
void clif_spawnmob(struct mob_data *); // area
void clif_walkok(MapSessionData *);   // self
void clif_movechar(MapSessionData *); // area
void clif_movemob(struct mob_data *);  //area
void clif_changemap(MapSessionData *, const fixed_string<16> &, int, int);
void clif_changemapserver(MapSessionData *, const fixed_string<16>&, int, int, in_addr_t, in_port_t);  //self
void clif_fixpos(BlockList *); // area
void clif_fixmobpos(struct mob_data *md);
void clif_fixpcpos(MapSessionData *sd);
void clif_npcbuysell(MapSessionData *, int);  //self
void clif_buylist(MapSessionData *, struct npc_data_shop *);   //self
void clif_selllist(MapSessionData *); //self
void clif_scriptmes(MapSessionData *, int, const char *);   //self
void clif_scriptnext(MapSessionData *, int);  //self
void clif_scriptclose(MapSessionData *, int); //self
void clif_scriptmenu(MapSessionData *, int, const char *);  //self
void clif_scriptinput(MapSessionData *, int); //self
void clif_scriptinputstr(MapSessionData *sd, int npcid);  // self
void clif_additem(MapSessionData *, int, int, int);   //self
void clif_delitem(MapSessionData *, int, int);    //self
void clif_updatestatus(MapSessionData *, int);    //self
void clif_damage(BlockList *, BlockList *, unsigned int, int, int, int, int, int, int);    // area
#define clif_takeitem(src,dst) clif_damage(src,dst,0,0,0,0,0,1,0)
void clif_changelook(BlockList *, int, int);   // area
void clif_changelook_accessories(BlockList *bl, MapSessionData *dst); // area or target; list gloves, boots etc.
void clif_arrowequip(MapSessionData *sd, int val);    //self
void clif_arrow_fail(MapSessionData *sd, int type);   //self
void clif_statusupack(MapSessionData *, int, int, int);   // self
void clif_equipitemack(MapSessionData *, int, int, int);  // self
void clif_unequipitemack(MapSessionData *, int, int, int);    // self
void clif_misceffect(BlockList *, int);    // area
void clif_changeoption(BlockList *);   // area
void clif_useitemack(MapSessionData *, int, int, int);    // self

void clif_emotion(BlockList *bl, int type);
void clif_wedding_effect(BlockList *bl);
void clif_soundeffect(MapSessionData *sd, BlockList *bl,
                      const char *name, int type);

// trade
void clif_traderequest(MapSessionData *sd, char *name);
void clif_tradestart(MapSessionData *sd, int type);
void clif_tradeadditem(MapSessionData *sd,
                       MapSessionData *tsd, int index, int amount);
void clif_tradeitemok(MapSessionData *sd, int index, int amount,
                      int fail);
void clif_tradedeal_lock(MapSessionData *sd, int fail);
void clif_tradecancelled(MapSessionData *sd);
void clif_tradecompleted(MapSessionData *sd, int fail);

// storage
#include "storage.hpp"
void clif_storageitemlist(MapSessionData *sd, struct storage *stor);
void clif_storageequiplist(MapSessionData *sd,
                           struct storage *stor);
void clif_updatestorageamount(MapSessionData *sd,
                              struct storage *stor);
void clif_storageitemadded(MapSessionData *sd, struct storage *stor,
                           int index, int amount);
void clif_storageitemremoved(MapSessionData *sd, int index,
                             int amount);
void clif_storageclose(MapSessionData *sd);

// map_forallinmovearea callbacks
void clif_pcinsight(BlockList *, MapSessionData *);
void clif_pcoutsight(BlockList *, MapSessionData *);
void clif_mobinsight(BlockList *, struct mob_data *);
void clif_moboutsight(BlockList *, struct mob_data *);

void clif_skillinfoblock(MapSessionData *sd);
void clif_skillup(MapSessionData *sd, int skill_num);

void clif_changemapcell(int m, int x, int y, int cell_type, int type);

void clif_status_change(BlockList *bl, int type, int flag);

void clif_whisper_message(int fd, const char *nick, const char *mes, int mes_len);
void clif_whisper_end(int fd, int flag);

void clif_equiplist(MapSessionData *sd);

void clif_movetoattack(MapSessionData *sd, BlockList *bl);

// party
void clif_party_created(MapSessionData *sd, int flag);
void clif_party_info(struct party *p, int fd);
void clif_party_invite(MapSessionData *sd,
                       MapSessionData *tsd);
void clif_party_inviteack(MapSessionData *sd, char *nick, int flag);
void clif_party_option(struct party *p, MapSessionData *sd, int flag);
void clif_party_left(struct party *p, MapSessionData *sd,
                     account_t account_id, const char *name, int flag);
void clif_party_message(struct party *p, int account_id, const char *mes, int len);
void clif_party_move(struct party *p, MapSessionData *sd, bool online);
void clif_party_xy(struct party *p, MapSessionData *sd);
void clif_party_hp(struct party *p, MapSessionData *sd);

// atcommand
void clif_displaymessage(int fd, const char *mes);
void clif_disp_onlyself(MapSessionData *sd, char *mes, int len);
void clif_GMmessage(BlockList *bl, const char *mes, int len, int flag);
void clif_resurrection(BlockList *bl, int type);

// special effects
void clif_specialeffect(BlockList *bl, int type, int flag);
// messages (from mobs/npcs/@tee)
void clif_message(BlockList *bl, const char *msg);

void clif_GM_kick(MapSessionData *sd, MapSessionData *tsd, int type);

void do_init_clif (void);

template<bool auth_required>
class SessionIterator
{
    int i;
public:
    SessionIterator(int fd) : i(fd) {}
    SessionIterator& operator ++()
    {
        do
        {
            ++i;
        } while (i < fd_max
                && (!session[i]
                        || !session[i]->session_data
                        || (auth_required && !static_cast<MapSessionData *>(session[i]->session_data)->state.auth)
                    ));
        return *this;
    }
    MapSessionData* operator *() const
    {
        return static_cast<MapSessionData *>(session[i]->session_data);
    }
    bool operator != (const SessionIterator& o) const
    {
        return i != o.i;
    }
};

template<bool auth_required>
class Sessions
{
public:
    SessionIterator<auth_required> begin()
    {
        SessionIterator<auth_required> out(-1);
        ++out;
        return out;
    }
    SessionIterator<auth_required> end()
    {
        return SessionIterator<auth_required>(fd_max);
    }
};

extern Sessions<true> auth_sessions;
extern Sessions<false> all_sessions;

#endif // CLIF_H
