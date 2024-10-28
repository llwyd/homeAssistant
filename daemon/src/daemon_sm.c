/*
*
*	Home Automation Daemon
*
*	T.L. 2020 - 2022
*
*/

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#include "daemon_sm.h"
#include "daemon_events.h"
#include "fifo_base.h"
#include "state.h"
#include "mqtt.h"
#include "sensor.h"
#include "timestamp.h"
#include "events.h"
#include "timer.h"
#include "comms.h"
#include "msg_fifo.h"
#include "meta.h"

#define SIGNALS(SIG ) \
    SIG( Tick ) \
    SIG( MessageReceived ) \
    SIG( Disconnect ) \
    SIG( Heartbeat ) \
    SIG( UpdateHomepage ) \

GENERATE_SIGNALS( SIGNALS );
GENERATE_SIGNAL_STRINGS( SIGNALS );

DEFINE_STATE(Connect);
DEFINE_STATE(MQTTConnect);
DEFINE_STATE(Subscribe);
DEFINE_STATE(Idle);

typedef struct
{
    char * name;
    bool (*event_fn)(comms_t * const comms);
    event_t event;
}
comms_callback_t;

#define CONNECT_ATTEMPTS (5U)

typedef struct
{
    state_t state;
    uint32_t retry_count;
}
daemon_state_t;


daemon_state_t state_machine;
daemon_fifo_t * event_fifo;

static char * client_name;
static event_fifo_t events;
static msg_fifo_t msg_fifo;
static mqtt_t mqtt;

static comms_t comms;

void Daemon_OnBoardLED( mqtt_data_t * data );
void Heartbeat( void );
bool Send(uint8_t * buffer, uint16_t len);
bool Recv(uint8_t * buffer, uint16_t len);


#define NUM_SUBS ( 1U )
static mqtt_subs_t subs[NUM_SUBS] = 
{
    {"debug_led", mqtt_type_bool, Daemon_OnBoardLED},
};

#define NUM_COMMS_EVENTS (2)
static comms_callback_t comms_callback[NUM_COMMS_EVENTS] =
{
    {"MQTT Message Received", Comms_MessageReceived, EVENT(MessageReceived)},
    {"TCP Disconnect", Comms_Disconnected, EVENT(Disconnect)},
};

#define NUM_EVENTS (3)
static event_callback_t event_callback[NUM_EVENTS] =
{
    {"Tick", Timer_Tick1s, EVENT(Tick)},
    {"Heartbeat Led", Timer_Tick500ms, EVENT(Heartbeat)},
    {"Homepage Update", Timer_Tick60s, EVENT(UpdateHomepage)},
};

