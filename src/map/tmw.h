#ifndef TMW_H
#define TMW_H

#include "map.h"

int  tmw_CheckChatSpam (struct map_session_data *sd, const char *message);
int  tmw_ShorterStrlen (const char *s1, const char *s2);
int  tmw_CheckChatLameness (const char *message);
void tmw_GmHackMsg (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void tmw_AutoBan (struct map_session_data *sd, const char *reason, int length);
void tmw_TrimStr (char *str);

#endif // TMW_H
