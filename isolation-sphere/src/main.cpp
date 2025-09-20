#include <FastLED.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <M5Unified.h>
#include <esp_task_wdt.h>
#include <TJpg_Decoder.h>

#include "config/ConfigManager.h"
#include "storage/StorageManager.h"
#include "storage/StorageStager.h"

#include <LittleFS.h>
#include <PSRamFS.h>

#define LED_PIN 35        // AtomS3R内蔵LED
#define NUM_LEDS 1
#define BUTTON_PIN 41     // AtomS3Rボタン

CRGB leds[NUM_LEDS];
StorageManager storageManager;

// TJpg_Decoder用のコールバック関数
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= M5.Display.height()) return 0;
  M5.Display.pushImage(x, y, w, h, bitmap);
  return 1;
}

// Opening連番JPEG表示関数
void playOpeningAnimation() {
  Serial.println("[Opening] Starting opening animation...");
  
  // TJpg_Decoderの初期化
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);
  
  const int totalFrames = 50; // opening/001.jpg から 050.jpg まで
  const int frameDelay = 100;  // 10fps = 100ms間隔
  
  for (int frame = 1; frame <= totalFrames; frame++) {
    unsigned long frameStart = millis();
    
    // ファイル名を生成 (例: /images/opening/001.jpg)
    char filename[64];
    snprintf(filename, sizeof(filename), "/images/opening/%03d.jpg", frame);
    
    Serial.println("[Opening] Loading frame " + String(frame) + ": " + String(filename));
    
    // PSRamFSから画像ファイルを読み込み
    File jpegFile = PSRamFS.open(filename, "r");
    if (jpegFile) {
      size_t fileSize = jpegFile.size();
      Serial.println("[Opening] File size: " + String(fileSize) + " bytes");
      
      // ファイル全体をメモリに読み込み
      uint8_t* jpegData = (uint8_t*)malloc(fileSize);
      if (jpegData) {
        size_t bytesRead = jpegFile.read(jpegData, fileSize);
        jpegFile.close();
        
        if (bytesRead == fileSize) {
          // JPEG画像をデコードして表示
          uint16_t w = 0, h = 0;
          TJpgDec.getJpgSize(&w, &h, jpegData, fileSize);
          Serial.println("[Opening] Image size: " + String(w) + "x" + String(h));
          
          // 画面中央に配置
          int16_t x = (M5.Display.width() - w) / 2;
          int16_t y = (M5.Display.height() - h) / 2;
          
          // 背景をクリア
          M5.Display.fillScreen(TFT_BLACK);
          
          // JPEG画像を表示
          TJpgDec.drawJpg(x, y, jpegData, fileSize);
          
          Serial.println("[Opening] Frame " + String(frame) + " displayed");
        } else {
          Serial.println("[Opening] Failed to read file completely");
        }
        
        free(jpegData);
      } else {
        Serial.println("[Opening] Failed to allocate memory for JPEG data");
        jpegFile.close();
      }
    } else {
      Serial.println("[Opening] Failed to open file: " + String(filename));
    }
    
    // フレームレート調整（10fps）
    unsigned long frameTime = millis() - frameStart;
    if (frameTime < frameDelay) {
      delay(frameDelay - frameTime);
    }
    
    // WDTリセット
    esp_task_wdt_reset();
    
    // ボタンが押されたら中断
    M5.update();
    if (M5.BtnA.wasPressed()) {
      Serial.println("[Opening] Animation interrupted by button press");
      break;
    }
  }
  
  Serial.println("[Opening] Opening animation completed");
  M5.Display.fillScreen(TFT_BLACK);
}
ConfigManager configManager;

