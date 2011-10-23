#ifndef CHRIF_HPP
#define CHRIF_HPP

#include "../lib/ip.hpp"

#include "../common/mmo.hpp"

#include "main.structs.hpp"

void chrif_setuserid(const char *);
void chrif_setpasswd(const char *);
const char *chrif_getpasswd(void) __attribute__((const));

void chrif_setip(IP_Address);
void chrif_setport(in_port_t);

bool chrif_isconnect(void) __attribute__((pure));

void chrif_authreq(MapSessionData *);
void chrif_save(MapSessionData *);
void chrif_charselectreq(MapSessionData *);

void chrif_changemapserver(MapSessionData *, const Point&, IP_Address ip, in_port_t port);

void chrif_changegm(account_t id, const char *pass, sint32 len);
void chrif_changeemail(account_t id, const char *actual_email, const char *new_email);
enum class CharOperation
{
    BLOCK = 1,
    BAN = 2,
    UNBLOCK = 3,
    UNBAN = 4,
    CHANGE_SEX = 5
};
void chrif_char_ask_name(account_t id, const char *character_name, CharOperation operation_type,
                         sint32 year = 0, sint32 month = 0, sint32 day = 0,
                         sint32 hour = 0, sint32 minute = 0, sint32 second = 0);
void chrif_saveaccountreg2(MapSessionData *sd);
void chrif_send_divorce(charid_t char_id);

void do_init_chrif (void);


void intif_GMmessage(const char *mes, sint32 len);

void intif_whisper_message(MapSessionData *sd, const char *nick,
                           const char *mes, sint32 mes_len);
void intif_whisper_message_to_gm(const char *whisper_name, gm_level_t min_gm_level,
                                 const char *mes, sint32 mes_len);

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
void intif_party_message(party_t party_id, account_t account_id, const char *mes, sint32 len);
void intif_party_checkconflict(party_t party_id, account_t account_id, const char *nick);

#endif // CHRIF_HPP
