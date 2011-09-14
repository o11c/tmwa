#ifndef PATH_HPP
#define PATH_HPP

#include "path.structs.hpp"

int32_t path_search(struct walkpath_data *, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t);
int32_t path_blownpos(int32_t m, int32_t x_0, int32_t y_0, int32_t dx, int32_t dy, int32_t count);

#endif // PATH_HPP
