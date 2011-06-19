#include "log.hpp"

#include <sys/time.h>

#include <cstdio>

#include <map>
#include <vector>

struct LogSink
{
    FILE *fp;
    bool timestamp;
    Level minlevel, maxlevel;
};

static std::map<std::string, FILE *> filenames =
{
    {"stdout", stdout},
    {"stderr", stderr}
};

static FILE *get_file(const std::string& filename)
{
    FILE *& out = filenames[filename.c_str()];
    if (!out)
        out = fopen(filename.c_str(), "a");
    return out;
}

std::multimap<std::string, LogSink> sink_map;

static void add_sink(const std::string& name, const LogSink& sink)
{
    sink_map.insert(std::make_pair(name, sink));
}

void Log::add(const std::string& filename, bool timestamp, Level min, Level max)
{
    LogSink sink = { get_file(filename), timestamp, min, max };
    if (sink.fp)
        add_sink(hname, sink);
    else
        root_log.error("Unable to open log file: %s", filename.c_str());
}

void Log::replace(const std::string& old_filename, const std::string& new_filename)
{
    for (std::map<std::string, FILE *>::iterator it = filenames.begin(); it != filenames.end(); ++it)
    {
        if (it->first == old_filename)
        {
            FILE *file = it->second;
            freopen(new_filename.c_str(), "a", file);
            if (old_filename != new_filename)
            {
                // granted, we probably shouldn't keep using the map, but
                // should we keep the old name in there?
                filenames.erase(it);
                filenames[new_filename] = file;
            }
            break;
        }
    }
}

void Log::log(Level level, const char *format, va_list ap)
{
    size_t length;
    {
        va_list copy;
        va_copy(copy, ap);
        length = vsnprintf(NULL, 0, format, copy);
        va_end(copy);
    }
    char buf[length + 2];
    vsnprintf(buf, length + 1, format, ap);
    if (buf[length - 1] != '\n')
        buf[length] = '\n', buf[length + 1] = '\0';

    struct timeval tv;
    gettimeofday(&tv, NULL);
    char time_str[20 + 4];
    strftime(time_str, 20, "%Y-%m-%d %H:%M:%S", gmtime(&tv.tv_sec));
    sprintf(time_str + 19, ".%03u", static_cast<unsigned int>(tv.tv_usec / 1000));

    std::string name = hname;
    while (true)
    {
        auto range = sink_map.equal_range(name);
        for (auto it = range.first; it != range.second; ++it)
        {
            auto& out = it->second;
            if (out.fp && out.minlevel <= level && level <= out.maxlevel)
            {
                if (out.timestamp)
                    fprintf(out.fp, "%s: ", time_str);
                fputs(buf, out.fp);
            }
        }
        if (name.empty())
            break;
        name.resize(name.size() - 1);
    }

    if (level == Level::FATAL)
        abort();
}



Log root_log = Log(std::string());

void init_log()
{
    root_log.add("stdout", false, Level::CONF, Level::CONF);
    root_log.add("stderr", false, Level::WARNING);
}