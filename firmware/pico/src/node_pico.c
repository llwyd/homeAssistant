#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/critical_section.h"

#include "environment.h"
#include "events.h"
#include "fifo_base.h"
#include "state.h"
#include "wifi.h"
#include "emitter.h"
#include "comms.h"
#include "mqtt.h"
#include "msg_fifo.h"
#include "node_events.h"

#define RETRY_PERIOD_MS (1500)


DEFINE_STATE( Idle );

/* Top level state */
DEFINE_STATE(Setup);
DEFINE_STATE(Root);

/* Second level */
DEFINE_STATE(WifiConnected);
DEFINE_STATE(WifiNotConnected);

/* Third level */
DEFINE_STATE(TCPNotConnected);
DEFINE_STATE(MQTTNotConnected);
DEFINE_STATE(MQTTSubscribing);
DEFINE_STATE(Idle);

/* Inherit from state_t */
typedef struct
{
    state_t state;
    struct repeating_timer * timer;
    struct repeating_timer * read_timer;
    struct repeating_timer * retry_timer;
    mqtt_t * mqtt;
    msg_fifo_t * msg_fifo;
}
node_state_t;

static void SubscribeCallback(mqtt_data_t * data);

static mqtt_subs_t subs[3] = 
{
    {"test_sub1", mqtt_type_bool, SubscribeCallback},
    {"test_sub2", mqtt_type_bool, SubscribeCallback},
    {"test_sub3", mqtt_type_bool, SubscribeCallback},
};

static state_ret_t State_Setup( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = NO_PARENT(this);
    node_state_t * node_state = (node_state_t *)this;
    switch(s)
    {
        case EVENT( Tick ):
        {
            WIFI_ToggleLed();
            ret = HANDLED();
            break;
        }
        case EVENT( Enter ):
        {
//            Emitter_Create(EVENT(Tick), node_state->timer, 250);
            ret = HANDLED();
            break;
        }
        case EVENT( Exit ):
        {
            /* Should never try and leave here! */
            //assert(false);
            ret = HANDLED();
            break;
        }
        default:
        {
            break;
        }
    }
    return ret;
}


static state_ret_t State_WifiConnected( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this,Setup);
    node_state_t * node_state = (node_state_t *)this;
    switch(s)
    {
        case EVENT( Enter ):
        {
            ret = HANDLED();
            break;
        }
        case EVENT( Exit ):
        {
            ret = HANDLED();
            break;
        }
        default:
        {
            break;
        }
    }
   return ret; 
}

static state_ret_t State_WifiNotConnected( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, Setup);
    node_state_t * node_state = (node_state_t *)this;

    switch(s)
    {
        case EVENT( Enter ):
        {
            WIFI_TryConnect();
            Emitter_Create(EVENT(WifiCheckStatus), node_state->retry_timer, RETRY_PERIOD_MS);
            ret = HANDLED();
            break;
        }
        case EVENT( WifiCheckStatus ):
        {
            if( WIFI_CheckStatus() )
            {
                printf("\tWifi Connected! :)\n");
                Emitter_Destroy(node_state->retry_timer);
                ret = TRANSITION(this, TCPNotConnected);
            }
            else
            {
                printf("\tWifi Disconnected! :(\n");
                WIFI_TryConnect();
                Emitter_Destroy(node_state->retry_timer);
                Emitter_Create(EVENT(WifiCheckStatus), node_state->retry_timer, RETRY_PERIOD_MS);
                ret = HANDLED();
            }
            break;
        }
        case EVENT( Exit ):
        {
            ret = HANDLED();
            break;
        }
        default:
        {
            break;
        }
    }

    return ret;
}

static state_ret_t State_TCPNotConnected( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, WifiConnected);
    node_state_t * node_state = (node_state_t *)this;
    switch(s)
    {
        case EVENT( TCPRetryConnect ):
        case EVENT( Enter ):
        {
            ret = HANDLED();
            Emitter_Destroy(node_state->retry_timer);
            if(Comms_TCPInit())
            {
                Emitter_Create(EVENT(TCPRetryConnect), node_state->retry_timer, RETRY_PERIOD_MS);
            }
            else
            {
                if(WIFI_CheckStatus())
                {
                    Emitter_Create(EVENT(TCPRetryConnect), node_state->retry_timer, RETRY_PERIOD_MS);
                }
                else
                {
                    /* Possible WIFI may have failed at this point, re-connect */
                    printf("\tWifi failed, reconnect\n");
                    ret = TRANSITION(this, WifiNotConnected);
                }
            }
            break;
        }
        case EVENT( TCPConnected ):
        {
            Emitter_Destroy(node_state->retry_timer);
            ret = TRANSITION(this, MQTTNotConnected);
            break;
        }
        case EVENT( Exit ):
        {
            ret = HANDLED();
            break;
        }
        default:
        {
            break;
        }
    }
    return ret;
}

