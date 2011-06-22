#ifndef UTILS_H
#define UTILS_H

#include "sanity.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "../lib/log.hpp"

/*
Notes about memory allocation in tmwAthena:
There used to be 3 sources of allocation: these macros,
a{C,M,Re}alloc in common/malloc.{h,c}, and direct calls.
I deleted malloc.{h,c} because it was redundant;
future calls should either use this or depend on the coming segfault.
*/
# define CREATE(result, type, number) \
   if (!((result) = reinterpret_cast<type *>(calloc((number), sizeof(type)))))   \
      { perror("SYSERR: malloc failure"); abort(); } else(void)0

# define RECREATE(result,type,number) \
  if (!((result) = reinterpret_cast<type *>(realloc((result), sizeof(type) * (number)))))\
      { perror("SYSERR: realloc failure"); abort(); } else(void)0

/// Dump data in hex (without ascii)
void hexdump(Log& log, const uint8_t *data, size_t len);

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

#define ARRAY_SIZEOF(arr) ({ typedef decltype(is_an_array<static_cast<bool>(__builtin_constant_p(sizeof(arr)))>::test(arr)) type; sizeof(arr) / sizeof((arr)[0]); })

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

const char *stamp_now(bool millis);

const char *stamp_time(time_t when, const char *def = NULL);

static inline void log_time(FILE *fp)
{
    fprintf(fp, "%s: ", stamp_now(true));
}

template<int unit>
inline void per_unit_adjust(int& val, int proportion)
{
    if (proportion == unit)
        return;
    val = val * proportion / unit;
}

template<int unit>
inline void per_unit_subtract(int& val, int proportion)
{
    per_unit_adjust<unit>(val, unit - proportion);
}

template<int unit>
inline void per_unit_add(int& val, int proportion)
{
    per_unit_adjust<unit>(val, unit + proportion);
}

#define PER_UNIT_SPECIALIZE(prefix, number) \
static inline void prefix##_adjust(int& val, int proportion) \
{ \
    per_unit_adjust<number>(val, proportion); \
} \
static inline void prefix##_subtract(int& val, int proportion) \
{ \
    per_unit_subtract<number>(val, proportion); \
} \
static inline void prefix##_add(int& val, int proportion) \
{ \
    per_unit_add<number>(val, proportion); \
}

PER_UNIT_SPECIALIZE(percent, 100)
PER_UNIT_SPECIALIZE(per256, 256)

template<class R, class A>
class sign_cast_impl;

template<class R, class A>
class sign_cast_impl<R*, A*>
{
public:
    static_assert(sizeof(R) == sizeof(A), "You can only use this on pointers to objects of the same size");
    static R* do_cast(A* input)
    {
        return reinterpret_cast<R*>(input);
    }
};

template<class R, class A>
class sign_cast_impl<R&, A&>
{
public:
    static_assert(sizeof(R) == sizeof(A), "You can only use this on references to objects of the same size");
    static R& do_cast(A& input)
    {
        return reinterpret_cast<R&>(input);
    }
};

template<class R, class A>
inline R sign_cast(A input)
{
    return sign_cast_impl<R, A>::do_cast(input);
}

template<class T>
class PointeeLess
{
public:
    bool operator () (const T& a, const T& b)
    {
        return *a < *b;
    }
};
#endif //UTILS_H
