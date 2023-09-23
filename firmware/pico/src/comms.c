#include "comms.h"
#include "node_events.h"
#include "emitter.h"
#include "eeprom.h"

#define MQTT_PORT ( 1883 )
#define MQTT_TIMEOUT ( 0xb4 )

#define BUFFER_SIZE (128)
#define POLL_PERIOD (5U)

static ip_addr_t remote_addr;
static struct tcp_pcb * tcp_pcb;
static bool connected = false;
static char * client_name = "pico";
static msg_fifo_t * msg_fifo;
static critical_section_t * critical;

static uint8_t broker_ip[EEPROM_ENTRY_SIZE] = {0U};

/* LWIP callback functions */
static err_t Sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t Recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void Error(void *arg, err_t err);
static err_t Connected(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t Poll(void *arg, struct tcp_pcb *tpcb);

static err_t Sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    //printf("\tTCP Sent\n");
//    printf("\n--Sent %d bytes total\n", len);
    return ERR_OK;
}

static err_t Recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
   // printf("\tTCP Recv\n");
  //  printf("\n--Recv %d bytes total\n", p->tot_len);
    cyw43_arch_lwip_check();
    err_t ret = ERR_OK;
    uint8_t recv[BUFFER_SIZE] = {0U};
    assert(p->tot_len < BUFFER_SIZE); 
    if( p != NULL )
    {
        for(struct pbuf *q = p; q != NULL; q = q->next )
        {
            //printf("\tMessage len %d bytes\n", p->len);
           
            memset(recv,0x00, BUFFER_SIZE);
            pbuf_copy_partial(q, recv, BUFFER_SIZE, 0);
            /* Handle bunched together mqtt packets */
            
            uint8_t * msg_ptr = recv;
            //uint8_t * msg_ptr = (uint8_t*)q->payload;
            uint32_t size_to_copy = 0U;

            for( uint32_t idx = 0; idx < q->len; idx+=size_to_copy )
            {
                size_to_copy = *(msg_ptr + 1) + 2U;
               
                msg_t msg =
                {
                    .data = msg_ptr,
                    .len = size_to_copy,
                };
                /*            
                for( uint32_t jdx = 0; jdx < size_to_copy; jdx++)
                {
                    printf("0x%x,", msg_ptr[jdx]);
                }
                
                printf("\b \n");
                */
                critical_section_enter_blocking(critical);
                if( !FIFO_IsFull( &msg_fifo->base ) )
                {
                    FIFO_Enqueue( msg_fifo, msg);

                    /* Only emit event if the message was actually put in buffer */
                    Emitter_EmitEvent(EVENT(MessageReceived));
                }
                critical_section_exit(critical);

                msg_ptr += size_to_copy;
            }
        }
        tcp_recved(tpcb, p->tot_len);
        pbuf_free(p);
    }
    else
    {
        printf("\tConnection Closed\n");
        Emitter_EmitEvent(EVENT(TCPDisconnected));
        ret = ERR_CLSD;
    }

    return ret;
}

extern void Comms_Close(void)
{
    connected = false;
    err_t close_err = tcp_close(tcp_pcb);
    if( close_err != ERR_OK )
    {
        tcp_abort(tcp_pcb);
    }
    tcp_pcb = NULL;
}

static void Error(void *arg, err_t err)
{
    printf("\tTCP Error\n");
    Emitter_EmitEvent(EVENT(TCPDisconnected));
}

static err_t Connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    printf("\tTCP Connected...");
    connected = true;
    if( err == ERR_OK )
    {
        printf("OK\n");
        Emitter_EmitEvent(EVENT(TCPConnected));
    }
    else
    {
        printf("FAIL\n");
    }
    return ERR_OK;
}

static err_t Poll(void *arg, struct tcp_pcb *tpcb)
{
    printf("\tTCP Poll\n");

    return ERR_OK;
}

extern void Comms_MQTTConnect(void)
{
}

extern bool Comms_Send( uint8_t * buffer, uint16_t len )
{
    critical_section_enter_blocking(critical);
    cyw43_arch_lwip_begin();
    err_t err = tcp_write(tcp_pcb, buffer, len, TCP_WRITE_FLAG_COPY);
    cyw43_arch_lwip_end();
    critical_section_exit(critical);
    bool success = true;
    if( err != ERR_OK )
    {
        printf("\nFailed to write\n");
        success = false;
        goto cleanup;
    }
    
    critical_section_enter_blocking(critical);
    cyw43_arch_lwip_begin();
    err = tcp_output(tcp_pcb);  
    cyw43_arch_lwip_end();
    critical_section_exit(critical);
    if( err != ERR_OK )
    {
        printf("\nFailed to output\n");
        success = false;
        goto cleanup;
    }
    
cleanup:
    return success;
}

extern bool Comms_Recv( uint8_t * buffer, uint16_t len )
{
    return true;
}

extern bool Comms_TCPInit(void)
{
    bool ret = false;
    connected = false;

    if(tcp_pcb != NULL )
    {
        Comms_Close();
    }

    /* Init */
    ip4addr_aton(broker_ip, &remote_addr);
    
    printf("\tInitialising TCP Comms\n");
    printf("\tAttempting Connection to %s port %d\n", ip4addr_ntoa(&remote_addr), MQTT_PORT);

    /* Initialise pcb struct */
    tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&remote_addr));
    if( tcp_pcb == NULL )
    {
        printf("\tFailed to create\n");
        assert(false);
    }

    /* Define Callbacks */
    // tcp_arg
    tcp_sent(tcp_pcb, Sent);
    //tcp_poll(tcp_pcb, Poll, POLL_PERIOD);
    tcp_recv(tcp_pcb, Recv);
    tcp_err(tcp_pcb, Error);

    /* Attempt connection */
    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(tcp_pcb, &remote_addr, MQTT_PORT, Connected);
    cyw43_arch_lwip_end();

    if(err==ERR_OK)
    {
        printf("\tTCP Initialising success, awaiting connection\n");
        ret = true;
    }
    else
    {
        printf("\tTCP Initialising failure, Retrying\n");
        err_t close_err = tcp_close(tcp_pcb);
        if( close_err != ERR_OK )
        {
            tcp_abort(tcp_pcb);
        }
        tcp_pcb = NULL;
        ret = false;
    }

    return ret;
}

extern bool Comms_CheckStatus(void)
{
    return connected;
}

extern void Comms_Init(msg_fifo_t * fifo, critical_section_t * crit)
{
    printf("Initialising Comms\n");
    printf("\tRetrieving settings from EEPROM\n");

    EEPROM_Read(broker_ip, EEPROM_ENTRY_SIZE, EEPROM_IP);

    printf("\tBroker IP: %s\n", broker_ip);
    msg_fifo = fifo;
    critical = crit;
}

