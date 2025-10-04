#pragma once

#include <vector>
#include "common.h"

// FastLEDを先にインクルードして、名前空間の競合を避ける
#include <FastLED.h>

// 4ストリップ構成定義（AtomS3Rで利用可能なGPIOピン）
#define LED_STRIP1_PIN 46  // 既存のメインLEDピン
#define LED_STRIP2_PIN 3   // 追加ストリップ
#define LED_STRIP3_PIN 7   // 追加ストリップ  
#define LED_STRIP4_PIN 8   // 追加ストリップ

#define LED_TYPE    WS2812
#define COLOR_ORDER GRB

// 各ストリップのLED数（800を4分割）
#define LEDS_PER_STRIP 200
#define NUM_STRIPS 4

namespace neon{
    class LED{
        private:
            TwoWire *wire;
            HWCDC *serial;

            // 4ストリップ分のLED配列（FastLEDのCRGBを使用）
            ::CRGB strip1[LEDS_PER_STRIP];
            ::CRGB strip2[LEDS_PER_STRIP];
            ::CRGB strip3[LEDS_PER_STRIP];
            ::CRGB strip4[LEDS_PER_STRIP];
            
            int num;

            // 論理LEDインデックスから物理ストリップとインデックスを計算
            void getStripIndex(int logicalIndex, int& stripNum, int& stripIndex) {
                stripNum = logicalIndex / LEDS_PER_STRIP;
                stripIndex = logicalIndex % LEDS_PER_STRIP;
            }

        public:
            LED(){
                num = 0;
            };
            
            void init(TwoWire *w, HWCDC *s){
                wire = w;
                serial = s;

                // 4ストリップをFastLEDに登録
                FastLED.addLeds<LED_TYPE, LED_STRIP1_PIN, COLOR_ORDER>(strip1, LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
                FastLED.addLeds<LED_TYPE, LED_STRIP2_PIN, COLOR_ORDER>(strip2, LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
                FastLED.addLeds<LED_TYPE, LED_STRIP3_PIN, COLOR_ORDER>(strip3, LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
                FastLED.addLeds<LED_TYPE, LED_STRIP4_PIN, COLOR_ORDER>(strip4, LEDS_PER_STRIP).setCorrection(TypicalLEDStrip);
                
                FastLED.setBrightness(LED_BRIGHTNESS);

                black();

                serial->println("LED 4-Strip Controller Standby");
                serial->printf("Strip configuration: %d LEDs per strip, %d strips total\n", LEDS_PER_STRIP, NUM_STRIPS);
            };

            int update(){
                FastLED.show();
                return LED_NUM;
            };

            // 外部からは0-799のインデックスでアクセス、内部で適切なストリップに振り分け
            void setPixel(int logicalIndex, ::CRGB color){
                if (logicalIndex < 0 || logicalIndex >= LED_NUM) return;
                
                int stripNum, stripIndex;
                getStripIndex(logicalIndex, stripNum, stripIndex);
                
                switch(stripNum) {
                    case 0: strip1[stripIndex] = color; break;
                    case 1: strip2[stripIndex] = color; break;
                    case 2: strip3[stripIndex] = color; break;
                    case 3: strip4[stripIndex] = color; break;
                }
            };

            // 特定のストリップの特定のLEDを直接制御（デバッグ用）
            void setStripPixel(int stripNum, int stripIndex, ::CRGB color){
                if (stripNum < 0 || stripNum >= NUM_STRIPS) return;
                if (stripIndex < 0 || stripIndex >= LEDS_PER_STRIP) return;
                
                switch(stripNum) {
                    case 0: strip1[stripIndex] = color; break;
                    case 1: strip2[stripIndex] = color; break;
                    case 2: strip3[stripIndex] = color; break;
                    case 3: strip4[stripIndex] = color; break;
                }
            };

            void black(){
                for(int i = 0; i < LEDS_PER_STRIP; i++){
                    strip1[i] = ::CRGB::Black;
                    strip2[i] = ::CRGB::Black;
                    strip3[i] = ::CRGB::Black;
                    strip4[i] = ::CRGB::Black;
                }
                FastLED.show();
            };

            // ストリップ別テスト用関数
            void testStrip(int stripNum, ::CRGB color, int duration_ms = 1000){
                if (stripNum < 0 || stripNum >= NUM_STRIPS) return;
                
                // 指定されたストリップのみ点灯
                black();
                for(int i = 0; i < LEDS_PER_STRIP; i++){
                    setStripPixel(stripNum, i, color);
                }
                update();
                delay(duration_ms);
                black();
            };
    };
}
