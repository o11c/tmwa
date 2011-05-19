#ifndef CHAR_H
#define CHAR_H
# include <arpa/inet.h>

# include "../common/mmo.hpp"
# include "../common/sanity.hpp"

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
    in_addr_t ip;
    in_port_t port;
    int users;
    /// each map name is max 16 characters
    char map[MAX_MAP_PER_SERVER][16];
};

struct mmo_charstatus *character_by_name(const char *character_name);
const char *get_character_name(int index) __attribute__((deprecated));

# define mapif_sendall(buf, len) mapif_sendallwos(-1, buf, len)
void mapif_sendallwos(int fd, const uint8_t *buf, unsigned int len);
void mapif_send(int fd, const uint8_t *buf, unsigned int len);

void char_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

extern int autosave_interval;

#endif
