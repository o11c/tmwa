#ifndef INTS_HPP
#define INTS_HPP

# include "cxxstdio.hpp"

/// Integer wrapper classes
// Since this is just a header, the compiler will always inline these

# include <cstdint>
# include <cstddef>

// simplify generation of null pointers and zero-valued integers
static struct
{
    template<class T>
    operator T()
    {
        return T();
    }
} DEFAULT __attribute__((unused));

template<class T>
__attribute__((pure))
auto unwrap(T v) -> decltype(make_scalar(v))
{
    return make_scalar(v);
}

template<class T>
struct _wrap
{
    T v;
    template<class U>
    operator U()
    {
        return U(v);
    }
};
template<class T>
_wrap<T> wrap(T v)
{
    return {v};
}

// typedefs until I can make a satisfactory int wrapper class
typedef int8_t sint8;
typedef int16_t sint16;
typedef int32_t sint32;
typedef int64_t sint64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

namespace _int
{
/// NOTE: there are, basically two distinct types:
// * those that you can do math with, but not subclass
//   * note: this needs to have a "delta" type
//     * off the top of my head: std::chrono::time_point and std::chrono::duration
// * those you can subclass, but not do math with (but you can still compare)
/// Actually, this might make more sense:
/// If only C++11 had added concepts!
/// Make each of these a template (in its own namespace?)
/// that takes a dummy argument to specify the type,
/// and a type to specify the implementations
// * fully unique types (true enum)
// * bitmask types (currently using BIT_ENUM)
// * counter (id) types
// * value types
// * delta types
// * scalar types
/// With the following operations (besides == and !=, which everybody needs):
//
// bits_or_bool = bits &,|,^ bits
// bits_or_bool = ~bits (=== bits = bits::ALL ^ bits)
// bool = !bits
//
// counter = counter ++,--
// counter& = ++,-- counter
//
// value = value +,- delta
// delta = delta +,- delta
// delta = value - value
// delta = delta *,/ scalar
// (no %, except for scalars)
// and scalar does everything an int does
/// For extended operations (not with the default types)
// first construct a preferred_type ?
// preferred_type needs to forward
//    so preferred_type<my_value> matches with preferred_type<my_delta>
/// Also I need a global "zero" object with a template <class T> operator T()
// - done, under the name DEFAULT
template<class I>
struct unique_int
{
    typedef I underlying_type;
    I value;

    constexpr unique_int() : value() {}
    explicit constexpr unique_int(underlying_type v) : value(v) {}

    constexpr explicit operator I() { return value; }
    constexpr explicit operator bool() { return value; }
    constexpr bool operator !() { return !value; }
};

template<class T>
T& operator ++(T& t) { typedef typename T::underlying_type type; ++t.value; return t; }
template<class T>
T& operator --(T& t) { typedef typename T::underlying_type type; --t.value; return t; }

template<class T>
T operator ++(T& t, int) { T tmp(t); ++t; return tmp; }
template<class T>
T operator --(T& t, int) { T tmp(t); --t; return tmp; }

#define UNIQUE_TYPE(Name, itype)                                        \
struct Name : _int::unique_int<itype>                                   \
{                                                                       \
    Name() = default;                                                   \
    constexpr explicit Name(itype val) : unique_int<itype>(val) {}      \
}

// there's a bunch of hackish stuff to make sure
#define IMPL_UNIQUE_COMPARISON_OPERATOR(op)             \
template<class I>                                       \
__attribute__((pure))                                   \
bool operator op (I l, I r)                             \
{                                                       \
    unique_int<typename I::underlying_type> l1 = l;     \
    unique_int<typename I::underlying_type> r1 = r;     \
    return l1.value op r1.value;                        \
}
IMPL_UNIQUE_COMPARISON_OPERATOR(==)
IMPL_UNIQUE_COMPARISON_OPERATOR(!=)
IMPL_UNIQUE_COMPARISON_OPERATOR(<)
IMPL_UNIQUE_COMPARISON_OPERATOR(<=)
IMPL_UNIQUE_COMPARISON_OPERATOR(>=)
IMPL_UNIQUE_COMPARISON_OPERATOR(>)
#undef IMPL_UNIQUE_COMPARISON_OPERATOR

#if 0
template<size_t i, bool s>
struct type_by_size;
#define I_KNOW(nat)                                 \
template<>                                          \
struct type_by_size<sizeof(nat), nat(-1) < nat(0)>  \
{                                                   \
    typedef nat type;                               \
}
I_KNOW(int8_t);
I_KNOW(uint8_t);
I_KNOW(int16_t);
I_KNOW(uint16_t);
I_KNOW(int32_t);
I_KNOW(uint32_t);
I_KNOW(int64_t);
I_KNOW(uint64_t);
#undef I_KNOW
constexpr size_t _max(size_t a, size_t b)
{
    return a > b ? a : b;
}

template<class A, class B>
struct _common_type_impl
{
    constexpr static bool A_is_signed = A(-1) < A(0);
    constexpr static bool B_is_signed = B(-1) < B(0);
    // this may fail due to int/long or long/longlong collisions
    // so don't you dare use those types, use int32_t or int64_t
    static_assert(std::is_same<A, typename type_by_size<sizeof(A), A_is_signed>::type>::value, "A is not u?int{8,16,32,64}_t");
    static_assert(std::is_same<B, typename type_by_size<sizeof(B), B_is_signed>::type>::value, "B is not u?int{8,16,32,64}_t");

