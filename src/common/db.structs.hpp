#ifndef DB_STRUCTS_HPP
#define DB_STRUCTS_HPP

/// Number of tree roots
// Somewhat arbitrary - larger wastes more space but is faster for large trees
// num % HASH_SIZE minimize collisions even for similar num
# define HASH_SIZE (256+27)

enum dbn_color
{
    RED,
    BLACK
};

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

#endif //DB_STRUCTS_HPP
