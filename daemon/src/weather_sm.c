#include "weather_sm.h"
#include "daemon_events.h"

DEFINE_STATE(AwaitingConnection);
DEFINE_STATE(Idle);

typedef struct
{
    state_t state;
    uint32_t retry_count;
}
weather_state_t;


static weather_state_t state_machine;
static daemon_fifo_t * event_fifo;

static char * client_name = "outside";
static mqtt_t mqtt;

static comms_t * comms;

static bool Send(uint8_t * buffer, uint16_t len);
static bool Recv(uint8_t * buffer, uint16_t len);

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
        case EVENT( UpdateHomepage ):
        {
            /*
            char * json = Sensor_GenerateJSON();
            if( MQTT_Publish(&mqtt, "summary", json))
            {
                ret = HANDLED();
            }
            else
            {
                ret = TRANSITION(this, STATE(AwaitingConnection) );
            }
            */
            ret = HANDLED();
            break;
        }
        case EVENT( Disconnect ):
            ret = TRANSITION(this, STATE(AwaitingConnection));
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

extern void WeatherSM_Init(weather_settings_t * settings, comms_t * tcp_comms, daemon_fifo_t * fifo)
{
    printf("!---------------------------!\n");
    printf("!   Initialising Weather    !\n");
    printf("!---------------------------!\n");

    comms = tcp_comms;
    event_fifo = fifo;

    mqtt_t mqtt_config = {
        .client_name = client_name,
        .send = Send,
        .recv = Recv,
        .subs = NULL,
        .num_subs = 0U,
    };
    mqtt = mqtt_config;
   
    state_machine.state.state = STATE(AwaitingConnection);
    state_machine.retry_count = 0U;

    DaemonEvents_Enqueue(event_fifo, &state_machine.state, EVENT(Enter));

    printf("!---------------------------!\n");
    printf("!   Complete!               !\n");
    printf("!---------------------------!\n");
}

extern state_t * const WeatherSM_GetState(void)
{
    return &state_machine.state;
}

extern char * WeatherSM_GetName(void)
{
    return client_name;
}

