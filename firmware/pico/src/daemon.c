#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/critical_section.h"
#include "pico/unique_id.h"

#include "daemon.h"
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
#include "i2c.h"
#include "accelerometer.h"
#include "udp.h"
#include "ntp.h"
#include "gpio.h"
#include "alarm.h"
#include "eeprom.h"
#include "meta.h"
#include "watchdog.h"

#define RETRY_ATTEMPTS (5U)
#define RETRY_PERIOD_MS (1000)
#define SENSOR_PERIOD_MS (200)

#define ID_STRING_SIZE ( 32U )
GENERATE_SIGNAL_STRINGS( SIGNALS );

/* Top level state */
DEFINE_STATE(SetupWIFI);
DEFINE_STATE(ConfigureRTC);
DEFINE_STATE(Setup);
DEFINE_STATE(Root);

/* Second level */
DEFINE_STATE(WifiConnected);
DEFINE_STATE(WifiNotConnected);
DEFINE_STATE(DNSRequest);
DEFINE_STATE(RequestNTP);
DEFINE_STATE(Idle);
DEFINE_STATE(AwaitingAck);

/* Third level */
DEFINE_STATE(TCPNotConnected);
DEFINE_STATE(MQTTNotConnected);
DEFINE_STATE(MQTTSubscribing);

/* Inherit from state_t */
typedef struct
{
    state_t state;
    uint32_t retry_counter;
    struct repeating_timer * timer;
    struct repeating_timer * read_timer;
    struct repeating_timer * retry_timer;
    mqtt_t * mqtt;
    ntp_t * ntp;
    msg_fifo_t * msg_fifo;
    msg_fifo_t * udp_fifo;
    critical_section_t * crit;
}
node_state_t;

static void SubscribeCallback(mqtt_data_t * data);
static void HashRequest(mqtt_data_t * data);

static mqtt_subs_t subs[3] = 
{
    {"req_hash", mqtt_type_str, HashRequest},
    {"test_sub1", mqtt_type_bool, SubscribeCallback},
    {"test_sub2", mqtt_type_bool, SubscribeCallback},
};


static void SubscribeCallback(mqtt_data_t * data)
{
    (void)data;
    printf("\tSubscribe Callback Test, Data: %s\n", data->s);
}

static void HashRequest(mqtt_data_t * data)
{
    (void)data;
    printf("\tRequesting GIT hash: %s\n", data->s);
    Emitter_EmitEvent(EVENT(HashRequest));
}

