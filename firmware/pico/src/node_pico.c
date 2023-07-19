#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "events.h"
#include "fifo_base.h"
#include "state.h"

#define SIGNALS(SIG ) \
    SIG( Tick ) \

GENERATE_SIGNALS( SIGNALS );
GENERATE_SIGNAL_STRINGS( SIGNALS );

DEFINE_STATE( Idle );

event_fifo_t events;

static bool tick(struct repeating_timer *t)
{
    if( !FIFO_IsFull(&events.base) )
    {
        FIFO_Enqueue(&events, EVENT(Tick));
    }
    return true;
}

static state_ret_t State_Idle( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = NO_PARENT(this);

    switch(s)
    {
        case EVENT( Enter ):
        case EVENT( Exit ):
        case EVENT( Tick ):
        {
            ret = HANDLED();
            break;
        }
        default:
        {
            break;
        }
    }
}

int main()
{
    stdio_init_all();
    struct repeating_timer timer;
    
    Events_Init(&events);
    
    state_t state_machine; 
    state_machine.state = State_Idle; 
    
    FIFO_Enqueue( &events, EVENT(Enter) );

    add_repeating_timer_ms(1000U, tick, NULL, &timer);

    while( true )
    {
        while( FIFO_IsEmpty( (fifo_base_t *)&events ) )
        {
            tight_loop_contents();
        }
        event_t e = FIFO_Dequeue( &events );
        STATEMACHINE_Dispatch(&state_machine, e);
    }
    return 0;
}
