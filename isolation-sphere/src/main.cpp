#include <FastLED.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <M5Unified.h>
#include <esp_task_wdt.h>
#include <TJpg_Decoder.h>

#include "audio/BuzzerService.h"
#include "boot/BootOrchestrator.h"
#include "config/ConfigManager.h"
#include "core/CoreTasks.h"
#include "core/SharedState.h"
#include "display/DisplayController.h"
#include "hardware/HardwareContext.h"
#include "storage/StorageManager.h"
#include "storage/StorageStager.h"

#include <LittleFS.h>
#include <PSRamFS.h>

class M5DisplayDriver : public HardwareContext::DisplayDriver {
 public:
  bool begin() override { return M5.Display.begin(); }

  void setRotation(std::int8_t rotation) override { M5.Display.setRotation(rotation); }

  void setBrightness(std::uint8_t brightness) override { M5.Display.setBrightness(brightness); }

  void fillScreen(std::uint16_t color) override { M5.Display.fillScreen(color); }
};

class M5HardwareContext : public HardwareContext {
 public:
  HardwareContext::DisplayDriver &display() override { return displayDriver_; }

 private:
  M5DisplayDriver displayDriver_;
};

// PSRAM検出・テスト関数
void testPSRAM() {
  Serial.println("\n=== PSRAM Test Start ===");
  
  const char* name;
  switch (M5.getBoard()) {
      case m5::board_t::board_M5StackCoreS3:  name = "StackS3";     break;
      case m5::board_t::board_M5AtomS3Lite:   name = "ATOMS3Lite";  break;
      case m5::board_t::board_M5AtomS3:       name = "ATOMS3";      break;
      case m5::board_t::board_M5StampC3:      name = "StampC3";     break;
      case m5::board_t::board_M5StampS3:      name = "StampS3";     break;
      case m5::board_t::board_M5StampC3U:     name = "StampC3U";    break;
      case m5::board_t::board_M5Stack:        name = "Stack";       break;
      case m5::board_t::board_M5StackCore2:   name = "StackCore2";  break;
      case m5::board_t::board_M5StickC:       name = "StickC";      break;
      case m5::board_t::board_M5StickCPlus:   name = "StickCPlus";  break;
      case m5::board_t::board_M5StackCoreInk: name = "CoreInk";     break;
      case m5::board_t::board_M5Paper:        name = "Paper";       break;
      case m5::board_t::board_M5Tough:        name = "Tough";       break;
      case m5::board_t::board_M5Station:      name = "Station";     break;
      case m5::board_t::board_M5Atom:         name = "ATOM";        break;
      case m5::board_t::board_M5AtomPsram:    name = "ATOM PSRAM";  break;
      case m5::board_t::board_M5AtomU:        name = "ATOM U";      break;
      case m5::board_t::board_M5TimerCam:     name = "TimerCamera"; break;
      case m5::board_t::board_M5StampPico:    name = "StampPico";   break;
      case m5::board_t::board_M5AtomS3R:      name = "M5AtomS3R";   break;
      case m5::board_t::board_M5AtomS3U:      name = "M5AtomS3U";   break;
      default:                                name = "Who am I ?";  break;
  }
  Serial.println(name);
  
  // PSRAMの初期化
  if (!psramInit()) {
      Serial.println("✗ PSRAM初期化失敗");
  } else {
      Serial.println("✓ PSRAM初期化成功");
      Serial.printf("Total PSRAM : %u bytes (%.2f MB)\n", ESP.getPsramSize(), ESP.getPsramSize() / 1024.0 / 1024.0);
      Serial.printf("Free  PSRAM : %u bytes (%.2f MB)\n", ESP.getFreePsram(), ESP.getFreePsram() / 1024.0 / 1024.0);
  }

  void* psram_buffer = heap_caps_malloc(1024 * 1024, MALLOC_CAP_SPIRAM);
  if (psram_buffer == NULL) {
      Serial.println("✗ PSRAMからメモリ確保失敗");
  } else {
      Serial.println("✓ PSRAMからメモリ確保成功");
      Serial.printf("Total PSRAM : %u bytes (%.2f MB)\n", ESP.getPsramSize(), ESP.getPsramSize() / 1024.0 / 1024.0);
      Serial.printf("Free  PSRAM : %u bytes (%.2f MB)\n", ESP.getFreePsram(), ESP.getFreePsram() / 1024.0 / 1024.0);
      free(psram_buffer);
  }    
  
  Serial.println("=== PSRAM Test End ===\n");
}

