#define EEPROM_ADDR 0x50

typedef struct 
{
	uint8_t hours, minutes, seconds;
	uint8_t year, month, day;
	float latitude, longitude;
	float altitude;
}logPoint;