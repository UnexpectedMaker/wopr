/*************************************************** 
  War Games - W.O.P.R. Missile Codes
  2020 Alarm Clock feature 
  Licensed under MIT Open Source

  This code is designed specifically to run on an ESP32. It uses features only
  available on the ESP32 like RMT and ledcSetup.

  W.O.P.R is available on tindie
  https://www.tindie.com/products/seonr/wopr-missile-launch-code-display-kit/
  
  Wired up for use with the TinyPICO ESP32 Development Board
  https://www.tinypico.com/shop/tinypico

  And the TinyPICO Audio Shield
  https://www.tinypico.com/shop/tinypico-mzffe-zatnr

  Time format helper
  http://www.cplusplus.com/reference/ctime/strftime/

Controls - 
Button 1 (Right) , change menu option
Button 2 (Left) , Select Current menu option / activate/set change of menu option

If no days are set, alarm will repeat every day (maybe a redo)
Temp alarm is a single event

2020/05/24 - CJW 
Adding this Alarm feature as follows
Added alarm base menu , with show header as first option before buttons take action (TODO : move this to WOPR_Display as core)
Added alarm base code
Added dev set alarm in "X" time
Added alarm prepare code for 5mins before alarm time
Added defcon counter for each min before alarm 
Added defcon 1 - launch movie phase (TODO : maybe an option to show message or random)
Added alarm "active" state - with last char dot as UI indicator
Added Launch code hooks on defcon 1

2020/05/25 - CJW 
Added concept of days and time to alarm delta functions
Added RGB leds get brighter based on solve progress
Tweeked alternative dot flasht to inactive dot
Tweeked Dev time to ve a menu for reseting, and added control to disable this feature 
Clean up code and make "setAlarm" functions
Started on Next Day alarm settings after alarm is cancelled

2020/05/26 - CJW 
Added setting Alarm time menu and controls
Added change alarm hour / min and set alarm flows
Added user set time, so temp alarms rest to this after they ran
Added Alarm mode - Movie, Random , Message
Added Days control and if set alarm is marked only for those days 

When i am bored :)
TODO : look into "next day activation" if alarm is canceled 
TODO : refactor days edit
TODO : should we only allow single alarms if no repeat days are set    

Furture Us 
If there is a V2 board
* Let it have RTC 
* Let it have some form of flash ram
* Let it have some "extra" side buttons to toggle up/down
* Let the reset be easier to get to
* Let it have a rear 3d print case that could hold a battery

 ****************************************************/

#include <numeric> 

#define TEMP_HR_TIME 0                 // Temp set alarm time in x hrs
#define TEMP_MIN_TIME 2                // Temp set alarm time in x min (clamped so no lower than 2)
#define MAX_ALARM_TIME (30*1000)       // 30seconds of alarm time before it auto cancels
#define LED_BASE_BRIGHTNESS 0.05       // Multiplier for RGB Led brightness - Too bright wakes people up
#define MAX_DAYS 7

// to presure the chars written to the matrix , for post modifications
char matrixChars[3][4] = { ' ' };       // init all to empty char to starts

// Alarm stuff
struct tm alarmInfo = {0,-1,-1};        // Set the min / hour for alarm if you want to bypass settings (seconds are ignored)
struct tm lastAlarmState = {0,-1,-1};   // Used to hold the user set alarm time, used when we cancel any active alarm
struct tm alarmChangeTo;                // This is what the menus use for changing , we only apply on exit
unsigned long nextAlarmCycle = 0;

/* 
 * 0 - not set
 * 1 - preview set time (auto moves to phase 2)
 * 2 - time set (alarmActive must be true for it to take effect) 
 * 3 - 6 - count down
 * 7 - launching count down
 * 8 - launching 
 * 9 - alarm active 
 */
uint8_t alarmState = 0; 
bool alarmActive = false;
bool firstTimeMenu = true;

// Day controls for alarm (if all false, then alarm is considered to be at this day) 
bool alarmActiveDays[MAX_DAYS] = { false };    // Monday 0 - Sunday 7 (tm says 1 to 8)

// Program & Menu's state
bool enableShortTime = true;
String enableShortTimeMenu = (String)"Temp "+TEMP_HR_TIME+" - "+TEMP_MIN_TIME;       // Temp dev menu

// MENU_END must be last in list, Exit must be present for backing out of that menu
#define MENU_END "*"
#define MENU_EXIT "Exit"
#define MENU_SET "Set"

String alarmMenus[][8] = {
    {"Enable", "Disable", enableShortTimeMenu, "Alarm Mode", "Alarm Time", "Alarm Days", MENU_EXIT, MENU_END},  // Main Menu
    {"MODE MOVIE", "MODE RANDOM", "MODE MESSAGE", MENU_EXIT, MENU_END},  // Alarm Mode
    {"Hr ", "Min ", MENU_SET, MENU_EXIT, MENU_END},  // Alarm Time
    {"MTWTFSS", MENU_EXIT, MENU_END}  // Alarm Days
};
uint8_t alarmMenuIndex = 0; // Current alarm menu we are in
uint8_t alarmMainMenuItem = 0;
uint8_t alarmSubMenuItem = 0;               // Used for cycling through an option , like days 
uint8_t alarmCodeMode = 0; // 0 - movie simulation, 1 - random sequence, 2 - message
String finalSolvedMessage = "";

void Setup_Alarm() {

    Serial.println((String)"PSRAM Free: "+ESP.getFreePsram()+" Total: "+ESP.getPsramSize() );

/* PSRam checks

   byte* psdRamBuffer = (byte*)ps_malloc(500000);
   Serial.println((String)"PSRAM Free: "+ESP.getFreePsram()+" Total: "+ESP.getPsramSize() );
   free(psdRamBuffer);
   Serial.println((String)"PSRAM Free: "+ESP.getFreePsram()+" Total: "+ESP.getPsramSize() );
*/
}

/*  
 *   Alarm functionality for button 1 actions
 */
