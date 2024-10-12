#include "watchdog.h"

#define TIMEOUT_MS (6000U)

extern void Watchdog_Init(void)
{
    printf("Watchdog Init\n");
    watchdog_enable(TIMEOUT_MS, 1);
}

extern void Watchdog_Kick(void)
{
    watchdog_update();
}

