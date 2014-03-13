#include "../common/config_parse.hpp"
#include "../common/core.hpp"
#include "../common/socket.hpp"
#include "../common/version.hpp"

#include "../io/cxxstdio.hpp"

#include "irc.hpp"

#include "../poison.hpp"

void SessionDeleter::operator()(SessionData *sd)
{
    really_delete1 static_cast<IrcSessionData *>(sd);
}

RString nick, pass, host;
uint16_t port;
std::vector<RString> channels;

static
bool irc_confs(XString key, ZString value)
{
    if (key == "pass")
    {
        pass = value;
        return true;
    }
    if (key == "nick")
    {
        nick = value;
        return true;
    }
    if (key == "host")
    {
        host = value;
        return true;
    }
    if (key == "port")
    {
        return extract(value, &port);
    }
    if (key == "join")
    {
        channels.push_back(value);
        return true;
    }

    return false;
}

static
void irc_parse(Session *s)
{
    static_cast<IrcSessionData *>(s->session_data.get())->parse();
}

int do_init(int argc, ZString *argv)
{
    bool loaded_config_yet = false;
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i].startswith('-'))
        {
            if (argv[i] == "--help")
            {
                PRINTF("Usage: %s [--help] [--version] [files...]\n",
                        argv[0]);
                exit(0);
            }
            else if (argv[i] == "--version")
            {
                PRINTF("%s\n", CURRENT_VERSION_STRING);
                exit(0);
            }
            else
            {
                FPRINTF(stderr, "Unknown argument: %s\n", argv[i]);
                runflag = false;
            }
        }
        else
        {
            loaded_config_yet = true;
            runflag &= load_config_file(argv[i], irc_confs);
        }
    }

    if (!loaded_config_yet)
    {
        PRINTF("Please specify config file explicitly");
        exit(0);
    }

    set_defaultparse(irc_parse);
    for (IP4Address ip : gai4(host))
    {
        if (Session *s = make_connection(ip, port))
        {
            auto sd = make_unique<IrcSessionData, SessionDeleter>(s, nick, pass);
            for (RString ch : channels)
                sd->join(ch);
            s->session_data = std::move(sd);
            break;
        }
    }
    return 0;
}

void term_func()
{
    return;
}
