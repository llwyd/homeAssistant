#include "comms_sm.h"
#include "comms.h"

DEFINE_STATE(Connect);
DEFINE_STATE(Connected);

typedef struct
{
    state_t state;
    uint32_t retry_count;
}
comms_state_t;

typedef struct
{
    char * name;
    bool (*event_fn)(comms_t * const comms);
    event_t event;
}
comms_callback_t;

static comms_t comms;
static comms_state_t state_machine;
static daemon_fifo_t * event_fifo;

#define NUM_COMMS_EVENTS (2)
static comms_callback_t comms_callback[NUM_COMMS_EVENTS] =
{
    {"MQTT Message Received", Comms_MessageReceived, EVENT(MessageReceived)},
    {"TCP Disconnect", Comms_Disconnected, EVENT(BrokerDisconnect)},
};

state_ret_t State_Connect( state_t * this, event_t s )
{
    TimeStamp_Print();
    printf("[COMMS] ");
    STATE_DEBUG( s );
    state_ret_t ret;
    comms_state_t * state = (comms_state_t *)this;

    switch( s )
    {
        case EVENT( Enter ):
        {
            state->retry_count = 0U;
            if( Comms_Connect(&comms) )
            {
                printf("\tTCP Connection successful\n");
                ret = TRANSITION(this, STATE(Connected));
            }
            else
            {
                ret = HANDLED();
            }
            break;
        }
        case EVENT( Exit ):
            ret = HANDLED();
            break;
        case EVENT( None ):
            assert(false);
            break;
        default:
            ret = HANDLED();
            break;
    }

    return ret;
    
}

state_ret_t State_Connected( state_t * this, event_t s )
{
    TimeStamp_Print();
    printf("[COMMS] ");
    STATE_DEBUG( s );
    state_ret_t ret;
    switch( s )
    {
        case EVENT( Enter ):
        case EVENT( Exit ):
            ret = HANDLED();
            break;
        case EVENT( MessageReceived ):
            /* Need to check whether event disconnected */
            /*
            assert( !FIFO_IsEmpty( &msg_fifo.base ) );
            msg_t msg = FIFO_Dequeue(&msg_fifo);
            */
            ret = HANDLED();
            break;
        case EVENT( Disconnect ):
            ret = TRANSITION(this, STATE(Connect));
            break;
        case EVENT( None ):
            assert(false);
            break;
        default:
            ret = HANDLED();
            break;
    }

    return ret;
}

extern void CommsSM_Init(comms_settings_t * settings, daemon_fifo_t * fifo)
{
    printf("!---------------------------!\n");
    printf("!   Init Comms SM           !\n");
    printf("!---------------------------!\n");
    memset(&comms, 0x00, sizeof(comms_t));
    comms.ip = settings->ip;
    comms.port = settings->port;
    comms.fifo = settings->msg_fifo;

    event_fifo = fifo;

    Message_Init(settings->msg_fifo);
    Comms_Init(&comms);
   
    state_machine.state.state = State_Connect;
    state_machine.retry_count = 0U;
    DaemonEvents_Enqueue(event_fifo, &state_machine.state, EVENT(Enter));

    printf("!---------------------------!\n");
    printf("!   Complete!               !\n");
    printf("!---------------------------!\n");

}

extern state_t * const CommsSM_GetState(void)
{
    return &state_machine.state;
}

extern void CommsSM_RefreshEvents( daemon_fifo_t * events )
{
    for( int idx = 0; idx < NUM_COMMS_EVENTS; idx++ )
    {
        if( comms_callback[idx].event_fn(&comms) )
        {
            DaemonEvents_Enqueue( events, CommsSM_GetState(), comms_callback[idx].event );
        }
    }
}

