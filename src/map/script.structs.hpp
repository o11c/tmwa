#ifndef SCRIPT_STRUCTS
#define SCRIPT_STRUCTS

struct script_data
{
    int32_t type;
    union
    {
        int32_t num;
        const char *str;
    } u;
};

struct script_stack
{
    int32_t sp, sp_max;
    struct script_data *stack_data;
};
struct script_state
{
    struct script_stack *stack;
    int32_t start, end;
    int32_t pos, state;
    int32_t rid, oid;
    const char *script, *new_script;
    int32_t defsp, new_pos, new_defsp;
};

struct ArgRec
{
    const char *name;
    union
    {
        int32_t i;
        const char *s;
    };

    ArgRec(const char *n, int32_t v) : name(n), i(v) {}
    ArgRec(const char *n, const char *v) : name(n), s(v) {}
};

#endif //SCRIPT_STRUCTS
