#ifndef MCP9808_H_
#define MCP9808_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

extern void MCP9808_Setup(void);
extern void MCP9808_Read(void);
extern double MCP9808_GetTemperature(void);

#endif /* MCP9808_H_ */ 
