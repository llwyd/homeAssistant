#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/critical_section.h"
#include "hardware/gpio.h"

#include "daemon.h"
#include "gpio.h"
#include "config.h"

int main()
{
    stdio_init_all();
    GPIO_Init();
    
    if(gpio_get(CONFIG_PIN))
    { 
        Daemon_Run();
    }
    else
    {
        Config_Run();
    }

    assert(false);
    return 0;
}
