#ifndef UDP_H_
#define UDP_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "pico/critical_section.h"

#include "msg_fifo.h"

extern void UDP_Recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
extern bool UDP_Send( uint8_t * buffer, uint16_t len, ip_addr_t ip, uint16_t port);
extern void UDP_Init(msg_fifo_t * fifo, critical_section_t * crit);

#endif /* UDP_H_ */

