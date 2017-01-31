
//
// ESP8266 Clock displayed on an OLED shield (64x48) using the Network Time Protocol to update every minute
// (c) D L Bird 2016
//
String   clock_version = "10.0";
#include <NTPClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <Ticker.h>    // Core library
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <WiFiManager.h>      //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_GFX.h>

// #include "TimeClient.h"

// Color definitions
#define BLACK    0x0000
#define BLUE     0x001F
#define RED      0xF800
#define GREEN    0x07E0
#define CYAN     0x07FF
#define MAGENTA  0xF81F
#define YELLOW   0xFFE0
#define WHITE    0xFFFF
#define GREY     0xC618
//wemos D1 icin
#define cs   0
#define dc   2
#define rst  15
Adafruit_ST7735 tft = Adafruit_ST7735(cs, dc, rst);
String day_of_week[7]    = {"PAZAR", "PAZARTESI", "SALI", "CARSAMBA", "PERSEMBE", "CUMA", "CUMARTESI"}; // Sunday is dayOfWeek 0
String month_of_year[12] = {"OCAK", "SUBAT", "MART", "NISAN", "MAYIS", "HAZIRAN", "TEMMUZ", "AGUSTOS", "EYLUL", "EKIM", "KASIM", "ARALIK"}; // January is month 0
String webpage           = "";

String APIKEY = "1b1ac338bbb911f1d248027ccca4a67ee";
String CityID = "1172451"; //lahore , pakista
//String CityID = "306571"; //konya ,

//int TimeZone = 4; //GMT +2

WiFiClient client;
WiFiUDP time_udp;
Ticker  screen_update;

// You can specify the time server source and time-zone offset in milli-seconds.
float TimeZone;
bool  AMPM = false;
int   epoch, local_epoch, current_year, current_month, current_day, dayOfWeek, hours, UTC_hours, minutes, seconds;
byte  set_status, alarm_HR, alarm_MIN, alarmed;
bool  DST = false, dstUK, dstUSA, dstAUS, alarm_triggered = false; // DayLightSaving on/off and selected zone indicators

char servername[] = "api.openweathermap.org"; // remote server we will connect to
String result;

boolean night = false;
int  counter = 360;
String weatherDescription = "";
String weatherLocation = "";
float Temperature;

extern  unsigned char  cloud[];
extern  unsigned char  thunder[];
extern  unsigned char  wind[];


#define output_pin D1 // the alarm pin that goes high when triggered, note this is the only spare Data pin

