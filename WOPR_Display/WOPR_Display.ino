/*************************************************** 
  War Games - W.O.P.R. Missile Codes
  2020 UNexpected Maker
  Licensed under MIT Open Source

  This code is designed specifically to run on an ESP32. It uses features only
  available on the ESP32 like RMT and ledcSetup.

  W.O.P.R is available on tindie
  https://www.tindie.com/products/seonr/wopr-missile-launch-code-display-kit/
  
  Wired up for use with the TinyPICO ESP32 Development Board
  https://www.tinypico.com/shop/tinypico

  And the TinyPICO Audio Shield
  https://www.tinypico.com/shop/tinypico-mzffe-zatnr

2020/05/23 - CJW 
Updated menu to be a dynamic array
Added the concept of idle user shows clock
Added the concept of super idle user clears display - shows first char dot as running active
Added Central animation flag counter with Binary tests
Added Setup to features

2020/05/24 - CJW 
Adding Feature Alarm file
Refactor WOPR_Display file to allow Features to hook in
Refactor Setup so we can "reset" the wifi - with some demo scrolling text

2020/05/26 - CJW 
Refactor Solve mode, so we can override in other features

 ****************************************************/

#include <Wire.h>
#include <Adafruit_GFX.h> // From Library Manager
#include "Adafruit_LEDBackpack.h" // From Library Manager
//#include <Adafruit_NeoPixel.h> // From Library Manager
#include "OneButton.h" // From Library Manager
#include <WiFi.h>
#include "time.h"
#include "secret.h"
#include "rmt.h"


// Defines
#ifndef _BV
  #define _BV(bit) (1<<(bit))
#endif

#define BUT1 14 // front lef button
#define BUT2 15 // front right button
#define RGBLED 27 // RGB LED Strip
#define DAC 25 // RGB LED Strip

#define IDLE_MINS 1                                                 // Number of min's before we are considered idle
#define NODISPLAY_MINS 2                                            // Number of min's before we are considered idle
#define IDLE_TIME ((IDLE_MINS*60)*1000)                             // ms of mins before we are considered idle and show clock
#define NODISPLAY_TIME ((NODISPLAY_MINS*60)*1000)                   // ms of mins after we are idle will disable display Time
#define ALLOW_CLOCK_BLANKING false                                  // Set true, if you want to allow idle clock blanking (Alarm can override this)

// NTP Wifi Time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600 * 11;              
const int   daylightOffset_sec = 3600;

// Program & Menu state
uint8_t currentState = 0; // 0 - menu, 1 - running
uint8_t currentMode = 0; // 0 - movie simulation, 1 - random sequence, 2 - message, 3 - clock, 4 - Alarm Controls
String mainMenu[] = {"MODE MOVIE", "MODE RANDOM", "MODE MESSAGE", "MODE CLOCK", "MODE ALARM"};
uint8_t mainMenuCount = ( sizeof(mainMenu) / sizeof(mainMenu[0]) ) - 1;

