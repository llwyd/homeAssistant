#include "sensor.h"
#include "timer.h"
#define JSON_PACKET_SIZE (64U)

#ifdef __arm__
static const char * device = "/dev/i2c-1";
static const uint8_t address = 0x18;
#endif
static float current_temp = 0.0f;
static float current_hum = 0.0f;

static char json_packet[JSON_PACKET_SIZE];

extern float Sensor_GetTemperature( void )
{
    return current_temp;
}

extern char * Sensor_GenerateJSON(void)
{
    memset(json_packet, 0x00, JSON_PACKET_SIZE);

    uint32_t time_since_start = Timer_TimeSinceStartMS();
    snprintf(json_packet, JSON_PACKET_SIZE,
            "{\"temperature\":%.1f,\"humidity\":%.1f,\"uptime_ms\":%u}",
            current_temp,
            current_hum,
            time_since_start);
    return json_packet;
}

static void CalculateTemperature( uint8_t * data )
{
    uint8_t upper_byte = data[0];
    uint8_t lower_byte = data[1];
    float temperature = 0.0f;

    upper_byte = upper_byte & 0x1F;

    if( ( upper_byte & 0x10 ) == 0x10 )
    {
        upper_byte = upper_byte & 0xF;
        temperature = (float)256 - ( (float)upper_byte * 16.f ) + ( (float)lower_byte / 16.f );
    }
    else
    {
        temperature = ( (float)upper_byte * 16.f ) + ( (float)lower_byte / 16.f );
    }

    printf("\tTemperature = %.2f\n", temperature );

    current_temp = temperature;
}

void Sensor_Init( void )
{

}

void Sensor_Read( void )
{
    uint8_t data[2] = { 0x00 };
#ifdef __arm__
    int file = open( device, O_RDWR );
    uint8_t reg = 0x05;

    if( file < 0 )
    {
        printf("Error opening device\n");
    }

    /* Write address */
    if( ioctl( file, I2C_SLAVE, address&0xFE ) < 0 )
    {
        printf("Writing write address failed\n");
    }
    if( write( file, &reg, 1U ) != 1 )
    {
        printf("Error writing register\n");
    }

    if( read( file, data, 2 ) !=2 )
    {
        printf("Error reading register\n");
    }

    close( file );
#endif
    CalculateTemperature( data );
}
