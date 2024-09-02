#ifndef WATCHDOG_H_
#define WATCHDOG_H_

#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

extern void Watchdog_Init(void);
extern void Watchdog_Kick(void);

#endif /* WATCHDOG_H_ */

