#ifndef ATCOMMAND_HPP
#define ATCOMMAND_HPP

# include "atcommand.structs.hpp"

# include "../lib/cxxstdio.hpp"

# include "main.structs.hpp"

/// Execute an atcommand, return true if it succeeded
bool is_atcommand(const sint32 fd, MapSessionData *sd,
                  const char *message, gm_level_t gmlvl);

void atcommand_config_read(const char *cfgName);

void gm_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
# define GM_LOG(fmt, ...) gm_log("%s", STR_PRINTF(fmt, ##__VA_ARGS__).c_str())

extern char *gm_logfile_name;

#endif // ATCOMMAND_HPP