bool BUT1Press_Alarm()
{
    // Cancel any alarm that is sounding or going nuclear
    if ( alarmState >= 7 ) {
        CancelCurrentAlarm();
        return true;
    }
    
    // If not in alarm state or not currentState active
    if ( currentMode != 4 || currentState != 1){
        return false;
    }
    
    // Menu toggle for alarm features
    if ( currentState == 1) {

        // Menu is not shown on toggle , so lets at least process the init message first
        if ( !firstTimeMenu ) {

            // If we are on the toggle days to be active
            if ( alarmMenuIndex == 3 && alarmMainMenuItem == 0 ) {

                alarmSubMenuItem++;

                // Cycle through the "days"
                if ( alarmSubMenuItem < alarmMenus[alarmMenuIndex][alarmMainMenuItem].length() ) {
                    Serial.println((String)"BUT1Press_AlarmFeature: In Menu ["+alarmMenuIndex+"] Sub Option > "+alarmSubMenuItem);
                }
                else {
                    alarmSubMenuItem = 0;
                    alarmMainMenuItem++;
                    Serial.println((String)"BUT1Press_AlarmFeature: In Menu ["+alarmMenuIndex+"] Next Option > "+alarmMainMenuItem);
                }
            }
            else {
                // Toggle Alarm menu items, clamp to max
                alarmMainMenuItem++;
    
                // If alarm menu control active
                if ( alarmMenuIndex == 0 ) {
                                
                    // only show short time menu if enabled
                    if ( !enableShortTime && alarmMainMenuItem == 3 )
                    {
                        alarmMainMenuItem++;
                    }
                } 
                Serial.println((String)"BUT1Press_AlarmFeature: In Menu ["+alarmMenuIndex+"] Next Option > "+alarmMainMenuItem);
            }

            // Clamp to menu index max
            if ( alarmMenus[alarmMenuIndex][alarmMainMenuItem] == MENU_END ) {
                alarmMainMenuItem = 0;
            }
        }
        
        // Show alarm menu
        MenuDisplay_Alarm();
          
        return true;
    }
    
    // Alarm mode not in control
    return false;
}

/*  
 *   Alarm functionality for button 2 actions
 */
