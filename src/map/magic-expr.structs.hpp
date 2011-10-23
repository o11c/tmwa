#ifndef MAGIC_EXPR_STRUCTS
#define MAGIC_EXPR_STRUCTS

#include "magic.structs.hpp"

/*
 * Argument types:
 *  i : sint32
 *  d : dir
 *  s : string
 *  e : entity
 *  l : location
 *  a : area
 *  S : spell
 *  I : invocation
 *  . : any, except for fail/undef
 *  _ : any, including fail, but not undef
 */
struct fun_t
{
    const char *signature;
    char ret_ty;
    bool (*fun)(val_t& result, val_t *args);
};

struct op_t
{
    const char *signature;
    bool (*op)(env_t *env, val_t *args);
};

#endif // MAGIC_EXPR_STRUCTS
