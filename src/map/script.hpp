#ifndef SCRIPT_HPP
#define SCRIPT_HPP

# include "script.structs.hpp"
# include "map.structs.hpp"

# include <string>

std::vector<Script> parse_script(const char *, const char *, int32_t) = delete;
std::vector<Script> parse_script(const std::string&, const char *, int32_t);
int32_t run_script_l(const std::vector<Script>&, int32_t, int32_t, int32_t, int32_t, ArgRec *args);
int32_t run_script(const std::vector<Script>&, int32_t, int32_t, int32_t);

struct dbt *script_get_label_db(void) __attribute__((pure));
struct dbt *script_get_userfunc_db(void);

void pre_init_script();
void do_init_script(void);
void do_final_script(void);

extern builtin_function_t builtin_functions[];

void set_reg_s(MapSessionData *sd, int32_t num, const char *name, const char *str) = delete;
void set_reg_s(MapSessionData *sd, int32_t num, const char *name, const std::string& str) = delete;
void set_reg_s(MapSessionData *sd, int32_t num, const std::string& name, const char *str) = delete;
void set_reg_s(MapSessionData *sd, int32_t num, const std::string& name, const std::string& str);
void set_reg_i(MapSessionData *sd, int32_t num, const char *name, int32_t val) = delete;
void set_reg_i(MapSessionData *sd, int32_t num, const std::string& name, int32_t val);
std::string get_reg_s(MapSessionData *sd, int32_t num, const char *name) = delete;
std::string get_reg_s(MapSessionData *sd, int32_t num, const std::string& name);
int32_t get_reg_i(MapSessionData *sd, int32_t num, const char *name) = delete;
int32_t get_reg_i(MapSessionData *sd, int32_t num, const std::string& name);

// Register a name in str_data
int32_t add_str(const char *p, size_t len);

/// convenience function
inline int32_t add_str(const char *p)
{
    return add_str(p, strlen(p));
}

void mapreg_setreg(int32_t num, int32_t val);
void mapreg_setregstr(int32_t num, const char *str) = delete;
void mapreg_setregstr(int32_t num, const std::string& str);

#endif // SCRIPT_HPP
