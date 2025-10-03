#include "boot/ProceduralOpeningSequence.h"
#include <algorithm>
#include <cmath>

#if defined(UNIT_TEST)
static inline unsigned long millis() { return 0; }
static inline void esp_task_wdt_reset() {}
#else
#include <Arduino.h>
#include <M5Unified.h>
#include <esp_task_wdt.h>
#endif

ProceduralOpeningSequence::ProceduralOpeningSequence(LEDSphere::LEDSphereManager& sphereManager)
    : sphereManager_(sphereManager) {
#if defined(UNIT_TEST)
    progressMutex_ = nullptr;
#else
    progressMutex_ = xSemaphoreCreateMutex();
#endif

    openingRingPattern_.setSphereManager(&sphereManager_);
}

ProceduralOpeningSequence::~ProceduralOpeningSequence() {
    stopSequence();
#if !defined(UNIT_TEST)
    if (progressMutex_) {
        vSemaphoreDelete(progressMutex_);
    }
#endif
}

#if defined(UNIT_TEST)
bool ProceduralOpeningSequence::startSequence(const SequenceConfig& config,
                                              const PhaseCallbacks& callbacks) {
    (void)config;
    (void)callbacks;
    return false;
}
#else
bool ProceduralOpeningSequence::startSequence(const SequenceConfig& config, 
                                             const PhaseCallbacks& callbacks) {
    if (taskHandle_) {
        Serial.println("[ProcOpening] Already running - stopping previous sequence");
        stopSequence();
    }

    config_ = config;
    callbacks_ = callbacks;
    sequenceStartMs_ = millis();
    currentPhase_ = SequencePhase::PHASE_BOOT_SPLASH;
    externalProgress_ = 0.0f;
    stopRequested_ = false;
    
    // 統計リセット
    stats_ = PerformanceStats{};

    // Core1でオープニングタスク開始（最高優先度）
    BaseType_t result = xTaskCreatePinnedToCore(
        sequenceTaskEntry,
        "ProcOpening",
        8192,               // 大容量スタック
        this,
        5,                  // 最高優先度（重要なオープニング体験）
        &taskHandle_,
        1                   // Core1固定
    );

    if (result != pdPASS) {
        Serial.println("[ProcOpening] Failed to create opening task");
        return false;
    }

    Serial.printf("[ProcOpening] 🎬 Started procedural opening (%.1fs target, %u fps)\n", 
                  config_.totalDuration, config_.targetFps);
    
    return true;
}
#endif

void ProceduralOpeningSequence::syncExternalProgress(float externalProgress) {
#if defined(UNIT_TEST)
    externalProgress_ = std::max(0.0f, std::min(1.0f, externalProgress));
#else
    if (xSemaphoreTake(progressMutex_, pdMS_TO_TICKS(1)) == pdTRUE) {
        externalProgress_ = constrain(externalProgress, 0.0f, 1.0f);
        xSemaphoreGive(progressMutex_);
    }
#endif
}

