#pragma once

#include "core/CoreTask.h"
#include "config/ConfigManager.h"
#include "storage/StorageManager.h"
#include "core/SharedState.h"
#include "led/LEDSphereManager.h"
#include "pattern/ProceduralPatternGenerator.h"

/**
 * @brief 高速起動専用Core0Task
 * 
 * 従来の重い処理をバックグラウンド化し、
 * クリティカルパスを短縮する並列初期化を実装
 */
class FastBootCore0Task : public CoreTask {
public:
    enum class InitPhase {
        STORAGE_CRITICAL,  // 最小限config読み込み
        STORAGE_FULL,      // LittleFS完全初期化
        ASSETS_STAGING,    // 画像アセット準備
        COMMUNICATION_INIT // MQTT/UDP初期化
    };

    FastBootCore0Task(const TaskConfig& config,
                     ConfigManager& configManager,
                     StorageManager& storageManager,
                     SharedState& sharedState);

    /**
     * @brief 段階的初期化要求
     */
    void requestPhase(InitPhase phase);

    /**
     * @brief フェーズ完了チェック
     */
    bool isPhaseComplete(InitPhase phase) const;

    /**
     * @brief バックグラウンド処理進捗
     */
    float getBackgroundProgress() const;

protected:
    void setup() override;
    void loop() override;

private:
    void executeStorageCritical();
    void executeStorageFull();
    void executeAssetsStaging();
    void executeCommunicationInit();

    ConfigManager& configManager_;
    StorageManager& storageManager_;
    SharedState& sharedState_;
    
    InitPhase currentPhase_ = InitPhase::STORAGE_CRITICAL;
    bool phaseComplete_[4] = {false, false, false, false};
    uint32_t phaseStartMs_ = 0;
    
    // バックグラウンド処理状態
    float assetStagingProgress_ = 0.0f;
    bool littleFsFormatRequired_ = true;
};

/**
 * @brief 高速起動専用Core1Task
 * 
 * LED/IMUシステムの並列初期化と
 * ProceduralPatternの即座開始を担当
 */
class FastBootCore1Task : public CoreTask {
public:
    enum class InitPhase {
        HARDWARE_BASIC,    // 基本LED/IMU初期化
        PATTERNS_READY,    // ProceduralPattern開始
        VISUAL_FEEDBACK,   // 起動進捗LED表示
        FULL_OPERATION     // フル機能動作
    };

    FastBootCore1Task(const TaskConfig& config,
                     SharedState& sharedState,
                     LEDSphere::LEDSphereManager& sphereManager);

    /**
     * @brief 段階的初期化要求
     */
    void requestPhase(InitPhase phase);

    /**
     * @brief ProceduralPattern即座開始
     */
    void startImmediatePatterns();

    /**
     * @brief 起動進捗LED表示
     */
    void displayBootProgress(float progress);

protected:
    void setup() override;
    void loop() override;

private:
    void executeHardwareBasic();
    void executePatternsReady();
    void executeVisualFeedback();
    void executeFullOperation();

    SharedState& sharedState_;
    LEDSphere::LEDSphereManager& sphereManager_;
    
    InitPhase currentPhase_ = InitPhase::HARDWARE_BASIC;
    bool phaseComplete_[4] = {false, false, false, false};
    
    // ProceduralPattern管理
    std::unique_ptr<ProceduralPatternGenerator> patternGenerator_;
    uint32_t lastPatternUpdateMs_ = 0;
    float bootProgressCache_ = 0.0f;
};