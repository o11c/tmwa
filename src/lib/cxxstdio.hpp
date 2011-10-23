// cxxstdio.hpp - ability to use arbitrary classes with scanf/printf
#ifndef CXXSTDIO_HPP
#define CXXSTDIO_HPP
// Copyright 2011 Ben Longbons
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or (at
//     your option) any later version.
//
//     This program is distributed in the hope that it will be useful, but
//     WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//     General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <cstdio>
#include <string>
#include <cstdlib>

template<class T, bool = std::is_enum<T>::value>
struct remove_enum
{
    typedef T type;
};
template<class T>
struct remove_enum<T, true>
{
    typedef typename std::conditional<bool(T(-1) < T(0)),
                                      typename std::make_signed<T>::type,
                                      typename std::make_unsigned<T>::type
                                     >::type type;
};

template<class T, bool = std::is_scalar<T>::value>
struct make_scalar_type
{
    typedef T type;
};
template<class T>
struct make_scalar_type<T, false>
{
    typedef typename T::underlying_type type;
};

template<class T>
__attribute__((pure))
typename make_scalar_type<T>::type make_scalar(const T& v)
{
    return typename make_scalar_type<T>::type(v);
}

// use this if you can static_cast between your custom_type
// it will automatically work for enums
// and for classes with a public type called underlying_type
template<class T>
struct SimpleConvertType
{
    typedef typename remove_enum<typename make_scalar_type<T>::type>::type primitive_type;
};

template<class T>
class Printer
{
    typedef typename SimpleConvertType<T>::primitive_type primitive_type;
public:
    static primitive_type for_printf(const T& t)
    {
        return static_cast<primitive_type>(t);
    }
    // Make sure nobody calls us with a temporary,
    // which would otherwise bind to a const reference.
    // In practice, it would be okay for the default implementation,
    // but specializations of SimpleConvertType might change that.
    static primitive_type for_printf(const T&&) = delete;
};

template<>
class Printer<std::string>
{
    typedef const char *primitive_type;
public:
    static primitive_type for_printf(const std::string& t)
    {
        return t.c_str();
    }
    // Make sure nobody calls us with a temporary,
    // which would otherwise bind to a const reference.
    // This is particularly important for std::string
    static primitive_type for_printf(const std::string&&) = delete;
};

template<class T>
__attribute__((const))
const T& _decay(const T& r)
{
    typedef typename std::enable_if<!std::is_array<T>::value, T>::type type;
    return r;
}
#if 0
// why doesn't this work?
// In any case, the arguments are passed by const reference
// (which includes capturing temporaries)
template<class T>
T&& _decay(typename std::enable_if<!std::is_array<T>::value, T>::type&& r)
{
    return r;
}
#endif
// allow VLAs
template<class T>
__attribute__((const))
T *_decay(T *p)
{
    return p;
}

// TODO: refactor this to allow arbitrary sinks, e.g. a file
// this probably means
// 1. adding another template parameter
// 2. passing the std::string value by pointer
// 3. creating a ???
// 4. creating a variant of the macro that takes a typename
template<class Impl>
struct SPrinter
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"
#pragma GCC diagnostic ignored "-Wstack-protector"
    template<class... Args>
    static std::string print_primitive(Args... args)
    {
        constexpr const char *fmt = Impl::format();
        // Since they are primitive and known by now,
        // it doesn't matter than they're passed twice.
        char buf[snprintf(NULL, 0, fmt, args...) + 1];
        sprintf(buf, fmt, args...);
        return buf;
    }
#pragma GCC diagnostic pop
#if 1
    // TODO: can this be remerged into print()
    template<class... Args>
    static std::string print_decay(const Args&... args)
    {
        return print_primitive(Printer<typename std::remove_reference<typename std::remove_cv<Args>::type>::type>::for_printf(args)...);
    }
#endif
    template<class... Args>
    static std::string print(const Args&... args)
    {
        return print_decay(_decay(args)...);
    }
};
// Woot! I thing this is fully standards-compliant!
#define STR_PRINTF(fmt, ...)                                        \
    [&]() -> std::string                                            \
    {                                                               \
        struct impl                                                 \
        {                                                           \
            static constexpr const char *format() { return fmt; };  \
        };                                                          \
        return SPrinter<impl>::print(__VA_ARGS__);                  \
    }()

template<class T>
class Scanner
{
private:
    T& ref;
public:
    typename SimpleConvertType<T>::primitive_type value;
    Scanner(T& r) : ref(r) {}
    ~Scanner()
    {
        ref = T(value);
    }
};

template<>
class Scanner<std::string>
{
    std::string& str;
public:
    char *value;
    Scanner(std::string& s) : str(s), value(NULL) {}
    ~Scanner()
    {
        if (value)
        {
            str = value;
            free(value);
        }
    }
};

template<class Impl>
struct SScanner
{
    template<class... Args>
    static int scanf_primitive(const char *src, Args... args)
    {
        constexpr const char *fmt = Impl::format();
        return sscanf(src, fmt, args...);
    }
    template<class... Args>
    static int scanf_mid(const char *src, Args&&... args)
    {
        return scanf_primitive(src, &args.value...);
    }
    template<class... Args>
    static int scan(const char *src, Args&&... args)
    {
        return scanf_mid(src, Scanner<typename std::remove_reference<decltype(*args)>::type>(*args)...);
    }
};

#define SSCANF(str, fmt, ...)                                       \
    [&]() -> int                                                    \
    {                                                               \
        struct impl                                                 \
        {                                                           \
            static constexpr const char *format() { return fmt; };  \
        };                                                          \
        return SScanner<impl>::scan(str, __VA_ARGS__);              \
    }()

inline void putstr(FILE *fp, const std::string& str)
{
    fputs(str.c_str(), fp);
}

#define FPRINTF(fp, ...)                \
    putstr(fp, STR_PRINTF(__VA_ARGS__))

#define PRINTF(...)     \
    FPRINTF(stdout, __VA_ARGS__)

#endif //CXXSTDIO_HPP
