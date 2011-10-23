#include "db.hpp"

#include <cstring>

#include "utils.hpp"

typedef uint32 hash_t;

static sint32 strdb_cmp(const char *a, const char *b)
{
    return strcmp(a, b);
}

static hash_t strdb_hash(const char *a) __attribute__((pure));
static hash_t strdb_hash(const char *a)
{
    hash_t h = 0;
    const uint8 *p = reinterpret_cast<const uint8 *>(a);
    while (*p)
    {
        h = (h * 33 + *p++) ^ (h >> 24);
    }
    return h;
}

struct dbt *strdb_init()
{
    struct dbt *table;
    CREATE(table, struct dbt, 1);
    table->type = DB_STRING;
    return table;
}

static sint32 numdb_cmp(numdb_key_t a, numdb_key_t b)
{
    if (a == b)
        return 0;
    if (a < b)
        return -1;
    return 1;
}

static hash_t numdb_hash(numdb_key_t a)
{
    return static_cast<hash_t>(a);
}

struct dbt *numdb_init(void)
{
    struct dbt *table;
    CREATE(table, struct dbt, 1);
    table->type = DB_NUMBER;
    return table;
}

static sint32 table_cmp(struct dbt *table, db_key_t a, db_key_t b) __attribute__((pure));
static sint32 table_cmp(struct dbt *table, db_key_t a, db_key_t b)
{
    switch (table->type)
    {
    case DB_NUMBER: return numdb_cmp(a.i, b.i);
    case DB_STRING: return strdb_cmp(a.s, b.s);
    }
    abort();
}

static hash_t table_hash(struct dbt *table, db_key_t key) __attribute__((pure));
static hash_t table_hash(struct dbt *table, db_key_t key)
{
    switch (table->type)
    {
    case DB_NUMBER: return numdb_hash(key.i);
    case DB_STRING: return strdb_hash(key.s);
    }
    abort();
}

/// Search for a node with the given key
db_val_t db_search(struct dbt *table, db_key_t key)
{
    struct dbn *p = table->ht[table_hash(table, key) % HASH_SIZE];

    while (p)
    {
        sint32 c = table_cmp(table, key, p->key);
        if (c == 0)
            return p->data;
        if (c < 0)
            p = p->left;
        else
            p = p->right;
    }
    return static_cast<void *>(NULL);
}

// Tree maintainance methods
static void db_rotate_left(struct dbn *p, struct dbn **root)
{
    struct dbn *y = p->right;
    p->right = y->left;
    if (y->left)
        y->left->parent = p;
    y->parent = p->parent;

    if (p == *root)
        *root = y;
    else if (p == p->parent->left)
        p->parent->left = y;
    else
        p->parent->right = y;
    y->left = p;
    p->parent = y;
}

static void db_rotate_right(struct dbn *p, struct dbn **root)
{
    struct dbn *y = p->left;
    p->left = y->right;
    if (y->right)
        y->right->parent = p;
    y->parent = p->parent;

    if (p == *root)
        *root = y;
    else if (p == p->parent->right)
        p->parent->right = y;
    else
        p->parent->left = y;
    y->right = p;
    p->parent = y;
}

static void db_rebalance(struct dbn *p, struct dbn **root)
{
    p->color = RED;
    while (p != *root && p->parent->color == RED)
    {
        if (p->parent == p->parent->parent->left)
        {
            struct dbn *y = p->parent->parent->right;
            if (y && y->color == RED)
            {
                p->parent->color = BLACK;
                y->color = BLACK;
                p->parent->parent->color = RED;
                p = p->parent->parent;
            }
            else
            {
                if (p == p->parent->right)
                {
                    p = p->parent;
                    db_rotate_left(p, root);
                }
                p->parent->color = BLACK;
                p->parent->parent->color = RED;
                db_rotate_right(p->parent->parent, root);
            }
        }
        else
        {
            struct dbn *y = p->parent->parent->left;
            if (y && y->color == RED)
            {
                p->parent->color = BLACK;
                y->color = BLACK;
                p->parent->parent->color = RED;
                p = p->parent->parent;
            }
            else
            {
                if (p == p->parent->left)
                {
                    p = p->parent;
                    db_rotate_right(p, root);
                }
                p->parent->color = BLACK;
                p->parent->parent->color = RED;
                db_rotate_left(p->parent->parent, root);
            }
        }
    }
    (*root)->color = BLACK;
}

