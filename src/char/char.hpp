#ifndef CHAR_HPP
#define CHAR_HPP

# include "char.structs.hpp"

# include "../lib/log.hpp"

# define CHAR_CONF_NAME "conf/char_athena.conf"
# define LOGIN_LAN_CONF_NAME "conf/lan_support.conf"

# define MAX_MAP_SERVERS 30
# define DEFAULT_AUTOSAVE_INTERVAL 300*1000
# define MAX_CHARS_PER_ACCOUNT 9

struct mmo_charstatus *character_by_name(const char *character_name);
const char *get_character_name(int index) __attribute__((deprecated));

void mapif_sendallwos(int fd, const uint8_t *buf, unsigned int len);
inline void mapif_sendall(const uint8_t *buf, unsigned int len)
{
    mapif_sendallwos(-1, buf, len);
}
void mapif_send(int fd, const uint8_t *buf, unsigned int len);

extern Log char_log;

extern int autosave_interval;

#endif //CHAR_HPP
