#ifndef MT_RAND_HPP
#define MT_RAND_HPP

# include <random>

extern std::mt19937 mt_random;

template<class I>
inline I rand2(I low, I high)
{
    std::uniform_int_distribution<I> dist(low, high);
    return dist(mt_random);
}

// these new versions are technically more correct,
// because modulus skews slightly to lower numbers
template<class I>
inline I MPRAND(I add, I mod)
{
    std::uniform_int_distribution<I> dist(add, add + mod - 1);
    return dist(mt_random);
}

template<class I>
inline I MRAND(I mod)
{
    return MPRAND<I>(0, mod);
}
#endif // MT_RAND_HPP