state_ret_t State_Connect( state_t * this, event_t s )
{
    TimeStamp_Print();
    STATE_DEBUG( s );
    state_ret_t ret;
    daemon_state_t * state = (daemon_state_t *)this;

    switch( s )
    {
        case EVENT( Tick ):
        {
            state->retry_count++;
            if (state->retry_count > CONNECT_ATTEMPTS)
            {
                /* Attempt again */
                state->retry_count = 0U;
                if( Comms_Connect(&comms) )
                {
                    printf("\tTCP Connection successful\n");
                    ret = TRANSITION(this, STATE(MQTTConnect));
                }
                else
                {
                    ret = HANDLED();
                }
            }
            break;
        }
        case EVENT( Enter ):
        {
            state->retry_count = 0U;
            if( Comms_Connect(&comms) )
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
        case EVENT( Heartbeat ):
            Heartbeat();
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

state_ret_t State_MQTTConnect( state_t * this, event_t s )
{
    TimeStamp_Print();
    STATE_DEBUG( s );
    state_ret_t ret;
    daemon_state_t * state = (daemon_state_t *)this;
    
    switch( s )
    {
        case EVENT( Tick ):
        {
            state->retry_count++;
            if (state->retry_count > CONNECT_ATTEMPTS)
            {
                state->retry_count = 0U;
                if(MQTT_Connect(&mqtt))
                {
                    ret = HANDLED();
                }
                else
                {
                    ret = TRANSITION(this, STATE(Connect));
                }
            }
            break;
        }
        case EVENT( Enter ):
        {
            state->retry_count = 0U;
            if(MQTT_Connect(&mqtt))
            {
                ret = HANDLED();
            }
            else
            {
                ret = TRANSITION(this, STATE(Connect));
            }
            break;
        }
        case EVENT( MessageReceived ):
            assert( !FIFO_IsEmpty( &msg_fifo.base ) );
            msg_t msg = FIFO_Dequeue(&msg_fifo);
            if( MQTT_HandleMessage(&mqtt, (uint8_t*)msg.data) )
            {
                ret = TRANSITION(this, STATE(Subscribe) );
            }
            else
            {
                ret = HANDLED();
            }

            break;
        case EVENT( Exit ):
            ret = HANDLED();
            break;
        case EVENT( Disconnect ):
            ret = TRANSITION(this, STATE(Connect));
            break;
        case EVENT( Heartbeat ):
            Heartbeat();
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

state_ret_t State_Subscribe( state_t * this, event_t s )
{
    TimeStamp_Print();
    STATE_DEBUG( s );
    state_ret_t ret;
    switch( s )
    {
        case EVENT( Enter ):
            if( MQTT_Subscribe(&mqtt) )
            {
                ret = HANDLED();
            }
            else
            {
                ret = TRANSITION(this, STATE(Connect) );
            }
            break;
        case EVENT( Exit ):
            ret = HANDLED();
            break;
        case EVENT( Disconnect ):
            ret = TRANSITION(this, STATE(Connect));
            break;
        case EVENT( MessageReceived ):
            assert( !FIFO_IsEmpty( &msg_fifo.base ) );
            msg_t msg = FIFO_Dequeue(&msg_fifo);
            if( MQTT_HandleMessage(&mqtt, (uint8_t*)msg.data) )
            {
                if( MQTT_AllSubscribed(&mqtt) )
                {
                    ret = TRANSITION(this, STATE(Idle) );
                }
                else
                {
                    ret = HANDLED();
                }
            }
            else
            {
                ret = TRANSITION(this, STATE(Connect) );
            }

            break;
        case EVENT( Heartbeat ):
            Heartbeat();
            ret = HANDLED();
            break;
        case EVENT( None ):
            assert(false);
            break;
        default:
        case EVENT( Tick ):
            ret = HANDLED();
            break;
    }

    return ret;
    
}

state_ret_t State_Idle( state_t * this, event_t s )
{
    TimeStamp_Print();
    STATE_DEBUG( s );
    state_ret_t ret;
    switch( s )
    {
        case EVENT( Enter ):
        case EVENT( Exit ):
            ret = HANDLED();
            break;
        case EVENT( Tick ):
            {
                Sensor_Read();
                char * json = Sensor_GenerateJSON();
                if( MQTT_Publish(&mqtt, "environment", json))
                {
                    ret = HANDLED();
                }
                else
                {
                    ret = TRANSITION(this, STATE(Connect) );
                }
            }
            break;
        case EVENT( UpdateHomepage ):
            {
                char * json = Sensor_GenerateJSON();
                if( MQTT_Publish(&mqtt, "summary", json))
                {
                    ret = HANDLED();
                }
                else
                {
                    ret = TRANSITION(this, STATE(Connect) );
                }
            }
            break;
        case EVENT( MessageReceived ):
            /* Need to check whether event disconnected */
            assert( !FIFO_IsEmpty( &msg_fifo.base ) );
            msg_t msg = FIFO_Dequeue(&msg_fifo);
            if( MQTT_HandleMessage(&mqtt, (uint8_t *)msg.data) )
            {
                ret = HANDLED();
            }
            else
            {
                ret = TRANSITION(this, STATE(Connect) );
            }
            break;
        case EVENT( Disconnect ):
            ret = TRANSITION(this, STATE(Connect));
            break;
        case EVENT( Heartbeat ):
            Heartbeat();
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

extern void Daemon_RefreshEvents( daemon_fifo_t * events )
{
    for( int idx = 0; idx < NUM_COMMS_EVENTS; idx++ )
    {
        if( comms_callback[idx].event_fn(&comms) )
        {
            DaemonEvents_Enqueue( events, Daemon_GetState(), comms_callback[idx].event );
        }
    }

    for( int idx = 0; idx < NUM_EVENTS; idx++ )
    {
        if( event_callback[idx].event_fn() )
        {
            DaemonEvents_Enqueue( events, Daemon_GetState(), event_callback[idx].event );
        }
    }
}

void Daemon_OnBoardLED( mqtt_data_t * data )
{
    if( data->b )
    {
        printf("\tLED ON\n");
    }
    else
    {
        printf("\tLED OFF\n");
    }
}

void Heartbeat( void )
{
#ifdef __arm__
    int led_fd = open("/sys/class/leds/ACT/brightness", O_WRONLY );
    static bool led_on;

    if( led_on )
    {
        write( led_fd, "1", 1 );
    }
    else
    {
        write( led_fd, "0", 1 );
    }
    close( led_fd );
    led_on ^= true;
#endif
}

bool Send(uint8_t * buffer, uint16_t len)
{
    return Comms_Send(&comms, buffer, len);
}

bool Recv(uint8_t * buffer, uint16_t len)
{
    return Comms_Recv(&comms, buffer, len);
}

extern state_t * const Daemon_GetState(void)
{
    return &state_machine.state;
}

extern void Daemon_Init(daemon_settings_t * settings, daemon_fifo_t * fifo)
{
    printf("!   Initialising Daemon     !\n");
    printf("!---------------------------!\n");
    memset(&comms, 0x00, sizeof(comms_t));
    comms.ip = settings->broker_ip;
    comms.port = settings->broker_port;
    comms.fifo = &msg_fifo;

    event_fifo = fifo;

    Message_Init(&msg_fifo);
    Comms_Init(&comms);
    mqtt_t mqtt_config = {
        .client_name = settings->client_name,
        .send = Send,
        .recv = Recv,
        .subs = subs,
        .num_subs = NUM_SUBS,
    };
    mqtt = mqtt_config;
    MQTT_Init(&mqtt);
   
    state_machine.state.state = State_Connect;
    state_machine.retry_count = 0U;

    DaemonEvents_Enqueue(event_fifo, &state_machine.state, EVENT(Enter));

    printf("!---------------------------!\n");
    printf("!   Complete!               !\n");
    printf("!---------------------------!\n");
}

