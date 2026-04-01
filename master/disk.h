#ifndef EEPROM_H
#define EEPROM_H

#include <avr/eeprom.h>

void eeprom_read2buffer(void* buffer, const void* offset, size_t len);
uint8_t eeprom_read_id(uint16_t address);

#endif