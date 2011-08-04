#ifndef MAGIC_STRUCTS
#define MAGIC_STRUCTS

# include "map.structs.hpp"

# include "../common/timer.structs.hpp"
# include "../common/socket.structs.hpp"

# include "../lib/string.hpp"
# include "../lib/darray.hpp"
# include "../lib/enum.hpp"

# include <vector>
# include <map>

# define MAGIC_CONFIG_FILE "conf/magic.conf"

struct spell_t;
struct expr_t;

enum class SpellArgType
{
    NONE,
    PC,
    STRING,
};

enum class TY : uint8_t
{
    UNDEF,
    INT,
    DIR,
    STRING,
    ENTITY,
    LOCATION,
    AREA,
    SPELL,
    INVOCATION,
    FAIL = 127,
};

struct location_t
{
    int m;
    int x, y;
};

enum class AreaType : uint8_t
{
    LOCATION,
    UNION,
    RECT,
};

struct area_t
{
    union
    {
        location_t a_loc;
        struct
        {
            location_t loc;
            int width, height;
        } a_rect;
        area_t *a_union[2];
    };
    /// number of cells in the area
    // used (only) to "pick a random location in the area"
    int size;
    AreaType ty;

    // defined in magic-expr.cpp
    ~area_t();
    area_t(const area_t &);

    // point
    area_t(const location_t&);
    // union
    area_t(area_t *, area_t *);
    // rectangle
    area_t(const location_t&, int, int);
    // bar (whatever that is)
    area_t(const location_t&, int, int, Direction);
};

struct val_t
{
    union
    {
        int v_int;
        POD_string v_string;
        // Used ONLY during operation/function invocation; otherwise we use v_int
        BlockList *v_entity;
        area_t *v_area;
        location_t v_location;
        // Used ONLY during operation/function invocation; otherwise we use v_int
        invocation_t *v_invocation;
        spell_t *v_spell;
        Direction v_dir;
    };
    TY ty;
};


/// Max. # of args used in builtin primitive functions
# define MAX_ARGS 7

enum class ExprType
{
    VAL,
    LOCATION,
    AREA,
    FUNAPP,
    ID,
    SPELLFIELD,
};

struct e_location_t
{
    expr_t *m, *x, *y;
};

struct e_area_t
{
    union
    {
        e_location_t a_loc;
        struct
        {
            e_location_t loc;
            expr_t *width, *depth, *dir;
        } a_bar;
        struct
        {
            e_location_t loc;
            expr_t *width, *height;
        } a_rect;
        e_area_t *a_union[2];
    };
    AreaType ty;
};

struct expr_t
{
    union
    {
        val_t e_val;
        e_location_t e_location;
        e_area_t e_area;
        struct
        {
            int id, line_nr, column;
            int args_nr;
            expr_t *args[MAX_ARGS];
        } e_funapp;
        int e_id;
        struct
        {
            expr_t *expr;
            int id;
        } e_field;
    };
    ExprType ty;
};


enum class EffectType
{
    SKIP,
    ABORT,
    ASSIGN,
    FOREACH,
    FOR,
    IF,
    SLEEP,
    SCRIPT,
    BREAK,
    OP,
    END,
    CALL,
};

enum class ForEach_FilterType
{
    MOB,
    PC,
    ENTITY,
    TARGET,
    SPELL,
    NPC,
};

struct effect_t
{
    effect_t *next;
    union
    {
        struct
        {
            int id;
            expr_t *expr;
        } e_assign;
        struct
        {
            int id;
            expr_t *area;
            effect_t *body;
            ForEach_FilterType filter;
        } e_foreach;
        struct
        {
            int id;
            expr_t *start, *stop;
            effect_t *body;
        } e_for;
        struct
        {
            expr_t *cond;
            effect_t *true_branch, *false_branch;
        } e_if;
        expr_t *e_sleep;        /* sleep time */
        const char *e_script;
        struct
        {
            int id;
            int args_nr;
            int line_nr, column;
            expr_t *args[MAX_ARGS];
        } e_op;
        struct
        {
            int args_nr;
            DArray<int> formals;
            expr_t **actuals;
            effect_t *body;
        } e_call;
    };
    EffectType ty;
};


struct component_t
{
    component_t *next;
    int item_id;
    int count;
};


enum class SpellGuardType
{
    CONDITION,
    COMPONENTS,
    CATALYSTS,
    CHOICE,
    MANA,
    CASTTIME,
    EFFECT,
};

struct effect_set_t
{
    effect_t *effect, *at_trigger, *at_end;
};

struct spellguard_t
{
    spellguard_t(SpellGuardType type);

