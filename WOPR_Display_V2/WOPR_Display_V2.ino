/***************************************************
  War Games - W.O.P.R. Missile Codes
  2024 UNexpected Maker
  Licensed under MIT Open Source

  This code is designed specifically to run on an ESP32. It uses features only
  available on the ESP32 like RMT and ledcSetup.

  W.O.P.R is available here...

  https://unexpectedmaker.com/shop/wopr-missile-launch-code-display-kit
  https://www.tindie.com/products/seonr/wopr-missile-launch-code-display-kit/

  Wired up for use with the TinyPICO, TinyS2 or TinyS3 Development Boards & TinyPICO Analog Audio Shield

  All products also available on my own store

  http://unexpectedmaker.com/shop/

  Changes:
  June 7, 2024 - code updated to support breaking changes in ESP32 Arduino Core v3.x and later.

 ****************************************************/

// Un-comment this line if using the new HAXORZ PCB revision
#define HAXORZ_EDITION 1

#include <Adafruit_GFX.h> // From Library Manager
#include "Adafruit_LEDBackpack.h" // From Library Manager
#include "OneButton.h" // From Library Manager
#include "ESPFlash_Mod.h"
#include <WiFi.h>
#include "time.h"
#include "secret.h"
#include "rmt.h"
#include <Arduino.h>
#include "utc_offset.h"

// Defines
#ifndef _BV
#define _BV(bit) (1<<(bit))
#endif

#define ELEMENTS(x)   (sizeof(x) / sizeof(x[0]))

#if defined(ARDUINO_TINYS3)

#define BUT1 2 // front lef button
#define BUT2 3 // front right button
#define RGBLED 4 // RGB LED Strip
#define DAC 21 // Audio Out

#ifdef HAXORZ_EDITION
#define BUT3 7 // back top button
#define BUT4 6 // back bottom button
#endif

#elif defined(ARDUINO_TINYS2)

#define BUT1 5 // front lef button
#define BUT2 6 // front right button
#define RGBLED 7 // RGB LED Strip
#define DAC 18 // Audio Out

#ifdef HAXORZ_EDITION
#define BUT3 38 // back top button
#define BUT4 33 // back bottom button
#endif

#else

#define BUT1 14 // front lef button
#define BUT2 15 // front right button
#define RGBLED 27 // RGB LED Strip
#define DAC 25 // Audio Out

#ifdef HAXORZ_EDITION
#define BUT3 32 // back top button
#define BUT4 33 // back bottom button
#endif

#endif

// User settings
// User settable countdown (seonds) for auto clock mode when in menu
// Set 0 to be off
uint8_t settings_clockCountdownTime = 60;
// User settable GMT value
int settings_GMT = 0;
// User settable Daylight Savings state
bool settings_24H = false;
// User settable Daylight Savings state
bool settings_DimAtNight = false;
// User settable show RGB LEDs when the clock is displayed
int settings_ClockRGB = 50;
// User settable display brightness
uint8_t settings_displayBrightness = 15;
// User settable clock separator
uint8_t settings_separator = 0; // 0 is " ", 1 is "-", 2 is "_"

// NTP Wifi Time
const char* ntpServer = "pool.ntp.org";
bool didChangeClockSettings = false;
bool hasWiFi = false;
bool isFirstBoot = false;

//// Program & Menu state
String clockSeparators [] = {" ", "-", "_"};
String stateStrings[] = {"MENU", "RUNNING", "SETTINGS"};
String menuStrings[] = {"MODE MOVIE", "MODE RANDOM", "MODE MESSAGE", "MODE CLOCK", "SETTINGS"};
String settingsStrings[] = {"GMT ", "24H MODE ", "BRIGHT ", "CLK RGB ", "CLK CNT ", "CLK SEP ", "UPDATE GMT"};

enum states {
  MENU = 0,
  RUNNING = 1,
  SET = 2,
} currentState;

enum modes {
  MOVIE = 0,
  RANDOM = 1,
  MESSAGE = 2,
  CLOCK = 3,
  SETTINGS = 4,
} currentMode;

enum settings {
  SET_GMT = 0,
  SET_24H = 1,
  SET_BRIGHT = 2,
  SET_CLOCK_RGB = 3,
  SET_CLOCK = 4,
  SET_SEP = 5,
  SET_UPDATE_GMT = 6,
} currentSetting;


