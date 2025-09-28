#pragma once

#include <functional>
#include <chrono>
#include "config/ConfigManager.h"
#include "core/SharedState.h"
#include "storage/StorageManager.h"

/**
 * @brief 高速起動オーケストレーター
 * 
 * パフォーマンステストで確認された30fps能力を活用し、
 * 段階的初期化により3秒以内の基本機能利用可能を実現
 */
class FastBootOrchestrator {
public:
    enum class BootPhase {
        PHASE_CRITICAL,    // 0-1秒：基本ハードウェア + 最小限config
        PHASE_FUNCTIONAL,  // 1-3秒：LED/IMU + ProceduralPattern
        PHASE_ENHANCED     // 3-5秒：画像アセット + 通信機能
    };

    struct PhaseCallbacks {
        std::function<bool()> onCriticalPhaseComplete;
        std::function<bool()> onFunctionalPhaseComplete; 
        std::function<bool()> onEnhancedPhaseComplete;
    };

    struct BootServices {
        // Phase 1: Critical services
        std::function<bool()> initializeHardware;
        std::function<bool()> loadMinimalConfig;
        
        // Phase 2: Functional services
        std::function<bool()> initializeLEDSystem;
        std::function<bool()> initializeIMU;
        std::function<bool()> startProceduralPatterns;
        
        // Phase 3: Enhanced services (background)
        std::function<bool()> stageImageAssets;
        std::function<bool()> initializeCommunication;
        std::function<void()> playStartupAnimation;
    };

    struct BootTiming {
        std::chrono::milliseconds phase1Target{1000};  // 1秒以内
        std::chrono::milliseconds phase2Target{3000};  // 3秒以内
        std::chrono::milliseconds phase3Target{5000};  // 5秒以内
        
        // 実測値
        std::chrono::milliseconds phase1Actual{0};
        std::chrono::milliseconds phase2Actual{0};
        std::chrono::milliseconds phase3Actual{0};
        
        bool phase1OnTime() const { return phase1Actual <= phase1Target; }
        bool phase2OnTime() const { return phase2Actual <= phase2Target; }
        bool phase3OnTime() const { return phase3Actual <= phase3Target; }
    };

    FastBootOrchestrator(StorageManager& storage,
                        ConfigManager& config,
                        SharedState& shared,
                        PhaseCallbacks callbacks = {},
                        BootServices services = {});

    /**
     * @brief 高速起動シーケンス実行
     * @return 段階的な起動成功状況
     */
    bool runFastBoot();

    /**
     * @brief 現在の起動フェーズ取得
     */
    BootPhase getCurrentPhase() const { return currentPhase_; }

    /**
     * @brief 起動タイミング統計取得
     */
    const BootTiming& getBootTiming() const { return timing_; }

    /**
     * @brief バックグラウンド処理完了チェック
     */
    bool isBackgroundProcessingComplete() const { return backgroundComplete_; }

    /**
     * @brief 起動進捗率取得（0.0-1.0）
     */
    float getBootProgress() const;

private:
    bool executeCriticalPhase();
    bool executeFunctionalPhase();
    void executeEnhancedPhaseAsync(); // バックグラウンド実行

    void startBootTimer();
    void markPhaseComplete(BootPhase phase);

    StorageManager& storage_;
    ConfigManager& config_;
    SharedState& shared_;
    PhaseCallbacks callbacks_;
    BootServices services_;
    
    BootPhase currentPhase_ = BootPhase::PHASE_CRITICAL;
    BootTiming timing_;
    std::chrono::steady_clock::time_point bootStartTime_;
    bool backgroundComplete_ = false;
};