#ifndef TMW_HPP
#define TMW_HPP

#include "main.structs.hpp"

sint32 tmw_CheckChatSpam(MapSessionData *sd, const char *message);
void tmw_GmHackMsg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void tmw_TrimStr(char *str);

#endif // TMW_HPP
