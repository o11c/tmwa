#ifndef SOCKET_H
#define SOCKET_H

# include "sanity.hpp"

# include <stdio.h>

# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>

# include <time.h>

// TODO replace these with inline functions since we're not superstitious
// maybe members of socket_data?
/// Check how much can be read
# define RFIFOREST(fd) (session[fd]->rdata_size-session[fd]->rdata_pos)
/// Read from the queue
# define RFIFOP(fd,pos) (static_cast<const uint8_t *>(session[fd]->rdata)+session[fd]->rdata_pos+(pos))
# define RFIFOB(fd,pos) (*reinterpret_cast<const uint8_t*>(RFIFOP(fd, pos)))
# define RFIFOW(fd,pos) (*reinterpret_cast<const uint16_t*>(RFIFOP(fd, pos)))
# define RFIFOL(fd,pos) (*reinterpret_cast<const uint32_t*>(RFIFOP(fd, pos)))

/// Done reading
void RFIFOSKIP(int fd, size_t len);
/// Internal - clean up by discarding handled bytes
// Atm this is also called in char/char.c, but that is unnecessary
# define RFIFOFLUSH(fd) (memmove(session[fd]->rdata,RFIFOP(fd,0),RFIFOREST(fd)),\
session[fd]->rdata_size=RFIFOREST(fd),\
session[fd]->rdata_pos=0)

/// Used internally - how much room there is to read more data
# define RFIFOSPACE(fd) (session[fd]->max_rdata-session[fd]->rdata_size)

/// Read from an arbitrary buffer
# define RBUFP(p,pos) (static_cast<const uint8_t*>(p)+(pos))
# define RBUFB(p,pos) (*reinterpret_cast<const uint8_t*>(RBUFP((p),(pos))))
# define RBUFW(p,pos) (*reinterpret_cast<const uint16_t*>(RBUFP((p),(pos))))
# define RBUFL(p,pos) (*reinterpret_cast<const uint32_t*>(RBUFP((p),(pos))))



/// Unused - check how much data can be written
# define WFIFOSPACE(fd) (session[fd]->max_wdata-session[fd]->wdata_size)
/// Write to the queue
# define WFIFOP(fd,pos) (session[fd]->wdata+session[fd]->wdata_size+(pos))
# define WFIFOB(fd,pos) (*reinterpret_cast<uint8_t*>(WFIFOP(fd,pos)))
# define WFIFOW(fd,pos) (*reinterpret_cast<uint16_t*>(WFIFOP(fd,pos)))
# define WFIFOL(fd,pos) (*reinterpret_cast<uint32_t*>(WFIFOP(fd,pos)))
/// Finish writing
void WFIFOSET(int fd, size_t len);

/// Write to an arbitrary buffer
#define WBUFP(p,pos) (static_cast<uint8_t*>(p)+(pos))
#define WBUFB(p,pos) (*reinterpret_cast<uint8_t*>(WBUFP((p),(pos))))
#define WBUFW(p,pos) (*reinterpret_cast<uint16_t*>(WBUFP((p),(pos))))
#define WBUFL(p,pos) (*reinterpret_cast<uint32_t*>(WBUFP((p),(pos))))

// Struct declaration

struct socket_data
{
    /// Checks whether a newly-connected socket actually does anything
    time_t created;
    bool connected;

    /// Flag needed since structure must be freed in a server-dependent manner
    bool eof;

    /// Since this is a single-threaded application, it can't block
    /// These are the read/write queues
    uint8_t *rdata, *wdata;
    size_t max_rdata, max_wdata;
    /// How much is actually in the queue
    size_t rdata_size, wdata_size;
    /// How much has already been read from the queue
    /// Note that there is no need for a wdata_pos
    size_t rdata_pos;

    struct sockaddr_in client_addr;

    /// Send or recieve
    /// Only called when select() indicates the socket is ready
    /// If, after that, nothing is read, it sets eof
    // These could probably be hard-coded with a little work
    void (*func_recv)(int);
    void (*func_send)(int);
    /// This is the important one
    /// Set to different functions depending on whether the connection
    /// is a player or a server/ladmin
    /// Can be set explicitly or via set_defaultparse
    void (*func_parse)(int);
    /// Server-specific data type
    // TODO make this into a type-safe-but-generic struct session_data *
    void *session_data;

    // used when forwarding a packet with different ID
    void rfifo_change_packet(uint16_t newpacket)
    {
        *reinterpret_cast<uint16_t *>(rdata + rdata_pos) = newpacket;
    }
};

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
// FIXME - this is IPv4 only!
int make_connection(uint32_t ip, uint16_t port);
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
bool free_fds(void);

#endif // SOCKET_H