// param z = node to remove
static void db_rebalance_erase(struct dbn *z, struct dbn **root)
{
    struct dbn *y = z;
    struct dbn *x = NULL;

    if (!y->left)
        x = y->right;
    else if (!y->right)
        x = y->left;
    else
    {
        y = y->right;
        while (y->left)
            y = y->left;
        x = y->right;
    }
    struct dbn *x_parent = NULL;
    if (y != z)
    {
        z->left->parent = y;
        y->left = z->left;
        if (y != z->right)
        {
            x_parent = y->parent;
            if (x)
                x->parent = y->parent;
            y->parent->left = x;
            y->right = z->right;
            z->right->parent = y;
        }
        else
            x_parent = y;
        if (*root == z)
            *root = y;
        else if (z->parent->left == z)
            z->parent->left = y;
        else
            z->parent->right = y;
        y->parent = z->parent;
        {
            dbn_color tmp = y->color;
            y->color = z->color;
            z->color = tmp;
        }
        y = z;
    }
    else
    {
        x_parent = y->parent;
        if (x)
            x->parent = y->parent;
        if (*root == z)
            *root = x;
        else if (z->parent->left == z)
            z->parent->left = x;
        else
            z->parent->right = x;
    }
    if (y->color != RED)
    {
        while (x != *root && (!x || x->color == BLACK))
            if (x == x_parent->left)
            {
                struct dbn *w = x_parent->right;
                if (w->color == RED)
                {
                    w->color = BLACK;
                    x_parent->color = RED;
                    db_rotate_left(x_parent, root);
                    w = x_parent->right;
                }
                if ((!w->left || w->left->color == BLACK) &&
                    (!w->right || w->right->color == BLACK))
                {
                    w->color = RED;
                    x = x_parent;
                    x_parent = x->parent;
                }
                else
                {
                    if (!w->right || w->right->color == BLACK)
                    {
                        if (w->left)
                            w->left->color = BLACK;
                        w->color = RED;
                        db_rotate_right(w, root);
                        w = x_parent->right;
                    }
                    w->color = x_parent->color;
                    x_parent->color = BLACK;
                    if (w->right)
                        w->right->color = BLACK;
                    db_rotate_left(x_parent, root);
                    break;
                }
            }
            else
            {
                // same as above, with right <-> left.
                struct dbn *w = x_parent->left;
                if (w->color == RED)
                {
                    w->color = BLACK;
                    x_parent->color = RED;
                    db_rotate_right(x_parent, root);
                    w = x_parent->left;
                }
                if ((!w->right || w->right->color == BLACK) &&
                    (!w->left || w->left->color == BLACK))
                {
                    w->color = RED;
                    x = x_parent;
                    x_parent = x_parent->parent;
                }
                else
                {
                    if (!w->left || w->left->color == BLACK)
                    {
                        if (w->right)
                            w->right->color = BLACK;
                        w->color = RED;
                        db_rotate_left(w, root);
                        w = x_parent->left;
                    }
                    w->color = x_parent->color;
                    x_parent->color = BLACK;
                    if (w->left)
                        w->left->color = BLACK;
                    db_rotate_right(x_parent, root);
                    break;
                }
            }
        if (x)
            x->color = BLACK;
    }
}

struct dbn *db_insert(struct dbt *table, db_key_t key, db_val_t data)
{
    hash_t hash = table_hash(table, key) % HASH_SIZE;
    sint32 c = 0;
    struct dbn *prev = NULL;
    struct dbn *p = table->ht[hash];
    while (p)
    {
        c = table_cmp(table, key, p->key);
        if (c == 0)
        {
            // key found in table, replace
            // Tell the user of the table to free the key and value
            if (table->release)
                table->release(p->key, p->data);
            p->data = data;
            p->key = key;
            return p;
        }
        // prev is always p->parent?
        prev = p;
        if (c < 0)
            p = p->left;
        else
            p = p->right;
    }
    CREATE(p, struct dbn, 1);
    p->key = key;
    p->data = data;
    p->color = RED;
    if (c == 0)
    {                           // hash entry is empty
        table->ht[hash] = p;
        p->color = BLACK;
        return p;
    }
    p->parent = prev;
    if (c < 0)
        prev->left = p;
    else
        prev->right = p;
    if (prev->color == RED)
    {
        // must rebalance
        db_rebalance(p, &table->ht[hash]);
    }
    return p;
}

