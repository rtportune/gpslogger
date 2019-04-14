#include "eeprom.h"
#include <Wire.h>


//Write the given data buffer into EEPROM beginning at the given address. Uses page writes.
void WriteEEPROM_Page(uint8_t deviceAddress, unsigned int startAddress, byte* data, int length)
{
	int address;
	int page_space;
	int data_len = 0;
	int write_size;

	// Calculate length of data
	if (length < 0)
	{
		do { data_len++; } while (data[data_len]);
	}
	else
		data_len = length;

	// Calculate space available in first page
	page_space = EEPROM_PAGE_SIZE - (startAddress % EEPROM_PAGE_SIZE);

	int dataOffset = 0;
	address = startAddress;
	while (data_len > 0)
	{
		write_size = min(EEPROM_MAX_WRITE, page_space);
		write_size = min(data_len, write_size);

		Wire.beginTransmission(deviceAddress);
		Wire.write((int)((address) >> 8));
		Wire.write((int)((address) & 0xFF));
		for (int i = 0; i < write_size; i++)
		{
			Wire.write(data[dataOffset + i]);
		}
		Wire.endTransmission();
		address += write_size;
		dataOffset += write_size;

		data_len -= write_size;
		page_space -= write_size;

		if (page_space == 0)
			page_space = EEPROM_PAGE_SIZE;

		delay(EEPROM_PAGE_DELAY);
	}
}

//Write one byte of data to the specified address in EEPROM
void WriteEEPROM_Byte(uint8_t deviceAddress, unsigned int address, byte data)
{
	Wire.beginTransmission(deviceAddress);
	Wire.write((int)((address) >> 8));   // MSB
	Wire.write((int)((address) & 0xFF)); // LSB
	Wire.write(data);
	Wire.endTransmission();
}

//Read the specified number of bytes from EEPROM into the given buffer (data). 
void ReadEEPROM(uint8_t deviceAddress, unsigned int startAddress, byte* data, unsigned int num_chars)
{
	unsigned char i = 0;
	Wire.beginTransmission(deviceAddress);
	Wire.write((int)(startAddress >> 8));   // MSB
	Wire.write((int)(startAddress & 0xFF)); // LSB
	Wire.endTransmission();

	Wire.requestFrom(deviceAddress, num_chars);

	while (Wire.available()) data[i++] = Wire.read();
}