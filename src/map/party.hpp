#ifndef PARTY_HPP
#define PARTY_HPP

# include "../common/mmo.hpp"

# include "main.structs.hpp"

void do_init_party(void);
struct party *party_search(party_t) __attribute__((pure));
struct party *party_searchname(const char *str) __attribute__((pure));

sint32 party_create(MapSessionData *sd, const char *name);
void party_created(account_t, bool fail, party_t, const char *name);
void party_request_info(party_t);
void party_invite(MapSessionData *sd, account_t);
void party_member_added(party_t, account_t, bool flag);
sint32 party_leave(MapSessionData *sd);
void party_removemember(MapSessionData *sd, account_t);
void party_member_left(party_t, account_t, const char *name);
void party_reply_invite(MapSessionData *, account_t, bool);
void party_recv_noinfo(party_t);
sint32 party_recv_info(const struct party *sp);
void party_recv_movemap(party_t, account_t, const char *map, bool online, level_t lv);
void party_broken(party_t);
void party_optionchanged(party_t, account_t, bool exp, bool item, uint8 flag);
void party_changeoption(MapSessionData *sd, bool exp, bool item);

void party_send_movemap(MapSessionData *sd);
void party_send_logout(MapSessionData *sd);

void party_send_message(MapSessionData *sd, char *mes, sint32 len);
void party_recv_message(party_t, account_t, const char *mes, sint32 len);

void party_send_hp_check(BlockList *bl, party_t, bool *);

sint32 party_exp_share(struct party *p, sint32 map, sint32 base_exp, sint32 job_exp);

#endif // PARTY_HPP