/* Code cracking stuff
   Though this works really well, there are probably much nicer and cleaner
   ways of doing this, so feel free to improve it and make a pull request!
*/
uint8_t counter = 0;
unsigned long nextTick = 0;
unsigned long nextSolve = 0;
uint16_t tickStep = 100;
uint16_t solveStep = 1000;
uint16_t solveStepMin = 4000;
uint16_t solveStepMax = 8000;
float solveStepMulti = 1;
uint8_t solveCount = 0;
uint8_t solveCountFinished = 10;
byte lastDefconLevel = 0;

// Audio stuff
bool beeping = false;
unsigned long nextBeep = 0;
uint8_t beepCount = 3;
int freq = 2000;
int channel = 0;
int resolution = 8;

// RGB stuff
unsigned long nextRGB = 0;
long nextPixelHue = 0;
uint32_t defcon_colors[] = {
  Color(255, 255, 255),
  Color(255, 0, 0),
  Color(255, 255, 0),
  Color(0, 255, 0),
  Color(0, 0, 255),
};

// General stuff
unsigned long countdownToClock = 0;


// Setup 3 AlphaNumeric displays (4 digits per display)
Adafruit_AlphaNum4 matrix[3] = { Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4() };

char displaybuffer[12] = {'-', '-', '-', ' ', '-', '-', '-', '-', ' ', '-', '-', '-'};

char missile_code[12] = {'A', 'B', 'C', 'D', 'E', 'F', '0', '1', '2', '3', '4', '5'};

char missile_code_movie[12] = {'C', 'P', 'E', ' ', '1', '7', '0', '4', ' ', 'T', 'K', 'S'};

char missile_code_message[12] = {'L', 'O', 'L', 'Z', ' ', 'F', 'O', 'R', ' ', 'Y', 'O', 'U'};

uint8_t code_solve_order_movie[10] = {7, 1, 4, 6, 11, 2, 5, 0, 10, 9}; // 4 P 1 0 S E 7 C K T

uint8_t code_solve_order_random[12] = {99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};

// Initialise the buttons using OneButton library
OneButton Button1(BUT1, false);
OneButton Button2(BUT2, false);
#ifdef HAXORZ_EDITION
OneButton Button3(BUT3, false);
OneButton Button4(BUT4, false);
#endif

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println("");
  Serial.println("Wargames Missile Codes");


  // Setup RMT RGB strip
  while ( !RGB_Setup(RGBLED, 50) )
  {
    // This is not good...
    DisplayText( "FAILED to init RMT :[" );
    delay(10);
  }

  // Attatch button IO for OneButton
  Button1.attachClick(BUT1Press);
  Button1.attachDuringLongPress(BUT1_SaveSettings);
  Button2.attachClick(BUT2Press);
#ifdef HAXORZ_EDITION
  Button3.attachClick(BUT3Press);
  Button4.attachClick(BUT4Press);
#endif

  // Initialise each of the HT16K33 LED Drivers
  matrix[0].begin(0x70);  // pass in the address  0x70 == 1, 0x72 == 2, 0x74 == 3
  matrix[1].begin(0x72);  // pass in the address  0x70 == 1, 0x72 == 2, 0x74 == 3
  matrix[2].begin(0x74);  // pass in the address  0x70 == 1, 0x72 == 2, 0x74 == 3

  // Reset the code variables
  ResetCode();

  // Clear the display & RGB strip
  Clear();
  RGB_Clear();

  // Setup the Audio channel
#if ESP_ARDUINO_VERSION_MAJOR < 3
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(DAC, channel);
#else
  ledcAttachChannel(DAC, freq, resolution, channel);
#endif

  // Load the user settings. If this fails, defaults are created.
  DisplayText( "FRMAT SPIFFS" );
  loadSettings();

  SetDisplayBrightness(settings_displayBrightness);

  /* Initialise WiFi to get the current time.
     Once the time is obtained, the internal ESP32 RTC is used to keep the time
     Make sure you have set your SSID and Password in secret.h
  */

  StartWifi();

  // User settable countdown from main menu to go into clock if no user interaction
  // Has happened. settings_clockCountdownTime is in seconds and we want milliseconds

  countdownToClock = millis() + settings_clockCountdownTime * 1000;

  // Display MENU
  DisplayText( "MENU" );
}

