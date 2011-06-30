#ifndef SOCKET_HPP
#define SOCKET_HPP

# include "socket.structs.hpp"

# include <cstdio>

inline void socket_data::rfifo_change_packet(uint16_t newpacket)
{
    *reinterpret_cast<uint16_t *>(rdata + rdata_pos) = newpacket;
}

# define FIFOSIZE_SERVERLINK    256*1024

// save file descriptors for important stuff
# define SOFT_LIMIT (FD_SETSIZE - 50)

// socket timeout to establish a full connection in seconds
# define CONNECT_TIMEOUT 15

/// Everyone who has connected
// note: call delete_session(i) to null out an element
extern struct socket_data *session[FD_SETSIZE];

/// Maximum used FD, +1
extern int fd_max;

/// open a socket, bind, and listen. Return an fd, or -1 if socket() fails,
/// but exit if bind() or listen() fails
int make_listen_port(uint16_t port);
/// Connect to an address, return a connected socket or -1
int make_connection(IP_Address ip, uint16_t port);
/// free() the structure and close() the fd
void delete_session(int);
/// Make a the internal queues bigger
void realloc_fifo(int fd, size_t rfifo_size, size_t wfifo_size);
/// Update all sockets that can be read/written from the queues
void do_sendrecv(uint32_t next);
/// Call the parser function for every socket that has read data
void do_parsepacket(void);

/// An init function
void do_socket(void);

/// Change the default parser for newly connected clients
// typically called once per server, but individual clients may identify
// themselves as servers
void set_defaultparse(void (*defaultparse)(int));

/// Wrappers to track number of free FDs
void fclose_(FILE * fp);
FILE *fopen_(const char *path, const char *mode);
bool free_fds(void) __attribute__((pure));



// TODO replace these with inline functions since we're not superstitious
// maybe members of socket_data?
/// Check how much can be read
inline size_t RFIFOREST(int fd)
{
    return session[fd]->rdata_size-session[fd]->rdata_pos;
}
/// Read from the queue
inline const uint8_t *RFIFOP(int fd, size_t pos)
{
    return session[fd]->rdata + session[fd]->rdata_pos + pos;
}
inline uint8_t RFIFOB(int fd, size_t pos)
{
    return *reinterpret_cast<const uint8_t*>(RFIFOP(fd, pos));
}
inline uint16_t RFIFOW(int fd, size_t pos)
{
    return *reinterpret_cast<const uint16_t*>(RFIFOP(fd, pos));
}
inline uint32_t RFIFOL(int fd, size_t pos)
{
    return *reinterpret_cast<const uint32_t*>(RFIFOP(fd, pos));
}

/// Done reading
void RFIFOSKIP(int fd, size_t len);

/// Read from an arbitrary buffer
inline const uint8_t *RBUFP(uint8_t *p, size_t pos)
{
    return p + pos;
}
inline uint8_t RBUFB(uint8_t *p, size_t pos)
{
    return *reinterpret_cast<const uint8_t*>(RBUFP(p, pos));
}
inline uint16_t RBUFW(uint8_t *p, size_t pos)
{
    return *reinterpret_cast<const uint16_t*>(RBUFP(p, pos));
}
inline uint32_t RBUFL(uint8_t *p, size_t pos)
{
    return *reinterpret_cast<const uint32_t*>(RBUFP(p, pos));
}


/// Write to the queue
inline uint8_t *WFIFOP(int fd, size_t pos)
{
    return session[fd]->wdata + session[fd]->wdata_size + pos;
}
inline uint8_t& WFIFOB(int fd, size_t pos)
{
    return *reinterpret_cast<uint8_t*>(WFIFOP(fd, pos));
}
inline uint16_t& WFIFOW(int fd, size_t pos)
{
    return *reinterpret_cast<uint16_t*>(WFIFOP(fd, pos));
}
inline uint32_t& WFIFOL(int fd, size_t pos)
{
    return *reinterpret_cast<uint32_t*>(WFIFOP(fd, pos));
}
/// Finish writing
void WFIFOSET(int fd, size_t len);

/// Write to an arbitrary buffer
inline uint8_t *WBUFP(uint8_t *p, size_t pos)
{
    return p + pos;
}
inline uint8_t& WBUFB(uint8_t *p, size_t pos)
{
    return *reinterpret_cast<uint8_t*>(WBUFP(p, pos));
}
inline uint16_t& WBUFW(uint8_t *p, size_t pos)
{
    return *reinterpret_cast<uint16_t*>(WBUFP(p, pos));
}
inline uint32_t& WBUFL(uint8_t *p, size_t pos)
{
    return *reinterpret_cast<uint32_t*>(WBUFP(p,pos));
}

#endif // SOCKET_HPP
