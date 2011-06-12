#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <sys/socket.h>

#include <time.h>

#include "timer.hpp"
#include "utils.hpp"

static struct TimerData *timer_data;
static uint32_t timer_data_max, timer_data_num;
static timer_id *free_timer_list;
static uint32_t free_timer_list_max, free_timer_list_pos;

/// Okay, I think I understand this structure now:
/// the timer heap is a magic queue that allows inserting timers and then popping them in order
/// designed to copy only log2(N) entries instead of N
// timer_heap[0] is the size (greatest index into the heap)
// timer_heap[1] is the first actual element
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


static void push_timer_heap(timer_id idx)
{
    if (timer_heap == NULL || timer_heap[0] + 1 >= timer_heap_max)
    {
        timer_heap_max += 256;
        RECREATE(timer_heap, timer_id, timer_heap_max);
        memset(timer_heap + (timer_heap_max - 256), 0, sizeof(timer_id) * 256);
    }
// timer_heap[0] is the greatest index into the heap, which increases
    timer_heap[0]++;

    timer_id h = timer_heap[0]-1, i = (h - 1) / 2;
    while (h)
    {
        // avoid wraparound problems, it really means this:
        //   timer_data[idx].tick >= timer_data[timer_heap[i+1]].tick
        if ( DIFF_TICK(timer_data[idx].tick, timer_data[timer_heap[i+1]].tick) >= 0)
            break;
        timer_heap[h + 1] = timer_heap[i + 1];
        h = i;
        i = (h - 1) / 2;
    }
    timer_heap[h + 1] = idx;
}

static timer_id top_timer_heap(void)
{
    if (!timer_heap || !timer_heap[0])
        return -1;
    return timer_heap[1];
}

static timer_id pop_timer_heap(void)
{
    if (!timer_heap || !timer_heap[0])
        return -1;
    timer_id ret = timer_heap[1];
    timer_id last = timer_heap[timer_heap[0]];
    timer_heap[0]--;

    uint32_t h, k;
    for (h = 0, k = 2; k < timer_heap[0]; k = k * 2 + 2)
    {
        if (DIFF_TICK(timer_data[timer_heap[k + 1]].tick, timer_data[timer_heap[k]].tick) > 0)
            k--;
        timer_heap[h + 1] = timer_heap[k + 1], h = k;
    }
    if (k == timer_heap[0])
        timer_heap[h + 1] = timer_heap[k], h = k - 1;

    uint32_t i = (h - 1) / 2;
    while (h)
    {
        if (DIFF_TICK(timer_data[timer_heap[i + 1]].tick, timer_data[last].tick) <= 0)
            break;
        timer_heap[h + 1] = timer_heap[i + 1];
        h = i;
        i = (h - 1) / 2;
    }
    timer_heap[h + 1] = last;

    return ret;
}

timer_id add_timer_impl(tick_t tick, TimerFunc func, interval_t interval)
{
    timer_id i;

    if (free_timer_list_pos)
    {
        // Retrieve a freed timer id instead of a new one
        // I think it should be possible to avoid the loop somehow
        do
        {
            i = free_timer_list[--free_timer_list_pos];
        }
        while (i >= timer_data_num && free_timer_list_pos > 0);
    }
    else
        i = timer_data_num;

    // I have no idea what this is doing
    if (i >= timer_data_num)
        for (i = timer_data_num; i < timer_data_max && timer_data[i].func; i++)
            ;
    if (i >= timer_data_num && i >= timer_data_max)
    {
        if (timer_data_max == 0)
        {
            timer_data_max = 256;
            CREATE(timer_data, struct TimerData, timer_data_max);
        }
        else
        {
            timer_data_max += 256;
            RECREATE(timer_data, struct TimerData, timer_data_max);
            memset(timer_data + (timer_data_max - 256), 0,
                    sizeof(struct TimerData) * 256);
        }
    }
    timer_data[i].tick = tick;
    timer_data[i].func = func;
    timer_data[i].interval = interval;
    push_timer_heap(i);
    if (i >= timer_data_num)
        timer_data_num = i + 1;
    return i;
}

void delete_timer(timer_id id)
{
    if (id == 0 || id >= timer_data_num)
    {
        fprintf(stderr, "delete_timer error : no such timer %d\n", id);
        abort();
    }
    // "to let them disappear" - is this just in case?
    // odd, this *doesn't* actually disabled the timer
    // I guess it makes sense, since it's a part of the data structure ...
    timer_data[id].func = NULL;
    timer_data[id].interval = 0;
    timer_data[id].tick = gettick();
}

struct TimerData *get_timer(timer_id tid)
{
    return &timer_data[tid];
}

interval_t do_timer()
{
    update_current_tick();
    tick_t tick = current_tick;
    timer_id i;
    /// Number of milliseconds until it calls this again
    // this says to wait 1 sec if all timers get popped
    interval_t nextmin = 1000;

    while ((i = top_timer_heap()) != -1)
    {
        // while the heap is not empty and
        if (DIFF_TICK(timer_data[i].tick, tick) > 0)
        {
            /// Return the time until the next timer needs to goes off
            nextmin = DIFF_TICK(timer_data[i].tick, tick);
            break;
        }
        pop_timer_heap();
        if (!timer_data[i].func)
            continue;
        if (DIFF_TICK(timer_data[i].tick, tick) < -1000)
        {
            // If we are too far past the requested tick, call with the current tick instead to fix reregistering problems
            timer_data[i].func(i, tick);
        }
        else
        {
            timer_data[i].func(i, timer_data[i].tick);
        }

        if (!timer_data[i].interval)
        {
            timer_data[i].func = NULL;
            if (free_timer_list_pos >= free_timer_list_max)
            {
                free_timer_list_max += 256;
                RECREATE(free_timer_list, uint32_t, free_timer_list_max);
                memset(free_timer_list + (free_timer_list_max - 256),
                        0, 256 * sizeof(uint32_t));
            }
            free_timer_list[free_timer_list_pos++] = i;
        }
        else
        {
            if (DIFF_TICK(timer_data[i].tick, tick) < -1000)
            {
                timer_data[i].tick = tick + timer_data[i].interval;
            }
            else
            {
                timer_data[i].tick += timer_data[i].interval;
            }
            push_timer_heap(i);
        }
    }

    if (nextmin < 10)
        nextmin = 10;
    return nextmin;
}
