#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "led/LEDSphereManager.h"
#include "pattern/ProceduralPatternGenerator.h"

/**
 * @brief 重い処理中のProceduralPatternオーバーレイ表示
 * 
 * LittleFSフォーマット等の重い処理中（2-5秒）に
 * 別コアでProceduralPatternを表示してユーザー体験を向上
 */
class BootTimeProceduralOverlay {
public:
    enum class OverlayPattern {
        BOOT_PROGRESS,      // 起動進捗リング
        ROTATING_AXIS,      // 回転座標軸
        PULSING_SPHERE,     // パルス球体
        LOADING_SPIRAL      // ローディング螺旋
    };

    struct OverlayConfig {
        OverlayPattern pattern = OverlayPattern::BOOT_PROGRESS;
        float duration = 3.0f;           // 表示時間（秒）
        float brightness = 0.3f;         // 輝度（0.0-1.0）
        uint32_t updateIntervalMs = 33;  // 更新間隔（30fps対応）
        bool autoStop = true;            // 重い処理完了で自動停止
    };

    BootTimeProceduralOverlay(LEDSphere::LEDSphereManager& sphereManager);
    ~BootTimeProceduralOverlay();

    /**
     * @brief 重い処理開始と同時にオーバーレイ開始
     * @param pattern 表示パターン
     * @param expectedDurationMs 重い処理の予想時間
     * @return 開始成功フラグ
     */
    bool startOverlay(OverlayPattern pattern, uint32_t expectedDurationMs);

    /**
     * @brief オーバーレイ停止（重い処理完了時に呼び出し）
     */
    void stopOverlay();

    /**
     * @brief 進捗更新（0.0-1.0）
     */
    void updateProgress(float progress);

    /**
     * @brief オーバーレイ実行中フラグ
     */
    bool isActive() const { return taskHandle_ != nullptr; }

    /**
     * @brief デバッグ：パフォーマンス統計取得
     */
    struct PerformanceStats {
        uint32_t totalFrames = 0;
        uint32_t avgFrameTimeMs = 0;
        uint32_t maxFrameTimeMs = 0;
        float actualFps = 0.0f;
    };
    PerformanceStats getPerformanceStats() const;

private:
    static void overlayTaskEntry(void* param);
    void overlayTaskLoop();

    // パターン描画メソッド
    void renderBootProgress(float progress, uint32_t timeMs);
    void renderRotatingAxis(float progress, uint32_t timeMs);
    void renderPulsingSphere(float progress, uint32_t timeMs);
    void renderLoadingSpiral(float progress, uint32_t timeMs);

    LEDSphere::LEDSphereManager& sphereManager_;
    
    // FreeRTOS制御
    TaskHandle_t taskHandle_ = nullptr;
    SemaphoreHandle_t progressMutex_ = nullptr;
    volatile bool stopRequested_ = false;
    
    // オーバーレイ状態
    OverlayConfig config_;
    volatile float currentProgress_ = 0.0f;
    uint32_t startTimeMs_ = 0;
    uint32_t expectedEndTimeMs_ = 0;
    
    // パフォーマンス統計
    mutable PerformanceStats stats_;
};

/**
 * @brief 重い処理とProceduralPatternの並列実行ヘルパー
 */
class HeavyTaskWithOverlay {
public:
    using HeavyTaskFunction = std::function<bool()>;
    using ProgressCallback = std::function<void(float)>;

    struct TaskConfig {
        const char* taskName = "HeavyTask";
        uint32_t estimatedTimeMs = 3000;
        BootTimeProceduralOverlay::OverlayPattern overlayPattern = 
            BootTimeProceduralOverlay::OverlayPattern::BOOT_PROGRESS;
        bool showProgressOnLCD = true;
    };

    HeavyTaskWithOverlay(LEDSphere::LEDSphereManager& sphereManager);

    /**
     * @brief 重い処理を並列オーバーレイ付きで実行
     * @param task 実行する重い処理
     * @param progressCallback 進捗通知コールバック（オプション）
     * @param config タスク設定
     * @return 処理成功フラグ
     */
    bool executeWithOverlay(HeavyTaskFunction task,
                           const TaskConfig& config,
                           ProgressCallback progressCallback = nullptr);

    /**
     * @brief 直近の実行統計取得
     */
    struct ExecutionStats {
        uint32_t actualTaskTimeMs = 0;
        uint32_t overlayActiveTimeMs = 0;
        bool taskSuccess = false;
        bool overlaySuccess = false;
        float avgOverlayFps = 0.0f;
    };
    ExecutionStats getLastExecutionStats() const;

private:
    BootTimeProceduralOverlay overlay_;
    ExecutionStats lastStats_;
};