#include "i2c.h"
#include "gpio.h"
#include <assert.h>
#include "hardware/gpio.h"
#include "eeprom.h"

static const uint32_t entry_size = 32U;
static const uint16_t eeprom_size = 256U;
static const uint8_t address = 0x50;

extern uint16_t EEPROM_GetSize(void)
{
    return eeprom_size;
}

static uint32_t GetIndex(eeprom_label_t label)
{
    assert(label < EEPROM_NONE);
    uint32_t raw_index = ((uint32_t)label) * entry_size;

    return raw_index;
}

extern void EEPROM_WriteRaw(uint8_t * buffer, uint16_t len, uint16_t loc)
{
    assert(buffer != NULL);
    assert((loc + len) <= eeprom_size);

    if( len <= 16 )
    {
        (void)I2C_WriteReg(loc, buffer, len, (void*)&address);
    }
    else
    {

    }
}

extern void EEPROM_ReadRaw(uint8_t * buffer, uint16_t len, uint16_t loc)
{
    assert(buffer != NULL);
    assert((loc + len) <= eeprom_size);
    memset(buffer, 0U, len);
    I2C_ReadReg(loc, buffer, len, (void*)&address);
}

extern bool EEPROM_Write(uint8_t * buffer, uint16_t len, eeprom_label_t loc)
{
    assert(buffer != NULL);
    assert(((uint16_t)loc + len) <= eeprom_size);
    assert(loc < EEPROM_NONE);
    
    uint32_t raw_index = GetIndex(loc);

    uint16_t bytes_written = 0;
    uint16_t remaining_len = len;
    uint8_t * write_ptr = buffer;

    while(bytes_written < len )
    {
        assert(remaining_len > 0);
        if( remaining_len <= 16U )
        {
            (void)I2C_WriteReg(raw_index, write_ptr, remaining_len, (void*)&address);
            raw_index += remaining_len;
            bytes_written += remaining_len;
            write_ptr += remaining_len;
            remaining_len -= remaining_len;
        }
        else
        {
            (void)I2C_WriteReg(raw_index, write_ptr, 16U, (void*)&address);
            raw_index += 16U;
            bytes_written += 16U;
            write_ptr += 16U;
            remaining_len -= 16U;
        }
        /* Wait for write to complete before next attempt */
        uint8_t unused_data;
        while( I2C_ReadRegQuiet(raw_index, &unused_data, 1U, (void*)&address) < 0);
    }

    assert(remaining_len == 0U);
    assert(bytes_written == len);

    return true;
}

extern bool EEPROM_Read(uint8_t * buffer, uint16_t len, eeprom_label_t loc)
{
    assert(buffer != NULL);
    assert(((uint16_t)loc + len) <= eeprom_size);
    assert(loc < EEPROM_NONE);
    
    uint32_t raw_index = GetIndex(loc);

    I2C_ReadReg(raw_index, buffer, len, (void*)&address);

    return true;
}

