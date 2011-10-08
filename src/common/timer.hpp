#ifndef TIMER_HPP
#define TIMER_HPP

# include "timer.structs.hpp"

/// This is needed to produce a signed result when 2 ticks are subtracted
inline int32_t DIFF_TICK(tick_t a, tick_t b)
{
    return static_cast<int32_t>(a - b);
}

inline bool operator <(const TimerData& lhs, const TimerData& rhs)
{
    // Note: the sense is inverted since the lowest tick is the highest priority
    // TODO delete this operator and just use a manual comparator
    return DIFF_TICK(lhs.tick, rhs.tick) > 0;
}


/// Server time, in milliseconds, since the epoch,
/// but use of 32-bit integers means it wraps every 49 days.
extern tick_t current_tick;
void update_current_tick();

inline tick_t gettick(void)
{
    return current_tick;
}

timer_id add_timer_impl(tick_t, TimerFunc func, interval_t);

template<class... Args>
timer_id add_timer_interval(tick_t tick, interval_t interval, void (&func)(timer_id, tick_t, Args...), Args... args)
{
    return add_timer_impl(tick,
                          std::bind(func,
                                    std::placeholders::_1,
                                    std::placeholders::_2,
                                    args...),
                          interval);
}

template<class... Args>
timer_id add_timer(tick_t tick, void (&func)(timer_id, tick_t, Args...), Args... args)
{
    return add_timer_interval(tick, 0, func, args...);
}

void delete_timer(timer_id);

/// Update the current tick, then do all pending timers
/// Return how long until the next timer is due
interval_t do_timer();

#endif // TIMER_HPP
