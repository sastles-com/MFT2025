/**
 * @file LEDSphereManager.cpp
 * @brief LED球体統合制御システム実装
 */

#include "led/LEDSphereManager.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

// 高速数学関数（CUBE-neonからの移植）
#include "math/fast_math.h"

#if defined(ARDUINO)
#include <FS.h>
#include <LittleFS.h>
#endif

#ifndef ARDUINO
struct DummySerial {
    template<typename... Args> void printf(const char*, Args...) {}
    void println(const char*) {}
    void print(const char*) {}
};
static DummySerial Serial;
#else
#include <Arduino.h>
#endif

namespace LEDSphere {

namespace {
template <typename T>
T clampValue(T value, T minValue, T maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}
}

static float degToRad(float deg) {
    return deg * static_cast<float>(M_PI) / 180.0f;
}

static float radToDeg(float rad) {
    return rad * 180.0f / static_cast<float>(M_PI);
}

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
    if (frameBuffer_) {
        free(frameBuffer_);
        frameBuffer_ = nullptr;
        totalLeds_ = 0;
    }
    layoutPositions_.clear();
    latitudeCacheDeg_.clear();
    longitudeCacheDeg_.clear();
    layoutLoaded_ = false;
}

// ========== 初期化・設定 ==========

bool LEDSphereManager::initialize(const char* csvPath) {
    if (initialized_) {
        Serial.println("[LEDSphereManager] Already initialized");
        return true;
    }

    Serial.printf("[LEDSphereManager] Initializing with CSV: %s\n", csvPath);

    layoutLoaded_ = loadLayoutFromCSV(csvPath);
    if (!layoutLoaded_) {
        Serial.println("[LEDSphereManager] ⚠️ Failed to load LED layout - latitude/longitude patterns may be approximate");
    } else {
        buildLayoutCaches();
        Serial.printf("[LEDSphereManager] Loaded %u LED layout entries\n", (unsigned)layoutPositions_.size());
    }

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
#if defined(USE_FASTLED)
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
#else
        (void)pin;
        (void)count;
#endif
        offset += count;
    }

    Serial.printf("[LEDSphereManager] LED hardware initialized, total LEDs=%u\n", (unsigned)totalLeds_);
#if defined(USE_FASTLED)
    clearAllLEDs();
    FastLED.show();
#endif
    return true;
}

bool LEDSphereManager::loadLayoutFromCSV(const char* csvPath) {
#if defined(ARDUINO)
    File file = LittleFS.open(csvPath, "r");
    if (!file) {
        String altPath = String("/littlefs") + String(csvPath);
        file = LittleFS.open(altPath, "r");
    }
    if (!file) {
        Serial.printf("[LEDSphereManager] Failed to open layout CSV: %s\n", csvPath);
        return false;
    }

    layoutPositions_.clear();
    latitudeCacheDeg_.clear();
    longitudeCacheDeg_.clear();

    // Skip header line
    String header = file.readStringUntil('\n');
    (void)header;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) {
            continue;
        }

        int comma1 = line.indexOf(',');
        int comma2 = line.indexOf(',', comma1 + 1);
        int comma3 = line.indexOf(',', comma2 + 1);
        int comma4 = line.indexOf(',', comma3 + 1);
        int comma5 = line.indexOf(',', comma4 + 1);

        if (comma1 < 0 || comma2 < 0 || comma3 < 0 || comma4 < 0 || comma5 < 0) {
            continue;
        }

        uint16_t faceID = static_cast<uint16_t>(line.substring(0, comma1).toInt());
        uint8_t strip = static_cast<uint8_t>(line.substring(comma1 + 1, comma2).toInt());
        uint8_t stripNum = static_cast<uint8_t>(line.substring(comma2 + 1, comma3).toInt());
        float x = line.substring(comma3 + 1, comma4).toFloat();
        float y = line.substring(comma4 + 1, comma5).toFloat();
        float z = line.substring(comma5 + 1).toFloat();

        layoutPositions_.emplace_back(faceID, strip, stripNum, x, y, z);
    }

    file.close();

    bool ok = !layoutPositions_.empty();
    if (!ok) {
        layoutPositions_.clear();
    }
    return ok;
#else
    (void)csvPath;
    return false;
#endif
}

void LEDSphereManager::buildLayoutCaches() {
    latitudeCacheDeg_.resize(layoutPositions_.size());
    longitudeCacheDeg_.resize(layoutPositions_.size());
    for (size_t i = 0; i < layoutPositions_.size(); ++i) {
        const auto& pos = layoutPositions_[i];
        latitudeCacheDeg_[i] = computeLatitudeDeg(pos.x, pos.y, pos.z);
        longitudeCacheDeg_[i] = computeLongitudeDeg(pos.x, pos.y, pos.z);
    }
}

