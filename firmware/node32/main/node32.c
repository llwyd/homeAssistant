
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "mqtt_client.h"

#include "sensing.h"
#include "comms.h"
#include "types.h"

static QueueHandle_t xTemperatureQueue;
static QueueHandle_t xSlowTemperatureQueue;

static QueueHandle_t xSensorDataQueue;

void app_main(void)
{
    TaskHandle_t xSensingHandle = NULL;
    TaskHandle_t xCommsHandle   = NULL;
    TaskHandle_t xWeatherHandle = NULL;

    xTemperatureQueue       = xQueueCreate( 16U, sizeof( float ) );
    xSlowTemperatureQueue   = xQueueCreate( 16U, sizeof( float ) );
    xSensorDataQueue   = xQueueCreate( 16U, sizeof( sensing_t ) );

    Sensing_Init( &xTemperatureQueue, &xSlowTemperatureQueue );
    Comms_Init( &xTemperatureQueue, &xSlowTemperatureQueue );

    xTaskCreate( Sensing_Task,  "Sensing Task", 5000, &xSensorDataQueue, (tskIDLE_PRIORITY + 3), &xSensingHandle );
    xTaskCreate( Comms_Task,    "Comms Task",   5000, &xSensorDataQueue, (tskIDLE_PRIORITY + 2), &xCommsHandle );
    xTaskCreate( Comms_Weather, "Weather Task", 5000, (void *)1, (tskIDLE_PRIORITY + 1), &xWeatherHandle );
}
