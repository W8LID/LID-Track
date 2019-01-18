#include <Plan13.h>
#include <Adafruit_GPS.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include "Adafruit_ILI9341.h" // Hardware-specific library
#include <Adafruit_STMPE610.h>
#include <Adafruit_ImageReader.h>
#include <Wire.h>
#include <SD.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans12pt7b.h>

#define mySerial Serial1
#define STMPE_CS 6
#define TFT_CS   9
#define TFT_DC   10
#define SD_CS    5
#define TS_MINX 150
#define TS_MINY 130
#define TS_MAXX 3800
#define TS_MAXY 4000
#define ONEPPM 1.0e-6
#define BG_COLOR 0xB5B6
#define DOWNLINK_COLOR 0x0C42
#define UPLINK_COLOR 0x8801

typedef enum{
    MAIN_SCREEN = 0,
    GPS_SCREEN,
    SAT_SELECT_SCREEN,
    SAT_SCREEN
} ScreenType;

Adafruit_GPS GPS(&mySerial);
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
Adafruit_STMPE610 touch = Adafruit_STMPE610(STMPE_CS);
Adafruit_ImageReader reader;
Plan13 p13;

bool wasTouched = false;
bool screenIsDim = false;

// TLE data
char e1[70];
char e2[70];

double bestLat, bestLon;

ScreenType currentScreen = SAT_SCREEN;

String callSign = "W8LID / VE6LID";
String lastDate = " ";
String lastTime = " ";
String lastGrid = " ";
String lastLat = " ";
String lastLon = " ";
String lastSats = " ";
String lastAz = " ";
String lastEl = " ";
String lastSatName = " ";
String selectedSat = " ";
String lastUplink = " ";
String lastDownlink = " ";
String uplinkMode = " ";
String downlinkMode = " ";
String description = " ";
String lastDescription = " ";

unsigned char *converted;    // holds the converted freq

unsigned int foundSats = 0;
unsigned int satIndex = 0;
unsigned int backlightPin = 11;

unsigned long screenTimer;
unsigned long screenTimeout = (30 * 1000);
unsigned long refreshTimer;
unsigned long refreshTimeout = 500;
unsigned long downlinkBaseFreq;
unsigned long uplinkBaseFreq;

void setup()
{
    // Screen brightness control
    pinMode(backlightPin, OUTPUT);
    wakeScreen();
    
    // Init GPS
    GPS.begin(9600);
    mySerial.begin(9600);
    GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 1 Hz update rate
    
    // Init touchscreen
    touch.begin();
    //Init SD slot
    SD.begin(SD_CS);
    // Init TFT display
    tft.begin();
    tft.setRotation(1);
    
    // Setup defaults
    satIndex = 0;
    showMainScreen();
    foundSats = numOfSats();
}


void loop()
{
    // read data from the GPS in the 'main loop'
    char c = GPS.read();
    
    // if a sentence is received, we can check the checksum, parse it...
    if (GPS.newNMEAreceived())
    {
        GPS.parse(GPS.lastNMEA());
        // this also sets the newNMEAreceived() flag to false
    }
    
    // Check for touch
    getSingleTouch();
    
    if (millis() - screenTimer >= screenTimeout && !screenIsDim)
    {
        dimScreen();
    }

    if (millis() - refreshTimer >= refreshTimeout)
    {
        updateLocation();
        refreshTimer = millis();
    }
}

///////////////////////////////
////   Hardware Control   /////
///////////////////////////////
void wakeScreen()
{
    screenIsDim = false;
    screenTimer = millis();
    analogWrite(11,168);
}

void dimScreen()
{
    screenIsDim = true;
    for(int i = 168; i > 8; i--)
    {
        analogWrite(11,i);
        delay(1);
    }
}

