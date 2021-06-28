#include "sensing.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "sdkconfig.h"

#include "stdio.h"

#include "bme280.h"

/* current temperature value */
static float temperature = 0.0f;


/* i2c config */
static int i2c_master_port = 0U;

static int taskDelay = 1000;

static float TMP102_Read( void );
static void  BME280_Setup( void );

static QueueHandle_t * xDataQueue;
static QueueHandle_t * xSlowDataQueue;


void Sensing_Init( QueueHandle_t * xTemperature, QueueHandle_t * xSlowTemperature )
{
    i2c_config_t conf =
    {
        .mode               = I2C_MODE_MASTER,
        .sda_io_num         = 23U,
        .sda_pullup_en      = GPIO_PULLUP_DISABLE,
        .scl_io_num         = 22U,
        .scl_pullup_en      = GPIO_PULLUP_DISABLE,
        .master.clk_speed   = 100000U,
    };

    i2c_param_config( i2c_master_port, &conf );
    i2c_driver_install( i2c_master_port, conf.mode, 0U, 0U, 0U );

    xDataQueue = xTemperature;
    xSlowDataQueue = xSlowTemperature;

    BME280_Setup();
}

extern void Sensing_Task( void * pvParameters )
{
    TickType_t xLastWaitTime = xTaskGetTickCount();
    TickType_t xSlowWaitTime = xTaskGetTickCount();
    TickType_t xCurrentTime  = xSlowWaitTime;

    int slowDelay = 1000 * 60 * 5;

    while( 1U )
    {
        vTaskDelayUntil( &xLastWaitTime, taskDelay / portTICK_PERIOD_MS );

        /* TODO - critical secion */
        temperature = TMP102_Read();

        xQueueSend( *xDataQueue, ( void *)&temperature, (TickType_t) 0U );

        xCurrentTime = xTaskGetTickCount();
        if( (xCurrentTime - xSlowWaitTime) > ( slowDelay / portTICK_PERIOD_MS ) )
        {
            xQueueSend( *xSlowDataQueue, ( void *)&temperature, (TickType_t) 0U );
            xSlowWaitTime = xCurrentTime;
        }

        printf("Temperature: %.3f\n", temperature);
    }
}

const float * Sensing_GetTemperature( void )
{
    return &temperature;
}

static float TMP102_Read( void )
{
    uint8_t data[2]             = {0U};
    const uint8_t address       = 0x48;
    const float scaling         = 0.0625f;

    float currentTemperature    = 0.0f;
    uint16_t rawTemperature     = 0x0U;


    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start( cmd );
    i2c_master_write_byte( cmd, ( address << 1) | 0x1, 0x1 );

    i2c_master_read(cmd, &data[0], 1, 0x0 );
    i2c_master_read(cmd, &data[1], 1, 0x1 );
    i2c_master_stop(cmd);
    
    i2c_master_cmd_begin( i2c_master_port, cmd, 100 / portTICK_PERIOD_MS );
    i2c_cmd_link_delete(cmd);

    rawTemperature = ( (( uint16_t )data[ 0 ] << 4) | data[ 1 ] >> 4 );
    currentTemperature = ( ( float )rawTemperature ) * scaling;

    return currentTemperature;
}

void BME280_Delay(uint32_t period, void *intf_ptr)
{
    ets_delay_us( period );
}

int8_t BME280_I2CRead(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
        int8_t rslt = 0;
        uint8_t address = *(uint8_t *)intf_ptr;

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start( cmd );
        i2c_master_write_byte( cmd, ( address << 1) & 0xFE, 0x1 );
        i2c_master_write_byte( cmd, reg_addr, 0x1 );

        i2c_master_start( cmd );
        i2c_master_write_byte( cmd, ( address << 1) | 0x1, 0x1 );

        if( len > 1 )
        {
            i2c_master_read( cmd, reg_data, len - 1, 0x0 );
        }

        i2c_master_read_byte( cmd, reg_data + len - 1, 0x1 );
        i2c_master_stop( cmd );

        esp_err_t ret = i2c_master_cmd_begin( i2c_master_port, cmd, 1000 / portTICK_PERIOD_MS );
        i2c_cmd_link_delete(cmd);

        if( ret != ESP_OK )
        {
            printf("BME280 READ FAIL\n");
            rslt = -1;
        }

        return rslt;
}


int8_t BME280_I2CWrite(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
        int8_t rslt = 0;
        uint8_t address = *(uint8_t *)intf_ptr;

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start( cmd );
        i2c_master_write_byte( cmd, ( address << 1) & 0xFE, 0x1 );
        i2c_master_write_byte( cmd, reg_addr, 0x1 );
        i2c_master_write( cmd, reg_data, len, 0x1 );
        i2c_master_stop( cmd );

        esp_err_t ret = i2c_master_cmd_begin( i2c_master_port, cmd, 1000 / portTICK_PERIOD_MS );
        i2c_cmd_link_delete(cmd);

        if( ret != ESP_OK )
        {
            printf("BME280 WRITE FAIL\n");
            rslt = -1;
        }
        return rslt;
}

static void BME280_Setup( void )
{
    struct bme280_dev dev;
    int8_t rslt = BME280_OK;
    uint8_t dev_addr = BME280_I2C_ADDR_PRIM;

    dev.intf_ptr    = &dev_addr;
    dev.intf        = BME280_I2C_INTF;
    dev.read        = BME280_I2CRead;
    dev.write       = BME280_I2CWrite;
    dev.delay_us    = BME280_Delay;

    rslt = bme280_init(&dev);
    if( rslt != BME280_OK )
    {
        printf("BME280 Init FAIL\n");
    }
    else
    {
        printf("BME280 Init OK\n");
    }
}

