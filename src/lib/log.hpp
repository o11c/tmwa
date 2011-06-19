#ifndef LOG_HPP
#define LOG_HPP

#include <string>

#include <cstdarg>

enum class Level
{
    // Useful for debugging only
    DEBUG,
    // Directly derived from the config files
    CONF,
    // General information
    INFO,
    // Something you should fix
    WARNING,
    // Something you must fix
    ERROR,
    // Something you must fix, that also causes the server to abort
    FATAL
};

inline bool operator < (Level lhs, Level rhs)
{
    return static_cast<int>(lhs) < static_cast<int>(rhs);
}
inline bool operator <= (Level lhs, Level rhs)
{
    return static_cast<int>(lhs) <= static_cast<int>(rhs);
}
inline bool operator > (Level lhs, Level rhs)
{
    return static_cast<int>(lhs) > static_cast<int>(rhs);
}
inline bool operator >= (Level lhs, Level rhs)
{
    return static_cast<int>(lhs) >= static_cast<int>(rhs);
}

// TODO add (optional) automatic log rotation
class Log
{
    // hierarchial name
    const std::string hname;
public:
    /// log a message to this logger and all of its parents
    void log(Level level, const char *format, va_list ap) __attribute__((format(printf, 3, 0)));
    void debug(const char *format, ...) __attribute__((format(printf, 2, 3)));
    void conf(const char *format, ...) __attribute__((format(printf, 2, 3)));
    void info(const char *format, ...) __attribute__((format(printf, 2, 3)));
    void warn(const char *format, ...) __attribute__((format(printf, 2, 3)));
    void error(const char *format, ...) __attribute__((format(printf, 2, 3)));
    void fatal(const char *format, ...) __attribute__((format(printf, 2, 3)));
    /// adds a destination for messages sent to this logger or any of its children
    /// the special filenames "stdout" and "stderr" can be used
    void add(const std::string& filename, bool timestamp,
             Level min, Level max = Level::FATAL);
    /// cause an existing log file to be closed and a new one opened
    /// new_filename may be the same as old_filename:
    /// this is useful if the file has been moved
    static void replace(const std::string& old_filename, const std::string& new_filename);
    Log(const std::string& name) : hname(name) {}
};

inline void Log::debug(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    log(Level::DEBUG, format, ap);
    va_end(ap);
}

inline void Log::conf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    log(Level::CONF, format, ap);
    va_end(ap);
}

inline void Log::info(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    log(Level::INFO, format, ap);
    va_end(ap);
}

inline void Log::warn(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    log(Level::WARNING, format, ap);
    va_end(ap);
}

inline void Log::error(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    log(Level::ERROR, format, ap);
    va_end(ap);
}

inline void Log::fatal(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    log(Level::FATAL, format, ap);
    va_end(ap);
}


/// a logger that is the parent of all other loggers, i.e. it receives all messages
extern Log root_log;

void init_log();
#endif //LOG_HPP