bool BUT2Press_Alarm()
{
    // Cancel any alarm that is sounding or going nuclear
    if ( alarmState >= 7 ) {
        CancelCurrentAlarm();
        return true;
    }

    // if pressing a button while in phase 1 , move to phase 2 and back to alarm menu
    if ( alarmState == 1 )
    {
        alarmState = 2;                   // Alarm state to set so we keep alarm time info
        return true;
    }
    
    // If not in alarm mode and state not active
    if ( currentMode != 4 ) {
        return false;
    }
    
    // select new alarm main menu
    if ( currentState == 0 ) {
        
        alarmMainMenuItem = 0;
        firstTimeMenu = true;
        DisplayText( "ALARM MENU" , true );
        currentState = 1;
        return true;
    }

    // if not first time in we can treat this button as an action
    if ( !firstTimeMenu ) {
        
        // Main alarm menu controls - Maybe this should be a index not a bool
        if ( alarmMenuIndex == 0 ) {

            // Reset sub menuItem if on main menu
            alarmSubMenuItem = 0;
            
            // "exit" menu option back to main menu, always the last item in the lists
            if ( alarmMenus[alarmMenuIndex][alarmMainMenuItem] == MENU_EXIT ) {
                BackToMainMenu( false );
                return true;
            }
            
            // Alarm is disabled , should we enable it
            if ( alarmMainMenuItem == 0 ) {

                // Lets try an activate any current alarms
                if ( TryAndActivateAlarmState(true) ) {
                    if ( !ApplyAlarmRequestedNextDay() ) {
                        Serial.println("Couldn't set alarm day");
                    }
                }
                else {
                    Serial.println("No Alarm to activate");
                }

            } 
            else if ( alarmMainMenuItem == 1 ) {
                // Disable alarm
                alarmActive = false;
                alarmState = 2; // make sure phase 
                RGB_Clear(true);
            } 
            else if ( alarmMainMenuItem == 2 ) {
                
                // Try and create a temp alarm
                if ( SetTemp_AlarmTime() ) {
                    Serial.println("Temp Alarm set");
                }
                else {
                    Serial.println("Alarm dev time not set");
                }
            } 
            else { // Select Sub menu 

                // "exit" menu option back to main menu, always the last item in the lists
                if ( alarmMenus[alarmMenuIndex][alarmMainMenuItem] == MENU_EXIT ) {
                    alarmMenuIndex = 0;
                    alarmMainMenuItem = 0;
                    alarmSubMenuItem = 0;
                    firstTimeMenu = true;
                    return true;
                }

                Serial.println((String)"BUT1Press_AlarmFeature: To Sub ["+alarmMainMenuItem+"] SubIndex ["+alarmSubMenuItem+"]");

                firstTimeMenu = true;

                // If this menu item is to allow user to change the alarm time
               if ( alarmMainMenuItem == 4 ) {

                    // if lastAlarmState is default
                    if ( lastAlarmState.tm_hour < 0 || lastAlarmState.tm_min < 0 )  {

                        // Get time as base for alarm start
                        if ( !getLocalTime(&alarmChangeTo) ) {
                            Serial.println("Set Alarm failed to obtain time");
                            DisplayText("Bad Set", true);
                            firstTimeMenu = true;
                            return true;
                        }
                    }
                    else {
                        alarmChangeTo = lastAlarmState;
                    }
    
                    alarmChangeTo.tm_sec = 0;
                    Serial.println(&alarmChangeTo, " Start Edit Alarm time %a %H:%M:%S");
                }
                // If we are selecting Set Days 
                else if ( alarmMainMenuItem == 5 ) {
                    // Start inactive next cycle for menu refreshing
                    nextAlarmCycle = GetTimeWithZeroMillis( 0 );
                }

                // So the menu item as a first in
                DisplayText( ">"+alarmMenus[alarmMenuIndex][alarmMainMenuItem] , true);

                alarmMenuIndex = alarmMainMenuItem - 2;
                alarmMainMenuItem = 0;

                firstTimeMenu = true;
                alarmActive = false;    // disable any alarm actived
                RGB_Clear(true);                
                return true;
            }
        } 
        else {
            Serial.println((String)"BUT2Press_AlarmFeature: Sub Menu Option > "+alarmMenus[alarmMenuIndex][alarmMainMenuItem]+" , SubIndex ["+alarmSubMenuItem+"] alarmActive:"+alarmActive);

            // "exit" menu option back to main menu, always the last item in the lists
            if ( alarmMenus[alarmMenuIndex][alarmMainMenuItem] == MENU_EXIT ) {

                // back to main alarm menu , show as first menu
                alarmMenuIndex = 0;
                alarmMainMenuItem = 0;
                firstTimeMenu = true;
    
                DisplayText( "<ALARM MENU" , true);

                return true;
            }

            // Alarm Mode 
            if ( alarmMenuIndex == 1 ) {

                alarmCodeMode = alarmMainMenuItem;
                
                alarmMenuIndex = 0;
                alarmMainMenuItem = 0;
                firstTimeMenu = true;
    
                DisplayText( "MODE SET" , true);
                return true;
            }
            // Set Time mode
            else if ( alarmMenuIndex == 2 ) {

                // Increase hrs
                if ( alarmMainMenuItem == 0 )  {
                    alarmChangeTo.tm_hour ++;
                    if ( alarmChangeTo.tm_hour >= 24 ) {
                        alarmChangeTo.tm_hour -= 24;
                    }
                }
                // Increase mins
                else if ( alarmMainMenuItem == 1 ) {
                    alarmChangeTo.tm_min ++;
                    if ( alarmChangeTo.tm_min >= 60 ) {
                        alarmChangeTo.tm_min -= 60;
                    }
                }                

                // Set this time
                if ( alarmMenus[alarmMenuIndex][alarmMainMenuItem] == MENU_SET ) {

                    struct tm timeinfo;
                    if ( !getLocalTime(&timeinfo) ) {
                        Serial.println("Change Alarm failed to obtain time");
                        DisplayText("Bad Change", true);
                        firstTimeMenu = true;
                        return true;
                    }

                    // Get delta in time for the requests alarm
                    struct tm alarmDelta = GetDeltaBetweenTimes(timeinfo, alarmChangeTo);
                    Serial.print(&alarmChangeTo, " Change Alarm to @ %a %H:%M:%S");
                    Serial.println((String)" in Days:"+alarmDelta.tm_wday);

                    // Lets make sure we dont have any pending alarm time when we set it
                    alarmInfo.tm_hour = -1;
                    alarmInfo.tm_min = -1;

                    if ( !CreateNextAlarm( alarmChangeTo ) ) {
                        Serial.println("Alarm failed to create");
                    }
                    else {
                        // If we have any days set, apply that to the alarm time
                        if ( !ApplyAlarmRequestedNextDay() ) {
                            Serial.println("Alarm failed to apply days");
                        }
                        else {
                            // Store this state if we set am alarm
                            lastAlarmState = alarmChangeTo;
                        }
                    }
                }                
            }
            // Set Days to activate on
            else if ( alarmMenuIndex == 3 ) {

                // if we are on the first option
                if  ( alarmMainMenuItem == 0 )
                {
                    // Toggle day active/inactive
                    alarmActiveDays[alarmSubMenuItem] = !alarmActiveDays[alarmSubMenuItem];
//                    Serial.println((String)"BUT2Press_AlarmFeature: Toggle day "+alarmSubMenuItem);
                    nextAlarmCycle = GetTimeWithZeroMillis( 1000 );
                }
            }
            else {
                Serial.println((String)"BUT2Press_AlarmFeature: MenuIndex "+alarmMenuIndex+" unknown ");
            }
            

            // TODO : action on sub menu item
        }
    }
    
    // Show alarm menu, if we are in active state
    if ( currentState == 1 ) {
        MenuDisplay_Alarm();
    }

    // Disable first time in 
    firstTimeMenu = false;

    return true;
}

/*  
 *  Allow idle test,(true) if we are are ok with screen being blanked
*/
bool IdleTest_Alarm(bool blankDisplay)
{
    bool allowIdle = blankDisplay;
    
    if  ( blankDisplay ) {
    
        // TODO : allow screen to blank if alarm is pending while at night
        if ( alarmState >= 2 && alarmActive ) {
        
            // TODO : time check for over "X" hour 
        }
        
        // Return core blanking state
        allowIdle = ALLOW_CLOCK_BLANKING;
    }
    else {
    
        // If we are in countdown phase , and on alarm main menu
        if ( alarmState >= 3 && alarmState <= 6 && currentMode == 4 && currentState == 1 && alarmMenuIndex == 0 ) {  
            // Stop idle as we want to show our alarm countdown
            allowIdle = false;
        }
        else {
            // Else we allow idle , if we are not finding the code/launching or alarming
            allowIdle = ( alarmState < 7 );
        }
    }

    // If we are allowing idle - lets clean up 
    if ( allowIdle ) {    
        firstTimeMenu = true;
        alarmMenuIndex = 0;
        alarmMainMenuItem = 0;
        alarmSubMenuItem = 0;
        currentState = 0;
    }
    
    return allowIdle;   
}

/*  
 *   Main loop for alarm feature
 */