void ProceduralOpeningSequence::stopSequence() {
#if defined(UNIT_TEST)
    stopRequested_ = true;
    taskHandle_ = nullptr;
#else
    if (!taskHandle_) return;

    stopRequested_ = true;
    
    // タスク終了を最大1秒待機
    for (int i = 0; i < 100 && taskHandle_; ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    if (taskHandle_) {
        Serial.println("[ProcOpening] Force terminating sequence task");
        vTaskDelete(taskHandle_);
        taskHandle_ = nullptr;
    }

    Serial.printf("[ProcOpening] 🎬 Sequence stopped - %u frames, %.1f fps\n",
                  stats_.totalFrames, stats_.averageFps);
#endif
}

void ProceduralOpeningSequence::sequenceTaskEntry(void* param) {
#if !defined(UNIT_TEST)
    auto* sequence = static_cast<ProceduralOpeningSequence*>(param);
    sequence->sequenceTaskLoop();
#else
    (void)param;
#endif
}

void ProceduralOpeningSequence::sequenceTaskLoop() {
#if defined(UNIT_TEST)
    // Not executed in unit tests.
    taskHandle_ = nullptr;
#else
    uint32_t frameCount = 0;
    uint32_t totalFrameTime = 0;
    uint32_t maxFrameTime = 0;
    uint32_t targetFrameTime = 1000 / config_.targetFps;

    Serial.printf("[ProcOpening] 🚀 Task started on Core1 - %u fps target (%ums/frame)\n",
                  config_.targetFps, targetFrameTime);

    while (!stopRequested_) {
        uint32_t frameStart = millis();
        
        // 進捗計算（外部同期 vs 時間ベース）
        float totalProgress = 0.0f;
        if (config_.syncWithHeavyTask) {
            // 外部同期進捗を優先
            if (xSemaphoreTake(progressMutex_, pdMS_TO_TICKS(1)) == pdTRUE) {
                totalProgress = externalProgress_;
                xSemaphoreGive(progressMutex_);
            }
        } else {
            // 時間ベース進捗
            uint32_t elapsed = millis() - sequenceStartMs_;
            uint32_t targetDuration = config_.totalDuration * 1000;
            totalProgress = std::min(1.0f, static_cast<float>(elapsed) / targetDuration);
        }

        // フェーズ判定
        SequencePhase newPhase;
        if (totalProgress < 0.2f) {
            newPhase = SequencePhase::PHASE_BOOT_SPLASH;
        } else if (totalProgress < 0.4f) {
            newPhase = SequencePhase::PHASE_SYSTEM_CHECK;
        } else if (totalProgress < 0.6f) {
            newPhase = SequencePhase::PHASE_SPHERE_EMERGE;
        } else if (totalProgress < 0.8f) {
            newPhase = SequencePhase::PHASE_AXIS_CALIBRATE;
        } else {
            newPhase = SequencePhase::PHASE_READY_PULSE;
        }

        // フェーズ遷移チェック
        if (newPhase != currentPhase_) {
            if (callbacks_.onPhaseComplete) {
                callbacks_.onPhaseComplete(currentPhase_);
            }
            currentPhase_ = newPhase;
            if (callbacks_.onPhaseStart) {
                callbacks_.onPhaseStart(currentPhase_);
            }
        }

        // フェーズ内進捗計算
        float phaseStart = 0.0f, phaseEnd = 0.2f;
        switch (currentPhase_) {
            case SequencePhase::PHASE_BOOT_SPLASH: phaseStart = 0.0f; phaseEnd = 0.2f; break;
            case SequencePhase::PHASE_SYSTEM_CHECK: phaseStart = 0.2f; phaseEnd = 0.4f; break;
            case SequencePhase::PHASE_SPHERE_EMERGE: phaseStart = 0.4f; phaseEnd = 0.6f; break;
            case SequencePhase::PHASE_AXIS_CALIBRATE: phaseStart = 0.6f; phaseEnd = 0.8f; break;
            case SequencePhase::PHASE_READY_PULSE: phaseStart = 0.8f; phaseEnd = 1.0f; break;
        }
        
        float phaseProgress = (totalProgress - phaseStart) / (phaseEnd - phaseStart);
        phaseProgress = constrain(phaseProgress, 0.0f, 1.0f);

        // フェーズ描画
        uint32_t animationTime = millis() - sequenceStartMs_;
        switch (currentPhase_) {
            case SequencePhase::PHASE_BOOT_SPLASH:
                renderBootSplash(phaseProgress, animationTime);
                break;
            case SequencePhase::PHASE_READY_PULSE:
                renderReadyPulse(phaseProgress, animationTime);
                break;
            default:
                // 一時的にその他のフェーズ表示を停止
                sphereManager_.clearAllLEDs();
                break;
        }

        // LED更新
        sphereManager_.show();

        // LCD進捗更新
        if (config_.showLCDProgress) {
            updateLCDProgress(currentPhase_, totalProgress);
        }

        // 進捗コールバック
        if (callbacks_.onPhaseProgress) {
            callbacks_.onPhaseProgress(currentPhase_, phaseProgress);
        }

        // 完了チェック
        if (totalProgress >= 1.0f) {
            stats_.completedNormally = true;
            if (callbacks_.onSequenceComplete) {
                callbacks_.onSequenceComplete();
            }
            Serial.println("[ProcOpening] ✅ Sequence completed normally");
            break;
        }

        // パフォーマンス統計更新
        uint32_t frameTime = millis() - frameStart;
        frameCount++;
        totalFrameTime += frameTime;
        maxFrameTime = std::max(maxFrameTime, frameTime);

        // フレームレート制御
        if (frameTime < targetFrameTime) {
            vTaskDelay(pdMS_TO_TICKS(targetFrameTime - frameTime));
        }

        // WDTフィード
        esp_task_wdt_reset();
    }

    // 統計更新
    stats_.totalFrames = frameCount;
    stats_.sequenceDurationMs = millis() - sequenceStartMs_;
    stats_.maxFrameTimeMs = maxFrameTime;
    stats_.averageFps = frameCount > 0 ? 
        (frameCount * 1000.0f) / stats_.sequenceDurationMs : 0.0f;

    Serial.printf("[ProcOpening] 🎬 Task ended - %u frames, %.1fs duration\n", 
                  frameCount, stats_.sequenceDurationMs / 1000.0f);
    
    taskHandle_ = nullptr;
    vTaskDelete(nullptr);
#endif
}

void ProceduralOpeningSequence::renderBootSplash(float phaseProgress, uint32_t timeMs) {
    ProceduralPattern::PatternParams params;
    params.progress = std::max(0.0f, std::min(phaseProgress, 1.0f));
    params.time = timeMs / 1000.0f;
    params.brightness = config_.brightness;
    params.enableFlicker = false;
    params.speed = 1.0f;

    openingRingPattern_.setBrightness(config_.brightness);
    openingRingPattern_.setRingWidth(6);
    openingRingPattern_.render(params);
}

void ProceduralOpeningSequence::renderSystemCheck(float phaseProgress, uint32_t timeMs) {
    // Phase 2: システムチェック風（診断シーケンス）
    sphereManager_.clearAllLEDs();
    
    // 緯度線を順番にスキャン（北極→南極）
    float scanLatitude = -90.0f + (180.0f * phaseProgress);
    
    // メインスキャンライン
    CRGB scanColor = CRGB(0, 255 * config_.brightness, 100 * config_.brightness); // 診断グリーン
    sphereManager_.drawLatitudeLine(scanLatitude, scanColor, 3.0f);
    
    // スキャン軌跡（フェード）
    for (int trail = 1; trail <= 5; trail++) {
        float trailLat = scanLatitude - trail * 10.0f;
        if (trailLat >= -90.0f) {
            CRGB trailColor = scanColor;
            trailColor.fadeToBlackBy(trail * 50); // フェード効果
            sphereManager_.drawLatitudeLine(trailLat, trailColor, 1.0f);
        }
    }
    
    // チェック完了インジケーター（経度線）
    int completedSections = phaseProgress * 12; // 12セクション
    for (int i = 0; i < completedSections; i++) {
        float longitude = i * 30.0f;
        CRGB checkColor = CRGB(0, 200 * config_.brightness, 0); // 完了グリーン
        sphereManager_.drawLongitudeLine(longitude, checkColor, 1.0f);
    }
}

void ProceduralOpeningSequence::renderSphereEmerge(float phaseProgress, uint32_t timeMs) {
    // Phase 3: 球体出現（3D形成アニメーション）
    sphereManager_.clearAllLEDs();
    
    // 球体構築エフェクト（緯度・経度線の同時出現）
    int maxLatLines = 9; // -80° to +80°, 20°刻み
    int maxLonLines = 12; // 30°刻み
    
    int visibleLatLines = phaseProgress * maxLatLines;
    int visibleLonLines = phaseProgress * maxLonLines;
    
    // 緯度線描画
    for (int i = 0; i < visibleLatLines; i++) {
        float latitude = -80.0f + i * 20.0f;
        float intensity = 1.0f - (i * 0.1f); // 中心ほど明るく
        CRGB latColor = CRGB(
            100 * intensity * config_.brightness,
            100 * intensity * config_.brightness,
            255 * intensity * config_.brightness
        );
        sphereManager_.drawLatitudeLine(latitude, latColor, 2.0f);
    }
    
    // 経度線描画
    for (int i = 0; i < visibleLonLines; i++) {
        float longitude = i * 30.0f;
        CRGB lonColor = CRGB(
            255 * config_.brightness,
            100 * config_.brightness,
            100 * config_.brightness
        );
        sphereManager_.drawLongitudeLine(longitude, lonColor, 1.5f);
    }
    
    // 球体パルス（完成感）
    if (phaseProgress > 0.7f) {
        float pulseIntensity = sinf((phaseProgress - 0.7f) * 3.0f * M_PI) * 0.3f + 0.7f;
        // 全体にパルス効果を適用（実装は球体全体の輝度調整）
    }
}

void ProceduralOpeningSequence::renderAxisCalibrate(float phaseProgress, uint32_t timeMs) {
    // Phase 4: 座標軸較正（回転・整列）
    sphereManager_.clearAllLEDs();
    
    // 回転する3軸
    float calibrationRotation = phaseProgress * 360.0f; // 1回転で較正完了
    
    // X軸（赤）
    float xAngle = calibrationRotation;
    CRGB xColor = CRGB(255 * config_.brightness, 0, 0);
    sphereManager_.drawLongitudeLine(xAngle, xColor, 3.0f);
    
    // Y軸（緑）- 120度位相差
    float yAngle = calibrationRotation + 120.0f;
    CRGB yColor = CRGB(0, 255 * config_.brightness, 0);
    sphereManager_.drawLongitudeLine(yAngle, yColor, 3.0f);
    
    // Z軸（青）- 240度位相差
    float zAngle = calibrationRotation + 240.0f;
    CRGB zColor = CRGB(0, 0, 255 * config_.brightness);
    sphereManager_.drawLongitudeLine(zAngle, zColor, 3.0f);
    
    // 較正完了表示（赤道リング）
    if (phaseProgress > 0.8f) {
        float ringIntensity = (phaseProgress - 0.8f) / 0.2f; // 80-100%で出現
        CRGB equatorColor = CRGB(
            255 * ringIntensity * config_.brightness,
            255 * ringIntensity * config_.brightness,
            255 * ringIntensity * config_.brightness
        );
        sphereManager_.drawLatitudeLine(0.0f, equatorColor, 4.0f * ringIntensity);
    }
}

void ProceduralOpeningSequence::renderReadyPulse(float phaseProgress, uint32_t timeMs) {
    (void)phaseProgress;
    (void)timeMs;
    sphereManager_.clearAllLEDs();
    sphereManager_.drawAxisMarkers(10.0f, 5);
}

#if !defined(UNIT_TEST)
void ProceduralOpeningSequence::updateLCDProgress(SequencePhase phase, float progress) {
    static SequencePhase lastPhase = SequencePhase::PHASE_BOOT_SPLASH;
    static uint32_t lastUpdate = 0;
    
    // 100ms間隔で更新
    if (millis() - lastUpdate < 100) return;
    lastUpdate = millis();
    
    // フェーズ変更時は画面クリア
    if (phase != lastPhase) {
        M5.Display.fillScreen(TFT_BLACK);
        lastPhase = phase;
    }
    
    M5.Display.setTextColor(TFT_CYAN);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(0, 0);
    M5.Display.printf("Procedural Opening\n");
    
    // フェーズ名表示
    const char* phaseName = "Unknown";
    switch (phase) {
        case SequencePhase::PHASE_BOOT_SPLASH: phaseName = "Boot Splash"; break;
        case SequencePhase::PHASE_SYSTEM_CHECK: phaseName = "System Check"; break;
        case SequencePhase::PHASE_SPHERE_EMERGE: phaseName = "Sphere Emerge"; break;
        case SequencePhase::PHASE_AXIS_CALIBRATE: phaseName = "Axis Calibrate"; break;
        case SequencePhase::PHASE_READY_PULSE: phaseName = "Ready Pulse"; break;
    }
    
    M5.Display.printf("Phase: %s\n", phaseName);
    M5.Display.printf("Progress: %.0f%%\n", progress * 100.0f);
    
    // プログレスバー
    int barWidth = M5.Display.width() - 10;
    int barHeight = 6;
    int barY = 40;
    
    M5.Display.drawRect(5, barY, barWidth, barHeight, TFT_WHITE);
    M5.Display.fillRect(6, barY + 1, (progress * (barWidth - 2)), barHeight - 2, TFT_GREEN);
}
#endif

ProceduralOpeningSequence::PerformanceStats ProceduralOpeningSequence::getPerformanceStats() const {
    return stats_;
}

// SynchronizedBootSequence implementation
SynchronizedBootSequence::SynchronizedBootSequence(LEDSphere::LEDSphereManager& sphereManager)
    : openingSequence_(sphereManager) {}

bool SynchronizedBootSequence::executeBootWithOpening(HeavyTaskFunction heavyTask, 
                                                     const BootConfig& config) {
    Serial.printf("[SyncBoot] 🚀 Starting synchronized boot: %s (%.1fs)\n", 
                  config.taskName, config.estimatedDuration);
    
    uint32_t startTime = millis();
    
    // オープニングシーケンス設定
    ProceduralOpeningSequence::SequenceConfig openingConfig;
    openingConfig.totalDuration = config.estimatedDuration;
    openingConfig.syncWithHeavyTask = true;
    openingConfig.showLCDProgress = config.showDetailed;
    openingConfig.brightness = 0.8f;
    openingConfig.targetFps = 30;
    
    ProceduralOpeningSequence::PhaseCallbacks openingCallbacks;
    openingCallbacks.onPhaseStart = [](ProceduralOpeningSequence::SequencePhase phase) {
        Serial.printf("[SyncBoot] 🎬 Opening phase started: %d\n", static_cast<int>(phase));
    };
    
    // オープニング開始
    bool openingStarted = openingSequence_.startSequence(openingConfig, openingCallbacks);
    if (!openingStarted) {
        Serial.println("[SyncBoot] ❌ Failed to start opening sequence");
    }
    
    // 重い処理実行（進捗コールバック付き）
    auto progressCallback = [this](float progress) {
        openingSequence_.syncExternalProgress(progress);
    };
    
    bool taskSuccess = heavyTask(progressCallback);
    uint32_t taskTime = millis() - startTime;
    
    // オープニング停止
    if (openingStarted) {
        openingSequence_.stopSequence();
    }
    
    // 結果記録
    lastResult_.taskSuccess = taskSuccess;
    lastResult_.openingSuccess = openingStarted;
    lastResult_.totalTimeMs = taskTime;
    lastResult_.taskTimeMs = taskTime;
    lastResult_.timeTargetMet = (taskTime / 1000.0f) <= (config.estimatedDuration + 1.0f);
    
    if (openingStarted) {
        auto openingStats = openingSequence_.getPerformanceStats();
        lastResult_.openingFps = openingStats.averageFps;
    }
    
    Serial.printf("[SyncBoot] ✅ Synchronized boot complete: Task=%s, Opening=%s, Time=%ums\n",
                  taskSuccess ? "SUCCESS" : "FAILED",
                  openingStarted ? "SUCCESS" : "FAILED",
                  taskTime);
    
    return taskSuccess && openingStarted;
}

SynchronizedBootSequence::ExecutionResult SynchronizedBootSequence::getLastResult() const {
    return lastResult_;
}
#ifdef UNIT_TEST
void ProceduralOpeningSequence::renderPhaseForTest(SequencePhase phase,
                                                   float phaseProgress,
                                                   float animationTimeMs,
                                                   LEDSphere::LEDSphereManager& manager) {
    ProceduralOpeningSequence sequence(manager);
    sequence.config_.showLCDProgress = false;
    sequence.config_.brightness = 1.0f;
    sequence.sequenceStartMs_ = 0;
    sequence.stopRequested_ = false;

    uint32_t timeMs = static_cast<uint32_t>(animationTimeMs);

    switch (phase) {
        case SequencePhase::PHASE_BOOT_SPLASH:
            sequence.renderBootSplash(phaseProgress, timeMs);
            break;
        case SequencePhase::PHASE_SYSTEM_CHECK:
            sequence.renderSystemCheck(phaseProgress, timeMs);
            break;
        case SequencePhase::PHASE_SPHERE_EMERGE:
            sequence.renderSphereEmerge(phaseProgress, timeMs);
            break;
        case SequencePhase::PHASE_AXIS_CALIBRATE:
            sequence.renderAxisCalibrate(phaseProgress, timeMs);
            break;
        case SequencePhase::PHASE_READY_PULSE:
            sequence.renderReadyPulse(phaseProgress, timeMs);
            break;
    }

    manager.show();
}
#endif
