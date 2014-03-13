#ifndef TMWA_ELANORE_IRC_HPP
#define TMWA_ELANORE_IRC_HPP

#include <vector>

#include "../strings/zstring.hpp"

#include "../common/ip.hpp"
#include "../common/socket.hpp"

class IrcSessionData : public SessionData
{
    Session *sess;
public:
    IrcSessionData(Session *s, ZString nick, ZString pass);
protected:
    void raw(ZString r);
public:
    void nick(ZString n);
    void pass(ZString p);
    void user(ZString ident, ZString realname);
    void join(ZString channel);
public:
    void parse();
};

std::vector<IP4Address> gai4(ZString h);

#endif //TMWA_ELANORE_IRC_HPP
