#ifndef CLIF_HPP
#define CLIF_HPP

# include "clif.structs.hpp"

# include "map.structs.hpp"

# include "../common/socket.hpp"

# include "../lib/ip.hpp"

# include <iterator>

/// What the client uses to connect to us
void clif_setip(IP_Address);
void clif_setport(in_port_t);
IP_Address clif_getip(void) __attribute__((pure));
in_port_t clif_getport(void) __attribute__((pure));

uint32_t clif_countusers(void) __attribute__((pure));
void clif_setwaitclose(int32_t);

void clif_authok(MapSessionData *);
void clif_authfail_fd(int32_t, int32_t);
void clif_charselectok(int32_t);
void clif_dropflooritem(struct flooritem_data *);
void clif_clearflooritem(struct flooritem_data *, int32_t);

void clif_being_remove(BlockList *, BeingRemoveType);
void clif_being_remove_id(uint32_t, BeingRemoveType, int32_t fd);
void clif_spawnpc(MapSessionData *);  //area
void clif_spawnnpc(struct npc_data *); // area
void clif_spawn_fake_npc_for_player(MapSessionData *sd, int32_t fake_npc_id);
void clif_spawnmob(struct mob_data *); // area
void clif_walkok(MapSessionData *);   // self
void clif_movechar(MapSessionData *); // area
void clif_movemob(struct mob_data *);  //area
void clif_changemap(MapSessionData *, const Point&);
void clif_changemapserver(MapSessionData *, const Point&, IP_Address, in_port_t);  //self
void clif_stop(MapSessionData *); // area
void clif_fixmobpos(struct mob_data *md);
void clif_fixpcpos(MapSessionData *sd);
void clif_npcbuysell(MapSessionData *, int32_t);  //self
void clif_buylist(MapSessionData *, struct npc_data_shop *);   //self
void clif_selllist(MapSessionData *); //self
void clif_scriptmes(MapSessionData *, int32_t, const char *);   //self
void clif_scriptnext(MapSessionData *, int32_t);  //self
void clif_scriptclose(MapSessionData *, int32_t); //self
void clif_scriptmenu(MapSessionData *, int32_t, const std::vector<std::string>&);  //self
void clif_scriptinput(MapSessionData *, int32_t); //self
void clif_scriptinputstr(MapSessionData *sd, int32_t npcid);  // self
void clif_additem(MapSessionData *, int32_t, int32_t, PickupFail);   //self
void clif_delitem(MapSessionData *, int32_t, int32_t);    //self
void clif_updatestatus(MapSessionData *, SP);    //self
void clif_damage(BlockList *, BlockList *, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);    // area
#define clif_takeitem(src,dst) clif_damage(src,dst,0,0,0,0,0,1,0)
void clif_changelook(BlockList *, LOOK, int32_t);   // area
void clif_changelook_accessories(BlockList *bl, MapSessionData *dst); // area or target; list gloves, boots etc.
void clif_arrowequip(MapSessionData *sd, int32_t val);    //self
void clif_arrow_fail(MapSessionData *sd, ArrowFail type);   //self
void clif_statusupack(MapSessionData *, SP, bool, int32_t);   // self
void clif_equipitemack(MapSessionData *, int32_t, EPOS, bool);  // self
void clif_unequipitemack(MapSessionData *, int32_t, EPOS, bool);    // self
void clif_misceffect(BlockList *, int32_t);    // area
void clif_changeoption(BlockList *);   // area
void clif_useitemack(MapSessionData *, int32_t, int32_t, int32_t);    // self

void clif_emotion(BlockList *bl, int32_t type);
void clif_wedding_effect(BlockList *bl);
void clif_soundeffect(MapSessionData *sd, BlockList *bl,
                      const char *name, int32_t type);

// trade
void clif_traderequest(MapSessionData *sd, char *name);
void clif_tradestart(MapSessionData *sd, int32_t type);
void clif_tradeadditem(MapSessionData *sd,
                       MapSessionData *tsd, int32_t index, int32_t amount);
