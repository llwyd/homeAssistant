#include "comms_sm.h"
#include "comms.h"

/* Top level states */
DEFINE_STATE(NotConnected);
DEFINE_STATE(Connected);

/* sub states */
DEFINE_STATE(TCPConnect);
DEFINE_STATE(MQTTConnect);

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

void BlankCallback(mqtt_data_t * data);

#define NUM_SUBS ( 1U )
static mqtt_subs_t subs[NUM_SUBS] = 
{
    {"blank_callback", mqtt_type_bool, BlankCallback},
};

static comms_t * comms;
static comms_state_t state_machine;
static daemon_fifo_t * event_fifo;
static mqtt_t mqtt;

#define NUM_COMMS_EVENTS (2)
static comms_callback_t comms_callback[NUM_COMMS_EVENTS] =
{
    {"MQTT Message Received", Comms_MessageReceived, EVENT(MessageReceived)},
    {"TCP Disconnect", Comms_Disconnected, EVENT(Disconnect)},
};

void BlankCallback(mqtt_data_t * data)
{
    (void)data;
    printf("Callback\n");
}

state_ret_t State_NotConnected( state_t * this, event_t s )
{
    STATE_DEBUG( s );
    state_ret_t ret = NO_PARENT(this);
    comms_state_t * state = (comms_state_t *)this;

    switch( s )
    {
        case EVENT( Enter ):
        case EVENT( Exit ):
            ret = HANDLED();
            break;
        default:
            break;
    }

    return ret;

}

state_ret_t State_TCPConnect( state_t * this, event_t s )
{
    STATE_DEBUG( s );
    state_ret_t ret = PARENT(this, STATE(NotConnected));
    comms_state_t * state = (comms_state_t *)this;

    switch( s )
    {
        case EVENT( Enter ):
        {
            state->retry_count = 0U;
            if( Comms_Connect(comms) )
            {
                printf("\tTCP Connection successful\n");
                ret = TRANSITION(this, STATE(MQTTConnect));
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
        default:
            break;
    }

    return ret;
    
}

state_ret_t State_MQTTConnect( state_t * this, event_t s )
{
    STATE_DEBUG( s );
    state_ret_t ret = PARENT(this, STATE(NotConnected));
    comms_state_t * state = (comms_state_t *)this;

    switch( s )
    {
        case EVENT( Enter ):
        {
            if(MQTT_Connect(&mqtt))
            {
                ret = HANDLED();
            }
            else
            {
                ret = TRANSITION(this, STATE(TCPConnect));
            }
            break;
        }
        case EVENT( Exit ):
            ret = HANDLED();
            break;
        
        case EVENT( MessageReceived ):
            assert( !FIFO_IsEmpty( &comms->fifo->base ) );
            msg_t msg = FIFO_Dequeue(comms->fifo);
            if( MQTT_HandleMessage(&mqtt, (uint8_t*)msg.data) )
            {
                ret = TRANSITION(this, STATE(Connected) );
            }
            else
            {
                ret = HANDLED();
            }

            break;
        case EVENT( Disconnect ):
            ret = TRANSITION(this, STATE(TCPConnect));
            break;
        default:
            break;
    }

    return ret;
}

state_ret_t State_Connected( state_t * this, event_t s )
{
    STATE_DEBUG( s );
    state_ret_t ret = NO_PARENT(this);
    switch( s )
    {
        case EVENT( Enter ):
            DaemonEvents_BroadcastEvent(event_fifo, EVENT(BrokerConnected));
            if( MQTT_Subscribe(&mqtt) )
            {
                ret = HANDLED();
            }
            else
            {
                ret = TRANSITION(this, STATE(TCPConnect) );
            }
            break;
        case EVENT( Exit ):
            ret = HANDLED();
            break;
        case EVENT( MessageReceived ):
            assert( !FIFO_IsEmpty( &comms->fifo->base ) );
            msg_t msg = FIFO_Dequeue(comms->fifo);
            if( MQTT_HandleMessage(&mqtt, (uint8_t *)msg.data) )
            {
                ret = HANDLED();
            }
            else
            {
                ret = TRANSITION(this, STATE(TCPConnect) );
            }
            break;
        case EVENT( Disconnect ):
            ret = TRANSITION(this, STATE(TCPConnect));
            break;
        default:
            break;
    }

    return ret;
}

static bool Send(uint8_t * buffer, uint16_t len)
{
    return Comms_Send(comms, buffer, len);
}

static bool Recv(uint8_t * buffer, uint16_t len)
{
    return Comms_Recv(comms, buffer, len);
}

extern void CommsSM_Init(comms_settings_t * settings, comms_t * tcp_comms, daemon_fifo_t * fifo)
{
    printf("!---------------------------!\n");
    printf("!   Init Comms SM           !\n");
    printf("!---------------------------!\n");

    comms = tcp_comms;
    event_fifo = fifo;
    Message_Init(comms->fifo);
    
    mqtt = (mqtt_t){
        .client_name = settings->client_name,
        .send = Send,
        .recv = Recv,
        .subs = subs,
        .num_subs = NUM_SUBS,
    };
    MQTT_Init(&mqtt);
   
    state_machine.state.state = State_TCPConnect;
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
        if( comms_callback[idx].event_fn(comms) )
        {
            DaemonEvents_Enqueue( events, CommsSM_GetState(), comms_callback[idx].event );
        }
    }
}

