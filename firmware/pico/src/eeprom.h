#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    EEPROM_SSID,
    EEPROM_PASS,
    EEPROM_IP,
    EEPROM_NAME,

    EEPROM_NONE,
}
eeprom_label_t;

#define EEPROM_ENTRY_SIZE (32U)

extern void EEPROM_WriteRaw(uint8_t * buffer, uint16_t len, uint16_t loc);
extern void EEPROM_ReadRaw(uint8_t * buffer, uint16_t len, uint16_t loc);

extern bool EEPROM_Write(uint8_t * buffer, uint16_t len, eeprom_label_t loc);
extern bool EEPROM_Read(uint8_t * buffer, uint16_t len, eeprom_label_t loc);

extern uint16_t EEPROM_GetSize(void);

#endif /* EEPROM_H_ */
