#ifndef CHRIF_H
#define CHRIF_H
#include "../common/mmo.hpp"

class MapSessionData;

void chrif_setuserid(const char *);
void chrif_setpasswd(const char *);
const char *chrif_getpasswd(void);

void chrif_setip(const char *);
void chrif_setport(in_port_t);

bool chrif_isconnect(void);

void chrif_authreq(MapSessionData *);
void chrif_save(MapSessionData *);
void chrif_charselectreq(MapSessionData *);

void chrif_changemapserver(MapSessionData *sd,
                           const char mapname[16], int x, int y,
                           in_addr_t ip, in_port_t port);

void chrif_changegm(int id, const char *pass, int len);
void chrif_changeemail(int id, const char *actual_email, const char *new_email);
enum class CharOperation
{
    BLOCK = 1,
    BAN = 2,
    UNBLOCK = 3,
    UNBAN = 4,
    CHANGE_SEX = 5
};
void chrif_char_ask_name(int id, const char *character_name, CharOperation operation_type,
                         int year = 0, int month = 0, int day = 0,
                         int hour = 0, int minute = 0, int second = 0);
void chrif_saveaccountreg2(MapSessionData *sd);
void chrif_send_divorce(int char_id);

void do_init_chrif (void);

#endif // CHRIF_H
