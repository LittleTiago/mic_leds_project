#include "disk.h"

void eeprom_read2buffer(void* buffer, const void* offset, size_t len){
    eeprom_read_block(buffer, offset, len);   
}

uint8_t eeprom_read_id(uint16_t address){
    return(eeprom_read_byte((uint8_t*)address));
}
