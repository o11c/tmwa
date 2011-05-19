#ifndef UTILS_H
#define UTILS_H

#include "sanity.hpp"

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
   if (!((result) = (type *) calloc((number), sizeof(type))))   \
      { perror("SYSERR: malloc failure"); abort(); } else(void)0

# define RECREATE(result,type,number) \
  if (!((result) = (type *) realloc((result), sizeof(type) * (number))))\
      { perror("SYSERR: realloc failure"); abort(); } else(void)0

/// Dump data in hex (without ascii)
void hexdump(FILE *fp, uint8_t *data, size_t len);
/// Dump an IP address (in network byte-order to a 15-byte string)
static inline void ip_to_str(in_addr_t ip, char out[16])
{
    ip = ntohl(ip);
    sprintf(out, "%hhu.%hhu.%hhu.%hhu", ip>>24, ip>>16, ip>>8, ip);
}

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

/// Like strncpy but ensures a NUL-terminator
// return true if there already was one, false if we had to add it
bool strzcpy(char *dst, const char *src, size_t n);

/// A type that exists
struct _true_ {};

template <bool constant>
struct is_an_array
{
    /// Used when a constant sized (non-VLA) object is passed in
    /// only allow arrays past
    template<class B, size_t n>
    static _true_ test( B(&)[n] );
};

template <>
struct is_an_array<false>
{
    /// This happens only for VLAs; force decay to a pointer to let it work with templates
    template <class B>
    static _true_ test(B *n);
};

#define ARRAY_SIZEOF(arr) ({ typedef decltype(is_an_array<(bool)__builtin_constant_p(sizeof(arr))>::test(arr)) type; sizeof(arr) / sizeof((arr)[0]); })

#define STRZCPY(dst, src) strzcpy(dst, src, ARRAY_SIZEOF(dst))
#define STRZCPY2(dst, src) strzcpy(dst, src, ARRAY_SIZEOF(src))
/// Make a string safe by replacing control characters with _
void remove_control_chars(char *str);
/// Check if there are any control chars
bool has_control_chars(char *str);


/// Check whether it looks like a valid email
bool e_mail_check(const char *email);

/// Convert string to number
// Parses booleans: on/off and yes/no in english, français, deutsch, español
// Then falls back to atoi (which means non-integers are parsed as 0)
// TODO replace by config_parse_bool and config_parse_int?
int config_switch (const char *str);

/// Create a stream that discards all output and/or returns EOF for all input
FILE *create_null_stream(const char *mode);

const char *stamp_now(bool millis);

const char *stamp_time(time_t when, const char *def);

static inline void log_time(FILE *fp)
{
    fprintf(fp, "%s: ", stamp_now(true));
}

FILE *create_or_fake_or_die(const char *filename);

/// Die, and hopefully generate a backtrace
#define SEGFAULT() *((char *) 0) = 0
#endif //UTILS_H