void getSingleTouch()
{
    TS_Point p;
    
    if (touch.bufferSize())
    {
        p = touch.getPoint();
        
        if(!wasTouched && !screenIsDim)
        {
            int x = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());
            int y = map(p.y, TS_MINY, TS_MAXY, 0, tft.width());
            switch(currentScreen)
            {
                case MAIN_SCREEN:
                {
                    if(x >= 80 && x <= 140)
                    {
                        showLocationScreen();
                    }

                    if(x >= 160 && x <= 220)
                    {
                        showSatSelectionScreen();
                    }
                }
                    break;
                case GPS_SCREEN:
                {
                    if( x>= 190 && y <= 110) // Back Button
                    {
                        showMainScreen();
                    }
                }
                    break;
                case SAT_SELECT_SCREEN:
                {
                    if( x>= 190 && y <= 110) // Back Button
                    {
                        showMainScreen();
                    }

                    if( x>= 190 && y >= 120) 
                    {
                        showSatScreen();
                    }
                    
                    if( x<= 160 && y <= 160) 
                    {
                        previousSat();
                    }
                    
                    if( x<= 160 && y >= 160) 
                    {
                        nextSat();
                    }
                }
                    break;
                case SAT_SCREEN:
                {
                    if( x>= 190 && y <= 110) 
                    {
                        showSatSelectionScreen();
                    }
                }
                    break;
                default:
                    break;
            }
            
        }
        wakeScreen();
        wasTouched = true;
    }
    else
    {
        if(wasTouched && !touch.touched())
        {
            wasTouched = false;
        }
    }
}

///////////////////////////////
////   Screen loading     /////
///////////////////////////////

void showMainScreen()
{
    refreshTimeout = 1000;
    currentScreen = MAIN_SCREEN;
    
    tft.fillScreen(BG_COLOR);
    
    int16_t  x1, y1;
    uint16_t w, h;
    
    tft.setFont(&FreeSansBold18pt7b);
    
    tft.getTextBounds(string2char(callSign), 0, 0, &x1, &y1, &w, &h);
    int cursorStart = (tft.width() / 2) - (w / 2);
    tft.setCursor(cursorStart, 50);
    tft.setTextColor(ILI9341_BLACK);
    tft.println(callSign);

    tft.fillRoundRect(10, 80, (tft.width() - 20), 60, 8, ILI9341_WHITE);
    
    tft.getTextBounds(string2char("GPS"), 0, 0, &x1, &y1, &w, &h);
    cursorStart = (tft.width() / 2) - (w / 2);
    tft.setCursor(cursorStart, 124);
    tft.setTextColor(ILI9341_BLACK);
    tft.println("GPS");
    
    tft.fillRoundRect(10, 160, (tft.width() - 20), 60, 8, ILI9341_WHITE);
    
    tft.getTextBounds(string2char("Satellites"), 0, 0, &x1, &y1, &w, &h);
    cursorStart = (tft.width() / 2) - (w / 2);
    tft.setCursor(cursorStart, 204);
    tft.setTextColor(ILI9341_BLACK);
    tft.println("Satellites");
}

void showSatSelectionScreen()
{
    refreshTimeout = 1000;
    currentScreen = SAT_SELECT_SCREEN;
    lastSatName = " ";
    lastDescription = " ";
    tft.fillScreen(BG_COLOR);

    showBackButton();

    tft.fillRoundRect(120, 190, 80, 40, 8, ILI9341_WHITE);

    tft.setFont(&FreeSansBold12pt7b);
    tft.setCursor(126, 218);
    tft.setTextColor(ILI9341_BLACK);
    tft.println("Select");
    switchToSat(satIndex);
}

void showSatScreen()
{
    refreshTimeout = 500;
    currentScreen = SAT_SCREEN;
    lastSatName = " ";
    lastDescription = " ";

    tft.fillScreen(BG_COLOR);
    
    tft.setFont(&FreeSansBold12pt7b);
    
    tft.setCursor(10, 90);
    tft.setTextColor(UPLINK_COLOR);
    tft.print("Uplink");
    
    tft.setCursor(10, 120);
    tft.setTextColor(DOWNLINK_COLOR);
    tft.print("Downlink");
    
    tft.setCursor(10, 150);
    tft.setTextColor(ILI9341_BLACK);
    tft.print("Azimuth");
    
    tft.setCursor(10, 180);
    tft.setTextColor(ILI9341_BLACK);
    tft.print("Elevation");

    tft.drawRect(5, 67, 310, 120, ILI9341_BLACK);

    showBackButton();
    //switchToSat(satIndex);
}

