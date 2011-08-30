#ifndef MAGIC_EXPR_HPP
#define MAGIC_EXPR_HPP

# include "magic-expr.structs.hpp"

# include "magic.structs.hpp"

/**
 * Retrieves a function by name
 * @param name The name to look up
 * @return A function of that name, or NULL
 */
const std::pair<const std::string, fun_t> *magic_get_fun(const char *name);

/**
 * Evaluates an expression and coerces the result into an integer
 */
int magic_eval_int(env_t *env, expr_t *expr);

/**
 * Evaluates an expression and coerces the result into a string
 */
POD_string magic_eval_str(env_t *env, expr_t *expr);

int map_is_solid(int m, int x, int y) __attribute__((pure));;

expr_t *magic_new_expr(ExprType ty);

void magic_clear_var(val_t *v);

void magic_copy_var(val_t *dest, val_t *src);

bool magic_find_item(val_t *args, int index, struct item *item, bool *stackable);

# define GET_ARG_ITEM(index, dest, stackable) if (magic_find_item(args, index, &dest, &stackable)) return 1;

int magic_signature_check(const char *opname, const char *funname, const char *signature,
                          int args_nr, val_t *args, int line, int column);

# define ASSERT_TYPE(x, type)   (args[x].ty == type ? 0 : throw args[x].type)

# define ARG_INT(x)         (*(TY::INT        == args[x].ty ? throw args[x].ty : &args[x].v_int))
# define ARG_DIR(x)         (*(TY::DIR        == args[x].ty ? throw args[x].ty : &args[x].v_dir))
# define ARG_STR(x)         (*(TY::STRING     == args[x].ty ? throw args[x].ty : &args[x].v_string))
# define ARG_ENTITY(x)      (*(TY::ENTITY     == args[x].ty ? throw args[x].ty : &args[x].v_entity))
# define ARG_LOCATION(x)    (*(TY::LOCATION   == args[x].ty ? throw args[x].ty : &args[x].v_location))
# define ARG_AREA(x)        (*(TY::AREA       == args[x].ty ? throw args[x].ty : &args[x].v_area))
# define ARG_SPELL(x)       (*(TY::SPELL      == args[x].ty ? throw args[x].ty : &args[x].v_spell))
# define ARG_INVOCATION(x)  (*(TY::INVOCATION == args[x].ty ? throw args[x].ty : &args[x].v_invocation))

# define ARG_PC(x)      (ARG_ENTITY(x)->type != BL_PC ? NULL : static_cast<MapSessionData *>(ARG_ENTITY(x)))
# define ARG_NPC(x)     (ARG_ENTITY(x)->type != BL_NPC ? NULL : static_cast<struct npc_data *>(ARG_ENTITY(x)))
# define ARG_MOB(x)     (ARG_ENTITY(x)->type != BL_MOB ? NULL : static_cast<struct mob_data *>(ARG_ENTITY(x)))

# define ARG_MAY_BE_AREA(x) (args[x].ty == TY::AREA || args[x].ty == TY::LOCATION)

#endif // MAGIC_EXPR_HPP
