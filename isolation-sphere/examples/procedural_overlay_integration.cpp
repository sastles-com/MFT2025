/**
 * @brief 重い処理中のProceduralPatternオーバーレイ統合例
 * 
 * LittleFSフォーマット等の重い処理中に別コアでProceduralPatternを表示し、
 * ユーザー体験を向上させる実装例
 */

// main.cppへの統合コード例
#include "boot/BootTimeProceduralOverlay.h"

// Global instances (既存のmain.cppに追加)
extern LEDSphere::LEDSphereManager sphereManager;
HeavyTaskWithOverlay* heavyTaskOverlay = nullptr;

/**
 * @brief setup()への統合例
 */
void setupWithProceduralOverlay() {
    Serial.begin(115200);
    delay(100);
    
    // =======================================================================
    // Phase 1: 高速初期化（オーバーレイ無し）
    // =======================================================================
    Serial.println("[Setup] Phase 1: Critical initialization...");
    
    // M5Unified初期化
    auto cfg = M5.config();
    cfg.external_spk = false;
    cfg.output_power = true;
    cfg.internal_imu = true;
    cfg.internal_rtc = true;
    cfg.fallback_board = m5::board_t::board_M5AtomS3R;
    M5.begin(cfg);
    
    // PSRAMテスト
    if (ESP.getPsramSize() > 0) {
        Serial.printf("[Setup] PSRAM: %d MB available\n", ESP.getPsramSize() / (1024*1024));
    }
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // =======================================================================
    // Phase 2: LED基盤システム初期化（オーバーレイ準備）
    // =======================================================================
    Serial.println("[Setup] Phase 2: LED system initialization...");
    
    // LEDSphereManager初期化（オーバーレイに必要）
    if (!sphereManager.initialize("/led_layout.csv")) {
        Serial.println("[Setup] LED Sphere Manager initialization failed");
        // フォールバック処理
    } else {
        Serial.println("[Setup] LED Sphere Manager ready for overlay");
    }
    
    // オーバーレイシステム初期化
    heavyTaskOverlay = new HeavyTaskWithOverlay(sphereManager);
    
    // =======================================================================
    // Phase 3: 重い処理をProceduralPatternオーバーレイ付きで実行
    // =======================================================================
    Serial.println("[Setup] Phase 3: Heavy tasks with procedural overlay...");
    
    // 🚀 LittleFSフォーマット（最も重い処理）をオーバーレイ付きで実行
    {
        HeavyTaskWithOverlay::TaskConfig config;
        config.taskName = "LittleFS Format";
        config.estimatedTimeMs = 3000;  // 3秒想定
        config.overlayPattern = BootTimeProceduralOverlay::OverlayPattern::BOOT_PROGRESS;
        config.showProgressOnLCD = true;
        
        auto littleFsFormatTask = []() -> bool {
            Serial.println("[LittleFS] Starting format with overlay...");
            uint32_t startMs = millis();
            
            bool success = LittleFS.begin(true, "/littlefs", 10, "littlefs");
            if (success) {
                Serial.println("[LittleFS] Format successful!");
                LittleFS.end();
            } else {
                Serial.println("[LittleFS] Format failed!");
            }
            
            uint32_t elapsedMs = millis() - startMs;
            Serial.printf("[LittleFS] Format took %ums\n", elapsedMs);
            return success;
        };
        
        bool formatSuccess = heavyTaskOverlay->executeWithOverlay(
            littleFsFormatTask, 
            config
        );
        
        Serial.printf("[Setup] LittleFS format: %s\n", formatSuccess ? "SUCCESS" : "FAILED");
    }
    
    // 🚀 FastLED初期化をオーバーレイ付きで実行
    {
        HeavyTaskWithOverlay::TaskConfig config;
        config.taskName = "FastLED Init";
        config.estimatedTimeMs = 1500;  // 1.5秒想定  
        config.overlayPattern = BootTimeProceduralOverlay::OverlayPattern::ROTATING_AXIS;
        config.showProgressOnLCD = true;
        
        auto fastLedInitTask = []() -> bool {
#if defined(USE_FASTLED)
            Serial.println("[FastLED] Starting initialization with overlay...");
            
            FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
            FastLED.setBrightness(50);
            
            // LED動作テスト（オーバーレイ表示中）
            Serial.println("[FastLED] LED test starting...");
            
            // 各色500ms → 200msに短縮（オーバーレイがメイン表示）
            leds[0] = CRGB::Red;
            FastLED.show();
            delay(200);
            
            leds[0] = CRGB::Green;
            FastLED.show();
            delay(200);
            
            leds[0] = CRGB::Blue;
            FastLED.show();
            delay(200);
            
            leds[0] = CRGB::Black;
            FastLED.show();
            
            Serial.println("[FastLED] Initialization complete with overlay!");
            return true;
#else
            Serial.println("[FastLED] Disabled - simulation delay");
            delay(1500); // シミュレーション
            return true;
#endif
        };
        
        bool ledSuccess = heavyTaskOverlay->executeWithOverlay(
            fastLedInitTask,
            config
        );
        
        Serial.printf("[Setup] FastLED init: %s\n", ledSuccess ? "SUCCESS" : "FAILED");
    }
    
    // 🚀 オープニングアニメーション（オプション：短縮版）
    {
        HeavyTaskWithOverlay::TaskConfig config;
        config.taskName = "Opening Animation";
        config.estimatedTimeMs = 2000;  // 短縮版2秒
        config.overlayPattern = BootTimeProceduralOverlay::OverlayPattern::LOADING_SPIRAL;
        config.showProgressOnLCD = false; // アニメーション表示のため
        
        auto openingAnimationTask = []() -> bool {
            Serial.println("[Opening] Starting short animation with overlay...");
            
            // 既存のオープニング関数を呼び出すか、短縮版を実行
            // playOpeningAnimation(); // 既存の50フレーム版
            
            // 短縮版（20フレーム、2秒）
            if (storageManager.isPsRamFsMounted() && PSRamFS.exists("/images/opening/001.jpg")) {
                const int totalFrames = 20; // 50→20に短縮
                const int frameDelay = 100;  // 10fps維持
                
                for (int frame = 1; frame <= totalFrames; frame++) {
                    // 簡略化されたJPEG表示処理
                    // （詳細は既存のplayOpeningAnimation参照）
                    
                    delay(frameDelay);
                    esp_task_wdt_reset();
                    
                    // 中断チェック
                    M5.update();
                    if (M5.BtnA.wasPressed()) {
                        Serial.println("[Opening] Animation interrupted");
                        break;
                    }
                }
            } else {
                // フォールバック：短縮テストアニメーション
                for (int i = 0; i < 5; i++) {
                    M5.Display.fillScreen(TFT_BLACK);
                    M5.Display.setTextColor(TFT_WHITE);
                    M5.Display.setTextSize(2);
                    M5.Display.setCursor(20, 50);
                    M5.Display.printf("Loading %d", i + 1);
                    delay(400);
                }
            }
            
            return true;
        };
        
        bool animationSuccess = heavyTaskOverlay->executeWithOverlay(
            openingAnimationTask,
            config
        );
        
        Serial.printf("[Setup] Opening animation: %s\n", animationSuccess ? "SUCCESS" : "FAILED");
    }
    
    // =======================================================================
    // Phase 4: 通常初期化完了
    // =======================================================================
    
    // パフォーマンステスター初期化
    if (perfTester.initialize(&sphereManager)) {
        Serial.println("[Setup] Performance tester ready");
        perfTester.setTestConfig(10000, true, true);
    }
    
    // 統計表示
    if (heavyTaskOverlay) {
        auto stats = heavyTaskOverlay->getLastExecutionStats();
        Serial.println("[Setup] 📊 Heavy Task Performance Report:");
        Serial.printf("  Task Time: %ums\n", stats.actualTaskTimeMs);
        Serial.printf("  Overlay FPS: %.1f\n", stats.avgOverlayFps);
        Serial.printf("  Success: Task=%s, Overlay=%s\n",
                      stats.taskSuccess ? "✅" : "❌",
                      stats.overlaySuccess ? "✅" : "❌");
    }
    
    // 起動完了表示
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_GREEN);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 40);
    M5.Display.printf("System\nReady!");
    M5.Display.setTextSize(1);
    M5.Display.setCursor(10, 80);
    M5.Display.printf("With Procedural\nOverlay Support");
    
    delay(2000);
    
    Serial.println("[Setup] 🎉 Setup complete with procedural overlay optimization!");
}

