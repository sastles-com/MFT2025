/**
 * @brief 高速起動シーケンス実装例
 * 
 * パフォーマンステストで確認された30fps能力を活用し、
 * 3秒以内に基本機能を利用可能にする最適化起動シーケンス
 */

// =============================================================================
// Fast Boot Integration Example for main.cpp
// =============================================================================

#include "boot/FastBootOrchestrator.h"
#include "core/FastBootCoreTasks.h"
#include "led/LEDSphereManager.h"
#include "test/ProceduralPatternPerformanceTest.h"

// Global instances (実際のmain.cppに統合時は既存インスタンスを活用)
extern ConfigManager configManager;
extern StorageManager storageManager;
extern SharedState sharedState;
extern LEDSphere::LEDSphereManager sphereManager;
extern PerformanceTest::ProceduralPatternPerformanceTester perfTester;

// Fast boot components
FastBootCore0Task* fastBootCore0 = nullptr;
FastBootCore1Task* fastBootCore1 = nullptr;
FastBootOrchestrator* fastBootOrchestrator = nullptr;

/**
 * @brief 高速起動setup()統合例
 */
void setupFastBoot() {
    Serial.begin(115200);
    delay(100);
    Serial.println("[FastBoot] 🚀 Starting optimized boot sequence...");
    
    // =======================================================================
    // Phase 1: Critical Hardware (0-500ms)
    // =======================================================================
    uint32_t phaseStart = millis();
    
    // M5Unified最小限初期化
    auto cfg = M5.config();
    cfg.external_spk = false;
    cfg.output_power = true;
    cfg.internal_imu = true;
    cfg.internal_rtc = true;
    cfg.fallback_board = m5::board_t::board_M5AtomS3R;
    M5.begin(cfg);
    
    // PSRAMテスト（簡略化）
    if (ESP.getPsramSize() > 0) {
        Serial.printf("[FastBoot] PSRAM: %d MB available\n", ESP.getPsramSize() / (1024*1024));
    }
    
    Serial.printf("[FastBoot] Phase 1 Critical: %lums\n", millis() - phaseStart);
    
    // =======================================================================
    // Phase 2: Fast Boot Orchestrator Setup (500-1000ms)
    // =======================================================================
    phaseStart = millis();
    
    // FastBoot CoreTasks初期化
    CoreTask::TaskConfig core0Config;
    core0Config.name = "FastBootCore0";
    core0Config.coreId = 0;
    core0Config.priority = 2;
    core0Config.stackSize = 8192;
    core0Config.loopIntervalMs = 10;
    
    CoreTask::TaskConfig core1Config;
    core1Config.name = "FastBootCore1";
    core1Config.coreId = 1;
    core1Config.priority = 3;
    core1Config.stackSize = 8192;
    core1Config.loopIntervalMs = 16; // ~60fps対応
    
    fastBootCore0 = new FastBootCore0Task(core0Config, configManager, storageManager, sharedState);
    fastBootCore1 = new FastBootCore1Task(core1Config, sharedState, sphereManager);
    
    // FastBootOrchestrator設定
    FastBootOrchestrator::PhaseCallbacks callbacks;
    callbacks.onCriticalPhaseComplete = []() {
        Serial.println("[FastBoot] ✅ Critical phase complete - basic functions ready");
        return true;
    };
    
    callbacks.onFunctionalPhaseComplete = []() {
        Serial.println("[FastBoot] ✅ Functional phase complete - LED/IMU systems ready");
        // LEDで成功を表示
        if (fastBootCore1) {
            fastBootCore1->displayBootProgress(0.6f);
        }
        return true;
    };
    
    callbacks.onEnhancedPhaseComplete = []() {
        Serial.println("[FastBoot] ✅ Enhanced phase complete - full system ready");
        // オープニングアニメーション開始
        if (fastBootCore1) {
            fastBootCore1->displayBootProgress(1.0f);
        }
        return true;
    };
    
    FastBootOrchestrator::BootServices services;
    
    // Phase 1 services (critical)
    services.initializeHardware = []() {
        // 基本GPIO初期化など
        pinMode(BUTTON_PIN, INPUT_PULLUP);
        return true;
    };
    
    services.loadMinimalConfig = []() {
        // 軽量config読み込み（LittleFSフォーマット回避）
        return storageManager.begin(false); // フォーマット無し
    };
    
    // Phase 2 services (functional) 
    services.initializeLEDSystem = []() {
        Serial.println("[FastBoot] Initializing LED system...");
        bool success = sphereManager.initialize("/led_layout.csv");
        if (success && fastBootCore1) {
            fastBootCore1->startImmediatePatterns();
        }
        return success;
    };
    
    services.initializeIMU = []() {
        Serial.println("[FastBoot] Initializing IMU...");
        // IMU初期化（並列実行可能）
        return true; // 実際のIMU初期化コード
    };
    
    services.startProceduralPatterns = []() {
        // ProceduralPattern即座開始（30fps確認済み）
        Serial.println("[FastBoot] Starting procedural patterns...");
        if (fastBootCore1) {
            fastBootCore1->requestPhase(FastBootCore1Task::InitPhase::PATTERNS_READY);
        }
        return true;
    };
    
    // Phase 3 services (enhanced - background)
    services.stageImageAssets = []() {
        // 画像アセット準備（バックグラウンド）
        Serial.println("[FastBoot] Staging image assets (background)...");
        if (fastBootCore0) {
            fastBootCore0->requestPhase(FastBootCore0Task::InitPhase::ASSETS_STAGING);
        }
        return true;
    };
    
    services.initializeCommunication = []() {
        // MQTT/UDP初期化（バックグラウンド）
        Serial.println("[FastBoot] Initializing communication (background)...");
        if (fastBootCore0) {
            fastBootCore0->requestPhase(FastBootCore0Task::InitPhase::COMMUNICATION_INIT);
        }
        return true;
    };
    
    services.playStartupAnimation = []() {
        // オープニングアニメーション（最後）
        Serial.println("[FastBoot] Playing startup animation...");
        // 実際のアニメーション再生コード
    };
    
    fastBootOrchestrator = new FastBootOrchestrator(
        storageManager, configManager, sharedState, 
        callbacks, services
    );
    
    Serial.printf("[FastBoot] Phase 2 Setup: %lums\n", millis() - phaseStart);
    
    // =======================================================================
    // Phase 3: Fast Boot Execution (1000-3000ms)
    // =======================================================================
    phaseStart = millis();
    
    // CoreTasks開始
    if (!fastBootCore0->start()) {
        Serial.println("[FastBoot] ❌ Failed to start FastBootCore0Task");
    }
    
    if (!fastBootCore1->start()) {
        Serial.println("[FastBoot] ❌ Failed to start FastBootCore1Task");
    }
    
    // 高速起動実行
    bool bootSuccess = fastBootOrchestrator->runFastBoot();
    
    Serial.printf("[FastBoot] Phase 3 Execution: %lums\n", millis() - phaseStart);
    
    // =======================================================================
    // Boot Results Display
    // =======================================================================
    const auto& timing = fastBootOrchestrator->getBootTiming();
    
    Serial.println("[FastBoot] 📊 Boot Performance Report:");
    Serial.printf("  Critical Phase: %lums/%lums %s\n",
                  timing.phase1Actual.count(),
                  timing.phase1Target.count(),
                  timing.phase1OnTime() ? "✅" : "❌");
    
    Serial.printf("  Functional Phase: %lums/%lums %s\n", 
                  timing.phase2Actual.count(),
                  timing.phase2Target.count(),
                  timing.phase2OnTime() ? "✅" : "❌");
    
    if (bootSuccess && timing.phase2OnTime()) {
        Serial.println("[FastBoot] 🎉 Fast boot SUCCESS - System ready for use!");
        
        // 成功表示
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextColor(TFT_GREEN);
        M5.Display.setTextSize(2);
        M5.Display.setCursor(10, 40);
        M5.Display.printf("Fast Boot\n");
        M5.Display.printf("SUCCESS!\n");
        M5.Display.setTextSize(1);
        M5.Display.printf("Ready in %lums", timing.phase2Actual.count());
        
    } else {
        Serial.println("[FastBoot] ⚠️ Fast boot targets missed - falling back to standard boot");
        
        // フォールバック処理
        M5.Display.fillScreen(TFT_YELLOW);
        M5.Display.setTextColor(TFT_BLACK);
        M5.Display.setTextSize(1);
        M5.Display.setCursor(10, 40);
        M5.Display.printf("Standard Boot\n");
        M5.Display.printf("Active\n");
    }
    
    delay(2000); // 結果表示
}

/**
 * @brief 高速起動対応loop()
 */
void loopFastBoot() {
    M5.update();
    
    // バックグラウンド処理進捗表示
    static uint32_t lastProgressUpdate = 0;
    if (millis() - lastProgressUpdate > 100) { // 10fps更新
        if (fastBootOrchestrator && !fastBootOrchestrator->isBackgroundProcessingComplete()) {
            float progress = fastBootOrchestrator->getBootProgress();
            if (fastBootCore1) {
                fastBootCore1->displayBootProgress(progress);
            }
        }
        lastProgressUpdate = millis();
    }
    
    // 既存のボタン処理・その他loop処理
    // ...
    
    delay(1);
}