#define LED_PIN 35        // AtomS3R内蔵LED
#define NUM_LEDS 1
#define BUTTON_PIN 41     // AtomS3Rボタン

CRGB leds[NUM_LEDS];
StorageManager storageManager;
SharedState sharedState;

namespace {

CoreTask::TaskConfig makeTaskConfig(const char *name, int coreId, std::uint32_t priority, std::uint32_t stackSize, std::uint32_t intervalMs) {
  CoreTask::TaskConfig cfg;
  cfg.name = name;
  cfg.coreId = coreId;
  cfg.priority = priority;
  cfg.stackSize = stackSize;
  cfg.loopIntervalMs = intervalMs;
  return cfg;
}

void scanI2CBus(TwoWire &bus, const char *label) {
  Serial.printf("[I2C] Scanning %s...\n", label);
  bool found = false;
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    bus.beginTransmission(address);
    uint8_t error = bus.endTransmission();
    if (error == 0) {
      Serial.printf("[I2C] %s device at 0x%02X\n", label, address);
      found = true;
    } else if (error == 4) {
      Serial.printf("[I2C] %s unknown error at 0x%02X\n", label, address);
    }
  }
  if (!found) {
    Serial.printf("[I2C] No devices detected on %s\n", label);
  }
}

void scanInternalI2C(const char *label) {
  Serial.printf("[I2C] Scanning %s (M5.In_I2C)...\n", label);
  bool results[120] = {false};
  M5.In_I2C.scanID(results);
  bool found = false;
  for (int addr = 0; addr < 120; ++addr) {
    if (!results[addr]) {
      continue;
    }
    Serial.printf("[I2C] %s device at 0x%02X\n", label, addr + 8);
    found = true;
  }
  if (!found) {
    Serial.printf("[I2C] No devices detected on %s\n", label);
  }
}

