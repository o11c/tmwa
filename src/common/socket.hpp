#ifndef SOCKET_HPP
#define SOCKET_HPP

# include "socket.structs.hpp"

# include "timer.structs.hpp"

# include <cstdio>

inline void socket_data::rfifo_change_packet(uint16 newpacket)
{
    *reinterpret_cast<uint16 *>(rdata + rdata_pos) = newpacket;
}

# define FIFOSIZE_SERVERLINK    (256 * 1024)

// save file descriptors for important stuff
# define SOFT_LIMIT (FD_SETSIZE - 50)

// socket timeout to establish a full connection in seconds
# define CONNECT_TIMEOUT 15

/// Everyone who has connected
// note: call delete_session(i) to null out an element
extern struct socket_data *session[FD_SETSIZE];

/// Maximum used FD, +1
extern sint32 fd_max;

/// open a socket, bind, and listen. Return an fd, or -1 if socket() fails,
/// but exit if bind() or listen() fails
sint32 make_listen_port(uint16 port);
/// Connect to an address, return a connected socket or -1
sint32 make_connection(IP_Address ip, uint16 port);
/// free() the structure and close() the fd
void delete_session(sint32);
/// Make a the internal queues bigger
void realloc_fifo(sint32 fd, size_t rfifo_size, size_t wfifo_size);
/// Update all sockets that can be read/written from the queues
void do_sendrecv(interval_t next);
/// Call the parser function for every socket that has read data
void do_parsepacket(void);

/// An init function
void do_socket(void);

/// Change the default parser for newly connected clients
// typically called once per server, but individual clients may identify
// themselves as servers
void set_defaultparse(void (*defaultparse)(sint32));

/// Wrappers to track number of free FDs
void fclose_(FILE * fp);
FILE *fopen_(const char *path, const char *mode);
bool free_fds(void) __attribute__((pure));



// TODO replace these with inline functions since we're not superstitious
// maybe members of socket_data?
/// Check how much can be read
inline size_t RFIFOREST(sint32 fd)
{
    return session[fd]->rdata_size - session[fd]->rdata_pos;
}
/// Read from the queue
inline const uint8 *RFIFOP(sint32 fd, size_t pos)
{
    return session[fd]->rdata + session[fd]->rdata_pos + pos;
}
inline uint8 RFIFOB(sint32 fd, size_t pos)
{
    return *reinterpret_cast<const uint8 *>(RFIFOP(fd, pos));
}
inline uint16 RFIFOW(sint32 fd, size_t pos)
{
    return *reinterpret_cast<const uint16 *>(RFIFOP(fd, pos));
}
inline uint32 RFIFOL(sint32 fd, size_t pos)
{
    return *reinterpret_cast<const uint32 *>(RFIFOP(fd, pos));
}

/// Done reading
void RFIFOSKIP(sint32 fd, size_t len);

/// Read from an arbitrary buffer
inline const uint8 *RBUFP(uint8 *p, size_t pos)
{
    return p + pos;
}
inline uint8 RBUFB(uint8 *p, size_t pos)
{
    return *reinterpret_cast<const uint8 *>(RBUFP(p, pos));
}
inline uint16 RBUFW(uint8 *p, size_t pos)
{
    return *reinterpret_cast<const uint16 *>(RBUFP(p, pos));
}
inline uint32 RBUFL(uint8 *p, size_t pos)
{
    return *reinterpret_cast<const uint32 *>(RBUFP(p, pos));
}


/// Write to the queue
inline uint8 *WFIFOP(sint32 fd, size_t pos)
{
    return session[fd]->wdata + session[fd]->wdata_size + pos;
}
inline uint8& WFIFOB(sint32 fd, size_t pos)
{
    return *reinterpret_cast<uint8 *>(WFIFOP(fd, pos));
}
inline uint16& WFIFOW(sint32 fd, size_t pos)
{
    return *reinterpret_cast<uint16 *>(WFIFOP(fd, pos));
}
inline uint32& WFIFOL(sint32 fd, size_t pos)
{
    return *reinterpret_cast<uint32 *>(WFIFOP(fd, pos));
}
/// Finish writing
void WFIFOSET(sint32 fd, size_t len);

/// Write to an arbitrary buffer
inline uint8 *WBUFP(uint8 *p, size_t pos)
{
    return p + pos;
}
inline uint8& WBUFB(uint8 *p, size_t pos)
{
    return *reinterpret_cast<uint8 *>(WBUFP(p, pos));
}
inline uint16& WBUFW(uint8 *p, size_t pos)
{
    return *reinterpret_cast<uint16 *>(WBUFP(p, pos));
}
inline uint32& WBUFL(uint8 *p, size_t pos)
{
    return *reinterpret_cast<uint32 *>(WBUFP(p, pos));
}

#endif // SOCKET_HPP
