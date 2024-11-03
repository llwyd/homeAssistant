#ifndef DAEMON_EVENTS_H_
#define DAEMON_EVENTS_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <state.h>
#include <string.h>
#include <fifo_base.h>
#include "event_observer.h"

#define FIFO_LEN (32U)

#define SIGNALS(SIG ) \
    SIG( Tick ) \
    SIG( Heartbeat ) \
    SIG( UpdateHomepage ) \
    SIG( TCPConnected ) \
    SIG( TCPDisconnected ) \
    SIG( MQTTMessageReceived ) \
    SIG( BrokerConnected ) \
    SIG( BrokerDisconnected ) \
    SIG( MessageReceived ) \
    SIG( Disconnect ) \

GENERATE_SIGNALS( SIGNALS );
GENERATE_SIGNAL_STRINGS( SIGNALS );

typedef struct
{
    char * name;
    bool (*event_fn)(void);
    state_t * (*get_state)(void);
    event_t event;
}
event_cb_t;

typedef struct
{
    fifo_base_t base;
    state_event_t queue[FIFO_LEN];
    state_event_t in;
    state_event_t out;
    event_observer_t * observer;
} daemon_fifo_t;

extern void DaemonEvents_Init(daemon_fifo_t * fifo);
extern void DaemonEvents_Enqueue(daemon_fifo_t * fifo, state_t * state, event_t event);
extern void DaemonEvents_BroadcastEvent(daemon_fifo_t * fifo, event_t event);
extern void DaemonEvents_Subscribe(daemon_fifo_t * fifo, state_t * state, event_t event);

#endif /* DAEMON_EVENTS_H_ */

