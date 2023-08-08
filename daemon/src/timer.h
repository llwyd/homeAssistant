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

extern void Timer_Init(void);

extern bool Timer_Tick500ms(void);
extern bool Timer_Tick1s(void);
extern bool Timer_Tick60s(void);

#endif /* TIMER_H_ */
