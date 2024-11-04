#ifndef WEATHER_H_
#define WEATHER_H_

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
#include "comms.h"
#include "msg_fifo.h"
#include "meta.h"
#include "json.h"
#include "daemon_events.h"

typedef struct
{
    char * api_key;
    char * location;
}
weather_settings_t;

extern void WeatherSM_Init(weather_settings_t * settings, comms_t * tcp_comms, daemon_fifo_t * fifo);
extern state_t * const WeatherSM_GetState(void);
extern char * WeatherSM_GetName(void);

#endif /* WEATHER_H */
