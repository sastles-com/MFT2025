#include "boot/BootTimeProceduralOverlay.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <esp_task_wdt.h>
#include <cmath>

BootTimeProceduralOverlay::BootTimeProceduralOverlay(LEDSphere::LEDSphereManager& sphereManager)
    : sphereManager_(sphereManager) {
    progressMutex_ = xSemaphoreCreateMutex();
}

BootTimeProceduralOverlay::~BootTimeProceduralOverlay() {
    stopOverlay();
    if (progressMutex_) {
        vSemaphoreDelete(progressMutex_);
    }
}

bool BootTimeProceduralOverlay::startOverlay(OverlayPattern pattern, uint32_t expectedDurationMs) {
    if (taskHandle_) {
        Serial.println("[Overlay] Already running - stopping previous overlay");
        stopOverlay();
    }

    config_.pattern = pattern;
    config_.duration = expectedDurationMs / 1000.0f;
    startTimeMs_ = millis();
    expectedEndTimeMs_ = startTimeMs_ + expectedDurationMs;
    currentProgress_ = 0.0f;
    stopRequested_ = false;
    
    // パフォーマンス統計リセット
    stats_ = PerformanceStats{};

    // Core1でオーバーレイタスク開始（リアルタイム処理優先）
    BaseType_t result = xTaskCreatePinnedToCore(
        overlayTaskEntry,
        "BootOverlay",
        4096,               // スタックサイズ
        this,
        3,                  // 高優先度（Core1の他タスクより上）
        &taskHandle_,
        1                   // Core1固定
    );

    if (result != pdPASS) {
        Serial.println("[Overlay] Failed to create overlay task");
        return false;
    }

    Serial.printf("[Overlay] Started %s pattern for %ums on Core1\n", 
                  pattern == OverlayPattern::BOOT_PROGRESS ? "BOOT_PROGRESS" :
                  pattern == OverlayPattern::ROTATING_AXIS ? "ROTATING_AXIS" :
                  pattern == OverlayPattern::PULSING_SPHERE ? "PULSING_SPHERE" : "LOADING_SPIRAL",
                  expectedDurationMs);
    
    return true;
}

