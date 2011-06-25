#ifndef SCRIPT_STRUCTS
#define SCRIPT_STRUCTS

struct script_data
{
    int type;
    union
    {
        int num;
        const char *str;
    } u;
};

struct script_stack
{
    int sp, sp_max;
    struct script_data *stack_data;
};
struct script_state
{
    struct script_stack *stack;
    int start, end;
    int pos, state;
    int rid, oid;
    char *script, *new_script;
    int defsp, new_pos, new_defsp;
};

typedef struct argrec
{
    const char *name;
    union
    {
        int i;
        const char *s;
    } v;
} argrec_t;

#endif //SCRIPT_STRUCTS
