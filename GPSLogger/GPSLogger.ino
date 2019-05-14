#include <U8x8lib.h>
#include <U8g2lib.h>
#include <Adafruit_SPIFlash.h>
#include <Adafruit_SPIFlash_FatFs.h>
#include <Adafruit_GPS.h>
#include <Wire.h>
#include <Keyboard.h>
#include "GPSLogger.h"

// -- HW --
//Display Object
U8G2_SSD1306_128X64_NONAME_F_HW_I2C _display(U8G2_R0, U8X8_PIN_NONE);
//Flash Storage Object
Adafruit_SPIFlash _flash(PIN_QSPI_SCK, PIN_QSPI_IO1, PIN_QSPI_IO0, PIN_QSPI_CS);
Adafruit_W25Q16BV_FatFs _fatfs(_flash);
//GPS Object
#define GPSSerial Serial1
Adafruit_GPS _gps(&GPSSerial);

//Config object
systemConfiguration _config;
//Current mode of the system
e_SystemMode _currentMode = Menu;
e_MenuMode _currentMenuMode = SelectMode;
bool _needRedraw = true;
trip _trips[20];
int _numTrips = 0;

//Tracking Variables
int _currentTripOffset;
logPoint _lastPoint;
int _currentUpdateRate;

//Menu Stuff
int _selectedUpdateRate;
e_MenuMode _selectedMode = (e_MenuMode)1;
int _currentCursorPos = 0;
int _selectedTripPos;
String _newTripName;

//Button States
int _usePrevious = 0;
int _advancePrevious = 0;

//Timekeeping
unsigned long _currentTime;
unsigned long _lastGPSUpdate = 0;
unsigned long _lastRenderTime = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  _display.begin();

  _newTripName = "                ";

  //This is how we determine that there is no last point
  _lastPoint.day = INVALID_POINT_DAY;

  delay(5000);
  LoadConfiguration();
  InitGPS();

  pinMode(USE_PIN, INPUT);
  pinMode(ADVANCE_PIN, INPUT);
}

void loop() 
{
	_currentTime = millis();

	CheckButtons();
	UpdateCurrentTrip();
	Render();
}

