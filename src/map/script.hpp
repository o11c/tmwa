#ifndef SCRIPT_HPP
#define SCRIPT_HPP

#include "script.structs.hpp"

char *parse_script(char *, int);
int run_script_l(char *, int, int, int, int, argrec_t * args);
int run_script(char *, int, int, int);

struct dbt *script_get_label_db(void);
struct dbt *script_get_userfunc_db(void);

int script_config_read(const char *cfgName);
int do_init_script(void);
int do_final_script(void);

extern char mapreg_txt[];

#endif // SCRIPT_HPP