float LEDSphereManager::computeLatitudeDeg(float x, float y, float z) {
    (void)x;
    (void)z;
    float clampedY = clampValue(y, -1.0f, 1.0f);
    return radToDeg(fast_asin(clampedY));
}

float LEDSphereManager::computeLongitudeDeg(float x, float y, float z) {
    (void)y;
    return radToDeg(fast_atan2(z, x));
}

float LEDSphereManager::wrappedLongitudeDifference(float aDeg, float bDeg) {
    float diff = fmodf(aDeg - bDeg + 540.0f, 360.0f) - 180.0f;
    return fabsf(diff);
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
    if (!frameBuffer_ || faceID >= totalLeds_) {
        Serial.printf("[LEDSphereManager] Invalid faceID: %d\n", faceID);
        return;
    }
    frameBuffer_[faceID] = color;
}

void LEDSphereManager::setLEDByUV(float u, float v, CRGB color, float radius) {
    Serial.printf("[LEDSphereManager] UV LED set: (%.3f, %.3f) RGB(%d,%d,%d) r=%.3f\n", 
                  u, v, color.r, color.g, color.b, radius);
    if (!frameBuffer_) return;
    float clampedU = clampValue(u, 0.0f, 1.0f);
    size_t index = static_cast<size_t>(clampedU * (totalLeds_ - 1));
    frameBuffer_[index] = color;
}

void LEDSphereManager::clearAllLEDs() {
#ifdef UNIT_TEST
    operationLog_.push_back("clear");
#endif
    if (!frameBuffer_) {
        return;
    }
    for (size_t i = 0; i < totalLeds_; ++i) {
        frameBuffer_[i] = CRGB(0, 0, 0);
    }
}

void LEDSphereManager::setBrightness(uint8_t brightness) {
    Serial.printf("[LEDSphereManager] Brightness set to %d\n", brightness);
#if defined(USE_FASTLED)
    FastLED.setBrightness(brightness);
#else
    (void)brightness;
#endif
}

void LEDSphereManager::show() {
#ifdef UNIT_TEST
    showCalledForTest_ = true;
    operationLog_.push_back("show");
#endif
#if defined(USE_FASTLED)
    FastLED.show();
#endif
}

// ========== 高速パターン描画 ==========

void LEDSphereManager::drawCoordinateAxis(bool showGrid, float brightness) {
    Serial.printf("[LEDSphereManager] Drawing coordinate axis (grid=%s, brightness=%.2f)\n", 
                  showGrid ? "true" : "false", brightness);
    
    // TODO: 座標軸描画
}

void LEDSphereManager::drawLatitudeLine(float latitude, CRGB color, uint8_t lineWidth) {
    if (!frameBuffer_) return;

    if (layoutLoaded_ && layoutPositions_.size() == latitudeCacheDeg_.size()) {
        float tolerance = std::max(1.0f, static_cast<float>(lineWidth) * 2.0f);
        for (size_t i = 0; i < layoutPositions_.size(); ++i) {
            if (fabsf(latitudeCacheDeg_[i] - latitude) <= tolerance) {
                uint16_t id = layoutPositions_[i].faceID;
                if (id < totalLeds_) {
                    frameBuffer_[id] = color;
                }
            }
        }
        return;
    }

    float normLat = clampValue((latitude + 90.0f) / 180.0f, 0.0f, 1.0f);
    size_t center = static_cast<size_t>(normLat * (totalLeds_ - 1));
    size_t bandWidth = std::max<size_t>(1, static_cast<size_t>(lineWidth)) * std::max<size_t>(1, totalLeds_ / 200 + 1);
    size_t start = (center > bandWidth) ? center - bandWidth : 0;
    size_t end = std::min(totalLeds_, center + bandWidth + 1);
    for (size_t i = start; i < end; ++i) {
        frameBuffer_[i] = color;
    }
#ifdef UNIT_TEST
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "lat:%.1f", latitude);
    operationLog_.emplace_back(buffer);
#endif
}

