#ifndef TMW_HPP
#define TMW_HPP

#include "map.structs.hpp"

int32_t tmw_CheckChatSpam(MapSessionData *sd, const char *message);
void tmw_GmHackMsg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void tmw_TrimStr(char *str);

#endif // TMW_HPP