void showLocationScreen()
{
    refreshTimeout = 100;
    currentScreen = GPS_SCREEN;
    tft.fillScreen(BG_COLOR);
    lastGrid = " "; //Need to do this so grid label refreshes on reentry
    lastDate = " ";
    lastTime = " ";
    lastLat = " ";
    lastLon = " ";
    showBackButton();
}

void showBackButton()
{
    tft.fillRoundRect(10, 190, 100, 40, 8, ILI9341_WHITE);
    tft.fillTriangle(15, 210, 45, 195, 45, 225, ILI9341_BLACK);

    tft.setFont(&FreeSansBold12pt7b);
    tft.setCursor(48, 218);
    tft.setTextColor(ILI9341_BLACK);
    tft.println("Back");
}

///////////////////////////////
////   Screen Updating    /////
///////////////////////////////
void updateLocation()
{
    if(GPS.fix)
    {
        double lon, lat;
        lat = (double)GPS.latitude_fixed / 10000000.0;
        lon = (double)GPS.longitude_fixed / 10000000.0;
        
        if(GPS.longitudeDegrees < 0.0)
        {
            lon = -lon;
        }
        
        if(GPS.latitudeDegrees < 0.0)
        {
            lat = -lat;
        }
        
        if(GPS.fixquality >= 1) // FIXME Quick and dirty
        {
            bestLat = lat;
            bestLon = lon;
        }
        newLocationReceived();
    }
}

void newLocationReceived()
{
    switch(currentScreen)
    {
        case MAIN_SCREEN:
            break;
        case GPS_SCREEN:
        {
            updateGridScreen();
            updateClockScreen();
            updateCoordScreen();
        }
            break;
        case SAT_SCREEN:
        {
            updateSatPosition();
            updateSatScreen();
        }
            break;
        default:
            break;
    }
}

void updateSatPosition()
{
    p13.setLocation(bestLon, bestLat, GPS.altitude);
    p13.setTime((GPS.year + 2000), GPS.month, GPS.day, GPS.hour, GPS.minute, GPS.seconds);
    readElements(satIndex);
    setFrequencies(satIndex);
    p13.calculate();
}

void nextSat()
{
    if(satIndex < (foundSats - 1))
        satIndex++;
    else
        satIndex = 0;
    
    switchToSat(satIndex);
}

void previousSat()
{
    if(satIndex > 0)
        satIndex--;
    else
        satIndex = (foundSats - 1);
    
    switchToSat(satIndex);
}

void switchToSat(int idx)
{
    String satData = selectedSatData(satIndex);
    selectedSat = getValue(satData, ',', 0);
    downlinkBaseFreq = getValue(satData, ',', 1).toInt();
    uplinkBaseFreq = getValue(satData, ',', 2).toInt();    
    downlinkMode = getValue(satData, ',', 3);
    uplinkMode = getValue(satData, ',', 4);
    description = getValue(satData, ',', 8);

    findTleForName(selectedSat);
    updateSatSelectionScreen();
}

void updateGridScreen()
{
    String gridString = gridSquare(bestLat, bestLon).substring(0,8);
    int16_t  x1, y1;
    uint16_t w, h;
    
    if(gridString != lastGrid)
    {
        tft.setFont(&FreeSansBold18pt7b);
        tft.getTextBounds(string2char(lastGrid), 0, 120, &x1, &y1, &w, &h);
        int cursorStart = (tft.width() / 2) - (w / 2);
        tft.fillRect(cursorStart, y1, w + 4, h, BG_COLOR);
        tft.getTextBounds(string2char(gridString), 0, 120, &x1, &y1, &w, &h);
        cursorStart = (tft.width() / 2) - (w / 2);
        tft.setCursor(cursorStart, 120);
        tft.setTextColor(ILI9341_BLACK);
        tft.println(gridString);
        lastGrid = gridString;
    }
}

