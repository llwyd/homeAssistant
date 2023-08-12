#ifndef _SENSOR_H_
#define _SENSOR_H_

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

void Sensor_Init( void );
void Sensor_Read( void );
extern float Sensor_GetTemperature( void );
extern char * Sensor_GenerateJSON(void);

#endif /* _SENSOR_H_ */
