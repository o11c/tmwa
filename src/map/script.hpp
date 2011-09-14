#ifndef SCRIPT_HPP
#define SCRIPT_HPP

#include "script.structs.hpp"

const char *parse_script(const char *, int32_t);
int32_t run_script_l(const char *, int32_t, int32_t, int32_t, int32_t, ArgRec *args);
int32_t run_script(const char *, int32_t, int32_t, int32_t);

struct dbt *script_get_label_db(void) __attribute__((pure));
struct dbt *script_get_userfunc_db(void);

int32_t script_config_read(const char *cfgName);
int32_t do_init_script(void);
int32_t do_final_script(void);

extern char mapreg_txt[];

#endif // SCRIPT_HPP