void playSound(uint32_t snd)
{
#if ESP_ARDUINO_VERSION_MAJOR < 3
  ledcWriteTone(channel, snd);
#else
  ledcWriteTone(DAC, snd);
#endif
}

void UpdateGMT_NTP()
{
  DisplayText( "Getting GMT" );

  // IP Adddress Stuff
  char szIP[32];

  if (GetExternalIP(szIP)) {
    int iTimeOffset; // offset in seconds
    // Serial.print("My IP: ");
    // Serial.println(szIP);
    // Get our time zone offset (including daylight saving time)
    iTimeOffset = GetTimeOffset(szIP);
    if (iTimeOffset != -1)
    {
      //init and get the time
      settings_GMT = iTimeOffset / 3600;
      saveSettings();
    }
    else
    {
      Serial.println("*** TZ info failed");
    }
  }
  else
  {
    Serial.println("*** IP info failed");
  }
}

void StartWifi()
{

  if ( ssid == "PUT SSID HERE" )
  {
    DisplayText( "SSID NOT SET" );
    RGB_SetColor_ALL( Color(255, 0, 0) );
    hasWiFi = false;
    delay(2000);
  }
  else if ( password == "PUT PASSWORD HERE" )
  {
    DisplayText( "PASS NOT SET" );
    RGB_SetColor_ALL( Color(255, 0, 0) );
    hasWiFi = false;
    delay(2000);
  }
  else
  {
    DisplayText( "TRYING WiFi" );

    //connect to WiFi
    int wifi_counter = 100;
    Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED && wifi_counter > 0)
    {
      delay(100);
      RGB_Rainbow(0);
      wifi_counter--;
      Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED && wifi_counter == 0)
    {
      DisplayText( "WiFi FAILED" );
      RGB_SetColor_ALL( Color(255, 0, 0) );
      hasWiFi = false;
      //while(1) {delay(1000);}
      delay(3000);
    }
    else
    {
      Serial.println(" CONNECTED");
      DisplayText( "WiFi GOOD" );
      RGB_SetColor_ALL( Color(0, 255, 0) );

      hasWiFi = true;
      if (isFirstBoot) // Need to grab from online and save locally if this is out first boot
      {
        isFirstBoot = false;
        UpdateGMT_NTP();
      }

      configTime(settings_GMT * 3600, 0, ntpServer);

      delay(100);

      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        DisplayText( "Time FAILED" );
        RGB_SetColor_ALL( Color(255, 0, 0) );
      }
      else
      {
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

        DisplayText( "Time Set OK" );
        RGB_SetColor_ALL( Color(0, 0, 255) );
      }

      delay(1500);

      DisplayText_Scroll( GetSecondsUntilXmas(), 200 );
    }
  }
}

// Button press code her
long nextButtonPress = 0;

// This is triggered from a long press on button 1
void BUT1_SaveSettings()
{
  if ( currentState == SET && currentMode == SETTINGS )
  {
    Serial.println("SAAAAAVE!");
    DisplayText( "SAVING..." );
    saveSettings();
    delay(500);

    if ( didChangeClockSettings )
    {
      // If the clock parameters were changed, we need to re-set the ESP32 RTC time.
      configTime(settings_GMT * 3600, 0, ntpServer);
    }

    // Reset the menu state after save
    currentState = MENU;
    currentSetting = SET_GMT;
    countdownToClock = millis() + settings_clockCountdownTime * 1000;
    DisplayText( "SETTINGS" );
  }
}