void updateClockScreen()
{
    // Build date string
    String dateString = "";
    dateString += "20";
    dateString += GPS.year;
    dateString += "/";
    
    if(GPS.month < 10)
        dateString += "0";
    
    dateString += GPS.month;
    dateString += "/";
    
    if(GPS.day < 10)
        dateString += "0";
    
    dateString += GPS.day;
    
    // Build time string
    String timeString = "";
    if(GPS.hour < 10)
        timeString = "0";
    
    timeString += GPS.hour;
    timeString += ":";
    
    if(GPS.minute < 10)
        timeString += "0";
    
    timeString += GPS.minute;
    timeString += ":";

    if(GPS.seconds < 10)
        timeString += "0";
    
    timeString += GPS.seconds;

    timeString += "z";

    int16_t  x1, y1;
    uint16_t w, h;
    
    // Do we need to redraw?
    if(dateString != lastDate)
    {
        tft.setFont(&FreeSansBold18pt7b);
        tft.getTextBounds(string2char(lastDate), 0, 40, &x1, &y1, &w, &h);
        int cursorStart = (tft.width() / 2) - (w / 2);
        tft.fillRect(cursorStart, y1, w + 4, h, BG_COLOR);
        tft.getTextBounds(string2char(dateString), 0, 40, &x1, &y1, &w, &h);
        cursorStart = (tft.width() / 2) - (w / 2);
        tft.setCursor(cursorStart, 40);
        tft.setTextColor(ILI9341_BLACK);
        tft.println(dateString);
        lastDate = dateString;
    }
    
    if(timeString != lastTime)
    {
        tft.setFont(&FreeSansBold18pt7b);
        tft.getTextBounds(string2char(lastTime), 0, 80, &x1, &y1, &w, &h);
        int cursorStart = (tft.width() / 2) - (w / 2);
        tft.fillRect(cursorStart, y1, w + 4, h, BG_COLOR);
        tft.getTextBounds(string2char(timeString), 0, 80, &x1, &y1, &w, &h);
        cursorStart = (tft.width() / 2) - (w / 2);
        tft.setCursor(cursorStart, 80);
        tft.setTextColor(ILI9341_BLACK);
        tft.print(timeString);
        lastTime = timeString;
    }
}

void updateCoordScreen()
{
    String latString = String(fabs(bestLat), 5);
    String lonString = String(fabs(bestLon), 5);

    latString += (bestLat >= 0.0) ? " N" : " S";
    lonString += (bestLon >= 0.0) ? " E" : " W";

    int16_t  x1, y1;
    uint16_t w, h;
    
    if(lastLat != latString)
    {
        tft.setFont(&FreeSansBold12pt7b);
        tft.getTextBounds(string2char(lastLat), 0, 150, &x1, &y1, &w, &h);
        int cursorStart = (tft.width() / 2) - (w / 2);
        tft.fillRect(cursorStart, y1, w + 4, h, BG_COLOR);
        tft.getTextBounds(string2char(latString), 0, 150, &x1, &y1, &w, &h);
        cursorStart = (tft.width() / 2) - (w / 2);
        tft.setCursor(cursorStart, 150);
        tft.setTextColor(ILI9341_BLACK);
        tft.println(latString);
        lastLat = latString;
    }
    
    if(lastLon != lonString)
    {
        tft.setFont(&FreeSansBold12pt7b);
        tft.getTextBounds(string2char(lastLon), 0, 180, &x1, &y1, &w, &h);
        int cursorStart = (tft.width() / 2) - (w / 2);
        tft.fillRect(cursorStart, y1, w + 4, h, BG_COLOR);
        tft.getTextBounds(string2char(lonString), 0, 180, &x1, &y1, &w, &h);
        cursorStart = (tft.width() / 2) - (w / 2);
        tft.setCursor(cursorStart, 180);
        tft.setTextColor(ILI9341_BLACK);
        tft.println(lonString);
        lastLon = lonString;
    }
}

