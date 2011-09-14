#ifndef PARTY_HPP
#define PARTY_HPP

# include "../common/mmo.hpp"

# include "map.structs.hpp"

void do_init_party(void);
struct party *party_search(int32_t party_id) __attribute__((pure));
struct party *party_searchname(const char *str);

int32_t party_create(MapSessionData *sd, const char *name);
int32_t party_created(int32_t account_id, int32_t fail, int32_t party_id, const char *name);
void party_request_info(int32_t party_id);
int32_t party_invite(MapSessionData *sd, int32_t account_id);
int32_t party_member_added(int32_t party_id, int32_t account_id, int32_t flag);
int32_t party_leave(MapSessionData *sd);
int32_t party_removemember(MapSessionData *sd, int32_t account_id,
                       const char *name);
int32_t party_member_left(int32_t party_id, int32_t account_id, const char *name);
int32_t party_reply_invite(MapSessionData *sd, int32_t account_id,
                        int32_t flag);
int32_t party_recv_noinfo(int32_t party_id);
int32_t party_recv_info(const struct party *sp);
int32_t party_recv_movemap(int32_t party_id, int32_t account_id, const char *map, int32_t online,
                        int32_t lv);
int32_t party_broken(int32_t party_id);
int32_t party_optionchanged(int32_t party_id, int32_t account_id, int32_t exp, int32_t item,
                         int32_t flag);
int32_t party_changeoption(MapSessionData *sd, int32_t exp, int32_t item);

int32_t party_send_movemap(MapSessionData *sd);
int32_t party_send_logout(MapSessionData *sd);

int32_t party_send_message(MapSessionData *sd, char *mes, int32_t len);
int32_t party_recv_message(int32_t party_id, int32_t account_id, const char *mes, int32_t len);

void party_send_hp_check(BlockList *bl, party_t, bool *);

int32_t party_exp_share(struct party *p, int32_t map, int32_t base_exp, int32_t job_exp);

#endif // PARTY_HPP