void BUT1Press()
{
  // Only allow a button press every 10ms
  if ( nextButtonPress < millis() )
  {
    nextButtonPress = millis() + 10;

    // If we are not in the menu, cancel the current state and show the menu
    if ( currentState == RUNNING )
    {
      currentState = MENU;

      DisplayText( "MENU" );

      //Shutdown the audio is it's beeping
      playSound(0);
      //      ledcWriteTone(channel, 0);
      beeping = false;
    }
    else if ( currentState == MENU )
    {
      // Update the current program state and display it on the menu
      int nextMode = (int)currentMode + 1;
      if ( nextMode == ELEMENTS(menuStrings) )
        nextMode = 0;
      currentMode = (modes)nextMode;

      DisplayText( menuStrings[(int)currentMode] );
    }
    else if ( currentState == SET )
    {
      // Update the current settings state and display it on the menu
      int nextMode = (int)currentSetting + 1;
      if ( nextMode == ELEMENTS(settingsStrings) )
        nextMode = 0;
      currentSetting = (settings)nextMode;

      ShowSettings();
    }

    // Reset the clock countdown now that we are back in the menu
    // settings_clockCountdownTime is in seconds, we need milliseconds
    countdownToClock = millis() + settings_clockCountdownTime * 1000;

    Serial.print("Current State: ");
    Serial.print( stateStrings[(int)currentState] );

    Serial.print("  Current Mode: ");
    Serial.println( menuStrings[(int)currentMode] );
  }
}

void BUT2Press()
{
  // Only allow a button press every 10ms
  if ( nextButtonPress < millis() )
  {
    nextButtonPress = millis() + 10;

    // If in the menu, start whatever menu option we are in
    if ( currentState == MENU )
    {
      // Check to see what mode we are in, because not all modes start the
      // code sequence is
      if ( currentMode == SETTINGS )
      {
        currentState = SET;
        Serial.println("Going into settings mode");
        ShowSettings();
      }
      else
      {
        // Set the defcon state if we are not the clock, otherwise clear the RGB
        if ( currentMode != CLOCK )
          RGB_SetDefcon(5, true);
        else
          RGB_Clear(true);

        ResetCode();
        Clear();
        currentState = RUNNING;
      }
    }
    else if ( currentState == SET )
    {
      // If in the settings, cycle the setting for whatever setting we are on
      if ( currentMode == SETTINGS )
      {
        UpdateSetting(1);
      }
    }
  }

  Serial.print("Current State: ");
  Serial.println( stateStrings[(int)currentState] );
}

#ifdef HAXORZ_EDITION
void BUT3Press()
{
  // Only allow a button press every 10ms
  if ( nextButtonPress < millis() )
  {
    nextButtonPress = millis() + 10;

    // If in the settings, cycle the setting for whatever menu option we are in
    if ( currentState == SET && currentMode == SETTINGS )
    {
      UpdateSetting(1);
    }
  }
}

void BUT4Press()
{
  // Only allow a button press every 10ms
  if ( nextButtonPress < millis() )
  {
    nextButtonPress = millis() + 10;

    // If in the settings, cycle the setting for whatever menu option we are in
    if ( currentState == SET && currentMode == SETTINGS )
    {
      UpdateSetting(-1);
    }
  }
}
#endif

// Cycle the setting for whatever current setting we are changing
void UpdateSetting( int dir )
{
  if ( currentSetting == SET_GMT )
  {
    settings_GMT += dir;
    if ( settings_GMT > 14 )
      settings_GMT = -12;
    else if ( settings_GMT < -12 )
      settings_GMT = 14;

    didChangeClockSettings = true;
  }
  else if ( currentSetting == SET_24H )
  {
    settings_24H = ! settings_24H;
  }
  else if ( currentSetting == SET_CLOCK_RGB )
  {
    settings_ClockRGB += (dir * 10);
    if ( settings_ClockRGB > 250 )
      settings_ClockRGB = 0;
    else if ( settings_ClockRGB < 0 )
      settings_ClockRGB = 250;

    //      Serial.print("settings_ClockRGB ");
    //      Serial.println(settings_ClockRGB);
    //
    //      Serial.println((settings_ClockRGB/250) * 100);

  }
  else if ( currentSetting == SET_BRIGHT )
  {
    settings_displayBrightness += dir;
    if ( settings_displayBrightness > 15 )
      settings_displayBrightness = 0;
    else if ( settings_displayBrightness < 0 )
      settings_displayBrightness = 15;

    SetDisplayBrightness(settings_displayBrightness);
  }
  else if ( currentSetting == SET_CLOCK )
  {
    settings_clockCountdownTime += dir * 10; // Larger increments for quicker change
    if ( settings_clockCountdownTime > 60 )
      settings_clockCountdownTime = 0;
    else if ( settings_clockCountdownTime < 0 )
      settings_clockCountdownTime = 60;

    countdownToClock = millis() + settings_clockCountdownTime * 1000;
  }
  else if ( currentSetting == SET_SEP )
  {
    settings_separator += dir;
    if ( settings_separator == ELEMENTS(clockSeparators))
      settings_separator = 0;
    else if ( settings_separator < 0 )
      settings_separator = ELEMENTS(clockSeparators) - 1;
  }
  else if ( currentSetting == SET_UPDATE_GMT )
  {
    UpdateGMT_NTP();
    DisplayText("GMT now " + String(settings_GMT));
    configTime(settings_GMT * 3600, 0, ntpServer);
    delay(2000);
    Clear();
    currentMode = CLOCK;
    currentState = RUNNING;

    if (settings_ClockRGB == 0)
      RGB_Clear(true);
    else
      RGB_SetBrightness(settings_ClockRGB);

    return;
  }

  // Update the display showing whatever the new current setting is
  ShowSettings();
}