// If your country is ahead of UTC by 2-hours use 2, or behind UTC by 2-hours use -2
// If your country is 1-hour 30-mins ahead of UTC use 1.5, so use decimal hours
// Change your time server to a local one, but they all return UTC!
NTPClient timeClient(time_udp, "uk.pool.ntp.org", TimeZone * 3600, 60000); // (logical name, address of server, time-zone offset in seconds, refresh-rate in mSec)
//pk.pool.ntp.org
ESP8266WebServer server(80);  // Set the port you wish to use, a browser default is 80, but any port can be used, if you set it to 5555 then connect with http://nn.nn.nn.nn:5555/
void setup() {
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  EEPROM.begin(12);
  // TimeZone = 5;
  //  0 - TimeZone 4-bytes for floating point
  //  1
  //  2
  //  3
  //  4 - AMPM Mode true/false
  //  5 - set status
  //  6 - dst_UK  true/false
  //  7 - dst_USA true/false
  //  8 - dst_AUS true/false
  //  9 - Alarm Hours
  // 10 - Alarm Minutes
  // 11 - alarmed (nor not)
  EEPROM.get(0, TimeZone);
  EEPROM.get(4, AMPM);
  // A simple test of first CPU usage, if first time run the probability of these locations equating to 0 is low, so initilaise the timezone
  if (set_status != 0 ) TimeZone = 0;
  EEPROM.get(5, set_status);
  EEPROM.get(6, dstUK);
  EEPROM.get(7, dstUSA);
  EEPROM.get(8, dstAUS);
  EEPROM.get(9, alarm_HR);
  EEPROM.get(10, alarm_MIN);
  EEPROM.get(11, alarmed);
  //pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  //------------------------------
  //WiFiManager intialisation. Once completed there is no need to repeat the process on the current board
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab
  tft.fillScreen(BLACK);
  tft.setCursor(23, 50);
  tft.setTextSize(1);
  tft.setTextColor(GREEN);
  tft.print("AG ARANIYOR!!!");
  tft.setCursor(10, 80);
  tft.setTextColor(WHITE);
  tft.print("AG TANIMLAMAK ICIN");
  tft.setTextSize(1);
  tft.setCursor(18, 100);
  tft.setTextColor(RED);
  tft.print("HavaISTASYONU'na");
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setCursor(30, 120);
  tft.print("BAGLANINIZ");

  WiFiManager wifiManager;

  wifiManager.autoConnect("HavaISTASYONU");
  tft.fillScreen(BLACK);
  tft.setCursor(30, 70);
  tft.setTextColor(GREEN);
  tft.setTextSize(1);
  tft.print("AG BULUNDU");
  tft.setCursor(25, 85);
  tft.setTextColor(RED);
  tft.print("BAGLANIYOR...");

  // A new OOB ESP8266 will have no credentials, so will connect and not need this to be uncommented and compiled in, a used one will, try it to see how it works
  // Uncomment the next line for a new device or one that has not connected to your Wi-Fi before or you want to reset the Wi-Fi connection
  // Then restart the ESP8266 and connect your PC to the wireless access point called 'ESP8266_AP' or whatever you call it below
  // wifiManager.resetSettings();
  // Next connect to http://192.168.4.1/ and follow instructions to make the WiFi connection

  // Set a timeout until configuration is turned off, useful to retry or go to sleep in n-seconds
  wifiManager.setTimeout(180);

  //fetches ssid and password and tries to connect, if connections succeeds it starts an access point with the name called "ESP8266_AP" and waits in a blocking loop for configuration

  // At this stage the WiFi manager will have successfully connected to a network, or if not will try again in 180-seconds
  //------------------------------
  // Print the IP address

  timeClient.begin(); // Start the NTP service for time

  tft.fillScreen(BLACK);

  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.print("Synching..");
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
 // display_ip();
  //display.display(); // Update screen
  screen_update.attach_ms(1000, display_time); // Call display update routine every 1-sec. Tick units are 1.024mS adjust as required, I've used 1000mS, but should be 976 x 1.024 = 1-sec
  timeClient.update();                         // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
  epoch   = timeClient.getEpochTime();         // You can test moving time forward or back by adding or subtracting the required number of seconds 1-day = 24*60*60
  seconds = epoch % 60;

  server.begin();  // Start the webserver
  Serial.println("Webserver started...");

  server.on("/", NTP_Clock_home_page);
  server.on("/AMPM_ONOFF", AMPM_ONOFF);         // Define what happens when a client requests attention
  server.on("/TIMEZONE_PLUS", timezone_plus);   // Define what happens when a client requests attention
  server.on("/TIMEZONE_MINUS", timezone_minus); // Define what happens when a client requests attention
  server.on("/DST_UK",  dst_UK);                // Define what happens when a client requests attention
  server.on("/DST_USA", dst_USA);               // Define what happens when a client requests attention
  server.on("/DST_AUS", dst_AUS);               // Define what happens when a client requests attention
  server.on("/ALARM_PLUS",  alarm_PLUS);        // Define what happens when a client requests attention
  server.on("/ALARM_MINUS", alarm_MINUS);       // Define what happens when a client requests attention
  server.on("/ALARM_RESET", alarm_RESET);       // Define what happens when a client requests attention
  server.on("/ALARM_ONOFF", alarm_ONOFF);       // Define what happens when a client requests attention
  server.on("/RESET_VALUES", reset_values);     // Define what happens when a client requests attention
  server.on("/EXIT_SETUP", exit_setup);         // Define what happens when a client requests attention

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
     tft.fillRect(0, 45, 128, 160, BLACK);
   // tft.fillScreen(BLACK);
    tft.setCursor(25, 50);
    tft.setTextSize(2);
    tft.setTextColor(YELLOW);
    tft.print("PROGRAM");
    tft.setCursor(30, 85);
    tft.setTextColor(RED);
    tft.setTextSize(1);
    tft.print("GUNCELLENDI");
    tft.setCursor(15, 110);
    tft.setTextColor(GREEN);
    tft.setTextSize(1);
    tft.print("LUTFEN BEKLEYINIZ");
    delay(3000);

  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
     tft.fillRect(0, 45, 128, 160, BLACK);
   // tft.fillScreen(BLACK);
    tft.setCursor(25, 50);
    tft.setTextSize(2);
    tft.setTextColor(YELLOW);
    tft.print("PROGRAM");
    tft.setCursor(20, 90);
    tft.setTextColor(RED);
    tft.setTextSize(1);
    tft.print("GUNCELLENIYOR...");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

}

