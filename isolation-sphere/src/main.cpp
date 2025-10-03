// --- ここから元の統合ファームウェア main.cpp ---
#include <FastLED.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <M5Unified.h>
#include <TJpg_Decoder.h>
#include <esp_task_wdt.h>
#include <memory>

#include "audio/BuzzerService.h"
#include "boot/ProceduralOpeningPlayer.h"
#include "boot/SynchronizedBootExecutor.h"
#include "boot/BootOrchestrator.h"
#include "config/ConfigManager.h"
// #include "core/CoreTasks.h" // TODO: Implement proper CoreTasks
#include "core/SharedState.h"
#include "display/DisplayController.h"
#include "hardware/HardwareContext.h"
#include "imu/ImuService.h"
#include "imu/ShakeDetector.h"
#include "imu/ShakeToUiBridge.h"
#include "storage/StorageManager.h"
#include "storage/StorageStager.h"

// LED基盤システム & パフォーマンステスト
#include "led/LEDSphereManager.h"
#include "test/ProceduralPatternPerformanceTest.h"

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

// ...existing code...
// --- 必要なグローバル定義（ビルドエラー対策） ---
#define BUTTON_PIN 41
#define NUM_LEDS 1
#define LED_PIN 35
CRGB leds[NUM_LEDS];
StorageManager storageManager;
SharedState sharedState;
ImuService imuService;
// Runtime: UI shake detector + bridge (created in setup())
ShakeDetector* uiShakeDetector = nullptr;
ShakeToUiBridge* shakeToUiBridge = nullptr;

ConfigManager configManager;

// LED基盤システム & パフォーマンステスト
LEDSphere::LEDSphereManager sphereManager;
PerformanceTest::ProceduralPatternPerformanceTester perfTester;
bool performanceTestMode = false;

// ダミーPSRAMテスト関数（必要に応じて本来の実装に差し替え）
void testPSRAM() {
  Serial.println("[PSRAM] testPSRAM() called (dummy)");
}

// NOTE: UNIT_TEST dummy setup/loop is provided earlier in this file to
// avoid multiple definitions. Keep only the earlier occurrence.

namespace {

// CoreTask system disabled for minimal build
/*
CoreTask::TaskConfig makeTaskConfig(const char *name, int coreId, std::uint32_t priority, std::uint32_t stackSize, std::uint32_t intervalMs) {
  CoreTask::TaskConfig cfg;
  cfg.name = name;
  cfg.coreId = coreId;
  cfg.priority = priority;
  cfg.stackSize = stackSize;
  cfg.loopIntervalMs = intervalMs;
  return cfg;
}
*/

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
  // pointer color: highlight (e.g., error) -> red, UI mode -> green, otherwise cyan/blue
  uint32_t pointerColor;
  if (highlight) {
    pointerColor = TFT_RED;
  } else {
    // read UI mode from sharedState; default false
    bool uiMode = false;
    if (sharedState.getUiMode(uiMode) && uiMode) {
      pointerColor = TFT_GREEN;
    } else {
      pointerColor = TFT_CYAN; // UI off -> blueish
    }
  }
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

// 関数宣言
void playOpeningAnimation();
void playOpeningAnimationFromLittleFS();
void playOpeningAnimationFromFS(fs::FS &fileSystem, const char* fsName);
void playTestAnimation();

namespace {

std::unique_ptr<SynchronizedBootSequence> gProceduralSequence;
std::unique_ptr<Boot::SynchronizedBootExecutor> gProceduralExecutor;
std::unique_ptr<Boot::ProceduralOpeningPlayer> gProceduralPlayer;

void ensureProceduralOpeningReady() {
  if (!gProceduralPlayer) {
    gProceduralSequence.reset(new SynchronizedBootSequence(sphereManager));
    gProceduralExecutor.reset(new Boot::SynchronizedBootExecutor(*gProceduralSequence));
    gProceduralPlayer.reset(new Boot::ProceduralOpeningPlayer(*gProceduralExecutor));
  }
}

bool runProceduralOpening(const Boot::ProceduralBootExecutor::HeavyTaskFunction &heavyTask) {
  ensureProceduralOpeningReady();
  if (!gProceduralPlayer) {
    Serial.println("[Opening] ❌ Procedural opening player unavailable");
    return false;
  }

  bool success = gProceduralPlayer->playStandardOpening(heavyTask);
  auto result = gProceduralPlayer->lastExecution();
  Serial.printf("[Opening] 🎬 result: task=%s opening=%s time=%ums fps=%.1f\n",
                result.taskSuccess ? "OK" : "NG",
                result.openingSuccess ? "OK" : "NG",
                result.totalTimeMs,
                result.openingFps);
#if defined(USE_FASTLED)
  sphereManager.clearAllLEDs();
  sphereManager.drawAxisMarkers(10.0f, 5);
  sphereManager.show();
#endif
  return success;
}

}  // namespace

namespace {
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= M5.Display.height()) return 0;
  M5.Display.pushImage(x, y, w, h, bitmap);
  return 1;
}

