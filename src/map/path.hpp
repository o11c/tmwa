#ifndef PATH_HPP
#define PATH_HPP

#include "path.structs.hpp"

int path_search(struct walkpath_data *, int, int, int, int, int, int);
int path_blownpos(int m, int x_0, int y_0, int dx, int dy, int count);

#endif // PATH_HPP
