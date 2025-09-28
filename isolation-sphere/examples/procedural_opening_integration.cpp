/**
 * @brief JPEG→ProceduralPattern完全置き換え統合例
 * 
 * LittleFSフォーマット時間と完全同期したProceduralオープニング実装
 * 従来のJPEGアニメーションを全て置き換え
 */

// main.cppへの統合例
#include "boot/ProceduralOpeningSequence.h"

// Global instances (既存のmain.cppに追加)
extern LEDSphere::LEDSphereManager sphereManager;
extern StorageManager storageManager;
SynchronizedBootSequence* syncBootSequence = nullptr;

/**
 * @brief JPEG関数群を完全置き換え（後方互換性保持）
 */

// 🗑️ 旧JPEG関数群 → ProceduralPattern完全置き換え
void playOpeningAnimation() {
    Serial.println("[Opening] 🎬 JPEG→Procedural: Starting procedural opening...");
    
    if (!syncBootSequence) {
        Serial.println("[Opening] ❌ SyncBootSequence not initialized");
        return;
    }
    
    // 軽い処理のシミュレーション（元JPEGアニメーション時間相当）
    auto lightTask = [](std::function<void(float)> progressCallback) -> bool {
        Serial.println("[Opening] 🎬 Simulating light task for procedural opening...");
        
        const int steps = 30; // 3秒 ÷ 100ms = 30ステップ
        for (int i = 0; i < steps; i++) {
            float progress = static_cast<float>(i + 1) / steps;
            progressCallback(progress);
            
            delay(100); // 100ms間隔
            esp_task_wdt_reset();
        }
        
        Serial.println("[Opening] ✅ Light task completed");
        return true;
    };
    
    SynchronizedBootSequence::BootConfig config;
    config.taskName = "Procedural Opening";
    config.estimatedDuration = 3.0f; // 3秒
    config.showDetailed = true;
    
    bool success = syncBootSequence->executeBootWithOpening(lightTask, config);
    Serial.printf("[Opening] 🎬 Procedural opening: %s\n", success ? "SUCCESS" : "FAILED");
}

void playOpeningAnimationFromLittleFS() {
    Serial.println("[Opening] 🎬 LittleFS→Procedural: Redirecting to procedural opening...");
    playOpeningAnimation(); // Proceduralにリダイレクト
}

void playOpeningAnimationFromFS(fs::FS &fileSystem, const char* fsName) {
    Serial.printf("[Opening] 🎬 %s→Procedural: Redirecting to procedural opening...\n", fsName);
    playOpeningAnimation(); // Proceduralにリダイレクト
}

void playProceduralOpening() {
    Serial.println("[Opening] 🎬 Direct procedural opening call");
    playOpeningAnimation(); // 統一関数に集約
}

void playTestAnimation() {
    Serial.println("[Opening] 🎬 Test→Procedural: Using procedural test pattern...");
    
    if (!syncBootSequence) {
        // フォールバック：簡易テスト
        for (int i = 0; i < 5; i++) {
            M5.Display.fillScreen(TFT_BLACK);
            M5.Display.setTextColor(TFT_WHITE);
            M5.Display.setTextSize(2);
            M5.Display.setCursor(20, 50);
            M5.Display.printf("Test %d", i + 1);
            delay(600);
        }
        return;
    }
    
    // ProceduralPattern テスト
    auto testTask = [](std::function<void(float)> progressCallback) -> bool {
        Serial.println("[Opening] 🎬 Test procedural pattern...");
        
        for (int i = 0; i < 10; i++) {
            float progress = static_cast<float>(i + 1) / 10;
            progressCallback(progress);
            delay(300);
            esp_task_wdt_reset();
        }
        
        return true;
    };
    
    SynchronizedBootSequence::BootConfig config;
    config.taskName = "Procedural Test";
    config.estimatedDuration = 3.0f;
    config.showDetailed = true;
    
    syncBootSequence->executeBootWithOpening(testTask, config);
}

/**
 * @brief setup()への統合例（LittleFSフォーマット完全同期）
 */
