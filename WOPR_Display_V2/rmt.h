/*************************************************** 
  RMT RGB Extension
  2020 UNexpected Maker
  Licensed under MIT Open Source

  This code is designed specifically to run on an ESP32. It uses features only
  available on the ESP32 like RMT and ledcSetup.

  W.O.P.R is available at:
  https://unexpectedmaker.com/shop/wopr-missile-launch-code-display-kit
  https://www.tindie.com/products/seonr/wopr-missile-launch-code-display-kit/
  
  Wired up for use with the TinyPICO, TinyS2 or TinyS3 Development Boards
  https://unexpectedmaker.com/shop

  And the TinyPICO Analog Audio Shield
  https://unexpectedmaker.com/shop

 ****************************************************/
 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "Arduino.h"
#include "esp32-hal.h"
#include "adafruit.h"

#define NR_OF_LEDS   5
#define NR_OF_ALL_BITS 24*NR_OF_LEDS

rmt_data_t led_data[NR_OF_ALL_BITS];
rmt_obj_t* rmt_send = NULL;
byte brightness = 50;
uint32_t leds[5];

uint8_t Red( uint32_t col )
{
  return col >> 16;
}

uint8_t Green( uint32_t col )
{
  return col >> 8;
}

uint8_t Blue( uint32_t col )
{
  return col;
}

uint8_t AdjustForBrightness( uint8_t col )
{
  uint8_t col_fixed = round( (float)col * ( (float)brightness/255.0 ) );
  return col_fixed;
}
 
bool RGB_Setup(byte ledPin, byte bright)
{
    if ((rmt_send = rmtInit(ledPin, true, RMT_MEM_64)) == NULL)
    {
      Serial.println("RMT init sender failed!\nSomething went wrong initialising the RMT peripheral for the  RGB LEDs\nHalting!!!\n");
      return false;
    }

    float realTick = rmtSetTick(rmt_send, 100);

    brightness = bright;
    return true;
}

void RGB_FillBuffer()
{
    uint16_t led;
    int col, bit, col_inner;
    int i=0;
    int color[] = {0,0,0};

    for ( led=0; led < 5; led++ )
    {
        color[0] = AdjustForBrightness ( Green( leds[ led ] ) );
        color[1] = AdjustForBrightness ( Red( leds[ led ] ) );
        color[2] = AdjustForBrightness ( Blue( leds[ led ] ) );

        for ( col=0; col<3; col++ )
        {    
            col_inner = color[col];

            for (bit=0; bit<8; bit++)
            {
                if ( col_inner & ( 1<<( 7-bit ) ) ) 
                {
                    led_data[i].duration0 = 8;
                    led_data[i].duration1 = 4;
                }
                else
                {
                    led_data[i].duration0 = 4;
                    led_data[i].duration1 = 8.5;
                }

                led_data[i].level0 = 1;
                led_data[i].level1 = 0;
                i++;
            }
        }
    }

    // Send the data
    rmtWrite(rmt_send, led_data, NR_OF_ALL_BITS);
}

void RGB_Clear( bool update=false )
{
  for ( int i = 0; i < 5; i++ )
        leds[i] = 0;

  if ( update )
    RGB_FillBuffer();
}

void RGB_SetBrightness( uint8_t bright )
{
  brightness = bright;
}
