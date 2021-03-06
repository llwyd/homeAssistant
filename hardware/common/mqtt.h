#ifndef MQTT_H
#define MQTT_H


#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "task.h"

typedef enum mqtt_type_t
{
    mqtt_type_float,
    mqtt_type_int16,
    mqtt_type_str,
    mqtt_type_bool,
} mqtt_type_t;

typedef union
{
    float f;
    uint16_t i;
    char * s;
    bool b;
} mqtt_data_t;

typedef enum mqtt_state_t
{
    mqtt_state_Connect,
    mqtt_state_Subscribe,
    mqtt_state_Idle,
    mqtt_state_Publish,
    mqtt_state_Disconnect,
    mqtt_state_Ping, 
} mqtt_state_t;

typedef enum mqtt_msg_type_t
{
    mqtt_msg_Connect,
    mqtt_msg_Subscribe,
    mqtt_msg_Publish,
    mqtt_msg_Ping,
    mqtt_msg_Disconnect,
} mqtt_msg_type_t;

typedef struct mqtt_func_t
{
    mqtt_state_t state;
    mqtt_state_t (*mqtt_fn)(void);
} mqtt_func_t;

typedef struct mqtt_pairs_t
{
    mqtt_msg_type_t msg_type;
    uint8_t send_code;
    uint8_t recv_code;
    bool (*ack_fn)(uint8_t * buff, uint8_t len, uint16_t id);
    const uint8_t * name;
    const uint8_t * ack_name;
} mqtt_pairs_t;

typedef struct mqtt_subs_t
{
    uint8_t * name;
    mqtt_type_t format;
    void (*sub_fn)( mqtt_data_t* );
} mqtt_subs_t;

typedef struct mqtt_pub_t
{
    uint8_t * name;
    mqtt_type_t format;
    mqtt_data_t data;
} mqtt_pub_t;

/* Utility functions */
uint16_t MQTT_Format( mqtt_msg_type_t msg_type, void * msg_data, uint16_t * id );
mqtt_data_t MQTT_Extract( uint8_t * data, mqtt_type_t type );
bool MQTT_Transmit( mqtt_msg_type_t msg_type, void * msg_data );
void MQTT_Init(uint8_t * ip, uint8_t * port, uint8_t * host_name, uint8_t * root_topic, mqtt_subs_t * subs, uint8_t num_subs);

mqtt_state_t MQTT_Connect( void );
mqtt_state_t MQTT_Subscribe( void );
mqtt_state_t MQTT_Idle( void );
mqtt_state_t MQTT_Publish( void );
mqtt_state_t MQTT_Disconnect( void );
mqtt_state_t MQTT_Ping( void );

/* main event loop */
void MQTT_Loop( void );


#endif /* MQTT_H */