bool playOpeningFramesFromFs(fs::FS &fs) {
  (void)fs;
  Serial.println("[Opening] JPEG opening disabled (procedural only)");
  return false;
}
}  // namespace

void playOpeningAnimation() {
  if (!playOpeningFramesFromFs(PSRamFS)) {
    Serial.println("[Opening] 🎬 Falling back to procedural opening");
    runProceduralOpening({});
  }
}

void playOpeningAnimationFromLittleFS() {
  if (!playOpeningFramesFromFs(LittleFS)) {
    Serial.println("[Opening] 🎬 Falling back to procedural opening (LittleFS trigger)");
    runProceduralOpening({});
  }
}

void playOpeningAnimationFromFS(fs::FS &fileSystem, const char* fsName) {
  Serial.printf("[Opening] 🎬 Starting opening sequence (%s trigger)...\n", fsName);
  if (!playOpeningFramesFromFs(fileSystem)) {
    runProceduralOpening({});
  }
}

// プロシージャル（関数的）オープニングアニメーション
void playProceduralOpening() {
  Serial.println("[Opening] Procedural opening will be handled by Core1Task");
  // この関数は現在使用されていません - Core1TaskのrenderProceduralOpening()を使用
}

// Phase 1: 球体ロゴアニメーション
void drawSphereLogoAnimation(float progress) {
  int centerX = M5.Display.width() / 2;
  int centerY = M5.Display.height() / 2;
  
  // 中央の球体
  float sphereRadius = 30.0f * progress;
  if (sphereRadius > 30.0f) sphereRadius = 30.0f;
  
  // グラデーション風の同心円
  for (int r = sphereRadius; r > 0; r -= 3) {
    uint16_t color = M5.Display.color565(
      0, 
      (int)(100 + 155 * (1.0f - (float)r/sphereRadius)), 
      (int)(200 * (float)r/sphereRadius)
    );
    M5.Display.drawCircle(centerX, centerY, r, color);
  }
  
  // 回転する軌道線
  for (int orbit = 0; orbit < 3; orbit++) {
    float orbitRadius = 40 + orbit * 15;
    float angle = progress * 360.0f + orbit * 120.0f;
    float rad = angle * PI / 180.0f;
    
    int x = centerX + cos(rad) * orbitRadius;
    int y = centerY + sin(rad) * orbitRadius;
    
    uint16_t orbitColor = (orbit == 0) ? TFT_CYAN : 
                         (orbit == 1) ? TFT_MAGENTA : TFT_YELLOW;
    M5.Display.fillCircle(x, y, 3, orbitColor);
  }
  
  // タイトルテキスト
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(centerX - 60, centerY + 60);
  M5.Display.print("ISOLATION");
  M5.Display.setCursor(centerX - 40, centerY + 80);
  M5.Display.print("SPHERE");
}