/* Code cracking stuff
 * Though this works really well, there are probably much nicer and cleaner 
 * ways of doing this, so feel free to improve it and make a pull request!
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

// Anim Toggles - 250ms cycle changing - add more defines as desired
#define PER_SECOND_TOGGLE           (1<<2)
#define PER_HALF_SECOND_TOGGLE      (1<<1)
#define PER_QTR_SECOND_TOGGLE       (1)
byte animCounter = 0;      //  Increase every 250ms,  mask for above defines to check toggle
unsigned long nextAnim = 0;

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

// Setup 3 AlphaNumeric displays (4 digits per display)
Adafruit_AlphaNum4 matrix[3] = { Adafruit_AlphaNum4(), Adafruit_AlphaNum4(), Adafruit_AlphaNum4() };

char displaybuffer[12] = {'-', '-', '-', ' ', '-', '-', '-', '-', ' ', '-', '-', '-'};

char missile_code[12] = {'A', 'B', 'C', 'D', 'E', 'F', '0', '1', '2', '3', '4', '5'};

char missile_code_movie[12] = {'C', 'P', 'E', ' ', '1', '7', '0', '4', ' ', 'T', 'K', 'S'};

char missile_code_message[12] = {'L', 'O', 'L', 'Z', ' ', 'F', 'O', 'R', ' ', 'Y', 'O', 'U'};

uint8_t code_solve_order_movie[10] = {7,1,4,6,11,2,5,0,10,9};  // 4 P 1 0 S E 7 C K T

uint8_t code_solve_order_random[12];    // Now initialized in randomize order function
uint8_t codeSolveMode;                  // Initialized on ResetCode

// Initialise the buttons using OneButton library
OneButton Button1(BUT1, false);
OneButton Button2(BUT2, false);
unsigned long nextIdleTime = 0;     // MS when we should consider the user abandom'd the controls

void setup()
{
  Serial.begin(115200);
  Serial.println("Wargames Missile Codes");

  // Setup RMT RGB strip
  while ( !RGB_Setup(RGBLED, 50) )
  {
    // This is not good...
    delay(1000);
  }
  
  // Attatch button IO for OneButton
  Button1.attachClick(BUT1Press);
  Button2.attachClick(BUT2Press);

  // Initialise each of the HT16K33 LED Drivers
  matrix[0].begin(0x70);  // pass in the address  0x70 == 1, 0x72 == 2, 0x74 == 3
  matrix[1].begin(0x72);  // pass in the address  0x70 == 1, 0x72 == 2, 0x74 == 3
  matrix[2].begin(0x74);  // pass in the address  0x70 == 1, 0x72 == 2, 0x74 == 3

  // Reset the code variables
  ResetCode( currentMode );

  // Clear the display
  Clear();

  // Setup the Audio channel
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(DAC, channel);

  /* Initialise WiFi to get the current time.
   * Once the time is obtained, WiFi is disconnected and the internal 
   * ESP32 RTC is used to keep the time
   * Make sure you have set your SSID and Password in secret.h
   */
  StartWifi();

  // Setup Features
  Setup_Alarm();
  
  // Display MENU
  DisplayText( "MENU" , true);
}