void CheckButtons()
{
	// ----------- Use Button --------------
	int sample = digitalRead(USE_PIN);
	if (sample == HIGH && sample != _usePrevious)
	{
		_needRedraw = true;

		if (_currentMode == Menu)
		{
			if (_currentMenuMode == SelectMode)
			{
				int sel = (int)_selectedMode;
				//If there are any trips stored, all options are available, otherwise only New Trip is available
				if (_numTrips > 0)
				{
					sel += 1;
					if (sel > 4)
						sel = 1;

					_selectedMode = (e_MenuMode)sel;
				}
			}
			else if (_currentMenuMode == NewTrip)
			{
				//If it is currently on top of one of the letters of the name
				if (_currentCursorPos <= 15)
				{
					uint8_t curr = _newTripName[_currentCursorPos];
					if (curr == 32)
						_newTripName[_currentCursorPos] = 65;
					else if (curr == 90)
						_newTripName[_currentCursorPos] = 97;
					else if (curr == 122)
						_newTripName[_currentCursorPos] = 32;
					else
						_newTripName[_currentCursorPos] += 1;
				}
				if (_currentCursorPos == CURSOR_POS_NEWTRIP_RATE)
				{
					//update rate selection
					_selectedUpdateRate += 1;
					if (_selectedUpdateRate > 7)
						_selectedUpdateRate = 0;
				}
				if (_currentCursorPos == CURSOR_POS_NEWTRIP_CANCEL)
				{
					//Go back to select menu mode
					_currentMenuMode = SelectMode;
					_currentCursorPos = 0;
				}
				if (_currentCursorPos == CURSOR_POS_NEWTRIP_START)
				{
					//todo handle invalid trip number (over 20)
					StartNewTrip();
					_currentCursorPos = 0;
				}
			}
			else if (_currentMenuMode == ResumeTrip || _currentMenuMode == DeleteTrip || _currentMenuMode == Dump)
			{
				if (_currentCursorPos == 0)
				{
					_selectedTripPos++;

					if (_selectedTripPos > MAX_TRIPS - 1)
						_selectedTripPos = 0;

					//Skip over the blank trips
					while (_trips[_selectedTripPos].friendlyName.length() == 0)
					{
						_selectedTripPos++;		

						if (_selectedTripPos > MAX_TRIPS - 1)
							_selectedTripPos = 0;
					}
				}
				if (_currentCursorPos == CURSOR_POS_RESDELTRIP_CANCEL)
				{
					_currentMenuMode = SelectMode;
					_currentCursorPos = 0;
				}
				if (_currentCursorPos == CURSOR_POS_RESDELTRIP_RESUMEDELETEDUMP)
				{
					if (_currentMenuMode == ResumeTrip)
					{
						ResumeSelectedTrip();
						_currentCursorPos = 0;
					}
					else if (_currentMenuMode == DeleteTrip)
					{
						DeleteSelectedTrip();

						//After the trip has been deleted, we need to fix the selected trip position

						_selectedTripPos += 1;
						if (_selectedTripPos > MAX_TRIPS - 1)
							_selectedTripPos = 0;

						while (_trips[_selectedTripPos].friendlyName.length() == 0)
						{
							_selectedTripPos++;

							if (_selectedTripPos > MAX_TRIPS - 1)
								_selectedTripPos = 0;
						}

						//Now go back to the main menu
						_currentMenuMode = SelectMode;
						_currentCursorPos = 0;
					}
					else if (_currentMenuMode == Dump)
					{
						DumpSelectedTrip();
						
						//Now go back to main menu
						_currentMenuMode = SelectMode;
						_currentCursorPos = 0;
					}
				}
			}
		}
		else if (_currentMode == Track)
		{
			//Hit the Quit tracking button
			if (_currentCursorPos == 0)
			{
				_currentMode = Menu;
				_currentMenuMode = ResumeTrip;
				_currentCursorPos = 0;

				//todo probably need to disable logging too
			}
		}
	}

	// --------- Advance Button ------------

	int sample2 = digitalRead(ADVANCE_PIN);
	if (sample2 == HIGH && sample2 != _advancePrevious)
	{
		_needRedraw = true;
		if (_currentMenuMode == SelectMode)
		{
			if (_selectedMode == NewTrip)
			{
				_currentMenuMode = _selectedMode;
				_currentCursorPos = 0;
			}
			if (_selectedMode == ResumeTrip || _selectedMode == DeleteTrip || _selectedMode == Dump)
			{
				_currentMenuMode = _selectedMode;
				_currentCursorPos = 0;
				_selectedTripPos = 0;
			}
		}
		else if (_currentMenuMode == NewTrip)
		{
			_currentCursorPos++;
			if (_currentCursorPos > 18)
				_currentCursorPos = 0;
		}
		else if (_currentMenuMode == ResumeTrip || _currentMenuMode == DeleteTrip || _selectedMode == Dump)
		{
			_currentCursorPos++;
			if (_currentCursorPos > 2)
				_currentCursorPos = 0;
		}
	}

	_advancePrevious = sample2;
	_usePrevious = sample;
}

