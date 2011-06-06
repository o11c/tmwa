#ifndef INTIF_H
#define INTIF_H

#include "../common/mmo.hpp"

void intif_GMmessage(const char *mes, int len);

void intif_whisper_message(MapSessionData *sd, const char *nick,
                           const char *mes, int mes_len);
void intif_whisper_message_to_gm(const char *whisper_name, gm_level_t min_gm_level,
                                 const char *mes, int mes_len);

void intif_saveaccountreg(MapSessionData *sd);
void intif_request_accountreg(MapSessionData *sd);

void intif_request_storage(account_t account_id);
void intif_send_storage(struct storage *stor);

void intif_create_party(MapSessionData *sd, const char *name);
void intif_request_partyinfo(party_t party_id);
void intif_party_addmember(party_t party_id, account_t account_id);
void intif_party_changeoption(party_t party_id, account_t account_id, bool exp, bool item);
void intif_party_leave(party_t party_id, account_t accound_id);
void intif_party_changemap(MapSessionData *sd, bool online);
void intif_party_message(party_t party_id, account_t account_id, const char *mes, int len);
void intif_party_checkconflict(party_t party_id, account_t account_id, const char *nick);

#endif // INTIF_H
