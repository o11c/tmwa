#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <sys/socket.h>

#include <time.h>


#include <queue>

#include "timer.hpp"
#include "utils.hpp"


static std::priority_queue<timer_id, std::vector<timer_id>, PointeeLess<timer_id>> timers;


tick_t current_tick;

void update_current_tick(void)
{
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    current_tick = static_cast<tick_t>(spec.tv_sec) * 1000 + spec.tv_nsec / 1000000;
}

static void push_timer_heap(timer_id data)
{
    timers.push(data);
}

static timer_id top_timer_heap(void)
{
    if (timers.empty())
        return NULL;
    return timers.top();
}

static void pop_timer_heap(void)
{
    timers.pop();
}

timer_id add_timer_impl(tick_t tick, TimerFunc func, interval_t interval)
{
    TimerData *i = new TimerData;
    i->tick = tick;
    i->func = func;
    i->interval = interval;
    push_timer_heap(i);
    return i;
}

void delete_timer(timer_id id)
{
    // We can't actually delete them, we have to wait for it to be triggered
    id->func = NULL;
    id->interval = 0;
    // I hopw this doesn't mess up anything
    id->tick = gettick();
}

interval_t do_timer()
{
    update_current_tick();
    tick_t tick = current_tick;
    timer_id i;
    /// Number of milliseconds until it calls this again
    // this says to wait 1 sec if all timers get popped
    interval_t nextmin = 1000;

    while ((i = top_timer_heap()))
    {
        // while the heap is not empty and
        if (DIFF_TICK(i->tick, tick) > 0)
        {
            /// Return the time until the next timer needs to goes off
            nextmin = DIFF_TICK(i->tick, tick);
            break;
        }
        pop_timer_heap();
        if (!i->func)
        {
            delete i;
            continue;
        }
        if (DIFF_TICK(i->tick, tick) < -1000)
        {
            // If we are too far past the requested tick, call with the current tick instead to fix reregistering problems
            i->func(i, tick);
        }
        else
        {
            i->func(i, i->tick);
        }

        if (!i->interval)
        {
            delete i;
        }
        else
        {
            if (DIFF_TICK(i->tick, tick) < -1000)
            {
                i->tick = tick + i->interval;
            }
            else
            {
                i->tick += i->interval;
            }
            push_timer_heap(i);
        }
    }

    if (nextmin < 10)
        nextmin = 10;
    return nextmin;
}
