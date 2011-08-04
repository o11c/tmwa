#include "string.hpp"

#include <cstring>
#include <cstdlib>

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
// overhead of string allocation system: refcount, capacity, size
static const size_t OVERHEAD = 3 * sizeof(size_t);

#define POD_STR_SIZE     reinterpret_cast<size_t *>(_ptr)[-1]
#define POD_STR_CAPACITY reinterpret_cast<size_t *>(_ptr)[-2]
#define POD_STR_REFCOUNT reinterpret_cast<size_t *>(_ptr)[-3]

static char zero [OVERHEAD + 1];
POD_string _calc_empty_string() __attribute__((const));
POD_string _calc_empty_string()
{
    POD_string out;
    out._ptr = &zero[OVERHEAD];
    return out;
}
const POD_string EMPTY_STRING = _calc_empty_string();

POD_string::operator bool() const
{
    return _ptr;
}
bool POD_string::empty() const
{
    return size() == 0;
}
size_t POD_string::size() const
{
    if (!_ptr)
        return 0;
    return POD_STR_SIZE;
}
size_t POD_string::capacity() const
{
    if (!_ptr)
        return 0;
    return POD_STR_CAPACITY;
}
size_t POD_string::_refcount() const
{
    if (!_ptr)
        return 0;
    return POD_STR_REFCOUNT;
}
void POD_string::_inc_ref() const
{
    if (_ptr)
        POD_STR_REFCOUNT++;
}
void POD_string::_dec_ref()
{
    if (!_ptr)
        return;
    if (_refcount())
    {
        POD_STR_REFCOUNT--;
        return;
    }
    ::free(_ptr - OVERHEAD);
    _ptr = NULL;
}
void POD_string::unique()
{
    if (!_refcount())
        return;
    _dec_ref();
    size_t sz = size();
    // cap is always at least one more than sz, but capacity() excludes the NUL
    size_t cap = (sz + OVERHEAD < MALLOC_MIN)
        ? MALLOC_MIN
        : (next_power_of_2(sz + OVERHEAD + MALLOC_OVERHEAD) - MALLOC_OVERHEAD);
    char *new_ptr = reinterpret_cast<char *>(malloc(cap));
    if (!new_ptr)
        abort();
    memcpy(new_ptr + OVERHEAD, _ptr, sz + 1);
    _ptr = new_ptr + OVERHEAD;
    POD_STR_REFCOUNT = 0;
    POD_STR_CAPACITY = cap - OVERHEAD - 1;
    POD_STR_SIZE = sz;
}

void POD_string::init()
{
    _ptr = NULL;
}
void POD_string::free()
{
    _dec_ref();
    init();
}
void POD_string::clear()
{
    _dec_ref();
    *this = EMPTY_STRING;
    _inc_ref();
}
void POD_string::reserve(size_t cap)
{
    if (capacity() >= cap)
        return;
    if (!_ptr)
        clear();
    unique();
    cap = (cap + OVERHEAD < MALLOC_MIN)
        ? MALLOC_MIN
        : (next_power_of_2(cap + OVERHEAD + MALLOC_OVERHEAD) - MALLOC_OVERHEAD);
    char *old_ptr = _ptr ? _ptr - OVERHEAD : NULL;
    char *new_ptr = reinterpret_cast<char *>(realloc(old_ptr, cap));
    if (!new_ptr)
        abort();
    _ptr = new_ptr + OVERHEAD;
    POD_STR_CAPACITY = cap - OVERHEAD - 1;
}
void POD_string::resize_no_fill(size_t sz)
{
    reserve(sz);
    unique();
    POD_STR_SIZE = sz;
    // we DO require there always be a NUL-terminator
    _ptr[sz] = '\0';
    if (capacity() > 2 * sz)
        shrink_to_fit();
}
void POD_string::resize(size_t sz)
{
    size_t old_sz = size();
    resize_no_fill(sz);
    if (sz > old_sz)
    {
        memset(_ptr + old_sz, '\0', sz - old_sz);

}
}
void POD_string::shrink_to_fit()
{
    size_t sz = size();
    if (sz == capacity())
        return;
    unique();
    // _ptr cannot be NULL because that is caught with size() == capacity()
    char *old_ptr = _ptr - OVERHEAD;
    char *new_ptr = reinterpret_cast<char *>(realloc(old_ptr, sz + OVERHEAD));
    if (!new_ptr)
        return;
    _ptr = new_ptr + OVERHEAD;
}

char POD_string::operator [] (size_t i) const
{
    return _ptr[i];
}
char& POD_string::operator [] (size_t i)
{
    unique();
    return _ptr[i];
}
POD_string::const_iterator POD_string::begin() const
{
    return _ptr;
}
POD_string::const_iterator POD_string::end() const
{
    return _ptr + size();
}
POD_string::iterator POD_string::begin()
{
    unique();
    return _ptr;
}
POD_string::iterator POD_string::end()
{
    unique();
    return _ptr + size();
}
const char *POD_string::c_str() const
{
    return _ptr;
}
const char *POD_string::data() const
{
    return _ptr;
}

