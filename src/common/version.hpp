#ifndef VERSION_H
#define VERSION_H

// Note: char server is also inter server - splitting them is not supported
# define ATHENA_SERVER_LOGIN     1
# define ATHENA_SERVER_CHAR      2
# define ATHENA_SERVER_INTER     4
# define ATHENA_SERVER_MAP       8

// The body of packet 0x7531
struct Version
{
    // old versions send 255, 'T', 'M', 'W' to client
    uint8_t major, minor, rev;

    uint8_t dev_flag;

    /// Info flags
    // bit 0 set if registration enabled
    uint8_t info_flags;
    /// The #defines above
    uint8_t what_server;
    /// Custom modification count. 0 for all official tmwA releases.
    // OTOH, this is the only number changed for eA releases
    uint16_t mod_version;
};

const Version tmwAthenaVersion =
{
    // I'm using yy-mm-dd since 2000
    11, 5, 9,
    // set to 0 for stable releases, keep as 1 in git
    1,

    // these 2 get overridden
    0, 0,
    // 0 for official tmwA
    0
};
#endif // VERSION_H
