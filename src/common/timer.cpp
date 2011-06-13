#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <sys/socket.h>

#include <time.h>

#include "timer.hpp"
#include "utils.hpp"

/// Okay, I think I understand this structure now:
/// the timer heap is a magic queue that allows inserting timers and then popping them in order
/// designed to copy only log2(N) entries instead of N
// timer_heap_count is the size (greatest index into the heap)
// timer_heap[0] is the first actual element
// timer_heap_max increases 256 at a time and never decreases
static uint32_t timer_heap_max = 0;
/// FIXME: refactor the code to put the size in a separate variable
//nontrivial because indices get multiplied
static timer_id *timer_heap = NULL;


tick_t current_tick;

void update_current_tick(void)
{
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    current_tick = static_cast<tick_t>(spec.tv_sec) * 1000 + spec.tv_nsec / 1000000;
}

static uint32_t timer_heap_count;

static void push_timer_heap(timer_id data)
{
    if (timer_heap == NULL || timer_heap_count >= timer_heap_max)
    {
        timer_heap_max += 256;
        RECREATE(timer_heap, timer_id, timer_heap_max);
    }
    timer_heap_count++;

    uint32_t h = timer_heap_count - 1;
    while (h)
    {
        uint32_t i = (h - 1) / 2;
        if (DIFF_TICK(data->tick, timer_heap[i]->tick) >= 0)
            break;
        timer_heap[h] = timer_heap[i];
        h = i;
    }
    timer_heap[h] = data;
}

static timer_id top_timer_heap(void)
{
    if (!timer_heap || !timer_heap_count)
        return NULL;
    return timer_heap[0];
}

static void pop_timer_heap(void)
{
    if (!timer_heap || !timer_heap_count)
        return;
    timer_id last = timer_heap[timer_heap_count - 1];
    timer_heap_count--;

    uint32_t h, k;
    for (h = 0, k = 2; k < timer_heap_count; k = k * 2 + 2)
    {
        if (DIFF_TICK(timer_heap[k]->tick, timer_heap[k - 1]->tick) > 0)
            k--;
        timer_heap[h] = timer_heap[k];
        h = k;
    }
    if (k == timer_heap_count)
    {
        timer_heap[h] = timer_heap[k - 1];
        h = k - 1;
    }
    while (h)
    {
        uint32_t i = (h - 1) / 2;
        if (DIFF_TICK(timer_heap[i]->tick, last->tick) <= 0)
            break;
        timer_heap[h] = timer_heap[i];
        h = i;
    }
    timer_heap[h] = last;
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
            continue;
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
            i->func = NULL;
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
