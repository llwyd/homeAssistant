#include "comms.h"
#include <assert.h>
#include <inttypes.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "msg_fifo.h"
#include "fifo_base.h"

#define BUFFER_SIZE (128)

static msg_fifo_t * msg_fifo;
static char * comms_ip;
static char * comms_port;
static int sock;
static struct pollfd comms_poll;
static uint8_t recv_buffer[BUFFER_SIZE] = {0U};
static bool connected = false;

void Comms_Init(char * ip, char * port, msg_fifo_t * fifo)
{
    assert(ip != NULL);
    assert(port != NULL);
    assert(comms_ip == NULL);
    assert(comms_port == NULL);

    printf("Initialising Comms\n");
    printf("\t%s:%s\n",ip, port);
    comms_ip = ip;
    comms_port = port;
    msg_fifo = fifo;
}

bool Comms_Connect(void)
{
    bool ret = false;
    
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset( &hints, 0U, sizeof( hints ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    /* 1. Resolve port information */
    status = getaddrinfo( comms_ip, comms_port, &hints, &servinfo );
    if( status != 0U )
    {
        fprintf( stderr, "Comms Error: %s\n", gai_strerror( status ) );	
        ret = false;
    }
    else
    {   
        /* 2. Create Socket */
        sock = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
        if( sock < 0 )
        {
            printf("Error creating socket\n");
            ret = false;
        }
        else
        {
            /* 3. Attempt Connection */
            int c = connect( sock, servinfo->ai_addr, servinfo->ai_addrlen);
            if( c < 0 )
            {
                printf("\tConnection Failed\n");
                ret = false;
            }
            else
            {
                /* 4. We have liftoff */ 
                comms_poll.fd = sock;
                comms_poll.events = POLLIN;
                ret = true;
                connected = true;
                FIFO_Flush(&msg_fifo->base);
            }
        }
    } 
    return ret;
}

extern bool Comms_Disconnected(void)
{
    if(!connected)
    {
        sleep(1U);
    }
    return !connected;
}
extern bool Comms_MessageReceived(void)
{
    bool msg_recvd = false;
    
    int rv = poll( &comms_poll, 1, 1 );

    if( rv & POLLIN )
    {
        msg_recvd = Comms_RecvToFifo();
        if(!msg_recvd)
        {
            connected = false;
        }
    }

    return msg_recvd;
}

bool Comms_Send(uint8_t * buffer, uint16_t len)
{
    int ret = false;
    /* Send */
    int snd = send( sock, buffer, len, 0);
    if( snd < 0 )
    {
        printf("\t Error Sending Data\n");
        ret = false;
    }
    else
    {
        ret = true;
    }

    return ret;
}

bool Comms_Recv(uint8_t * buffer, uint16_t len)
{
    bool ret = false;

    int rcv = recv( sock, buffer, len, 0U);
    if( rcv < 0 )
    {
        printf("\tError Sending Data\n");
        ret = false;
    }
    else if( rcv == 0 )
    {
        printf("\tConnection Closed\n");
        ret = false;
    }
    else
    {
        ret = true;
    }
    
    return ret;
}

bool Comms_RecvToFifo(void)
{
    bool ret = false;
    memset(recv_buffer, 0x00, BUFFER_SIZE);
    int rcv = recv( sock, recv_buffer, BUFFER_SIZE, 0U);
    if( rcv < 0 )
    {
        printf("\tError Sending Data\n");
        ret = false;
    }
    else if( rcv == 0 )
    {
        printf("\tConnection Closed\n");
        ret = false;
    }
    else
    {
        uint8_t * msg_ptr = (uint8_t *)recv_buffer;
        uint32_t size_to_copy = 0U;
        size_to_copy = *(msg_ptr + 1) + 2U;
        msg_t msg =
        {
            .data = (char *)msg_ptr,
            .len = size_to_copy,
        };
        if( !FIFO_IsFull( &msg_fifo->base ) )
        {
            FIFO_Enqueue( msg_fifo, msg);
            ret = true;
        }
    }
    
    return ret;
}

