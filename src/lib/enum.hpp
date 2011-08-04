#ifndef ENUM_HPP
#define ENUM_HPP

template<class Self, class U>
struct operators
{
private:
    U u;
protected:
    constexpr operators() = default;
    constexpr operators(U raw) : u(raw) {}
    ~operators() = default;
public:
    constexpr explicit operator bool()
    {
        return u;
    }
    constexpr bool operator ! ()
    {
        return !u;
    }
    friend constexpr Self operator | (const Self& l, const Self& r)
    {
        return Self(operators(l.u | r.u));
    }
    friend constexpr Self operator & (const Self& l, const Self& r)
    {
        return Self(operators(l.u & r.u));
    }
    friend constexpr Self operator ^ (const Self& l, const Self& r)
    {
        return Self(operators(l.u ^ r.u));
    }
    friend constexpr Self operator ~ (const Self& r)
    {
        return Self(operators(~r.u));
    }
    friend constexpr bool operator == (const Self& l, const Self& r)
    {
        return l.u == r.u;
    }
    friend constexpr bool operator != (const Self& l, const Self& r)
    {
        return l.u != r.u;
    }
    Self& operator = (const Self& o)
    {
        u = o.u;
        return static_cast<Self&>(*this);
    }
    Self& operator |= (const Self& o)
    {
        u = o.u;
        return static_cast<Self&>(*this);
    }
    Self& operator &= (const Self& o)
    {
        u = o.u;
        return static_cast<Self&>(*this);
    }
    Self& operator ^= (const Self& o)
    {
        u = o.u;
        return static_cast<Self&>(*this);
    }
};

#define BIT_ENUM(Name, underlying_type)                         \
struct Name : operators<Name, underlying_type>                  \
{                                                               \
    enum impl_t : underlying_type;                              \
    /* implicitly constexpr */                                  \
    Name() = default;                                           \
    constexpr Name(impl_t v) :                                  \
        operators<Name, underlying_type>(v) {}                  \
    constexpr Name(const operators<Name, underlying_type>& p) : \
        operators<Name, underlying_type>(p) {}                  \
};                                                              \
/* Separate definitions because of when it gets instantiated */ \
constexpr Name operator | (Name::impl_t l, Name::impl_t r)      \
{                                                               \
    return Name(l) | Name(r);                                   \
}                                                               \
constexpr Name operator & (Name::impl_t l, Name::impl_t r)      \
{                                                               \
    return Name(l) & Name(r);                                   \
}                                                               \
constexpr Name operator ^ (Name::impl_t l, Name::impl_t r)      \
{                                                               \
    return Name(l) ^ Name(r);                                   \
}                                                               \
constexpr Name operator ~ (Name::impl_t r)                      \
{                                                               \
    return ~Name(r);                                            \
}                                                               \
enum Name::impl_t : underlying_type
// user-supplied braces and enum values

#define SHIFT_ENUM(Name, underlying_type)                       \
struct Name : operators<Name, underlying_type>                  \
{                                                               \
    enum impl_t : underlying_type;                              \
    /* implicitly constexpr */                                  \
    Name() = default;                                           \
    constexpr Name(impl_t v) :                                  \
        operators<Name, underlying_type>(1 << v) {}             \
    constexpr Name(const operators<Name, underlying_type>& p) : \
        operators<Name, underlying_type>(p) {}                  \
};                                                              \
/* Separate definitions because of when it gets instantiated */ \
constexpr Name operator | (Name::impl_t l, Name::impl_t r)      \
{                                                               \
    return Name(l) | Name(r);                                   \
}                                                               \
constexpr Name operator & (Name::impl_t l, Name::impl_t r)      \
{                                                               \
    return Name(l) & Name(r);                                   \
}                                                               \
constexpr Name operator ^ (Name::impl_t l, Name::impl_t r)      \
{                                                               \
    return Name(l) ^ Name(r);                                   \
}                                                               \
constexpr Name operator ~ (Name::impl_t r)                      \
{                                                               \
    return ~Name(r);                                            \
}                                                               \
enum Name::impl_t : underlying_type
// user-supplied braces and enum values

#endif //ENUM_HPP