static state_ret_t State_MQTTNotConnected( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, WifiConnected);
    node_state_t * node_state = (node_state_t *)this;
    switch(s)
    {
        case EVENT( Exit ):
        {
            ret = HANDLED();
            break;
        }
        case EVENT( TCPDisconnected ):
        {
            ret = TRANSITION(this, TCPNotConnected);
            break;
        }
        case EVENT( MQTTRetryConnect ):
        case EVENT( Enter ):
        {
            Emitter_Destroy(node_state->retry_timer);
            MQTT_Connect(node_state->mqtt);
            ret = HANDLED();
            break;
        }
        case EVENT( MessageReceived ):
        {
            /* Presumably the buffer has a message... */
            assert( !FIFO_IsEmpty( &node_state->msg_fifo->base ) );
            char * msg = FIFO_Dequeue(node_state->msg_fifo);
            if(MQTT_HandleMessage(node_state->mqtt, msg))
            {
                ret = TRANSITION(this, MQTTSubscribing);
            }
            else
            {
                Emitter_Create(EVENT(MQTTRetryConnect), node_state->retry_timer, RETRY_PERIOD_MS);
                ret = HANDLED();
            }
            break;
        }
        default:
        {
            break;
        }
    }
    
    return ret;
}

static state_ret_t State_MQTTSubscribing( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, WifiConnected);
    node_state_t * node_state = (node_state_t *)this;
    switch(s)
    {
        case EVENT( MessageReceived ):
        {
            assert( !FIFO_IsEmpty( &node_state->msg_fifo->base ) );
            char * msg = FIFO_Dequeue(node_state->msg_fifo);
            if(MQTT_HandleMessage(node_state->mqtt, msg))
            {
                if(MQTT_AllSubscribed(node_state->mqtt))
                {
                    ret = TRANSITION(this, Idle);
                }
                else
                {
                    ret = HANDLED();
                }
            }
            else
            {
                /* This doesn't work, need to fix */
                ret = TRANSITION(this, MQTTNotConnected);
                assert(false);
            }
            break;
        }
        case EVENT( Exit ):
        {
            ret = HANDLED();
            break;
        }
        case EVENT( Enter ):
        {
            Emitter_Destroy(node_state->retry_timer);
            MQTT_Subscribe(node_state->mqtt);
            ret = HANDLED();
            break;
        }
        default:
        {
            break;
        }
    }
    
    return ret;
}

static state_ret_t State_Root( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = NO_PARENT(this);
    node_state_t * node_state = (node_state_t *)this;
    switch(s)
    {
        case EVENT( Enter ):
        {
            Emitter_Create(EVENT(ReadSensor), node_state->read_timer, 1000);
            WIFI_SetLed();
            ret = HANDLED();
            break;
        }
        case EVENT( Exit ):
        {
            /* Should never try and leave here! */
            Emitter_Destroy(node_state->read_timer);
            ret = HANDLED();
            break;
        }
        default:
        {
            break;
        }
    }
    return ret;
}
static state_ret_t State_Idle( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, Root);
    node_state_t * node_state = (node_state_t *)this;

    switch(s)
    {
        case EVENT( Exit ):
        case EVENT( Enter ):
        {
            ret = HANDLED();
            break;
        }
        case EVENT( TCPDisconnected ):
        {
            ret = TRANSITION(this, TCPNotConnected);
            break;
        }
        case EVENT( ReadSensor ):
        {
            Enviro_Read();
            Enviro_Print();
            char sensor_str[8];
            Enviro_ConvertToStr(sensor_str, 8U, Enviro_GetTemperature());
            MQTT_Publish(node_state->mqtt,"temperature_live", sensor_str);
            
            Enviro_ConvertToStr(sensor_str, 8U, Enviro_GetHumidity());
            MQTT_Publish(node_state->mqtt,"humidity_live", sensor_str);
            ret = HANDLED();
            break;
        }
        case EVENT( MessageReceived ):
        {
            /* Presumably the buffer has a message... */
            assert( !FIFO_IsEmpty( &node_state->msg_fifo->base ) );
            char * msg = FIFO_Dequeue(node_state->msg_fifo);
            (void)MQTT_HandleMessage(node_state->mqtt, msg); 
            ret = HANDLED();
            break;
        }
        default:
        {
            break;
        }
    }

    return ret;
}

static void SubscribeCallback(mqtt_data_t * data)
{
    (void)data;
    printf("\tSubscribe Callback Test, Data: %s\n", data->s);
}

int main()
{
    char * client_name = "pico";
    event_fifo_t events;
    critical_section_t crit;
    msg_fifo_t msg_fifo;
    mqtt_t mqtt =
    {
        .client_name = client_name,
        .send = Comms_Send,
        .recv = Comms_Recv,
        .subs = subs,
        .num_subs = 3U,
    };

    struct repeating_timer timer;
    struct repeating_timer read_timer;
    struct repeating_timer retry_timer;
   
    /* Initialise various sub modules */ 
    stdio_init_all();
    critical_section_init(&crit);
    Enviro_Init(); 
    Events_Init(&events);
    
    Message_Init(&msg_fifo);
    Comms_Init(&msg_fifo);
    MQTT_Init(&mqtt);
    Emitter_Init(&events, &crit);
    (void)WIFI_Init();

    node_state_t state_machine; 
    state_machine.timer = &timer;
    state_machine.read_timer = &read_timer;
    state_machine.retry_timer = &retry_timer;
    state_machine.mqtt = &mqtt;
    state_machine.msg_fifo = &msg_fifo;

    STATEMACHINE_Init( &state_machine.state, STATE( WifiNotConnected ) );

    while( true )
    {
        while( FIFO_IsEmpty( (fifo_base_t *)&events ) )
        {
            tight_loop_contents();
        }
        event_t e = FIFO_Dequeue( &events );
        STATEMACHINE_Dispatch(&state_machine.state, e);
    }

    /* Shouldn't get here! */
    assert(false);
    return 0;
}
