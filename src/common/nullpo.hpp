#ifndef NULLPO_HPP
#define NULLPO_HPP
/// return wrappers for unexpected NULL pointers

#include "../lib/ints.hpp"

/// Comment this out to live dangerously
# define NULLPO_CHECK

/// All functions print to standard error (was: standard output)
/// nullpo_ret(cond) - return 0 if given pointer is NULL
/// nullpo_retv(cond) - just return (function returns void)
/// nullpo_retr(rv, cond) - return given value instead

# ifdef NULLPO_CHECK
#  define nullpo_retr(ret, t) \
    if (nullpo_chk(__FILE__, __LINE__, __func__, t)) \
        return ret;
# else // NULLPO_CHECK
#  define nullpo_retr(ret, t) t;
# endif // NULLPO_CHECK

# define nullpo_ret(t) nullpo_retr(DEFAULT, t)
# define nullpo_retv(t) nullpo_retr(, t)

/// Used by macros in this header
bool nullpo_chk(const char *file, sint32 line, const char *func,
                const void *target);

#endif // NULLPO_HPP
