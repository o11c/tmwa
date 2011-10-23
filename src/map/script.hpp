#ifndef SCRIPT_HPP
#define SCRIPT_HPP

# include "script.structs.hpp"
# include "main.structs.hpp"

std::vector<Script> parse_script(const char *, const char *, sint32) = delete;
std::vector<Script> parse_script(const std::string&, const char *, sint32);
sint32 run_script_l(const std::vector<Script>&, sint32, account_t, BlockID, sint32, ArgRec *args);
sint32 run_script(const std::vector<Script>&, sint32, account_t, BlockID);

struct dbt *script_get_label_db(void) __attribute__((pure));
struct dbt *script_get_userfunc_db(void);

void pre_init_script();
void do_init_script(void);
void do_final_script(void);

extern builtin_function_t builtin_functions[];

void set_reg_s(MapSessionData *sd, sint32 num, const char *name, const char *str) = delete;
void set_reg_s(MapSessionData *sd, sint32 num, const char *name, const std::string& str) = delete;
void set_reg_s(MapSessionData *sd, sint32 num, const std::string& name, const char *str) = delete;
void set_reg_s(MapSessionData *sd, sint32 num, const std::string& name, const std::string& str);
void set_reg_i(MapSessionData *sd, sint32 num, const char *name, sint32 val) = delete;
void set_reg_i(MapSessionData *sd, sint32 num, const std::string& name, sint32 val);
std::string get_reg_s(MapSessionData *sd, sint32 num, const char *name) = delete;
std::string get_reg_s(MapSessionData *sd, sint32 num, const std::string& name);
sint32 get_reg_i(MapSessionData *sd, sint32 num, const char *name) = delete;
sint32 get_reg_i(MapSessionData *sd, sint32 num, const std::string& name);

// Register a name in str_data
sint32 add_str(const char *p, size_t len);

/// convenience function
inline sint32 add_str(const char *p)
{
    return add_str(p, strlen(p));
}

void mapreg_setreg(sint32 num, sint32 val);
void mapreg_setregstr(sint32 num, const char *str) = delete;
void mapreg_setregstr(sint32 num, const std::string& str);

#endif // SCRIPT_HPP
