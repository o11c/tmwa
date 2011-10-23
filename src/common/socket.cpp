#include "socket.hpp"

#include <netinet/tcp.h>
#include <fcntl.h>

#include "utils.hpp"

// It's too ugly to surround EVERY call site
#pragma GCC diagnostic ignored "-Wold-style-cast"

/// Internal - clean up by discarding handled bytes
inline void RFIFOFLUSH(sint32 fd)
{
    memmove(session[fd]->rdata, RFIFOP(fd, 0), RFIFOREST(fd));
    session[fd]->rdata_size = RFIFOREST(fd);
    session[fd]->rdata_pos = 0;
}
/// Used internally - how much room there is to read more data
inline size_t RFIFOSPACE(sint32 fd)
{
    return session[fd]->max_rdata - session[fd]->rdata_size;
}


fd_set readfds;
sint32 fd_max;
sint32 currentuse;

const uint32 RFIFO_SIZE = 65536;
const uint32 WFIFO_SIZE = 65536;

struct socket_data *session[FD_SETSIZE];

/// Discard all input
static void null_parse(sint32 fd);
/// Default parser for new connections
static void (*default_func_parse)(sint32) = null_parse;

void set_defaultparse(void (*defaultparse)(sint32))
{
    default_func_parse = defaultparse;
}

/// Read from socket to the queue
static void recv_to_fifo(sint32 fd)
{
    if (session[fd]->eof)
        return;

    ssize_t len = read(fd, session[fd]->rdata + session[fd]->rdata_size,
                        RFIFOSPACE(fd));

    if (len > 0)
    {
        session[fd]->rdata_size += len;
        session[fd]->connected = 1;
    }
    else
    {
        session[fd]->eof = 1;
    }
}

static void send_from_fifo(sint32 fd)
{
    if (session[fd]->eof)
        return;

    ssize_t len = write(fd, session[fd]->wdata, session[fd]->wdata_size);

    if (len > 0)
    {
        session[fd]->wdata_size -= len;
        if (len < static_cast<ssize_t>(session[fd]->wdata_size))
        {
            memmove(session[fd]->wdata, session[fd]->wdata + len,
                     session[fd]->wdata_size);
        }
        session[fd]->connected = 1;
    }
    else
    {
        session[fd]->eof = 1;
    }
}

static void null_parse(sint32 fd)
{
    printf("null_parse : %d\n", fd);
    RFIFOSKIP(fd, RFIFOREST(fd));
}

static void connect_client(sint32 listen_fd)
{
    struct sockaddr_in client_address;
    socklen_t len = sizeof(client_address);

    sint32 fd = accept(listen_fd, reinterpret_cast<struct sockaddr *>(&client_address), &len);
    if (fd == -1)
    {
        perror("accept");
        return;
    }
    if (fd_max <= fd)
    {
        fd_max = fd + 1;
    }
    if (!free_fds())
    {
        fprintf(stderr, "softlimit reached, disconnecting : %d\n", fd);
        delete_session(fd);
        return;
    }

    const sint32 yes = 1;
    /// Allow to bind() again after the server restarts.
    // Since the socket is still in the TIME_WAIT, there's a possibility
    // that formerly lost packets might be delivered and confuse the server.
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    /// Send packets as soon as possible
    /// even if the kernel thinks there is too little for it to be worth it!
    // I'm not convinced this is a good idea; although in minimizes the
    // latency for an individual write, it increases traffic in general.
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);

    FD_SET(fd, &readfds);

    fcntl(fd, F_SETFL, O_NONBLOCK);

    CREATE(session[fd], struct socket_data, 1);
    CREATE(session[fd]->rdata, uint8, RFIFO_SIZE);
    CREATE(session[fd]->wdata, uint8, WFIFO_SIZE);

    session[fd]->max_rdata = RFIFO_SIZE;
    session[fd]->max_wdata = WFIFO_SIZE;
    session[fd]->func_recv = recv_to_fifo;
    session[fd]->func_send = send_from_fifo;
    session[fd]->func_parse = default_func_parse;
    session[fd]->client_addr = IP_Address(&client_address);
    session[fd]->created = time(NULL);
    session[fd]->connected = 0;

    currentuse++;
}

sint32 make_listen_port(uint16 port)
{
    struct sockaddr_in server_address;
    sint32 fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("socket");
        return -1;
    }
    if (fd_max <= fd)
        fd_max = fd + 1;

    fcntl(fd, F_SETFL, O_NONBLOCK);

    const sint32 yes = 1;
    /// Allow to bind() again after the server restarts.
    // Since the socket is still in the TIME_WAIT, there's a possibility
    // that formerly lost packets might be delivered and confuse the server.
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    /// Send packets as soon as possible
    /// even if the kernel thinks there is too little for it to be worth it!
    // I'm not convinced this is a good idea; although in minimizes the
    // latency for an individual write, it increases traffic in general.
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    if (bind(fd, reinterpret_cast<struct sockaddr *>(&server_address),
             sizeof(server_address)) == -1)
    {
        perror("bind");
        exit(1);
    }
    if (listen(fd, 5) == -1)
    {                           /* error */
        perror("listen");
        exit(1);
    }

    FD_SET(fd, &readfds);

    CREATE(session[fd], struct socket_data, 1);

    session[fd]->func_recv = connect_client;
    session[fd]->created = time(NULL);
    session[fd]->connected = 1;

    currentuse++;
    return fd;
}

