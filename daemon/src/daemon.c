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

#define NUM_SUBS ( 1U )

#define SIGNALS(SIG ) \
    SIG( Tick ) \
    SIG( MessageReceived ) \
    SIG( Heartbeat ) \
    SIG( UpdateHomepage ) \

GENERATE_SIGNALS( SIGNALS );
GENERATE_SIGNAL_STRINGS( SIGNALS );

DEFINE_STATE(Connect);
DEFINE_STATE(Subscribe);
DEFINE_STATE(Idle);

static char * broker_ip;
static char * client_name;
static event_fifo_t events;

void Daemon_OnBoardLED( mqtt_data_t * data );
void Heartbeat( void );

static mqtt_subs_t subs[NUM_SUBS] = 
{
    {"debug_led", mqtt_type_bool, Daemon_OnBoardLED},
};

#define NUM_EVENTS (4)
static event_callback_t event_callback[NUM_EVENTS] =
{
    {"Tick", Timer_Tick1s, EVENT(Tick)},
    {"MQTT Message Received", MQTT_MessageReceived, EVENT(MessageReceived)},
    {"Heartbeat Led", Timer_Tick500ms, EVENT(Heartbeat)},
    {"Homepage Update", Timer_Tick60s, EVENT(UpdateHomepage)},
};

state_ret_t State_Connect( state_t * this, event_t s )
{
    TimeStamp_Print();
    STATE_DEBUG( s );
    state_ret_t ret;
    switch( s )
    {
        case EVENT( Enter ):
        case EVENT( Tick ):
            
            if( MQTT_Connect() )
            {
                MQTT_CreatePoll();
            }
            else
            {
            }
            ret = HANDLED();
            break;
        case EVENT( MessageReceived ):
            
            if( MQTT_Receive() )
            {
                ret = TRANSITION(this, Subscribe );
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
            if( MQTT_Subscribe() )
            {
                ret = HANDLED();
            }
            else
            {
                ret = TRANSITION(this, Connect );
            }
            break;
        case EVENT( Exit ):
            ret = HANDLED();
            break;
        case EVENT( MessageReceived ):
            if( MQTT_Receive() )
            {
                if( MQTT_AllSubscribed() )
                {
                    ret = TRANSITION(this, Idle );
                }
                else
                {
                    ret = HANDLED();
                }
            }
            else
            {
                ret = TRANSITION(this, Connect );
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
                if( MQTT_EncodeAndPublish("environment", mqtt_type_str, json ) )
                {
                    ret = HANDLED();
                }
                else
                {
                    ret = TRANSITION(this, Connect );
                }
            }
            break;
        case EVENT( UpdateHomepage ):
            {
                float temperature = Sensor_GetTemperature();
                if( MQTT_EncodeAndPublish("temperature", mqtt_type_float, &temperature ) )
                {
                    ret = HANDLED();
                }
                else
                {
                    ret = TRANSITION(this, Connect );
                }
            }
            break;
        case EVENT( MessageReceived ):
            
            if( MQTT_Receive() )
            {
                ret = HANDLED();
            }
            else
            {
                ret = TRANSITION(this, Connect );
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
        MQTT_Init( broker_ip, client_name, subs, NUM_SUBS );
    }

    return success;
}

int main( int argc, char ** argv )
{
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