static state_ret_t State_Setup( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = NO_PARENT(this);
    node_state_t * node_state = (node_state_t *)this;
    (void)node_state;
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

static state_ret_t State_SetupWIFI( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = NO_PARENT(this);
    node_state_t * node_state = (node_state_t *)this;
    (void)node_state;
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


static state_ret_t State_WifiConnected( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this,STATE(Setup));
    node_state_t * node_state = (node_state_t *)this;
    (void)node_state;
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
    state_ret_t ret = PARENT(this, STATE(SetupWIFI));
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
                Emitter_Destroy(node_state->retry_timer);
                //ret = TRANSITION(this, STATE(TCPNotConnected));
                // Get NTP upon wifi connection 
                ret = TRANSITION(this, STATE(DNSRequest));
            }
            else
            {
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
    state_ret_t ret = PARENT(this, STATE(WifiConnected));
    node_state_t * node_state = (node_state_t *)this;
    switch(s)
    {
        case EVENT(RetryCounterIncrement):
        {
            Emitter_Destroy(node_state->retry_timer);
            node_state->retry_counter++;
            if(node_state->retry_counter < RETRY_ATTEMPTS)
            {
                Emitter_Create(EVENT(RetryCounterIncrement), node_state->retry_timer, RETRY_PERIOD_MS);
            }
            else
            {
                Emitter_Create(EVENT(TCPRetryConnect), node_state->retry_timer, RETRY_PERIOD_MS);
                Emitter_EmitEvent(EVENT(TCPDisconnected));
            }
            ret = HANDLED();
            break;
        }
        case EVENT( TCPRetryConnect ):
        case EVENT( Enter ):
        {
            ret = HANDLED();
            Emitter_Destroy(node_state->retry_timer);
            node_state->retry_counter = 0U;
            if(WIFI_CheckTCPStatus())
            {
                if(Comms_TCPInit())
                {
                    Emitter_Create(EVENT(RetryCounterIncrement), node_state->retry_timer, RETRY_PERIOD_MS);
                }
                else
                {
                    Emitter_Create(EVENT(RetryCounterIncrement), node_state->retry_timer, RETRY_PERIOD_MS);
                }
            }
            else
            {
                    ret = TRANSITION(this, STATE(WifiNotConnected));
            }
            break;
        }
        case EVENT( TCPConnected ):
        {
            Emitter_Destroy(node_state->retry_timer);
            FIFO_Flush( &node_state->msg_fifo->base );
            ret = TRANSITION(this, STATE(MQTTNotConnected));
            break;
        }
        case EVENT( PCBInvalid ):
        case EVENT( TCPDisconnected ):
        {
            Comms_Close();
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

static state_ret_t State_MQTTNotConnected( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, STATE(WifiConnected));
    node_state_t * node_state = (node_state_t *)this;
    switch(s)
    {
        case EVENT( Exit ):
        {
            ret = HANDLED();
            break;
        }
        case EVENT( PCBInvalid ):
        case EVENT( TCPDisconnected ):
        {
            Comms_Close();
            ret = TRANSITION(this, STATE(TCPNotConnected));
            break;
        }
        case EVENT(RetryCounterIncrement):
        {
            Emitter_Destroy(node_state->retry_timer);
            node_state->retry_counter++;
            if(node_state->retry_counter < RETRY_ATTEMPTS)
            {
                Emitter_Create(EVENT(RetryCounterIncrement), node_state->retry_timer, RETRY_PERIOD_MS);
            }
            else
            {
                Emitter_Create(EVENT(MQTTRetryConnect), node_state->retry_timer, RETRY_PERIOD_MS);
            }
            ret = HANDLED();
            break;
        }
        case EVENT( MQTTRetryConnect ):
        case EVENT( Enter ):
        {
            Emitter_Destroy(node_state->retry_timer);
            node_state->retry_counter = 0U;
            if(MQTT_Connect(node_state->mqtt))
            {
                ret = HANDLED();
            }
            else
            {
                Emitter_Create(EVENT(RetryCounterIncrement), node_state->retry_timer, RETRY_PERIOD_MS);
                ret = HANDLED();
                //ret = TRANSITION(this, STATE(TCPNotConnected));
            }
            break;
        }
        case EVENT( MessageReceived ):
        {
            /* Presumably the buffer has a message... */
            assert( !FIFO_IsEmpty( &node_state->msg_fifo->base ) );
            msg_t msg = FIFO_Dequeue(node_state->msg_fifo);
            if(MQTT_HandleMessage(node_state->mqtt, (uint8_t*)msg.data))
            {
                ret = TRANSITION(this, STATE(MQTTSubscribing));
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
    state_ret_t ret = PARENT(this, STATE(WifiConnected));
    node_state_t * node_state = (node_state_t *)this;
    switch(s)
    {
        case EVENT( MessageReceived ):
        {
            assert( !FIFO_IsEmpty( &node_state->msg_fifo->base ) );
            msg_t msg = FIFO_Dequeue(node_state->msg_fifo);
            if(MQTT_HandleMessage(node_state->mqtt, (uint8_t*)msg.data))
            {
                if(MQTT_AllSubscribed(node_state->mqtt))
                {
                    ret = TRANSITION(this, STATE(Idle));
                }
                else
                {
                    ret = HANDLED();
                }
            }
            else
            {
                ret = TRANSITION(this, STATE(MQTTSubscribing));
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
            if(MQTT_Subscribe(node_state->mqtt))
            {
                ret = HANDLED();
            }
            else
            {
                ret = TRANSITION(this, STATE(TCPNotConnected));
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

static state_ret_t State_ConfigureRTC( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = NO_PARENT(this);
    node_state_t * node_state = (node_state_t *)this;
    (void)node_state;
    switch(s)
    {
        case EVENT( Tick ):
        {
            ret = HANDLED();
            break;
        }
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

static state_ret_t State_DNSRequest( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, STATE(ConfigureRTC));
    node_state_t * node_state = (node_state_t *)this;
    switch(s)
    {
        case EVENT(RetryCounterIncrement):
        {
            Emitter_Destroy(node_state->retry_timer);
            node_state->retry_counter++;
            if(node_state->retry_counter < RETRY_ATTEMPTS)
            {
                Emitter_Create(EVENT(RetryCounterIncrement), node_state->retry_timer, RETRY_PERIOD_MS);
            }
            else
            {
                Emitter_Create(EVENT(DNSRetryRequest), node_state->retry_timer, RETRY_PERIOD_MS);
            }
            ret = HANDLED();
            break;
        }
        case EVENT( DNSRetryRequest ):
        case EVENT( Enter ):
        {
            ret = HANDLED();
            Emitter_Destroy(node_state->retry_timer);
            node_state->retry_counter = 0U;
            NTP_RequestDNS(node_state->ntp);
            if(WIFI_CheckStatus())
            {
                Emitter_Create(EVENT(RetryCounterIncrement), node_state->retry_timer, RETRY_PERIOD_MS);
            }
            else
            {
                /* Possible WIFI may have failed at this point, re-connect */
                ret = TRANSITION(this, STATE(WifiNotConnected));
            }
            break;
        }
        case EVENT( DNSReceived ):
        {
            Emitter_Destroy(node_state->retry_timer);
            NTP_PrintIP(node_state->ntp);
            ret = TRANSITION(this, STATE(RequestNTP));
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

static state_ret_t State_RequestNTP( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, STATE(ConfigureRTC));
    node_state_t * node_state = (node_state_t *)this;
    switch(s)
    {
        case EVENT(RetryCounterIncrement):
        {
            ret = HANDLED();
            Emitter_Destroy(node_state->retry_timer);
            node_state->retry_counter++;
            if(node_state->retry_counter < RETRY_ATTEMPTS)
            {
                Emitter_Create(EVENT(RetryCounterIncrement), node_state->retry_timer, RETRY_PERIOD_MS);
            }
            else
            {
                ret = TRANSITION(this, STATE(DNSRequest));
                //Emitter_Create(EVENT(NTPRetryRequest), node_state->retry_timer, RETRY_PERIOD_MS);
            }
            break;
        }
        case EVENT( NTPRetryRequest ):
        case EVENT( Enter ):
        {
            ret = HANDLED();
            Emitter_Destroy(node_state->retry_timer);
            NTP_Get(node_state->ntp);
            node_state->retry_counter = 0U;
            if(WIFI_CheckStatus())
            {
                Emitter_Create(EVENT(RetryCounterIncrement), node_state->retry_timer, RETRY_PERIOD_MS);
            }
            else
            {
                /* Possible WIFI may have failed at this point, re-connect */
                ret = TRANSITION(this, STATE(WifiNotConnected));
            }
            break;
        }
        case EVENT( Exit ):
        {
            ret = HANDLED();
            break;
        }
        case EVENT(NTPReceived):
        {
            Emitter_Destroy(node_state->retry_timer);
            assert( !FIFO_IsEmpty( &node_state->udp_fifo->base ) );
            msg_t msg = FIFO_Dequeue(node_state->udp_fifo);
            NTP_Decode((uint8_t*)msg.data);
            ret = TRANSITION(this, STATE(TCPNotConnected));
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
            Emitter_Create(EVENT(ReadSensor), node_state->read_timer, SENSOR_PERIOD_MS);
            WIFI_SetLed();
            Alarm_Start();
            Accelerometer_Ack();
            Accelerometer_Start();
            ret = HANDLED();
            break;
        }
        case EVENT( Exit ):
        {
            /* Should never try and leave here! */
            WIFI_ClearLed();
            Alarm_Stop();
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
    state_ret_t ret = PARENT(this, STATE(Root));
    node_state_t * node_state = (node_state_t *)this;

    switch(s)
    {
        case EVENT( Exit ):
        case EVENT( Enter ):
        {
            ret = HANDLED();
            break;
        }
        case EVENT( AccelMotion ):
        {
            Accelerometer_Ack();
            bool success = MQTT_Publish(node_state->mqtt,"motion","1");
            if(success)
            {
                ret = TRANSITION(this, STATE(AwaitingAck));
            }
            else
            {
                Comms_Close();
                ret = TRANSITION(this, STATE(TCPNotConnected));
            }
            break;
        }
        case EVENT( HashRequest ):
        {
            bool success = MQTT_Publish(node_state->mqtt,"hash", META_GITHASH);
            if(success)
            {
                ret = TRANSITION(this, STATE(AwaitingAck));
            }
            else
            {
                Comms_Close();
                ret = TRANSITION(this, STATE(TCPNotConnected));
            }
            break;
        }
        case EVENT( PCBInvalid ):
        case EVENT( TCPDisconnected ):
        {
            Comms_Close();
            ret = TRANSITION(this, STATE(TCPNotConnected));
            break;
        }
        case EVENT( ReadSensor ):
        {
            Enviro_Read();
            Enviro_Print();

            char json[64];
            Enviro_GenerateJSON(json, 64);
            bool success = MQTT_Publish(node_state->mqtt,"environment", json);
            if(success)
            {
                ret = TRANSITION(this, STATE(AwaitingAck));
            }
            else
            {
                Comms_Close();
                ret = TRANSITION(this, STATE(TCPNotConnected));
            }
            break;
        }
        case EVENT( MessageReceived ):
        {
            /* Presumably the buffer has a message... */
            assert( !FIFO_IsEmpty( &node_state->msg_fifo->base ) );
            msg_t msg = FIFO_Dequeue(node_state->msg_fifo);
            (void)MQTT_HandleMessage(node_state->mqtt, (uint8_t*)msg.data); 
            ret = HANDLED();
            break;
        }
        case EVENT( AlarmElapsed ):
        {
            char json[64];
            Enviro_GenerateJSON(json, 64);
            MQTT_Publish(node_state->mqtt,"summary", json);
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

static state_ret_t State_AwaitingAck( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, STATE(Root));
    node_state_t * node_state = (node_state_t *)this;
    switch(s)
    {
        case EVENT( ReadSensor ):
        case EVENT( Exit ):
        {
            ret = HANDLED();
            break;
        }
        case EVENT( Enter ):
        {
            Emitter_Destroy(node_state->retry_timer);
            node_state->retry_counter = 0U;
            Emitter_Create(EVENT(RetryCounterIncrement), node_state->retry_timer, RETRY_PERIOD_MS);
            ret = HANDLED();
            break;
        }
        case EVENT(RetryCounterIncrement):
        {
            Emitter_Destroy(node_state->retry_timer);
            node_state->retry_counter++;
            if(node_state->retry_counter < RETRY_ATTEMPTS)
            {
                Emitter_Create(EVENT(RetryCounterIncrement), node_state->retry_timer, RETRY_PERIOD_MS);
                ret = HANDLED();
            }
            else
            {
                Comms_Abort();
                ret = TRANSITION(this, STATE(TCPNotConnected));
            }
            break;
        }
        case EVENT( AckReceived ):
        {
            Emitter_Destroy(node_state->retry_timer);
            ret = TRANSITION(this, STATE(Idle));
            break;
        }
        case EVENT( PCBInvalid ):
        case EVENT( TCPDisconnected ):
        {
            Emitter_Destroy(node_state->retry_timer);
            Comms_Close();
            ret = TRANSITION(this, STATE(TCPNotConnected));
            break;
        }
        default:
        {
            break;
        }
    }

    return ret;
}

extern void Daemon_Run(void)
{
    char unique_id[ID_STRING_SIZE]={0};
    pico_get_unique_board_id_string(unique_id, ID_STRING_SIZE);

    event_fifo_t events;
    critical_section_t crit;
    msg_fifo_t msg_fifo;
    msg_fifo_t udp_fifo;
    mqtt_t mqtt =
    {
        .client_name = unique_id,
        .send = Comms_Send,
        .recv = Comms_Recv,
        .subs = subs,
        .num_subs = 3U,
    };

    ntp_t ntp =
    {
        .send = UDP_Send,
    };

    struct repeating_timer timer;
    struct repeating_timer read_timer;
    struct repeating_timer retry_timer;
    
    critical_section_t crit_events;
    critical_section_t crit_tcp;
    critical_section_t crit_udp;
    critical_section_t crit_msg_fifo;
    critical_section_t crit_udp_fifo;
   
    /* Initialise various sub modules */ 
    critical_section_init(&crit);
    critical_section_init_with_lock_num(&crit_events, 0U);
    critical_section_init_with_lock_num(&crit_tcp, 1U);
    critical_section_init_with_lock_num(&crit_udp, 2U);
    critical_section_init_with_lock_num(&crit_msg_fifo, 3U);
    critical_section_init_with_lock_num(&crit_udp_fifo, 4U);
    
    Watchdog_Init();
    I2C_Init();
    Alarm_Init();
    Enviro_Init();
    Accelerometer_Init();
    Events_Init(&events);
    EEPROM_Read((uint8_t*)unique_id, EEPROM_ENTRY_SIZE, EEPROM_NAME);
    
    Watchdog_Kick();
    Message_Init(&msg_fifo, &crit_msg_fifo);
    Message_Init(&udp_fifo, &crit_udp_fifo);
    Comms_Init(&msg_fifo, &crit_tcp);
    UDP_Init(&udp_fifo, &crit_udp);
    MQTT_Init(&mqtt);
    NTP_Init(&ntp);
    Emitter_Init(&events, &crit_events);
    WIFI_Init();

    node_state_t state_machine; 
    state_machine.retry_counter = 0U;
    state_machine.timer = &timer;
    state_machine.read_timer = &read_timer;
    state_machine.retry_timer = &retry_timer;
    state_machine.mqtt = &mqtt;
    state_machine.msg_fifo = &msg_fifo;
    state_machine.udp_fifo = &udp_fifo;
    state_machine.ntp = &ntp;
    state_machine.crit = &crit;

    Watchdog_Kick();
    STATEMACHINE_Init( &state_machine.state, STATE( WifiNotConnected ) );

    while( true )
    {
        while( FIFO_IsEmpty( (fifo_base_t *)&events ) )
        {
            tight_loop_contents();
        }
        critical_section_enter_blocking(&crit_events);
        event_t e = FIFO_Dequeue( &events );
        critical_section_exit(&crit_events);
        STATEMACHINE_Dispatch(&state_machine.state, e);
        //cyw43_arch_poll();
        Watchdog_Kick();
    }

    /* Shouldn't get here! */
    assert(false);
}

