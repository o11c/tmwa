#ifndef MAGIC_STRUCTS
#define MAGIC_STRUCTS

# include "map.structs.hpp"

# include "../common/timer.structs.hpp"
# include "../common/socket.structs.hpp"

# include "../lib/string.hpp"

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
    BAR,
};

struct area_t
{
    union
    {
        location_t a_loc;
        struct
        {
            location_t loc;
            int width, depth;
            Direction dir;
        } a_bar;
        struct
        {
            location_t loc;
            int width, height;
        } a_rect;
        area_t *a_union[2];
    };
    int size;
    AreaType ty;
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
            int args_nr, *formals;
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

// not (yet) an enum class because
// 1. it must be bitmasked
//      SpellFlag operator | (SpellFlag, SpellFlag);
//      bool operator & (SpellFlag, SpellFlag);
// 2. it gets put in a YACC variable
// 2 is not really a problem, I figured out how to add 2 other types already
namespace SpellFlag
{
    /// spell associated not with caster but with place
    const int LOCAL    = (1 << 0);
    /// spell invocation never uttered
    const int SILENT   = (1 << 1);
    /// `magic word' only:  don't require spellcasting ability
    const int NONMAGIC = (1 << 2);
};

struct spell_t
{
    POD_string name;
    POD_string invocation;
    // Relative location in the definitions file
    int idx;
    int flags;
    int arg;
    SpellArgType spellarg_ty;

    int letdefs_nr;
    letdef_t *letdefs;

    spellguard_t *spellguard;
};


struct teleport_anchor_t
{
    POD_string name;
    POD_string invocation;
    expr_t *location;
};

// The configuration
// FIXME this is implemented in magic-base.cpp, this is the wrong header
namespace magic_conf
{
    extern std::vector<std::pair<POD_string, val_t>> vars;

    //extern int obscure_chance;
    //extern int min_casttime;

    extern std::map<POD_string, spell_t *> spells;

    extern std::map<POD_string, teleport_anchor_t *> anchors;
};


/// This represents asserted intern indices, NOT an enum
namespace Var
{
    const int MIN_CASTTIME = 0;
    const int OBSCURE_CHANCE = 1;
    const int CASTER = 2;
    const int SPELLPOWER = 3;
    const int SPELL = 4;
    const int INVOCATION = 5;
    const int TARGET = 6;
    const int SCRIPTTARGET = 7;
    const int LOCATION = 8;
};

struct env_t
{
    val_t *vars;
};

// nasty macro, captures a scope variable called "env"
# define VAR(i) ((!env->vars || env->vars[i].ty == TY::UNDEF)? magic_conf::vars[i].second : env->vars[i])

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
            int args_nr, *formals;
            val_t *old_actuals;
        } c_proc;
    };
    ContStackType ty;
};

struct status_change_ref_t
{
    int sc_type;
    int bl_id;
};

// not (yet) an enum class
namespace InvocationFlag
{
    /// Bound directly to the caster (i.e., ignore its location)
    const int BOUND      = (1 << 0);
    /// Used `abort' to terminate
    const int ABORTED    = (1 << 1);
    /// On magical attacks:  if we run out of steam, stop attacking altogether
    const int STOPATTACK = (1 << 2);
};

struct invocation_t : public BlockList
{
    // linked-list of spells directly associated with a caster
    invocation_t *next_invocation;
    int flags;

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
struct args_rec_t
{
    int args_nr;
    expr_t **args;
};

struct proc_t
{
    POD_string name;
    int args_nr;
    int *args;
    effect_t *body;
};

#endif // MAGIC_STRUCTS
