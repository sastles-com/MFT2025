#include <FastLED.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <M5Unified.h>
// #include "AtomS3R_Display.h"  // M5Unifiedを使用するため無効化

// AtomS3R LED設定
#define LED_PIN 35        // AtomS3R内蔵LED
#define NUM_LEDS 1
#define BUTTON_PIN 41     // AtomS3Rボタン

CRGB leds[NUM_LEDS];
Preferences prefs;
// AtomS3R_Display lcd;  // M5.Lcdを使用するため無効化

void setup() {
  // M5Unified初期化（必須）
  M5.begin();
  
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting AtomS3R with FastLED...");
  
  // ボタン初期化
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // FastLED初期化
  Serial.println("Initializing FastLED...");
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);  // 輝度50%
  
  // LED動作テスト
  Serial.println("LED test starting...");
  leds[0] = CRGB::Red;
  FastLED.show();
  delay(500);
  
  leds[0] = CRGB::Green;
  FastLED.show();
  delay(500);
  
  leds[0] = CRGB::Blue;
  FastLED.show();
  delay(500);
  
  leds[0] = CRGB::Black;
  FastLED.show();
  
  Serial.println("FastLED initialized successfully!");
  
  // M5Unified LCD初期化
  Serial.println("=== Starting M5Unified LCD initialization ===");
  
  // LCD設定
  M5.Lcd.begin();
  M5.Lcd.setRotation(0);  // 0度回転
  M5.Lcd.setBrightness(180);  // 明度設定
  M5.Lcd.fillScreen(TFT_BLACK);
  
  Serial.println("Step 1: M5.Lcd initialized");
  
  // テスト表示
  M5.Lcd.fillScreen(TFT_GREEN);  // Green background
  delay(200);
  
  M5.Lcd.setTextColor(TFT_BLACK);  // Black text on green
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 30);
  M5.Lcd.println("AtomS3R");
  
  M5.Lcd.setTextColor(TFT_WHITE);  // White text
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(10, 60);
  M5.Lcd.println("LCD Working!");
  
  M5.Lcd.setCursor(10, 80);
  M5.Lcd.println("M5Unified OK!");
  
  // カラーテスト
  M5.Lcd.fillRect(10, 100, 20, 20, TFT_RED);     // Red
  M5.Lcd.fillRect(40, 100, 20, 20, TFT_GREEN);   // Green  
  M5.Lcd.fillRect(70, 100, 20, 20, TFT_BLUE);    // Blue
  
  Serial.println("Step 2: M5Unified LCD test display completed!");
  Serial.println("=== M5Unified LCD initialization complete ===");
  
  // デバイス情報を表示
  Serial.println("Device Info:");
  Serial.println("- Heap free: " + String(ESP.getFreeHeap()));
  Serial.println("- PSRAM size: " + String(ESP.getPsramSize()));
  Serial.println("- Flash size: " + String(ESP.getFlashChipSize()));
  Serial.println("- CPU frequency: " + String(ESP.getCpuFreqMHz()) + "MHz");
  Serial.println("- MAC address: " + WiFi.macAddress());
  
  Serial.println("Setup complete - AtomS3R ready!");
}

void loop() {
  // M5Unified更新（必須）
  M5.update();
  
  static unsigned long lastUpdate = 0;
  static int counter = 0;
  static int colorIndex = 0;
  
  // ボタン状態チェック
  bool buttonPressed = digitalRead(BUTTON_PIN) == LOW;
  
  if (millis() - lastUpdate > 2000) {  // 2秒間隔
    counter++;
    
    // シリアル出力
    Serial.println("Device running stable - " + String(millis()/1000) + "s uptime, count: " + String(counter));
    
    // LED色を変更（虹色サイクル）
    CRGB colors[] = {CRGB::Red, CRGB::Orange, CRGB::Yellow, CRGB::Green, CRGB::Blue, CRGB::Purple};
    leds[0] = colors[colorIndex % 6];
    colorIndex++;
    
    // ボタンが押されている場合は白色
    if (buttonPressed) {
      leds[0] = CRGB::White;
      Serial.println("Button pressed!");
    }
    
    FastLED.show();
    lastUpdate = millis();
  }
  
  // delay(50);
  delay(3);
}
