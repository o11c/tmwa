/// return wrappers for unexpected NULL pointers
#ifndef SANITY_H
#define SANITY_H

# if __STDC_VERSION__ < 199901L && !(defined __cplusplus)
#  error "Please compile in C99 mode or as C++"
# endif
# if __GNUC__ < 3
// I don't specifically know what version this requires,
// but GCC 3 was the beginning of modern GCC
#  error "Please upgrade your compiler to at least GCC 3"
# endif
# ifndef __i386__
// Known platform dependencies:
// endianness for the [RW]FIFO.* macros
// possibly, some signal-handling
#  error "Unsupported platform"
# endif
# ifdef __x86_64__
// I'm working on it - I know there are some pointer-size assumptions.
#  error "Sorry, this code is believed not to be 64-bit safe"
#  error "please compile with -m32"
# endif

# ifdef __cplusplus
#  define UNUSED
# else
/// A name for unused function arguments - can be repeated
#  define UNUSED UNUSED_IMPL(__COUNTER__)
// Don't you just love the hoops the preprocessor makes you go through?
#  define UNUSED_IMPL(arg) UNUSED_IMPL2(arg)
#  define UNUSED_IMPL2(suffix) unused_ ## suffix __attribute__((unused))
#endif

/// Convert conditions to use the bool type
# include <stdbool.h>
/// Convert type assumptions to use the standard types here
# include <stdint.h>
/// size_t, NULL
# include <stddef.h>

#endif // SANITY_H
