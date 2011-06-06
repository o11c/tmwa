#ifndef ATCOMMAND_H
#define ATCOMMAND_H

#include "map.hpp"

// categories based on the old conf/help.txt
enum AtCommandCategory
{
    ATCC_UNK,
    ATCC_MISC,
    ATCC_INFO,
    ATCC_MSG,
    ATCC_SELF,
    ATCC_MOB,
    ATCC_ITEM,
    ATCC_GROUP,
    ATCC_CHAR,
    ATCC_ENV,
    ATCC_ADMIN,
};

struct AtCommandInfo
{
    const char *command;
    gm_level_t level;
    int (*proc)(const int, MapSessionData *,
                const char *command, const char *message);
    AtCommandCategory cat;
    const char *arg_help;
    const char *long_help;
};

/// Execute an atcommand, return true if it succeeded
bool is_atcommand(const int fd, MapSessionData *sd,
                  const char *message, gm_level_t gmlvl);

void atcommand_config_read(const char *cfgName);

void gm_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif // ATCOMMAND_H
