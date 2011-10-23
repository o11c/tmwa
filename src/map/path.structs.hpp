#ifndef PATH_STRUCTS
#define PATH_STRUCTS

# include "battle.structs.hpp"

# define MAX_WALKPATH 48

struct walkpath_data
{
    uint8 path_len, path_pos, path_half;
    Direction path[MAX_WALKPATH];
};

BIT_ENUM(MapCell, uint8_t)
{
    NONE = 0x00,

    SOLID = 0x01,

    NPC = 0x80,

    ALL = NPC | SOLID,
};

#endif // PATH_STRUCTS
