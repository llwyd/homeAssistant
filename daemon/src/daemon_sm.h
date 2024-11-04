#ifndef DAEMON_SM_H_
#define DAEMON_SM_H_

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
    char * broker_ip;
    char * broker_port;
    char * client_name;
}
daemon_settings_t;

extern void Daemon_Init(daemon_settings_t * settings, comms_t * tcp_comms, daemon_fifo_t * fifo);

extern state_t * const Daemon_GetState(void);
extern char * Daemon_GetName(void);
extern mqtt_t * const Daemon_GetMQTT(void);
extern void Daemon_RefreshEvents( daemon_fifo_t * events );

#endif /* DAEMON_SM_H_ */
