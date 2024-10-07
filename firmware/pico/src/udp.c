#include "udp.h"
#include "lwip/pbuf.h"
#include "emitter.h"
#include "node_events.h"

#define BUFFER_SIZE (128)

static msg_fifo_t * msg_fifo;
static critical_section_t * critical;
static struct udp_pcb * udp_pcb;

static void Close(void)
{
    udp_remove(udp_pcb);
}

extern void UDP_Recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    (void)arg;
    (void)pcb;
    (void)addr;
    (void)port;
    cyw43_arch_lwip_check();
    uint8_t recv[BUFFER_SIZE] = {0U};
    assert(p->tot_len < BUFFER_SIZE); 
    
    if( p != NULL )
    {
        for(struct pbuf *q = p; q != NULL; q = q->next )
        {
            memset(recv,0x00, BUFFER_SIZE);
            pbuf_copy_partial(q, recv, BUFFER_SIZE, 0);
            
            uint8_t * msg_ptr = recv;
            /*
            for( uint32_t jdx = 0; jdx < q->len; jdx++)
            {
                printf("0x%x,", msg_ptr[jdx]);
            }
            
            printf("\b \n");
            */
            msg_t msg =
            {
                .data = (char*)msg_ptr,
                .len = q->len,
            };
                
            critical_section_enter_blocking(critical);
            if( !FIFO_IsFull( &msg_fifo->base ) )
            {
                FIFO_Enqueue( msg_fifo, msg);

                /* Only emit event if the message was actually put in buffer */
                Emitter_EmitEvent(EVENT(NTPReceived));
            }
            critical_section_exit(critical);

        }
        //udp_recved(udp_pcb, p->tot_len);
        pbuf_free(p);
    }
    else
    {
        printf("\tConnection Closed\n");
    }

    Close();
}

extern bool UDP_Send( uint8_t * buffer, uint16_t len, ip_addr_t ip, uint16_t port)
{
    bool ret = true;
    if( udp_pcb != NULL )
    {
        Close();
    }

    udp_pcb = udp_new();
    udp_recv( udp_pcb, UDP_Recv, NULL);
    struct pbuf * p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    memcpy(p->payload, buffer, len);


    cyw43_arch_lwip_begin();
    critical_section_enter_blocking(critical);
    err_t err = udp_sendto(udp_pcb, p,&ip,port);
    critical_section_exit(critical);
    cyw43_arch_lwip_end();
    
    pbuf_free(p);
    if(err != ERR_OK)
    {
        ret = false;
        Close();
    }

    return ret;
}

extern void UDP_Init(msg_fifo_t * fifo, critical_section_t * crit)
{
    printf("Initialising UDP Comms\n");
    msg_fifo = fifo;
    critical = crit;
}

