#ifndef TMWA_ELANORE_IRC_HPP
#define TMWA_ELANORE_IRC_HPP

#include <vector>

#include "../strings/zstring.hpp"

#include "../common/ip.hpp"
#include "../common/socket.hpp"

#include "../io/write.hpp"

/// Settings for an IRC connection (one network)
class IrcSettings
{
    enum privacy { privater };
public:
    IrcSettings(privacy) {}
    IrcSettings(const IrcSettings&) = delete;
public:
    static
    std::unique_ptr<IrcSettings> create() { return make_unique<IrcSettings>(privater); }

    RString host;
    uint16_t port = 6667;
    RString nick;
    RString pass;
    RString owner;
    std::vector<RString> channels;
    unsigned reconnect = 300;
    unsigned ping = 180;
    RString raw_log;
};

class Irc : public SessionData
{
    enum privacy { privater };
private:
    Session *sess;
    std::unique_ptr<IrcSettings> config;

    io::AppendFile raw_log;
public:
    explicit
    Irc(privacy, Session *, std::unique_ptr<IrcSettings>);
public:
    static
    void create(std::unique_ptr<IrcSettings> settings);
    ~Irc();
protected:
    void raw(ZString r);
public:
    void nick(ZString n);
    void pass(ZString p);
    void user(ZString ident, ZString realname);
    void join(ZString channel);
    void pong(ZString tag);
    void privmsg(XString to, ZString tag);
public:
    void parse();
};

std::vector<IP4Address> gai4(ZString h);

#endif //TMWA_ELANORE_IRC_HPP
