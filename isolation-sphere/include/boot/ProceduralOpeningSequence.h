#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "led/LEDSphereManager.h"
#include "pattern/ProceduralPatternGenerator.h"

/**
 * @brief ProceduralPatternによるオープニングアニメーション
 * 
 * JPEG連番アニメーションを完全置き換え
 * LittleFSフォーマット時間（2-5秒）と完全同期した美しいオープニング
 */
class ProceduralOpeningSequence {
public:
    enum class SequencePhase {
        PHASE_BOOT_SPLASH,     // 0-20%: 起動スプラッシュ（システムロゴ風）
        PHASE_SYSTEM_CHECK,    // 20-40%: システムチェック（診断風）  
        PHASE_SPHERE_EMERGE,   // 40-60%: 球体出現（3D形成アニメーション）
        PHASE_AXIS_CALIBRATE,  // 60-80%: 座標軸較正（回転・整列）
        PHASE_READY_PULSE      // 80-100%: 準備完了パルス（完了合図）
    };

    struct SequenceConfig {
        float totalDuration = 3.0f;        // 総時間（LittleFSフォーマットに合わせる）
        bool syncWithHeavyTask = true;      // 重い処理との同期
        bool showPhaseTransitions = true;   // フェーズ遷移エフェクト
        float brightness = 0.8f;           // 全体輝度
        uint32_t targetFps = 30;           // 目標フレームレート
        bool showLCDProgress = true;        // LCD進捗表示
    };

    struct PhaseCallbacks {
        std::function<void(SequencePhase)> onPhaseStart;
        std::function<void(SequencePhase, float)> onPhaseProgress;
        std::function<void(SequencePhase)> onPhaseComplete;
        std::function<void()> onSequenceComplete;
    };

    ProceduralOpeningSequence(LEDSphere::LEDSphereManager& sphereManager);
    ~ProceduralOpeningSequence();

    /**
     * @brief オープニングシーケンス開始
     * @param config シーケンス設定
     * @param callbacks フェーズコールバック
     * @return 開始成功フラグ
     */
    bool startSequence(const SequenceConfig& config, const PhaseCallbacks& callbacks);
    bool startSequence(const SequenceConfig& config);
    bool startSequence();

    /**
     * @brief 外部進捗同期（重い処理の進捗を反映）
     * @param externalProgress 外部処理進捗（0.0-1.0）
     */
    void syncExternalProgress(float externalProgress);

    /**
     * @brief シーケンス停止
     */
    void stopSequence();

    /**
     * @brief 現在フェーズ取得
     */
    SequencePhase getCurrentPhase() const { return currentPhase_; }

    /**
     * @brief 実行中フラグ
     */
    bool isRunning() const { return taskHandle_ != nullptr; }

    /**
     * @brief パフォーマンス統計
     */
    struct PerformanceStats {
        uint32_t totalFrames = 0;
        float averageFps = 0.0f;
        uint32_t maxFrameTimeMs = 0;
        uint32_t sequenceDurationMs = 0;
        bool completedNormally = false;
    };
    PerformanceStats getPerformanceStats() const;

private:
    static void sequenceTaskEntry(void* param);
    void sequenceTaskLoop();

    // フェーズ描画メソッド
    void renderBootSplash(float phaseProgress, uint32_t timeMs);
    void renderSystemCheck(float phaseProgress, uint32_t timeMs);
    void renderSphereEmerge(float phaseProgress, uint32_t timeMs);
    void renderAxisCalibrate(float phaseProgress, uint32_t timeMs);
    void renderReadyPulse(float phaseProgress, uint32_t timeMs);

    // LCD表示ヘルパー
    void updateLCDProgress(SequencePhase phase, float progress);

    LEDSphere::LEDSphereManager& sphereManager_;
    
    // FreeRTOS制御
    TaskHandle_t taskHandle_ = nullptr;
    SemaphoreHandle_t progressMutex_ = nullptr;
    volatile bool stopRequested_ = false;
    
    // シーケンス状態
    SequenceConfig config_;
    PhaseCallbacks callbacks_;
    SequencePhase currentPhase_ = SequencePhase::PHASE_BOOT_SPLASH;
    volatile float externalProgress_ = 0.0f;  // 外部同期進捗
    uint32_t sequenceStartMs_ = 0;
    
    // パフォーマンス統計
    mutable PerformanceStats stats_;
};

/**
 * @brief LittleFSフォーマット + ProceduralOpening統合実行
 * 
 * 重い処理とProceduralPatternオープニングを完全同期実行
 */
class SynchronizedBootSequence {
public:
    using HeavyTaskFunction = std::function<bool(std::function<void(float)>)>; // 進捗コールバック付き

    struct BootConfig {
        const char* taskName = "Boot Sequence";
        float estimatedDuration = 3.0f;    // 推定時間
        bool fallbackToFastMode = true;    // 高速モードフォールバック
        bool showDetailed = true;          // 詳細表示
    };

    SynchronizedBootSequence(LEDSphere::LEDSphereManager& sphereManager);

    /**
     * @brief 重い処理 + Proceduralオープニング同期実行
     * @param heavyTask 重い処理（進捗コールバック対応）
     * @param config ブート設定
     * @return 実行成功フラグ
     */
    bool executeBootWithOpening(HeavyTaskFunction heavyTask, const BootConfig& config);
    bool executeBootWithOpening(HeavyTaskFunction heavyTask);

    /**
     * @brief 直近の実行統計
     */
    struct ExecutionResult {
        bool taskSuccess = false;
        bool openingSuccess = false;
        uint32_t totalTimeMs = 0;
        uint32_t taskTimeMs = 0;
        float openingFps = 0.0f;
        bool timeTargetMet = false;
    };
    ExecutionResult getLastResult() const;

private:
    ProceduralOpeningSequence openingSequence_;
    ExecutionResult lastResult_;
};