    constexpr static size_t A_signed_size = sizeof(A) * (1 + (A_is_signed < B_is_signed));
    constexpr static size_t B_signed_size = sizeof(B) * (1 + (B_is_signed < A_is_signed));
    typedef typename type_by_size<_max(A_signed_size, B_signed_size), A_is_signed || B_is_signed>::type type;
};

template<class... M>
struct _common_type;
// the use of make_scalar in the following specializations
// means that _common_type<_common_type<A, B>, _common_type<C, D>>
// yields the same underlying_type as _common_type<A, B, C, D>
template<class T>
struct _common_type<T>
{
    typedef typename make_scalar<T>::type underlying_type;
};
template<class F, class... R>
struct _common_type<F, R...>
{
    typedef typename _common_type_impl<typename make_scalar<F>::type, typename _common_type<R...>::underlying_type>::type underlying_type;
};

template<class T, class... P>
struct preferred_type;
template<class T>
struct preferred_type<T>
{
    typedef T underlying_type;
    T value;

    preferred_type(T t) : value(t) {}
};
template<class T, class P, class... Args>
struct preferred_type<T, P, Args...> : preferred_type<T, Args...>
{
    preferred_type(T t) : preferred_type<T, Args...>(t) {}
    operator P() { return P(preferred_type<T>::value); }
};

// class actually used to do math
// the default implementation simply uses the smallest type
// capable of holding all the input values
template<class A, class B>
struct _math_type
{
    typedef typename _common_type<A, B>::underlying_type type;
};
//
template<class A, class B>
struct _shift_types
{
    typedef typename make_scalar<A>::type type;
    typedef typename make_scalar<B>::type shift_type;
};
template<class A>
struct _unary_math_type
{
    typedef typename make_scalar<A>::type type;
};

#define IMPL_GENERIC_OPERATOR(op)                                           \
template<class L, class R>                                                  \
preferred_type<typename _math_type<L, R>::type, L, R> operator op(L l, R r) \
{                                                                           \
    typedef typename make_scalar<L>::type L_scalar;                         \
    typedef typename make_scalar<R>::type R_scalar;                         \
    typedef typename _math_type<L, R>::type math_type;                      \
    typedef preferred_type<math_type, L, R> preferred_type;                 \
    return preferred_type(math_type(L_scalar(l)) op math_type(R_scalar(r)));\
}                                                                           \
IMPL_ASSIGNMENT_OPERATOR(op)

#define IMPL_SHIFT_OPERATOR(op)                         \
template<class L, class R>                              \
constexpr L operator op(L l, R r)                       \
{                                                       \
    typedef typename make_scalar<L>::type L_math_type;  \
    typedef typename make_scalar<R>::type R_math_type;  \
    return L(L_math_type(l) op R_math_type(r));         \
}                                                       \
IMPL_ASSIGNMENT_OPERATOR(op)

#define IMPL_ASSIGNMENT_OPERATOR(op)    \
template<class L, class R>              \
L& operator op##=(L l, R r)             \
{                                       \
    l = l op r;                         \
}

IMPL_GENERIC_OPERATOR(+)
IMPL_GENERIC_OPERATOR(-)
IMPL_GENERIC_OPERATOR(*)
IMPL_GENERIC_OPERATOR(/)
IMPL_GENERIC_OPERATOR(%)
// logically, & should be different, but it isn't in the language
IMPL_GENERIC_OPERATOR(&)
IMPL_GENERIC_OPERATOR(|)
IMPL_GENERIC_OPERATOR(^)
IMPL_SHIFT_OPERATOR(>>)
IMPL_SHIFT_OPERATOR(<<)
#undef IMPL_GENERIC_OPERATOR
#undef IMPL_SHIFT_OPERATOR
#undef IMPL_ASSIGNMENT_OPERATOR

template<class T>
constexpr T operator +(T r)
{
    return r;
}

template<class T>
constexpr bool operator !(T r)
{
    typedef typename _unary_math_type<T>::type math_type;
    return !math_type(r);
}

template<class T>
constexpr T operator ~(T r)
{
    typedef typename _unary_math_type<T>::type math_type;
    return T(~math_type(r));
}

template<class T>
constexpr T operator -(T r)
{
    typedef typename _unary_math_type<T>::type math_type;
    return T(-math_type(r));
}

#endif //0

} // namespace _int

#endif // INTS_HPP
