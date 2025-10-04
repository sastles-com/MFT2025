#pragma once

#include <vector>
#include "common.h"


#include <FastLED.h>

//#define LED_PIN 3
#define LED_PIN 46

#define LED_TYPE    WS2812
#define COLOR_ORDER GRB


namespace neon{
    class LED{
        private:
            TwoWire *wire;
            HWCDC *serial;

            CRGB leds[LED_NUM];
            int num;
        public:
            LED(){
                num = 0;
            };
            void init(TwoWire *w, HWCDC *s){
                wire = w;
                serial = s;

                FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, LED_NUM).setCorrection( TypicalLEDStrip );
                FastLED.setBrightness(  LED_BRIGHTNESS );

                black();

                serial->println("LED Standby");
            };

            int update(){
                FastLED.show();
                return num;
            };

            void setPixel(int num, CRGB color){
                leds[num] = color;
            };


            void black(){
                for(int i=0;i<LED_NUM;i++){
                    leds[i] = CRGB::Black;
                }
                FastLED.show();
            };
    };
}