void LEDSphereManager::drawLongitudeLine(float longitude, CRGB color, uint8_t lineWidth) {
    if (!frameBuffer_) return;

    if (layoutLoaded_ && layoutPositions_.size() == longitudeCacheDeg_.size()) {
        float tolerance = std::max(2.0f, static_cast<float>(lineWidth) * 4.0f);
        for (size_t i = 0; i < layoutPositions_.size(); ++i) {
            float diff = wrappedLongitudeDifference(longitudeCacheDeg_[i], longitude);
            if (diff <= tolerance) {
                uint16_t id = layoutPositions_[i].faceID;
                if (id < totalLeds_) {
                    frameBuffer_[id] = color;
                }
            }
        }
        return;
    }

    float normalized = longitude;
    while (normalized < 0.0f) normalized += 360.0f;
    while (normalized >= 360.0f) normalized -= 360.0f;
    float norm = normalized / 360.0f;
    size_t center = static_cast<size_t>(norm * (totalLeds_ - 1));
    size_t bandWidth = std::max<size_t>(1, static_cast<size_t>(lineWidth)) * std::max<size_t>(1, totalLeds_ / 200 + 1);
    size_t start = (center > bandWidth) ? center - bandWidth : 0;
    size_t end = std::min(totalLeds_, center + bandWidth + 1);
    for (size_t i = start; i < end; ++i) {
        frameBuffer_[i] = color;
    }
#ifdef UNIT_TEST
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "lon:%.1f", longitude);
    operationLog_.emplace_back(buffer);
#endif
}

void LEDSphereManager::drawSparsePattern(const std::map<uint16_t, CRGB>& points) {
    Serial.printf("[LEDSphereManager] Drawing sparse pattern: %zu points\n", points.size());
    if (!frameBuffer_) return;
    for (const auto& entry : points) {
        if (entry.first < totalLeds_) {
            frameBuffer_[entry.first] = entry.second;
        }
    }
}

void LEDSphereManager::setAxisMarkerParams(float thresholdDegrees, uint8_t maxCount) {
    axisMarkerThresholdDeg_ = thresholdDegrees;
    axisMarkerMaxCount_ = std::max<uint8_t>(1, maxCount);
}

void LEDSphereManager::drawAxisMarkers(float thresholdDegrees, uint8_t maxPerAxis) {
    setAxisMarkerParams(thresholdDegrees, maxPerAxis);
    drawAxisMarkers();
}

