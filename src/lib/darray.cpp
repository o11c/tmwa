#include "darray.hpp"

#include <cstdlib>
#include <cstring>

#include "int.hpp"

// if possible, allocate this much less than a power of 2
// It won't really hurt anything if this is wrong.
// In glibc, memory is allocated with 2 leading sizes,
// but the first one is part of the previous block, not the current one!
static const size_t MALLOC_OVERHEAD = sizeof(size_t);
// minimum size to malloc() or realloc() (except in shrink_to_fit)
// in glibc, a minimal unallocated block is 2 sizes and 2 pointers
// Note that due to the OVERHEAD, we have to allocate at least that much anyway!
// but I might ditch e.g. the refcount thing
// but, I am considering dropping the refcount field
// so it may relevant even if you DO use a variant of the expected allocator
static const size_t MALLOC_MIN = 2 * sizeof(size_t) + 2 * sizeof(void *) - MALLOC_OVERHEAD;
// elem_size, refcount, capacity, size
static const size_t OVERHEAD = 4 * sizeof(size_t);


DArray_base::DArray_base()
{
    mem = NULL;
}
DArray_base::DArray_base(const DArray_base& r)
{
    mem = r.mem;
    inc_ref();
}
DArray_base::DArray_base(DArray_base&& r)
{
    mem = r.mem;
    r.mem = NULL;
}
DArray_base::~DArray_base()
{
    dec_ref();
}

DArray_base& DArray_base::operator = (const DArray_base& r)
{
    dec_ref();
    mem = r.mem;
    inc_ref();
    return *this;
}
DArray_base& DArray_base::operator = (DArray_base&& r)
{
    dec_ref();
    mem = r.mem;
    r.mem = NULL;
    return *this;
}


size_t DArray_base::size() const
{
    if (!mem)
        return 0;
    return reinterpret_cast<size_t *>(mem)[-1];
}
size_t DArray_base::capacity() const
{
    if (!mem)
        return 0;
    return reinterpret_cast<size_t *>(mem)[-2];
}
size_t DArray_base::refcount() const
{
    if (!mem)
        return 0;
    return reinterpret_cast<size_t *>(mem)[-3];
}
size_t DArray_base::elem_size() const
{
    if (!mem)
        return 0;
    return reinterpret_cast<size_t *>(mem)[-4];
}

void DArray_base::inc_ref()
{
    if (!mem)
        return;
    ++reinterpret_cast<size_t *>(mem)[-3];
}
void DArray_base::dec_ref()
{
    if (!mem)
        return;
    if (!reinterpret_cast<size_t *>(mem)[-3]--)
    {
        free(mem - OVERHEAD);
        mem = NULL;
    }
}

