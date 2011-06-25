#ifndef ATCOMMAND_HPP
#define ATCOMMAND_HPP

# include "atcommand.structs.hpp"

# include "map.structs.hpp"

/// Execute an atcommand, return true if it succeeded
bool is_atcommand(const int fd, MapSessionData *sd,
                  const char *message, gm_level_t gmlvl);

void atcommand_config_read(const char *cfgName);

void gm_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

extern char *gm_logfile_name;

#endif // ATCOMMAND_HPP
