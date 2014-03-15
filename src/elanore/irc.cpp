#include "irc.hpp"

#include <netdb.h>

#include <set>

#include "../common/utils.hpp"

#include "../io/cxxstdio.hpp"


struct IrcMessage
{
    XString prefix, cmd, args[15];
    ZString last;
};


static
void irc_parse(Session *s)
{
    static_cast<Irc *>(s->session_data.get())->parse();
}

static
VString<19> timestamp()
{
    timestamp_seconds_buffer rv;
    stamp_time(rv);
    return rv;
}

Irc::Irc(privacy, Session *s, std::unique_ptr<IrcSettings> settings)
: sess(s), config(std::move(settings)), raw_log(config->raw_log, true)
{
    raw_log.put_line("");
}
Irc::~Irc()
{
    // TODO schedule reconnect here
    ;
}
void Irc::create(std::unique_ptr<IrcSettings> config)
{
    for (IP4Address ip : gai4(config->host))
    {
        if (Session *s = make_connection(ip, config->port))
        {
            s->func_parse = irc_parse;
            auto sd = make_unique<Irc, SessionDeleter>(privater, s, std::move(config));
            sd->pass(sd->config->pass);
            sd->nick(sd->config->nick);
            sd->user("elanore", STRPRINTF("%s's bot", sd->config->owner));
            for (RString ch : sd->config->channels)
                sd->join(ch);
            s->session_data = std::move(sd);
            break;
        }
    }
}

void Irc::pass(ZString p)
{
    raw(STRPRINTF("PASS %s", p));
}
void Irc::nick(ZString n)
{
    raw(STRPRINTF("NICK %s", n));
}
void Irc::user(ZString ident, ZString realname)
{
    raw(STRPRINTF("USER %s 8 * :%s", ident, realname));
}
void Irc::join(ZString channel)
{
    raw(STRPRINTF("JOIN %s", channel));
}
void Irc::pong(ZString target)
{
    raw(STRPRINTF("PONG :%s", target));
}
void Irc::privmsg(XString to, ZString msg)
{
    raw(STRPRINTF("PRIVMSG %s :%s", AString(to), msg));
}

void Irc::raw(ZString line)
{
    raw_log.really_put(timestamp().c_str(), 19);
    raw_log.really_put(" < ", 3);
    raw_log.put_line(line);
    sess->write_ptr(line.data(), line.size());
    sess->write_ptr("\r\n", 2);
}
void Irc::parse()
{
    AString line;
    while (sess->read_line(line))
    {
        raw_log.really_put(timestamp().c_str(), 19);
        raw_log.really_put(" > ", 3);
        raw_log.put_line(line);
        if (!line)
            continue;

        ZString rest = line;
        IrcMessage msg;
        // prefix
        if (rest.startswith(':'))
        {
            auto space = std::find(rest.begin(), rest.end(), ' ');
            msg.prefix = rest.xislice(rest.begin() + 1, space);
            rest = rest.xislice_t(space).lstrip();
        }
        // command
        {
            auto space = std::find(rest.begin(), rest.end(), ' ');
            msg.cmd = rest.xislice_h(space);
            rest = rest.xislice_t(space).lstrip();
        }
        // arguments
        for (XString& arg : msg.args)
        {
            if (rest)
                msg.last = rest;
            if (rest.startswith(':'))
            {
                arg = msg.last = rest.xslice_t(1);
                rest = "";
                break;
            }
            auto space = std::find(rest.begin(), rest.end(), ' ');
            arg = rest.xislice_h(space);
            rest = rest.xislice_t(space).lstrip();
            if (!rest)
                break;
        }
        if (rest)
        {
            PRINTF("Warning: too many arguments; command ignored.\n");
            PRINTF("line: %s\n", line);
            PRINTF("cmd: %s; remaining: %s\n", AString(msg.cmd), rest);
            continue;
        }

        // explicitly do nothing
        if (msg.cmd == "NOTICE")
            continue;
        else if (msg.cmd == "PING")
            pong(msg.last);
        else if (msg.cmd == "PRIVMSG")
        {
            auto sender = msg.prefix;
            auto bang = std::find(sender.begin(), sender.end(), '!');
            auto at = std::find(sender.begin(), sender.end(), '@');
            XString sender_nick = sender.xislice_h(bang);
            //XString sender_ident = sender.xislice(bang + 1, at);
            XString sender_host = sender.xislice_t(at + 1);
            auto recver = msg.args[0];
            auto body = msg.last; // msg.args[1]

            auto repliee = recver.startswith('#') ? recver : sender_nick;

            if (sender_host == "unaffiliated/o11c")
            {
                if (body.contains_seq("echo"))
                {
                    privmsg(repliee, body);
                }
            }
        }
    }
    if (RFIFOREST(sess) >= 512)
    {
        sess->eof = true;
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