void ShowSettings()
{
  Serial.print("current setting: ");
  Serial.println(currentSetting);

  String val = "";

  if ( currentSetting == SET_GMT )
    val = String(settings_GMT);
  else if ( currentSetting == SET_24H )
    val = settings_24H ? "ON" : "OFF";
  else if ( currentSetting == SET_BRIGHT )
    val = String(settings_displayBrightness);
  else if ( currentSetting == SET_CLOCK )
  {
    if ( settings_clockCountdownTime > 0 )
      val = String(settings_clockCountdownTime);
    else
      val = "OFF";
  }
  else if ( currentSetting == SET_CLOCK_RGB )
  {
    if ( settings_ClockRGB > 0 )
      val = String(int((float)settings_ClockRGB / 250.0 * 100)) + "%";
    else
      val = "OFF";
  }
  else if ( currentSetting == SET_SEP)
  {
    if ( settings_separator == 0 )
      val = "SPC";
    else
      val = clockSeparators[settings_separator];
  }
  else if ( currentSetting == SET_UPDATE_GMT)
  {
    val = "";
  }

  DisplayText( settingsStrings[(int)currentSetting] + val);
}

// Adjust the LED display brightness: Range is 0-15
void SetDisplayBrightness( int val )
{
  for (int x = 0; x < 3; x++)
    matrix[x].setBrightness(val);
}

// Take the time data from the RTC and format it into a string we can display
void DisplayTime()
{
  if (!hasWiFi)
  {
    DisplayText("NO CLOCK");
    RGB_SetColor_ALL( Color(0, 255, 0) );
    return;
  }
  // Store the current time into a struct
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    DisplayText("TIME FAILED");
    RGB_SetColor_ALL( Color(0, 255, 0) );
    return;
  }
  // Formt the contents of the time struct into a string for display
  char DateAndTimeString[12];
  String sep = clockSeparators[settings_separator];

  int the_hour = timeinfo.tm_hour;

  // Adjust for 24 hour display mode
  if (!settings_24H) {
    if (the_hour > 12)
      the_hour -= 12;
    else if (the_hour == 0)
      the_hour = 12;
  }

  // Padd the time if the hour is a single digit
  if ( the_hour < 10 )
    sprintf(DateAndTimeString, "   %d%s%02d%s%02d", the_hour, sep, timeinfo.tm_min, sep, timeinfo.tm_sec);
  else
    sprintf(DateAndTimeString, "  %d%s%02d%s%02d", the_hour, sep, timeinfo.tm_min, sep, timeinfo.tm_sec);

  // Iterate through each digit on the display and populate the time, or clear the digit
  uint8_t curDisplay = 0;
  uint8_t curDigit = 0;

  for ( uint8_t i = 0; i < 10; i++ )
  {
    matrix[curDisplay].writeDigitAscii( curDigit, DateAndTimeString[i]);
    curDigit++;
    if ( curDigit == 4 )
    {
      curDigit = 0;
      curDisplay++;
    }
  }

  // Show whatever is in the display buffer on the display
  Display();
}