void clif_tradeitemok(MapSessionData *sd, int32_t index, int32_t amount,
                      int32_t fail);
void clif_tradedeal_lock(MapSessionData *sd, int32_t fail);
void clif_tradecancelled(MapSessionData *sd);
void clif_tradecompleted(MapSessionData *sd, int32_t fail);

// storage
void clif_storageitemlist(MapSessionData *sd, struct storage *stor);
void clif_storageequiplist(MapSessionData *sd,
                           struct storage *stor);
void clif_updatestorageamount(MapSessionData *sd,
                              struct storage *stor);
void clif_storageitemadded(MapSessionData *sd, struct storage *stor,
                           int32_t index, int32_t amount);
void clif_storageitemremoved(MapSessionData *sd, int32_t index,
                             int32_t amount);
void clif_storageclose(MapSessionData *sd);

// map_forallinmovearea callbacks
void clif_pcinsight(BlockList *, MapSessionData *);
void clif_pcoutsight(BlockList *, MapSessionData *);
void clif_mobinsight(BlockList *, struct mob_data *);
void clif_moboutsight(BlockList *, struct mob_data *);

void clif_skillinfoblock(MapSessionData *sd);
void clif_skillup(MapSessionData *sd, int32_t skill_num);

void clif_changemapcell(int32_t m, int32_t x, int32_t y, int32_t cell_type, int32_t type);

void clif_status_change(BlockList *bl, int32_t type, int32_t flag);

void clif_whisper_message(int32_t fd, const char *nick, const char *mes, int32_t mes_len);
void clif_whisper_end(int32_t fd, int32_t flag);

void clif_equiplist(MapSessionData *sd);

void clif_movetoattack(MapSessionData *sd, BlockList *bl);

// party
void clif_party_created(MapSessionData *sd, int32_t flag);
void clif_party_info(struct party *p, int32_t fd);
void clif_party_invite(MapSessionData *sd,
                       MapSessionData *tsd);
void clif_party_inviteack(MapSessionData *sd, char *nick, int32_t flag);
void clif_party_option(struct party *p, MapSessionData *sd, int32_t flag);
void clif_party_left(struct party *p, MapSessionData *sd,
                     account_t account_id, const char *name, int32_t flag);
void clif_party_message(struct party *p, int32_t account_id, const char *mes, int32_t len);
void clif_party_move(struct party *p, MapSessionData *sd, bool online);
void clif_party_xy(struct party *p, MapSessionData *sd);
void clif_party_hp(struct party *p, MapSessionData *sd);

// atcommand
void clif_displaymessage(int32_t fd, const char *mes);
void clif_disp_onlyself(MapSessionData *sd, char *mes, int32_t len);
void clif_GMmessage(BlockList *bl, const char *mes, int32_t len, int32_t flag);
void clif_resurrection(BlockList *bl, int32_t type);

// special effects
void clif_specialeffect(BlockList *bl, int32_t type, int32_t flag);
// messages (from mobs/npcs/@tee)
void clif_message(BlockList *bl, const char *msg);

void clif_GM_kick(MapSessionData *sd, MapSessionData *tsd, int32_t type);

void do_init_clif (void);

template<bool auth_required>
class SessionIterator
{
    int32_t i;
public:
    SessionIterator(int32_t fd) : i(fd) {}
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
    SessionIterator<auth_required> begin() __attribute__((pure));
    SessionIterator<auth_required> end() __attribute__((pure));
};
template<bool auth_required>
SessionIterator<auth_required> Sessions<auth_required>::begin()
{
    SessionIterator<auth_required> out(-1);
    ++out;
    return out;
}
template<bool auth_required>
SessionIterator<auth_required> Sessions<auth_required>::end()
{
    return SessionIterator<auth_required>(fd_max);
}

extern Sessions<true> auth_sessions;
extern Sessions<false> all_sessions;

#endif // CLIF_HPP