void setupWithProceduralOpeningIntegration() {
    Serial.begin(115200);
    delay(100);
    Serial.println("[Setup] 🚀 Starting setup with Procedural Opening integration...");
    
    // =======================================================================
    // Phase 1: 基本初期化
    // =======================================================================
    
    // M5Unified初期化
    auto cfg = M5.config();
    cfg.external_spk = false;
    cfg.output_power = true;
    cfg.internal_imu = true;
    cfg.internal_rtc = true;
    cfg.fallback_board = m5::board_t::board_M5AtomS3R;
    M5.begin(cfg);
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // PSRAMテスト
    if (ESP.getPsramSize() > 0) {
        Serial.printf("[Setup] PSRAM: %d MB available\n", ESP.getPsramSize() / (1024*1024));
    }
    
    // =======================================================================
    // Phase 2: LEDシステム初期化（Procedural準備）
    // =======================================================================
    
    Serial.println("[Setup] 🎨 Initializing LED system for procedural opening...");
    
    if (!sphereManager.initialize("/led_layout.csv")) {
        Serial.println("[Setup] ❌ LED Sphere Manager initialization failed");
        // フォールバック処理
    } else {
        Serial.println("[Setup] ✅ LED Sphere Manager ready for procedural opening");
    }
    
    // SynchronizedBootSequence初期化
    syncBootSequence = new SynchronizedBootSequence(sphereManager);
    
    // =======================================================================  
    // Phase 3: 🎬 LittleFSフォーマット + ProceduralOpening同期実行
    // =======================================================================
    
    Serial.println("[Setup] 🎬 Starting LittleFS format with synchronized procedural opening...");
    
    auto littleFsFormatTaskWithProgress = [](std::function<void(float)> progressCallback) -> bool {
        Serial.println("[LittleFS] 🎬 Starting format with procedural opening sync...");
        uint32_t startMs = millis();
        
        // 進捗シミュレーション（実際のフォーマット中）
        progressCallback(0.1f); // 10%: 開始
        
        // 実際のLittleFSフォーマット
        bool success = LittleFS.begin(true, "/littlefs", 10, "littlefs");
        
        progressCallback(0.7f); // 70%: フォーマット完了
        
        if (success) {
            Serial.println("[LittleFS] ✅ Format successful with procedural opening!");
            LittleFS.end();
            progressCallback(0.9f); // 90%: クリーンアップ
        } else {
            Serial.println("[LittleFS] ❌ Format failed!");
            return false;
        }
        
        progressCallback(1.0f); // 100%: 完了
        
        uint32_t elapsedMs = millis() - startMs;
        Serial.printf("[LittleFS] 🎬 Format with procedural opening took %ums\n", elapsedMs);
        return true;
    };
    
    SynchronizedBootSequence::BootConfig formatConfig;
    formatConfig.taskName = "LittleFS Format + Procedural Opening";
    formatConfig.estimatedDuration = 3.0f;  // 3秒目標
    formatConfig.showDetailed = true;
    formatConfig.fallbackToFastMode = true;
    
    bool formatSuccess = syncBootSequence->executeBootWithOpening(
        littleFsFormatTaskWithProgress, 
        formatConfig
    );
    
    Serial.printf("[Setup] 🎬 LittleFS + Procedural Opening: %s\n", 
                  formatSuccess ? "SUCCESS" : "FAILED");
    
    // 結果統計表示
    auto result = syncBootSequence->getLastResult();
    Serial.println("[Setup] 📊 Synchronized Boot Results:");
    Serial.printf("  Task Success: %s\n", result.taskSuccess ? "✅" : "❌");
    Serial.printf("  Opening Success: %s\n", result.openingSuccess ? "✅" : "❌");
    Serial.printf("  Total Time: %ums\n", result.totalTimeMs);
    Serial.printf("  Opening FPS: %.1f\n", result.openingFps);
    Serial.printf("  Time Target: %s\n", result.timeTargetMet ? "✅ MET" : "⚠️ MISSED");
    
    // =======================================================================
    // Phase 4: その他初期化
    // =======================================================================
    
    // FastLED初期化（短縮版）
#if defined(USE_FASTLED)
    Serial.println("[Setup] FastLED initialization (optimized)...");
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    
    // 短縮テスト（各色200ms）
    leds[0] = CRGB::Red; FastLED.show(); delay(200);
    leds[0] = CRGB::Green; FastLED.show(); delay(200);
    leds[0] = CRGB::Blue; FastLED.show(); delay(200);
    leds[0] = CRGB::Black; FastLED.show();
    
    Serial.println("[Setup] ✅ FastLED initialized (optimized)");
#endif
    
    // パフォーマンステスター初期化
    if (perfTester.initialize(&sphereManager)) {
        Serial.println("[Setup] ✅ Performance tester ready");
        perfTester.setTestConfig(10000, true, true);
    }
    
    // 起動完了表示
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_GREEN);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(5, 30);
    M5.Display.printf("System Ready");
    M5.Display.setTextSize(1);
    M5.Display.setCursor(5, 60);
    M5.Display.printf("Procedural Opening");
    M5.Display.setCursor(5, 75);
    M5.Display.printf("Integrated!");
    
    delay(2000);
    
    Serial.println("[Setup] 🎉 Setup complete with full Procedural Opening integration!");
}

/**
 * @brief runtime中でのProceduralオープニング使用例
 */
void demonstrateProceduralOpeningAtRuntime() {
    if (!syncBootSequence) {
        Serial.println("[Demo] ❌ SyncBootSequence not available");
        return;
    }
    
    Serial.println("[Demo] 🎬 Runtime procedural opening demonstration...");
    
    // 任意の重い処理とProceduralオープニング同期デモ
    auto demoHeavyProcess = [](std::function<void(float)> progressCallback) -> bool {
        Serial.println("[Demo] 🎬 Heavy process simulation with procedural opening...");
        
        const int totalSteps = 50;
        for (int step = 0; step < totalSteps; step++) {
            // CPU集約的処理シミュレーション
            volatile long computation = 0;
            for (long i = 0; i < 50000; i++) {
                computation += i * i % 1000;
            }
            
            float progress = static_cast<float>(step + 1) / totalSteps;
            progressCallback(progress);
            
            delay(60); // I/O待機シミュレーション（合計3秒）
            esp_task_wdt_reset();
        }
        
        Serial.println("[Demo] ✅ Heavy process completed");
        return true;
    };
    
    SynchronizedBootSequence::BootConfig config;
    config.taskName = "Runtime Demo Process";
    config.estimatedDuration = 3.0f;
    config.showDetailed = true;
    
    bool success = syncBootSequence->executeBootWithOpening(demoHeavyProcess, config);
    
    auto result = syncBootSequence->getLastResult();
    Serial.printf("[Demo] 🎬 Runtime demo result: %s (%.1f fps)\n", 
                  success ? "SUCCESS" : "FAILED", 
                  result.openingFps);
}