void StartNewTrip()
{
	//New trip offset is equal to the number of trips.
	_currentTripOffset = _numTrips;
	
	//If we deleted a trip, there may be a hole in the trips collection, so we need to find the first
	//vacant spot. I.e. where the trip name is still blank.
	while (_trips[_currentTripOffset].friendlyName.length() != 0)
	{
		_currentTripOffset++;

		if (_currentTripOffset > MAX_TRIPS - 1)
			_currentTripOffset = 0;
	}

	//Increase number of trips
	_numTrips++;

	_trips[_currentTripOffset].friendlyName = _newTripName;
	_trips[_currentTripOffset].uniqueID = CRC32((byte*)_newTripName.c_str(), 16);
	_trips[_currentTripOffset].pollRate = UPDATE_RATES[_selectedUpdateRate];

	_currentUpdateRate = _trips[_currentTripOffset].pollRate;

	//Save new data to trips descriptor file. This will open for append (which we want)
	File tripsFile = _fatfs.open(TRIPS_DESCRIPTOR_FILE, FILE_WRITE);
	if (tripsFile)
	{
		tripsFile.println(_newTripName);
		tripsFile.println(_trips[_currentTripOffset].uniqueID);
		tripsFile.println(_trips[_currentTripOffset].pollRate);

		tripsFile.close();
	}

	//Create a new trip file for the new trip. We use the uniqueID as the file name
	String fName = String(_trips[_currentTripOffset].uniqueID) + String(".txt");
	File tripFile = _fatfs.open(fName, FILE_WRITE);
	if (tripFile)
	{
		tripFile.close();
	}

	//Change to Tracking mode
	_currentMode = Track;
}

void DeleteSelectedTrip()
{
	//Remove the trip from the trips list
	_trips[_selectedTripPos].friendlyName = "";
	_trips[_selectedTripPos].pollRate = -1;
	_trips[_selectedTripPos].uniqueID = 0;

	//Number of trips just dropped by one
	_numTrips--;

	//Delete the trips descriptor file
	if (_fatfs.remove(TRIPS_DESCRIPTOR_FILE))
	{
		//Create new trips descriptor file
		File tripsFile = _fatfs.open(TRIPS_DESCRIPTOR_FILE, FILE_WRITE);
		if (tripsFile)
		{
			//Put all the valid trips in it
			for (int i = 0; i < MAX_TRIPS; i++)
			{
				//If this is a valid trip
				if (_trips[i].friendlyName.length() > 0)
				{
					tripsFile.println(_trips[i].friendlyName);
					tripsFile.println(_trips[i].uniqueID);
					tripsFile.println(_trips[i].pollRate);
				}
			}

			tripsFile.close();
		}
	}
}

void ResumeSelectedTrip()
{
	//If the name length is zero, then this is an invalid trip
	if (_trips[_selectedTripPos].friendlyName.length() == 0)
		return;

	_currentTripOffset = _selectedTripPos;
	_currentUpdateRate = _trips[_currentTripOffset].pollRate;

	//Load the trip file, find the last log point so we can display it on screen
	String tripFileName = String(_trips[_currentTripOffset].uniqueID) + String(".txt");

	File tripFile = _fatfs.open(tripFileName, FILE_READ);

	if (tripFile)
	{
		String currentLine;
		while (tripFile.available())
		{
			currentLine = tripFile.readStringUntil('\n');

			Serial.print("Reading: ");
			Serial.println(currentLine);
		}

		tripFile.close();

		//currentLine now contains the last GPS entry
		if (currentLine.length() > 0)
		{
			//Valid entry - copy the bytes. Sizeof _lastPoint guarantees we don't get the extra newline character at the end of the string, or the null terminator.
			memcpy((char*)&_lastPoint, currentLine.c_str(), sizeof(_lastPoint));
		}
		else
			_lastPoint.day = INVALID_POINT_DAY;
	}

	_currentMode = Track;
}

