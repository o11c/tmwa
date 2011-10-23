#ifndef MAIN_STRUCTS
#define MAIN_STRUCTS

# include "../common/mmo.hpp"

# include "../lib/ip.hpp"
# include "../lib/fixed_string.hpp"

enum class Gender
{
    MALE,
    FEMALE
};

struct mmo_map_server
{
    IP_Address ip;
    in_port_t port;
    sint32 users;
    fixed_string<16> map[MAX_MAP_PER_SERVER];
};

#endif // MAIN_STRUCTS
