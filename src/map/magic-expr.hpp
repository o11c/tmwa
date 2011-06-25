#ifndef MAGIC_EXPR_HPP
#define MAGIC_EXPR_HPP

# include "magic-expr.structs.hpp"

# include "magic.structs.hpp"

/**
 * Retrieves a function by name
 * @param name The name to look up
 * @return A function of that name, or NULL, and a function index
 */
fun_t *magic_get_fun(const char *name, int *index);

/**
 * Retrieves an operation by name
 * @param name The name to look up
 * @return An operation of that name, or NULL, and a function index
 */
op_t *magic_get_op(char *name, int *index);

/**
 * Evaluates an expression and stores the result in `dest'
 */
void magic_eval(env_t * env, val_t * dest, expr_t * expr);

/**
 * Evaluates an expression and coerces the result into an integer
 */
int magic_eval_int(env_t * env, expr_t * expr);

/**
 * Evaluates an expression and coerces the result into a string
 */
char *magic_eval_str(env_t * env, expr_t * expr);

int map_is_solid(int m, int x, int y);

expr_t *magic_new_expr(int ty);

void magic_clear_var(val_t * v);

void magic_copy_var(val_t * dest, val_t * src);

void magic_random_location(location_t * dest, area_t * area);

/// ret -1: not a string, ret 1: no such item, ret 0: OK
int magic_find_item(val_t * args, int index, struct item *item, int *stackable);

# define GET_ARG_ITEM(index, dest, stackable) switch (magic_find_item(args, index, &dest, &stackable)) { case -1 : return 1; case 1 : return 0; }

int magic_signature_check(const char *opname, const char *funname, const char *signature,
                          int args_nr, val_t * args, int line, int column);

void magic_area_rect(int *m, int *x, int *y, int *width, int *height, area_t *area);

# define ARGINT(x) args[x].v.v_int
# define ARGDIR(x) args[x].v.v_int
# define ARGSTR(x) args[x].v.v_string
# define ARGENTITY(x) args[x].v.v_entity
# define ARGLOCATION(x) args[x].v.v_location
# define ARGAREA(x) args[x].v.v_area
# define ARGSPELL(x) args[x].v.v_spell
# define ARGINVOCATION(x) args[x].v.v_invocation

# define RESULTINT result->v.v_int
# define RESULTDIR result->v.v_int
# define RESULTSTR result->v.v_string
# define RESULTENTITY result->v.v_entity
# define RESULTLOCATION result->v.v_location
# define RESULTAREA result->v.v_area
# define RESULTSPELL result->v.v_spell
# define RESULTINVOCATION result->v.v_invocation

# define TY(x) args[x].ty
# define ETY(x) ARGENTITY(x)->type

# define ARGPC(x)    static_cast<MapSessionData *>(ARGENTITY(x))
# define ARGNPC(x)   static_cast<struct npc_data *>(ARGENTITY(x))
# define ARGMOB(x)   static_cast<struct mob_data *>(ARGENTITY(x))

# define ARG_MAY_BE_AREA(x) (TY(x) == TY_AREA || TY(x) == TY_LOCATION)

#endif // MAGIC_EXPR_HPP