void Loop_Alarm()
{
    // Alarm disabled or we are editing alarms - early exit
    if ( !alarmActive ) {

        // For menu refreshing
        if ( nextAlarmCycle < millis() ) {

            nextAlarmCycle = GetTimeWithZeroMillis( 1000 );
        
            // If we are on the toggle days to be active
            if ( alarmMenuIndex == 3 && alarmMainMenuItem == 0 ) {
                MenuDisplay_Alarm();
            }
        }
        return;
    }

    // Alarm activated - any button to cancel
    if ( alarmState == 9 ) {

        // If we been idle longer than IDLE_TIME+NODISPLAY_TIME , and if alarm approves
        if ( nextIdleTime+(MAX_ALARM_TIME-IDLE_TIME)  < millis() ) {            
            Serial.println((String)"The only way to win is not to play");
            // Auto cancel alarm ,
            CancelCurrentAlarm();
        }
        else {
    
            RGB_Rainbow(10);
        
            // Beep and alarm active show messages
            if ( nextSolve < millis() ) {                
                nextSolve = millis() + 500;
                
                if ( animCounter&PER_SECOND_TOGGLE ) {
                    ledcWriteTone(channel, 1500);
                    DisplayText("...ALARM...", false);
                } 
                else {
                    ledcWriteTone(channel, 0 );
                    DisplayText(finalSolvedMessage, false);
                }
            }
        }
    } 
    // Flash the Code/RGB defcon
    else if ( alarmState == 8 ) {

        // The next solve step is a random length to make it take a different time every run
        if ( nextSolve < millis() ) {

//            Serial.println((String)"Nuke warning : "+( animCounter&PER_HALF_SECOND_TOGGLE ));            
            nextSolve = millis() + 250;

            // Strob RGB
            if ( animCounter&PER_HALF_SECOND_TOGGLE ) {
                Clear();
                RGB_SetDefcon_Alarm(1);
            }
            else {
                DisplayText(finalSolvedMessage, false);
                RGB_Clear(true);
            }
        }
    }
    // Else alarm in solve code phase - and not yet solved
    else if ( alarmState == 7 ) {

        // Pulsing RGB to solved level
        RGBPulsing_Alarm( 10 );

        // The next solve step is a random length to make it take a different time every run
        if ( nextSolve < millis() ) {
            
            // Set the solve time step to a random length
            solveStep = GetNextSolveStep();
            nextSolve = millis() + solveStep;
         
            // process solving as alarm mode
            SolveCode(true);
            FillCodes();

            // If solved - lets move to Launching Alarm and noise
            if ( solveCount == solveCountFinished ) {

                // Capture the message for flashing
                finalSolvedMessage = "";
                for ( int i = 0; i < 3; i++ )
                {
                    // There are 4 digits per LED driver
                    for ( int d = 0; d < 4; d++ )
                    {
                        finalSolvedMessage += matrixChars[i][d]; 
                    }
                }
                
                ChangeAlarmState( alarmState+1 );
                nextSolve = millis();

                Serial.println((String)"Launching Warning : "+finalSolvedMessage);
            }
        }
        else if ( solveCount != solveCountFinished ) {
            FillCodes();
        }
    } 

    // Process alarm regular cycle
    if ( nextAlarmCycle < millis() ) {

        nextAlarmCycle = GetTimeWithZeroMillis( 1000 );
        
        // Enable alarm active dot
        Display();    

        struct tm timeinfo;
        if ( !getLocalTime(&timeinfo) ) {
            Serial.println("Alarm Failed to obtain time");
            DisplayText("Bad Alarm", true);
            currentState = 0;
            return;
        }

        // no alarm time ready - leave 
        if ( alarmState < 2 ) {
            return;
        }

        // lets update defcon state based on current phase, if below phase 7
        if ( (currentMode == 4 || currentMode == 3) && currentState == 1 && alarmState < 7 ) {
            RGB_SetDefcon_Alarm(8-alarmState);
        }

        // alarm prep delta based on phase 3 to 6 , if we on phase 3 <= 5min , 6 <= 2mins
//        Serial.println((String)"Alarm Phase["+alarmState+"] : "+(8-alarmState)+" adjusted -"+(7-alarmState) );
        struct tm alarmPrepDelta = createTimeAdjusted( alarmInfo , 0 , -(7-alarmState) );
//        Serial.print(&alarmInfo, "Alarm time %a %H:%M:%S - ");
//        Serial.println(&alarmPrepDelta, " Next Defcon time %a %H:%M:%S");
        // If in phase 8 - lets move to alarm phase once we have reached the target time
        if ( alarmState == 8 ) {  

            // If we are within 1 second or jumped a day - lets change to next phase
            struct tm alarmDelta = GetDeltaBetweenTimes(timeinfo, alarmInfo);
            if ( alarmDelta.tm_wday != 0 || alarmDelta.tm_sec <= 1 ) {
                ChangeAlarmState( alarmState+1 );
                Clear();                
                
                nextIdleTime = GetTimeWithZeroMillis(IDLE_TIME);      // Set check idle to be in the future
                
                Serial.println((String)"Joshua, You nuked them all : "+( animCounter&PER_SECOND_TOGGLE ));            
                Serial.println(&alarmDelta, " @ %H:%M:%S");
            }
        }
        // If in phase 7 and alarm 
        else if ( alarmState >= 3 && alarmState <= 6 ) {  

            // Get a delta till timer goes nuck mode :P 
            struct tm alarmDelta = GetDeltaBetweenTimes(timeinfo, alarmInfo);

            // Prep Delta time holds the next min we need to toggle
            if ( alarmPrepDelta.tm_min == timeinfo.tm_min ) {

                ChangeAlarmState( alarmState+1 );
                
                // if we are going into defcon 1 (1min before alarm) - start movie squence
                if ( alarmState == 7 ) {
                    currentMode = 4;         // force alarm into alarm display mode
                    alarmMenuIndex = 0;      // force alarm menu to main
                    alarmMainMenuItem = 0;   //
                    currentState = 1;        // force mode active
                    
                    nextPixelHue = 0;
                    
                    ResetCode( alarmCodeMode );
                    Clear();

                    solveStepMulti = 0.80;   // Solves code before 1 seconds, so we can pulse flash code/launching
                    solveStep = GetNextSolveStep();
                }
                
                RGB_SetDefcon_Alarm(8-alarmState);
                Serial.print((String)"Alarm To Defcon : "+(8-alarmState) );
                Serial.println(&alarmDelta, " @ %H:%M:%S");
            }
            else {
//                Serial.print((String)"Alarm In Defcon : "+(8-alarmState) );
//                Serial.println(&alarmDelta, " @ %H:%M:%S");
                  
                // only if we are in alarm mode, and in alarm state, and not in an alarm sub menu
                if ( currentMode == 4 && currentState == 1 && alarmMenuIndex == 0 ) {
                    // Display Alarm Delta if no user interaction
                    if ( nextIdleTime < millis() ) {
                        DisplayTime(alarmDelta);
                    }
                }
            }
        }
        // Else if alarm time is set - lets see if we are about to go defcon
        else if ( alarmState == 2 ) {

            // Are we to prep this day ?
            struct tm alarmDelta = GetDeltaBetweenTimes(timeinfo, alarmPrepDelta);
            // if delta days is zero
            if ( alarmDelta.tm_wday != 0 ) {
                // if we are within 5min prep time lets move to phase 3
                if ( (alarmPrepDelta.tm_hour == timeinfo.tm_hour) && (alarmPrepDelta.tm_min == timeinfo.tm_min) ) {
                  
                    ChangeAlarmState( alarmState+1 );
                    alarmPrepDelta.tm_min += 1;
    
                    if ( alarmPrepDelta.tm_min > 60 ) {
                        alarmPrepDelta.tm_min -= 60;
                    }
    
                    Serial.println("Alarm Defcon Activated" );
                }
                else {
                    // we are alarmed to go off - should we show how long ?
                    struct tm alarmDelta = GetDeltaBetweenTimes(timeinfo, alarmPrepDelta);
                    int alarmDays = alarmDelta.tm_wday;

                    // Add day from current time
                    alarmDelta.tm_wday += timeinfo.tm_wday;
                    if ( alarmDelta.tm_wday >= MAX_DAYS ) {
                        alarmDelta.tm_wday -= MAX_DAYS;
                    }

                    if ( alarmDays != 0 ) {
                        Serial.print(&alarmDelta, "Not Today %a %H:%M:%S");
                        Serial.print((String)" in ["+alarmDays+( alarmDays > 1 ? "] days" : "] day" ) );
                        Serial.println(&timeinfo, " time %a %H:%M:%S");
                    }
                    else {
                        // under 24hrs before next alarm
                        /*
                        Serial.print(&alarmDelta, "Next Alarm %a %H:%M:%S");
                        Serial.print((String)" in ["+alarmDays+( alarmDays > 1 ? "] days" : "] day" ) );
                        Serial.println(&timeinfo, " time %a %H:%M:%S");
                        */
                        // Core Controls not touched since "X" time
                        if ( nextIdleTime < millis() ) {
        
                            // only if we are in alarm mode, and in alarm state, and not in an alarm sub menu
                            if ( currentMode == 4 && currentState == 1 && alarmMenuIndex == 0 ) {
                
                                if ( animCounter&PER_SECOND_TOGGLE ) {
                                    DisplayTime(alarmDelta);
                                }
                            }
                        }    
                    }                    
                }
            }
        }
        else if ( alarmState < 2 ) {  
            Serial.println("alarmState < 2");
            SetNextAlarmDay();
        }   
    }
}

