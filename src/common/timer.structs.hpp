#ifndef TIMER_STRUCTS
#define TIMER_STRUCTS

# include <functional>

// TODO replace with signed 64-bit to make code more clear and protect from the future
typedef uint32_t tick_t;
typedef uint32_t interval_t;
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