void UpdateCurrentTrip()
{
	char c = _gps.read();
	if (_gps.newNMEAreceived()) {
		// a tricky thing here is if we print the NMEA sentence, or data
		// we end up not listening and catching other sentences!
		// so be very wary if using OUTPUT_ALLDATA and trytng to print out data
		Serial.println(_gps.lastNMEA());
		if (!_gps.parse(_gps.lastNMEA())) // this also sets the newNMEAreceived() flag to false
			return; // we can fail to parse a sentence in which case we should just wait for another
	}

	if (_currentMode != Track)
	{
		_lastGPSUpdate = _currentTime;
		return;
	}

	//See if enough time has elapsed, and if we have a good GPS fix. If so, add a new log point to the current trip
	if (_currentTime - _lastGPSUpdate >= (_currentUpdateRate * 1000))
	{
		Serial.print("Fix Quality = ");
		Serial.println(_gps.fixquality);

		//need update
		if (_gps.fixquality > 0)
		{
			_lastPoint.day = _gps.day;
			_lastPoint.month = _gps.month;
			_lastPoint.year = _gps.year;

			_lastPoint.hours = _gps.hour;
			_lastPoint.minutes = _gps.minute;
			_lastPoint.seconds = _gps.seconds;

			_lastPoint.latitude = _gps.latitude;
			_lastPoint.longitude = _gps.longitude;
			_lastPoint.altitude = _gps.altitude;

			_lastPoint.lat = _gps.lat;
			_lastPoint.lon = _gps.lon;
			
			//Save data to file
			String tripFileName = String(_trips[_currentTripOffset].uniqueID) + String(".txt");
			File tripFile = _fatfs.open(tripFileName, FILE_WRITE);
			if (tripFile)
			{
				tripFile.write((uint8_t*)&_lastPoint, sizeof(_lastPoint));
				tripFile.write('\n');

				tripFile.close();
			}

			_needRedraw = true;
		}

		_lastGPSUpdate += (_currentUpdateRate * 1000);
	}
}

void DumpSelectedTrip()
{
	String tripFileName = String(_trips[_selectedTripPos].uniqueID) + String(".txt");
	File tripFile = _fatfs.open(tripFileName, FILE_READ);
	if (tripFile)
	{
		//Write the trip name
		const char* tripName = _trips[_selectedTripPos].friendlyName.c_str();
		Keyboard.write((unsigned char*)tripName, sizeof(tripName));
		Keyboard.write('\n');
		String currentLine;
		logPoint currentPoint;
		char buf[100];
		while (tripFile.available())
		{
			currentLine = tripFile.readStringUntil('\n');
			
			if (currentLine.length() > 0)
			{
				memcpy((char*)&currentPoint, currentLine.c_str(), sizeof(currentPoint));
				PointToFullString(&currentPoint, buf);
				for (int i = 0; i < 100; i++)
				{
					Keyboard.write(buf[i]);
					if (buf[i] == '\n')
						break;
				}
			}
		}

		tripFile.close();
	}
	else
		Serial.println("Failed to open trip file!");
}

void Render()
{
	//If the last time we rendered was more than the timeout period ago, then shut off the display.
	if (_currentTime - _lastRenderTime > DISPLAY_TIMEOUT)
		_display.setPowerSave(true);

	if (!_needRedraw) return;

	//record last render time
	_lastRenderTime = _currentTime;
	//Turn the display on
	_display.setPowerSave(false);

	_display.clearBuffer();

	switch (_currentMode) 
	{
	case Menu:
		RenderMode_Menu();
		break;
	case Track:
		RenderMode_Track();
		break;
	case SerialDump:
		RenderMode_SerialDump();
		break;
	}

	_display.sendBuffer();
	_needRedraw = false;
}

void RenderMode_Menu()
{
	RenderMenu_Header();

	if (_currentMenuMode == SelectMode)
		RenderMenu_SelectMode();
	else if (_currentMenuMode == ResumeTrip || _currentMenuMode == DeleteTrip || _currentMenuMode == Dump)
		RenderMenu_ResumeDeleteDumpTrip();
	else if (_currentMenuMode == NewTrip)
		RenderMenu_NewTrip();
}

void RenderMode_Track()
{
	RenderMode_Header();

	//---- trip Name ----
	_display.setDrawColor(1);
	_display.setFontMode(0);

	_display.drawStr(0, PT_8_FONT_ROW_2, "Trip:");
	_display.drawStr(30, PT_8_FONT_ROW_2, _trips[_currentTripOffset].friendlyName.c_str());

	//Last Point
	_display.drawStr(0, PT_8_FONT_ROW_3, "Last:");

	if (_lastPoint.day == INVALID_POINT_DAY)
	{
		//Invalid point
		_display.drawStr(30, PT_8_FONT_ROW_3, "None");
	}
	else //valid point
	{
		char ptStr[18];
		PointToString(&_lastPoint, ptStr);
		_display.drawStr(30, PT_8_FONT_ROW_3, ptStr);
	}

	//Quit button
	_display.drawBox(0, PT_8_FONT_ROW_4_H, 24, 10);

	_display.setDrawColor(0);
	_display.setFontMode(1);

	_display.drawStr(0, PT_8_FONT_ROW_4, "QUIT");
}