// Phase 2: システム初期化アニメーション
void drawSystemInitAnimation(float progress) {
  int y = 20;
  const int lineHeight = 12;
  
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.setTextSize(1);
  
  // 初期化メッセージリスト
  const char* initMessages[] = {
    "✓ Hardware initialized",
    "✓ IMU calibrated", 
    "✓ LED strips detected",
    "✓ WiFi connecting...",
    "✓ MQTT broker ready",
    "◐ Loading assets...",
    "◐ Preparing sphere mapping...",
    "◑ Optimizing performance..."
  };
  
  int totalMessages = sizeof(initMessages) / sizeof(initMessages[0]);
  int visibleMessages = (int)(progress * totalMessages);
  
  for (int i = 0; i < visibleMessages && i < totalMessages; i++) {
    M5.Display.setCursor(10, y + i * lineHeight);
    
    // 完了項目は緑、進行中は黄色
    if (strstr(initMessages[i], "✓")) {
      M5.Display.setTextColor(TFT_GREEN);
    } else {
      M5.Display.setTextColor(TFT_YELLOW);
    }
    
    M5.Display.print(initMessages[i]);
  }
  
  // プログレスバー
  int barY = y + totalMessages * lineHeight + 10;
  int barWidth = M5.Display.width() - 20;
  M5.Display.drawRect(10, barY, barWidth, 8, TFT_WHITE);
  M5.Display.fillRect(11, barY + 1, (int)(progress * (barWidth - 2)), 6, TFT_CYAN);
}

// Phase 3: 完了アニメーション
void drawCompletionAnimation(float progress) {
  int centerX = M5.Display.width() / 2;
  int centerY = M5.Display.height() / 2;
  
  // 成功メッセージ
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(centerX - 50, centerY - 10);
  M5.Display.print("READY!");
  
  // 拡散エフェクト
  float effectRadius = progress * 60.0f;
  for (int r = 0; r < effectRadius; r += 5) {
    uint16_t alpha = (uint16_t)(255 * (1.0f - (float)r/effectRadius));
    uint16_t color = M5.Display.color565(0, alpha >> 3, 0);
    M5.Display.drawCircle(centerX, centerY, r, color);
  }
}

// 転送進捗の表示
void drawTransferProgress() {
  // Core0Taskの転送進捗を取得（簡易版）
  bool transferInProgress = true; // 簡易版では常にtrue
  
  if (transferInProgress) {
    M5.Display.setTextColor(TFT_YELLOW);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(5, M5.Display.height() - 10);
    
    // 点滅ドット
    static int dotCount = 0;
    static unsigned long lastDotUpdate = 0;
    if (millis() - lastDotUpdate > 500) {
      dotCount = (dotCount + 1) % 4;
      lastDotUpdate = millis();
    }
    
    String dots = "";
    for (int i = 0; i < dotCount; i++) dots += ".";
    M5.Display.printf("Loading assets%s", dots.c_str());
  }
}

// 転送完了待機
void waitForTransferCompletion() {
  unsigned long waitStart = millis();
  const unsigned long maxWait = 5000; // 最大5秒待機
  
  while ((millis() - waitStart) < maxWait) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_CYAN);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(10, 50);
    M5.Display.print("Finalizing asset loading...");
    
    drawTransferProgress();
    
    delay(100);
    M5.update();
    
    // ユーザーがスキップ可能
    if (M5.BtnA.wasPressed()) {
      Serial.println("[Opening] Transfer wait skipped by user");
      break;
    }
  }
}