/**
 * @brief runtime中でのオーバーレイ使用例
 */
void demonstrateRuntimeOverlay() {
    if (!heavyTaskOverlay) return;
    
    Serial.println("[Demo] Demonstrating runtime overlay usage...");
    
    HeavyTaskWithOverlay::TaskConfig config;
    config.taskName = "Heavy Processing Demo";
    config.estimatedTimeMs = 3000;
    config.overlayPattern = BootTimeProceduralOverlay::OverlayPattern::PULSING_SPHERE;
    config.showProgressOnLCD = true;
    
    auto demoHeavyTask = []() -> bool {
        Serial.println("[Demo] Starting heavy computation...");
        
        // 重い処理のシミュレーション
        for (int i = 0; i < 30; i++) {
            // CPU集約的処理のシミュレーション
            volatile long result = 0;
            for (long j = 0; j < 100000; j++) {
                result += j * j;
            }
            
            delay(100); // I/O待機のシミュレーション
            
            Serial.printf("[Demo] Progress: %d/30\n", i + 1);
        }
        
        Serial.println("[Demo] Heavy computation completed");
        return true;
    };
    
    // 進捗コールバック付きで実行
    auto progressCallback = [](float progress) {
        Serial.printf("[Demo] Progress callback: %.1f%%\n", progress * 100.0f);
    };
    
    bool success = heavyTaskOverlay->executeWithOverlay(
        demoHeavyTask,
        config,
        progressCallback
    );
    
    Serial.printf("[Demo] Runtime overlay demo: %s\n", success ? "SUCCESS" : "FAILED");
}