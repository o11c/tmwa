#ifndef LFSR_HPP
#define LFSR_HPP

# include <cstdint>
# include <cstdlib>

uint64_t lfsr_next_internal(uint64_t last, int32_t bits, bool allow_zero_state) __attribute__((const, pure));

template<int32_t bits, bool allow_zero_state>
uint64_t lfsr_next(uint64_t last) __attribute__((const));
template<int32_t bits, bool allow_zero_state>
uint64_t lfsr_next(uint64_t last)
{
    static_assert(2 <= bits && bits <= 64, "bad number of bits");

    return lfsr_next_internal(last, bits, allow_zero_state);
}

uint64_t lfsr_next(uint64_t last, int32_t bits, bool allow_zero_state) __attribute__((pure));
inline uint64_t lfsr_next(uint64_t last, int32_t bits, bool allow_zero_state)
{
    if (2 <= bits && bits <= 64)
        return lfsr_next_internal(last, bits, allow_zero_state);
    abort();
}

#endif // LFSR_HPP
