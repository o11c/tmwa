#include "nullpo.hpp"

#include <cstdio>

static void nullpo_info(const char *file, sint32 line, const char *func)
{
    if (!file)
        file = "??";
    if (!func || !*func)
        func = "unknown";

    fprintf(stderr, "%s:%d: in func `%s': NULL pointer\n", file, line, func);
}

bool nullpo_chk(const char *file, sint32 line, const char *func, const void *target)
{
    if (target)
        return 0;

    nullpo_info(file, line, func);
    return 1;
}
