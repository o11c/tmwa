#ifndef UTILS_H
#define UTILS_H

#include "sanity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

/*
Notes about memory allocation in tmwAthena:
There used to be 3 sources of allocation: these macros,
a{C,M,Re}alloc in common/malloc.{h,c}, and direct calls.
I deleted malloc.{h,c} because it was redundant;
future calls should either use this or depend on the coming segfault.
*/
# define CREATE(result, type, number) \
   if (!((result) = (type *) calloc ((number), sizeof(type))))   \
      { perror("SYSERR: malloc failure"); abort(); } else (void)0

# define RECREATE(result,type,number) \
  if (!((result) = (type *) realloc ((result), sizeof(type) * (number))))\
      { perror("SYSERR: realloc failure"); abort(); } else (void)0

/// Dump data in hex (without ascii)
void hexdump (FILE *fp, uint8_t *data, size_t len);
/// Dump an IP address (in network byte-order to a 15-byte string)
static inline void ip_to_str (in_addr_t ip, char out[16])
{
    ip = ntohl (ip);
    sprintf (out, "%hhu.%hhu.%hhu.%hhu", ip>>24, ip>>16, ip>>8, ip);
}

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/// Like strncpy but ensures a NUL-terminator
// return true if there already was one, false if we had to add it
static inline bool strzcpy (char *dst, const char *src, size_t n)
{
    if (!n) abort();
    strncpy (dst, src, n);
    dst[n-1] = '\0';
    return strnlen (src, n) != n;
}

int _ptr_used_(void) __attribute__((error("Pointer used in place of array") ));
#define ARRAY_SIZEOF(arr) ( \
__builtin_types_compatible_p(typeof(arr), typeof((arr)[0])*) \
? _ptr_used_() \
: sizeof(arr)/sizeof((arr)[0]) \
)
#define STRZCPY(dst, src) strzcpy (dst, src, ARRAY_SIZEOF(dst))
#endif //UTILS_H
