#ifndef UTILS_HPP
#define UTILS_HPP

# include <cstdlib>

# include <limits>

# include "../lib/int.hpp"
# include "../lib/ints.hpp"
# include "../lib/log.hpp"

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

# define RECREATE(result, type, number) \
  if (!((result) = reinterpret_cast<type *>(realloc((result), sizeof(type) * (number)))))\
      { perror("SYSERR: realloc failure"); abort(); } else(void)0

/// Dump data in hex (without ascii)
void hexdump(Log& log, const uint8 *data, size_t len);

template<class A, class B>
struct min_type
{
    static_assert(std::numeric_limits<A>::is_specialized,
                  "You must use primitives");
    static_assert(std::numeric_limits<B>::is_specialized,
                  "You must use primitives");
    static_assert(std::numeric_limits<A>::is_integer == std::numeric_limits<B>::is_integer,
                  "Can't (yet) mix integers and floats");
    // This can maybe be simplified, but this is clear
    typedef typename std::conditional<std::numeric_limits<A>::is_signed,
                                      typename std::conditional<std::numeric_limits<B>::is_signed,
                                                                // both signed, need the wider
                                                                typename std::conditional<(sizeof(A) >/*=*/ sizeof(B)), A, B>::type,
                                                                // only A is signed
                                                                A
                                                               >::type,
                                      typename std::conditional<std::numeric_limits<B>::is_signed,
                                                                // only B is signed
                                                                B,
                                                                // both unsigned, can use the narrower
                                                                typename std::conditional<(sizeof(A) </*=*/ sizeof(B)), A, B>::type
                                                               >::type
                                     >::type type;
};
template<class A>
struct min_type<A, A>
{
    typedef A type;
};

template<class A, class B>
struct max_type
{
    static_assert(std::numeric_limits<A>::is_specialized,
                  "You must use primitives");
    static_assert(std::numeric_limits<B>::is_specialized,
                  "You must use primitives");
    static_assert(std::numeric_limits<A>::is_integer == std::numeric_limits<B>::is_integer,
                  "Can't (yet) mix integers and floats");
    // This can definitely be simplified, but this is the most clear
    typedef typename std::conditional<std::numeric_limits<A>::is_signed,
                                      typename std::conditional<std::numeric_limits<B>::is_signed,
                                                               // both signed, need the wider
                                                               typename std::conditional<(sizeof(A) >/*=*/ sizeof(B)), A, B>::type,
                                                               // only A is signed, use B if possible
                                                               typename std::conditional<(sizeof(A) > sizeof(B)), A, B>::type
                                                              >::type,
                                      typename std::conditional<std::numeric_limits<B>::is_signed,
                                                               // only B is signed, use A if possible
                                                               typename std::conditional<sizeof(A) >= sizeof(B), A, B>::type,
                                                               // both unsigned, need the wider
                                                               typename std::conditional<(sizeof(A) >/*=*/ sizeof(B)), A, B>::type
                                                              >::type
                                     >::type type;
};
template<class A>
struct max_type<A, A>
{
    typedef A type;
};

template<class A, class B>
typename min_type<A, B>::type min(A a, B b) __attribute__((const));
template<class A, class B>
typename min_type<A, B>::type min(A a, B b)
{
    return lt(a, b) ? a : b;
}

template<class A, class B>
typename max_type<A, B>::type max(A a, B b) __attribute__((const));
template<class A, class B>
typename max_type<A, B>::type max(A a, B b)
{
    return lt(b, a) ? a : b;
}

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

# define ARRAY_SIZEOF(arr) ({ typedef decltype(is_an_array<static_cast<bool>(__builtin_constant_p(sizeof(arr)))>::test(arr)) type; sizeof(arr) / sizeof((arr)[0]); })

# define STRZCPY(dst, src) strzcpy(dst, src, ARRAY_SIZEOF(dst))
# define STRZCPY2(dst, src) strzcpy(dst, src, ARRAY_SIZEOF(src))
/// Make a string safe by replacing control characters with _
void remove_control_chars(char *str);
/// Check if there are any control chars
bool has_control_chars(char *str) __attribute__((pure));


/// Check whether it looks like a valid email
bool e_mail_check(const char *email) __attribute__((pure));

/// Convert string to number
// Parses booleans: on/off and yes/no in english, français, deutsch, español
// Then falls back to atoi (which means non-integers are parsed as 0)
// TODO replace by config_parse_bool and config_parse_int?
sint32 config_switch (const char *str);

const char *stamp_now(bool millis);

const char *stamp_time(time_t when, const char *def = NULL);

template<sint32 unit, class V>
inline void per_unit_adjust(V& val, sint32 proportion)
{
    if (proportion == unit)
        return;
    val = val * proportion / unit;
}

template<sint32 unit, class V>
inline void per_unit_subtract(V& val, sint32 proportion)
{
    per_unit_adjust<unit>(val, unit - proportion);
}

template<sint32 unit, class V>
inline void per_unit_add(V& val, sint32 proportion)
{
    per_unit_adjust<unit>(val, unit + proportion);
}

# define PER_UNIT_SPECIALIZE(prefix, number)            \
template<class V>                                       \
inline void prefix##_adjust(V& val, sint32 proportion)  \
{                                                       \
    per_unit_adjust<number>(val, proportion);           \
}                                                       \
template<class V>                                       \
inline void prefix##_subtract(V& val, sint32 proportion)\
{                                                       \
    per_unit_subtract<number>(val, proportion);         \
}                                                       \
template<class V>                                       \
inline void prefix##_add(V& val, sint32 proportion)     \
{                                                       \
    per_unit_add<number>(val, proportion);              \
}

PER_UNIT_SPECIALIZE(percent, 100)
PER_UNIT_SPECIALIZE(per256, 256)

template<class R, class A>
class sign_cast_impl;

template<class R, class A>
class sign_cast_impl<R *, A *>
{
public:
    static_assert(sizeof(R) == sizeof(A), "You can only use this on pointers to objects of the same size");
    static R *do_cast(A *input)
    {
        return reinterpret_cast<R *>(input);
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

// disallow free()ing an array of any type
template<class T, size_t n>
void free(T(&)[n]) = delete;
#endif //UTILS_HPP