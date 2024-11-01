#ifndef COMMS_SM_H_
#define COMMS_SM_H_

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
#include "daemon_events.h"
#include "state.h"
#include "mqtt.h"
#include "sensor.h"
#include "timestamp.h"
#include "events.h"
#include "timer.h"
#include "comms.h"
#include "msg_fifo.h"
#include "meta.h"

typedef struct
{
    char * ip;
    char * port;
    msg_fifo_t * msg_fifo;
}
comms_settings_t;

typedef struct
{
    state_t * (*get_state)(void);
    mqtt_t * (*get_mqtt)(void);
}
comms_client_t;

extern void CommsSM_Init(comms_settings_t * settings, daemon_fifo_t * fifo);

extern state_t * const CommsSM_GetState(void);
extern void CommsSM_RefreshEvents( daemon_fifo_t * events );

#endif /* COMMS_SM_H_ */