void loop() {
  ArduinoOTA.handle();
  if (counter == 360) //Get new data every 30 minutes
  {
    counter = 0;
    getWeatherData();

  } else
  {
    counter++;
    delay(5000);
    Serial.println(counter);
  }
  if (millis() % 300000 == 0) {
    timeClient.update();                 // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
    epoch   = timeClient.getEpochTime(); // You can test moving time forward or back by adding or subtracting the required number of seconds 1-day = 24*60*60
    seconds = epoch % 60;
  }
  server.handleClient();   // Wait for a client to connect and when they do process their requests

}

//Program comes here under interrupt control every 1-sec
void display_time() { // Note Ticker called routines cannot get a time update using timeclient.update()!
  epoch          = epoch + 1;
  UTC_hours      = epoch % 86400 / 3600;
  if (DST) epoch = epoch + 3600;
  epoch = epoch + int(TimeZone * 3600);
  calcDate();
  epoch = epoch - int(TimeZone * 3600);
  if (DST) epoch = epoch - 3600;
  DST = false; // Until calculated otherwise as follows:
  if (dstUK) { // For the UK DST begins at 0100 on the last Sunday in March and ends at 0200 on the last Sunday in October
    if ((current_month > 2 && current_month < 9 ) ||
        (current_month == 2 && ((current_day - dayOfWeek) >= 25) &&  dayOfWeek == 0 && hours >= 1) ||
        (current_month == 2 && ((current_day - dayOfWeek) >= 25) &&  dayOfWeek >  0) ||
        (current_month == 9 && ((current_day - dayOfWeek) <= 24) &&  dayOfWeek >  0) ||
        (current_month == 9 && ((current_day - dayOfWeek) <= 24) && (dayOfWeek == 0 && hours <= 2))) DST = true;
  }
  if (dstUSA) { // For the USA DST begins at 0200 on the second Sunday of March and ends on the first Sunday of November at 0200
    if ((current_month  > 2 &&   current_month < 10 ) ||
        (current_month == 2 &&  (current_day - dayOfWeek) >= 12 &&  dayOfWeek == 0 && hours > 2) ||
        (current_month == 2 &&  (current_day - dayOfWeek) >= 12 &&  dayOfWeek >  0) ||
        (current_month == 10 && (current_day - dayOfWeek) <= 5  &&  dayOfWeek >  0) ||
        (current_month == 10 && (current_day - dayOfWeek) <= 5  && (dayOfWeek == 0 && hours < 2))) DST = true;
  }
  if (dstAUS) { // For Australia DST begins on the first Sunday of October @ 0200 and ends the first Sunday in April @ 0200
    if ((current_month  > 9 || current_month < 3 ) ||
        (current_month == 3 && (current_day - dayOfWeek) <= 0  &&  dayOfWeek == 0 && hours < 3) ||
        (current_month == 3 && (current_day - dayOfWeek) <= 0  &&  dayOfWeek >  0) ||
        (current_month == 9 && (current_day - dayOfWeek) <= 29 &&  dayOfWeek >  0) ||
        (current_month == 9 && (current_day - dayOfWeek) <= 29 && (dayOfWeek == 0 && hours > 1))) DST = true;
  }

  //   tft.fillScreen(BLACK);
  tft.fillRect(0, 0, 128, 43, BLACK);
  tft.setTextColor(GREEN);
  tft.setTextSize(1); // Display size is 10 x 6 characters when set to size=1 where font size is 6x6 pixels
  tft.setCursor((128 - day_of_week[dayOfWeek].length() * 6) / 2, 3); // Display size is 10 characters per line and 6 rows when set to size=1 when font size is 6x6 pixels
  tft.println(day_of_week[dayOfWeek]); // Extract and print day of week

  tft.setCursor(35, 16); // centre date display
  tft.print((current_day < 10 ? "0" : "") + String(current_day) + "-" + month_of_year[current_month] + "-" + String(current_year).substring(2)); // print Day-Month-Year
  tft.setTextSize(2);  // Increase text size for time display
  tft.setTextColor(WHITE);
  tft.setCursor(25, 29); // Move dayOfWeekn a litle and remember location is in pixels not lines!
  if (AMPM) {
    if (hours % 12 == 0) tft.print("12");
    else
    {
      if (hours % 12 < 10) tft.print(" " + String(hours % 12)); else tft.print(String(hours % 12));
    }
  } else if ((hours < 10) || (hours % 24 == 0)) tft.print("0" + String(hours % 24)); else tft.print(String(hours % 24));

  tft.print((minutes < 10) ? ":0" + String(minutes % 60) : ":" + String(minutes % 60));
  //tft.fillRect(85,33,20,10,BLACK); // Clear the portion of screen that displays seconds (nn) ready for next update
  tft.setTextSize(1);   // Reduce text size to fit in the remaining screen area

  if (AMPM) tft.setCursor(85, 35); else tft.setCursor(85, 35); // Move dayOfWeekn to a position that can accomodate seconds display
  tft.print(":"); tft.print((seconds < 10) ? "0" + String(seconds) : String(seconds)); tft.print(" ");
  if (AMPM) {
    if (hours % 24 < 12) tft.print("AM");
    else tft.print("PM");
  }
  tft.setCursor(12, 41); // Move dayOfWeek to a position that can accomodate seconds display
  /*  if (DST)    tft.print("DST-"); else tft.print("UTC-");
    if (dstUK)  tft.print("UK");
    if (dstUSA) tft.print("USA");
    if (dstAUS) tft.print("AUS"); */



  if (hours == alarm_HR && minutes == alarm_MIN && alarmed) {
    if (alarm_triggered) {
      alarm_triggered = false;
      tft.setCursor(13, 32); tft.print("*");
      tft.setCursor(42, 32); tft.print("*");
      //     pinMode(output_pin, OUTPUT);   // Set D1 pin LOW when alarm goes off
      //     digitalWrite(output_pin,LOW);
    }
    else
    {
      alarm_triggered = true;
      tft.setCursor(13, 32); tft.print(" ");
      tft.setCursor(42, 32); tft.print(" ");
      //     pinMode(output_pin, OUTPUT);   // Set D1 pin HIGH when alarm goes on
      //     digitalWrite(output_pin,HIGH);
    }
  }
  /* if (alarmed) {
    tft.setCursor(0,32); tft.print("A");
    tft.setCursor(0,41); tft.print("S");
    } else
    {
     tft.fillRect(0,32,10,20,BLACK);
    } */
  //display.display(); //Update the screen
}

