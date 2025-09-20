#include <FastLED.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <M5Unified.h>
#include <esp_task_wdt.h>

#define BUTTON_PIN 41     // AtomS3Rボタン


void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting AtomS3R initialization...");
  
  // M5Unified初期化（動作実績のある設定を使用）
  auto cfg = M5.config();
  cfg.external_spk = false;  // 外部スピーカー無効
  cfg.output_power = false;  // 電源出力無効
  cfg.internal_imu = false;  // IMU無効（SPIバス競合回避）
  cfg.internal_rtc = false;  // RTC無効（I2C競合回避）
  M5.begin(cfg);
  
  Serial.println("M5.begin() completed");
  delay(500);
  
  // ボタン初期化
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // FastLED初期化（一時的に無効化してSPI競合を回避）
#if defined(USE_FASTLED)
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
#else
  Serial.println("FastLED disabled (USE_FASTLED not defined)");
#endif
  
  // WDTフィード（初期化時間を確保）
  esp_task_wdt_reset();
  delay(100);
  
  // M5.Display初期化（動作実績のあるアプローチ）
  Serial.println("=== Starting M5.Display initialization ===");
  esp_task_wdt_reset();
  
  // Display設定
  bool display_ok = M5.Display.begin();
  esp_task_wdt_reset();
  
  if (display_ok) {
    Serial.println("Step 1: M5.Display.begin() SUCCESS");
  } else {
    Serial.println("Step 1: M5.Display.begin() FAILED");
    // 失敗してもCPUリセットを回避して続行
  }
  
  // AtomS3R は GC9107 + offset_y=32 が自動適用される想定
  M5.Display.setRotation(0);  // 0度回転
  M5.Display.setBrightness(200);  // 明度設定（0-255）
  M5.Display.fillScreen(TFT_BLACK);
  esp_task_wdt_reset();
  
  Serial.println("Step 2: M5.Display basic setup completed");
  
  // テスト表示
  M5.Display.fillScreen(TFT_GREEN);  // Green background
  delay(200);
  
  M5.Display.setTextColor(TFT_BLACK);  // Black text on green
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(MC_DATUM);  // 中央揃え
  M5.Display.drawString("AtomS3R", 64, 30);
  
  M5.Display.setTextColor(TFT_WHITE);  // White text
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.drawString("Display OK!", 64, 60);
  M5.Display.drawString("M5Unified", 64, 80);
  
  // カラーテスト
  M5.Display.fillRect(10, 100, 20, 20, TFT_RED);     // Red
  M5.Display.fillRect(40, 100, 20, 20, TFT_GREEN);   // Green  
  M5.Display.fillRect(70, 100, 20, 20, TFT_BLUE);    // Blue
  
  Serial.println("Step 2: M5.Display test display completed!");
  Serial.println("=== M5.Display initialization complete ===");
  
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

#if defined(USE_FASTLED)
    FastLED.show();
#endif
    lastUpdate = millis();
  }
  
  // delay(50);
  delay(3);
}
