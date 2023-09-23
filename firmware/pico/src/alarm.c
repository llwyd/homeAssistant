#include "alarm.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/rtc.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "node_events.h"
#include "emitter.h"

#define NUM_MINUTES (12U)

static datetime_t dt;
static datetime_t alarm;

uint8_t pos = 0U;
int8_t minutes[NUM_MINUTES] = {0,5,10,15,20,25,30,35,40,45,50,55};

static void Callback(void)
{
    Emitter_EmitEvent(EVENT(AlarmElapsed));
    pos++;
    if( pos >= NUM_MINUTES )
    {
        pos = 0;
    }
    alarm.min = minutes[pos];
    rtc_set_alarm(&alarm, Callback);
}

extern void Alarm_Init(void)
{
    printf("\tInitialising Alarm\n");
    rtc_init();
    memset(&dt, 0x00, sizeof(datetime_t));
}

extern void Alarm_SetClock(time_t * unixtime)
{
    printf("\tSetting Clock to %s\n", ctime(unixtime));
    struct tm * time = gmtime(unixtime);

    dt.year = time->tm_year + 1900U;
    dt.month = time->tm_mon;
    dt.day = time->tm_mon;
    dt.dotw = time->tm_wday;
    dt.hour = time->tm_hour;
    dt.min = time->tm_min;
    dt.sec = time->tm_sec;

    rtc_set_datetime(&dt);

    alarm.year = -1;
    alarm.month = -1;
    alarm.day = -1;
    alarm.dotw = -1;
    alarm.hour = -1;
    alarm.min = 0;
    alarm.sec = 0;
}

extern void Alarm_Start(void)
{
    printf("\tStarting Alarm\n");
    rtc_get_datetime(&dt);
    
    pos = (dt.min / 5) + 1;
    if( pos >= NUM_MINUTES )
    {
        pos = 0;
    }

    alarm.min = minutes[pos];
    rtc_set_alarm(&alarm, Callback);
}

extern void Alarm_Stop(void)
{
    printf("\tStopping Alarm\n");
    rtc_disable_alarm();
}

