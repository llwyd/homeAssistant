#ifndef WIFI_H_
#define WIFI_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/critical_section.h"

extern void WIFI_Init(void);
extern bool WIFI_CheckStatus(void);
extern bool WIFI_CheckTCPStatus(void);
extern void WIFI_TryConnect(void);
extern void WIFI_SetLed(void);
extern void WIFI_ClearLed(void);
extern void WIFI_ToggleLed(void);

#endif /* WIFI_H_ */
