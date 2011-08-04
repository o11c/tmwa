#ifndef DARRAY_HPP
#define DARRAY_HPP

# include <cstdint>
# include <cstddef>
# include <type_traits>

# include <cstdio>

class DArray_base
{
    uint8_t *mem;
protected:
    DArray_base();
    DArray_base(const DArray_base& r);
    DArray_base(DArray_base&& r);
    ~DArray_base();

    size_t size() const __attribute__((pure));
    size_t capacity() const __attribute__((pure));
    size_t refcount() const __attribute__((pure));
    size_t elem_size() const __attribute__((pure));

    void inc_ref();
    void dec_ref();

    void insert(size_t off, size_t n, size_t elem_size);
    void erase(size_t off, size_t n);
    void reserve(size_t sz, size_t elem_size);
    void resize(size_t sz, size_t elem_size);
    void shrink_to_fit();
    void nullify();
    void unique();

    void *at(size_t idx);
    const void *at(size_t idx) const __attribute__((pure));
};

/// An extensible array
// like std::vector, except Copy-on-Write, and can be NULL
// unlike POD_string, I am making this one a full-blown class
template<class T>
class DArray : public DArray_base
{
    static_assert(std::is_pod<T>::value, "You must use a POD type");
    static_assert(__alignof__(T) < 4 * sizeof(size_t), "Bad alignment");
public:
    bool empty()
    {
        return size() == 0;
    }
    size_t size() const
    {
        return DArray_base::size();
    }
    size_t capacity() const
    {
        return DArray_base::capacity();
    }

    typedef T* iterator;
    typedef const T* const_iterator;

    iterator begin()
    {
        unique();
        return reinterpret_cast<T*>(at(0));
    }
    iterator end()
    {
        unique();
        return reinterpret_cast<T*>(at(size()));
    }
    const_iterator begin() const
    {
        return reinterpret_cast<const T*>(at(0));
    }
    const_iterator end() const
    {
        return reinterpret_cast<T*>(at(size()));
    }
    T operator [] (size_t i) const
    {
        return *reinterpret_cast<T*>(at(i));
    }
    T& operator [] (size_t i)
    {
        return *reinterpret_cast<T*>(at(i));
    }

    void nullify()
    {
        DArray_base::nullify();
    }
    void resize(size_t sz)
    {
        DArray_base::resize(sz, sizeof(T));
    }
    void clear()
    {
        resize(0);
    }

    void push_back(const T& v)
    {
        size_t sz = size();
        resize(sz + 1);
        (*this)[sz] = v;
    }
    void pop_back()
    {
        resize(size() - 1);
    }
    size_t offset(const_iterator pos) const
    {
        return pos - begin();
    }
    iterator insert(iterator pos, const T& t = T(), size_t n = 1)
    {
        size_t off = offset(pos);
        DArray_base::insert(off, n, sizeof(T));
        T *const start = reinterpret_cast<T *>(at(off));
        T *const fin = reinterpret_cast<T *>(at(off + n));
        for (T *cur = start; cur != fin; ++cur)
            fprintf(stderr, "(write %zd/%zd)\n", offset(cur), size()),
            *cur = t;
        return start;
    }
    void insert(size_t off, const T& t = T(), size_t n = 1)
    {
        DArray_base::insert(off, n, sizeof(T));
        T *const start = reinterpret_cast<T *>(at(off));
        T *const fin = reinterpret_cast<T *>(at(off + n));
        for (T *cur = start; cur != fin; ++cur)
            fprintf(stderr, "(write %zd/%zd)\n", offset(cur), size()),
            *cur = t;
    }
    void erase(iterator pos)
    {
        DArray_base::erase(offset(pos), 1);
    }
    void erase(iterator first, iterator last)
    {
        DArray_base::erase(offset(first), last - first);
    }
    void erase(size_t off, size_t n = 1)
    {
        DArray_base::erase(off, n);
    }

    bool operator ==(const DArray& arr)
    {
        // internal pointers are equal, share same memory
        return at(0) == arr.at(0);
    }
    bool operator !=(const DArray& arr)
    {
        return at(0) != arr.at(0);
    }
};

#endif //DARRAY_HPP