namespace {

float quaternionToRoll(const ImuService::Reading &r) {
  const float qw = r.qw;
  const float qx = r.qx;
  const float qy = r.qy;
  const float qz = r.qz;
  const float t0 = +2.0f * (qw * qx + qy * qz);
  const float t1 = +1.0f - 2.0f * (qx * qx + qy * qy);
  return atan2f(t0, t1);
}

float quaternionToPitch(const ImuService::Reading &r) {
  const float qw = r.qw;
  const float qx = r.qx;
  const float qy = r.qy;
  const float qz = r.qz;
  float t2 = +2.0f * (qw * qy - qz * qx);
  if (t2 > 1.0f) t2 = 1.0f;
  if (t2 < -1.0f) t2 = -1.0f;
  return asinf(t2);
}

float quaternionToYaw(const ImuService::Reading &r) {
  const float qw = r.qw;
  const float qx = r.qx;
  const float qy = r.qy;
  const float qz = r.qz;
  const float t3 = +2.0f * (qw * qz + qx * qy);
  const float t4 = +1.0f - 2.0f * (qy * qy + qz * qz);
  return atan2f(t3, t4);
}

void drawImuVisualization(const ImuService::Reading &reading, bool highlight) {
  static uint32_t lastDrawMs = 0;
  const uint32_t now = millis();
  if (now - lastDrawMs < 80) {
    return;
  }
  lastDrawMs = now;

  constexpr int areaX = 0;
  constexpr int areaY = 36;
  const int areaW = M5.Display.width();
  const int areaH = std::min(100, M5.Display.height() - areaY);
  const int centerX = areaX + areaW / 2;
  const int centerY = areaY + areaH / 2 + 10;
  const int radius = std::min(areaW, areaH) / 3;

  const float roll = quaternionToRoll(reading);
  const float pitch = quaternionToPitch(reading);
  const float yaw = quaternionToYaw(reading);

  M5.Display.fillRect(areaX, areaY, areaW, areaH, TFT_BLACK);

  M5.Display.drawCircle(centerX, centerY, radius, TFT_DARKGREY);
  M5.Display.drawCircle(centerX, centerY, radius / 2, TFT_DARKGREY);

  const float pitchOffset = pitch * radius * 0.6f;
  const float cosR = cosf(roll);
  const float sinR = sinf(roll);

  auto drawHorizon = [&](float localX) {
    const float x = localX;
    const float y = pitchOffset;
    const float rotatedX = x * cosR - y * sinR;
    const float rotatedY = x * sinR + y * cosR;
    return std::pair<int16_t, int16_t>{
        static_cast<int16_t>(centerX + rotatedX),
        static_cast<int16_t>(centerY + rotatedY)};
  };

  auto left = drawHorizon(-radius);
  auto right = drawHorizon(radius);
  M5.Display.drawLine(left.first, left.second, right.first, right.second, TFT_YELLOW);

  const float cosY = cosf(yaw);
  const float sinY = sinf(yaw);
  auto rotateYaw = [&](float x, float y) {
    const float rx = x * cosY - y * sinY;
    const float ry = x * sinY + y * cosY;
    return std::pair<int16_t, int16_t>{
        static_cast<int16_t>(centerX + rx),
        static_cast<int16_t>(centerY + ry)};
  };

  const float pointerLen = radius - 4;
  auto p0 = rotateYaw(0.0f, -pointerLen);
  auto p1 = rotateYaw(-12.0f, pointerLen);
  auto p2 = rotateYaw(12.0f, pointerLen);
  uint32_t pointerColor = highlight ? TFT_RED : TFT_CYAN;
  M5.Display.fillTriangle(p0.first, p0.second, p1.first, p1.second, p2.first, p2.second, pointerColor);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  const int textY = areaY + areaH - 30;
  M5.Display.setCursor(areaX + 2, textY);
  constexpr float radToDeg = 180.0f / PI;
  M5.Display.printf("R:%6.1f P:%6.1f Y:%6.1f\n", roll * radToDeg, pitch * radToDeg, yaw * radToDeg);
}

}  // namespace

}

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
    
    // LittleFSから画像ファイルを読み込み
    File jpegFile = LittleFS.open(filename, "r");
    if (jpegFile) {
      size_t fileSize = jpegFile.size();
      // Serial.println("[Opening] File size: " + String(fileSize) + " bytes");
      
      // ファイル全体をメモリに読み込み
      uint8_t* jpegData = (uint8_t*)malloc(fileSize);
      if (jpegData) {
        size_t bytesRead = jpegFile.read(jpegData, fileSize);
        jpegFile.close();
        
        if (bytesRead == fileSize) {
          // ファイルがJPEGかを判定
          if (fileSize >= 2 && jpegData[0] == 0xFF && jpegData[1] == 0xD8) {
            // JPEG形式の場合
            uint16_t w = 0, h = 0;
            TJpgDec.getJpgSize(&w, &h, jpegData, fileSize);
            // Serial.println("[Opening] JPEG Image size: " + String(w) + "x" + String(h));
            
            // 画面中央に配置
            int16_t x = (M5.Display.width() - w) / 2;
            int16_t y = (M5.Display.height() - h) / 2;
            
            // 背景をクリア
            M5.Display.fillScreen(TFT_BLACK);
            
            // JPEG画像を表示
            TJpgDec.drawJpg(x, y, jpegData, fileSize);
            
            // Serial.println("[Opening] JPEG Frame " + String(frame) + " displayed");
          } else {
            Serial.println("[Opening] Invalid JPEG format detected");
          }
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
Core0Task core0Task(makeTaskConfig("Core0Task", 0, 4, 4096, 50), configManager, storageManager, sharedState);
Core1Task core1Task(makeTaskConfig("Core1Task", 1, 4, 4096, 20), sharedState);

#ifndef UNIT_TEST

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting AtomS3R initialization...");
  
  // M5Unified初期化（動作実績のある設定を使用）
  auto cfg = M5.config();
  cfg.external_spk = false;  // 外部スピーカー無効
  cfg.output_power = true;   // 5V出力を有効化して周辺へ電源供給
  cfg.internal_imu = true;   // 内蔵IMUをM5Unifiedで利用
  cfg.internal_rtc = true;   // 内蔵RTC/I2Cバスを有効化
  cfg.fallback_board = m5::board_t::board_M5AtomS3R;  // ボード検出が失敗した場合のフォールバック
  M5.begin(cfg);
  
  Serial.println("M5.begin() completed");
  delay(500);

#if defined(IMU_SENSOR_BMI270)
  {
    constexpr int kMaxImuAttempts = 5;
    bool imuReady = M5.Imu.isEnabled();
    for (int attempt = 0; attempt < kMaxImuAttempts && !imuReady; ++attempt) {
      if (attempt > 0) {
        Serial.printf("[IMU] Retry %d after delay\n", attempt);
        delay(50 * attempt);
      }
      if (M5.Imu.begin(&M5.In_I2C, M5.getBoard())) {
        imuReady = true;
        break;
      }
    }
    if (!imuReady) {
      Serial.println("[IMU] Failed to initialize internal IMU via M5Unified");
      scanInternalI2C("Internal I2C");
    } else {
      Serial.println("[IMU] Internal IMU ready via M5Unified");
      scanInternalI2C("Internal I2C");
    }
  }
#endif

  
  // PSRAMテスト実行
  testPSRAM();
  
  // ボタン初期化
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("Mounting storage...");
  
  // PSRamFS容量を3MBに設定（8MB PSRAMの一部使用）
  Serial.println("[Storage] Configuring PSRamFS for 3MB capacity...");
  

  uint32_t t = millis();
  // LittleFSの破損を修復するため、一度強制フォーマットを実行
  Serial.println("[Storage] Attempting LittleFS format to fix corruption...");
  if (LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
    Serial.println("[Storage] LittleFS format and mount successful!");
    LittleFS.end();
  } else {
    Serial.println("[Storage] LittleFS format failed!");
  }
  Serial.printf("[Timing] LittleFS begin took %lu ms\n", millis() - t); 



  t = millis();
  // PSRamFSを3MB容量で初期化
  if (PSRamFS.setPartitionSize(3 * 1024 * 1024) && PSRamFS.begin()) {
    Serial.println("[Storage] PSRamFS initialized with 3MB capacity");
  } else {
    Serial.println("[Storage] PSRamFS initialization failed, falling back to heap");
  }
  Serial.printf("[Timing] PSRamFS begin took %lu ms\n", millis() - t); 




  M5HardwareContext hardwareContext;
  DisplayController displayController(hardwareContext.display());

  BootOrchestrator::Callbacks bootCallbacks;
  bootCallbacks.onStorageReady = [&]() {
    Serial.println(storageManager.isLittleFsMounted() ? "[Storage] LittleFS mounted"
                                                      : "[Storage] LittleFS not mounted");
    Serial.println(storageManager.isPsRamFsMounted() ? "[Storage] PSRamFS mounted"
                                                     : "[Storage] PSRamFS not mounted");
    if (storageManager.isLittleFsMounted()) {
      Serial.println("[Config] Core0Task will load config.json asynchronously");
    }
  };
  bootCallbacks.stageAssets = [&]() {
    bool success = true;

    if (!storageManager.isPsRamFsMounted()) {
      Serial.println("[Storage] PSRamFS unavailable - skipping asset staging");
      return success;
    }

    File root = PSRamFS.open("/");
    if (!root || !root.isDirectory()) {
      Serial.println("[Storage] Warning: PSRamFS root not accessible");
      success = false;
    } else {
      if (!PSRamFS.exists("/images")) {
        if (PSRamFS.mkdir("/images")) {
          Serial.println("[Storage] Created /images directory in PSRamFS");
        } else {
          Serial.println("[Storage] Failed to create /images directory in PSRamFS");
          success = false;
        }
      }
      root.close();
    }
    // esp_task_wdt_reset(); 
    if (storageManager.isLittleFsMounted()) {
      StorageStager stager(StorageStager::makeSourceFsOps(LittleFS),
                           StorageStager::makeDestinationFsOps(PSRamFS, LittleFS));
      if (stager.stageDirectory("/images")) {
        Serial.println("[Storage] Assets mirrored from LittleFS to PSRamFS");
      } else {
        Serial.println("[Storage] Asset mirroring failed - will use PSRamFS only");
        success = false;
      }
    } else {
      Serial.println("[Storage] LittleFS unavailable - using PSRamFS only mode");
    }
    // esp_task_wdt_reset(); 
    
    
    return success;
  };

  BootOrchestrator::Services bootServices;
  bootServices.displayInitialize = [&](const ConfigManager::DisplayConfig &displayCfg) {
    if (!displayCfg.displaySwitch) {
      Serial.println("[Display] Display disabled by config");
      return true;
    }
    return displayController.initialize(displayCfg);
  };
  bootServices.playStartupTone = [&](const ConfigManager::Config &cfg) {
    if (!cfg.buzzer.enabled) {
      Serial.println("[Buzzer] Startup tone disabled by config");
      return;
    }
    BuzzerService buzzer;
    if (!buzzer.begin()) {
      Serial.println("[Buzzer] Initialization failed");
      return;
    }
    Serial.println("[Buzzer] Startup tone playing");
    buzzer.playStartupTone();
    buzzer.stop();
  };

#if defined(IMU_SENSOR_BMI270)
  delay(100);  // IMU電源立ち上がり待機
  if (!M5.Imu.isEnabled() && !M5.Imu.begin(&M5.In_I2C, M5.getBoard())) {
    Serial.println("[IMU] Failed to initialize internal IMU via M5Unified");
    scanInternalI2C("Internal I2C");
  } else {
    Serial.println("[IMU] Internal IMU ready via M5Unified");
    scanInternalI2C("Internal I2C");
  }
#elif defined(IMU_SENSOR_BNO055)
  Wire1.begin(2, 1);
  Wire1.setClock(400000);
  scanI2CBus(Wire1, "Wire1 (external)");
#endif
  core1Task.markImuWireInitialized();


  BootOrchestrator bootOrchestrator(storageManager, configManager, sharedState, bootCallbacks, bootServices);
  if (!bootOrchestrator.run()) {
    Serial.println("[Boot] Boot orchestrator failed - storage or staging incomplete");
  } else if (!bootOrchestrator.hasLoadedConfig()) {
    Serial.println("[Boot] Config not loaded during boot");
  }


  if (!core0Task.isStarted() && !core0Task.start()) {
    Serial.println("[Core0] Failed to start task");
  }
  if (!core1Task.isStarted() && !core1Task.start()) {
    Serial.println("[Core1] Failed to start task");
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
  
  // // WDTフィード（初期化時間を確保）
  // esp_task_wdt_reset();
  // delay(100);
  
  // // M5.Display初期化（動作実績のあるアプローチ）
  // Serial.println("=== Starting M5.Display initialization ===");
  // esp_task_wdt_reset();
  
  // // PSRamFSテスト用：簡単な画像データを生成して保存
  // Serial.println("[Image Test] Starting image generation test...");
  // if (storageManager.isPsRamFsMounted()) {
  //   Serial.println("[Image Test] PSRamFS is mounted - proceeding with test");
    
  //   // 32x32ピクセルの簡単なテスト画像データを生成（RGB565形式）
  //   const int imageWidth = 32;
  //   const int imageHeight = 32;
  //   const int imageSize = imageWidth * imageHeight * 2; // RGB565は2バイト/ピクセル
    
  //   Serial.println("[Image Test] Allocating memory for " + String(imageSize) + " bytes...");
  //   uint16_t* imageData = (uint16_t*)malloc(imageSize);
  //   if (imageData) {
  //     Serial.println("[Image Test] Memory allocated successfully");
      
  //     // カラフルなテストパターンを生成
  //     for (int y = 0; y < imageHeight; y++) {
  //       for (int x = 0; x < imageWidth; x++) {
  //         uint16_t color;
  //         if (y < 8) {
  //           color = M5.Display.color565(255, 0, 0); // Red
  //         } else if (y < 16) {
  //           color = M5.Display.color565(0, 255, 0); // Green
  //         } else if (y < 24) {
  //           color = M5.Display.color565(0, 0, 255); // Blue
  //         } else {
  //           color = M5.Display.color565(255, 255, 0); // Yellow
  //         }
  //         imageData[y * imageWidth + x] = color;
  //       }
  //     }
  //     Serial.println("[Image Test] Test pattern generated");
      
  //     // PSRamFSに保存
  //     File testImageFile = PSRamFS.open("/images/test_pattern.rgb565", "w");
  //     if (testImageFile) {
  //       size_t written = testImageFile.write((uint8_t*)imageData, imageSize);
  //       testImageFile.close();
  //       Serial.println("[Image Test] Test pattern saved: " + String(written) + " bytes");
        
  //       // すぐに表示テスト
  //       M5.Display.pushImage(80, 50, imageWidth, imageHeight, imageData);
  //       Serial.println("[Image Test] Test pattern displayed on screen at (80,50)");
  //     } else {
  //       Serial.println("[Image Test] Failed to create test image file");
  //     }
      
  //     free(imageData);
  //     Serial.println("[Image Test] Memory freed");
  //   } else {
  //     Serial.println("[Image Test] Failed to allocate memory for test image");
  //   }
  // } else {
  //   Serial.println("[Image Test] PSRamFS not mounted - skipping test");
  // }
  
  // Opening連番JPEG表示テスト
  if (storageManager.isPsRamFsMounted()) {
    Serial.println("[Opening] Checking for opening animation files...");
    
    // 最初のフレームが存在するかチェック
    if (PSRamFS.exists("/images/opening/001.jpg")) {
      Serial.println("[Opening] Opening animation files found");
      delay(10); // 少し待ってからアニメーション開始
      playOpeningAnimation();
    } else {
      Serial.println("[Opening] Opening animation files not found in PSRamFS");
      Serial.println("[Opening] Creating temporary test JPEG files for demonstration...");
      
      // テスト用：PSRamFSにopeningディレクトリを作成
      if (!PSRamFS.exists("/images/opening")) {
        if (PSRamFS.mkdir("/images/opening")) {
          Serial.println("[Opening] Created /images/opening directory");
        }
      }
      
      // // 実際のミニJPEGデータを作成（最小限のJPEGヘッダー + データ）
      // Serial.println("[Opening] Generating actual JPEG test data...");
      
      // // 最小限の有効なJPEGデータを作成（8x8グレースケール）
      // uint8_t miniJpegTemplate[] = {
      //   0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01,
      //   0x01, 0x01, 0x00, 0x48, 0x00, 0x48, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43,
      //   0x00, 0x08, 0x06, 0x06, 0x07, 0x06, 0x05, 0x08, 0x07, 0x07, 0x07, 0x09,
      //   0x09, 0x08, 0x0A, 0x0C, 0x14, 0x0D, 0x0C, 0x0B, 0x0B, 0x0C, 0x19, 0x12,
      //   0x13, 0x0F, 0x14, 0x1D, 0x1A, 0x1F, 0x1E, 0x1D, 0x1A, 0x1C, 0x1C, 0x20,
      //   0x24, 0x2E, 0x27, 0x20, 0x22, 0x2C, 0x23, 0x1C, 0x1C, 0x28, 0x37, 0x29,
      //   0x2C, 0x30, 0x31, 0x34, 0x34, 0x34, 0x1F, 0x27, 0x39, 0x3D, 0x38, 0x32,
      //   0x3C, 0x2E, 0x33, 0x34, 0x32, 0xFF, 0xC0, 0x00, 0x11, 0x08, 0x00, 0x20,
      //   0x00, 0x20, 0x01, 0x01, 0x11, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01,
      //   0xFF, 0xC4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      //   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0xFF, 0xC4,
      //   0x00, 0x14, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      //   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xDA, 0x00, 0x0C,
      //   0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3F, 0x00, 0xAA, 0xFF, 0xD9
      // };
      
      // for (int i = 1; i <= 3; i++) {
      //   char filename[64];
      //   snprintf(filename, sizeof(filename), "/images/opening/%03d.jpg", i);
        
      //   File jpegFile = PSRamFS.open(filename, "w");
      //   if (jpegFile) {
      //     // 基本のJPEGデータをコピー
      //     uint8_t* jpegData = (uint8_t*)malloc(sizeof(miniJpegTemplate));
      //     memcpy(jpegData, miniJpegTemplate, sizeof(miniJpegTemplate));
          
      //     // データを少し変更してフレームごとに違いを作る
      //     jpegData[143] = 0xAA + (i * 0x11); // 最後のデータバイトを変更
          
      //     jpegFile.write(jpegData, sizeof(miniJpegTemplate));
      //     jpegFile.close();
      //     free(jpegData);
          
      //     Serial.println("[Opening] Created mini JPEG file: " + String(filename) + " (" + String(sizeof(miniJpegTemplate)) + " bytes)");
      //   }
      // }
      
      // Serial.println("[Opening] Test files created, starting demonstration...");
      // delay(1000);
      playOpeningAnimation();
    }
  }

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

  if (M5.BtnPWR.wasClicked()) {
    Serial.println("[IMU] Calibration requested from power button");
    core1Task.requestImuCalibration();
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

  static uint32_t lastImuOverlayMs = 0;
  if (millis() - lastImuOverlayMs >= 200) {
    ImuService::Reading imuReading;
    bool uiActive = false;
    bool uiStateKnown = sharedState.getUiMode(uiActive);
    if (sharedState.getImuReading(imuReading)) {
      lastImuOverlayMs = millis();
      const int overlayWidth = M5.Display.width();
      const int overlayHeight = 34;  // 3行分
      M5.Display.fillRect(0, 0, overlayWidth, overlayHeight, TFT_BLACK);
      M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
      M5.Display.setTextSize(1);
      M5.Display.setCursor(0, 0);
      M5.Display.printf("qw:%6.3f qx:%6.3f\n", imuReading.qw, imuReading.qx);
      M5.Display.printf("qy:%6.3f qz:%6.3f\n", imuReading.qy, imuReading.qz);
      M5.Display.printf("ts:%lu\n", static_cast<unsigned long>(imuReading.timestampMs));
      if (uiStateKnown) {
        M5.Display.printf("UI:%s\n", uiActive ? "ON" : "OFF");
      }
      drawImuVisualization(imuReading, uiActive);
    }
  }
  
  // delay(3);
  delay(1);
}

#endif  // UNIT_TEST
