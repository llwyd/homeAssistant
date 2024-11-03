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
/*
#define SIGNALS(SIG ) \
    SIG( Tick ) \
    SIG( MessageReceived ) \
    SIG( Disconnect ) \
    SIG( Heartbeat ) \
    SIG( UpdateHomepage ) \

GENERATE_SIGNALS( SIGNALS );
GENERATE_SIGNAL_STRINGS( SIGNALS );
*/
DEFINE_STATE(AwaitingConnection);
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


static daemon_state_t state_machine;
static daemon_fifo_t * event_fifo;

static char * client_name;
static event_fifo_t events;
static msg_fifo_t msg_fifo;
static mqtt_t mqtt;

static comms_t * comms;

void Daemon_OnBoardLED( mqtt_data_t * data );
void Heartbeat( void );
static bool Send(uint8_t * buffer, uint16_t len);
static bool Recv(uint8_t * buffer, uint16_t len);


#define NUM_SUBS ( 1U )
static mqtt_subs_t subs[NUM_SUBS] = 
{
    {"debug_led", mqtt_type_bool, Daemon_OnBoardLED},
};

#define NUM_EVENTS (3)
static event_callback_t event_callback[NUM_EVENTS] =
{
    {"Tick", Timer_Tick1s, EVENT(Tick)},
    {"Heartbeat Led", Timer_Tick500ms, EVENT(Heartbeat)},
    {"Homepage Update", Timer_Tick60s, EVENT(UpdateHomepage)},
};

state_ret_t State_AwaitingConnection( state_t * this, event_t s )
{
    STATE_DEBUG( s );
    state_ret_t ret = NO_PARENT(this);

    switch( s )
    {
        case EVENT( Enter ):
        case EVENT( Exit ):
            ret = HANDLED();
            break;
        case EVENT( BrokerConnected ):
            ret = TRANSITION(this, STATE(Idle));
            break;
        default:
            break;
    }

    return ret;

}

state_ret_t State_Subscribe( state_t * this, event_t s )
{
    STATE_DEBUG( s );
    state_ret_t ret = NO_PARENT(this);
    switch( s )
    {
        case EVENT( Enter ):
        case EVENT( Exit ):
            ret = HANDLED();
            break;
        case EVENT( Disconnect ):
            ret = TRANSITION(this, STATE(AwaitingConnection));
            break;
        case EVENT( Heartbeat ):
            Heartbeat();
            ret = HANDLED();
            break;
        case EVENT( Tick ):
            ret = HANDLED();
            break;
        default:
            break;
    }

    return ret;
    
}

state_ret_t State_Idle( state_t * this, event_t s )
{
    STATE_DEBUG( s );
    state_ret_t ret = NO_PARENT(this);
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
                    ret = TRANSITION(this, STATE(AwaitingConnection) );
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
                    ret = TRANSITION(this, STATE(AwaitingConnection) );
                }
            }
            break;
        case EVENT( Disconnect ):
            ret = TRANSITION(this, STATE(AwaitingConnection));
            break;
        case EVENT( Heartbeat ):
            Heartbeat();
            ret = HANDLED();
            break;
        default:
            break;
    }

    return ret;
}

extern void Daemon_RefreshEvents( daemon_fifo_t * events )
{
    /*
    for( int idx = 0; idx < NUM_COMMS_EVENTS; idx++ )
    {
        if( comms_callback[idx].event_fn(comms) )
        {
            DaemonEvents_Enqueue( events, Daemon_GetState(), comms_callback[idx].event );
        }
    }
*/
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

static bool Send(uint8_t * buffer, uint16_t len)
{
    return Comms_Send(comms, buffer, len);
}

static bool Recv(uint8_t * buffer, uint16_t len)
{
    return Comms_Recv(comms, buffer, len);
}

extern state_t * const Daemon_GetState(void)
{
    return &state_machine.state;
}

extern void Daemon_Init(daemon_settings_t * settings, comms_t * tcp_comms, daemon_fifo_t * fifo)
{
    printf("!---------------------------!\n");
    printf("!   Initialising Daemon     !\n");
    printf("!---------------------------!\n");

    comms = tcp_comms;
    event_fifo = fifo;

    mqtt_t mqtt_config = {
        .client_name = settings->client_name,
        .send = Send,
        .recv = Recv,
        .subs = subs,
        .num_subs = NUM_SUBS,
    };
    mqtt = mqtt_config;
    MQTT_Init(&mqtt);
   
    state_machine.state.state = STATE(AwaitingConnection);
    state_machine.retry_count = 0U;

    DaemonEvents_Enqueue(event_fifo, &state_machine.state, EVENT(Enter));

    printf("!---------------------------!\n");
    printf("!   Complete!               !\n");
    printf("!---------------------------!\n");
}

extern mqtt_t * const Daemon_GetMQTT(void)
{
    return &mqtt;
}

extern char * Daemon_GetName(void)
{
    return client_name;
}

