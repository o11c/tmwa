#ifndef EARRAY_HPP
#define EARRAY_HPP

# include <cstddef>
# include <type_traits>

/// An array that enforces indexing by a particular type (generally an enum)
template<class E, class I, I max>
class earray
{
    static constexpr size_t asi(I i)
    {
        return static_cast<size_t>(i);
    }
    E elts[asi(max)];
public:
    earray() = default;

    // needed due to funny initializer_list rules
    // (all E2 are E)
    template<typename... E2>
    constexpr earray(E2... e) : elts({e...}) {}
    // needed because for some reason GCC is trying to use the template constructor
    earray(const earray&) = default;
    earray(earray&&) = default;

    typedef E *iterator;
    typedef const E *const_iterator;

    E& operator [](I idx)
    {
        return elts[asi(idx)];
    }
    const E& operator [] (I idx) const
    {
        return elts[asi(idx)];
    }

    iterator begin()
    {
        return &elts[0];
    }
    iterator end()
    {
        return &elts[asi(max)];
    }
    const_iterator begin() const
    {
        return &elts[0];
    }
    const_iterator end() const
    {
        return &elts[asi(max)];
    }
};
#endif //EARRAY_HPP
