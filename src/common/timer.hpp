#ifndef TIMER_H
#define TIMER_H

# include "sanity.hpp"

enum TIMER_TYPE
{
    TIMER_NONE,
    TIMER_ONCE_AUTODEL,
    TIMER_INTERVAL,
};
/// This is needed to produce a signed result when 2 ticks are subtracted
# define DIFF_TICK(a,b) ((int32_t)((a)-(b)))

// TODO replace with signed 64-bit to make code more clear and protect from the future
typedef uint32_t tick_t;
typedef uint32_t interval_t;
typedef uint32_t timer_id;
// BUG: pointers are stored in here
typedef int32_t custom_id_t;
typedef int32_t custom_data_t;
typedef void(*timer_func)(timer_id, tick_t, custom_id_t, custom_data_t);

struct TimerData
{
    /// When it will be triggered
    tick_t tick;
    /// What will be done
    timer_func func;
    /// Arbitrary data. WARNING, callers are stupid and put pointers in here
    // Should we change to void* or intptr_t ?
    custom_id_t  id;
    custom_data_t  data;
    /// Type of timer - 0 initially
    enum TIMER_TYPE type;
    /// Repeat rate
    interval_t interval;
};

/// Server time, in milliseconds, since the epoch,
/// but use of 32-bit integers means it wraps every 49 days.
// The only external caller of this function is the core.c main loop, but that makes sense
// in fact, it might make more sense if gettick() ALWAYS returned that cached value
tick_t gettick_nocache(void);
/// This function is called enough that it's worth caching the result for
/// the next 255 times
tick_t gettick(void);

timer_id add_timer(tick_t, timer_func, custom_id_t, custom_data_t);
timer_id add_timer_interval(tick_t, timer_func, custom_id_t, custom_data_t, interval_t);
void delete_timer(timer_id, timer_func);

tick_t addtick_timer(timer_id, interval_t);
struct TimerData *get_timer(timer_id tid);

/// Do all timers scheduled before tick, and return the number of milliseconds until the next timer happens
interval_t do_timer(tick_t tick);



#endif // TIMER_H
