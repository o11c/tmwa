#ifndef PARTY_HPP
#define PARTY_HPP

# include "../common/mmo.hpp"

# include "map.structs.hpp"

void do_init_party(void);
struct party *party_search(int party_id);
struct party *party_searchname(const char *str);

int party_create(MapSessionData *sd, const char *name);
int party_created(int account_id, int fail, int party_id, const char *name);
void party_request_info(int party_id);
int party_invite(MapSessionData *sd, int account_id);
int party_member_added(int party_id, int account_id, int flag);
int party_leave(MapSessionData *sd);
int party_removemember(MapSessionData *sd, int account_id,
                       const char *name);
int party_member_left(int party_id, int account_id, const char *name);
int party_reply_invite(MapSessionData *sd, int account_id,
                        int flag);
int party_recv_noinfo(int party_id);
int party_recv_info(const struct party *sp);
int party_recv_movemap(int party_id, int account_id, const char *map, int online,
                        int lv);
int party_broken(int party_id);
int party_optionchanged(int party_id, int account_id, int exp, int item,
                         int flag);
int party_changeoption(MapSessionData *sd, int exp, int item);

int party_send_movemap(MapSessionData *sd);
int party_send_logout(MapSessionData *sd);

int party_send_message(MapSessionData *sd, char *mes, int len);
int party_recv_message(int party_id, int account_id, const char *mes, int len);

void party_send_hp_check(BlockList *bl, party_t, bool *);

int party_exp_share(struct party *p, int map, int base_exp, int job_exp);

#endif // PARTY_HPP
