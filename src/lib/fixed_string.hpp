#ifndef FIXED_STRING_HPP
#define FIXED_STRING_HPP

#include <cstring>

/// A string taking exactly sz bytes of memory
// Unused bytes are set to 0
// by default, there must always be a nul-terminator
template<size_t sz, bool nul = true>
class fixed_string
{
    static_assert(sz > nul + 1, "Empty string storage not allowed");
    char data[sz];
public:
    fixed_string() : data({}) {}


    char *operator & ()
    {
        return data;
    }
    const char *operator & () const
    {
        return data;
    }
    char& operator[](size_t i)
    {
        return data[i];
    }
    const char& operator[](size_t i) const
    {
        return data[i];
    }
    size_t length() const
    {
        for (size_t i = 0; i < sz; i++)
            if (!data[i])
                return i;
        return sz;
    }
    bool operator == (const fixed_string& r) const
    {
        return strncmp(data, r.data, sz) == 0;
    }
    bool operator != (const fixed_string& r) const
    {
        return strncmp(data, r.data, sz) != 0;
    }
    bool operator < (const fixed_string& r) const
    {
        return strncmp(data, r.data, sz) < 0;
    }
    bool operator <= (const fixed_string& r) const
    {
        return strncmp(data, r.data, sz) <= 0;
    }
    bool operator > (const fixed_string& r) const
    {
        return strncmp(data, r.data, sz) > 0;
    }
    bool operator >= (const fixed_string& r) const
    {
        return strncmp(data, r.data, sz) >= 0;
    }
    void copy_from(const char *src)
    {
        strncpy(data, src, sz - nul);
    }
    void write_to(char *dst) const
    {
        strncpy(dst, data, sz);
    }

    bool contains(const char* srch) const
    {
        size_t len = strlen(srch);
        if (len >= sz)
             return false;
        for (int32_t i = 0; i < sz - len; i++)
        {
            if (memcmp(data+i, srch, len) == 0)
                return true;
        }
        return false;
    }
};

template<size_t sz, bool nul = true>
fixed_string<sz, nul> make_fixed(const char (&arg)[sz])
{
    fixed_string<sz, nul> out;
    out.copy_from(arg);
    return out;
}
#endif //FIXED_STRING_HPP
