#ifndef _WEATHER_H_
#define _WEATHER_H_

#include <stdio.h>
#include <stdlib.h>
#include "sensor.h"
#include <linux/i2c-dev.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

void Weather_Init( char * key, char * loc );
void Weather_Read( void );
extern float Weather_GetTemperature( void );
extern char * Weather_GenerateJSON(void);

#endif /* _WEATHER_H_ */
