#ifndef TMW_H
#define TMW_H

#include "map.hpp"

int tmw_CheckChatSpam(MapSessionData *sd, const char *message);
void tmw_GmHackMsg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void tmw_TrimStr(char *str);

#endif // TMW_H
