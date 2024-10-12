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
#include "pico/critical_section.h"

#include "msg_fifo.h"

extern void Comms_Init(msg_fifo_t * fifo, critical_section_t * crit);
extern bool Comms_TCPInit(void);
extern bool Comms_CheckStatus(void);
extern void Comms_MQTTConnect(void);
extern bool Comms_Send( uint8_t * buffer, uint16_t len );
extern bool Comms_Recv( uint8_t * buffer, uint16_t len );
extern void Comms_Close(void);
extern void Comms_Abort(void);

#endif /* COMMS_H_ */
