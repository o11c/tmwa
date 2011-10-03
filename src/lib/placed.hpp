#ifndef PLACED_HPP
#define PLACED_HPP

# include <new>
# include <utility>

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

template<class Tcheck, class... Tlist>
struct _is_in_pack;
template<class Tcheck, class Tcomp, class... Rest>
struct _is_in_pack<Tcheck, Tcomp, Rest...>
{
    constexpr static bool value = std::is_same<Tcheck, Tcomp>::value | _is_in_pack<Tcheck, Rest...>::value;
};
template<class Tbad>
struct _is_in_pack<Tbad>
{
    constexpr static bool value = false;
};

template<class... Types>
class Union;
template<class First, class... Rest>
class Union<First, Rest...>
{
    union
    {
        First frist;
        Union<Rest...> rest;
    };
};
template<class Only>
class Union<Only>
{
    Only only;
};

// A slightly ... okay, significantly ... less safe alternative for boost::Variant
// this is intended to be used in a wrapper class that takes care of the details
// WARNING: quite a bit of work needs to be done to support
// copy/move constructors and destructors
// the default ones will just do a binary copy/move
// which is usually not what you'd want, otherwise you'd just use a union
template<class... Types>
class AnyPlaced
{
    char storage[sizeof(Union<Types...>)] __attribute__((aligned(alignof(Union<Types...>))));
public:
    template<class T>
    T& ref()
    {
        static_assert(_is_in_pack<T, Types...>::value, "You can only get the declared types.");
        return *reinterpret_cast<T *>(storage);
    }
    template<class T>
    const T& cref() const
    {
        static_assert(_is_in_pack<T, Types...>::value, "You can only get the declared types.");
        return *reinterpret_cast<const T *>(storage);
    }
    template<class T>
    void destroy()
    {
        ref<T>().~T();
    }
    template<class T, class... Args>
    void construct(Args&&... args)
    {
        static_assert(_is_in_pack<T, Types...>::value, "You can only get the declared types.");
        new(storage) T(std::forward<Args>(args)...);
    }
};

#endif // PLACED_HPP
