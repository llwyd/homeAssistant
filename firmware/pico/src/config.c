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
#include "cli.h"

DEFINE_STATE(Config);
DEFINE_STATE(AwaitingCommand);
DEFINE_STATE(ReadRaw);
DEFINE_STATE(SetValue);
DEFINE_STATE(ReadAll);

typedef struct
{
    state_t state;
    uint8_t * buffer;
    uint32_t command_idx;
}
config_state_t;

#define NUM_COMMANDS (6U)
const cli_command_t command_table[NUM_COMMANDS] =
{
    { "set ssid", STATE(SetValue), "SSID", EEPROM_SSID},
    { "set pass", STATE(SetValue), "PASSWORD", EEPROM_PASS},
    { "set broker", STATE(SetValue), "BROKER", EEPROM_IP},
    { "set name", STATE(SetValue), "NAME", EEPROM_NAME},
    { "read all", STATE(ReadAll), "ALL", EEPROM_NONE },
    { "read raw", STATE(ReadRaw), "RAW", EEPROM_NONE},
};

_Static_assert(NUM_COMMANDS == (sizeof(command_table)/sizeof(cli_command_t)));

static state_ret_t State_Config( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = NO_PARENT(this);
    config_state_t * config_state = (config_state_t *)this;
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

static state_ret_t State_AwaitingCommand( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, STATE(Config));
    config_state_t * config_state = (config_state_t *)this;
    switch(s)
    {
        case EVENT( Enter ):
        {
            CLI_Start();
            ret = HANDLED();
            break;
        }
        case EVENT( Exit ):
        {
            ret = HANDLED();
            break;
        }
        case EVENT( HandleCommand ):
        {
            CLI_Stop();
            bool valid_command = false;
            uint32_t idx;
            for( idx = 0; idx < NUM_COMMANDS; idx++)
            {
                if(strncmp(command_table[idx].command, config_state->buffer, CLI_CMD_SIZE) == 0U)
                {
                    valid_command = true;
                    config_state->command_idx = idx;
                    break;
                } 
            }

            if(valid_command)
            {
                ret = TRANSITION(this, command_table[idx].state);
            }
            else
            {
                printf("Invalid Command\n");
                CLI_Start();
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

static state_ret_t State_ReadRaw( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, STATE(Config));
    config_state_t * config_state = (config_state_t *)this;
    switch(s)
    {
        case EVENT( Enter ):
        {
            uint32_t eeprom_size = EEPROM_GetSize();
            uint8_t raw_buffer[16];
            for(uint32_t idx = 0; idx < eeprom_size; idx += 16U)
            {
                EEPROM_ReadRaw(raw_buffer, 16U, idx);                
                for(uint32_t jdx = 0; jdx < 16U; jdx++)
                {
                    printf("%x ", raw_buffer[jdx]);
                }
                printf("\n");
            }
            ret = TRANSITION(this, STATE(AwaitingCommand));
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

static state_ret_t State_ReadAll( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, STATE(Config));
    config_state_t * config_state = (config_state_t *)this;
    switch(s)
    {
        case EVENT( Enter ):
        {
            uint32_t eeprom_size = EEPROM_GetSize();
            uint8_t raw_buffer[CLI_CMD_SIZE];
            
            (void)EEPROM_Read(raw_buffer, CLI_CMD_SIZE, EEPROM_SSID);
            printf("SSID: %s\n", raw_buffer);

            (void)EEPROM_Read(raw_buffer, CLI_CMD_SIZE, EEPROM_PASS);
            printf("PASS: %s\n", raw_buffer);
            
            (void)EEPROM_Read(raw_buffer, CLI_CMD_SIZE, EEPROM_IP);
            printf("Broker IP: %s\n", raw_buffer);
            
            (void)EEPROM_Read(raw_buffer, CLI_CMD_SIZE, EEPROM_NAME);
            printf("NAME: %s\n", raw_buffer);

            ret = TRANSITION(this, STATE(AwaitingCommand));
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

static state_ret_t State_SetValue( state_t * this, event_t s )
{
    STATE_DEBUG(s);
    state_ret_t ret = PARENT(this, STATE(Config));
    config_state_t * config_state = (config_state_t *)this;
    switch(s)
    {
        case EVENT( Enter ):
        {
            assert(config_state->command_idx < NUM_COMMANDS);
            uint16_t cmd_idx = config_state->command_idx;
            printf("Enter %s:\n", command_table[cmd_idx].name);
            CLI_Start();
            ret = HANDLED();
            break;
        }
        case EVENT( Exit ):
        {
            ret = HANDLED();
            break;
        }
        case EVENT( HandleCommand ):
        {
            CLI_Stop();
            assert(config_state->command_idx < NUM_COMMANDS);
            uint16_t cmd_idx = config_state->command_idx;
            uint16_t str_len = strnlen(config_state->buffer, CLI_CMD_SIZE - 1U);
            str_len++;
            EEPROM_Write(config_state->buffer, str_len, command_table[cmd_idx].label);
            ret = TRANSITION(this, STATE(AwaitingCommand));
        }
        default:
        {
            break;
        }
    }
    return ret;
}

extern void Config_Run(void)
{
    uint8_t command_buffer[CLI_CMD_SIZE];
    event_fifo_t events;
    critical_section_t crit;
    critical_section_t crit_events;
    critical_section_init_with_lock_num(&crit_events, 0U);
    
    config_state_t state_machine;
    state_machine.buffer = command_buffer;

    I2C_Init();
    CLI_Init(state_machine.buffer);
    Events_Init(&events);
    Emitter_Init(&events, &crit_events);
    STATEMACHINE_Init( &state_machine.state, STATE( AwaitingCommand ) );

    while(true)
    {
        while( FIFO_IsEmpty( (fifo_base_t *)&events ) )
        {
            tight_loop_contents();
        }
        critical_section_enter_blocking(&crit_events);
        event_t e = FIFO_Dequeue( &events );
        critical_section_exit(&crit_events);
        STATEMACHINE_Dispatch(&state_machine.state, e);
    }
    
    /* Shouldn't get here! */
    assert(false);
}