void updateSatSelectionScreen()
{
    int16_t  x1, y1;
    uint16_t w, h;
    if(lastSatName != selectedSat)
    {
        tft.setFont(&FreeSansBold18pt7b);
        tft.getTextBounds(string2char(lastSatName), 0, 32, &x1, &y1, &w, &h);
        int cursorStart = (tft.width() / 2) - (w / 2);
        tft.fillRect(cursorStart, y1, w + 4, h, BG_COLOR);
        tft.getTextBounds(string2char(selectedSat), 0, 32, &x1, &y1, &w, &h);
        cursorStart = (tft.width() / 2) - (w / 2);
        tft.setCursor(cursorStart, 32);
        tft.setTextColor(ILI9341_BLUE);
        tft.println(selectedSat);
        lastSatName = selectedSat;
    }

    if(description != lastDescription)
    {
        tft.setFont(&FreeSans12pt7b);
        tft.getTextBounds(string2char(lastDescription), 0, 60, &x1, &y1, &w, &h);
        int cursorStart = (tft.width() / 2) - (w/2);
        tft.fillRect(cursorStart, y1, w + 2, h, BG_COLOR);
        tft.getTextBounds(string2char(description), 0, 60, &x1, &y1, &w, &h);
        cursorStart = (tft.width() / 2) - (w/2);
        tft.setCursor(cursorStart, 60);
        tft.setTextColor(ILI9341_BLACK);
        tft.println(description);
        lastDescription = description;
    }

    ImageReturnCode stat;
    String filename = "/";
    filename += selectedSat;
    filename += ".bmp";
    stat = reader.drawBMP(string2char(filename), tft, 100, 80);
}

