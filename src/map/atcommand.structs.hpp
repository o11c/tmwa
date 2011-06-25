#ifndef ATCOMMAND_STRUCTS
#define ATCOMMAND_STRUCTS

# include "../common/mmo.hpp"

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

#endif //ATCOMMAND_STRUCTS
