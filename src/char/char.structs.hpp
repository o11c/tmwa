#ifndef CHAR_STRUCTS
#define CHAR_STRUCTS

# include "../common/mmo.hpp"

# include "../lib/ip.hpp"
# include "../lib/fixed_string.hpp"

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

#endif // CHAR_STRUCTS
