#include "emitter.h"
#include "fifo_base.h"


static critical_section_t * critical;
static event_fifo_t * fifo;

static bool Callback(struct repeating_timer *t)
{
    event_t event = (event_t)t->user_data;
    critical_section_enter_blocking(critical);

    if( !FIFO_IsFull(&fifo->base) )
    {
        FIFO_Enqueue(fifo, event);
    }
    
    critical_section_exit(critical);
    return true;
}

extern void Emitter_Init( event_fifo_t * event_fifo, critical_section_t * crit )
{
    critical = crit;
    fifo = event_fifo;
}
extern void Emitter_Create( event_t event, struct repeating_timer * timer, int32_t period )
{
    add_repeating_timer_ms(period, Callback, (void*)event, timer);
}

extern void Emitter_Destroy( struct repeating_timer * timer )
{
    cancel_repeating_timer(timer);
}

extern bool Emitter_EmitEvent(event_t e)
{
    bool success = false;
    critical_section_enter_blocking(critical);
    if( !FIFO_IsFull(&fifo->base) )
    {
        FIFO_Enqueue(fifo, e);
        success = true;
    }
    critical_section_exit(critical);

    return success;
}

