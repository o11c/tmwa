#ifndef TIMER_H
#define TIMER_H

# include "sanity.hpp"

# include <functional>

// TODO replace with signed 64-bit to make code more clear and protect from the future
typedef uint32_t tick_t;
typedef uint32_t interval_t;
typedef uint32_t timer_id;

/// This is needed to produce a signed result when 2 ticks are subtracted
inline int32_t DIFF_TICK(tick_t a, tick_t b)
{
    return static_cast<int32_t>(a-b);
}

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


/// Server time, in milliseconds, since the epoch,
/// but use of 32-bit integers means it wraps every 49 days.
extern tick_t current_tick;
void update_current_tick();

inline tick_t gettick_nocache()
{
    update_current_tick();
    return current_tick;
}
inline tick_t gettick(void)
{
    return current_tick;
}

timer_id add_timer_impl(tick_t, TimerFunc func, interval_t);
template<class... Args>
timer_id add_timer_interval(tick_t tick, interval_t interval, void(&func)(timer_id, tick_t, Args...), Args... args)
{
    return add_timer_impl(tick,
                          std::bind(func,
                                    std::placeholders::_1,
                                    std::placeholders::_2,
                                    args...),
                          interval);
}
template<class... Args>
timer_id add_timer(tick_t tick, void(&func)(timer_id, tick_t, Args...), Args... args)
{
    return add_timer_interval(tick, 0, func, args...);
}

void delete_timer(timer_id);

struct TimerData *get_timer(timer_id tid);

/// Do all timers scheduled before tick, and return the number of milliseconds until the next timer happens
interval_t do_timer(tick_t tick);



#endif // TIMER_H
