#ifndef STRING_HPP
#define STRING_HPP

#include <cstddef>
#include <cstring>
#include <string>

/// A POD string, which is halfway between a C char * and a C++ std::string.
// Written because I need to put strings into unions.
// Also, can have a NULL value.

// Strongly inspired by the libstdc++ header, bits/basic_string.h

// Note that this implementation does NOT try to be thread-safe
// in the case of reference counting.
// Also, if you copy and then modify, it's your own fault

// WARNING: unlike std::string, remember you *MUST* call .init() and .free() manually!
// Please don't use this class for new code. It is only intended to make porting C code easier
class POD_string
{
public:
    /// pointer to the beginning of the character data, or NULL
    // Don't access this directly!
    // The only safe thing is set initialize it to NULL,
    // which feature is provided by the init() function.
    // It is visible only due to a shortcoming of C++.
    // Of course, you could say that whole reason this class exists
    // is due to a shortcoming of C++.
    char *_ptr;

public:
    typedef char *iterator;
    typedef const char *const_iterator;

    /// Is this string non-NULL?
    operator bool() const __attribute__((pure));
    /// Is this string empty?
    bool empty() const __attribute__((pure));
    /// length of the string, excluding NUL
    size_t size() const __attribute__((pure));
    /// Capacity of the string, before it must
    size_t capacity() const __attribute__((pure));

    /// Leak whatever was there before
    void init();
    /// Decrement the refcount and set to NULL
    void free();
    /// Make this the empty string (distinct from NULL)
    // empty string is a shared representation
    void clear();
    /// request more capacity (nonbinding)
    void reserve(size_t);
    /// Increase the "size" of the string, initializing to zero
    void resize(size_t);
    /// Same, but only assign the NUL terminator
    // if the string expands, the new data is uninitialized
    void resize_no_fill(size_t);
    /// Request to not waste space
    void shrink_to_fit();

    /// Access
    char operator [] (size_t i) const __attribute__((pure));
    char& operator [] (size_t i) __attribute__((pure));
    const_iterator begin() const __attribute__((pure));
    const_iterator end() const __attribute__((pure));
    iterator begin() __attribute__((pure));
    iterator end() __attribute__((pure));
    const char *c_str() const __attribute__((pure));
    const char *data() const __attribute__((pure));

    /// Do explicitly what std::string would do implicitly
    POD_string clone() const;

    /// Ensure there are no other references to this string
    void unique();
    // You probably shouldn't use these
    // Note: refcount is zero-based
    // trying to decrement with a refcount of zero frees the memory
    size_t _refcount() const __attribute__((pure));
    void _inc_ref() const;
    void _dec_ref();

    /// Supplementary methods

    /// Replace this string with the given content
    // does not take a POD_string - you should assign clone() instead
    void assign(const char *str, size_t sz);
    void assign(const char *str);
    void assign(const std::string& str);

    /// Append the content to the string
    void append(const char *str, size_t sz);
    void append(const char *str);
    void append(const std::string& str);
    void append(POD_string str);
    /// Take ownership of a string allocated with malloc()
    // likely to cause a realloc() unless it allocated more than it needed
    // will cause a memmove()
    // but it can't be worse than malloc(), memcpy(), free()
    void take_ownership(char *, size_t);
    void take_ownership(char *str);

    void insert(size_t off, const char *str, size_t sz);
    void insert(size_t off, const char *str);
    void insert(size_t off, POD_string str);
    void insert(size_t off, const std::string& str);
    void erase(size_t start, size_t count);
    void erase(char *start, char *fin);

    // TODO implement this more efficiently
    void replace(size_t start, size_t count, const char *str, size_t sz);
    void replace(size_t start, size_t count, const char *str);
    void replace(size_t start, size_t count, POD_string str);
    void replace(size_t start, size_t count, const std::string& str);
    void replace(char *start, char *fin, const char *str, size_t sz);
    void replace(char *start, char *fin, const char *str);
    void replace(char *start, char *fin, POD_string str);
    void replace(char *start, char *fin, const std::string& str);

    /// Inspection (?) methods
    int compare(POD_string rhs) const __attribute__((pure));
    bool operator == (POD_string rhs) const __attribute__((pure));
    bool operator != (POD_string rhs) const __attribute__((pure));
    bool operator < (POD_string rhs) const __attribute__((pure));
    bool operator <= (POD_string rhs) const __attribute__((pure));
    bool operator > (POD_string rhs) const __attribute__((pure));
    bool operator >= (POD_string rhs) const __attribute__((pure));
};
#endif // STRING_HPP
