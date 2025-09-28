#include "boot/FastBootOrchestrator.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <M5Unified.h>

FastBootOrchestrator::FastBootOrchestrator(StorageManager& storage,
                                         ConfigManager& config,
                                         SharedState& shared,
                                         PhaseCallbacks callbacks,
                                         BootServices services)
    : storage_(storage),
      config_(config),
      shared_(shared),
      callbacks_(std::move(callbacks)),
      services_(std::move(services)) {}

bool FastBootOrchestrator::runFastBoot() {
    Serial.println("[FastBoot] 🚀 Starting high-performance boot sequence...");
    startBootTimer();
    
    // Phase 1: Critical (0-1秒) - 最低限動作
    if (!executeCriticalPhase()) {
        Serial.println("[FastBoot] ❌ Critical phase failed");
        return false;
    }
    markPhaseComplete(BootPhase::PHASE_CRITICAL);
    
    // Phase 2: Functional (1-3秒) - 基本機能利用可能
    if (!executeFunctionalPhase()) {
        Serial.println("[FastBoot] ❌ Functional phase failed");
        return false;
    }
    markPhaseComplete(BootPhase::PHASE_FUNCTIONAL);
    
    // Phase 3: Enhanced (バックグラウンド) - 高品質機能
    executeEnhancedPhaseAsync();
    
    Serial.printf("[FastBoot] ✅ Fast boot complete in %lums (Target: %lums)\n",
                  timing_.phase2Actual.count(), timing_.phase2Target.count());
    
    return true;
}

bool FastBootOrchestrator::executeCriticalPhase() {
    Serial.println("[FastBoot] Phase 1: Critical initialization...");
    auto phaseStart = std::chrono::steady_clock::now();
    
    // 必須ハードウェア初期化
    if (services_.initializeHardware && !services_.initializeHardware()) {
        Serial.println("[FastBoot] Hardware initialization failed");
        return false;
    }
    
    // 最小限config読み込み（重いLittleFSフォーマットをスキップ）
    if (services_.loadMinimalConfig && !services_.loadMinimalConfig()) {
        Serial.println("[FastBoot] Minimal config load failed");
        // 非致命的エラー：デフォルト設定で継続
    }
    
    // WDTリセット
    esp_task_wdt_reset();
    
    auto elapsed = std::chrono::steady_clock::now() - phaseStart;
    timing_.phase1Actual = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
    
    Serial.printf("[FastBoot] Phase 1 complete: %lums/%lums %s\n",
                  timing_.phase1Actual.count(),
                  timing_.phase1Target.count(),
                  timing_.phase1OnTime() ? "✅" : "⚠️");
    
    if (callbacks_.onCriticalPhaseComplete) {
        callbacks_.onCriticalPhaseComplete();
    }
    
    return true;
}

bool FastBootOrchestrator::executeFunctionalPhase() {
    Serial.println("[FastBoot] Phase 2: Functional systems...");
    auto phaseStart = std::chrono::steady_clock::now();
    
    currentPhase_ = BootPhase::PHASE_FUNCTIONAL;
    
    // LED基盤システム初期化（パフォーマンステスト済み30fps対応）
    if (services_.initializeLEDSystem && !services_.initializeLEDSystem()) {
        Serial.println("[FastBoot] LED system initialization failed");
        return false;
    }
    
    // IMU初期化（並列可能）
    if (services_.initializeIMU && !services_.initializeIMU()) {
        Serial.println("[FastBoot] IMU initialization failed");
        // 非致命的エラー：IMU無しで継続
    }
    
    // ProceduralPattern即座開始（30fps確認済み）
    if (services_.startProceduralPatterns && !services_.startProceduralPatterns()) {
        Serial.println("[FastBoot] Procedural patterns failed to start");
        return false;
    }
    
    // WDTリセット
    esp_task_wdt_reset();
    
    auto elapsed = std::chrono::steady_clock::now() - bootStartTime_;
    timing_.phase2Actual = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
    
    Serial.printf("[FastBoot] Phase 2 complete: %lums/%lums %s\n",
                  timing_.phase2Actual.count(),
                  timing_.phase2Target.count(),
                  timing_.phase2OnTime() ? "✅" : "⚠️");
    
    if (callbacks_.onFunctionalPhaseComplete) {
        callbacks_.onFunctionalPhaseComplete();
    }
    
    return true;
}

void FastBootOrchestrator::executeEnhancedPhaseAsync() {
    Serial.println("[FastBoot] Phase 3: Enhanced features (async)...");
    currentPhase_ = BootPhase::PHASE_ENHANCED;
    
    // Core0Taskでバックグラウンド実行される処理
    // （実際の実装はCore0Taskに委任）
    
    auto asyncTask = [this]() {
        auto phaseStart = std::chrono::steady_clock::now();
        
        // 画像アセット準備（遅延ロード）
        if (services_.stageImageAssets) {
            services_.stageImageAssets();
        }
        
        // 通信機能初期化
        if (services_.initializeCommunication) {
            services_.initializeCommunication();
        }
        
        // オープニングアニメーション（最後に実行）
        if (services_.playStartupAnimation) {
            services_.playStartupAnimation();
        }
        
        auto elapsed = std::chrono::steady_clock::now() - phaseStart;
        timing_.phase3Actual = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        
        backgroundComplete_ = true;
        
        Serial.printf("[FastBoot] Phase 3 complete: %lums (background)\n",
                      timing_.phase3Actual.count());
        
        if (callbacks_.onEnhancedPhaseComplete) {
            callbacks_.onEnhancedPhaseComplete();
        }
    };
    
    // TODO: Core0Taskのバックグラウンドキューに追加
    // 現在は同期実行（将来の並列化準備）
    asyncTask();
}

void FastBootOrchestrator::startBootTimer() {
    bootStartTime_ = std::chrono::steady_clock::now();
}

void FastBootOrchestrator::markPhaseComplete(BootPhase phase) {
    auto elapsed = std::chrono::steady_clock::now() - bootStartTime_;
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
    
    switch (phase) {
        case BootPhase::PHASE_CRITICAL:
            timing_.phase1Actual = elapsedMs;
            break;
        case BootPhase::PHASE_FUNCTIONAL:
            timing_.phase2Actual = elapsedMs;
            break;
        case BootPhase::PHASE_ENHANCED:
            timing_.phase3Actual = elapsedMs;
            break;
    }
}

float FastBootOrchestrator::getBootProgress() const {
    switch (currentPhase_) {
        case BootPhase::PHASE_CRITICAL:
            return 0.2f;
        case BootPhase::PHASE_FUNCTIONAL:
            return 0.6f;
        case BootPhase::PHASE_ENHANCED:
            return backgroundComplete_ ? 1.0f : 0.8f;
        default:
            return 0.0f;
    }
}