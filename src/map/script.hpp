#ifndef SCRIPT_H
#define SCRIPT_H

/// This really should be plain char, but there is a lot of bad code
typedef unsigned char *script_ptr;

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
    script_ptr script, new_script;
    int defsp, new_pos, new_defsp;
};

script_ptr parse_script(script_ptr, int);
typedef struct argrec
{
    const char *name;
    union
    {
        int i;
        const char *s;
    } v;
} argrec_t;
int run_script_l(script_ptr, int, int, int, int, argrec_t * args);
int run_script(script_ptr, int, int, int);

struct dbt *script_get_label_db(void);
struct dbt *script_get_userfunc_db(void);

int script_config_read(const char *cfgName);
int do_init_script(void);
int do_final_script(void);

extern char mapreg_txt[];

#endif // SCRIPT_H