// Display whatever is in txt on the display
void DisplayText(String txt)
{
  uint8_t curDisplay = 0;
  uint8_t curDigit = 0;

  Clear();

  // Iterate through each digit and push the character rom the txt string into that position
  for ( uint8_t i = 0; i < txt.length(); i++ )
  {
    matrix[curDisplay].writeDigitAscii( curDigit, txt.charAt(i));
    curDigit++;
    if ( curDigit == 4 )
    {
      curDigit = 0;
      curDisplay++;
    }
  }

  // Show whatever is in the display buffer on the display
  Display();
}

// Display whatever is in txt on the display
void DisplayText_Scroll(String txt, uint8_t delay_ms)
{
  uint8_t curDisplay = 0;
  uint8_t curDigit = 0;

  Clear();

  String padded_txt = "            " + txt + "            ";
  int txt_len = padded_txt.length();
  int start_index = 0;


  // Iterate through each digit and push the character rom the txt string into that position
  for ( uint8_t i = 0; i < padded_txt.length() - 12; i++ )
  {
    for (uint8_t d = 0; d < 12; d++)
    {
      matrix[curDisplay].writeDigitAscii( curDigit, padded_txt.charAt(start_index + d));
      curDigit++;
      if ( curDigit == 4 )
      {
        curDigit = 0;
        curDisplay++;
      }
    }

    Display();
    delay(delay_ms);
    start_index++;

    curDisplay = 0;
    curDigit = 0;

  }
}


// Return a random time step for the next solving solution
uint16_t GetNextSolveStep()
{
  return random( solveStepMin, solveStepMax ) * solveStepMulti;
}

// Fill whatever is in the code buffer into the display buffer
void FillCodes()
{
  int matrix_index = 0;
  int character_index = 0;
  char c = 0;
  char c_code = 0;

  for ( int i = 0; i < 12; i++ )
  {
    c = displaybuffer[i];
    c_code = missile_code[i];
    if ( c == '-' )
    {
      // c is a character we need to randomise
      c = random( 48, 91 );
      while ( ( c > 57 && c < 65 ) || c == c_code )
        c = random( 48, 91 );
    }
    matrix[matrix_index].writeDigitAscii( character_index, c );
    character_index++;
    if ( character_index == 4 )
    {
      character_index = 0;
      matrix_index++;
    }
  }

  // Show whatever is in the display buffer on the display
  Display();
}

// Randomise the order of the code being solved
void RandomiseSolveOrder()
{
  // Ensure all array slots start with 99
  for ( uint8_t i = 0; i < 12; i++ )
    code_solve_order_random[i] = 99;

  for ( uint8_t i = 0; i < 12; i++ )
  {
    uint8_t ind = random(0, 12);
    while ( code_solve_order_random[ind] < 99 )
      ind = random(0, 12);

    code_solve_order_random[ind] = i;
  }
}

// Reset the code being solved back to it's starting state
void ResetCode()
{
  if ( currentMode == MOVIE )
  {
    solveStepMulti = 1;
    solveCountFinished  = 10;
    for ( uint8_t i = 0; i < 12; i++ )
      missile_code[i] = missile_code_movie[i];
  }
  else if ( currentMode == RANDOM )
  {
    solveStepMulti = 0.5;

    // Randomise the order in which we solve this code
    RandomiseSolveOrder();

    // Set the code length and populate the code with random chars
    solveCountFinished = 12;

    for ( uint8_t i = 0; i < 12; i++ )
    {
      Serial.print("Setting code index ");
      Serial.print(i);

      // c is a character we need to randomise
      char c = random( 48, 91 );
      while ( c > 57 && c < 65 )
        c = random( 48, 91 );


      Serial.print(" to char ");
      Serial.println( c );

      missile_code[i] = c;
    }
  }
  else if ( currentMode == MESSAGE )
  {
    solveStepMulti = 0.5;

    // Randomise the order in which we solve this code
    RandomiseSolveOrder();

    // Set the code length and populate the code with the stored message
    solveCountFinished = 12;
    for ( uint8_t i = 0; i < 12; i++ )
      missile_code[i] = missile_code_message[i];
  }

  // Set the first solve time step for the first digit lock

  solveStep = GetNextSolveStep();
  nextSolve = millis() + solveStep;
  solveCount = 0;
  lastDefconLevel = 0;

  // Clear code display buffer
  for ( uint8_t i = 0; i < 12; i++ )
  {
    if ( currentMode == 0 && ( i == 3 || i == 8 ) )
      displaybuffer[ i ] = ' ';
    else
      displaybuffer[ i ] = '-';
  }
}

