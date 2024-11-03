#include "daemon_events.h"

static void Enqueue( fifo_base_t * const fifo );
static void Dequeue( fifo_base_t * const fifo );
static void Flush( fifo_base_t * const fifo );

extern void DaemonEvents_Init(daemon_fifo_t * fifo)
{
    printf("Initialising DaemonEvents FIFO\n");
    
    static const fifo_vfunc_t vfunc =
    {
        .enq = Enqueue,
        .deq = Dequeue,
        .flush = Flush,
    };
    FIFO_Init( (fifo_base_t *)fifo, FIFO_LEN );
    
    fifo->base.vfunc = &vfunc;
    fifo->in = (state_event_t){.state = NULL, .event = 0x00};
    fifo->out = (state_event_t){.state = NULL, .event = 0x00};
    memset(fifo->queue, 0x00, FIFO_LEN * sizeof(fifo->in));

    EventObserver_Init(fifo->observer, (uint32_t)EVENT(EventCount));
}

extern void DaemonEvents_Enqueue(daemon_fifo_t * fifo, state_t * state, event_t event)
{
    assert(fifo != NULL);
    assert(state != NULL);
    state_event_t daemon_event = {.state = state, .event = event};
    FIFO_Enqueue(fifo, daemon_event);
}

extern void DaemonEvents_BroadcastEvent(daemon_fifo_t * fifo, event_t event)
{
    assert(fifo != NULL);

    const event_observer_t * const broadcast_event = EventObserver_GetSubs(fifo->observer, event);

    for(uint32_t idx = 0U; idx < broadcast_event->subscriptions; idx++)
    {
        assert(!FIFO_IsFull(&fifo->base));
        DaemonEvents_Enqueue(fifo, broadcast_event->subscriber[idx], event);
    }
}


extern void DaemonEvents_Subscribe(daemon_fifo_t * fifo, state_t * state, event_t event)
{
    EventObserver_Subscribe(fifo->observer, event, state);
}

static void Enqueue( fifo_base_t * const base )
{
    assert(base != NULL );
    ENQUEUE_BOILERPLATE( daemon_fifo_t, base );
}

static void Dequeue( fifo_base_t * const base )
{
    assert(base != NULL );
    DEQUEUE_BOILERPLATE( daemon_fifo_t, base );
}

static void Flush( fifo_base_t * const base )
{
    assert(base != NULL );
    FLUSH_BOILERPLATE( daemon_fifo_t, base );
}

