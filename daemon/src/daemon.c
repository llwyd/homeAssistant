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

#define NUM_SUBS ( 1U )

#define SIGNALS(SIG ) \
    SIG( Tick ) \
    SIG( MessageReceived ) \
    SIG( Heartbeat ) \
    SIG( UpdateHomepage ) \

GENERATE_SIGNALS( SIGNALS );
GENERATE_SIGNAL_STRINGS( SIGNALS );

static int mqtt_sock;
static struct pollfd mqtt_poll;
static char * broker_ip;
static char * client_name;

void Daemon_OnBoardLED( mqtt_data_t * data );
void Heartbeat( void );

static mqtt_subs_t subs[NUM_SUBS] = 
{
    {"debug_led", mqtt_type_bool, Daemon_OnBoardLED},
};

DEFINE_STATE(Connect);
DEFINE_STATE(Subscribe);
DEFINE_STATE(Idle);

#define FIFO_LEN (32U)

typedef struct
{
    fifo_base_t base;
    event_t queue[FIFO_LEN];
    event_t data;
} event_fifo_t;

static void Enqueue( fifo_base_t * const fifo );
static void Dequeue( fifo_base_t * const fifo );
static void Flush( fifo_base_t * const fifo );

static void Init( event_fifo_t * fifo )
{
    static const fifo_vfunc_t vfunc =
    {
        .enq = Enqueue,
        .deq = Dequeue,
        .flush = Flush,
    };
    FIFO_Init( (fifo_base_t *)fifo, FIFO_LEN );
    
    fifo->base.vfunc = &vfunc;
    fifo->data = 0x0;
    memset(fifo->queue, 0x00, FIFO_LEN * sizeof(fifo->data));
}

void Enqueue( fifo_base_t * const base )
{
    assert(base != NULL );
    ENQUEUE_BOILERPLATE( event_fifo_t, base );
}

void Dequeue( fifo_base_t * const base )
{
    assert(base != NULL );
    DEQUEUE_BOILERPLATE( event_fifo_t, base );
}

void Flush( fifo_base_t * const base )
{
    assert(base != NULL );
    FLUSH_BOILERPLATE( event_fifo_t, base );
}

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
                mqtt_poll.fd = mqtt_sock;
                mqtt_poll.events = POLLIN;
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
                float temperature = Sensor_GetTemperature();
                if( MQTT_EncodeAndPublish("temp_live", mqtt_type_float, &temperature ) )
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
                if( MQTT_EncodeAndPublish("node_temp", mqtt_type_float, &temperature ) )
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

    /* 500ms onboard blink */
    static struct timespec current_nano_tick;
    static struct timespec last_nano_tick;

    timespec_get( &current_nano_tick, TIME_UTC );
    if( (unsigned long)( current_nano_tick.tv_nsec - last_nano_tick.tv_nsec ) > 500000000UL )
    {
        last_nano_tick = current_nano_tick;
        FIFO_Enqueue( events, EVENT( Heartbeat ) );
    }


    /* Check for MQTT/Comms events */
    int rv = poll( &mqtt_poll, 1, 1 );

    if( rv & POLLIN )
    {
        FIFO_Enqueue( events, EVENT( MessageReceived ) );
    }

    /* Check whether Tick has Elapsed */
    static time_t current_time;

    static time_t last_tick;
    static time_t last_homepage_tick;
    
    const double period = 1;
    const double homepage_period = 60;

    time( &current_time );

    double delta = difftime( current_time, last_tick );
    if( delta > period )
    {   
        FIFO_Enqueue( events, EVENT( Tick ) );
        last_tick = current_time;
    }

    delta = difftime( current_time, last_homepage_tick );
    if( delta > homepage_period )
    {
        FIFO_Enqueue( events, EVENT( UpdateHomepage ) );
        last_homepage_tick = current_time;
    }

}

static void Loop( void )
{
    state_t daemon; 
    daemon.state = State_Connect; 
    event_t sig = EVENT( None );
    event_fifo_t events;

    Init(&events);
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
    int led_fd = open("/sys/class/leds/led0/brightness", O_WRONLY );
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

bool InitDaemon( int argc, char ** argv )
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
        MQTT_Init( broker_ip, client_name, &mqtt_sock, subs, NUM_SUBS );
    }

    return success;
}

int main( int argc, char ** argv )
{
    (void)TimeStamp_Generate();
    bool success = InitDaemon( argc, argv );

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

