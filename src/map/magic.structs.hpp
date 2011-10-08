#ifndef MAGIC_STRUCTS
#define MAGIC_STRUCTS

# include "map.structs.hpp"

# include "../common/timer.structs.hpp"
# include "../common/socket.structs.hpp"

# include "../lib/string.hpp"
# include "../lib/darray.hpp"
# include "../lib/enum.hpp"
# include "../lib/fixed_stack.hpp"
# include "../lib/placed.hpp"

# define MAGIC_CONFIG_FILE "conf/magic.conf"

struct spell_t;
struct expr_t;
struct fun_t;
struct op_t;

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
    int32_t m;
    uint16_t x, y;
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
            uint32_t width, height;
        } a_rect;
        area_t *a_union[2];
    };
    /// number of cells in the area
    // used (only) to "pick a random location in the area"
    int32_t size;
    AreaType ty;

    // defined in magic-expr.cpp
    ~area_t();
    area_t(const area_t &);

    // point
    area_t(const location_t&);
    // union
    area_t(area_t *, area_t *);
    // rectangle
    area_t(const location_t&, int32_t, int32_t);
    // bar (whatever that is)
    area_t(const location_t&, int32_t, int32_t, Direction);

    // in magic-expr.cpp
    location_t rect(uint32_t& w, uint32_t& h);
    location_t random_location();
    bool contains(location_t);
};

struct val_t
{
    union
    {
        int32_t v_int;
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
            // *grumble* why does it need to know the name?
            const std::pair<const std::string, fun_t> *fun;
            int32_t line_nr, column;
            int32_t args_nr;
            expr_t *args[MAX_ARGS];
        } e_funapp;
        int32_t e_id;
        struct
        {
            expr_t *expr;
            int32_t id;
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
            int32_t id;
            expr_t *expr;
        } e_assign;
        struct
        {
            int32_t var_id;
            expr_t *area;
            effect_t *body;
            ForEach_FilterType filter;
        } e_foreach;
        struct
        {
            int32_t var_id;
            expr_t *start, *stop;
            effect_t *body;
        } e_for;
        struct
        {
            expr_t *cond;
            effect_t *true_branch, *false_branch;
        } e_if;
        expr_t *e_sleep;
        const std::vector<Script> *e_script;
        struct
        {
            const std::pair<const std::string, op_t> *op;
            int32_t args_nr;
            int32_t line_nr, column;
            expr_t *args[MAX_ARGS];
        } e_op;
        struct
        {
            int32_t args_nr;
            DArray<int32_t> formals;
            expr_t **actuals;
            effect_t *body;
        } e_call;
    };
    EffectType ty;
};


struct component_t
{
    component_t *next;
    int32_t item_id;
    int32_t count;
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
    int32_t id;
    expr_t *expr;
};

BIT_ENUM(SpellFlag, uint8_t)
{
    NONE        = 0,

    LOCAL       = 1 << 0,
    SILENT      = 1 << 1,
    NONMAGIC    = 1 << 2,

    ALL = LOCAL | SILENT | NONMAGIC
};

struct spell_t
{
    POD_string name;
private:
    static int32_t spell_counter;
public:
    // implemented in magic-parser.ypp
    spell_t(spellguard_t *spellguard);
    // Relative location in the definitions file
    int32_t idx;
    SpellFlag flags;
    int32_t arg;
    SpellArgType spellarg_ty;

    int32_t letdefs_nr;
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
    val_t& VAR(int32_t i);
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
            int32_t var_id;
            TY ty;
            effect_t *body;
            std::vector<int32_t> entities;
        } c_foreach;
        struct
        {
            int32_t var_id;
            effect_t *body;
            int32_t current;
            int32_t stop;
        } c_for;
        struct
        {
            int32_t args_nr;
            DArray<int32_t> formals;
            DArray<val_t> old_actuals;
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
    // this destructor only destructs the appropriate union member
    ~cont_activation_record_t()
    {
        switch(ty)
        {
        case ContStackType::FOREACH:
            destruct(c_foreach);
            break;
        case ContStackType::FOR:
            destruct(c_for);
            break;
        case ContStackType::PROC:
            destruct(c_proc);
            break;
        }
    }
};

struct status_change_ref_t
{
    int32_t sc_type;
    int32_t bl_id;
};

inline bool operator ==(const status_change_ref_t& lhs, const status_change_ref_t& rhs)
{
    return lhs.sc_type == rhs.sc_type && lhs.bl_id == rhs.bl_id;
}

BIT_ENUM(InvocationFlag, uint8_t)
{
    NONE = 0,

    // removed since it is the same as checking whether there is a subject
//    BOUND        = 1 << 0,
    ABORTED     = 1 << 1,
    STOPATTACK  = 1 << 2,

    ALL = ABORTED | STOPATTACK
};

struct invocation_t : public BlockList
{
    InvocationFlag flags;

    env_t *env;
    spell_t *spell;
    // this is the person who originally invoked the spell
    int32_t caster;
    // when this person dies, the spell dies with it
    int32_t subject;

    // spell timer, if any
    timer_id timer;

    fixed_stack<cont_activation_record_t, MAX_STACK_SIZE> stack;

    // Script position; if nonzero, resume the script we were running.
    int32_t script_pos;
    effect_t *current_effect;
    // If non-NULL, this is used to spawn a cloned effect based on the same environment
    effect_t *trigger_effect;
    // If non-NULL, this is executed when the spell terminates naturally, e.g. when all status changes have run out or all delays are over.
    effect_t *end_effect;

    // Status change references:  for status change updates, keep track of whom we updated where
    std::vector<status_change_ref_t> status_change_refs;

    invocation_t() : BlockList(BL_SPELL) {}
    invocation_t(const invocation_t&) = delete;
    // in magic-base.cpp
    invocation_t(invocation_t* rhs);
};

// Fake default environment
extern env_t magic_default_env;

// The following is used only by the parser:
// it cannot become a vector because it is used in a union
struct args_rec_t
{
    int32_t args_nr;
    expr_t *args[MAX_ARGS];
};

struct proc_t
{
    POD_string name;
    DArray<int32_t> args;
    effect_t *body;
};

extern template class std::vector<status_change_ref_t>;
extern template class std::vector<int32_t>;

#endif // MAGIC_STRUCTS