/*  Solve the code based on the order of the solver for the current mode
    This is fake of course, but so was the film!
    The reason we solve based on a solver order, is so we can solve the code
    in the order it was solved in the movie.
*/

void SolveCode()
{
  // If the number of digits solved is less than the number to be solved
  if ( solveCount < solveCountFinished )
  {
    // Grab the next digit from the code based on the mode
    uint8_t index = 0;

    if ( currentMode == MOVIE )
    {
      index = code_solve_order_movie[ solveCount ];
      displaybuffer[ index ] = missile_code[ index ];
    }
    else
    {
      index = code_solve_order_random[ solveCount ];
      displaybuffer[ index ] = missile_code[ index ];
    }

    Serial.println("Found " + String(displaybuffer[ index ]) + " @ index: " + String(solveCount));

    // move tghe solver to the next digit of the code
    solveCount++;

    // Get current percentage of code solved so we can set the defcon display
    float solved = 1 - ( (float)solveCount / (float)solveCountFinished);

    Serial.println("Solved " + String(solved));

    byte defconValue = int(solved * 5 + 1);
    RGB_SetDefcon(defconValue, false);

    Serial.println("Defcon " + String(defconValue));

    Serial.println("Next solve index: " + String(solveCount));

    FillCodes();

    // Long beep to indicate a digit in he code has been solved!
    playSound(1500);
    //    ledcWriteTone(channel, 1500 );
    beeping = true;
    beepCount = 3;
    nextBeep = millis() + 500;
  }
}

// Clear the contents of the display buffers and update the display
void Clear()
{
  // There are 3 LED drivers
  for ( int i = 0; i < 3; i++ )
  {
    // There are 4 digits per LED driver
    for ( int d = 0; d < 4; d++ )
      matrix[i].writeDigitAscii( d, ' ');

    matrix[i].writeDisplay();
  }
}

// Show the contents of the display buffer on the displays
void Display()
{
  for ( int i = 0; i < 3; i++ )
    matrix[i].writeDisplay();
}

void RGB_SetDefcon( byte level, bool force )
{
  // Only update the defcon display if the value has changed
  // to prevent flickering
  if ( lastDefconLevel != level || force )
  {
    lastDefconLevel = level;

    // Clear the RGB LEDs
    RGB_Clear();

    // Level needs to be clamped to between 0 and 4
    byte newLevel = constrain(level - 1, 0, 4);
    leds[newLevel] = defcon_colors[newLevel];

    RGB_FillBuffer();
  }
}

void RGB_Rainbow(int wait)
{
  if ( nextRGB < millis() )
  {
    nextRGB = millis() + wait;
    nextPixelHue += 256;

    if ( nextPixelHue > 65536 )
      nextPixelHue = 0;

    // For each RGB LED
    for (int i = 0; i < 5; i++)
    {
      int pixelHue = nextPixelHue + (i * 65536L / 5);
      leds[i] = gamma32(ColorHSV(pixelHue));
    }
    // Update RGB LEDs
    RGB_FillBuffer();
  }
}

int pingpong(int t, int length)
{
  return t % length;
}


void RGB_SetColor_ALL(uint32_t col)
{
  // For each RGB LED
  for (int i = 0; i < 5; i++)
    leds[i] = col;

  // Update RGB LEDs
  RGB_FillBuffer();
}