/*
 *  Try and activate any current or future alarms
 */
bool TryAndActivateAlarmState(bool showNextAlarm)
{    
    if ( alarmActive ) {
        Serial.println("TryAndActivateAlarmState when we are already active");
        return false;
    }
    
    // If the alarm time is invalid
    if ( alarmInfo.tm_hour < 0 || alarmInfo.tm_min < 0 )  {

        Serial.println("Try SetNextAlarmDay");

        if ( !SetNextAlarmDay() ) {
            Serial.println("No Alarm time Set");
            return false;
        }
    } 
    else {
        // If we have an alarm info set , so lets activate
        alarmActive = true;
    }
    
    // if we have set the alarm active 
    if ( alarmActive )  {

        Serial.println("Try alarmActive");

        struct tm timeinfo;
        if ( !getLocalTime(&timeinfo) ) {
            Serial.println("Alarm Active failed to obtain time");
            DisplayText("Bad Alarm", true);
            alarmActive = false;
            RGB_Clear(true);
        }
        else {
            // make sure alarmState is updated 
            struct tm alarmDelta = GetDeltaBetweenTimes(timeinfo, alarmInfo);
            int alarmDays = alarmDelta.tm_wday;
            Serial.print((String)"Trying to prime for ["+alarmDays+( alarmDays > 1 ? "] days, " : "] day, " ) );
            Serial.print(&alarmDelta, " in %H hrs %M mins %S seconds");
            Serial.println(&timeinfo, " ,current time %a %H:%M:%S");

            // Set Alarm state based on how long before alarm is activeated, state 2 means longer than 5mins
            alarmState = ( alarmDays == 0 && alarmDelta.tm_hour == 0 && alarmDelta.tm_min <= 5 ) ? 7 - alarmDelta.tm_min : 2; 
            Serial.println((String)"Alarm alarmState : "+alarmState );

            // Add the days delta to alarm info and clamp to a 7 day week
            alarmInfo.tm_wday += alarmDays;
            while ( alarmInfo.tm_wday >= MAX_DAYS )
            {
                alarmInfo.tm_wday -= MAX_DAYS;
            }
        
            Serial.println(&timeinfo, "Current time [%Y] %a %D %H:%M:%S");
            Serial.print((String)"Alarm Mode ["+alarmCodeMode+"] State: "+alarmState);
            Serial.print(&alarmInfo, " @ : %a %D %H:%M:%S");
            if ( alarmDays == 0 ) {
                Serial.println(" Today");
            }
            else {    
                Serial.println((String)" in ["+alarmDays+( alarmDays > 1 ? "] days" : "] day" ) );
            }

            // If we are already in Find code or higher, abort - would need to break the loop function as we have not initialized parts
            if ( alarmState >= 7 ) {
                alarmActive = false;
                Serial.println((String)"Alarm Mode ["+alarmCodeMode+"] , Too soon for State:"+alarmState+" , launching count down error min: "+alarmDelta.tm_min );
            }
            else {
                if ( alarmState >= 3 ) {
                    Serial.println((String)"Alarm Mode ["+alarmCodeMode+"] Activated State : "+alarmState+" , in defcon launching in min:"+alarmDelta.tm_min+" , showNextAlarm:"+showNextAlarm );
                }
                else {
                    Serial.println((String)"Alarm Mode ["+alarmCodeMode+"] Activated State : "+alarmState+" , shall we play a game, launching in hr:"+alarmDelta.tm_hour+" min: "+alarmDelta.tm_min+" , showNextAlarm:"+showNextAlarm );
                }
    
                if ( showNextAlarm ) {
                    // TODO : show - day : time for next alarm
                    // for now exit to main menu
                    BackToMainMenu( false );
                }
            }
        }
    }

    return alarmActive;
}
    