void updateSatScreen()
{
    int16_t  x1, y1;
    uint16_t w, h;
    if(lastSatName != selectedSat)
    {
        tft.setFont(&FreeSansBold18pt7b);
        tft.getTextBounds(string2char(lastSatName), 0, 32, &x1, &y1, &w, &h);
        int cursorStart = (tft.width() / 2) - (w / 2);
        tft.fillRect(cursorStart, y1, w + 4, h, BG_COLOR);
        tft.getTextBounds(string2char(selectedSat), 0, 32, &x1, &y1, &w, &h);
        cursorStart = (tft.width() / 2) - (w / 2);
        tft.setCursor(cursorStart, 32);
        tft.setTextColor(ILI9341_BLUE);
        tft.println(selectedSat);
        lastSatName = selectedSat;
    }

    if(description != lastDescription)
    {
        tft.setFont(&FreeSans12pt7b);
        tft.getTextBounds(string2char(lastDescription), 0, 60, &x1, &y1, &w, &h);
        int cursorStart = (tft.width() / 2) - (w/2);
        tft.fillRect(cursorStart, y1, w + 2, h, BG_COLOR);
        tft.getTextBounds(string2char(description), 0, 60, &x1, &y1, &w, &h);
        cursorStart = (tft.width() / 2) - (w/2);
        tft.setCursor(cursorStart, 60);
        tft.setTextColor(ILI9341_BLACK);
        tft.println(description);
        lastDescription = description;
    }

    uint32_t tmpUp = GPS.fix ? p13.txOutLong : uplinkBaseFreq;
    String uplink = " ";
    
    if(tmpUp > 0)
    {
        int tmpUpHz = (tmpUp % 1000);
        String upHz = "";
        if(tmpUpHz < 100)
        {
            upHz += "0";
            if(tmpUpHz < 10)
            {
                upHz += "0";
            }
        }
        upHz += String(tmpUpHz);
        tmpUp = tmpUp / 1000;
        
        String upkHz = "";
        int tmpUpkHz = (tmpUp % 1000);
        if(tmpUpkHz < 100)
        {
            upkHz += "0";
            if(tmpUpkHz < 10)
            {
                upkHz += "0";
            }
        }
        upkHz += String(tmpUpkHz);
        
        tmpUp = tmpUp / 1000;
        String upMHz = String(tmpUp);
        
        uplink = upMHz;
        uplink += ".";
        uplink += upkHz;
        uplink += ".";
        uplink += upHz;
        uplink += " ";
        uplink += uplinkMode;
    }
    
    if(lastUplink != uplink)
    {
        tft.setFont(&FreeSans12pt7b);
        tft.getTextBounds(string2char(lastUplink), 125, 90, &x1, &y1, &w, &h);
        tft.fillRect(x1, y1, w, h, BG_COLOR);
        tft.setCursor(125, 90);
        tft.setTextColor(UPLINK_COLOR);
        tft.println(uplink);
        lastUplink = uplink;
    }
    
    uint32_t tmpDown = GPS.fix ? p13.rxOutLong : downlinkBaseFreq;
    String downlink = "0";
    
    if(tmpDown > 0)
    {
        int tmpDownHz = (tmpDown % 1000);
        String downHz = "";
        if(tmpDownHz < 100)
        {
            downHz += "0";
            if(tmpDownHz < 10)
            {
                downHz += "0";
            }
        }
        downHz += String(tmpDownHz);
        tmpDown = tmpDown / 1000;
        
        int tmpDownkHz = (tmpDown % 1000);
        String downkHz = "";
        if(tmpDownkHz < 100)
        {
            downkHz += "0";
            if(tmpDownkHz < 10)
            {
                downkHz += "0";
            }
        }
        downkHz += String(tmpDownkHz);
        tmpDown = tmpDown / 1000;
        
        String downMHz = String(tmpDown);
        
        downlink = downMHz;
        downlink += ".";
        downlink += downkHz;
        downlink += ".";
        downlink += downHz;
        downlink += " ";
        downlink += downlinkMode;
    }
    
    if(lastDownlink != downlink)
    {
        tft.setFont(&FreeSans12pt7b);
        tft.getTextBounds(string2char(lastDownlink), 125, 120, &x1, &y1, &w, &h);
        tft.fillRect(x1, y1, w, h, BG_COLOR);
        tft.setCursor(125, 120);
        tft.setTextColor(DOWNLINK_COLOR);
        tft.println(downlink);
        lastDownlink = downlink;
    }
    
    String satEl = String(p13.EL, 1);
    String satAz = String(p13.AZ, 1);
    
    if(lastAz != satAz)
    {
        tft.setFont(&FreeSans12pt7b);
        tft.getTextBounds(string2char(lastAz), 125, 150, &x1, &y1, &w, &h);
        tft.fillRect(x1, y1, w, h, BG_COLOR);
        tft.setCursor(125, 150);
        tft.setTextColor(ILI9341_BLACK);
        tft.println(satAz);
        lastAz = satAz;
    }
    
    if(lastEl != satEl)
    {
        tft.setFont(&FreeSans12pt7b);
        tft.getTextBounds(string2char(lastEl), 125, 180, &x1, &y1, &w, &h);
        tft.fillRect(x1, y1, w, h, BG_COLOR);
        tft.setCursor(125, 180);
        tft.setTextColor(ILI9341_BLACK);
        tft.println(satEl);
        lastEl = satEl;
    }
}

// Satellite code

void setFrequencies(int idx)
{
    p13.setFrequency(downlinkBaseFreq, uplinkBaseFreq);
}

double getElement(char *gstr, int gstart, int gstop)
{
    double retval;
    int    k, glength;
    char   gestr[80];
    
    glength = gstop - gstart + 1;
    
    for (k = 0; k <= glength; k++)
    {
        gestr[k] = gstr[gstart+k-1];
    }
    
    gestr[glength] = '\0';
    retval = atof(gestr);
    return(retval);
}

