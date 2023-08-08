#include "msg_fifo.h"
#include <string.h>

static void Enqueue( fifo_base_t * const base );
static void Dequeue( fifo_base_t * const base );
static void Flush( fifo_base_t * const base );

extern void Message_Init(msg_fifo_t * fifo)
{
    printf("Initialising Message FIFO\n");
    
    static const fifo_vfunc_t vfunc =
    {
        .enq = Enqueue,
        .deq = Dequeue,
        .flush = Flush,
    };
    FIFO_Init( (fifo_base_t *)fifo, MSG_FIFO_LEN );
    
    fifo->base.vfunc = &vfunc;
    fifo->data = NULL;
}

static void Enqueue( fifo_base_t * const base )
{
    assert( base != NULL );
    msg_fifo_t * fifo = (msg_fifo_t *)base;

    /* Clear what was in there */
    memset(fifo->queue[ fifo->base.write_index ], 0x00, MSG_SIZE);

    int bytes_to_copy = fifo->data[1] + 2U;

    /* Copy the new string */
    memcpy(fifo->queue[ fifo->base.write_index ], fifo->data, bytes_to_copy);

    fifo->base.write_index++;
    fifo->base.fill++;
    fifo->base.write_index = ( fifo->base.write_index & ( fifo->base.max - 1U ) );
}

static void Dequeue( fifo_base_t * const base )
{
    assert( base != NULL );
    msg_fifo_t * fifo = (msg_fifo_t *)base;
    fifo->data = fifo->queue[ fifo->base.read_index ];
    fifo->base.read_index++;
    fifo->base.fill--;
    fifo->base.read_index = ( fifo->base.read_index & ( fifo->base.max - 1U ) );
}

static void Flush( fifo_base_t * const base )
{
    assert( base != NULL );
    msg_fifo_t * fifo = (msg_fifo_t *)base;
    fifo->base.read_index = fifo->base.write_index;
    fifo->base.fill = 0U;
}

