#ifndef CHRIF_H
#define CHRIF_H
#include "../common/mmo.hpp"

void chrif_setuserid(char *);
void chrif_setpasswd(char *);
char *chrif_getpasswd(void);

void chrif_setip(char *);
void chrif_setport(int);

int chrif_isconnect(void);

int chrif_authreq(struct map_session_data *);
int chrif_save(struct map_session_data *);
int chrif_charselectreq(struct map_session_data *);

int chrif_changemapserver(struct map_session_data *sd, char *name, int x,
                           int y, int ip, short port);

int chrif_searchcharid(int char_id);
int chrif_changegm(int id, const char *pass, int len);
int chrif_changeemail(int id, const char *actual_email,
                       const char *new_email);
enum class CharOperation
{
    BLOCK = 1,
    BAN = 2,
    UNBLOCK = 3,
    UNBAN = 4,
    CHANGE_SEX = 5
};
int chrif_char_ask_name(int id, char *character_name, CharOperation operation_type,
                          int year = 0, int month = 0, int day = 0,
                         int hour = 0, int minute = 0, int second = 0);
int chrif_saveaccountreg2(struct map_session_data *sd);
int chrif_send_divorce(int char_id);

int do_init_chrif (void);

#endif // CHRIF_H