// 簡易テストアニメーション（フォールバック）
void playTestAnimation() {
  Serial.println("[Opening] Playing simple test animation...");
  
  for (int i = 0; i < 10; i++) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, 50);
    M5.Display.printf("Loading... %d", i + 1);
    
    delay(300);
    M5.update();
    if (M5.BtnA.wasPressed()) break;
  }
  
  M5.Display.fillScreen(TFT_BLACK);
  Serial.println("[Opening] Test animation completed");
}
// TODO: Implement proper CoreTasks
// SphereCore0Task core0Task(makeTaskConfig("SphereCore0Task", 0, 4, 4096, 50), configManager, storageManager, sharedState);
// SphereCore1Task core1Task(makeTaskConfig("SphereCore1Task", 1, 4, 4096, 20), sharedState);

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
  // PSRamFS初期化を一時的に無効化（ライブラリの問題により停止するため）
  Serial.println("[Storage] PSRamFS temporarily disabled due to library halt issue");
  Serial.println("[Storage] Will use LittleFS only mode for now");
  Serial.printf("[Timing] PSRamFS skip took %lu ms\n", millis() - t); 




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
      Serial.println("[Storage] Asset mirroring will be handled asynchronously by Core0Task");
      // 重いコピー処理はCore0Taskに委任して起動時間を短縮
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
    Serial.printf("[Buzzer] Startup tone callback invoked - buzzer enabled: %s\n", cfg.buzzer.enabled ? "true" : "false");
    if (!cfg.buzzer.enabled) {
      Serial.println("[Buzzer] Startup tone disabled by config");
      return;
    }
    Serial.println("[Buzzer] Creating BuzzerService...");
    BuzzerService buzzer;
    if (!buzzer.begin()) {
      Serial.println("[Buzzer] Initialization failed");
      return;
    }
    Serial.println("[Buzzer] Startup tone playing");
    buzzer.playStartupTone();
    buzzer.stop();
    Serial.println("[Buzzer] Startup tone completed");
  };

#if defined(IMU_SENSOR_BMI270)
  // IMUサービス初期化
  Serial.println("[IMU] Initializing IMU service...");
  if (imuService.begin()) {
    Serial.println("[IMU] IMU service initialized successfully");
  } else {
    Serial.println("[IMU] Failed to initialize IMU service");
  }
  scanInternalI2C("Internal I2C");
#elif defined(IMU_SENSOR_BNO055)
  Wire1.begin(2, 1);
  Wire1.setClock(400000);
  scanI2CBus(Wire1, "Wire1 (external)");
#endif
  // core1Task.markImuWireInitialized(); // TODO: Implement proper CoreTasks

  // Configure UI shake detection from config (if available)
  {
    // default values
    uint8_t triggerCount = 3;
    uint32_t windowMs = 900;
    if (configManager.isLoaded()) {
      const auto &cfg = configManager.config();
      triggerCount = static_cast<uint8_t>(cfg.imu.uiShakeTriggerCount);
      windowMs = cfg.imu.uiShakeWindowMs;
    }
    // create detector and bridge
    uiShakeDetector = new ShakeDetector(2.0f, static_cast<int>(triggerCount), windowMs, 2000, 1000);
    shakeToUiBridge = new ShakeToUiBridge(sharedState, static_cast<int>(triggerCount));
    Serial.printf("[IMU] UI shake detector configured: triggerCount=%d windowMs=%lu\n", triggerCount, (unsigned long)windowMs);
  }


  BootOrchestrator bootOrchestrator(storageManager, configManager, sharedState, bootCallbacks, bootServices);
  if (!bootOrchestrator.run()) {
    Serial.println("[Boot] Boot orchestrator failed - storage or staging incomplete");
  } else if (!bootOrchestrator.hasLoadedConfig()) {
    Serial.println("[Boot] Config not loaded during boot");
  }


  // TODO: Implement proper CoreTasks
  /*
  if (!core0Task.isStarted() && !core0Task.start()) {
    Serial.println("[Core0] Failed to start task");
  }
  if (!core1Task.isStarted() && !core1Task.start()) {
    Serial.println("[Core1] Failed to start task");
  }
  
  // プロシージャルオープニングを開始
  Serial.println("[Opening] Starting procedural opening animation...");
  core0Task.startOpening();
  core1Task.startOpening();
  */
  
  // FastLED初期化（一時的に無効化してSPI競合を回避）
#if defined(USE_FASTLED)
  Serial.println("Initializing FastLED via LEDSphereManager...");
  // Use runtime config to initialize multi-strip output if available
  if (configManager.isLoaded()) {
    const auto &cfg = configManager.config();
    if (!cfg.led.ledsPerStrip.empty() && !cfg.led.stripGpios.empty()) {
      sphereManager.initializeLedHardware(cfg.led.numStrips, cfg.led.ledsPerStrip, cfg.led.stripGpios);
    } else {
      Serial.println("[Main] LED config missing or incomplete, falling back to single-pin init");
      FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
      FastLED.clear(true);
      FastLED.show();
    }
  } else {
    Serial.println("[Main] Config not loaded, using default FastLED init");
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.clear(true);
    FastLED.show();
  }
  FastLED.setBrightness(50);
