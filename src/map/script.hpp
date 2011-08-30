#ifndef SCRIPT_HPP
#define SCRIPT_HPP

#include "script.structs.hpp"

const char *parse_script(const char *, int);
int run_script_l(const char *, int, int, int, int, ArgRec *args);
int run_script(const char *, int, int, int);

struct dbt *script_get_label_db(void) __attribute__((pure));
struct dbt *script_get_userfunc_db(void);

int script_config_read(const char *cfgName);
int do_init_script(void);
int do_final_script(void);

extern char mapreg_txt[];

#endif // SCRIPT_HPP
