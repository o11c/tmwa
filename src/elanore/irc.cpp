#include "irc.hpp"

#include <netdb.h>

#include "../io/cxxstdio.hpp"


IrcSessionData::IrcSessionData(Session *s, ZString n, ZString p)
: sess(s)
{
    pass(p);
    nick(n);
    user("elanore", "Elanore in C++");
}

void IrcSessionData::pass(ZString p)
{
    raw(STRPRINTF("PASS %s", p));
}
void IrcSessionData::nick(ZString n)
{
    raw(STRPRINTF("NICK %s", n));
}
void IrcSessionData::user(ZString ident, ZString realname)
{
    raw(STRPRINTF("USER %s 8 * :%s", ident, realname));
}
void IrcSessionData::join(ZString channel)
{
    raw(STRPRINTF("JOIN %s", channel));
}

void IrcSessionData::raw(ZString r)
{
    sess->write_ptr(r.data(), r.size());
    sess->write_ptr("\r\n", 2);
}
void IrcSessionData::parse()
{
    AString line;
    while (sess->read_line(line))
    {
        PRINTF("got line: %s\n", line);
    }
}

std::vector<IP4Address> gai4(ZString h)
{
    struct addrinfo hints {};
    struct addrinfo *res = nullptr;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0; //IPPROTO_TCP;
    hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;

    if (0 != getaddrinfo(h.c_str(), nullptr, &hints, &res))
        return {};

    std::vector<IP4Address> rv;
    for (struct addrinfo *it = res; it; it = it->ai_next)
        rv.push_back(IP4Address(reinterpret_cast<struct sockaddr_in *>(it->ai_addr)->sin_addr));

    freeaddrinfo(res);
    return rv;
}