/*
 * If we have days set & no alarm set - lets see if we have any more alarms
 */
bool SetNextAlarmDay()
{
    // If we have an active alarmState
    if ( alarmState >= 2 ) {
        return false;
    }

    // Reset User set alarm time
    alarmInfo = lastAlarmState;
    alarmInfo.tm_sec = 0;

    // if we do not have an active user alarm time set
    if ( alarmInfo.tm_hour == -1 || alarmInfo.tm_min == -1 )  {
        Serial.println("SetNextAlarmDay Has no user time set");
        return false;
    }

    return ApplyAlarmRequestedNextDay();
}

/*
 * Set alarm for next "day" required
 */
bool ApplyAlarmRequestedNextDay()
{
    struct tm timeinfo;
    if ( !getLocalTime(&timeinfo) ) {
      Serial.println("SetNextAlarmDay Failed to obtain time");
      return false;
    }

    // Check if we have any days controls set
    int sum = std::accumulate(alarmActiveDays, alarmActiveDays + MAX_DAYS, 0);
    // If no active days are set, we must have a single alarm set
    if ( sum == 0 ) {
        Serial.println(&alarmInfo, "Applying Set for [%Y] %D %H:%M:%S");
        alarmActive = true;
    }
    else {

        // Start at current day
        int checkDay = timeinfo.tm_wday-1;

        // Shouldn't be a inf loop as we have a count of at least one day set
        while ( !alarmActiveDays[checkDay] ) {
            checkDay++;
            if ( checkDay >= MAX_DAYS ) {
                checkDay = 0;        
            }
        };

        Serial.println((String)"Alarm next Day:"+(checkDay+1)+" currentDay:"+(timeinfo.tm_wday)+" From Total:"+sum );
        alarmInfo.tm_wday = checkDay+1;

        alarmActive = true;
    }

    // Info dump if priming alarm
    if ( alarmActive ) {
        struct tm alarmDelta = GetDeltaBetweenTimes(timeinfo, alarmInfo);
        int alarmDays = alarmDelta.tm_wday;

        // Set delta day to match alarm day for debug
        alarmDelta.tm_wday = alarmInfo.tm_wday;
        Serial.print((String)"Alarm primed for ["+alarmDays+( alarmDays > 1 ? "] days, " : "] day, " ) );
        Serial.print(&alarmDelta, " on %a in %H hrs %M mins %S seconds");
        Serial.println(&timeinfo, " ,current time %a %H:%M:%S");
    }
    
    return alarmActive;
}

/*
 * Cancel the current alarm , set next alarm / state as required
 */
void CancelCurrentAlarm()
{
    Serial.println("CancelCurrentAlarm");
    // Clear alarm State
    alarmState = 0;
    alarmInfo.tm_hour = -1;
    alarmInfo.tm_min = -1;
    RGB_Clear(true);

    // If we didnt set a next alarm day , lets cancel activation on exit
    BackToMainMenu( !SetNextAlarmDay() );
}

/*
 * Show the current alarmMode menu item
 */
void MenuDisplay_Alarm()
{
    firstTimeMenu = false;

    // Main Menu current option
    if ( alarmMenuIndex == 0 ) {

        // Only allow enable / disable based on active state
        if ( (alarmMainMenuItem == 0 && alarmActive ) || ( alarmMainMenuItem == 1 && !alarmActive ) ) {

            alarmMainMenuItem++;
            if ( alarmMenus[alarmMenuIndex][alarmMainMenuItem] == MENU_END ) {
                alarmMainMenuItem = 0;
            }
        }
        
        // Main Menu if not selected
        DisplayText( alarmMenus[alarmMenuIndex][alarmMainMenuItem] , false );
    }
    else { // other menus        

        // Set Time menu
        if ( alarmMenuIndex == 2 ) {

            String postFix = "";
            
            // Set Hour
            if ( alarmMainMenuItem == 0 ) {
                postFix = alarmChangeTo.tm_hour;
            } 
            // Set Min
            else if ( alarmMainMenuItem == 1 ) {
                postFix = alarmChangeTo.tm_min;
            }

            DisplayText( alarmMenus[alarmMenuIndex][alarmMainMenuItem] + postFix , false );
        }
        else if ( alarmMenuIndex == 3 && alarmMainMenuItem == 0 ) {

            // Flash the Active day (letter from alarmMainMenuItem)
            String dayText = alarmMenus[alarmMenuIndex][alarmMainMenuItem];

            if ( !(animCounter&PER_SECOND_TOGGLE) ) {
                dayText.setCharAt(alarmSubMenuItem, '*');
            }

            DisplayText( dayText , false );
        }
        else {
            DisplayText( alarmMenus[alarmMenuIndex][alarmMainMenuItem] , false );
        }
    }
}

/*
 * Set alarmState generic function 
 */
void ChangeAlarmState(uint8_t newState)
{
    Serial.println((String)"ChangeAlarmState ["+alarmState+"] to : "+newState);
    alarmState = newState;

    // Force next frame processing
    nextAlarmCycle = millis();
}

/*  
 *   Alarm back to main menu clean up
 */
