#ifndef MEMBER_MAP_HPP
#define MEMBER_MAP_HPP

# include <set>

# include "placed.hpp"

template<class IT>
class IteratorRange
{
    IT b, e;
public:
    IteratorRange(IT beg, IT en) : b(beg), e(en) {}
    IT begin() { return b; }
    IT end() { return e; }
};

template<class IT>
IteratorRange<IT> range(IT a, IT b)
{
    return IteratorRange<IT>(a, b);
}

template<class T>
struct Mutable
{
    mutable T mut;
    Mutable(const T& t) : mut(t) {}
    Mutable(T&& t) : mut(std::move(t)) {}
};

/// NOTE: there is no way to prevent using a pointer to a nonconst data member.
// If you do, demons may fly out of your nose.
template<class T, class M, const M T::* mem_ptr, class C = std::less<M>>
class MemberMap
{
    struct Compare
    {
        C cmp;
        bool operator()(const Mutable<T>& l, const Mutable<T>& r) const
        {
            return cmp(l.mut.*mem_ptr, r.mut.*mem_ptr);
        }
    };
    typedef std::set<Mutable<T>, Compare> impl_t;
    impl_t impl;
    typedef typename impl_t::iterator _iterator;
    typedef typename impl_t::const_iterator _const_iterator;
public:
    class iterator
    {
        _iterator impl;
    public:
        iterator(_iterator i) : impl(i) {}
        T& operator * () const  { return impl->mut; }
        iterator& operator ++() { ++impl; return *this; }
        iterator& operator --() { --impl; return *this; }
        bool operator ==(const iterator& r) { return impl == r.impl; }
        bool operator !=(const iterator& r) { return impl != r.impl; }
    };
    class const_iterator
    {
        _const_iterator impl;
    public:
        const_iterator(_const_iterator i) : impl(i) {}
        const T& operator * () const    { return impl->mut; }
        const_iterator& operator ++()   { ++impl; return *this; }
        const_iterator& operator --()   { --impl; return *this; }
        bool operator ==(const const_iterator& r) { return impl == r.impl; }
        bool operator !=(const const_iterator& r) { return impl != r.impl; }
    };

    iterator begin() { return impl.begin(); }
    iterator end() { return impl.end(); }
    const_iterator begin() const { return impl.begin(); }
    const_iterator end() const { return impl.end(); }
private:
    class PlacedMutableMember
    {
        Placed<Mutable<T>> pl;
    public:
        Mutable<T>& ref()
        {
            return pl.ref();
        }

        PlacedMutableMember(const M& m)
        {
            new(&const_cast<M&>(pl.ref().mut.*mem_ptr)) M(m);
        }
        ~PlacedMutableMember()
        {
            //const_cast<M&>(pl.ref().mut.*mem_ptr).~M();
            (pl.ref().mut.*mem_ptr).~M();
        }
    };
public:
    iterator find(const M& key)
    {
        return impl.find(PlacedMutableMember(key).ref());
    }
    const_iterator find(const M& key) const
    {
        return impl.find(PlacedMutableMember(key).ref());
    }

    std::pair<iterator, bool> insert(const T& t)
    {
        return impl.insert(Mutable<T>(t));
    }
    std::pair<iterator, bool> insert(T&& t)
    {
        return impl.insert(Mutable<T>(std::move(t)));
    }
    size_t size()
    {
        return impl.size();
    }
    iterator lower_bound(const M& key)
    {
        return impl.lower_bound(PlacedMutableMember(key).ref());
    }
    const_iterator lower_bound(const M& key) const
    {
        return impl.lower_bound(PlacedMutableMember(key).ref());
    }
    iterator upper_bound(const M& key)
    {
        return impl.upper_bound(PlacedMutableMember(key).ref());
    }
    const_iterator upper_bound(const M& key) const
    {
        return impl.upper_bound(PlacedMutableMember(key).ref());
    }

    bool erase(const M& key)
    {
        return impl.erase(PlacedMutableMember(key).ref());
    }
};

#endif //MEMBER_MAP_HPP