    spellguard_t *next;
    union
    {
        expr_t *s_condition;
        expr_t *s_mana;
        expr_t *s_casttime;
        component_t *s_components;
        component_t *s_catalysts;
        // either `next' or `s.s_alt'
        spellguard_t *s_alt;
        effect_set_t s_effect;
    };
    SpellGuardType ty;
};


struct letdef_t
{
    int id;
    expr_t *expr;
};

SHIFT_ENUM(SpellFlag, uint8_t)
{
    LOCAL, SILENT, NONMAGIC
};

struct spell_t
{
    POD_string name;
private:
    static int spell_counter;
public:
    // implemented in magic-parser.ypp
    spell_t(spellguard_t *spellguard);
    // Relative location in the definitions file
    int idx;
    SpellFlag flags;
    int arg;
    SpellArgType spellarg_ty;

    int letdefs_nr;
    letdef_t *letdefs;

    spellguard_t *spellguard;
};


/// This represents asserted intern indices, NOT really an enum
namespace Var
{
    enum
    {
        MIN_CASTTIME,
//         OBSCURE_CHANCE,
        CASTER,
        SPELLPOWER,
        SPELL,
        INVOCATION,
        TARGET,
        SCRIPTTARGET,
        LOCATION,
    };
};

struct env_t
{
    env_t(std::nullptr_t) : vars(NULL) {}
    // defined in magic-base.cpp
    // which is the only user of the constructors
    // TODO maybe replace with shared_ptr?
    env_t();
    env_t(const env_t&);
    ~env_t();

    val_t *vars;
    // in magic-expr.cpp
    val_t magic_eval(expr_t *expr);
    // implemented inline in magic-base.hpp
    val_t& VAR(int i);
};

# define MAX_STACK_SIZE 32

enum class ContStackType
{
    FOREACH,
    FOR,
    PROC,
};

struct cont_activation_record_t
{
    effect_t *return_location;
    union
    {
        struct
        {
            int id;
            TY ty;
            effect_t *body;
            int entities_nr;
            int *entities;
            int idx;
        } c_foreach;
        struct
        {
            int id;
            effect_t *body;
            int current;
            int stop;
        } c_for;
        struct
        {
            int args_nr;
            DArray<int> formals;
            val_t *old_actuals;
        } c_proc;
    };
    ContStackType ty;
    cont_activation_record_t() : ty(ContStackType::FOR) {}
    cont_activation_record_t(ContStackType type, effect_t *rl) : return_location(rl), ty(type) {}
    cont_activation_record_t(const cont_activation_record_t& r) : return_location(r.return_location), ty(r.ty)
    {
        switch(ty)
        {
        case ContStackType::FOREACH:
            new(&c_foreach)decltype(c_foreach)(r.c_foreach);
            break;
        case ContStackType::FOR:
            new(&c_for)decltype(c_for)(r.c_for);
            break;
        case ContStackType::PROC:
            new(&c_proc)decltype(c_proc)(r.c_proc);
            break;
        }
    }
    ~cont_activation_record_t()
    {
        if (ty == ContStackType::PROC)
        {
            c_proc.formals.~DArray<int>();
        }
    }
};

struct status_change_ref_t
{
    int sc_type;
    int bl_id;
};

SHIFT_ENUM(InvocationFlag, uint8_t)
{
    // removed since it is the same as checking whether there is a subject
//    BOUND,
    ABORTED,
    STOPATTACK
};

struct invocation_t : public BlockList
{
    InvocationFlag flags;

    env_t *env;
    spell_t *spell;
    // this is the person who originally invoked the spell
    int caster;
    // when this person dies, the spell dies with it
    int subject;

    // spell timer, if any
    timer_id timer;

    int stack_size;
    cont_activation_record_t stack[MAX_STACK_SIZE];

    // Script position; if nonzero, resume the script we were running.
    int script_pos;
    effect_t *current_effect;
    // If non-NULL, this is used to spawn a cloned effect based on the same environment
    effect_t *trigger_effect;
    // If non-NULL, this is executed when the spell terminates naturally, e.g. when all status changes have run out or all delays are over.
    effect_t *end_effect;

    // Status change references:  for status change updates, keep track of whom we updated where
    int status_change_refs_nr;
    status_change_ref_t *status_change_refs;

    invocation_t() : BlockList(BL_SPELL) {}
};

// Fake default environment
extern env_t magic_default_env;

// The following is used only by the parser:
// it cannot become a vector because it is used in a union
struct args_rec_t
{
    int args_nr;
    expr_t *args[MAX_ARGS];
};

struct proc_t
{
    POD_string name;
    DArray<int> args;
    effect_t *body;
};

#endif // MAGIC_STRUCTS