void readElements(int x)//order in the array above
{
    // for example ...
    // char line1[] = "1 28375U 04025K   09232.55636497 -.00000001  00000-0 12469-4 0   4653";
    // char line2[] = "2 28375 098.0531 238.4104 0083652 290.6047 068.6188 14.40649734270229";
    
    p13.setElements(getElement(e1,19,20) + 2000, getElement(e1,21,32), getElement(e2,9,16),
                    getElement(e2,18,25), getElement(e2,27,33) * 1.0e-7, getElement(e2,35,42), getElement(e2,44,51), getElement(e2,53,63),
                    getElement(e1,34,43), (getElement(e2,64,68) + ONEPPM), 0);
}

// Scan through nasabare.txt
bool findTleForName(String satName)
{
    File nasaBare;
    
    // Open requested file on SD card
    if ((nasaBare = SD.open("nasabare.txt")) == NULL)
    {
        //Serial.print(F("File not found"));
        return false;
    }
    
    String tmpLine = "";
    
    int lineIdx = 0;
    
    while (nasaBare.available())
    {
        tmpLine = nasaBare.readStringUntil('\n');
        if (tmpLine == "")
        {//no blank lines are anticipated
            break;
        }
        if(lineIdx % 3 == 0)
        {
            if( tmpLine == satName)
            {
                nasaBare.readStringUntil('\n').toCharArray(e1, 70);
                nasaBare.readStringUntil('\n').toCharArray(e2, 70);
                nasaBare.close();
                return true;
            }
        }
        
        lineIdx++;
    }
    // close the file:
    nasaBare.close();
    return false;
}

String selectedSatData(int selected)
{
    String data = "";
    
    File doppler;
    
    // Open requested file on SD card
    if ((doppler = SD.open("Doppler.sqf")) == NULL)
    {
        //Serial.print(F("File not found"));
        return data;
    }
    
    int lineIdx = 0;
    
    while (doppler.available())
    {
        String list = doppler.readStringUntil('\n');
        
        if(lineIdx == selected)
        {
            data = list;
            // Hey, we found the one that we want to do something with
            doppler.close();
            return data;
        }
        lineIdx++; // Count the record
    }
    // close the file:
    doppler.close();
    return data;
}

int numOfSats()
{
    int num = 0;
    
    File doppler;
    
    // Open requested file on SD card
    if ((doppler = SD.open("Doppler.sqf")) == NULL)
    {
        //Serial.print(F("File not found"));
        return -1;
    }
    
    while (doppler.available())
    {
        doppler.readStringUntil('\n');
        num++; // Count the record
    }
    // close the file:
    doppler.close();
    return num;
}

// Geo conversions
String gridSquare(float latitude, float longitude)
{
    String grid;
    char m[11];
    const int pairs=5;
    const double scaling[]={360.,360./18.,(360./18.)/10., \
        ((360./18.)/10.)/24.,(((360./18.)/10.)/24.)/10., \
        ((((360./18.)/10.)/24.)/10.)/24., \
        (((((360./18.)/10.)/24.)/10.)/24.)/10.,
        ((((((360./18.)/10.)/24.)/10.)/24.)/10.)/24.};
    int i;
    int index;
    
    for (i=0;i<pairs;i++)
    {
        index = (int)floor(fmod((180.0+longitude), scaling[i])/scaling[i+1]);
        m[i*2] = (i&1) ? 0x30+index : (i&2 || i&5) ? 0x61+index : 0x41+index;
        index = (int)floor(fmod((90.0+latitude), (scaling[i]/2))/(scaling[i+1]/2));
        m[i*2+1] = (i&1) ? 0x30+index : (i&2 || i&5) ? 0x61+index : 0x41+index;
    }
    m[pairs*2]=0;
    
    grid = String(m);
    return grid;
}

//Convenience
char* string2char(String command)
{
    if(command.length()!=0){
        char *p = const_cast<char*>(command.c_str());
        return p;
    }
}

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length()-1;
    
    for(int i=0; i<=maxIndex && found<=index; i++)
    {
        if(data.charAt(i)==separator || i==maxIndex)
        {
            found++;
            strIndex[0] = strIndex[1]+1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    
    return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}
