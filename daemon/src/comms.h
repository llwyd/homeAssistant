#ifndef COMMS_H_
#define COMMS_H_

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/poll.h>
#include "msg_fifo.h"

typedef struct
{
    int sock;
    char * ip;
    char * port;
    bool connected;
    struct pollfd poll;
    msg_fifo_t * fifo;
}
comms_t;

extern char * Comms_GetLatestMessage(void);
void Comms_Init(const comms_t * const comms);
bool Comms_Connect(comms_t * const comms);
void Comms_Close(comms_t * comms);

extern bool Comms_MessageReceived(comms_t * const comms);
extern bool Comms_Disconnected(comms_t * const comms);

bool Comms_Send(comms_t * const comms, uint8_t * buffer, uint16_t len);
bool Comms_Recv(comms_t * const comms, uint8_t * buffer, uint16_t len);

bool Comms_RecvToFifo(comms_t * const comms);

#endif /* COMMS_H_ */
