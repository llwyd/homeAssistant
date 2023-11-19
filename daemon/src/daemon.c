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

static char * broker_ip;
static char * broker_port;
static char * client_name;
static event_fifo_t events;
static msg_fifo_t msg_fifo;
static mqtt_t mqtt;

void Daemon_OnBoardLED( mqtt_data_t * data );
void Heartbeat( void );

#define NUM_SUBS ( 1U )
static mqtt_subs_t subs[NUM_SUBS] = 
{
    {"debug_led", mqtt_type_bool, Daemon_OnBoardLED},
};

#define NUM_EVENTS (5)
static event_callback_t event_callback[NUM_EVENTS] =
{
    {"Tick", Timer_Tick1s, EVENT(Tick)},
    {"MQTT Message Received", Comms_MessageReceived, EVENT(MessageReceived)},
    {"Heartbeat Led", Timer_Tick500ms, EVENT(Heartbeat)},
    {"Homepage Update", Timer_Tick60s, EVENT(UpdateHomepage)},
    {"TCP Disconnect", Comms_Disconnected, EVENT(Disconnect)},
};

state_ret_t State_Connect( state_t * this, event_t s )
{
    TimeStamp_Print();
    STATE_DEBUG( s );
    state_ret_t ret;
    switch( s )
    {
        case EVENT( Tick ):         
        case EVENT( Enter ):
            if( Comms_Connect() )
            {
                printf("\tTCP Connection successful\n");
                ret = TRANSITION(this, STATE(MQTTConnect));
            }
            else
            {
                ret = HANDLED();
            }
            break;
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
    switch( s )
    {
        case EVENT( Tick ):         
        case EVENT( Enter ):
            if(MQTT_Connect(&mqtt))
            {
                ret = HANDLED();
            }
            else
            {
                ret = TRANSITION(this, STATE(Connect));
            }
            break;
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

void RefreshEvents( event_fifo_t * events )
{
    for( int idx = 0; idx < NUM_EVENTS; idx++ )
    {
        if( event_callback[idx].event_fn() )
        {
            FIFO_Enqueue( events, event_callback[idx].event );
        }
    }
}

static void Loop( void )
{
    state_t daemon; 
    daemon.state = State_Connect; 
    event_t sig = EVENT( None );

    FIFO_Enqueue( &events, EVENT( Enter ) );

    while( 1 )
    {
        /* Get Event */    
        while( FIFO_IsEmpty( (fifo_base_t *)&events ) )
        {
            RefreshEvents( &events );
        }

        sig = FIFO_Dequeue( &events );

        /* Dispatch */
        STATEMACHINE_FlatDispatch( &daemon, sig );
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

bool Init( int argc, char ** argv )
{
    bool success = false;
    bool ip_found = false;
    bool name_found = false;
    int input_flags;

    while( ( input_flags = getopt( argc, argv, "b:c:" ) ) != -1 )
    {
        switch( input_flags )
        {
            case 'b':
                broker_ip = optarg;
                broker_port = "1883";
                ip_found = true;
                break;
            case 'c':
                client_name = optarg;
                name_found = true;
                break;
            default:
                break;
        }
    } 
   
    success = ip_found & name_found;

    if( success )
    {
        Message_Init(&msg_fifo);
        Comms_Init(broker_ip, broker_port, &msg_fifo);
        mqtt_t mqtt_config = {
            .client_name = client_name,
            .send = Comms_Send,
            .recv = Comms_Recv,
            .subs = subs,
            .num_subs = NUM_SUBS,
        };
        mqtt = mqtt_config;
        MQTT_Init(&mqtt);
    }

    return success;
}

int main( int argc, char ** argv )
{
    printf("Git Hash: %s\n", META_GITHASH);
    printf("Build Time: %s %s\n", META_DATESTAMP, META_TIMESTAMP);
    (void)TimeStamp_Generate();
    Timer_Init();
    Events_Init(&events);
    bool success = Init( argc, argv );

    if( success )
    {
        Loop();
    }
    else
    {
        printf("[CRITICAL ERROR!] FAILED TO INITIALISE\n");
    }
    return 0U;
}

