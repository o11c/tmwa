#ifndef SANITY_HPP
#define SANITY_HPP

# ifndef __cplusplus
#  error "Please compile as C++"
# endif

# if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 5)
// Requires decent C++0x support
#  error "Please upgrade your compiler to at least GCC 4.5"
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

/// Convert type assumptions to use the standard types here
# include <cstdint>
/// size_t, NULL
# include <cstddef>

#endif // SANITY_HPP