void RenderMode_SerialDump()
{
	RenderMode_Header();
}

void RenderMenu_Header()
{
	_display.setFont(u8g2_font_helvB12_tr);

	char modeStr[15];
	int len;

	MenuModeToString(_currentMenuMode, modeStr, &len);

	_display.setDrawColor(1);
	_display.setFontMode(0);
	_display.drawStr(0, 12, modeStr);

	_display.setFont(u8g2_font_t0_12_tr);
}

void RenderMode_Header()
{
	_display.setFont(u8g2_font_helvB12_tr);

	char modeStr[15];
	int len;

	SystemModeToString(_currentMode, modeStr, &len);

	_display.setDrawColor(1);
	_display.setFontMode(0);
	_display.drawStr(0, 12, modeStr);

	_display.setFont(u8g2_font_t0_12_tr);
}

void RenderMenu_ResumeDeleteDumpTrip()
{
	_display.drawStr(0, PT_8_FONT_ROW_2, "Trip:");

	int len = _trips[_selectedTripPos].friendlyName.length();

	if (_currentCursorPos == 0)
		_display.drawBox(30, PT_8_FONT_ROW_2_H, len * 6, 10);

	_display.setDrawColor(_currentCursorPos == 0 ? 0 : 1);
	_display.setFontMode(_currentCursorPos == 0 ? 1 : 0);

	_display.drawStr(30, PT_8_FONT_ROW_2, _trips[_selectedTripPos].friendlyName.c_str());

	//Draw two buttons either Resume & Cancel or Delete & Canc
	_display.setDrawColor(1);
	_display.setFontMode(0); 
	if (_currentCursorPos > 0)
	{
		if (_currentCursorPos == CURSOR_POS_RESDELTRIP_RESUMEDELETEDUMP)
			_display.drawBox(0, PT_8_FONT_ROW_3_H, 36, 10);
		else
			_display.drawBox(42, PT_8_FONT_ROW_3_H, 36, 10);
	}
	if (_currentCursorPos == CURSOR_POS_RESDELTRIP_RESUMEDELETEDUMP)
	{
		_display.setDrawColor(0);
		_display.setFontMode(1);
	}

	_display.drawStr(0, PT_8_FONT_ROW_3, _currentMenuMode == ResumeTrip ? "RESUME" : _currentMenuMode == DeleteTrip ? "DELETE" : "DUMP");

	_display.setDrawColor(_currentCursorPos == CURSOR_POS_RESDELTRIP_CANCEL ? 0 : 1);
	_display.setFontMode(_currentCursorPos == CURSOR_POS_RESDELTRIP_CANCEL ? 1 : 0);

	_display.drawStr(42, PT_8_FONT_ROW_3, "CANCEL");
}

void RenderMenu_SelectMode()
{
	_display.setDrawColor(1);
	_display.setFontMode(0);
	_display.drawStr(0, PT_8_FONT_ROW_2, "Mode: ");

	char modeStr[15];
	int len;

	MenuModeToString(_selectedMode, modeStr, &len);

	_display.drawBox(35, PT_8_FONT_ROW_2_H, len * 6, 10);

	_display.setDrawColor(0);
	_display.setFontMode(1);

	_display.drawStr(35, PT_8_FONT_ROW_2, modeStr);
}

