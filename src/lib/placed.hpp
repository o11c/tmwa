#ifndef PLACED_HPP
#define PLACED_HPP

# include <new>
template<class T>
void destruct(T& v)
{
    v.~T();
}

// TODO actually use this class instead of duplicating all the code
template<class T>
class Placed
{
    char memory[sizeof(T)] __attribute__((aligned(alignof(T))));
    T& ref()
    {
        return *reinterpret_cast<T *>(memory);
    }
public:
    void construct_default()
    {
        new(memory) T();
    }
    void construct_copy(Placed<T>& t)
    {
        new(memory) T(t.ref());
    }
    void construct_move(Placed<T>& t)
    {
        new(memory) T(std::move(t.ref()));
    }
    template<class... Args>
    void construct_args(Args&&... args)
    {
        new(memory) T(std::forward(args)...);
    }
    void destruct()
    {
        ref().~T();
    }
};

#endif // PLACED_HPP
