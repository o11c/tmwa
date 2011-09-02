#ifndef ENUM_HPP
#define ENUM_HPP

#include <type_traits>

namespace _eops
{
    template<class Enum>
    class _underlie
    {
        static_assert(std::is_enum<Enum>::value, "only enums have an underlying type");
        typedef typename std::make_unsigned<Enum>::type _unsigned;
        typedef typename std::make_signed<Enum>::type _signed;
    public:
        typedef typename std::conditional<Enum(-1) < Enum(0), _signed, _unsigned>::type type;
    };

    /// A type used as the return type of all operators
    // needed because 'operator bool' must be a method
    // but 'enum class' can't have methods

    template<class Enum>
    struct Enum_or_bool
    {
        Enum val;

        constexpr operator Enum()
        {
            return val;
        }
        constexpr explicit operator bool()
        {
            // You can only convert to bool, if there is an explicit Enum::NONE
            // (since this method is only instantiated if used,
            // you can always convert to Enum)
            return val != Enum::NONE;
        }
        constexpr bool operator !()
        {
            return val == Enum::NONE;
        }

        constexpr Enum_or_bool(Enum v) : val(v) {}
    };


    template<class E>
    constexpr bool operator !(E r)
    {
        typedef typename std::enable_if<std::is_enum<E>::value, E>::type type;
        return r == E::NONE;
    }
    template<class E>
    constexpr bool operator !(Enum_or_bool<E> r)
    {
        typedef typename std::enable_if<std::is_enum<E>::value, E>::type type;
        return !E(r);
    }


    template<class E>
    constexpr Enum_or_bool<E> operator | (E l, E r)
    {
        typedef typename std::enable_if<std::is_enum<E>::value, E>::type type;
        typedef typename _underlie<E>::type U;
        return E(U(l) | U(r));
    }
    template<class E>
    constexpr Enum_or_bool<E> operator | (Enum_or_bool<E> l, E r)
    {
        return E(l) | r;
    }
    template<class E>
    constexpr Enum_or_bool<E> operator | (E l, Enum_or_bool<E> r)
    {
        return l | E(r);
    }
    template<class E>
    constexpr Enum_or_bool<E> operator | (Enum_or_bool<E> l, Enum_or_bool<E> r)
    {
        return E(l) | E(r);
    }


    template<class E>
    constexpr Enum_or_bool<E> operator & (E l, E r)
    {
        typedef typename std::enable_if<std::is_enum<E>::value, E>::type type;
        typedef typename _underlie<E>::type U;
        return E(U(l) & U(r));
    }
    template<class E>
    constexpr Enum_or_bool<E> operator & (Enum_or_bool<E> l, E r)
    {
        return E(l) & r;
    }
    template<class E>
    constexpr Enum_or_bool<E> operator & (E l, Enum_or_bool<E> r)
    {
        return l & E(r);
    }
    template<class E>
    constexpr Enum_or_bool<E> operator & (Enum_or_bool<E> l, Enum_or_bool<E> r)
    {
        return E(l) & E(r);
    }


    template<class E>
    constexpr Enum_or_bool<E> operator ^ (E l, E r)
    {
        typedef typename std::enable_if<std::is_enum<E>::value, E>::type type;
        typedef typename _underlie<E>::type U;
        return E(U(l) ^ U(r));
    }
    template<class E>
    constexpr Enum_or_bool<E> operator ^ (Enum_or_bool<E> l, E r)
    {
        return E(l) ^ r;
    }
    template<class E>
    constexpr Enum_or_bool<E> operator ^ (E l, Enum_or_bool<E> r)
    {
        return l ^ E(r);
    }
    template<class E>
    constexpr Enum_or_bool<E> operator ^ (Enum_or_bool<E> l, Enum_or_bool<E> r)
    {
        return E(l) ^ E(r);
    }


    template<class E>
    constexpr Enum_or_bool<E> operator ~(E r)
    {
        typedef typename std::enable_if<std::is_enum<E>::value, E>::type type;
        return E::ALL ^ r;
    }
    template<class E>
    constexpr Enum_or_bool<E> operator ~(Enum_or_bool<E> r)
    {
        return E::ALL ^ E(r);
    }


    template<class E>
    E& operator |= (E& l, E r)
    {
        typedef typename std::enable_if<std::is_enum<E>::value, E>::type type;
        return l = l | r;
    }
    template<class E>
    E& operator |= (E& l, Enum_or_bool<E> r)
    {
        return l = l | E(r);
    }


    template<class E>
    E& operator &= (E& l, E r)
    {
        typedef typename std::enable_if<std::is_enum<E>::value, E>::type type;
        return l = l & r;
    }
    template<class E>
    E& operator &= (E& l, Enum_or_bool<E> r)
    {
        return l = l & E(r);
    }


    template<class E>
    E& operator ^= (E& l, E r)
    {
        typedef typename std::enable_if<std::is_enum<E>::value, E>::type type;
        return l = l ^ r;
    }
    template<class E>
    E& operator ^= (E& l, Enum_or_bool<E> r)
    {
        return l = l ^ (r);
    }
} // namespace _eops

# define BIT_ENUM(N, U) \
namespace _eops         \
{                       \
    enum class N : U;   \
}                       \
using _eops::N;         \
enum class _eops::N : U \
// user-supplied braces and enum values

#endif //ENUM_HPP