void RenderMenu_NewTrip()
{
	//------------ Name Entry -------------

	_display.setDrawColor(1);
	_display.setFontMode(0);
	_display.drawStr(0, PT_8_FONT_ROW_2, "Name:");

	if (_currentCursorPos < 16)
		_display.drawBox(29 + (6 * _currentCursorPos), PT_8_FONT_ROW_2_H, 6, 10);

	uint8_t drawColor, fontMode;
	for (int i = 0; i < 16; i++)
	{
		drawColor = i == _currentCursorPos ? 0 : 1;
		fontMode = i == _currentCursorPos ? 1 : 0;

		_display.setDrawColor(drawColor);
		_display.setFontMode(fontMode);
		_display.drawGlyph(29 + (6 * i), PT_8_FONT_ROW_2, _newTripName[i]);
	}

	// ---------- Update Rate Selection ------------

	_display.setDrawColor(1);
	_display.setFontMode(0);

	_display.drawStr(0, PT_8_FONT_ROW_3, "Update: ");

	char rateStr[5];
	int len;
	UpdateRateToString(_selectedUpdateRate, rateStr, &len);

	//handle update rates selection
	if (_currentCursorPos == 16) //update rate selection
	{
		_display.drawBox(45, PT_8_FONT_ROW_3_H, len * 6, 10);
		//set to reversed text before next draw
		_display.setDrawColor(0);
		_display.setFontMode(1);
	}

	_display.drawStr(45, PT_8_FONT_ROW_3, rateStr);


	//---------------last two buttons -----------

	_display.setDrawColor(1);
	_display.setFontMode(0);

	if (_currentCursorPos > 16)
	{
		if (_currentCursorPos == CURSOR_POS_NEWTRIP_START)
			_display.drawBox(0, PT_8_FONT_ROW_4_H, 30, 10);
		else
			_display.drawBox(35, PT_8_FONT_ROW_4_H, 36, 10);
	}

	if (_currentCursorPos == 17)
	{
		_display.setDrawColor(0);
		_display.setFontMode(1);
	}

	_display.drawStr(0, PT_8_FONT_ROW_4, "START");

	_display.setDrawColor(_currentCursorPos == CURSOR_POS_NEWTRIP_CANCEL ? 0 : 1);
	_display.setFontMode(_currentCursorPos == CURSOR_POS_NEWTRIP_CANCEL ? 1 : 0);

	_display.drawStr(35, PT_8_FONT_ROW_4, "CANCEL");
}

void LoadConfiguration()
{
  Serial.println("Loading Trips File...");
  
	//Start Flash communication
	_flash.begin(FLASH_TYPE);
  if (!_fatfs.begin())
    Serial.println("Failed to read filesystem!");
    
	//Open trip descriptor file if it exists
	if (_fatfs.exists(TRIPS_DESCRIPTOR_FILE))
	{
		//load it
		File tripsFile = _fatfs.open(TRIPS_DESCRIPTOR_FILE, FILE_READ);
		if (tripsFile)
		{
			ParseTripsDescriptorFile(tripsFile);
			tripsFile.close();
		}
		else
			Serial.println("Error opening trips file!");
	}
	else //Create a blank trips file
	{	
		Serial.println("Trips file does not exist. Creating new trips file");
		File tripsFile = _fatfs.open(TRIPS_DESCRIPTOR_FILE, FILE_WRITE);
		if (tripsFile)
		{
			Serial.println("Successfully created trips file");
			tripsFile.close();
		}
		else
			Serial.println("Error creating trips file!");
	}
}

void ParseTripsDescriptorFile(File& file)
{
	int currentTrip = 0;
	while (file.available())
	{
		//first line is name
		_trips[currentTrip].friendlyName = String(file.readStringUntil('\n'));
		//Unique ID is next
		_trips[currentTrip].uniqueID = atoi(file.readStringUntil('\n').c_str());
		//Poll rate in seconds is next
		_trips[currentTrip].pollRate = atoi(file.readStringUntil('\n').c_str());

		currentTrip++;
	}

	_numTrips = currentTrip;

	Serial.print("Found ");
	Serial.print(_numTrips);
	Serial.println(" trips.");

	//Only thing we can do with no trips is create a new trip
	if (_numTrips == 0)
	{
		_selectedMode = NewTrip;
	}
}

bool InitGPS()
{
	_gps.begin(9600);
	_gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
	_gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
	_gps.sendCommand(PGCMD_NOANTENNA);
	return true;
}