void StartWifi()
{
  //connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  String whileMessage = "Connecting  ";
  int whileCount = 0;
  
  DisplayText(whileMessage, true);
  while (WiFi.status() != WL_CONNECTED) {
    
    delay( 500 );

    // If we have tried 20 times - lets reset WIFI (sometimes this helps :) )
    if (  whileCount > 20 ) {
      Serial.println(".");
      WiFi.begin(ssid, password);
      Serial.printf("Reseting to %s ", ssid);
      DisplayText("Reseting", true);
      whileCount = 0;
    }
    else {
      Serial.print(".");
      whileCount++;
      whileMessage = whileMessage.substring(1, whileMessage.length() )+whileMessage[0];
      DisplayText(whileMessage, true);
    }
  }
  Serial.println("CONNECTED");
  DisplayText("Connected", true);

  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  if ( !getLocalTime(&timeinfo) ) {
    Serial.println("Failed to obtain time");
    DisplayText("Bad Time", true);
    delay(500);
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  nextIdleTime = GetTimeWithZeroMillis(IDLE_TIME);      // Set check idle to be in the future - remove to auto move to clock on start

  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// Button press code her
long nextButtonPress = 0;

void BUT1Press()
{
  // Only allow a button press every 10ms
  if ( nextButtonPress < millis() )
  {
    nextIdleTime = GetTimeWithZeroMillis(IDLE_TIME);      // Set check idle to be in the future

    nextButtonPress = millis() + 10;

    // Button used by alarm
    if ( BUT1Press_Alarm() )
    {
      return;
    }

    // If we are not in the menu, cancel the current state and show the menu
    if ( currentState == 1 )
    {
      currentState = 0;
            
      DisplayText( "MENU" , true );

      //Shutdown the audio is it's beeping
      ledcWriteTone(channel, 0);
      beeping = false;
    }
    else
    {
      // Update the current program state and display it on the menu - refactored so we are array index based
      currentMode++;
      if ( currentMode > mainMenuCount ) {
        currentMode = 0;
      }

       MenuDisplay();
    }

    Serial.print("Current State: ");
    Serial.print( currentState );
    
    Serial.print("  Current Mode: ");
    Serial.println( currentMode );
  }
}

void BUT2Press()
{
  // Only allow a button press every 10ms
  if ( nextButtonPress < millis() )
  {
    nextIdleTime = GetTimeWithZeroMillis(IDLE_TIME);      // Set check idle to be in the future
  
    // Button used by alarm
    if ( BUT2Press_Alarm() )
    {
      return;
    }

    nextButtonPress = millis() + 10;

    // If in the menu, start whatever menu option we are in 
    if ( currentState == 0 )
    {
      // Set the defcon state if we are not the clock, otherwise clear the RGB
      if ( currentMode != 3 )
        RGB_SetDefcon(5, true);
      else
        RGB_Clear(true);
        
      ResetCode( currentMode );
      Clear();
      currentState = 1;
    }
  }

  Serial.print("Current State: ");
  Serial.println( currentState );
}

// Display current menu item - refactored to an array
void MenuDisplay()
{
  DisplayText( mainMenu[currentMode] , false );
}

// Display the provide time data structure
void DisplayTime(struct tm timeinfo)
{
  // If we been idle longer than IDLE_TIME+NODISPLAY_TIME , and if alarm approves
  if ( nextIdleTime + NODISPLAY_TIME < millis() && IdleTest_Alarm(true) )
  {
      Clear();  // Clear the display

      // lets flash the first dot for a UI indication we are running  
      matrix[0].writeDigitAscii( 0, ' ',  ( animCounter&PER_SECOND_TOGGLE ) );
  }
  else
  {
      // Formt the contents of the time struct into a string for display
      char DateAndTimeString[12];
      if ( timeinfo.tm_hour < 10 )
        sprintf(DateAndTimeString, "   %d %02d %02d", timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
      else
        sprintf(DateAndTimeString, "  %d %02d %02d", timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
    
      // Iterate through each digit on the display and populate the time, or clear the digit
      uint8_t curDisplay = 0;
      uint8_t curDigit = 0;
    
      for ( uint8_t i = 0; i < 10; i++ )
      {
        ApplyDotState(curDisplay, curDigit, DateAndTimeString[i] );
        curDigit++;
        if ( curDigit == 4 )
        {
          curDigit = 0;
          curDisplay++;
        }
      }
  }
  
  // Show whatever is in the display buffer on the display
  Display();
}

// Display whatever is in txt on the display
void DisplayText(String txt, bool clearLeds)
{
  uint8_t curDisplay = 0;
  uint8_t curDigit = 0;

  Clear();

  // Iterate through each digit and push the character rom the txt string into that position
  for ( uint8_t i = 0; i < txt.length(); i++ )
  {
    ApplyDotState(curDisplay, curDigit, txt.charAt(i) );
    curDigit++;
    if ( curDigit == 4 )
    {
      curDigit = 0;
      curDisplay++;
    }
  }

  if ( clearLeds ) 
  {
    RGB_Clear(true);
  }
  
  // Show whatever is in the display buffer on the display
  Display();
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

    ApplyDotState(matrix_index, character_index, c );
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
  // lets stop inf loop, if this is called more than once per session
  static const uint8_t initRandomOrder[] = {99,99,99,99,99,99,99,99,99,99,99,99};
  memcpy (code_solve_order_random, initRandomOrder, sizeof (code_solve_order_random));

  for ( uint8_t i = 0; i < 12; i++ )
  {
    uint8_t ind = random(0, 12);
    while ( code_solve_order_random[ind] < 99 )
      ind = random(0, 12);
  
    code_solve_order_random[ind] = i;
  }
}

// Reset the code being solved back to it's starting state
void ResetCode(uint8_t solveMode)
{
  codeSolveMode = solveMode;
  Serial.println((String)"ResetCode : "+codeSolveMode);

  switch ( codeSolveMode )
  {
    // Movie mode
    default:
    case 0: 
        solveStepMulti = 1;
        solveCountFinished  = 10;
        for ( uint8_t i = 0; i < 12; i++ )
          missile_code[i] = missile_code_movie[i];
    break;
    
    // Randomize mode
    case 1:
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
    break;
    
    // Message mode
    case 2: 
        solveStepMulti = 0.5;
    
        // Randomise the order in which we solve this code
        RandomiseSolveOrder();
    
        // Set the code length and populate the code with the stored message
        solveCountFinished = 12;
        for ( uint8_t i = 0; i < 12; i++ )
          missile_code[i] = missile_code_message[i];
     break;
  }

  // Set the first solve time step for the first digit lock
  solveStep = GetNextSolveStep();
  nextSolve = millis() + solveStep;
  solveCount = 0;
  lastDefconLevel = 0;
    
  // Clear code display buffer
  for ( uint8_t i = 0; i < 12; i++ )
  {
    if ( codeSolveMode == 0 && ( i == 3 || i == 8 ) )
      displaybuffer[ i ] = ' ';
    else
      displaybuffer[ i ] = '-';
  }    
}

/*  Solve the code based on the order of the solver for the current mode
 *  This is fake of course, but so was the film!
 *  The reason we solve based on a solver order, is so we can solve the code 
 *  in the order it was solved in the movie.
 */

void SolveCode(bool alarmMode)
{
  // If the number of digits solved is less than the number to be solved
  if ( solveCount < solveCountFinished )
  {
    nextIdleTime = GetTimeWithZeroMillis(IDLE_TIME);      // Set check idle to be in the future
    
    // Grab the next digit from the code based on the mode
    uint8_t index = 0;
    
    if ( codeSolveMode == 0 )
    {
      index = code_solve_order_movie[ solveCount ];
      displaybuffer[ index ] = missile_code[ index ];
    }
    else
    {
      index = code_solve_order_random[ solveCount ];
      displaybuffer[ index ] = missile_code[ index ];
    }

    Serial.println("Found "+(String)(currentMode)+" "+ String(displaybuffer[ index ]) +" @ index: " + String(solveCount));
    
    // move tghe solver to the next digit of the code
    solveCount++;

    // Get current percentage of code solved so we can set the defcon display
    float solved = 1 - ( (float)solveCount/(float)solveCountFinished);

    Serial.println("Solved " + String(solved));

    if ( !alarmMode )
    {
      byte defconValue = int(solved * 5 + 1);
      RGB_SetDefcon(defconValue, false);

      Serial.println("Defcon " + String(defconValue));

      Serial.println("Next solve index: " + String(solveCount));

      // Long beep to indicate a digit in he code has been solved!
      ledcWriteTone(channel, 1500 );
      beeping = true;
      beepCount = 3;
      nextBeep = millis() + 500;
    }
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
      ApplyDotState(i, d, ' ' );

    matrix[i].writeDisplay();
  }
}

// Get a dot state for LED driver, as we write this char
void ApplyDotState(int ledDriver, int ledDigit, char charByte)
{
  bool ledDotState = false;
  
  // Ask alarm if it should be enabled, pass in currentState to enable 
  ledDotState = GetDotState_Alarm(ledDriver, ledDigit, charByte, ledDotState);

  matrix[ledDriver].writeDigitAscii( ledDigit, charByte , ledDotState);
}

// Show the contents of the display buffer on the displays
void Display()
{
  // Additional Display updates from Alarm
  Display_Alarm();

  //
  for ( int i = 0; i < 3; i++ ) {
    matrix[i].writeDisplay();
  }
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
      byte newLevel = constrain(level-1, 0, 4);
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
    for(int i=0; i<5; i++)
    {
      int pixelHue = nextPixelHue + (i * 65536L / 5);
      leds[i] = gamma32(ColorHSV(pixelHue));
    }
    // Update RGB LEDs
    RGB_FillBuffer();
  }
}

// Test if user is idle 
void IdleTest()
{
  // if we idle out , lets show the clock
  if ( nextIdleTime < millis() )
  {
    // Only check if we are not already in clock mode
    if ( currentMode != 3 || currentState != 1 )
    {
      // Check alarm to see if we should allow idle flows
      if ( !IdleTest_Alarm(false) )
      {
        return;
      }

      Clear();
      RGB_Clear(true);
        
      currentMode = 3;
      currentState = 1;
    }
  }
}

void loop()
{
  // Used by OneButton to poll for button inputs
  Button1.tick();
  Button2.tick();

  // Animation flag controls for all features
  if ( nextAnim < millis() )
  {
    nextAnim = millis() + 250;
    animCounter++;
  }

  // Alarm loop always processes for alarm state changes
  Loop_Alarm();

  // Lets check if we should go into user idle
  IdleTest();

  // We are in the menu
  if ( currentState == 0 )
  {
    // We dont need to do anything here, but lets show some fancy RGB!
    RGB_Rainbow(10);
  }
  // If not in alarm mode
  else if ( currentMode != 4 )
  {
    // We are running a simulation
    if ( currentMode == 3 )
    {
      // update clock every second
      if ( nextBeep < millis() )
      {
        nextBeep = millis() + 1000;
        
        // Take the time data from the RTC and format it into a string we can display
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo)){
          Serial.println("Failed to obtain time");
          return;
        }

        DisplayTime(timeinfo);
      }
    }
    else
    {
      // We have solved the code
      if ( solveCount == solveCountFinished )
      {
        if ( nextBeep < millis() )
        {
          beeping = !beeping; // TODO : should/could we use (animFlag&PER_HALF_SECOND_TOGGLE);
          nextBeep = millis() + 500;
    
          if ( beeping ) // TODO : test if this would work (animFlag&PER_HALF_SECOND_TOGGLE);
          {
            if ( beepCount > 0 )
            {
              RGB_SetDefcon(1, true);
              FillCodes();
              beepCount--;        
              ledcWriteTone(channel, 1500);
            }
            else
            {
              RGB_SetDefcon(1, true);
              DisplayText("LAUNCHING...", false);
            }
          }
          else
          {
            Clear();
            RGB_Clear(true);
            ledcWriteTone(channel, 0 );
          }
        }
    
        // We are solved, so no point running any of the code below!
        return;
      }
  
      // Only update the displays every "tickStep"
      if ( nextTick < millis() )
      {
        nextTick = millis() + tickStep;
  
        // This displays whatever teh current state of the display is
        FillCodes();
  
        // If we are not currently beeping, play some random beep/bop computer-y sounds
        if ( !beeping )
          ledcWriteTone(channel, random(90,250));
      }
  
      // This is where we solve each code digit
      // The next solve step is a random length to make it take a different time every run
      if ( nextSolve < millis() )
      {
        nextSolve = millis() + solveStep;
        // Set the solve time step to a random length
        solveStep = GetNextSolveStep();
        //
        SolveCode(false);
      }
  
      // Zturn off any beeping if it's trying to beep
      if ( beeping )
      {
        if ( nextBeep < millis() )
        {
          ledcWriteTone(channel, 0);
          beeping = false;
        }
      }
    }
  }
}

/*
 * Lets try and keep all counters to be in the same second for timing
*/
unsigned long GetTimeWithZeroMillis(unsigned long addTime)
{
    return ( (millis() /1000) *1000) + addTime;
}
