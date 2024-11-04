#ifndef TIMER_H_
#define TIMER_H_

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
#include "daemon_events.h"

typedef struct
{
    time_t last_tick_s;
    struct timespec last_tick_ms;
}
daemon_timer_t;

typedef struct
{
    char * name;
    daemon_timer_t * timer;
    bool (*event_fn)(daemon_timer_t * timer);
    event_t event;
}
timer_callback_t;

extern void Timer_Init(daemon_timer_t * const timer);

extern bool Timer_Tick500ms(daemon_timer_t * const timer);
extern bool Timer_Tick1s(daemon_timer_t * const timer);
extern bool Timer_Tick300s(daemon_timer_t * const timer);

extern uint32_t Timer_TimeSinceStartMS(void);

#endif /* TIMER_H_ */
