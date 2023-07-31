#ifndef COMMS_H_
#define COMMS_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "secret_keys.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

extern bool Comms_Init(void);
extern bool Comms_CheckStatus(void);
extern void Comms_MQTTConnect(void);
extern bool Comms_Send( uint8_t * buffer, uint16_t len );
extern bool Comms_Recv( uint8_t * buffer, uint16_t len );

#endif /* COMMS_H_ */