void loop()
{
  // Used by OneButton to poll for button inputs
  Button1.tick();
  Button2.tick();
#ifdef HAXORZ_EDITION
  Button3.tick();
  Button4.tick();
#endif

  // We are in the menu
  if ( currentState == MENU || currentState == SET )
  {
    // We dont need to do anything here, but lets show some fancy RGB!
    // Brightness of RGB LEDs is always 20% when in menu mode - 50/255
    RGB_SetBrightness(50);
    RGB_Rainbow(10);

    // Timer to go into clock if no user interaction for XX seconds
    // If settings_clockCountdownTime is 0, this feature is off
    if ( hasWiFi && settings_clockCountdownTime > 0 && countdownToClock < millis()  )
    {
      Clear();
      currentMode = CLOCK;
      currentState = RUNNING;

      if (settings_ClockRGB == 0)
        RGB_Clear(true);
    }
  }
  // We are running a simulation
  else
  {
    if ( currentMode == CLOCK )
    {
      // If the Clock RGB brightness is not 0, show the rainbow at the current clock RGB brightness
      if (settings_ClockRGB > 0)
      {
        RGB_SetBrightness(settings_ClockRGB);
        RGB_Rainbow(40);
      }

      if ( nextBeep < millis() )
      {
        DisplayTime();
        nextBeep = millis() + 1000;
      }
    }
    else
    {
      // We have solved the code
      if ( solveCount == solveCountFinished )
      {
        if ( nextBeep < millis() )
        {
          beeping = !beeping;
          nextBeep = millis() + 500;

          if ( beeping )
          {
            if ( beepCount > 0 )
            {
              RGB_SetDefcon(1, true);
              FillCodes();
              beepCount--;
              playSound(1500);
              //              ledcWriteTone(channel, 1500);
            }
            else
            {
              RGB_SetDefcon(1, true);
              DisplayText("LAUNCHING...");
            }
          }
          else
          {
            Clear();
            RGB_Clear(true);
            playSound(0);
            //            ledcWriteTone(channel, 0 );
          }
        }

        // We are solved, so no point running any of the code below!
        return;
      }

      // Only update the displays every "tickStep"
      if ( nextTick < millis() )
      {
        nextTick = millis() + tickStep;

        // This displays whatever the current state of the display is
        FillCodes();

        // If we are not currently beeping, play some random beep/bop computer-y sounds
        if ( !beeping )
        {
          playSound(random(90, 250));
          // ledcWriteTone(channel, random(90, 250));
        }
      }

      // This is where we solve each code digit
      // The next solve step is a random length to make it take a different time every run
      if ( nextSolve < millis() )
      {
        nextSolve = millis() + solveStep;
        // Set the solve time step to a random length
        solveStep = GetNextSolveStep();
        //
        SolveCode();
      }

      // Zturn off any beeping if it's trying to beep
      if ( beeping )
      {
        if ( nextBeep < millis() )
        {
          playSound(0);
          //          ledcWriteTone(channel, 0);
          beeping = false;
        }
      }
    }
  }
}

// File/Settings Stuff
void loadSettings()
{
  ESPFlash<uint8_t> set_ClockCountdown("/set_ClockCountdown");
  int leng = set_ClockCountdown.length();

  // If the clock countdown is 0, then no data exists
  // So we wil create the defaults and reload
  if ( leng == 0 )
  {
    isFirstBoot = true;
    Serial.println("**** Creating settings!!!");
    saveSettings();
    loadSettings();
    return;
  }
  Clear();

  //  ESPFlash<uint8_t> set_ClockCountdown("/set_ClockCountdown");
  settings_clockCountdownTime = set_ClockCountdown.get();

  ESPFlash<uint8_t> set_Separator("/set_Separator");
  settings_separator = constrain(set_Separator.get(), 0, ELEMENTS(clockSeparators) - 1);

  ESPFlash<int> set_GMT("/set_GMT");
  settings_GMT = set_GMT.get();

  ESPFlash<int> set_24H("/set_24H");
  settings_24H = (set_24H.get() == 1);

  ESPFlash<uint8_t> set_ClockRGB("/set_ClockRGB");
  settings_ClockRGB = set_ClockRGB.get();

  ESPFlash<uint8_t> set_Brightness("/set_Brightness");
  settings_displayBrightness = set_Brightness.get();
}

void saveSettings()
{
  ESPFlash<int> set_GMT("/set_GMT");
  set_GMT.set(settings_GMT);

  ESPFlash<int> set_24H("/set_24H");
  set_24H.set(settings_24H ? 1 : 0);

  ESPFlash<uint8_t> set_ClockRGB("/set_ClockRGB");
  set_ClockRGB.set(settings_ClockRGB);

  ESPFlash<uint8_t> set_ClockCountdown("/set_ClockCountdown");
  set_ClockCountdown.set(settings_clockCountdownTime);

  ESPFlash<uint8_t> set_Separator("/set_Separator");
  set_Separator.set(settings_separator);

  ESPFlash<uint8_t> set_Brightness("/set_Brightness");
  set_Brightness.set(settings_displayBrightness);
}