void BackToMainMenu(bool cancelActiveAlarm)
{
    firstTimeMenu = true;
    alarmMenuIndex = 0;
    alarmMainMenuItem = 0;
    alarmSubMenuItem = 0;
    currentState = 0;      
    RGB_Clear(true);

    //Shutdown the audio it might be beeping
    ledcWriteTone(channel, 0);
    
    // if we are canceling an alarm on exit
    if ( cancelActiveAlarm ) {
        
        alarmActive = false;               // TODO : false if a single alarm no days set
        ResetCode( currentMode );

        Serial.println((String)"BackToMainMenu cancelActiveAlarm");

        // TODO : when canceling process out via showing the next alarm time if any
    }
    
    DisplayText( "MENU" , true);

    Serial.println((String)"BackToMainMenu alarmActive:"+alarmActive);
}

/*
 * Sets the RGB of the defcon level 5 to 1 - Blue only as its hue is lower
 * Always connts to defcon 5 incases where alarm is under 5mins away
 * 
 * Applies a brightness reduction so defcon 1 is 25% max 
 * (alarm lights might wake people up early - the worst ever being woken up just before the alarm) 
 */
void RGB_SetDefcon_Alarm( byte level )
{
    // No defcon till we in phase 3 or higher
    if ( alarmState < 3 || alarmState == 7 )
    {
        return;
    }

    // Clear Defcon Leds from defconLevel-1 to 0
    byte defconClear = constrain(level-2, 0, 4);     // Level needs to be clamped to between 0 and 4;    
    while( defconClear > 0) {
        defconClear--;
        leds[defconClear] = Color(0,0,0);
    };
    
    // Set Defcon Leds from defconLevel to 4
    byte defconLevel = constrain(level-1, 0, 4);     // Level needs to be clamped to between 0 and 4

    do {
        // if we are at defcon 1
        if ( level <= 1 ) {

            // If solved then set RGB to meduim white
            if ( solveCount == solveCountFinished) {
                leds[defconLevel] = Color(128,128,128);
            }
        }
        else {
            // Led gets bright as it gets closer to 1 , 25% max at 1
            leds[defconLevel] = Color(0,0,255) * (LED_BASE_BRIGHTNESS*(5-defconLevel));
        }
        defconLevel++;
    } while( defconLevel < 5);

    RGB_FillBuffer();
}

/*
 * Strobes the RGB from current solved level to min and backup
 */
void RGBPulsing_Alarm(int wait)
{
    if ( nextRGB < millis() ) {
            
        // decrease wait next time as we solve more
        nextRGB = millis() + ( wait - ( ( (float)solveCount / ( float)solveCountFinished ) * wait ) );
        nextPixelHue += wait;
        
        if ( nextPixelHue >= 511 ) {
          nextPixelHue -= 510;
        }
    
        // HUE is used as a +/- range from 
        float ledBright = (LED_BASE_BRIGHTNESS*5)+( ( (float)solveCount /( float)solveCountFinished ) * (1-(LED_BASE_BRIGHTNESS*5)) );
        if ( nextPixelHue < 256 ) {
            ledBright = (ledBright*nextPixelHue)/256;
        }
        else {
            ledBright = (ledBright*(511-nextPixelHue))/256;
        }
        
        // For each RGB LED
        for(int i=0; i<5; i++)
        {
            leds[i] = Color(0,0,255) * (ledBright);
        }
    
        // Update RGB LEDs
        RGB_FillBuffer();
    }
}

/*
 * Set next alarm day & time
 * 
*/
bool CreateNextAlarm( struct tm nextAlarmInfo )
{
    Serial.println("CreateNextAlarm");
    struct tm timeinfo;
    if ( !getLocalTime(&timeinfo) ) {
      Serial.println("CreateNextAlarm Failed to obtain time");
      return false;
    }

    struct tm alarmDelta = GetDeltaBetweenTimes(timeinfo, nextAlarmInfo);
    int alarmDays = alarmDelta.tm_wday;

    // Set Alarm state based on how long before alarm is activeated
    alarmState = ( alarmDays != 0 && alarmDelta.tm_hour != 0 && alarmDelta.tm_min >= 6 ) ? 2 : 8 - alarmDelta.tm_min;    

    // All go lets make this alarm for the time requested
    alarmInfo = nextAlarmInfo;
    Serial.println(&alarmInfo, "CreateNextAlarm - Set alarminfo [%Y] %D %H:%M:%S");

    return TryAndActivateAlarmState( true );
}


/*
 * Get the delta time between two times within a 24hour period
 * 
 * If only they had allowed -/+/* on tm struct :| - lets do it the old fashioned way
 * 
*/
struct tm GetDeltaBetweenTimes(struct tm current, struct tm future)
{
//    Serial.println(&current, "current %d %H:%M:%S - day of week [%u] %a");
//    Serial.println(&future, "future %d %H:%M:%S - day of week [%u] %a");
    
    struct tm deltaTime = future;

    // Calculate the number of days we are away by adding as *24hrs
    int dayDelta = deltaTime.tm_wday - current.tm_wday;
    deltaTime.tm_hour += (dayDelta*24);  

    deltaTime.tm_wday = 0; // Zero delta days 
//    Serial.println((String)"GetDeltaBetweenTimes sum delta days:"+dayDelta+"  delta day hrs:"+(deltaTime.tm_hour/24));
      
    // Subtract seconds
    deltaTime.tm_sec -= current.tm_sec;  
    if ( deltaTime.tm_sec < 0 ) {
        deltaTime.tm_sec += 60;
        deltaTime.tm_min--;
    }

    // Subtract mins
    deltaTime.tm_min -= current.tm_min;
    if ( deltaTime.tm_min < 0 ) {
        deltaTime.tm_min += 60;
        deltaTime.tm_hour -= 1;
    }

    // Subtract hours
    deltaTime.tm_hour -= current.tm_hour;  
    if ( deltaTime.tm_hour < 0 ) {
        deltaTime.tm_hour += 24;

        // lets increase days as time is behind current time
        deltaTime.tm_wday += 1;
        if ( deltaTime.tm_wday >= MAX_DAYS ) {
            deltaTime.tm_wday -= MAX_DAYS;
        }
//        Serial.println((String)"GetDeltaBetweenTimes hr pos delta days:"+deltaTime.tm_wday);
    }