POD_string POD_string::clone() const
{
    _inc_ref();
    return *this;
}

void POD_string::take_ownership(char *str, size_t sz)
{
    _dec_ref();
    size_t cap = (sz + OVERHEAD < MALLOC_MIN)
        ? MALLOC_MIN
        : (next_power_of_2(sz + OVERHEAD + MALLOC_OVERHEAD) - MALLOC_OVERHEAD);
    char *new_ptr = reinterpret_cast<char *>(realloc(str, cap));
    if (!new_ptr)
        abort();
    _ptr = new_ptr + OVERHEAD;
    memmove(_ptr, new_ptr, sz + 1);
    POD_STR_SIZE = sz;
    POD_STR_CAPACITY = cap - OVERHEAD - 1;
    POD_STR_REFCOUNT = 0;
}


/// Replace this string with the given content
void POD_string::assign(const char *str, size_t sz)
{
    resize_no_fill(sz);
    memcpy(_ptr, str, sz);
}
void POD_string::assign(const char *str)
{
    assign(str, strlen(str));
}
void POD_string::assign(const std::string& str)
{
    assign(str.data(), str.size());
}

/// Append the content to the string
void POD_string::append(const char *str, size_t sz)
{
    size_t old_sz = size();
    resize_no_fill(old_sz + sz);
    memcpy(_ptr + old_sz, str, sz);
}
void POD_string::append(const char *str)
{
    append(str, strlen(str));
}
void POD_string::append(const std::string& str)
{
    append(str.data(), str.size());
}
void POD_string::append(POD_string str)
{
    append(str.data(), str.size());
}


/// Take ownership of a string allocated with malloc()
// likely to cause a realloc() unless it allocated more than it needed
// will cause a memmove()
// but it can't be worse than malloc(), memcpy(), free()
void POD_string::take_ownership(char *str)
{
    take_ownership(str, strlen(str));
}

void POD_string::insert(size_t off, const char *str, size_t sz)
{
    size_t old_sz = size();
    resize_no_fill(old_sz + sz);
    memmove(_ptr + off + sz, _ptr + off, old_sz - off + 1);
    memcpy(_ptr + off, str, sz);
}
void POD_string::insert(size_t off, const char *str)
{
    insert(off, str, strlen(str));
}
void POD_string::insert(size_t off, POD_string str)
{
    insert(off, str.data(), str.size());
}
void POD_string::insert(size_t off, const std::string& str)
{
    insert(off, str.data(), str.size());
}
void POD_string::erase(size_t start, size_t count)
{
    unique();
    size_t old_sz = size();
    memmove(_ptr + start, _ptr + start + count, old_sz - (start + count) + 1);
    resize(old_sz - count);
}
void POD_string::erase(char *start, char *fin)
{
    erase(start - _ptr, fin - start);
}

// TODO implement this more efficiently
void POD_string::replace(size_t start, size_t count, const char *str, size_t sz)
{
    erase(start, count);
    insert(start, str, sz);
}
void POD_string::replace(size_t start, size_t count, const char *str)
{
    erase(start, count);
    insert(start, str);
}
void POD_string::replace(size_t start, size_t count, POD_string str)
{
    erase(start, count);
    insert(start, str);
}
void POD_string::replace(size_t start, size_t count, const std::string& str)
{
    erase(start, count);
    insert(start, str);
}
void POD_string::replace(char *start, char *fin, const char *str, size_t sz)
{
    replace(start - _ptr, fin - start, str, sz);
}
void POD_string::replace(char *start, char *fin, const char *str)
{
    replace(start - _ptr, fin - start, str);
}
void POD_string::replace(char *start, char *fin, POD_string str)
{
    replace(start - _ptr, fin - start, str);
}
void POD_string::replace(char *start, char *fin, const std::string& str)
{
    replace(start - _ptr, fin - start, str);
}

/// Inspection (?) methods
int POD_string::compare(POD_string rhs) const
{
    int out = memcmp(_ptr, rhs._ptr, std::min(size(), rhs.size()));
    if (out != 0)
        return out;
    return size() - rhs.size();
}
bool POD_string::operator == (POD_string rhs) const
{
    return (size() == rhs.size()) && (compare(rhs) == 0);
}
bool POD_string::operator != (POD_string rhs) const
{
    return !(*this == rhs);
}
bool POD_string::operator < (POD_string rhs) const
{
    return compare(rhs) < 0;
}
bool POD_string::operator <= (POD_string rhs) const
{
    return compare(rhs) <= 0;
}
bool POD_string::operator > (POD_string rhs) const
{
    return compare(rhs) > 0;
}
bool POD_string::operator >= (POD_string rhs) const
{
    return compare(rhs) >= 0;
}
