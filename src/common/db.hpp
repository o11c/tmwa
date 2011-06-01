// WARNING: there is a system header by this name
#ifndef DB_H
#define DB_H
# include "sanity.hpp"

# include <stdarg.h>

#include <functional>

/// Number of tree roots
// Somewhat arbitrary - larger wastes more space but is faster for large trees
// num % HASH_SIZE minimize collisions even for similar num
# define HASH_SIZE (256+27)

typedef enum dbn_color
{
    RED,
    BLACK
} dbn_color;

typedef intptr_t numdb_key_t;
struct db_key_t
{
    union
    {
        const char* s;
        numdb_key_t i;
    };
    db_key_t(const char *name) : s(name) {}
    db_key_t(numdb_key_t idx) : i(idx) {}
};
struct db_val_t
{
    union
    {
        void* p;
        intptr_t i;
    };
    db_val_t(void *ptr) : p(ptr) {}
    db_val_t(intptr_t iv) : i(iv) {}
};
typedef uint32_t hash_t;

/// DataBase Node
struct dbn
{
    struct dbn *parent, *left, *right;
    dbn_color color;
    db_key_t key;
    db_val_t data;
};

typedef enum dbt_type
{
    DB_NUMBER,
    DB_STRING,
} dbt_type;

/// DataBase Table
struct dbt
{
    dbt_type type;
    /// Note, before replacement, key/values to be replaced
    // TODO refactor to decrease/eliminate the uses of this?
    void (*release)(db_key_t, db_val_t) __attribute__((deprecated));
    /// The root trees
    struct dbn *ht[HASH_SIZE];
};

/// Create a map from char* to void*, with strings always nul-terminated
struct dbt *strdb_init();
/// Create a map from int to void*
struct dbt *numdb_init(void);
/// Return the value corresponding to the key, or NULL if not found
db_val_t db_search(struct dbt *table, db_key_t key);
/// Add or replace table[key] = data
// if it was already there, call release
struct dbn *db_insert(struct dbt *table, db_key_t key, db_val_t data);
/// Remove a key from the table, returning the data
db_val_t db_erase(struct dbt *table, db_key_t key);


typedef std::function<void(db_key_t, db_val_t)> DB_Func;

/// Execute a function for every element, in unspecified order
void db_foreach(struct dbt *, DB_Func);
// opposite of init? Calls release for every element and frees memory
// This probably isn't really needed: we don't have to free memory while exiting
void db_final(struct dbt *, DB_Func = 0); // __attribute__((deprecated));


inline db_val_t strdb_search(struct dbt *t, const char *k)
{
    return db_search(t, k);
}

inline struct dbn *strdb_insert(struct dbt *t, const char *k, db_val_t d)
{
    return db_insert(t, k, d);
}

inline db_val_t strdb_erase(struct dbt *t, const char *k)
{
    return db_erase(t, k);
}

template<class... Args>
inline void strdb_foreach(struct dbt *t, void (&func)(db_key_t, db_val_t, Args...), Args... args)
{
    db_foreach(t,
               std::bind(func,
                         std::placeholders::_1,
                         std::placeholders::_2,
                         args...));
}

template<class... Args>
inline void strdb_final(struct dbt *t, void (&func)(db_key_t, db_val_t, Args...), Args... args) __attribute__((deprecated));
template<class... Args>
inline void strdb_final(struct dbt *t, void (&func)(db_key_t, db_val_t, Args...), Args... args)
{
    db_final(t,
             std::bind(func,
                       std::placeholders::_1,
                       std::placeholders::_2,
                       args...));
}

inline db_val_t numdb_search(struct dbt *t, numdb_key_t k)
{
    return db_search(t, k);
}

inline struct dbn *numdb_insert(struct dbt *t, numdb_key_t k, db_val_t d)
{
    return db_insert(t, k, d);
}

inline db_val_t numdb_erase(struct dbt* t, numdb_key_t k)
{
    return db_erase(t, k);
}

template<class... Args>
inline void numdb_foreach(struct dbt *t, void (&func)(db_key_t, db_val_t, Args...), Args... args)
{
    db_foreach(t,
               std::bind(func,
                         std::placeholders::_1,
                         std::placeholders::_2,
                         args...));
}

template<class... Args>
inline void numdb_final(struct dbt *t, void (&func)(db_key_t, db_val_t, Args...), Args... args) __attribute__((deprecated));
template<class... Args>
inline void numdb_final(struct dbt *t, void (&func)(db_key_t, db_val_t, Args...), Args... args)
{
    db_final(t,
             std::bind(func,
                       std::placeholders::_1,
                       std::placeholders::_2,
                       args...));
}


#endif