void DArray_base::insert(size_t off, size_t n, size_t esz)
{
    if (!n)
        return;
    size_t sz = size() + n;
    size_t asz = (sz * esz + OVERHEAD <= MALLOC_MIN)
        ? MALLOC_MIN
        : (next_power_of_2(sz * esz + OVERHEAD + MALLOC_OVERHEAD - 1) - MALLOC_OVERHEAD);
    uint8_t *old_mem = mem;
    mem = reinterpret_cast<uint8_t *>(malloc(sz * esz + OVERHEAD)) + OVERHEAD;
    reinterpret_cast<size_t *>(mem)[-1] = sz;
    reinterpret_cast<size_t *>(mem)[-2] = (asz - OVERHEAD) / esz;
    reinterpret_cast<size_t *>(mem)[-3] = 0;
    reinterpret_cast<size_t *>(mem)[-4] = esz;
    if (old_mem)
    {
        memcpy(mem, old_mem, off * esz);
        memcpy(mem + (off + n) * esz, old_mem + off * esz, (sz - off - n) * esz);
        if (!reinterpret_cast<size_t *>(old_mem)[-3]--)
            free(old_mem - OVERHEAD);
    }
}
void DArray_base::erase(size_t off, size_t n)
{
    if (!n)
        return;
    size_t esz = elem_size();
    size_t sz = size() - n;
    size_t asz = (sz * esz + OVERHEAD <= MALLOC_MIN)
        ? MALLOC_MIN
        : (next_power_of_2(sz * esz + OVERHEAD + MALLOC_OVERHEAD - 1) - MALLOC_OVERHEAD);
    uint8_t *old_mem = mem;
    mem = reinterpret_cast<uint8_t *>(malloc(asz)) + OVERHEAD;
    memcpy(mem, old_mem, off * esz);
    memcpy(mem + off * esz, old_mem + (off + n) * esz, (sz - off) * esz);
    reinterpret_cast<size_t *>(mem)[-1] = sz;
    reinterpret_cast<size_t *>(mem)[-2] = (asz - OVERHEAD) / esz;
    reinterpret_cast<size_t *>(mem)[-3] = 0;
    reinterpret_cast<size_t *>(mem)[-4] = esz;
    if (!reinterpret_cast<size_t *>(old_mem)[-3]--)
        free(old_mem - OVERHEAD);
    if (capacity() > 2 * sz)
        shrink_to_fit();
}
void DArray_base::reserve(size_t cap, size_t esz)
{
    if (capacity() >= cap)
        return;
    size_t asz = ((cap * esz + OVERHEAD) <= MALLOC_MIN)
        ? MALLOC_MIN
        : (next_power_of_2(cap * esz + OVERHEAD + MALLOC_OVERHEAD - 1) - MALLOC_OVERHEAD);
    size_t sz = size();
    uint8_t *old_mem = mem;
    mem = static_cast<uint8_t *>(malloc(asz)) + OVERHEAD;
    reinterpret_cast<size_t *>(mem)[-1] = sz;
    reinterpret_cast<size_t *>(mem)[-2] = (asz - OVERHEAD) / esz;
    reinterpret_cast<size_t *>(mem)[-3] = 0;
    reinterpret_cast<size_t *>(mem)[-4] = esz;
    if (old_mem)
    {
        memcpy(mem, old_mem, sz * esz);
        if (!reinterpret_cast<size_t *>(old_mem)[-3]--)
            free(old_mem - OVERHEAD);
    }
}
void DArray_base::resize(size_t sz, size_t esz)
{
    reserve(sz, esz);
    unique();
    size_t old_sz = reinterpret_cast<size_t *>(mem)[-1];
    reinterpret_cast<size_t *>(mem)[-1] = sz;
    // don't shrink on every removal, but don't shrink when adding
    if (capacity() > 2 * sz && old_sz > sz)
        shrink_to_fit();
}
void DArray_base::shrink_to_fit()
{
    size_t sz = size();
    if (sz == capacity())
        return;
    size_t esz = elem_size();
    // non-NULL because size() != capacity()
    uint8_t *old_mem = mem;
    mem = reinterpret_cast<uint8_t *>(malloc(OVERHEAD + sz * esz)) + OVERHEAD;
    memcpy(mem, old_mem, sz * esz);
    reinterpret_cast<size_t *>(mem)[-1] = sz;
    reinterpret_cast<size_t *>(mem)[-2] = sz;
    reinterpret_cast<size_t *>(mem)[-3] = 0;
    reinterpret_cast<size_t *>(mem)[-4] = esz;
    if (!reinterpret_cast<size_t *>(old_mem)[-3]--)
        free(old_mem - OVERHEAD);
}
void DArray_base::nullify()
{
    if (!mem)
        return;
    if (!reinterpret_cast<size_t *>(mem)[-3]--)
        free(mem - OVERHEAD);
    mem = NULL;
}
void DArray_base::unique()
{
    if (!refcount())
        return;
    --reinterpret_cast<size_t *>(mem)[-3];
    size_t esz = elem_size();
    size_t sz = size();
    size_t cap = (sz * esz + OVERHEAD <= MALLOC_MIN)
        ? MALLOC_MIN
        : (next_power_of_2(sz * esz + OVERHEAD + MALLOC_OVERHEAD - 1) - MALLOC_OVERHEAD);
    const uint8_t *old_mem = mem;
    mem = reinterpret_cast<uint8_t *>(malloc(cap)) + OVERHEAD;
    memcpy(mem, old_mem, sz * esz);
    reinterpret_cast<size_t *>(mem)[-1] = sz;
    reinterpret_cast<size_t *>(mem)[-2] = (cap - OVERHEAD) / esz;
    reinterpret_cast<size_t *>(mem)[-3] = 0;
    reinterpret_cast<size_t *>(mem)[-4] = esz;
}

void *DArray_base::at(size_t idx)
{
    unique();
    return mem + idx * elem_size();
}
const void *DArray_base::at(size_t idx) const
{
    return mem + idx * elem_size();
}
