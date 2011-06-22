#ifndef CHAR_H
#define CHAR_H
# include <arpa/inet.h>

# include "../common/mmo.hpp"
# include "../common/sanity.hpp"

# include "../lib/fixed_string.hpp"
# include "../lib/ip.hpp"
# include "../lib/log.hpp"

# define CHAR_CONF_NAME "conf/char_athena.conf"
# define LOGIN_LAN_CONF_NAME "conf/lan_support.conf"

# define MAX_MAP_SERVERS 30
# define DEFAULT_AUTOSAVE_INTERVAL 300*1000
# define MAX_CHARS_PER_ACCOUNT 9

enum gender
{
    MALE,
    FEMALE
};

struct mmo_map_server
{
    IP_Address ip;
    in_port_t port;
    int users;
    fixed_string<16> map[MAX_MAP_PER_SERVER];
};

struct mmo_charstatus *character_by_name(const char *character_name);
const char *get_character_name(int index) __attribute__((deprecated));

# define mapif_sendall(buf, len) mapif_sendallwos(-1, buf, len)
void mapif_sendallwos(int fd, const uint8_t *buf, unsigned int len);
void mapif_send(int fd, const uint8_t *buf, unsigned int len);

extern Log char_log;

extern int autosave_interval;

#endif