sint32 make_connection(IP_Address ip, uint16 port)
{
    struct sockaddr_in server_address;
    sint32 fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("socket");
        return -1;
    }
    if (fd_max <= fd)
        fd_max = fd + 1;

    const sint32 yes = 1;
    /// Allow to bind() again after the server restarts.
    // Since the socket is still in the TIME_WAIT, there's a possibility
    // that formerly lost packets might be delivered and confuse the server.
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    /// Send packets as soon as possible
    /// even if the kernel thinks there is too little for it to be worth it!
    // I'm not convinced this is a good idea; although in minimizes the
    // latency for an individual write, it increases traffic in general.
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = ip.to_n();
    server_address.sin_port = htons(port);

    fcntl(fd, F_SETFL, O_NONBLOCK);

    /// Errors not caught - we must not block
    /// Let the main select() loop detect when we know the state
    connect(fd, reinterpret_cast<struct sockaddr *>(&server_address),
            sizeof(struct sockaddr_in));

    FD_SET(fd, &readfds);

    CREATE(session[fd], struct socket_data, 1);
    CREATE(session[fd]->rdata, uint8, RFIFO_SIZE);
    CREATE(session[fd]->wdata, uint8, WFIFO_SIZE);

    session[fd]->max_rdata = RFIFO_SIZE;
    session[fd]->max_wdata = WFIFO_SIZE;
    session[fd]->func_recv = recv_to_fifo;
    session[fd]->func_send = send_from_fifo;
    session[fd]->func_parse = default_func_parse;
    session[fd]->created = time(NULL);
    session[fd]->connected = 1;

    currentuse++;
    return fd;
}

void delete_session(sint32 fd)
{
    if (fd < 0 || fd >= FD_SETSIZE)
        return;
    // If this was the highest fd, decrease it
    // We could add a loop to decrement fd_max further for every null session,
    // but this is cheap and good enough for the typical case
    if (fd == fd_max - 1)
        fd_max--;
    FD_CLR(fd, &readfds);
    if (session[fd])
    {
        free(session[fd]->rdata);
        free(session[fd]->wdata);
        delete session[fd]->session_data;
        free(session[fd]);
    }
    session[fd] = NULL;

    // just close() would try to keep sending buffers
    shutdown(fd, SHUT_RDWR);
    close(fd);
    currentuse--;
    if (currentuse < 0)
    {
        fprintf(stderr, "delete_session: current sessions negative!\n");
        currentuse = 0;
    }
    return;
}

void realloc_fifo(sint32 fd, size_t rfifo_size, size_t wfifo_size)
{
    struct socket_data *s = session[fd];
    if (s->max_rdata != rfifo_size && s->rdata_size < rfifo_size)
    {
        RECREATE(s->rdata, uint8, rfifo_size);
        s->max_rdata = rfifo_size;
    }
    if (s->max_wdata != wfifo_size && s->wdata_size < wfifo_size)
    {
        RECREATE(s->wdata, uint8, wfifo_size);
        s->max_wdata = wfifo_size;
    }
}

void WFIFOSET(sint32 fd, size_t len)
{
    struct socket_data *s = session[fd];
    if (s->wdata_size + len + 16384 > s->max_wdata)
    {
        realloc_fifo(fd, s->max_rdata, s->max_wdata << 1);
        printf("socket: %d wdata expanded to %d bytes.\n", fd, s->max_wdata);
    }
    if (s->wdata_size + len + 2048 < s->max_wdata)
        s->wdata_size += len;
    else
        fprintf(stderr, "socket: %d wdata lost !!\n", fd), abort();
}

void do_sendrecv(interval_t next)
{
    fd_set rfd = readfds, wfd;
    FD_ZERO(&wfd);
    for (sint32 i = 0; i < fd_max; i++)
    {
        if (session[i] && session[i]->wdata_size)
            FD_SET(i, &wfd);
    }
    struct timeval timeout;
    timeout.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(next).count();
    timeout.tv_usec = std::chrono::duration_cast<std::chrono::nanoseconds>(next).count() % (1000 * 1000 * 1000);
    if (select(fd_max, &rfd, &wfd, NULL, &timeout) <= 0)
        return;
    for (sint32 i = 0; i < fd_max; i++)
    {
        if (!session[i])
            continue;
        if (FD_ISSET(i, &wfd))
        {
            if (session[i]->func_send)
                //send_from_fifo(i);
                session[i]->func_send(i);
        }
        if (FD_ISSET(i, &rfd))
        {
            if (session[i]->func_recv)
                //recv_to_fifo(i);
                //or connect_client(i);
                session[i]->func_recv(i);
        }
    }
}

void do_parsepacket(void)
{
    for (sint32 i = 0; i < fd_max; i++)
    {
        if (!session[i])
            continue;
        if (!session[i]->connected
            && time(NULL) - session[i]->created > CONNECT_TIMEOUT)
        {
            printf("Session #%d timed out\n", i);
            session[i]->eof = 1;
        }
        if (!session[i]->rdata_size && !session[i]->eof)
            continue;
        if (session[i]->func_parse)
        {
            session[i]->func_parse(i);
            /// some func_parse may call delete_session
            if (!session[i])
                continue;
        }
        /// Reclaim buffer space for what was read
        RFIFOFLUSH(i);
    }
}

void do_socket(void)
{
    FD_ZERO(&readfds);
    currentuse = 3;
}

void RFIFOSKIP(sint32 fd, size_t len)
{
    struct socket_data *s = session[fd];
    s->rdata_pos += len;

    if (s->rdata_size < s->rdata_pos)
    {
        fprintf(stderr, "too many skip\n");
        abort();
    }
}

void fclose_(FILE * fp)
{
    if (fclose(fp))
        perror("fclose"), abort();
    currentuse--;
}

FILE *fopen_(const char *path, const char *mode)
{
    FILE *f = fopen(path, mode);
    if (f)
        currentuse++;
    return f;
}

bool free_fds(void)
{
    return currentuse < SOFT_LIMIT;
}
