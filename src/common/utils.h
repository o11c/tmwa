#ifndef UTILS_H
#define UTILS_H

#include "sanity.h"

#include <stdio.h>

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

#endif //UTILS_H
