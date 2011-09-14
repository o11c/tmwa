#ifndef ATCOMMAND_STRUCTS
#define ATCOMMAND_STRUCTS

# include "../common/mmo.hpp"

// categories based on the old conf/help.txt
enum class AtCommandCategory
{
    UNK,
    MISC,
    INFO,
    MSG,
    SELF,
    MOB,
    ITEM,
    GROUP,
    CHAR,
    ENV,
    ADMIN,
};

struct AtCommandInfo
{
    const char *command;
    gm_level_t level;
    int32_t (*proc)(const int32_t, MapSessionData *,
                const char *command, const char *message);
    AtCommandCategory cat;
    const char *arg_help;
    const char *long_help;
};

#endif //ATCOMMAND_STRUCTS