#else
  Serial.println("FastLED disabled (USE_FASTLED not defined)");
#endif

  // LED基盤システム初期化
  Serial.println("[LEDSphere] Initializing LED Sphere Manager...");
  if (sphereManager.initialize("/led_layout.csv")) {
    Serial.println("[LEDSphere] LED Sphere Manager initialized successfully");
    
    // パフォーマンステスター初期化
    if (perfTester.initialize(&sphereManager)) {
      Serial.println("[PerfTest] Performance tester ready");
      perfTester.setTestConfig(10000, true, true); // 10秒テスト
    } else {
      Serial.println("[PerfTest] Failed to initialize performance tester");
    }
  } else {
    Serial.println("[LEDSphere] LED Sphere Manager initialization failed");
  }
  
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
  
  // Proceduralオープニングを実行
  Serial.println("[Opening] Starting procedural opening animation...");
  playOpeningAnimation();

  Serial.println("Device Info:");
  Serial.println("- Heap free: " + String(ESP.getFreeHeap()));
  Serial.println("- PSRAM size: " + String(ESP.getPsramSize()));
  Serial.println("- Flash size: " + String(ESP.getFlashChipSize()));
  Serial.println("- CPU frequency: " + String(ESP.getCpuFreqMHz()) + "MHz");
  Serial.println("- MAC address: " + WiFi.macAddress());
  
  // 起動完了メッセージをLCDに表示
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 50);
  M5.Display.println("Main System");
  M5.Display.setCursor(30, 70);
  M5.Display.println("Ready");
  delay(2000); // 2秒間表示
  
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
  
  // ボタン操作処理
  static uint32_t lastBtnPressMs = 0;
  static bool testPatternActive = false;
  
  if (M5.BtnA.wasPressed()) {
    uint32_t now = millis();
    
    if (performanceTestMode) {
      // パフォーマンステストモード：簡易テスト実行
      Serial.println("[PerfTest] Running quick performance test...");
      PerformanceTest::runQuickPerformanceTest(&sphereManager);
    } else if (testPatternActive) {
      // テストパターンモード中：パターン切り替え（CoreTask無効化中）
      // core1Task.switchTestPattern();
      Serial.println("Switched test pattern (CoreTask disabled)");
    } else {
      // 通常モード：オープニング再生
      Serial.println("M5 Button pressed - playing opening animation");
      
      // PSRamFS優先、LittleFSフォールバック
      if (storageManager.isPsRamFsMounted() && PSRamFS.exists("/images/opening/001.jpg")) {
        Serial.println("[Opening] Playing from PSRamFS");
        playOpeningAnimation();
      } else if (storageManager.isLittleFsMounted() && LittleFS.exists("/images/opening/001.jpg")) {
        Serial.println("[Opening] PSRamFS unavailable, playing from LittleFS");
        playOpeningAnimationFromLittleFS();
      } else {
        Serial.println("Opening animation files not available in both PSRamFS and LittleFS");
        // テスト用の簡単なアニメーション表示
        playTestAnimation();
      }
    }
    lastBtnPressMs = now;
  }
  
  // ボタンA長押し（2秒）でテストパターンモード切り替え
  if (M5.BtnA.isPressed() && (millis() - lastBtnPressMs > 2000)) {
    if (!testPatternActive) {
      testPatternActive = true;
      // core1Task.enterTestPatternMode();
      Serial.println("Entered test pattern mode - Hold A: switch, B: exit (CoreTask disabled)");
    }
    lastBtnPressMs = millis(); // 連続入力防止
  }
  
  // ボタンBでテストパターン/パフォーマンステストモード終了
  if (M5.BtnB.wasPressed() && (testPatternActive || performanceTestMode)) {
    if (performanceTestMode) {
      performanceTestMode = false;
      Serial.println("Exited performance test mode");
      M5.Display.fillScreen(TFT_BLACK);
      M5.Display.setTextColor(TFT_GREEN);
      M5.Display.setCursor(10, 50);
      M5.Display.println("Performance");
      M5.Display.println("Test Mode OFF");
      delay(1000);
    } else {
      testPatternActive = false;
      // core1Task.exitTestPatternMode();
      Serial.println("Exited test pattern mode (CoreTask disabled)");
    }
  }

  if (M5.BtnPWR.wasClicked()) {
    if (!performanceTestMode && !testPatternActive) {
      // パフォーマンステストモードに切り替え
      performanceTestMode = true;
      Serial.println("[PerfTest] Entered performance test mode");
      
      M5.Display.fillScreen(TFT_BLACK);
      M5.Display.setTextColor(TFT_CYAN);
      M5.Display.setTextSize(1);
      M5.Display.setCursor(0, 0);
      M5.Display.println("=== PERF TEST MODE ===");
      M5.Display.println("A: Run quick test");
      M5.Display.println("B: Exit mode");
      M5.Display.println("PWR: Full test suite");
      delay(2000);
    } else if (performanceTestMode) {
      // フルテストスイート実行
      Serial.println("[PerfTest] Running full test suite...");
      
      M5.Display.fillScreen(TFT_BLACK);
      M5.Display.setTextColor(TFT_YELLOW);
      M5.Display.setCursor(0, 0);
      M5.Display.println("Full Performance Test");
      M5.Display.println("Starting...");
      
      auto results = perfTester.testAllPatterns();
      
      // 結果をシリアルに詳細表示
      for (const auto& pair : results) {
        perfTester.printResults(pair.second, pair.first.c_str());
      }
    } else {
      Serial.println("[IMU] Calibration requested from power button (CoreTask disabled)");
      // core1Task.requestImuCalibration();
    }
  }

  // IMUデータ更新（100Hz目標 = 10ms間隔）
  static uint32_t lastImuUpdateMs = 0;
  if (millis() - lastImuUpdateMs >= 10) {
    ImuService::Reading imuReading;
    if (imuService.read(imuReading)) {
      sharedState.updateImuReading(imuReading);
      // Feed IMU accelerations to shake detector (if configured)
      if (uiShakeDetector && shakeToUiBridge) {
        bool detected = uiShakeDetector->update(imuReading.ax, imuReading.ay, imuReading.az, imuReading.timestampMs);
        if (detected) {
          Serial.println("[IMU] Shake detected -> notifying ShakeToUiBridge");
          shakeToUiBridge->onShakeDetected();
        }
      }
    }
    lastImuUpdateMs = millis();
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
  static uint32_t lastImuDebugMs = 0;
  if (millis() - lastImuOverlayMs >= 200) {
    ImuService::Reading imuReading;
    bool uiActive = false;
    bool uiStateKnown = sharedState.getUiMode(uiActive);
    if (sharedState.getImuReading(imuReading)) {
      lastImuOverlayMs = millis();
      
      // デバッグ: IMUデータ取得成功を1秒に1回だけ表示
      if (millis() - lastImuDebugMs >= 1000) {
        Serial.printf("[IMU] Data: qw=%6.3f qx=%6.3f qy=%6.3f qz=%6.3f\n", 
                     imuReading.qw, imuReading.qx, imuReading.qy, imuReading.qz);
        lastImuDebugMs = millis();
      }
      
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
    } else {
      // デバッグ: IMUデータ取得失敗を5秒に1回だけ表示
      static uint32_t lastErrorMs = 0;
      if (millis() - lastErrorMs >= 5000) {
        Serial.println("[IMU] No data available from SharedState");
        lastErrorMs = millis();
      }
    }
  }
  
  // delay(3);
  delay(1);
}

#endif  // UNIT_TEST


#ifdef UNIT_TEST
// --- PlatformIO test build用ダミー ---
void setup() {}
void loop() {}
#endif