void LEDSphereManager::drawAxisMarkers() {
    if (!frameBuffer_ || !layoutLoaded_) {
        return;
    }

    const uint8_t maxPerAxis = std::max<uint8_t>(1, axisMarkerMaxCount_);
    const float cosThreshold = cosf(degToRad(axisMarkerThresholdDeg_));

    struct AxisMarker {
        float score;
        uint16_t faceID;
    };

    auto selectAxis = [&](float ax, float ay, float az, const CRGB& color) {
        std::vector<AxisMarker> markers;
        markers.reserve(layoutPositions_.size());

        for (const auto& pos : layoutPositions_) {
            float len = fast_sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
            if (len <= 0.0001f) {
                continue;
            }
            float dot = (pos.x * ax + pos.y * ay + pos.z * az) / len;  // -1.0 ~ 1.0 の範囲
            markers.push_back({dot, pos.faceID});
        }

        std::sort(markers.begin(), markers.end(), [](const AxisMarker& a, const AxisMarker& b) {
            return a.score > b.score;
        });

        std::vector<AxisMarker> selected;
        selected.reserve(maxPerAxis);
        for (const auto& marker : markers) {
            if (marker.score < cosThreshold && !selected.empty()) {
                continue;
            }
            selected.push_back(marker);
            if (selected.size() >= maxPerAxis) {
                break;
            }
        }

        if (selected.empty() && !markers.empty()) {
            selected.push_back(markers.front());
        }

        for (const auto& marker : selected) {
            if (marker.faceID < totalLeds_) {
                frameBuffer_[marker.faceID] = color;
            }
        }
    };

    selectAxis(1.0f, 0.0f, 0.0f, CRGB(255, 0, 0));   // +X 赤
    selectAxis(0.0f, 1.0f, 0.0f, CRGB(0, 255, 0));   // +Y 緑
    selectAxis(0.0f, 0.0f, 1.0f, CRGB(0, 0, 255));   // +Z 青
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
    
    // CUBE-neon実績実装: IMU回転適用後のUV座標変換
    // 1. IMUクォータニオンで3D座標を回転
    float rotated_x, rotated_y, rotated_z;
    applyQuaternionRotation(x, y, z, rotated_x, rotated_y, rotated_z);
    
    // 2. 球面座標変換（CUBE-neon方式）
    // u = atan2(sqrt(x^2 + z^2), y)  // 緯度成分
    // v = atan2(x, z)                // 経度成分
    float u = fast_atan2(fast_sqrt(rotated_x * rotated_x + rotated_z * rotated_z), rotated_y);
    float v = fast_atan2(rotated_x, rotated_z);
    
    UVCoordinate result;
    result.u = u;
    result.v = v;
    
    return result;
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

void LEDSphereManager::updateAllLEDsFromImage() {
    if (!frameBuffer_ || !layoutLoaded_) {
        Serial.println("[LEDSphereManager] Cannot update LEDs: framebuffer or layout not ready");
        return;
    }
    
    Serial.printf("[LEDSphereManager] Updating %zu LEDs from image using CUBE-neon method\n", layoutPositions_.size());
    
    // CUBE-neon実績実装: 全LEDループでIMU変換→UV変換→色抽出
    for (size_t i = 0; i < layoutPositions_.size(); ++i) {
        const auto& pos = layoutPositions_[i];
        
        // 1. LED座標取得
        float x = pos.x;
        float y = pos.y;
        float z = pos.z;
        
        // 2. IMUクォータニオンで回転適用
        float rotated_x, rotated_y, rotated_z;
        applyQuaternionRotation(x, y, z, rotated_x, rotated_y, rotated_z);
        
        // 3. UV座標変換（CUBE-neon方式）
        float u = fast_atan2(fast_sqrt(rotated_x * rotated_x + rotated_z * rotated_z), rotated_y);
        float v = fast_atan2(rotated_x, rotated_z);
        
        // 4. 画像から色抽出
        CRGB color = extractColorFromImageUV(u, v);
        
        // 5. LED色設定
        uint16_t faceID = pos.faceID;
        if (faceID < totalLeds_) {
            frameBuffer_[faceID] = color;
        }
        
        // デバッグ出力（最初のLEDのみ）
        if (i == 0) {
            Serial.printf("[LEDSphereManager] LED[0]: pos(%.3f,%.3f,%.3f) → rot(%.3f,%.3f,%.3f) → uv(%.3f,%.3f) → RGB(%d,%d,%d)\n",
                         x, y, z, rotated_x, rotated_y, rotated_z, u, v, color.r, color.g, color.b);
        }
    }
}

void LEDSphereManager::updateUVCacheIfNeeded() {
    // TODO: UV座標キャッシュ更新判定と実行
    Serial.println("[LEDSphereManager] UV cache update check");
}

// ========== CUBE-neon実績実装: 座標変換ヘルパー関数 ==========

void LEDSphereManager::applyQuaternionRotation(float x, float y, float z, 
                                             float& out_x, float& out_y, float& out_z) const {
    // CUBE-neonからの移植: クォータニオン回転適用
    // q * v * q^(-1) の計算
    float qw = lastPosture_.quaternionW;
    float qx = lastPosture_.quaternionX;
    float qy = lastPosture_.quaternionY;
    float qz = lastPosture_.quaternionZ;
    
    // クォータニオンの正規化
    float norm = fast_sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
    if (norm > 0.0001f) {
        qw /= norm; qx /= norm; qy /= norm; qz /= norm;
    }
    
    // ベクトル回転: v' = q * v * q^(-1)
    // 展開形での高速計算
    float qw2 = qw * qw;
    float qx2 = qx * qx;
    float qy2 = qy * qy;
    float qz2 = qz * qz;
    
    out_x = (qw2 + qx2 - qy2 - qz2) * x + 2.0f * (qx*qy - qw*qz) * y + 2.0f * (qx*qz + qw*qy) * z;
    out_y = 2.0f * (qx*qy + qw*qz) * x + (qw2 - qx2 + qy2 - qz2) * y + 2.0f * (qy*qz - qw*qx) * z;
    out_z = 2.0f * (qx*qz - qw*qy) * x + 2.0f * (qy*qz + qw*qx) * y + (qw2 - qx2 - qy2 + qz2) * z;
}

CRGB LEDSphereManager::extractColorFromImageUV(float u, float v) const {
    // CUBE-neon実績実装: UV座標から画像色抽出
    // TODO: 画像データの実装が必要（half-grad-160-80.hスタイル）
    
    // 暫定実装: プロシージャル色生成
    // u: 緯度系（-π/2 〜 π/2） → 0〜1に正規化
    // v: 経度系（-π 〜 π） → 0〜1に正規化
    float norm_u = (u + M_PI/2.0f) / M_PI;
    float norm_v = (v + M_PI) / (2.0f * M_PI);
    
    // クランプ
    norm_u = clampValue(norm_u, 0.0f, 1.0f);
    norm_v = clampValue(norm_v, 0.0f, 1.0f);
    
    // HSV色空間でのプロシージャル生成（CUBE-neon方式）
    uint8_t hue = (uint8_t)(norm_v * 255);        // 経度で色相
    uint8_t sat = 255;                           // 彩度は最大
    uint8_t val = (uint8_t)(norm_u * 255);       // 緯度で明度
    
    CRGB color;
    color.setHSV(hue, sat, val);
    return color;
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
