#ifndef _MQTT_H_
#define _MQTT_H_

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

typedef struct mqtt_subs_t
{
    char * name;
    mqtt_type_t format;
    void (*sub_fn)( mqtt_data_t* );
} mqtt_subs_t;

extern bool MQTT_Receive( void );
extern bool MQTT_EncodeAndPublish( char * name, mqtt_type_t format, void * data );
extern void MQTT_Init( char * ip, char * name, int *mqtt_sock, mqtt_subs_t * subscriptions, uint8_t number_subs );
extern bool MQTT_Connect( void );
extern bool MQTT_Subscribe( void );
bool MQTT_AllSubscribed( void );

#endif /* _MQTT_H_ */
