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

extern void Timer_Init(void)
{
    printf("Initialising timers\n");
    time(&start_time);
}

extern bool Timer_Tick500ms(void)
{
    bool timerElapsed = false;
    
    static struct timespec current_tick;
    static struct timespec last_tick;

    timespec_get( &current_tick, TIME_UTC );
    if( (unsigned long)( current_tick.tv_nsec - last_tick.tv_nsec ) >= 500000000UL )
    {
        last_tick = current_tick;
        timerElapsed = true;
    }

    return timerElapsed;
}

extern bool Timer_Tick1s(void)
{
    const double period = 1.f;
    static time_t last_tick;

    return HasSecondsTimerElapsed(period, &last_tick);
}

extern bool Timer_Tick60s(void)
{
    const double period = 60.f;
    static time_t last_tick;

    return HasSecondsTimerElapsed(period, &last_tick);
}

