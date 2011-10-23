#ifndef LOCK_HPP
#define LOCK_HPP

#include <cstdio>

# include "../lib/ints.hpp"

/// Locked FILE I/O
// Changes are made in a separate file until lock_fclose
FILE *lock_fopen(const char *filename, sint32 *info);
void lock_fclose(FILE * fp, const char *filename, sint32 *info);

#endif // LOCK_HPP
