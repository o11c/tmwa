#include "core.hpp"

#include <sys/wait.h>

#include "mt_rand.hpp"
#include "socket.hpp"
#include "timer.hpp"

static void chld_proc(int32_t)
{
    wait(NULL);
}
static void sig_proc(int32_t)
{
    term_func();
    for (int32_t i = 0; i < fd_max; i++)
        if (session[i])
            close(i);
    _exit(0);
}

// Added by Gabuzomeu
//
// This is an implementation of signal() using sigaction() for portability.
// (sigaction() is POSIX; signal() is not.)  Taken from Stevens' _Advanced
// Programming in the UNIX Environment_.
//
typedef void (*sigfunc)(int32_t);
static sigfunc compat_signal(int32_t signo, sigfunc func)
{
    struct sigaction sact, oact;

    sact.sa_handler = func;
    sigemptyset(&sact.sa_mask);
    sact.sa_flags = 0;

#pragma GCC diagnostic ignored "-Wold-style-cast"
    if (sigaction(signo, &sact, &oact) < 0)
        return SIG_ERR;
#pragma GCC diagnostic pop
    return oact.sa_handler;
}

bool runflag = true;

int32_t main(int32_t argc, char **argv)
{
    do_socket();

    atexit(term_func);
#pragma GCC diagnostic ignored "-Wold-style-cast"
    compat_signal(SIGPIPE, SIG_IGN);
    compat_signal(SIGTERM, sig_proc);
    compat_signal(SIGINT, sig_proc);
    compat_signal(SIGCHLD, chld_proc);

    // Signal to create coredumps by system when necessary (crash)
    compat_signal(SIGSEGV, SIG_DFL);
    compat_signal(SIGBUS, SIG_DFL);
    compat_signal(SIGTRAP, SIG_DFL);
    compat_signal(SIGILL, SIG_DFL);
#pragma GCC diagnostic pop

    update_current_tick();
    do_init(argc, argv);
    while (runflag)
    {
        interval_t until_next_timer = do_timer();
        do_sendrecv(until_next_timer);
        do_parsepacket();
    }
}
