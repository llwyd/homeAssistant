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
}

extern void DaemonEvents_Enqueue(daemon_fifo_t * fifo, state_t * state, event_t event)
{
    state_event_t daemon_event = {.state = state, .event = event};
    FIFO_Enqueue(fifo, daemon_event);
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

