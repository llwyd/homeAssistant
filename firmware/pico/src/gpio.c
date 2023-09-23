#include "gpio.h"

static void IRQ_Callback(uint gpio, uint32_t events)
{
    switch(gpio)
    {
        case INT2_PIN:
            Emitter_EmitEvent(EVENT(AccelDataReady));
            break;
        case INT1_PIN:
            Emitter_EmitEvent(EVENT(AccelMotion));
            break;
        default:
            printf("GPIO: %d\n", gpio);
            assert(false);
            break;
    }
}

extern void GPIO_Init(void)
{
    printf("Initialising GPIO\n");

    /* Config Mode */
    gpio_init(CONFIG_PIN);
    gpio_set_dir(CONFIG_PIN, GPIO_IN);
    gpio_set_pulls(CONFIG_PIN,true,false);
    
    /* EEPROM Write protect */
    gpio_init(WRITE_PIN);
    gpio_set_dir(WRITE_PIN, GPIO_OUT);
    gpio_put(WRITE_PIN, false);

    /* Accelerometer */
    gpio_init(INT1_PIN);
    gpio_init(INT2_PIN);

    gpio_set_dir(INT1_PIN, GPIO_IN);
    gpio_set_pulls(INT1_PIN,true,false);
    gpio_set_dir(INT2_PIN, GPIO_IN);
    gpio_set_pulls(INT2_PIN,true,false);    
    
    gpio_set_irq_enabled_with_callback(INT1_PIN, GPIO_IRQ_EDGE_FALL, false, IRQ_Callback);
    gpio_set_irq_enabled(INT2_PIN, GPIO_IRQ_EDGE_FALL, false);

    /* I2C */
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    bi_decl(bi_2pins_with_func(SDA_PIN, SCL_PIN, GPIO_FUNC_I2C));
}
