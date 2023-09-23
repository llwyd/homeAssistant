#ifndef I2C_H_
#define I2C_H_

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include <string.h>
#include <stdio.h>

void I2C_Init(void);
int8_t I2C_ReadReg(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
int8_t I2C_ReadRegQuiet(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
int8_t I2C_WriteReg(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);

#endif /* I2C_H_ */
