#ifndef TIMER_STRUCTS
#define TIMER_STRUCTS

# include "../lib/ints.hpp"

# include <functional>
# include <chrono>

// std::chrono::monotonic_clock isn't always monotonic
// and, in any case, it got renamed to steady_clock between GCC 4.6 and 4.7
class monotonic_clock
{
public:
    // there is still a LOT of code that assumes this is milliseconds
    // that said - maybe we should move it to nanoseconds
    // the server would *still* be valid for 584 years ...
    typedef std::chrono::milliseconds duration;
    typedef duration::rep rep;
    typedef duration::period period;
    typedef std::chrono::time_point<monotonic_clock, duration> time_point;

    static constexpr bool is_monotonic = true;
    static constexpr bool is_steady = true;

    static time_point now();

    static time_t to_time_t(const time_point& t)
    {
        return time_t(std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count());
    }

    static time_point from_time_t(time_t t)
    {
        return std::chrono::time_point_cast<duration>(std::chrono::time_point<monotonic_clock, std::chrono::seconds>(std::chrono::seconds(t)));
    }

};

typedef monotonic_clock::duration interval_t;
typedef monotonic_clock::time_point tick_t;
typedef struct TimerData *timer_id;

typedef std::function<void (timer_id, tick_t)> TimerFunc;
struct TimerData
{
    /// When it will be triggered
    tick_t tick;
    /// What will be done
    TimerFunc func;
    /// Repeat rate
    interval_t interval;
};

#endif //TIMER_STRUCTS
