#include "timer.h"

/* In seconds */
static time_t start_time;

static bool HasSecondsTimerElapsed( double period, time_t *last_tick )
{
    assert( last_tick != NULL );
    bool hasElapsed = false;

    time_t current_time;
    time( &current_time );

    double delta = difftime( current_time, *last_tick );
    if( delta >= period )
    {
        hasElapsed = true; 
        *last_tick = current_time;
    }

    return hasElapsed;
}

extern uint32_t Timer_TimeSinceStartMS(void)
{
    time_t current_time;
    time( &current_time );

    double delta = difftime( current_time, start_time );
    
    /* Cast and convert to MS */
    uint32_t delta32 = (uint32_t)delta;
    delta32 *= 1000U;
    
    return delta32;
}

extern void Timer_Init(daemon_timer_t * const timer)
{
    printf("Initialising timers\n");
    time(&start_time);

    time(&timer->last_tick_s);
    timespec_get( &timer->last_tick_ms, TIME_UTC );
}

extern bool Timer_Tick500ms(daemon_timer_t * const timer)
{
    bool timerElapsed = false;
    
    struct timespec current_tick;

    timespec_get( &current_tick, TIME_UTC );
    if( (unsigned long)( current_tick.tv_nsec - timer->last_tick_ms.tv_nsec ) >= 500000000UL )
    {
        timer->last_tick_ms = current_tick;
        timerElapsed = true;
    }

    return timerElapsed;
}

extern bool Timer_Tick1s(daemon_timer_t * const timer)
{
    const double period = 1.f;

    return HasSecondsTimerElapsed(period, &timer->last_tick_s);
}

extern bool Timer_Tick300s(daemon_timer_t * const timer)
{
    const double period = 300.f;

    return HasSecondsTimerElapsed(period, &timer->last_tick_s);
}