void calcDate() {
  uint32_t cDyear_day;
  seconds      = epoch;
  minutes      = seconds / 60; /* calculate minutes */
  seconds     -= minutes * 60; /* calcualte seconds */
  hours        = minutes / 60; /* calculate hours */
  minutes     -= hours   * 60;
  current_day  = hours   / 24; /* calculate days */
  hours       -= current_day * 24;
  current_year = 1970;         /* Unix time starts in 1970 on a Thursday */
  dayOfWeek    = 4;
  while (1) {
    bool     leapYear   = (current_year % 4 == 0 && (current_year % 100 != 0 || current_year % 400 == 0));
    uint16_t daysInYear = leapYear ? 366 : 365;
    if (current_day >= daysInYear) {
      dayOfWeek += leapYear ? 2 : 1;
      current_day   -= daysInYear;
      if (dayOfWeek >= 7) dayOfWeek -= 7;
      ++current_year;
    }
    else
    {
      cDyear_day  = current_day;
      dayOfWeek  += current_day;
      dayOfWeek  %= 7;
      /* calculate the month and day */
      static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      for (current_month = 0; current_month < 12; ++current_month) {
        uint8_t dim = daysInMonth[current_month];
        if (current_month == 1 && leapYear) + dim; /* add a day to feburary if this is a leap year */
        if (current_day >= dim) current_day -= dim;
        else break;
      }
      break;
    }
  }
}

void NTP_Clock_home_page () {
  update_webpage();
  server.send(200, "text/html", webpage);
}

// A short method of adding the same web page header to some text
void append_webpage_header() {
  // webpage is a global variable
  webpage = ""; // A blank string variable to hold the web page
  webpage += "<!DOCTYPE html><html><head><title>ESP8266 NTP Clock</title>";
  webpage += "<style>";
  webpage += "#header  {background-color:#6A6AE2; font-family:tahoma; width:1280px; padding:10px; color:white; text-align:center; }";
  webpage += "#section {background-color:#E6E6FA; font-family:tahoma; width:1280px; padding:10px; color_blue;  font-size:22px; text-align:center;}";
  webpage += "#footer  {background-color:#6A6AE2; font-family:tahoma; width:1280px; padding:10px; color:white; font-size:14px; clear:both; text-align:center;}";
  webpage += "</style></head><body>";
}