void BootTimeProceduralOverlay::stopOverlay() {
    if (!taskHandle_) return;

    stopRequested_ = true;
    
    // タスク終了を最大500ms待機
    for (int i = 0; i < 50 && taskHandle_; ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    if (taskHandle_) {
        Serial.println("[Overlay] Force terminating overlay task");
        vTaskDelete(taskHandle_);
        taskHandle_ = nullptr;
    }

    Serial.printf("[Overlay] Stopped - Stats: %u frames, %.1f fps\n",
                  stats_.totalFrames, stats_.actualFps);
}

void BootTimeProceduralOverlay::updateProgress(float progress) {
    if (xSemaphoreTake(progressMutex_, pdMS_TO_TICKS(1)) == pdTRUE) {
        currentProgress_ = constrain(progress, 0.0f, 1.0f);
        xSemaphoreGive(progressMutex_);
    }
}

void BootTimeProceduralOverlay::overlayTaskEntry(void* param) {
    auto* overlay = static_cast<BootTimeProceduralOverlay*>(param);
    overlay->overlayTaskLoop();
}

void BootTimeProceduralOverlay::overlayTaskLoop() {
    uint32_t frameCount = 0;
    uint32_t totalFrameTime = 0;
    uint32_t maxFrameTime = 0;
    uint32_t lastStatsUpdate = millis();

    Serial.println("[Overlay] Task started on Core1 - 30fps target");

    while (!stopRequested_) {
        uint32_t frameStart = millis();
        
        // 進捗とタイミング計算
        float progress = 0.0f;
        if (xSemaphoreTake(progressMutex_, pdMS_TO_TICKS(1)) == pdTRUE) {
            progress = currentProgress_;
            xSemaphoreGive(progressMutex_);
        }

        // 自動進捗計算（時間ベース）
        if (config_.autoStop) {
            uint32_t elapsed = millis() - startTimeMs_;
            uint32_t expected = expectedEndTimeMs_ - startTimeMs_;
            if (expected > 0) {
                float timeProgress = static_cast<float>(elapsed) / expected;
                progress = std::max(progress, timeProgress);
            }
        }

        // 完了チェック
        if (progress >= 1.0f && config_.autoStop) {
            Serial.println("[Overlay] Auto-stopping on completion");
            break;
        }

        // パターン描画
        uint32_t animationTime = millis() - startTimeMs_;
        switch (config_.pattern) {
            case OverlayPattern::BOOT_PROGRESS:
                renderBootProgress(progress, animationTime);
                break;
            case OverlayPattern::ROTATING_AXIS:
                renderRotatingAxis(progress, animationTime);
                break;
            case OverlayPattern::PULSING_SPHERE:
                renderPulsingSphere(progress, animationTime);
                break;
            case OverlayPattern::LOADING_SPIRAL:
                renderLoadingSpiral(progress, animationTime);
                break;
        }

        // LED更新
        sphereManager_.show();
        
        // パフォーマンス統計更新
        uint32_t frameTime = millis() - frameStart;
        frameCount++;
        totalFrameTime += frameTime;
        maxFrameTime = std::max(maxFrameTime, frameTime);

        // 統計更新（1秒間隔）
        if (millis() - lastStatsUpdate >= 1000) {
            stats_.totalFrames = frameCount;
            stats_.avgFrameTimeMs = frameCount > 0 ? totalFrameTime / frameCount : 0;
            stats_.maxFrameTimeMs = maxFrameTime;
            stats_.actualFps = frameCount > 0 ? 1000.0f / stats_.avgFrameTimeMs : 0.0f;
            lastStatsUpdate = millis();
        }

        // フレームレート制御（30fps = 33ms間隔）
        uint32_t targetFrameTime = config_.updateIntervalMs;
        if (frameTime < targetFrameTime) {
            vTaskDelay(pdMS_TO_TICKS(targetFrameTime - frameTime));
        }

        // WDTフィード
        esp_task_wdt_reset();
    }

    Serial.printf("[Overlay] Task ended - %u frames in %ums\n", 
                  frameCount, millis() - startTimeMs_);
    taskHandle_ = nullptr;
    vTaskDelete(nullptr);
}

void BootTimeProceduralOverlay::renderBootProgress(float progress, uint32_t timeMs) {
    // 進捗リングパターン（緯度線ベース）
    sphereManager_.clearAllLEDs();
    
    // 進捗に応じた緯度での光の輪
    float progressLatitude = -90.0f + (180.0f * progress); // 南極→北極
    
    // メインリング
    CRGB progressColor = CRGB(0, 255 * progress, 255 * (1.0f - progress)); // 青→緑
    sphereManager_.drawLatitudeLine(progressLatitude, progressColor, 3.0f);
    
    // 回転する装飾リング
    float decorRotation = (timeMs * 0.1f); // 1回転/10秒
    for (int i = 0; i < 3; i++) {
        float decorLatitude = progressLatitude + 15.0f * sinf((decorRotation + i * 120.0f) * M_PI / 180.0f);
        CRGB decorColor = CRGB(100, 100, 100);
        sphereManager_.drawLatitudeLine(decorLatitude, decorColor, 1.0f);
    }
}

void BootTimeProceduralOverlay::renderRotatingAxis(float progress, uint32_t timeMs) {
    // 回転座標軸パターン
    sphereManager_.clearAllLEDs();
    
    float rotation = timeMs * 0.36f; // 1回転/秒
    
    // X軸（赤）- 回転
    float xLong = rotation;
    sphereManager_.drawLongitudeLine(xLong, CRGB(255 * config_.brightness, 0, 0), 2.0f);
    
    // Y軸（緑）- 120度ずらして回転  
    float yLong = rotation + 120.0f;
    sphereManager_.drawLongitudeLine(yLong, CRGB(0, 255 * config_.brightness, 0), 2.0f);
    
    // Z軸（青）- 240度ずらして回転
    float zLong = rotation + 240.0f;  
    sphereManager_.drawLongitudeLine(zLong, CRGB(0, 0, 255 * config_.brightness), 2.0f);
    
    // 進捗インジケーター（赤道リング）
    CRGB progressColor = CRGB(255 * progress * config_.brightness, 255 * progress * config_.brightness, 0);
    sphereManager_.drawLatitudeLine(0.0f, progressColor, progress * 5.0f);
}

void BootTimeProceduralOverlay::renderPulsingSphere(float progress, uint32_t timeMs) {
    // パルス球体パターン
    sphereManager_.clearAllLEDs();
    
    float pulse = sinf(timeMs * 0.01f) * 0.5f + 0.5f; // 0-1のパルス
    float brightness = pulse * config_.brightness * (0.5f + 0.5f * progress);
    
    // 全体パルス
    CRGB pulseColor = CRGB(
        255 * brightness * (1.0f - progress),  // 赤は減衰
        255 * brightness * progress,           // 緑は増加
        255 * brightness * 0.5f               // 青は一定
    );
    
    // 複数の緯度リングでパルス効果
    for (int lat = -60; lat <= 60; lat += 30) {
        float phaseShift = lat * 0.1f;
        float ringPulse = sinf((timeMs + phaseShift) * 0.01f) * 0.5f + 0.5f;
        CRGB ringColor = pulseColor;
        ringColor.fadeToBlackBy(255 * (1.0f - ringPulse));
        sphereManager_.drawLatitudeLine(lat, ringColor, 2.0f);
    }
}

void BootTimeProceduralOverlay::renderLoadingSpiral(float progress, uint32_t timeMs) {
    // ローディング螺旋パターン
    sphereManager_.clearAllLEDs();
    
    float spiralRotation = timeMs * 0.36f; // 回転速度
    int numSpirals = 3;
    
    for (int spiral = 0; spiral < numSpirals; spiral++) {
        float spiralOffset = spiral * (360.0f / numSpirals);
        CRGB spiralColor;
        
        // 螺旋毎に色分け
        switch (spiral) {
            case 0: spiralColor = CRGB(255 * config_.brightness, 0, 0); break;
            case 1: spiralColor = CRGB(0, 255 * config_.brightness, 0); break; 
            case 2: spiralColor = CRGB(0, 0, 255 * config_.brightness); break;
        }
        
        // 進捗に応じて螺旋の長さを調整
        float maxLatitude = -90.0f + (180.0f * progress);
        
        // 螺旋を緯度線の組み合わせで近似描画
        for (float lat = -90.0f; lat <= maxLatitude; lat += 10.0f) {
            float longitude = spiralRotation + spiralOffset + (lat + 90.0f) * 2.0f; // 螺旋計算
            
            // 輝度をfade効果
            float fadeFactor = (lat + 90.0f) / 180.0f;
            CRGB fadedColor = spiralColor;
            fadedColor.fadeToBlackBy(255 * (1.0f - fadeFactor));
            
            sphereManager_.drawLongitudeLine(longitude, fadedColor, 1.0f);
        }
    }
}

BootTimeProceduralOverlay::PerformanceStats BootTimeProceduralOverlay::getPerformanceStats() const {
    return stats_;
}

// HeavyTaskWithOverlay implementation
HeavyTaskWithOverlay::HeavyTaskWithOverlay(LEDSphere::LEDSphereManager& sphereManager)
    : overlay_(sphereManager) {}

bool HeavyTaskWithOverlay::executeWithOverlay(HeavyTaskFunction task,
                                             const TaskConfig& config,
                                             ProgressCallback progressCallback) {
    Serial.printf("[HeavyTask] Starting '%s' with overlay (est. %ums)\n", 
                  config.taskName, config.estimatedTimeMs);
    
    uint32_t taskStart = millis();
    
    // オーバーレイ開始
    bool overlayStarted = overlay_.startOverlay(config.overlayPattern, config.estimatedTimeMs);
    if (!overlayStarted) {
        Serial.println("[HeavyTask] Warning: Failed to start overlay");
    }
    
    // LCD進捗表示
    if (config.showProgressOnLCD) {
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextColor(TFT_CYAN);
        M5.Display.setTextSize(1);
        M5.Display.setCursor(0, 0);
        M5.Display.printf("Processing...\n%s\n", config.taskName);
        M5.Display.printf("Est: %.1fs", config.estimatedTimeMs / 1000.0f);
    }
    
    // 重い処理を実行（進捗コールバック付き）
    bool taskSuccess = false;
    if (progressCallback) {
        // 進捗通知対応タスクの場合
        auto wrappedCallback = [&](float progress) {
            overlay_.updateProgress(progress);
            progressCallback(progress);
            
            if (config.showProgressOnLCD) {
                M5.Display.setCursor(0, 30);
                M5.Display.printf("Progress: %3.0f%%", progress * 100.0f);
            }
        };
        
        // 注意：この実装例では単純化。実際は進捗付きタスク実行機構が必要
        taskSuccess = task();
        wrappedCallback(1.0f); // 完了通知
    } else {
        // 進捗通知無しタスクの場合
        taskSuccess = task();
    }
    
    uint32_t taskTime = millis() - taskStart;
    
    // オーバーレイ停止
    if (overlayStarted) {
        overlay_.stopOverlay();
    }
    
    // 統計記録
    lastStats_.actualTaskTimeMs = taskTime;
    lastStats_.taskSuccess = taskSuccess;
    lastStats_.overlaySuccess = overlayStarted;
    if (overlayStarted) {
        auto overlayStats = overlay_.getPerformanceStats();
        lastStats_.avgOverlayFps = overlayStats.actualFps;
    }
    
    Serial.printf("[HeavyTask] Completed '%s' in %ums (success: %s)\n",
                  config.taskName, taskTime, taskSuccess ? "true" : "false");
    
    return taskSuccess;
}

HeavyTaskWithOverlay::ExecutionStats HeavyTaskWithOverlay::getLastExecutionStats() const {
    return lastStats_;
}