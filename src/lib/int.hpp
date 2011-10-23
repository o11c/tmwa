#ifndef INT_HPP
#define INT_HPP

# include <cstdint>

# pragma GCC diagnostic ignored "-Wtype-limits"

/// safe comparisons between signed and unsigned integers
template<class A, class B>
__attribute__((const))
bool eq(A a, B b)
{
    if (a < 0 && b >= 0)
        return false;
    if (a >= 0 && b < 0)
        return false;
    return a == b;
}
template<class A, class B>
__attribute__((const))
bool ne(A a, B b)
{
    return !eq(a, b);
}
template<class A, class B>
__attribute__((const))
bool lt(A a, B b)
{
    if (a < 0 && b >= 0)
        return true;
    if (a >= 0 && b < 0)
        return false;
    return a < b;
}
template<class A, class B>
__attribute__((const))
bool gt(A a, B b)
{
    return lt(b, a);
}
template<class A, class B>
__attribute__((const))
bool le(A a, B b)
{
    return !gt(a, b);
    if (a < 0 && b >= 0)
        return true;
    if (a >= 0 && b < 0)
        return false;
    return a < b;
}
template<class A, class B>
__attribute__((const))
bool ge(A a, B b)
{
    return !lt(a, b);
}
# pragma GCC diagnostic pop

/// bit twiddling
/// http://graphics.stanford.edu/~seander/bithacks.html

// 0 -> 1, 4 -> 8
// if you want 0 -> 0, 4 -> 4, call next_power_of_2(v-1)
inline uint8_t next_power_of_2(uint8_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x++;
    return x;
}
inline uint16_t next_power_of_2(uint16_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x++;
    return x;
}
inline uint32_t next_power_of_2(uint32_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}
inline uint64_t next_power_of_2(uint64_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    x++;
    return x;
}

inline bool is_power_of_2(uint64_t v)
{
    return (v & (v - 1)) == 0;
}

inline uint32_t highest_bit(uint64_t v)
{
    // ^ instead of -
    // because I checked the ASM
    return 63 ^ __builtin_clzll(v);
}

inline uint32_t lowest_bit(uint64_t v)
{
    return __builtin_ctzll(v);
}

#endif // INT_HPP