    // Clamp mins within 60
    while ( deltaTime.tm_min >= 60 ) {
        deltaTime.tm_min -= 60;
        deltaTime.tm_hour += 1;
    };

    // Clamp hours within 24
    while ( deltaTime.tm_hour >= 24 ) {
        deltaTime.tm_hour -= 24;

        // lets increase days
        deltaTime.tm_wday += 1;
        if ( deltaTime.tm_wday >= MAX_DAYS ) {
            deltaTime.tm_wday -= MAX_DAYS;
        }
//        Serial.println((String)"GetDeltaBetweenTimes day pos delta days:"+deltaTime.tm_wday);
    }

//    Serial.println((String)"GetDeltaBetweenTimes delta days:"+deltaTime.tm_wday);

    return deltaTime;
}

/*  
 *   Sets time based no dev hr/min - only if feature is enabled
 */
bool SetTemp_AlarmTime()
{
    struct tm timeinfo;
    if ( !getLocalTime(&timeinfo) ) {
      Serial.println("SetTemp_AlarmTime Failed to obtain time");
      return false;
    }

    // If we are in the lower 30seconds on current time, lets add a min
    uint8_t alarmInMin = TEMP_MIN_TIME + ( timeinfo.tm_sec < 30 ? 1 : 0 );
    // Clamp so we must have at least 2 Alarm
    if ( alarmInMin <= 2 ) {
        alarmInMin = 2;
    }
    uint8_t alarmInHr = TEMP_HR_TIME;

    // Clamp dev time to within 24hrs
    if ( alarmInMin >= 60 ) { 
        alarmInMin -= 60;    
        alarmInHr ++;
        if ( alarmInHr >= 24 ) {
            alarmInHr = 23;
            alarmInMin = 59;
        }
    }

    Serial.println((String)"SetTemp_AlarmTime in Hr:"+alarmInHr+" Min:"+alarmInMin);

    struct tm nextAlarm = createTimeAdjusted( timeinfo , alarmInHr , alarmInMin );

    // TODO : check if we have an active alarm state - and its not within the active delta 
    // if yes , lets stop alarm - and set this in place

    // Lets make sure we dont have any pending alarm time when we set it
    alarmInfo.tm_hour = -1;
    alarmInfo.tm_min = -1;
    alarmActive = false;

    if ( CreateNextAlarm( nextAlarm ) )
    {
        return enableShortTime;
    }

    // we failed to createNextAlarm type ?
    Serial.println("SetTemp_AlarmTime Failed to Create Alarm");
    return false;
}

/*
 * Creates a delta time struct, clamping min and hours
 */
struct tm createTimeAdjusted(struct tm baseTime , int adjustHour, int adjustMin )
{
//    Serial.println((String)"createTimeAdjusted["+adjustHour+"]["+adjustMin+"]");
    struct tm adjustedTime = baseTime;
  
    adjustedTime.tm_sec = 0;
    adjustedTime.tm_min += adjustMin;
  
    // Clamp mins to be within 60
    while ( adjustedTime.tm_min < 0 ) {
        adjustedTime.tm_min += 60;
        adjustedTime.tm_hour -= 1;
    };
    
    while ( adjustedTime.tm_min >= 60 ) {
        adjustedTime.tm_min -= 60;
        adjustedTime.tm_hour += 1;
    };
  
    adjustedTime.tm_hour += adjustHour;
    
    // Clamp hours to be within 24hrs
    while ( adjustedTime.tm_hour < 0 ) {
        adjustedTime.tm_hour += 24;
    }
    while ( adjustedTime.tm_hour >= 24 ) {
        adjustedTime.tm_hour -= 24;
    }    
  
    // Future us , could include day and month increases 
  
//   Serial.println(&adjustedTime, "adjustedTime %d %H:%M:%S - day of week [%u]");
  
    return adjustedTime;
}

/*
 * Apply alarm specific display modification to the buffer
 * 
 */
void Display_Alarm()
{
    // Set matrix dot if alarm is enabled on digit 11 (last digit on matrix 2) - only if alarm active and we not in final launch phase
    matrix[2].writeDigitAscii( 3, matrixChars[2][3], GetDotState_Alarm( 2, 3, matrixChars[2][3], false ) );

    // Show the dot for alarm days Mon,Tue,Wed,Thr,Fri,Sat,Sun
    if ( alarmMenuIndex == 3 && alarmMainMenuItem == 0 ) {
        matrix[0].writeDigitAscii( 0, matrixChars[0][0], alarmActiveDays[0] );
        matrix[0].writeDigitAscii( 1, matrixChars[0][1], alarmActiveDays[1] );
        matrix[0].writeDigitAscii( 2, matrixChars[0][2], alarmActiveDays[2] );
        matrix[0].writeDigitAscii( 3, matrixChars[0][3], alarmActiveDays[3] );
        matrix[1].writeDigitAscii( 0, matrixChars[1][0], alarmActiveDays[4] );
        matrix[1].writeDigitAscii( 1, matrixChars[1][1], alarmActiveDays[5] );
        matrix[1].writeDigitAscii( 2, matrixChars[1][2], alarmActiveDays[6] );
    }
}

/*
 * Apply alarm specific DotState modification for the ledDriver & ledDigit
 * 
 * Stores the digit for "cycle" display update for flashing etc
 * 
 */
bool GetDotState_Alarm(int ledDriver, int ledDigit, char charByte, bool ledDotState)
{  
    matrixChars[ledDriver][ledDigit] = charByte;

    // Alarm active dot - if we are on last digit
    if ( ledDriver == 2 && ledDigit == 3 ) {
        // we invert toggle so dot alternates against core
        ledDotState = (alarmState < 7 && !( animCounter&PER_SECOND_TOGGLE ) && alarmActive);
    }
  
    // TODO : Day of the week DOT state 
  
    return ledDotState;
}