db_val_t db_erase(struct dbt *table, db_key_t key)
{
    hash_t hash = table_hash(table, key) % HASH_SIZE;
    struct dbn *p = table->ht[hash];
    while (p)
    {
        sint32 c = table_cmp(table, key, p->key);
        if (c == 0)
            break;
        if (c < 0)
            p = p->left;
        else
            p = p->right;
    }
    if (!p)
        return static_cast<void *>(NULL);
    db_val_t data = p->data;
    db_rebalance_erase(p, &table->ht[hash]);
    free(p);
    return data;
}
#ifdef SMART_WALK_TREE
static inline void db_walk_tree(bool dealloc, struct dbn *p, db_func_t func, va_list ap)
{
    if (!p)
        return;
    if (!dealloc && !func)
    {
        fprintf(stderr, "DEBUG: Must walk tree to either free or invoke a function.\n");
        abort();
    }
    if (p->parent)
    {
        fprintf(stderr, "DEBUG: Root nodes must not have parents\n");
        abort();
    }
    while (true)
    {
        // apply_func loop
        if (func)
            func(p->key, p->data, ap);
        if (p->left)
        {
            // continue descending
            p = p->left;
            continue; //goto apply_func;
        }
        if (p->right)
        {
            // descending the other side
            p = p->right;
            continue; //goto apply_func;
        }
        while (true)
        {
            // backtrack loop
            if (!p->parent)
            {
                if (dealloc)
                    free(p);
                // if we have already done both children, there is no more to do
                return;
            }
            if (p->parent->left == p && p->parent->right)
            {
                // finished the left tree, now walk the right tree
                p = p->parent->right;
                if (dealloc)
                    free(p->parent->left);
                break; //goto apply_func;
            }
            // p->parent->right == p
            // or p->parent->left == p but p->parent->right == NULL
            // keep backtracking
            p = p->parent;
            if (dealloc)
                free(p->right?:p->left);
        } //backtrack loop
    } // apply_func loop
}
#endif // SMART_WALK_TREE

void db_foreach(struct dbt *table, DB_Func func)
{
    for (sint32 i = 0; i < HASH_SIZE; i++)
    {
#ifdef SMART_WALK_TREE
        db_walk_tree(false, table->ht[i], func);
#else
        struct dbn *p = table->ht[i];
        if (!p)
            continue;
        struct dbn *stack[64];
        sint32 sp = 0;
        while (1)
        {
            func(p->key, p->data);
            struct dbn *pn = p->left;
            if (pn)
            {
                if (p->right)
                    stack[sp++] = p->right;
                p = pn;
            }
            else // pn == NULL, time to do the right branch
            {
                if (p->right)
                    p = p->right;
                else
                {
                    if (sp == 0)
                        break;
                    p = stack[--sp];
                }
            } // if pn else if !pn
        } // while true
#endif // else ! SMART_WALK_TREE
    } // for i
}

// This function is suspiciously similar to the previous
void db_final(struct dbt *table, DB_Func func)
{
    for (sint32 i = 0; i < HASH_SIZE; i++)
    {
#ifdef SMART_WALK_TREE
        db_walk_tree(true, table->ht[i], func);
#else
        struct dbn *p = table->ht[i];
        if (!p)
            continue;
        struct dbn *stack[64];
        sint32 sp = 0;
        while (1)
        {
            if (func)
                func(p->key, p->data);
            struct dbn *pn = p->left;
            if (pn)
            {
                if (p->right)
                    stack[sp++] = p->right;
            }
            else // pn == NULL, check the right
            {
                if (p->right)
                    pn = p->right;
                else
                {
                    if (sp == 0)
                        break;
                    pn = stack[--sp];
                }
            } // if pn else if !pn
            free(p);
            p = pn;
        } // while true
#endif // else ! SMART_WALK_TREE
    } // for i
    free(table);
}
