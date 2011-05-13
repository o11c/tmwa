#ifndef INTIF_H
#define INTIF_H

#include "../common/mmo.hpp"

int  intif_parse (int fd);

int  intif_GMmessage (const char *mes, int len, int flag);

int  intif_wis_message (struct map_session_data *sd, char *nick, char *mes,
                        int mes_len);
int  intif_wis_message_to_gm (char *Wisp_name, int min_gm_level, char *mes,
                              int mes_len);

int  intif_saveaccountreg (struct map_session_data *sd);
int  intif_request_accountreg (struct map_session_data *sd);

int  intif_request_storage (int account_id);
int  intif_send_storage (struct storage *stor);

int  intif_create_party (struct map_session_data *sd, char *name);
int  intif_request_partyinfo (int party_id);
int  intif_party_addmember (int party_id, int account_id);
int  intif_party_changeoption (int party_id, int account_id, int exp,
                               int item);
int  intif_party_leave (int party_id, int accound_id);
int  intif_party_changemap (struct map_session_data *sd, int online);
int  intif_party_message (int party_id, int account_id, char *mes, int len);
int  intif_party_checkconflict (int party_id, int account_id, char *nick);

#endif // INTIF_H
