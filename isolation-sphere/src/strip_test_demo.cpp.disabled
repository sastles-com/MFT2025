/**
 * @file strip_test_demo.cpp
 * @brief 4ストリップLEDテスト用デモ - 各ストリップの動作確認
 * @details isolation-sphere用マルチストリップLED動作確認デモ
 * 
 * ハードウェア要件:
 * - M5Stack AtomS3R
 * - GPIO 8-11に接続された4本のWS2812B LEDストリップ
 * - 各ストリップ200個LED（合計800個）
 */

#include <M5Unified.h>
#include <FastLED.h>

// LEDハードウェア設定（M5Stack AtomS3R用 - 4ストリップ構成）
#define LED_DATA_PIN_1 8   // GPIO 8: ストリップ1
#define LED_DATA_PIN_2 9   // GPIO 9: ストリップ2  
#define LED_DATA_PIN_3 10  // GPIO 10: ストリップ3
#define LED_DATA_PIN_4 11  // GPIO 11: ストリップ4
#define LEDS_PER_STRIP 200 // ストリップあたりLED数
#define NUM_STRIPS 4       // ストリップ数
#define TOTAL_LEDS (LEDS_PER_STRIP * NUM_STRIPS) // 合計800個
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// FastLED配列
CRGB leds[TOTAL_LEDS];

// 関数宣言
void testEachStrip();
void rainbowTest();

void setup() {
    // M5Stack初期化
    M5.begin();
    Serial.begin(115200);
    Serial.println("\n================================");
    Serial.println("4ストリップLEDテストデモ開始");
    Serial.println("isolation-sphere LED動作確認");
    Serial.println("================================");

    // FastLED初期化（4ストリップ構成）
    Serial.println("\n[1] FastLED初期化（4ストリップ構成）");
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN_1, COLOR_ORDER>(leds, 0 * LEDS_PER_STRIP, LEDS_PER_STRIP);
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN_2, COLOR_ORDER>(leds, 1 * LEDS_PER_STRIP, LEDS_PER_STRIP);
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN_3, COLOR_ORDER>(leds, 2 * LEDS_PER_STRIP, LEDS_PER_STRIP);
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN_4, COLOR_ORDER>(leds, 3 * LEDS_PER_STRIP, LEDS_PER_STRIP);
    
    FastLED.setBrightness(32);  // 輝度設定（12.5%）
    FastLED.clear();
    FastLED.show();
    
    Serial.printf("LED初期化完了: %d ストリップ x %d LED = %d LED総数\n", NUM_STRIPS, LEDS_PER_STRIP, TOTAL_LEDS);

    // ストリップ個別テスト
    Serial.println("\n[2] ストリップ個別テスト開始");
    testEachStrip();
    
    Serial.println("\n[3] 全ストリップ虹色テスト");
    rainbowTest();
    
    Serial.println("\n[4] リアルタイム動作開始");
    delay(1000);
}

void testEachStrip() {
    Serial.println("各ストリップを個別に点灯テスト...");
    
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        Serial.printf("ストリップ %d (GPIO %d) テスト中...\n", 
                     strip + 1, 8 + strip);
        
        // 全LED消灯
        FastLED.clear();
        
        // 対象ストリップのみ点灯
        int start_led = strip * LEDS_PER_STRIP;
        int end_led = start_led + LEDS_PER_STRIP;
        
        // ストリップごとに異なる色で点灯
        CRGB colors[4] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow};
        
        for (int i = start_led; i < end_led; i++) {
            leds[i] = colors[strip];
        }
        
        FastLED.show();
        delay(2000);  // 2秒間点灯
        
        Serial.printf("ストリップ %d テスト完了\n", strip + 1);
    }
    
    // 全消灯
    FastLED.clear();
    FastLED.show();
    delay(500);
}

void rainbowTest() {
    Serial.println("全ストリップ虹色パターン表示...");
    
    for (int phase = 0; phase < 256; phase += 8) {
        for (int i = 0; i < TOTAL_LEDS; i++) {
            uint8_t hue = (uint8_t)(phase + (i * 256 / TOTAL_LEDS));
            leds[i].setHSV(hue, 255, 128);
        }
        FastLED.show();
        delay(50);
    }
    
    // フェードアウト
    for (int brightness = 128; brightness >= 0; brightness -= 4) {
        FastLED.setBrightness(brightness / 4);
        FastLED.show();
        delay(50);
    }
    
    FastLED.setBrightness(32);  // 元の輝度に復元
    FastLED.clear();
    FastLED.show();
}

void loop() {
    M5.update();
    
    static uint32_t last_update = 0;
    static float rotation = 0.0f;
    
    uint32_t now = millis();
    if (now - last_update >= 50) {  // 20 FPS
        last_update = now;
        rotation += 0.05f;
        if (rotation >= 2.0f * M_PI) rotation = 0.0f;
        
        // 回転する波パターン（全ストリップ）
        for (int i = 0; i < TOTAL_LEDS; i++) {
            float t = (float)i / TOTAL_LEDS + rotation / (2.0f * M_PI);
            if (t > 1.0f) t -= 1.0f;
            
            uint8_t hue = (uint8_t)(t * 255);
            uint8_t val = (uint8_t)(64 + 64 * sin(t * 4 * M_PI + rotation));
            
            leds[i].setHSV(hue, 255, val);
        }
        
        FastLED.show();
    }
    
    // ボタン押下で詳細情報表示
    if (M5.BtnA.wasPressed()) {
        Serial.printf("動作中: %d LED, 回転角: %.2f rad\n", TOTAL_LEDS, rotation);
        Serial.printf("ストリップ構成: GPIO 8-11, %d LED/ストリップ\n", LEDS_PER_STRIP);
    }
    
    delay(10);
}