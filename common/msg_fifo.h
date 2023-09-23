#ifndef MSG_FIFO_H_
#define MSG_FIFO_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <state.h>
#include <string.h>
#include <fifo_base.h>
#include "pico/critical_section.h"
#include "pico/cyw43_arch.h"

#define MSG_FIFO_LEN (32U)
#define MSG_SIZE (128U)

typedef struct
{
    char * data;
    uint32_t len;
}
msg_t;

typedef struct
{
    fifo_base_t base;
    char queue[MSG_FIFO_LEN][MSG_SIZE];
    msg_t in;
    msg_t out;
} msg_fifo_t;

extern void Message_Init(msg_fifo_t * fifo, critical_section_t * crit);
extern char * Message_Get(void);

#endif /* MSG_FIFO_H_ */

