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
    int  (*proc) (const int, struct map_session_data *,
                  const char *command, const char *message);
    AtCommandCategory cat;
    const char *arg_help;
    const char *long_help;
};

/// Execute an atcommand, return true if it succeeded
bool is_atcommand (const int fd, struct map_session_data *sd,
                   const char *message, gm_level_t gmlvl);

// int  get_atcommand_level (const AtCommandType type);

int  atcommand_item (const int fd, struct map_session_data *sd, const char *command, const char *message);  // [Valaris]
int  atcommand_warp (const int fd, struct map_session_data *sd, const char *command, const char *message);  // [Yor]
int  atcommand_spawn (const int fd, struct map_session_data *sd, const char *command, const char *message); // [Valaris]
int  atcommand_goto (const int fd, struct map_session_data *sd, const char *command, const char *message);  // [Yor]
int  atcommand_recall (const int fd, struct map_session_data *sd, const char *command, const char *message);    // [Yor]

int  atcommand_config_read (const char *cfgName);

void log_atcommand (struct map_session_data *sd, const char *fmt, ...) __attribute__((format (printf, 2, 3)));
void gm_log (const char *fmt, ...) __attribute__((format (printf, 1, 2)));

#endif // ATCOMMAND_H
