#ifndef FIXED_STACK_HPP
#define FIXED_STACK_HPP

# include <cstddef>

# include <stdexcept>

# include <new>

/**
 * A fixed storage container that looks like it's not.
 *
 * Please don't use this for new code, it exists only to ease conversion
 * from raw arrays as I convert the member type to something with a destructor.
 *
 * In particular, iterators remain valid for the whole life of the container.
 */

template<class T, size_t n>
class fixed_stack
{
    char storage[sizeof(T) * n] __attribute__((aligned(alignof(T))));
    size_t stack_size;
public:
    typedef T* iterator;
    typedef const T* const_iterator;

    fixed_stack() : stack_size(0) {}
    fixed_stack(const fixed_stack& rhs) : stack_size(rhs.stack_size)
    {
        for (size_t i = 0; i < stack_size; i++)
            new (&reinterpret_cast<T *>(storage)[i]) T(rhs[i]);
    }
    // same, but uses move constructor of contained objects
    fixed_stack(fixed_stack&& rhs) : stack_size(rhs.stack_size)
    {
        for (size_t i = 0; i < stack_size; i++)
            new (&reinterpret_cast<T *>(storage)[i]) T(std::move(rhs[i]));
    }
    ~fixed_stack()
    {
        // no need to actually update the stack_size field
        for (size_t sz = stack_size; sz--; )
            reinterpret_cast<T *>(storage)[sz].~T();
    }

    // Note: these don't use T's operator =, since there aren't any guarantees
    fixed_stack& operator =(const fixed_stack& rhs)
    {
        for (size_t sz = stack_size; sz--; )
            reinterpret_cast<T *>(storage)[sz].~T();
        for(size_t i = 0; i < rhs.stack_size; i++)
            reinterpret_cast<T *>(storage)[i] = rhs[i];
        stack_size = rhs.stack_size;
    }
    fixed_stack& operator =(fixed_stack&& rhs)
    {
        for (size_t sz = stack_size; sz--; )
            reinterpret_cast<T *>(storage)[sz].~T();
        for(size_t i = 0; i < rhs.stack_size; i++)
            reinterpret_cast<T *>(storage)[i] = rhs[i];
        stack_size = rhs.stack_size;
    }

    void clear()
    {
        for (size_t sz = stack_size; sz--; )
            reinterpret_cast<T *>(storage)[sz].~T();
        stack_size = 0;
    }

    T& operator[] (size_t i)
    {
        return reinterpret_cast<T *>(storage)[i];
    }
    const T& operator[] (size_t i) const
    {
        return reinterpret_cast<T *>(storage)[i];
    }

    iterator begin()
    {
        return reinterpret_cast<T *>(storage);
    }
    const_iterator begin() const
    {
        return reinterpret_cast<const T *>(storage);
    }

    iterator end()
    {
        return &reinterpret_cast<T *>(storage)[stack_size];
    }
    const_iterator end() const
    {
        return &reinterpret_cast<const T *>(storage)[stack_size];
    }

    T& front()
    {
        return reinterpret_cast<T *>(storage)[0];
    }
    const T& front() const
    {
        return reinterpret_cast<const T *>(storage)[0];
    }

    T& back()
    {
        return reinterpret_cast<T *>(storage)[stack_size-1];
    }
    const T& back() const
    {
        return reinterpret_cast<const T *>(storage)[stack_size-1];
    }

    size_t size() const
    {
        return stack_size;
    }
    bool empty() const
    {
        return stack_size == 0;
    }
    bool full() const
    {
        return stack_size == n;
    }

    void push_back(const T& v)
    {
        if (full())
            throw std::out_of_range("fixed stack storage is full");
        new(reinterpret_cast<T *>(storage)[stack_size++]) T(v);
    }
    void push_back(T&& v)
    {
        if (full())
            throw std::out_of_range("fixed stack storage is full");
        // yes, you have to explicitly call std::move
        // because a named rvalue reference is no longer an rvalue reference
        new(&reinterpret_cast<T *>(storage)[stack_size++]) T(std::move(v));
    }

    void pop_back()
    {
        if (empty())
            throw std::out_of_range("fixed stack storage is full");
        reinterpret_cast<T *>(storage)[--stack_size].~T();
    }
};

#endif //FIXED_STACK_HPP
