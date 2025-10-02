/**
 * @file LEDSphereManager.cpp
 * @brief LED球体統合制御システム実装
 */

#include "led/LEDSphereManager.h"
#include <Arduino.h>

namespace LEDSphere {

// ========== LEDSphereManager実装 ==========

LEDSphereManager::LEDSphereManager() 
    : initialized_(false), 
      sparseMode_(true), 
      targetFPS_(30) {
    
    // 姿勢パラメータ初期化
    lastPosture_.quaternionW = 1.0f;
    lastPosture_.quaternionX = 0.0f;
    lastPosture_.quaternionY = 0.0f;
    lastPosture_.quaternionZ = 0.0f;
    lastPosture_.latitudeOffset = 0.0f;
    lastPosture_.longitudeOffset = 0.0f;
    
    Serial.println("[LEDSphereManager] Constructor called");
}

LEDSphereManager::~LEDSphereManager() {
    Serial.println("[LEDSphereManager] Destructor called");
}

// ========== 初期化・設定 ==========

bool LEDSphereManager::initialize(const char* csvPath) {
    if (initialized_) {
        Serial.println("[LEDSphereManager] Already initialized");
        return true;
    }

    Serial.printf("[LEDSphereManager] Initializing with CSV: %s\n", csvPath);
    
    // TODO: 実際の初期化処理
    // - LEDLayoutManager初期化
    // - SphereCoordinateTransform初期化
    // - FastLEDController初期化
    // - UVCoordinateCache初期化
    // - PerformanceMonitor初期化
    
    initialized_ = true;
    Serial.println("[LEDSphereManager] Initialization completed");
    return true;
}

bool LEDSphereManager::initializeLedHardware(uint8_t numStrips, const std::vector<uint16_t>& ledsPerStrip, const std::vector<uint8_t>& stripGpios) {
    Serial.printf("[LEDSphereManager] Initializing LED hardware: strips=%d\n", numStrips);
    // Compute total LEDs
    size_t total = 0;
    for (size_t i=0;i<ledsPerStrip.size();++i) total += ledsPerStrip[i];
    if (total == 0) {
        Serial.println("[LEDSphereManager] No LEDs configured");
        return false;
    }

    // Create or reallocate manager framebuffer
    if (frameBuffer_) {
        free(frameBuffer_);
        frameBuffer_ = nullptr;
        totalLeds_ = 0;
    }
    frameBuffer_ = (CRGB*)malloc(sizeof(CRGB) * total);
    if (!frameBuffer_) {
        Serial.println("[LEDSphereManager] Failed to allocate framebuffer");
        return false;
    }
    memset(frameBuffer_, 0, sizeof(CRGB) * total);
    totalLeds_ = total;

    // Register each strip with FastLED using offsets into the single framebuffer
    size_t offset = 0;
    for (size_t s = 0; s < ledsPerStrip.size() && s < stripGpios.size(); ++s) {
        uint8_t pin = stripGpios[s];
        uint16_t count = ledsPerStrip[s];
        Serial.printf("[LEDSphereManager] Registering strip %d: pin=%d count=%d offset=%u\n", (int)s, (int)pin, (int)count, (unsigned)offset);
        // Use compile-time template instantiation for common pins (0..16). If pin is out of range, fall back to warning.
        switch (pin) {
            case 0: FastLED.addLeds<WS2812, 0, GRB>(&frameBuffer_[offset], count); break;
            case 1: FastLED.addLeds<WS2812, 1, GRB>(&frameBuffer_[offset], count); break;
            case 2: FastLED.addLeds<WS2812, 2, GRB>(&frameBuffer_[offset], count); break;
            case 3: FastLED.addLeds<WS2812, 3, GRB>(&frameBuffer_[offset], count); break;
            case 4: FastLED.addLeds<WS2812, 4, GRB>(&frameBuffer_[offset], count); break;
            case 5: FastLED.addLeds<WS2812, 5, GRB>(&frameBuffer_[offset], count); break;
            case 6: FastLED.addLeds<WS2812, 6, GRB>(&frameBuffer_[offset], count); break;
            case 7: FastLED.addLeds<WS2812, 7, GRB>(&frameBuffer_[offset], count); break;
            case 8: FastLED.addLeds<WS2812, 8, GRB>(&frameBuffer_[offset], count); break;
            case 9: FastLED.addLeds<WS2812, 9, GRB>(&frameBuffer_[offset], count); break;
            case 10: FastLED.addLeds<WS2812, 10, GRB>(&frameBuffer_[offset], count); break;
            case 11: FastLED.addLeds<WS2812, 11, GRB>(&frameBuffer_[offset], count); break;
            case 12: FastLED.addLeds<WS2812, 12, GRB>(&frameBuffer_[offset], count); break;
            case 13: FastLED.addLeds<WS2812, 13, GRB>(&frameBuffer_[offset], count); break;
            case 14: FastLED.addLeds<WS2812, 14, GRB>(&frameBuffer_[offset], count); break;
            case 15: FastLED.addLeds<WS2812, 15, GRB>(&frameBuffer_[offset], count); break;
            case 16: FastLED.addLeds<WS2812, 16, GRB>(&frameBuffer_[offset], count); break;
            default:
                Serial.printf("[LEDSphereManager] Unsupported GPIO pin for templated addLeds: %d. Skipping this strip.\n", (int)pin);
                break;
        }
        offset += count;
    }

    Serial.printf("[LEDSphereManager] LED hardware initialized, total LEDs=%u\n", (unsigned)totalLeds_);
    return true;
}

// ========== 姿勢・座標制御 ==========

void LEDSphereManager::setIMUPosture(float qw, float qx, float qy, float qz) {
    lastPosture_.quaternionW = qw;
    lastPosture_.quaternionX = qx;
    lastPosture_.quaternionY = qy;
    lastPosture_.quaternionZ = qz;
    Serial.printf("[LEDSphereManager] IMU Posture set: (%.3f, %.3f, %.3f, %.3f)\n", qw, qx, qy, qz);
}

void LEDSphereManager::setUIOffset(float latOffset, float lonOffset) {
    lastPosture_.latitudeOffset = latOffset;
    lastPosture_.longitudeOffset = lonOffset;
    Serial.printf("[LEDSphereManager] UI Offset set: (lat=%.1f, lon=%.1f)\n", latOffset, lonOffset);
}

void LEDSphereManager::setPostureParams(const PostureParams& params) {
    lastPosture_ = params;
    Serial.println("[LEDSphereManager] Posture params set");
}

// ========== LED制御 ==========

void LEDSphereManager::setLED(uint16_t faceID, CRGB color) {
    if (faceID >= LED_COUNT) {
        Serial.printf("[LEDSphereManager] Invalid faceID: %d\n", faceID);
        return;
    }
    
    // TODO: 実際のLED設定処理
    Serial.printf("[LEDSphereManager] LED %d set to RGB(%d,%d,%d)\n", 
                  faceID, color.r, color.g, color.b);
}

void LEDSphereManager::setLEDByUV(float u, float v, CRGB color, float radius) {
    Serial.printf("[LEDSphereManager] UV LED set: (%.3f, %.3f) RGB(%d,%d,%d) r=%.3f\n", 
                  u, v, color.r, color.g, color.b, radius);
    
    // TODO: UV座標からLED IDを検索して設定
}

void LEDSphereManager::clearAllLEDs() {
    Serial.println("[LEDSphereManager] Clearing all LEDs");
    
    // TODO: 実際の全LED消去処理
}

void LEDSphereManager::setBrightness(uint8_t brightness) {
    Serial.printf("[LEDSphereManager] Brightness set to %d\n", brightness);
    
    // TODO: FastLED輝度設定
}

void LEDSphereManager::show() {
    Serial.println("[LEDSphereManager] Showing LEDs");
    
    // TODO: FastLED.show()実行
}

// ========== 高速パターン描画 ==========

void LEDSphereManager::drawCoordinateAxis(bool showGrid, float brightness) {
    Serial.printf("[LEDSphereManager] Drawing coordinate axis (grid=%s, brightness=%.2f)\n", 
                  showGrid ? "true" : "false", brightness);
    
    // TODO: 座標軸描画
}

void LEDSphereManager::drawLatitudeLine(float latitude, CRGB color, uint8_t lineWidth) {
    Serial.printf("[LEDSphereManager] Drawing latitude line: %.1f° RGB(%d,%d,%d) width=%d\n", 
                  latitude, color.r, color.g, color.b, lineWidth);
    
    // TODO: 緯度線描画
}

void LEDSphereManager::drawLongitudeLine(float longitude, CRGB color, uint8_t lineWidth) {
    Serial.printf("[LEDSphereManager] Drawing longitude line: %.1f° RGB(%d,%d,%d) width=%d\n", 
                  longitude, color.r, color.g, color.b, lineWidth);
    
    // TODO: 経度線描画
}

void LEDSphereManager::drawSparsePattern(const std::map<uint16_t, CRGB>& points) {
    Serial.printf("[LEDSphereManager] Drawing sparse pattern: %zu points\n", points.size());
    
    // TODO: スパースパターン描画
}

// ========== 検索・クエリ機能 ==========

uint16_t LEDSphereManager::findClosestLED(float u, float v) const {
    Serial.printf("[LEDSphereManager] Finding closest LED for UV(%.3f, %.3f)\n", u, v);
    
    // TODO: 実際の最寄りLED検索
    return LED_COUNT; // 見つからない場合
}

std::vector<uint16_t> LEDSphereManager::findLEDsInRange(float u, float v, float radius) const {
    Serial.printf("[LEDSphereManager] Finding LEDs in range UV(%.3f, %.3f) r=%.3f\n", u, v, radius);
    
    // TODO: 範囲内LED検索
    return std::vector<uint16_t>();
}

UVCoordinate LEDSphereManager::transformToUV(float x, float y, float z) const {
    Serial.printf("[LEDSphereManager] Transforming 3D(%.3f, %.3f, %.3f) to UV\n", x, y, z);
    
    // TODO: 実際の座標変換
    return UVCoordinate();
}

const LEDPosition* LEDSphereManager::getLEDPosition(uint16_t faceID) const {
    if (faceID >= LED_COUNT) {
        Serial.printf("[LEDSphereManager] Invalid faceID for position query: %d\n", faceID);
        return nullptr;
    }
    
    // TODO: 実際のLED位置データ取得
    return nullptr;
}

// ========== パフォーマンス監視 ==========

void LEDSphereManager::frameStart() {
    // TODO: フレーム開始時刻記録
    Serial.println("[LEDSphereManager] Frame start");
}

void LEDSphereManager::frameEnd() {
    // TODO: フレーム終了時刻記録とFPS計算
    Serial.println("[LEDSphereManager] Frame end");
}

PerformanceStats LEDSphereManager::getPerformanceStats() const {
    PerformanceStats stats;
    
    // TODO: 実際の統計情報取得
    stats.currentFPS = 30.0f;
    stats.averageRenderTime = 33.3f;
    stats.frameCount = 0;
    stats.activeLEDCount = 0;
    stats.memoryUsage = sizeof(*this);
    
    return stats;
}

float LEDSphereManager::getCurrentFPS() const {
    // TODO: 実際のFPS計算
    return 30.0f;
}

uint16_t LEDSphereManager::getActiveLEDCount() const {
    // TODO: アクティブLED数カウント
    return 0;
}

// ========== デバッグ・ユーティリティ ==========

void LEDSphereManager::printSystemStatus() const {
    Serial.println("=== LEDSphereManager System Status ===");
    Serial.printf("Initialized: %s\n", initialized_ ? "Yes" : "No");
    Serial.printf("Sparse Mode: %s\n", sparseMode_ ? "On" : "Off");
    Serial.printf("Target FPS: %d\n", targetFPS_);
    
    PerformanceStats stats = getPerformanceStats();
    Serial.printf("Current FPS: %.2f\n", stats.currentFPS);
    Serial.printf("Active LEDs: %d\n", stats.activeLEDCount);
    Serial.printf("Memory Usage: %zu bytes\n", stats.memoryUsage);
    
    Serial.println("Current Posture:");
    Serial.printf("  Quaternion: (%.3f, %.3f, %.3f, %.3f)\n", 
                 lastPosture_.quaternionW, lastPosture_.quaternionX,
                 lastPosture_.quaternionY, lastPosture_.quaternionZ);
    Serial.printf("  Offset: (lat=%.1f°, lon=%.1f°)\n", 
                 lastPosture_.latitudeOffset, lastPosture_.longitudeOffset);
    Serial.println("=====================================");
}

void LEDSphereManager::printMemoryUsage() const {
    PerformanceStats stats = getPerformanceStats();
    Serial.printf("[LEDSphereManager] Memory Usage: %zu bytes\n", stats.memoryUsage);
}

void LEDSphereManager::printLEDLayout(size_t maxCount) const {
    Serial.printf("[LEDSphereManager] LED Layout (showing max %zu LEDs):\n", maxCount);
    
    for (size_t i = 0; i < maxCount && i < LED_COUNT; ++i) {
        // TODO: 実際のLED位置データ出力
        Serial.printf("  LED %zu: position data not loaded\n", i);
    }
}

// ========== プライベートメソッド ==========

bool LEDSphereManager::initializeFastLED() {
    Serial.println("[LEDSphereManager] Initializing FastLED");
    
    // TODO: FastLED初期化
    return true;
}

bool LEDSphereManager::initializeComponents() {
    Serial.println("[LEDSphereManager] Initializing components");
    
    // TODO: 各コンポーネント初期化
    return true;
}

bool LEDSphereManager::hasPostureChanged(const PostureParams& params) const {
    const float epsilon = 0.001f;
    
    return (abs(params.quaternionW - lastPosture_.quaternionW) > epsilon) ||
           (abs(params.quaternionX - lastPosture_.quaternionX) > epsilon) ||
           (abs(params.quaternionY - lastPosture_.quaternionY) > epsilon) ||
           (abs(params.quaternionZ - lastPosture_.quaternionZ) > epsilon) ||
           (abs(params.latitudeOffset - lastPosture_.latitudeOffset) > epsilon) ||
           (abs(params.longitudeOffset - lastPosture_.longitudeOffset) > epsilon);
}

void LEDSphereManager::updateUVCacheIfNeeded() {
    // TODO: UV座標キャッシュ更新判定と実行
    Serial.println("[LEDSphereManager] UV cache update check");
}

// ========== シングルトンアクセス ==========

LEDSphereManager* SpherePatternInterface::instance_ = nullptr;

LEDSphereManager* SpherePatternInterface::getInstance() {
    if (!instance_) {
        instance_ = new LEDSphereManager();
    }
    return instance_;
}

bool SpherePatternInterface::initialize(const char* csvPath) {
    LEDSphereManager* manager = getInstance();
    return manager->initialize(csvPath);
}

void SpherePatternInterface::shutdown() {
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
}

} // namespace LEDSphere