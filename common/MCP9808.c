#include "MCP9808.h"
#include "i2c.h"

static double current_temp = 0.0f;
static const uint8_t address = 0x18;

static void CalculateTemperature( uint8_t * data )
{
    uint8_t upper_byte = data[0];
    uint8_t lower_byte = data[1];
    double temperature = 0.0f;

    upper_byte = upper_byte & 0x1F;

    if( ( upper_byte & 0x10 ) == 0x10 )
    {
        upper_byte = upper_byte & 0xF;
        temperature = (double)256 - ( (double)upper_byte * 16.f ) + ( (double)lower_byte / 16.f );
    }
    else
    {
        temperature = ( (double)upper_byte * 16.f ) + ( (double)lower_byte / 16.f );
    }
    current_temp = temperature;
}

extern void MCP9808_Setup(void)
{
    printf("\tMCP9808 Init OK\n");
}

extern void MCP9808_Read(void)
{
    uint8_t data[2] = { 0x00 };
    uint8_t reg = 0x05;
    (void)I2C_ReadReg(reg, data, 2U, (void*)&address);
    CalculateTemperature(data);
}

extern double MCP9808_GetTemperature(void)
{
    return current_temp;
}

