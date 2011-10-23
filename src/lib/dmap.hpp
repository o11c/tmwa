#ifndef DMAP_HPP
#define DMAP_HPP

# include <map>

/// A wrapper for a std::map, that never contains entries with a default value
template<class K, class V>
class DMap
{
    std::map<K, V> impl;
public:
    V get(const K& key) const __attribute__((pure))
    {
        auto it = impl.find(key);
        if (it == impl.end())
            return V();
        return it->second;
    }

    V take(const K& key)
    {
        auto it = impl.find(key);
        if (it == impl.end())
            return V();
        V rv = std::move(it->second);
        impl.erase(it);
        return rv;
    }

    void remove(const K& key)
    {
        impl.erase(key);
    }

    void set(const K& key, const V& value)
    {
        if (value == V())
        {
            impl.erase(key);
            return;
        }
        auto pair = impl.insert({key, value});
        if (!pair.second)
            pair.first->second = value;
    }

    V replace(const K& key, const V& value)
    {
        if (value == V())
        {
            auto it = impl.find(key);
            if (it == impl.end())
                return V();
            V out = std::move(it->second);
            impl.erase(it);
            return out;
        }
        auto pair = impl.insert({key, value});
        if (pair.second)
            return V();
        V out = std::move(pair.first->second);
        pair.first->second = value;
        return out;
    }

    typedef typename std::map<K, V>::const_iterator const_iterator;
    const_iterator begin() const
    {
        return impl.begin();
    }
    const_iterator end() const
    {
        return impl.end();
    }

    void clear()
    {
        impl.clear();
    }

    size_t size() const
    {
        return impl.size();
    }
};

#endif //DMAP_HPP
