#include <Arduino.h>

const int EEPROM_PAGE_SIZE = 128;						//bytes : page size in eeprom
const int EEPROM_MAX_WRITE = 32;						//bytes : maximum data to write in one page write cycle (limitation of Wire library)
const int EEPROM_PAGE_DELAY = 5;						//ms : time to wait between each sequential page write cycle.

void WriteEEPROM_Page(uint8_t deviceAddress, unsigned int startAddress, byte* data, int length);
void WriteEEPROM_Byte(uint8_t deviceAddress, unsigned int address, byte data);
void ReadEEPROM(uint8_t deviceAddress, unsigned int startAddress, byte* data, unsigned int num_chars);