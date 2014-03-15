#include "../common/config_parse.hpp"
#include "../common/core.hpp"
#include "../common/socket.hpp"
#include "../common/version.hpp"

#include "../io/cxxstdio.hpp"

#include "irc.hpp"

#include "../poison.hpp"

void SessionDeleter::operator()(SessionData *sd)
{
    really_delete1 static_cast<Irc *>(sd);
}

std::unique_ptr<IrcSettings> config = IrcSettings::create();

static
bool irc_confs(XString key, ZString value)
{
    if (key == "host")
    {
        config->host = value;
        return true;
    }
    if (key == "port")
    {
        return extract(value, &config->port);
    }
    if (key == "pass")
    {
        config->pass = value;
        return true;
    }
    if (key == "nick")
    {
        config->nick = value;
        return true;
    }
    if (key == "owner")
    {
        config->owner = value;
        return true;
    }
    if (key == "ping")
    {
        return extract(value, &config->ping);
    }
    if (key == "reconnect")
    {
        return extract(value, &config->reconnect);
    }
    if (key == "join")
    {
        config->channels.push_back(value);
        return true;
    }
    if (key == "raw_log")
    {
        config->raw_log = value;
        return true;
    }

    return false;
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

    Irc::create(std::move(config));
    return 0;
}

void term_func()
{
    return;
}
