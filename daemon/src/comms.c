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

static uint8_t recv_buffer[BUFFER_SIZE] = {0U};

void Comms_Init(const comms_t * const comms)
{
    assert(comms->ip != NULL);
    assert(comms->port != NULL);
    assert(comms->fifo != NULL);

    printf("Initialising Comms\n");
    printf("\t%s:%s\n",comms->ip, comms->port);
}

bool Comms_Connect(comms_t * const comms)
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
    status = getaddrinfo( comms->ip, comms->port, &hints, &servinfo );
    if( status != 0U )
    {
        fprintf( stderr, "Comms Error: %s\n", gai_strerror( status ) );	
        ret = false;
    }
    else
    {   
        /* 2. Create Socket */
        comms->sock = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
        if( comms->sock < 0 )
        {
            printf("Error creating socket\n");
            ret = false;
        }
        else
        {
            /* 3. Attempt Connection */
            int c = connect( comms->sock, servinfo->ai_addr, servinfo->ai_addrlen);
            if( c < 0 )
            {
                printf("\tConnection Failed\n");
                ret = false;
            }
            else
            {
                /* 4. We have liftoff */ 
                comms->poll.fd = comms->sock;
                comms->poll.events = POLLIN;
                comms->connected = true;
                ret = true;
                FIFO_Flush(&comms->fifo->base);
            }
        }
    } 
    return ret;
}

extern bool Comms_Disconnected(comms_t * const comms)
{
    if(!comms->connected)
    {
        sleep(1U);
    }
    return !comms->connected;
}

extern bool Comms_MessageReceived(comms_t * const comms)
{
    bool msg_recvd = false;
    
    int rv = poll( &comms->poll, 1, 1 );

    if( rv & POLLIN )
    {
        msg_recvd = Comms_RecvToFifo(comms);
        if(!msg_recvd)
        {
            comms->connected = false;
        }
    }

    return msg_recvd;
}

bool Comms_Send(comms_t * const comms, uint8_t * buffer, uint16_t len)
{
    int ret = false;
    /* Send */
    int snd = send( comms->sock, buffer, len, 0);
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

bool Comms_Recv(comms_t * const comms, uint8_t * buffer, uint16_t len)
{
    bool ret = false;

    int rcv = recv( comms->sock, buffer, len, 0U);
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

bool Comms_RecvToFifo(comms_t * const comms)
{
    bool ret = false;
    memset(recv_buffer, 0x00, BUFFER_SIZE);
    int rcv = recv( comms->sock, recv_buffer, BUFFER_SIZE, 0U);
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
        if( !FIFO_IsFull( &comms->fifo->base ) )
        {
            FIFO_Enqueue( comms->fifo, msg);
            ret = true;
        }
    }
    
    return ret;
}

void Comms_Disconnect(comms_t * const comms)
{
    close(comms->sock);
}

