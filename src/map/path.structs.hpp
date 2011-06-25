#ifndef PATH_STRUCTS
#define PATH_STRUCTS

# include "battle.structs.hpp"

# define MAX_WALKPATH 48

struct walkpath_data
{
    uint8_t path_len, path_pos, path_half;
    Direction path[MAX_WALKPATH];
};

#endif // PATH_STRUCTS
