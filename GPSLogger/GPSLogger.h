#include <string.h>

#define FLASH_TYPE SPIFLASHTYPE_W25Q16BV

#define TRIPS_DESCRIPTOR_FILE "trips.txt"

const int CONFIG_ADDRESS = 0x00000004;
const int CRC_ADDRESS = 0x00000000;

const uint8_t USE_PIN = 10;
const uint8_t ADVANCE_PIN = 11;

const uint8_t INVALID_POINT_DAY = 255;
const uint8_t MAX_TRIPS = 20;

const uint16_t DISPLAY_TIMEOUT = 5000;

//Starting Y cursor coordinates for rendering menu strings (using 8-pt fonts)
const uint8_t PT_8_FONT_ROW_2 = 26;
const uint8_t PT_8_FONT_ROW_3 = 37;
const uint8_t PT_8_FONT_ROW_4 = 48;

const uint8_t PT_8_FONT_ROW_2_H = 17;
const uint8_t PT_8_FONT_ROW_3_H = 28;
const uint8_t PT_8_FONT_ROW_4_H = 39;

//Cursor offsets for menu modes
const uint8_t CURSOR_POS_NEWTRIP_RATE = 16;
const uint8_t CURSOR_POS_NEWTRIP_START = 17;
const uint8_t CURSOR_POS_NEWTRIP_CANCEL = 18;

const uint8_t CURSOR_POS_RESDELTRIP_RESUMEDELETEDUMP = 1;
const uint8_t CURSOR_POS_RESDELTRIP_CANCEL = 2;

//15s 30s 1m 15m 30m 1h 2h 4h 
const int UPDATE_RATES[8] = { 15, 30, 60, 900, 1800, 3600, 7200, 14400 };

typedef struct 
{
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t lat;
	uint8_t lon;
	float latitude;
	float longitude;
	float altitude;
}logPoint;

typedef struct
{
	unsigned long uniqueID;
	String friendlyName;
	int pollRate;
}trip;

typedef enum
{
	Menu = 0,
	Track = 1,
	SerialDump = 2
}e_SystemMode;

typedef enum
{
	SelectMode = 0,
	ResumeTrip = 1,
	DeleteTrip = 2,
	NewTrip = 3,
	Dump = 4
}e_MenuMode;

typedef struct
{
	int* lastTrip;
	int* lastMemoryAddress;
	e_SystemMode lastMode;
	e_MenuMode lastMenuMode;
}systemConfiguration;

//Calculates the CRC32 of the given input buffer
unsigned long CRC32(byte* input, int size)
{
	const unsigned long crc_table[16] =
	{
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};

	unsigned long crc = ~0L;

	for (int i = 0; i < size; i++)
	{
		crc = crc_table[(crc ^ input[i]) & 0x0F] ^ (crc >> 4);
		crc = crc_table[(crc ^ (input[i] >> 4)) & 0x0F] ^ (crc >> 4);
		crc = ~crc;
	}

	return crc;
}

int GetTripNameLength(const trip* trip)
{
	int count = 0;

	for (int i = 0; i < 16; i++)
		if ((byte)trip->friendlyName[i] != 0)
			count++;

	return count;
}

void PointToString(const logPoint* point, char* string)
{
	sprintf(string, "%d/%d/20%d %d:%d", point->day, point->month, point->year, point->hours, point->minutes);
}

void PointToFullString(const logPoint* point, char* string)
{
	sprintf(string, "%d/%d/20%d %d:%d:%d -- Lat=%f%c Long=%f%c Alt=%fm\n", point->day, point->month, point->year, point->hours, point->minutes, point->seconds, point->latitude, point->lat, point->longitude, point->lon, point->altitude);
}

void MenuModeToString(e_MenuMode mode, char* string, int* size)
{
	switch (mode)
	{
	case SelectMode:
		sprintf(string, "Select Mode");
		*size = 11;
		break;
	case ResumeTrip:
		sprintf(string, "Resume Trip");
		*size = 11;
		break;
	case DeleteTrip:
		sprintf(string, "Delete Trip");
		*size = 11;
		break;
	case NewTrip:
		sprintf(string, "New Trip");
		*size = 8;
		break;
	case Dump:
		sprintf(string, "Dump Data");
		*size = 9;
		break;
	}
}

void SystemModeToString(e_SystemMode mode, char* string, int* size)
{
	switch (mode)
	{
	case Menu:
		sprintf(string, "");
		*size = 0;
		break;
	case Track:
		sprintf(string, "Tracking");
		*size = 7;
		break;
	case SerialDump:
		sprintf(string, "Dump to Serial");
		*size = 14;
		break;
	}
}

void UpdateRateToString(int updateRate, char* string, int* size)
{
	switch (updateRate)
	{
	case 0:
		sprintf(string, "15s");
		*size = 3;
		break;
	case 1:
		sprintf(string, "30s");
		*size = 3;
		break;
	case 2:
		sprintf(string, "1m");
		*size = 2;
		break;
	case 3:
		sprintf(string, "15m");
		*size = 3;
		break;
	case 4:
		sprintf(string, "30m");
		*size = 3;
		break;
	case 5:
		sprintf(string, "1h");
		*size = 2;
		break;
	case 6:
		sprintf(string, "2h");
		*size = 2;
		break;
	case 7:
		sprintf(string, "4h");
		*size = 2;
		break;
	}
}
