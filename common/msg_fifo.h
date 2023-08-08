#ifndef MSG_FIFO_H_
#define MSG_FIFO_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <state.h>
#include <string.h>
#include <fifo_base.h>

#define MSG_FIFO_LEN (32U)
#define MSG_SIZE (128U)

typedef struct
{
    fifo_base_t base;
    char queue[MSG_FIFO_LEN][MSG_SIZE];
    char * data;
    int len;
} msg_fifo_t;

extern void Message_Init(msg_fifo_t * fifo);

#endif /* MSG_FIFO_H_ */