void update_webpage() {
  append_webpage_header();
  webpage += "<div id=\"header\"><h1>NTP OLED Clock Setup " + clock_version + "</h1></div>";
  webpage += "<div id=\"section\"><h2>Time Zone, AM-PM and DST Mode Selection</h2>";
  webpage += "[AM-PM Mode: ";
  if (AMPM) webpage += "ON]"; else webpage += "OFF]";
  webpage += "&nbsp&nbsp&nbsp";
  webpage += "[DST Mode: ";
  if (dstUK) webpage += "UK]"; else if (dstUSA) webpage += "USA]"; else if (dstAUS) webpage += "AUS]";
  webpage += "&nbsp&nbsp&nbsp";
  webpage += "[Time Zone: ";
  if (TimeZone > -1 && TimeZone < 0) webpage += "-";
  webpage += String(int(TimeZone)) + ":";
  if (abs(round(60 * (TimeZone - int(TimeZone)))) == 0) webpage += "00]"; else webpage += "30]";
  webpage += "&nbsp&nbsp&nbsp";
  webpage += "[Alarm Time: ";
  if (alarm_HR < 10) webpage += "0";
  webpage += String(alarm_HR) + ":";
  if (alarm_MIN < 10) webpage += "0";
  webpage += String(alarm_MIN) + "]";
  webpage += "&nbsp&nbsp&nbsp";
  webpage += "[Alarm: ";
  if (alarmed) webpage += "ON]"; else webpage += "OFF]";

  webpage += "<p><a href=\"AMPM_ONOFF\">AM-PM Mode ON/OFF</a></p>";

  webpage += "<a href=\"TIMEZONE_PLUS\">Time Zone Plus 30-mins</a>";
  webpage += "&nbsp&nbsp&nbsp&nbsp";
  webpage += "<a href=\"TIMEZONE_MINUS\">Time Zone Minus 30-mins</a>";

  webpage += "<p><a href=\"DST_UK\">DST UK  Mode</a>";
  webpage += "&nbsp&nbsp&nbsp&nbsp";
  webpage += "<a href=\"DST_USA\">DST USA Mode</a>";
  webpage += "&nbsp&nbsp&nbsp&nbsp";
  webpage += "<a href=\"DST_AUS\">DST AUS Mode</a></p>";

  webpage += "<p><a href=\"ALARM_PLUS\">Alarm Time +</a>";
  webpage += "&nbsp&nbsp&nbsp&nbsp";
  webpage += "<a href=\"ALARM_MINUS\">Alarm Time -</a>";
  webpage += "&nbsp&nbsp&nbsp&nbsp";
  webpage += "<a href=\"ALARM_RESET\">Alarm Reset</a>";
  webpage += "&nbsp&nbsp&nbsp&nbsp";
  webpage += "<a href=\"ALARM_ONOFF\">Alarm ON/OFF</a></p>";

  webpage += "<p><a href=\"RESET_VALUES\">Reset clock parameters</a></p>";
  webpage += "<p><a href=\"EXIT_SETUP\">Run the clock</a></p>";

  webpage += "</div>";
  webpage += "<div id=\"footer\">Copyright &copy; D L Bird 2016</div>";
}

