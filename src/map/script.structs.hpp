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
    const char *script, *new_script;
    int defsp, new_pos, new_defsp;
};

struct ArgRec
{
    const char *name;
    union
    {
        int i;
        const char *s;
    };

    ArgRec(const char *n, int v) : name(n), i(v) {}
    ArgRec(const char *n, const char *v) : name(n), s(v) {}
};

#endif //SCRIPT_STRUCTS
