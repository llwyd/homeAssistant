#include "msg_fifo.h"
#include <string.h>
#include <assert.h>

#ifdef PICO_BOARD
static critical_section_t * critical;
#endif

static void Enqueue( fifo_base_t * const base );
static void Dequeue( fifo_base_t * const base );
static void Flush( fifo_base_t * const base );
static void Peek( fifo_base_t * const base );

/* Need critical section... */

#ifdef PICO_BOARD
extern void Message_Init(msg_fifo_t * fifo, critical_section_t * crit)
#else
extern void Message_Init(msg_fifo_t * fifo)
#endif
{
    printf("Initialising Message FIFO\n");
    
    static const fifo_vfunc_t vfunc =
    {
        .enq = Enqueue,
        .deq = Dequeue,
        .flush = Flush,
        .peek = Peek,
    };
    FIFO_Init( (fifo_base_t *)fifo, MSG_FIFO_LEN );
    
    fifo->base.vfunc = &vfunc;
#ifdef PICO_BOARD
    critical = crit;
#endif
}


static void Enqueue( fifo_base_t * const base )
{
    assert( base != NULL );
#ifdef PICO_BOARD
    critical_section_enter_blocking(critical);
#endif

    msg_fifo_t * fifo = (msg_fifo_t *)base;

//    printf("ENQ read:%d, write:%d, Fill:%d\n", fifo->base.read_index, fifo->base.write_index, fifo->base.fill);
    /* Clear what was in there */
    memset(fifo->queue[ fifo->base.write_index ], 0x00, MSG_SIZE);

    uint32_t bytes_to_copy = fifo->in.len;
    assert(bytes_to_copy <= MSG_SIZE);
    /* Copy the new string */
    memcpy(fifo->queue[ fifo->base.write_index ], fifo->in.data, bytes_to_copy);

    fifo->base.write_index++;
    fifo->base.fill++;
    fifo->base.write_index = ( fifo->base.write_index & ( fifo->base.max - 1U ) );
//    printf("ENQ read:%d, write:%d, Fill:%d\n", fifo->base.read_index, fifo->base.write_index, fifo->base.fill);
    
#ifdef PICO_BOARD
    critical_section_exit(critical);
#endif
}

static void Dequeue( fifo_base_t * const base )
{
#ifdef PICO_BOARD
    critical_section_enter_blocking(critical);
#endif
    assert( base != NULL );
    msg_fifo_t * fifo = (msg_fifo_t *)base;
//    printf("DEQ read:%d, write:%d, Fill:%d\n", fifo->base.read_index, fifo->base.write_index, fifo->base.fill);
    fifo->out.data = fifo->queue[ fifo->base.read_index ];
    
    fifo->base.read_index++;
    fifo->base.fill--;
    fifo->base.read_index = ( fifo->base.read_index & ( fifo->base.max - 1U ) );
//    printf("DEQ read:%d, write:%d, Fill:%d\n", fifo->base.read_index, fifo->base.write_index, fifo->base.fill);
    
#ifdef PICO_BOARD
    critical_section_exit(critical);
#endif
}

static void Peek( fifo_base_t * const base )
{
#ifdef PICO_BOARD
    critical_section_enter_blocking(critical);
#endif    
    assert( base != NULL );
    msg_fifo_t * fifo = (msg_fifo_t *)base;
//    printf("PK read:%d, write:%d, Fill:%d\n", fifo->base.read_index, fifo->base.write_index, fifo->base.fill);
    char * ptr = fifo->queue[ fifo->base.read_index ];
    
    fifo->out.data = ptr;
#ifdef PICO_BOARD
    critical_section_exit(critical);
#endif
}

static void Flush( fifo_base_t * const base )
{
#ifdef PICO_BOARD
    critical_section_enter_blocking(critical);
#endif
    assert( base != NULL );
    msg_fifo_t * fifo = (msg_fifo_t *)base;
    fifo->base.read_index = fifo->base.write_index;
    fifo->base.fill = 0U;
#ifdef PICO_BOARD
    critical_section_exit(critical);
#endif
}