void timezone_plus() {
  TimeZone += 0.5;
  EEPROM.put(0, TimeZone);
  EEPROM.put(5, 0); // set_status to a known value
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void timezone_minus() {
  TimeZone -= 0.5;
  EEPROM.put(0, TimeZone);
  EEPROM.put(5, 0); // set_status to a known value
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void AMPM_ONOFF() {
  if (AMPM) AMPM = false; else AMPM = true;
  EEPROM.put(4, AMPM);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void dst_UK() {
  dstUK  = true;
  dstUSA = false;
  dstAUS = false;
  EEPROM.put(6, dstUK);
  EEPROM.put(7, dstUSA);
  EEPROM.put(8, dstAUS);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void dst_USA() {
  dstUK  = false;
  dstUSA = true;
  dstAUS = false;
  EEPROM.put(6, dstUK);
  EEPROM.put(7, dstUSA);
  EEPROM.put(8, dstAUS);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void dst_AUS() {
  dstUK  = false;
  dstUSA = false;
  dstAUS = true;
  EEPROM.put(6, dstUK);
  EEPROM.put(7, dstUSA);
  EEPROM.put(8, dstAUS);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void alarm_PLUS() {
  alarm_MIN += 2;
  if (alarm_MIN >= 60) {
    alarm_MIN = 00;
    alarm_HR += 1;
  }
  EEPROM.put(9, alarm_HR);
  EEPROM.put(10, alarm_MIN);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void alarm_MINUS() {
  alarm_MIN -= 2;
  if (alarm_MIN = 00 || alarm_MIN > 59) { // It's an unsigned byte! so 0 - 1 = 255
    alarm_MIN = 59;
    alarm_HR -= 1;
  }
  EEPROM.put(9, alarm_HR);
  EEPROM.put(10, alarm_MIN);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void alarm_RESET() {
  alarm_HR  = 06;
  alarm_MIN = 30; // Default alarm time of 06:30
  alarmed   = false;
  alarm_triggered = false;
  EEPROM.put(9, alarm_HR);
  EEPROM.put(10, alarm_MIN);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void alarm_ONOFF() {
  if (alarmed) alarmed = false; else alarmed = true;
  EEPROM.put(11, alarmed);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void reset_values() {
  AMPM       = false;
  TimeZone   = 0;
  set_status = 0;
  dstUK      = true;
  dstUSA     = false;
  dstAUS     = false;
  alarm_HR   = 06;
  alarm_MIN  = 30;
  alarmed    = false;
  EEPROM.put(0, TimeZone);
  EEPROM.put(4, AMPM);
  EEPROM.put(5, set_status);
  EEPROM.put(6, dstUK);
  EEPROM.put(7, dstUSA);
  EEPROM.put(8, dstAUS);
  EEPROM.put(9, alarm_HR);
  EEPROM.put(10, alarm_MIN);
  EEPROM.put(11, alarmed);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void exit_setup() {
  screen_update.attach_ms(60000, display_time); // Call display routine every 1-sec. Tick units are 1.024mS
  timeClient.update();                 // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
  epoch   = timeClient.getEpochTime(); // You can test moving time forward or back by adding or subtracting the required number of seconds 1-day = 24*60*60
  seconds = 60; // Set to trigger first time update
  seconds = epoch % 60;
  epoch = timeClient.getEpochTime();
  display_time();
  server.send(200, "text/html", webpage);
}

void display_ip() {
  // Print the IP address
  tft.fillScreen(BLACK);
  tft.setCursor(35, 10); // centre date display
  tft.print("Connect:");
  tft.setCursor(2, 20); // centre date display
  tft.print("http://");
  tft.print(WiFi.localIP());
  tft.print("/");
  //display.display();
  delay(2500);
  tft.fillScreen(BLACK);
}





void getWeatherData() //client function to send/receive GET request data.
{
  Serial.println("Getting Weather Data");
  if (client.connect(servername, 80)) {  //starts client connection, checks for connection
    client.println("GET /data/2.5/forecast?id=" + CityID + "&units=metric&cnt=1&APPID=" + APIKEY);
    client.println("Host: api.openweathermap.org");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();
  }
  else {
    Serial.println("connection failed"); //error message if no client connect
    Serial.println();
  }

  while (client.connected() && !client.available()) delay(1); //waits for data

  Serial.println("Waiting for data");

  while (client.connected() || client.available()) { //connected or data available
    char c = client.read(); //gets byte from ethernet buffer
    result = result + c;
  }

  client.stop(); //stop client
  result.replace('[', ' ');
  result.replace(']', ' ');
  Serial.println(result);

  char jsonArray [result.length() + 1];
  result.toCharArray(jsonArray, sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';

  StaticJsonBuffer<1024> json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);
  if (!root.success())
  {
    Serial.println("parseObject() failed");
  }

  String location = root["city"]["name"];
  String temperature = root["list"]["main"]["temp"];
  String weather = root["list"]["weather"]["main"];
  String description = root["list"]["weather"]["description"];
  String idString = root["list"]["weather"]["id"];
  String timeS = root["list"]["dt_txt"];

  //timeS = convertGMTTimeToLocal(timeS);

  int length = temperature.length();
  if (length == 5)
  {
    temperature.remove(length - 1);
  }

  Serial.println(location);
  Serial.println(weather);
  Serial.println(temperature);
  Serial.println(description);
  Serial.println(temperature);
  Serial.println(timeS);

  clearScreen();

  int weatherID = idString.toInt();
  printData(timeS, temperature, timeS, weatherID);

}

void printData(String timeString, String temperature, String time, int weatherID)
{
  /*  tft.setCursor(35, 20);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.print(timeString);
  */
  tft.fillRect(0, 45, 128, 160, BLACK);
  printWeatherIcon(weatherID);

  tft.setCursor(27, 132);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.print(temperature);

  tft.setCursor(83, 130);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.print("o");
  tft.setCursor(93, 132);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.print("C");

  // display_time();

}


void printWeatherIcon(int id)
{
  switch (id)
  {
    case 800: drawClearWeather(); break;
    case 801: drawFewClouds(); break;
    case 802: drawFewClouds(); break;
    case 803: drawCloud(); break;
    case 804: drawCloud(); break;

    case 200: drawThunderstorm(); break;
    case 201: drawThunderstorm(); break;
    case 202: drawThunderstorm(); break;
    case 210: drawThunderstorm(); break;
    case 211: drawThunderstorm(); break;
    case 212: drawThunderstorm(); break;
    case 221: drawThunderstorm(); break;
    case 230: drawThunderstorm(); break;
    case 231: drawThunderstorm(); break;
    case 232: drawThunderstorm(); break;

    case 300: drawLightRain(); break;
    case 301: drawLightRain(); break;
    case 302: drawLightRain(); break;
    case 310: drawLightRain(); break;
    case 311: drawLightRain(); break;
    case 312: drawLightRain(); break;
    case 313: drawLightRain(); break;
    case 314: drawLightRain(); break;
    case 321: drawLightRain(); break;

    case 500: drawLightRainWithSunOrMoon(); break;
    case 501: drawLightRainWithSunOrMoon(); break;
    case 502: drawLightRainWithSunOrMoon(); break;
    case 503: drawLightRainWithSunOrMoon(); break;
    case 504: drawLightRainWithSunOrMoon(); break;
    case 511: drawLightRain(); break;
    case 520: drawModerateRain(); break;
    case 521: drawModerateRain(); break;
    case 522: drawHeavyRain(); break;
    case 531: drawHeavyRain(); break;

    case 600: drawLightSnowfall(); break;
    case 601: drawModerateSnowfall(); break;
    case 602: drawHeavySnowfall(); break;
    case 611: drawLightSnowfall(); break;
    case 612: drawLightSnowfall(); break;
    case 615: drawLightSnowfall(); break;
    case 616: drawLightSnowfall(); break;
    case 620: drawLightSnowfall(); break;
    case 621: drawModerateSnowfall(); break;
    case 622: drawHeavySnowfall(); break;

    case 701: drawFog(); break;
    case 711: drawFog(); break;
    case 721: drawFog(); break;
    case 731: drawFog(); break;
    case 741: drawFog(); break;
    case 751: drawFog(); break;
    case 761: drawFog(); break;
    case 762: drawFog(); break;
    case 771: drawFog(); break;
    case 781: drawFog(); break;

    default: break;
  }
}

String convertGMTTimeToLocal(String timeS)
{
  int length = timeS.length();
  timeS = timeS.substring(length - 8, length - 6);
  int time = timeS.toInt();
  time = time + TimeZone;

  if (time > 21 ||  time < 7)
  {
    night = true;
  } else
  {
    night = false;
  }
  timeS = String(time) + ":00";
  return timeS;
}


void clearScreen()
{
  tft.fillScreen(BLACK);
}

void drawClearWeather()
{
  if (night)
  {
    drawTheMoon();
  } else
  {
    drawTheSun();
  }
}

void drawFewClouds()
{
  if (night)
  {
    drawCloudAndTheMoon();
  } else
  {
    drawCloudWithSun();
  }
}

void drawTheSun()
{
  tft.fillCircle(64, 80, 26, YELLOW);
}

void drawTheFullMoon()
{
  tft.fillCircle(64, 80, 26, GREY);
}

void drawTheMoon()
{
  tft.fillCircle(64, 80, 26, GREY);
  tft.fillCircle(75, 73, 26, BLACK);
}

void drawCloud()
{
  tft.drawBitmap(0, 35, cloud, 128, 90, GREY);
}

void drawCloudWithSun()
{
  tft.fillCircle(73, 70, 20, YELLOW);
  tft.drawBitmap(0, 36, cloud, 128, 90, BLACK);
  tft.drawBitmap(0, 40, cloud, 128, 90, GREY);
}

void drawLightRainWithSunOrMoon()
{
  if (night)
  {
    drawCloudTheMoonAndRain();
  } else
  {
    drawCloudSunAndRain();
  }
}

void drawLightRain()
{
  tft.drawBitmap(0, 35, cloud, 128, 90, GREY);
  tft.fillRoundRect(50, 105, 3, 13, 1, BLUE);
  tft.fillRoundRect(65, 105, 3, 13, 1, BLUE);
  tft.fillRoundRect(80, 105, 3, 13, 1, BLUE);
}

void drawModerateRain()
{
  tft.drawBitmap(0, 35, cloud, 128, 90, GREY);
  tft.fillRoundRect(50, 105, 3, 15, 1, BLUE);
  tft.fillRoundRect(57, 102, 3, 15, 1, BLUE);
  tft.fillRoundRect(65, 105, 3, 15, 1, BLUE);
  tft.fillRoundRect(72, 102, 3, 15, 1, BLUE);
  tft.fillRoundRect(80, 105, 3, 15, 1, BLUE);
}

void drawHeavyRain()
{
  tft.drawBitmap(0, 35, cloud, 128, 90, GREY);
  tft.fillRoundRect(43, 102, 3, 15, 1, BLUE);
  tft.fillRoundRect(50, 105, 3, 15, 1, BLUE);
  tft.fillRoundRect(57, 102, 3, 15, 1, BLUE);
  tft.fillRoundRect(65, 105, 3, 15, 1, BLUE);
  tft.fillRoundRect(72, 102, 3, 15, 1, BLUE);
  tft.fillRoundRect(80, 105, 3, 15, 1, BLUE);
  tft.fillRoundRect(87, 102, 3, 15, 1, BLUE);
}

void drawThunderstorm()
{
  tft.drawBitmap(0, 40, thunder, 128, 90, YELLOW);
  tft.drawBitmap(0, 35, cloud, 128, 90, GREY);
  tft.fillRoundRect(48, 102, 3, 15, 1, BLUE);
  tft.fillRoundRect(55, 102, 3, 15, 1, BLUE);
  tft.fillRoundRect(74, 102, 3, 15, 1, BLUE);
  tft.fillRoundRect(82, 102, 3, 15, 1, BLUE);
}

void drawLightSnowfall()
{
  tft.drawBitmap(0, 30, cloud, 128, 90, GREY);
  tft.fillCircle(50, 100, 3, GREY);
  tft.fillCircle(65, 103, 3, GREY);
  tft.fillCircle(82, 100, 3, GREY);
}

void drawModerateSnowfall()
{
  tft.drawBitmap(0, 35, cloud, 128, 90, GREY);
  tft.fillCircle(50, 105, 3, GREY);
  tft.fillCircle(50, 115, 3, GREY);
  tft.fillCircle(65, 108, 3, GREY);
  tft.fillCircle(65, 118, 3, GREY);
  tft.fillCircle(82, 105, 3, GREY);
  tft.fillCircle(82, 115, 3, GREY);
}

void drawHeavySnowfall()
{
  tft.drawBitmap(0, 35, cloud, 128, 90, GREY);
  tft.fillCircle(40, 105, 3, GREY);
  tft.fillCircle(52, 105, 3, GREY);
  tft.fillCircle(52, 115, 3, GREY);
  tft.fillCircle(65, 108, 3, GREY);
  tft.fillCircle(65, 118, 3, GREY);
  tft.fillCircle(80, 105, 3, GREY);
  tft.fillCircle(80, 115, 3, GREY);
  tft.fillCircle(92, 105, 3, GREY);
}

void drawCloudSunAndRain()
{
  tft.fillCircle(73, 70, 20, YELLOW);
  tft.drawBitmap(0, 32, cloud, 128, 90, BLACK);
  tft.drawBitmap(0, 35, cloud, 128, 90, GREY);
  tft.fillRoundRect(50, 105, 3, 13, 1, BLUE);
  tft.fillRoundRect(65, 105, 3, 13, 1, BLUE);
  tft.fillRoundRect(80, 105, 3, 13, 1, BLUE);
}

void drawCloudAndTheMoon()
{
  tft.fillCircle(94, 60, 18, GREY);
  tft.fillCircle(105, 53, 18, BLACK);
  tft.drawBitmap(0, 32, cloud, 128, 90, BLACK);
  tft.drawBitmap(0, 35, cloud, 128, 90, GREY);
}

void drawCloudTheMoonAndRain()
{
  tft.fillCircle(94, 60, 18, GREY);
  tft.fillCircle(105, 53, 18, BLACK);
  tft.drawBitmap(0, 32, cloud, 128, 90, BLACK);
  tft.drawBitmap(0, 35, cloud, 128, 90, GREY);
  tft.fillRoundRect(50, 105, 3, 11, 1, BLUE);
  tft.fillRoundRect(65, 105, 3, 11, 1, BLUE);
  tft.fillRoundRect(80, 105, 3, 11, 1, BLUE);
}

void drawWind()
{
  tft.drawBitmap(0, 35, wind, 128, 90, GREY);
}

void drawFog()
{
  tft.fillRoundRect(45, 60, 40, 4, 1, GREY);
  tft.fillRoundRect(40, 70, 50, 4, 1, GREY);
  tft.fillRoundRect(35, 80, 60, 4, 1, GREY);
  tft.fillRoundRect(40, 90, 50, 4, 1, GREY);
  tft.fillRoundRect(45, 100, 40, 4, 1, GREY);
}

void clearIcon()
{
  tft.fillRect(0, 40, 128, 100, BLACK);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab
  tft.fillScreen(BLACK);
  tft.setCursor(18, 60);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setTextColor(RED);
  tft.print("ACCESS POINT OLARAK ACILDI..");

}