#ifndef UNIT_TEST

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

  Serial.println("Mounting storage...");
  
  // LittleFSの破損を修復するため、一度強制フォーマットを実行
  Serial.println("[Storage] Attempting LittleFS format to fix corruption...");
  if (LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
    Serial.println("[Storage] LittleFS format and mount successful!");
    LittleFS.end();
  } else {
    Serial.println("[Storage] LittleFS format failed!");
  }
  
  const bool storageReady = storageManager.begin();
  Serial.println(storageManager.isLittleFsMounted() ? "[Storage] LittleFS mounted"
                                                    : "[Storage] LittleFS not mounted");
  Serial.println(storageManager.isPsRamFsMounted() ? "[Storage] PSRamFS mounted"
                                                   : "[Storage] PSRamFS not mounted");
  if (!storageReady) {
    Serial.println("[Storage] Initialization incomplete - subsequent features may be limited");
  }

  if (storageManager.isLittleFsMounted()) {
    if (configManager.load("/littlefs/config.json")) {
      Serial.println("[Config] Loaded config.json");
      const auto &cfg = configManager.config();
      Serial.printf("[Config] system.name=%s\n", cfg.system.name.c_str());
      Serial.printf("[Config] system.psram=%s debug=%s\n",
                    cfg.system.psramEnabled ? "true" : "false",
                    cfg.system.debug ? "true" : "false");
      Serial.printf("[Config] display=%ux%u rot=%d offset=(%d,%d) depth=%u switch=%s\n",
                    cfg.display.width,
                    cfg.display.height,
                    cfg.display.rotation,
                    cfg.display.offsetX,
                    cfg.display.offsetY,
                    cfg.display.colorDepth,
                    cfg.display.displaySwitch ? "on" : "off");
      Serial.printf("[Config] wifi.ssid=%s retries=%u\n",
                    cfg.wifi.ssid.c_str(),
                    cfg.wifi.maxRetries);
      Serial.printf("[Config] mqtt.enabled=%s broker=%s:%u\n",
                    cfg.mqtt.enabled ? "true" : "false",
                    cfg.mqtt.broker.c_str(),
                    cfg.mqtt.port);
      Serial.printf("[Config] mqtt.topics ui=%s status=%s image=%s\n",
                    cfg.mqtt.topicUi.c_str(),
                    cfg.mqtt.topicStatus.c_str(),
                    cfg.mqtt.topicImage.c_str());
    } else {
      Serial.println("[Config] Failed to load config.json");
    }
  }

  // LittleFSが利用できない場合でも、PSRamFSだけで動作するよう改善
  if (storageManager.isPsRamFsMounted()) {
    Serial.println("[Storage] PSRamFS available - ready for runtime asset loading");
    
    // PSRamFSに基本ディレクトリ構造を作成
    File root = PSRamFS.open("/");
    if (!root || !root.isDirectory()) {
      Serial.println("[Storage] Warning: PSRamFS root not accessible");
    } else {
      // 画像用ディレクトリを作成
      if (!PSRamFS.exists("/images")) {
        if (PSRamFS.mkdir("/images")) {
          Serial.println("[Storage] Created /images directory in PSRamFS");
        }
      }
      root.close();
    }
    
    // LittleFSが利用可能な場合のみアセットミラーリングを実行
    if (storageManager.isLittleFsMounted()) {
      StorageStager stager(StorageStager::makeSourceFsOps(LittleFS),
                           StorageStager::makeDestinationFsOps(PSRamFS, LittleFS));
      if (stager.stageDirectory("/images")) {
        Serial.println("[Storage] Assets mirrored from LittleFS to PSRamFS");
      } else {
        Serial.println("[Storage] Asset mirroring failed - will use PSRamFS only");
      }
    } else {
      Serial.println("[Storage] LittleFS unavailable - using PSRamFS only mode");
    }
  }
  
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
  
  // PSRamFSテスト用：簡単な画像データを生成して保存
  Serial.println("[Image Test] Starting image generation test...");
  if (storageManager.isPsRamFsMounted()) {
    Serial.println("[Image Test] PSRamFS is mounted - proceeding with test");
    
    // 32x32ピクセルの簡単なテスト画像データを生成（RGB565形式）
    const int imageWidth = 32;
    const int imageHeight = 32;
    const int imageSize = imageWidth * imageHeight * 2; // RGB565は2バイト/ピクセル
    
    Serial.println("[Image Test] Allocating memory for " + String(imageSize) + " bytes...");
    uint16_t* imageData = (uint16_t*)malloc(imageSize);
    if (imageData) {
      Serial.println("[Image Test] Memory allocated successfully");
      
      // カラフルなテストパターンを生成
      for (int y = 0; y < imageHeight; y++) {
        for (int x = 0; x < imageWidth; x++) {
          uint16_t color;
          if (y < 8) {
            color = M5.Display.color565(255, 0, 0); // Red
          } else if (y < 16) {
            color = M5.Display.color565(0, 255, 0); // Green
          } else if (y < 24) {
            color = M5.Display.color565(0, 0, 255); // Blue
          } else {
            color = M5.Display.color565(255, 255, 0); // Yellow
          }
          imageData[y * imageWidth + x] = color;
        }
      }
      Serial.println("[Image Test] Test pattern generated");
      
      // PSRamFSに保存
      File testImageFile = PSRamFS.open("/images/test_pattern.rgb565", "w");
      if (testImageFile) {
        size_t written = testImageFile.write((uint8_t*)imageData, imageSize);
        testImageFile.close();
        Serial.println("[Image Test] Test pattern saved: " + String(written) + " bytes");
        
        // すぐに表示テスト
        M5.Display.pushImage(80, 50, imageWidth, imageHeight, imageData);
        Serial.println("[Image Test] Test pattern displayed on screen at (80,50)");
      } else {
        Serial.println("[Image Test] Failed to create test image file");
      }
      
      free(imageData);
      Serial.println("[Image Test] Memory freed");
    } else {
      Serial.println("[Image Test] Failed to allocate memory for test image");
    }
  } else {
    Serial.println("[Image Test] PSRamFS not mounted - skipping test");
  }
  
  // Opening連番JPEG表示テスト
  if (storageManager.isPsRamFsMounted()) {
    Serial.println("[Opening] Checking for opening animation files...");
    
    // 最初のフレームが存在するかチェック
    if (PSRamFS.exists("/images/opening/001.jpg")) {
      Serial.println("[Opening] Opening animation files found");
      delay(1000); // 少し待ってからアニメーション開始
      playOpeningAnimation();
    } else {
      Serial.println("[Opening] Opening animation files not found in PSRamFS");
      Serial.println("[Opening] Make sure to upload opening images to data/images/opening/");
    }
  }
  
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
  
  // M5ボタンが押されたらOpeningアニメーションを再生
  if (M5.BtnA.wasPressed()) {
    Serial.println("M5 Button pressed - playing opening animation");
    if (storageManager.isPsRamFsMounted() && PSRamFS.exists("/images/opening/001.jpg")) {
      playOpeningAnimation();
    } else {
      Serial.println("Opening animation files not available");
    }
  }
  
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

#endif  // UNIT_TEST
