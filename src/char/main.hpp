#ifndef MAIN_HPP
#define MAIN_HPP

# include "main.structs.hpp"

# include "../lib/log.hpp"

# define CHAR_CONF_NAME "conf/char_athena.conf"
# define LOGIN_LAN_CONF_NAME "conf/lan_support.conf"

# define MAX_MAP_SERVERS 30
# define MAX_CHARS_PER_ACCOUNT 9

struct mmo_charstatus *character_by_name(const char *character_name) __attribute__((pure));

void mapif_sendallwos(sint32 fd, const uint8 *buf, uint32 len);
inline void mapif_sendall(const uint8 *buf, uint32 len)
{
    mapif_sendallwos(-1, buf, len);
}
void mapif_send(sint32 fd, const uint8 *buf, uint32 len);

extern Log char_log;
#define LOG_DEBUG(...)  char_log.debug(STR_PRINTF(__VA_ARGS__))
#define LOG_CONF(...)   char_log.conf(STR_PRINTF(__VA_ARGS__))
#define LOG_INFO(...)   char_log.info(STR_PRINTF(__VA_ARGS__))
#define LOG_WARN(...)   char_log.warn(STR_PRINTF(__VA_ARGS__))
#define LOG_ERROR(...)  char_log.error(STR_PRINTF(__VA_ARGS__))
#define LOG_FATAL(...)  char_log.fatal(STR_PRINTF(__VA_ARGS__))

#endif //MAIN_HPP
