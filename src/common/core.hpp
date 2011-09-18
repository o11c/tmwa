#ifndef CORE_HPP
#define CORE_HPP

/// core.c contains a server-independent main() function
/// and then runs a do_sendrecv loop

/// When this is cleared, the server exits gracefully
/// only used by map server's GM command: @mapexit
extern bool runflag;

/// This is an external function defined by each server
/// This function must register stuff for the parse loop
extern void do_init(int32_t, char **);

/// Cleanup function called whenever a signal kills us
/// also called when exit() is called
extern void term_func(void);

#endif // CORE_HPP
