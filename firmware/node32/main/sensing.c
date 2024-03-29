#include "sensing.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "sdkconfig.h"

#include "stdio.h"

#include "bme280.h"
#include "dataqueue.h"

/* i2c config */
static int i2c_master_port = 0U;

static int taskDelay = 1000;

static void  BME280_Setup( void );

static struct   bme280_dev dev;
static uint8_t  bme280_addr = BME280_I2C_ADDR_PRIM;
static struct   bme280_data bme280_rawData;

static void Init( void )
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

    BME280_Setup();
}


static void AddDataToQueue( QueueHandle_t * q, sensing_type_t type, sensing_data_t data, char * label )
{    
    sensing_t queueData;

    queueData.type          = type;
    queueData.data.f        = data.f;
    queueData.mqtt_label    = label;

    xQueueSend( *q, ( void *)&queueData, (TickType_t) 0U);
}

extern void Sensing_Task( void * pvParameters )
{
    TickType_t xLastWaitTime = xTaskGetTickCount();
    TickType_t xSlowWaitTime = xTaskGetTickCount();
    TickType_t xCurrentTime  = xSlowWaitTime;

    QueueHandle_t * queue = (QueueHandle_t *)pvParameters;

    int slowDelay = 1000 * 60 * 1;
    static sensing_t sensor_data;

    Init();

    while( 1U )
    {
        vTaskDelayUntil( &xLastWaitTime, taskDelay / portTICK_PERIOD_MS );
        
        bme280_get_sensor_data(BME280_ALL, &bme280_rawData, &dev);

        float temperature   = bme280_rawData.temperature;
        float humidity      = bme280_rawData.humidity;
        float pressure      = bme280_rawData.pressure;
        
        DQ_AddDataToQueue( queue, &temperature, dq_data_float, dq_desc_temperature, "temp_live");  
        DQ_AddDataToQueue( queue, &humidity, dq_data_float, dq_desc_temperature, "hum_live");  
        DQ_AddDataToQueue( queue, &pressure, dq_data_float, dq_desc_temperature, "pres_live");  

        xCurrentTime = xTaskGetTickCount();
        if( (xCurrentTime - xSlowWaitTime) > ( slowDelay / portTICK_PERIOD_MS ) )
        {
            DQ_AddDataToQueue( queue, &temperature, dq_data_float, dq_desc_temperature, "node_temp");  
            DQ_AddDataToQueue( queue, &humidity, dq_data_float, dq_desc_temperature, "node_hum");  
            
            xSlowWaitTime = xCurrentTime;
        }

        printf("BME280 T: %.2f\n", bme280_rawData.temperature);
        printf("BME280 H: %.2f\n", bme280_rawData.humidity);
        printf("BME280 P: %.2f\n", bme280_rawData.pressure);
    }
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

static void BME280_Configure( void )
{
    uint8_t settings_sel;
    int8_t rslt = BME280_OK;

    dev.settings.osr_h     = BME280_OVERSAMPLING_1X;
    dev.settings.osr_p     = BME280_OVERSAMPLING_16X;
    dev.settings.osr_t     = BME280_OVERSAMPLING_1X;
    dev.settings.filter    = BME280_FILTER_COEFF_2;
    dev.settings.standby_time = BME280_STANDBY_TIME_250_MS;

    settings_sel = BME280_OSR_PRESS_SEL;
    settings_sel |= BME280_OSR_TEMP_SEL;
    settings_sel |= BME280_OSR_HUM_SEL;
    settings_sel |= BME280_STANDBY_SEL;
    settings_sel |= BME280_FILTER_SEL;
    
    rslt = bme280_set_sensor_settings(settings_sel, &dev);
    rslt = bme280_set_sensor_mode(BME280_NORMAL_MODE, &dev);

    if( rslt != BME280_OK )
    {
        printf("BME280 Configure FAIL\n");
    }
    else 
    {
        printf("BME280 Configure OK\n");
    }
}

static void BME280_Setup( void )
{
    int8_t rslt = BME280_OK;

    dev.intf_ptr    = &bme280_addr;
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
        BME280_Configure();
    }
}

