#ifndef PATH_HPP
#define PATH_HPP

#include "path.structs.hpp"

sint32 path_search(struct walkpath_data *, sint32, sint32, sint32, sint32, sint32, sint32);
sint32 path_blownpos(sint32 m, sint32 x_0, sint32 y_0, sint32 dx, sint32 dy, sint32 count);

#endif // PATH_HPP
