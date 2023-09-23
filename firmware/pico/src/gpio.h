#ifndef GPIO_H_
#define GPIO_H_

#include "node_events.h"
#include "emitter.h"
#include "hardware/gpio.h"
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"

#define CONFIG_PIN (13U)
#define INT1_PIN (14U)
#define INT2_PIN (15U)
#define SDA_PIN (16U)
#define SCL_PIN (17U)
#define WRITE_PIN ( 18U )

extern void GPIO_Init(void);

#endif /* GPIO_H_ */
