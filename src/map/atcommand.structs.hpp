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

typedef sint32 (*atcommand_proc)(sint32, MapSessionData *, const char *);

struct AtCommandInfo
{
    const char *command;
    gm_level_t level;
    atcommand_proc proc;
    AtCommandCategory cat;
    const char *arg_help;
    const char *long_help;

    AtCommandInfo(const char *cmd, int lv, atcommand_proc pr,
                  AtCommandCategory atcc,
                  const char *synopsis, const char *help)
    : command(cmd)
    , level(lv)
    , proc(pr)
    , cat(atcc)
    , arg_help(synopsis)
    , long_help(help)
    {}
};

#endif //ATCOMMAND_STRUCTS
