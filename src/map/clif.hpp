#ifndef CLIF_H
#define CLIF_H

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../common/socket.hpp"

#include "map.hpp"

#include <iterator>

/// What the client uses to connect to us
void clif_setip(const char *);
void clif_setport(in_port_t);
in_addr_t clif_getip(void);
in_port_t clif_getport(void);

unsigned int clif_countusers(void);
void clif_setwaitclose(int);

void clif_authok(struct map_session_data *);
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
void clif_being_remove(struct block_list *, BeingRemoveType);
void clif_being_remove_delay(tick_t, struct block_list *, BeingRemoveType);
void clif_being_remove_id(int, int, int);
void clif_spawnpc(struct map_session_data *);  //area
void clif_spawnnpc(struct npc_data *); // area
void clif_spawn_fake_npc_for_player(struct map_session_data *sd, int fake_npc_id);
void clif_spawnmob(struct mob_data *); // area
void clif_walkok(struct map_session_data *);   // self
void clif_movechar(struct map_session_data *); // area
void clif_movemob(struct mob_data *);  //area
void clif_changemap(struct map_session_data *, const char *, int, int);  //self
void clif_changemapserver(struct map_session_data *, const char *, int, int, int, int);  //self
void clif_fixpos(struct block_list *); // area
void clif_fixmobpos(struct mob_data *md);
void clif_fixpcpos(struct map_session_data *sd);
void clif_npcbuysell(struct map_session_data *, int);  //self
void clif_buylist(struct map_session_data *, struct npc_data *);   //self
void clif_selllist(struct map_session_data *); //self
void clif_scriptmes(struct map_session_data *, int, const char *);   //self
void clif_scriptnext(struct map_session_data *, int);  //self
void clif_scriptclose(struct map_session_data *, int); //self
void clif_scriptmenu(struct map_session_data *, int, const char *);  //self
void clif_scriptinput(struct map_session_data *, int); //self
void clif_scriptinputstr(struct map_session_data *sd, int npcid);  // self
void clif_additem(struct map_session_data *, int, int, int);   //self
void clif_delitem(struct map_session_data *, int, int);    //self
void clif_updatestatus(struct map_session_data *, int);    //self
void clif_damage(struct block_list *, struct block_list *, unsigned int, int, int, int, int, int, int);    // area
#define clif_takeitem(src,dst) clif_damage(src,dst,0,0,0,0,0,1,0)
void clif_changelook(struct block_list *, int, int);   // area
void clif_changelook_accessories(struct block_list *bl, struct map_session_data *dst); // area or target; list gloves, boots etc.
void clif_arrowequip(struct map_session_data *sd, int val);    //self
void clif_arrow_fail(struct map_session_data *sd, int type);   //self
void clif_statusupack(struct map_session_data *, int, int, int);   // self
void clif_equipitemack(struct map_session_data *, int, int, int);  // self
void clif_unequipitemack(struct map_session_data *, int, int, int);    // self
void clif_misceffect(struct block_list *, int);    // area
void clif_changeoption(struct block_list *);   // area
void clif_useitemack(struct map_session_data *, int, int, int);    // self

void clif_emotion(struct block_list *bl, int type);
void clif_wedding_effect(struct block_list *bl);
void clif_soundeffect(struct map_session_data *sd, struct block_list *bl,
                      const char *name, int type);

// trade
void clif_traderequest(struct map_session_data *sd, char *name);
void clif_tradestart(struct map_session_data *sd, int type);
void clif_tradeadditem(struct map_session_data *sd,
                       struct map_session_data *tsd, int index, int amount);
void clif_tradeitemok(struct map_session_data *sd, int index, int amount,
                      int fail);
void clif_tradedeal_lock(struct map_session_data *sd, int fail);
void clif_tradecancelled(struct map_session_data *sd);
void clif_tradecompleted(struct map_session_data *sd, int fail);

// storage
#include "storage.hpp"
void clif_storageitemlist(struct map_session_data *sd, struct storage *stor);
void clif_storageequiplist(struct map_session_data *sd,
                           struct storage *stor);
void clif_updatestorageamount(struct map_session_data *sd,
                              struct storage *stor);
void clif_storageitemadded(struct map_session_data *sd, struct storage *stor,
                           int index, int amount);
void clif_storageitemremoved(struct map_session_data *sd, int index,
                             int amount);
void clif_storageclose(struct map_session_data *sd);

// map_forallinmovearea callbacks
void clif_pcinsight(struct block_list *, struct map_session_data *);
void clif_pcoutsight(struct block_list *, struct map_session_data *);
void clif_mobinsight(struct block_list *, struct mob_data *);
void clif_moboutsight(struct block_list *, struct mob_data *);

void clif_skillinfoblock(struct map_session_data *sd);
void clif_skillup(struct map_session_data *sd, int skill_num);

void clif_changemapcell(int m, int x, int y, int cell_type, int type);

void clif_status_change(struct block_list *bl, int type, int flag);

void clif_whisper_message(int fd, const char *nick, const char *mes, int mes_len);
void clif_whisper_end(int fd, int flag);

void clif_equiplist(struct map_session_data *sd);

void clif_movetoattack(struct map_session_data *sd, struct block_list *bl);

// party
void clif_party_created(struct map_session_data *sd, int flag);
void clif_party_info(struct party *p, int fd);
void clif_party_invite(struct map_session_data *sd,
                       struct map_session_data *tsd);
void clif_party_inviteack(struct map_session_data *sd, char *nick, int flag);
void clif_party_option(struct party *p, struct map_session_data *sd, int flag);
void clif_party_left(struct party *p, struct map_session_data *sd,
                     account_t account_id, const char *name, int flag);
void clif_party_message(struct party *p, int account_id, const char *mes, int len);
void clif_party_move(struct party *p, struct map_session_data *sd, bool online);
void clif_party_xy(struct party *p, struct map_session_data *sd);
void clif_party_hp(struct party *p, struct map_session_data *sd);

// atcommand
void clif_displaymessage(int fd, const char *mes);
void clif_disp_onlyself(struct map_session_data *sd, char *mes, int len);
void clif_GMmessage(struct block_list *bl, const char *mes, int len, int flag);
void clif_resurrection(struct block_list *bl, int type);

// special effects
void clif_specialeffect(struct block_list *bl, int type, int flag);
// messages (from mobs/npcs/@tee)
void clif_message(struct block_list *bl, const char *msg);

void clif_GM_kick(struct map_session_data *sd, struct map_session_data *tsd, int type);

void do_init_clif (void);

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
                        || !reinterpret_cast<struct map_session_data *>(session[i]->session_data)->state.auth
                    ));
        return *this;
    }
    struct map_session_data* operator *() const
    {
        return reinterpret_cast<struct map_session_data *>(session[i]->session_data);
    }
    bool operator != (const SessionIterator& o) const
    {
        return i != o.i;
    }
};

class Sessions
{
public:
    SessionIterator begin()
    {
        SessionIterator out(-1);
        ++out;
        return out;
    }
    SessionIterator end()
    {
        return SessionIterator(fd_max);
    }
};

extern Sessions sessions;

#endif // CLIF_H
