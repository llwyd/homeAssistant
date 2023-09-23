#ifndef ENVIRONMENT_H_
#define ENVIRONMENT_H_


#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"

extern void Enviro_Init(void);
extern void Enviro_Read(void);
extern void Enviro_Print(void);
extern void Enviro_GenerateJSON(char * buffer, uint8_t buffer_len);

#endif /* ENVIRONMENT_H_ */

