#ifndef DMAP_HPP
#define DMAP_HPP

# include <map>

/// A wrapper for a std::map, that never contains entries with a default value
template<class K, class V>
class DMap
{
    std::map<K, V> impl;
public:
    V get(const K& key) __attribute__((pure))
    {
        auto it = impl.find(key);
        if (it == impl.end())
            return V();
        return it->second;
    }
    void set(const K& key, const V& value)
    {
        if (value == V())
        {
            impl.erase(key);
            return;
        }
        // I was going to make the slightly-more-efficient version
        // that inserts directly, and overwrites if it can't
        // but then I realized value is hopefully trivial
        // and this is easier to read anyway
        impl[key] = value;
    }

    typedef typename std::map<K, V>::const_iterator const_iterator;
    const_iterator begin()
    {
        return impl.begin();
    }
    const_iterator end()
    {
        return impl.end();
    }

    void clear()
    {
        impl.clear();
    }
};

#endif //DMAP_HPP
