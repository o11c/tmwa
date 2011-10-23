#ifndef SOCKET_STRUCTS
#define SOCKET_STRUCTS

# include "../lib/ints.hpp"
# include "../lib/ip.hpp"

// Struct declaration
class SessionData
{
    SessionData(SessionData&) = delete;
protected:
    SessionData() {}
public:
    virtual ~SessionData() {}
};

struct socket_data
{
    /// Checks whether a newly-connected socket actually does anything
    time_t created;
    bool connected;

    /// Flag needed since structure must be freed in a server-dependent manner
    bool eof;

    /// Since this is a single-threaded application, it can't block
    /// These are the read/write queues
    uint8 *rdata, *wdata;
    size_t max_rdata, max_wdata;
    /// How much is actually in the queue
    size_t rdata_size, wdata_size;
    /// How much has already been read from the queue
    /// Note that there is no need for a wdata_pos
    size_t rdata_pos;

    IP_Address client_addr;

    /// Send or recieve
    /// Only called when select() indicates the socket is ready
    /// If, after that, nothing is read, it sets eof
    // These could probably be hard-coded with a little work
    void (*func_recv)(sint32);
    void (*func_send)(sint32);
    /// This is the important one
    /// Set to different functions depending on whether the connection
    /// is a player or a server/ladmin
    /// Can be set explicitly or via set_defaultparse
    void (*func_parse)(sint32);
    /// Server-specific data type
    SessionData *session_data;

    // used when forwarding a packet with different ID
    void rfifo_change_packet(uint16 newpacket);
};

#endif //SOCKET_STRUCTS
