#ifndef ALARM_H_
#define ALARM_H_

#include <time.h>
extern void Alarm_Init(void);
extern void Alarm_SetClock(time_t * unixtime);
extern void Alarm_Start(void);
extern void Alarm_Stop(void);

#endif /* ALARM_H_ */
