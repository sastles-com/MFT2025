/**
 * @file LEDSphereRenderer.cpp
 * @brief LED球体統合レンダリングシステム実装
 */

#include "led/LEDSphereRenderer.h"
#include "M5Unified.h"
#include <LittleFS.h>
#include <cmath>
#include <algorithm>

namespace LEDSphere {

// ---- LEDSphereRenderer 実装 ----

LEDSphereRenderer::LEDSphereRenderer() 
    : enableSparseRendering_(true), lastFullRenderMs_(0), lastSparseRenderMs_(0),
      uvCacheDirty_(true), lastQuaternionW_(1.0f), lastQuaternionX_(0.0f), 
      lastQuaternionY_(0.0f), lastQuaternionZ_(0.0f), lastLatOffset_(0.0f), lastLonOffset_(0.0f) {
    
    // LEDバッファ初期化
    FastLED.clear();
    
    // パターンジェネレータ初期化
    patternGenerator_ = std::make_unique<ProceduralPattern::PatternGenerator>();
    
    // LED配置データ予約
    ledPositions_.reserve(LED_COUNT);
    
    Serial.println("[LEDSphere] Renderer initialized");
}

LEDSphereRenderer::~LEDSphereRenderer() {
    Serial.println("[LEDSphere] Renderer destroyed");
}

bool LEDSphereRenderer::initialize() {
    Serial.println("[LEDSphere] Initializing FastLED...");
    
    // FastLED設定（4ストリップ構成）
    // GPIO 5/6/7/8 を想定
    FastLED.addLeds<WS2812, 5, GRB>(leds_, 0, 200);      // Strip 0: LED 0-199
    FastLED.addLeds<WS2812, 6, GRB>(leds_, 200, 200);    // Strip 1: LED 200-399
    FastLED.addLeds<WS2812, 7, GRB>(leds_, 400, 200);    // Strip 2: LED 400-599  
    FastLED.addLeds<WS2812, 8, GRB>(leds_, 600, 200);    // Strip 3: LED 600-799
    
    FastLED.setBrightness(128);  // 50%輝度でスタート
    FastLED.clear();
    FastLED.show();
    
    Serial.println("[LEDSphere] FastLED configured for 4 strips (800 LEDs)");
    return true;
}

bool LEDSphereRenderer::loadLEDLayout(const char* csvPath) {
    Serial.printf("[LEDSphere] Loading LED layout from: %s\n", csvPath);
    
    File csvFile = LittleFS.open(csvPath, "r");
    if (!csvFile) {
        Serial.printf("[LEDSphere] ERROR: Cannot open %s\n", csvPath);
        return false;
    }
    
    ledPositions_.clear();
    String line;
    bool isHeader = true;
    
    while (csvFile.available()) {
        line = csvFile.readStringUntil('\n');
        line.trim();
        
        if (isHeader) {
            isHeader = false;
            continue;  // ヘッダー行をスキップ
        }
        
        if (line.length() == 0) continue;
        
        // CSV解析: FaceID,strip,strip_num,x,y,z
        int commaPos[5];
        int commaCount = 0;
        for (int i = 0; i < line.length() && commaCount < 5; i++) {
            if (line.charAt(i) == ',') {
                commaPos[commaCount++] = i;
            }
        }
        
        if (commaCount >= 5) {
            LEDPosition pos;
            pos.faceID = line.substring(0, commaPos[0]).toInt();
            pos.strip = line.substring(commaPos[0] + 1, commaPos[1]).toInt();
            pos.strip_num = line.substring(commaPos[1] + 1, commaPos[2]).toInt();
            pos.x = line.substring(commaPos[2] + 1, commaPos[3]).toFloat();
            pos.y = line.substring(commaPos[3] + 1, commaPos[4]).toFloat();
            pos.z = line.substring(commaPos[4] + 1).toFloat();
            pos.uvDirty = true;
            
            ledPositions_.push_back(pos);
        }
    }
    
    csvFile.close();
    
    Serial.printf("[LEDSphere] Loaded %d LED positions\n", ledPositions_.size());
    return ledPositions_.size() == LED_COUNT;
}

void LEDSphereRenderer::render(const RenderParams& params) {
    uint32_t renderStartMs = millis();
    
    // UV座標キャッシュ更新（姿勢変化検出）
    updateUVCoordinates(params);
    
    // レンダリング方式選択
    switch (params.source) {
        case RenderSource::PROCEDURAL_PATTERN:
            if (enableSparseRendering_) {
                renderSparsePattern(params);
                lastSparseRenderMs_ = millis();
            } else {
                renderProceduralPattern(params);
                lastFullRenderMs_ = millis();
            }
            break;
            
        case RenderSource::IMAGE_TEXTURE:
            renderImageTexture(params);
            lastFullRenderMs_ = millis();
            break;
            
        case RenderSource::HYBRID:
            renderHybrid(params);
            lastFullRenderMs_ = millis();
            break;
    }
    
    // 輝度調整
    if (params.brightness < 1.0f) {
        FastLED.setBrightness(static_cast<uint8_t>(params.brightness * 255));
    }
    
    uint32_t renderTimeMs = millis() - renderStartMs;
    
    // デバッグ情報（高頻度描画時は間引き）
    static uint32_t lastDebugMs = 0;
    if (millis() - lastDebugMs > 1000) {  // 1秒間隔
        Serial.printf("[LEDSphere] Render: %s, %dms, Active LEDs: %d\n", 
                     (params.source == RenderSource::PROCEDURAL_PATTERN) ? "Procedural" : "Image",
                     renderTimeMs, getActiveLEDCount());
        lastDebugMs = millis();
    }
}

void LEDSphereRenderer::show() {
    FastLED.show();
}

void LEDSphereRenderer::updateUVCoordinates(const RenderParams& params) {
    // 姿勢変化検出
    bool quaternionChanged = (abs(params.quaternionW - lastQuaternionW_) > 0.001f ||
                             abs(params.quaternionX - lastQuaternionX_) > 0.001f ||
                             abs(params.quaternionY - lastQuaternionY_) > 0.001f ||
                             abs(params.quaternionZ - lastQuaternionZ_) > 0.001f);
    
    bool offsetChanged = (abs(params.latitudeOffset - lastLatOffset_) > 0.1f ||
                         abs(params.longitudeOffset - lastLonOffset_) > 0.1f);
    
    if (quaternionChanged || offsetChanged || uvCacheDirty_) {
        // 全LEDのUV座標を更新
        for (auto& led : ledPositions_) {
            float x = led.x, y = led.y, z = led.z;
            
            // 姿勢変換適用
            transformPosition(x, y, z, params);
            
            // UV座標計算
            cartesianToUV(x, y, z, led.u, led.v);
            led.uvDirty = false;
        }
        
        // キャッシュ状態更新
        lastQuaternionW_ = params.quaternionW;
        lastQuaternionX_ = params.quaternionX;
        lastQuaternionY_ = params.quaternionY;
        lastQuaternionZ_ = params.quaternionZ;
        lastLatOffset_ = params.latitudeOffset;
        lastLonOffset_ = params.longitudeOffset;
        uvCacheDirty_ = false;
        
        Serial.println("[LEDSphere] UV coordinates updated");
    }
}

void LEDSphereRenderer::transformPosition(float& x, float& y, float& z, const RenderParams& params) const {
    // クォータニオン回転変換
    float qw = params.quaternionW, qx = params.quaternionX, qy = params.quaternionY, qz = params.quaternionZ;
    
    // クォータニオン * ベクトル * クォータニオン共役
    float qxx = qx * qx, qyy = qy * qy, qzz = qz * qz;
    float qxy = qx * qy, qxz = qx * qz, qyz = qy * qz;
    float qwx = qw * qx, qwy = qw * qy, qwz = qw * qz;
    
    float newX = x * (1 - 2 * (qyy + qzz)) + y * (2 * (qxy - qwz)) + z * (2 * (qxz + qwy));
    float newY = x * (2 * (qxy + qwz)) + y * (1 - 2 * (qxx + qzz)) + z * (2 * (qyz - qwx));
    float newZ = x * (2 * (qxz - qwy)) + y * (2 * (qyz + qwx)) + z * (1 - 2 * (qxx + qyy));
    
    x = newX;
    y = newY; 
    z = newZ;
    
    // 緯度・経度オフセット適用（簡易実装）
    if (abs(params.latitudeOffset) > 0.1f || abs(params.longitudeOffset) > 0.1f) {
        // TODO: 球面座標での緯度・経度オフセット適用
        // 現在は省略（複雑な三角関数計算が必要）
    }
}

void LEDSphereRenderer::cartesianToUV(float x, float y, float z, float& u, float& v) const {
    // 球面座標変換
    float theta = atan2f(x, z);              // 経度 [-π, π]
    float phi = asinf(y);                    // 緯度 [-π/2, π/2]
    
    // UV座標に正規化 [0.0, 1.0]
    u = (theta + PI) / (2.0f * PI);          // 経度: 0.0-1.0
    v = (phi + PI / 2.0f) / PI;              // 緯度: 0.0-1.0
    
    // 境界処理
    u = fmodf(u + 1.0f, 1.0f);              // [0.0, 1.0)にラップ
    v = constrain(v, 0.0f, 1.0f);           // [0.0, 1.0]にクランプ
}

void LEDSphereRenderer::renderProceduralPattern(const RenderParams& params) {
    // 全LED黒クリア
    clearLEDs();
    
    if (params.patternName == "coordinate_axis") {
        // 座標軸インジケータ：X(赤), Y(緑), Z(青)
        renderCoordinateAxisPattern(params);
    } else if (params.patternName == "latitude_rings") {
        // 緯度線光の輪パターン
        renderLatitudeRingPattern(params);
    } else if (params.patternName == "longitude_lines") {
        // 経度線波動パターン
        renderLongitudeLinesPattern(params);
    } else {
        // 汎用プロシージャルパターン（全LED計算）
        renderGenericPattern(params);
    }
}

void LEDSphereRenderer::renderSparsePattern(const RenderParams& params) {
    // スパースレンダリング：必要なLEDのみ計算
    clearLEDs();
    
    if (params.patternName == "coordinate_axis") {
        // 最小3点のLED（X, Y, Z軸）
        renderSparseCoordinateAxis(params);
    } else if (params.patternName == "latitude_rings") {
        // 1つの緯度線のみ
        renderSparseLatitudeRing(params);
    } else {
        // フォールバック：通常レンダリング
        renderProceduralPattern(params);
    }
}

void LEDSphereRenderer::renderCoordinateAxisPattern(const RenderParams& params) {
    // X軸（赤）：+X方向のLED検索
    float targetU_X = 0.75f;  // X軸正方向は経度270度 ≈ u=0.75
    float targetV_X = 0.5f;   // 赤道面 ≈ v=0.5
    
    uint16_t closestLED_X = findClosestLED(targetU_X, targetV_X);
    if (closestLED_X < LED_COUNT) {
        setLED(closestLED_X, CRGB::Red);
        // 周辺LEDも点灯（範囲拡大）
        for (int i = 0; i < 5; i++) {
            uint16_t nearbyLED = findNthClosestLED(targetU_X, targetV_X, i);
            if (nearbyLED < LED_COUNT) {
                setLED(nearbyLED, CRGB(0x800000));  // 暗赤
            }
        }
    }
    
    // Y軸（緑）：+Y方向のLED検索
    float targetU_Y = 0.5f;   // 任意経度
    float targetV_Y = 1.0f;   // 北極 ≈ v=1.0
    
    uint16_t closestLED_Y = findClosestLED(targetU_Y, targetV_Y);
    if (closestLED_Y < LED_COUNT) {
        setLED(closestLED_Y, CRGB::Green);
        for (int i = 0; i < 5; i++) {
            uint16_t nearbyLED = findNthClosestLED(targetU_Y, targetV_Y, i);
            if (nearbyLED < LED_COUNT) {
                setLED(nearbyLED, CRGB(0x008000));  // 暗緑
            }
        }
    }
    
    // Z軸（青）：+Z方向のLED検索
    float targetU_Z = 0.5f;   // 経度0度 ≈ u=0.5
    float targetV_Z = 0.5f;   // 赤道面 ≈ v=0.5
    
    uint16_t closestLED_Z = findClosestLED(targetU_Z, targetV_Z);
    if (closestLED_Z < LED_COUNT) {
        setLED(closestLED_Z, CRGB::Blue);
        for (int i = 0; i < 5; i++) {
            uint16_t nearbyLED = findNthClosestLED(targetU_Z, targetV_Z, i);
            if (nearbyLED < LED_COUNT) {
                setLED(nearbyLED, CRGB(0x000080));  // 暗青
            }
        }
    }
}

void LEDSphereRenderer::renderSparseCoordinateAxis(const RenderParams& params) {
    // 高速版：各軸1点のみ
    clearLEDs();
    
    uint16_t xAxisLED = findClosestLED(0.75f, 0.5f);  // X軸
    uint16_t yAxisLED = findClosestLED(0.5f, 1.0f);   // Y軸  
    uint16_t zAxisLED = findClosestLED(0.5f, 0.5f);   // Z軸
    
    if (xAxisLED < LED_COUNT) setLED(xAxisLED, CRGB::Red);
    if (yAxisLED < LED_COUNT) setLED(yAxisLED, CRGB::Green);
    if (zAxisLED < LED_COUNT) setLED(zAxisLED, CRGB::Blue);
}

uint16_t LEDSphereRenderer::findClosestLED(float targetU, float targetV) const {
    uint16_t closestID = LED_COUNT;  // 無効値
    float minDistance = 10.0f;       // 十分大きな値
    
    for (size_t i = 0; i < ledPositions_.size(); i++) {
        const auto& led = ledPositions_[i];
        
        // UV距離計算（トーラス面での距離）
        float du = abs(led.u - targetU);
        float dv = abs(led.v - targetV);
        
        // 経度の境界ラップ処理
        if (du > 0.5f) du = 1.0f - du;
        
        float distance = sqrtf(du * du + dv * dv);
        
        if (distance < minDistance) {
            minDistance = distance;
            closestID = led.faceID;
        }
    }
    
    return closestID;
}

uint16_t LEDSphereRenderer::findNthClosestLED(float targetU, float targetV, int n) const {
    // TODO: N番目に近いLEDを検索する実装
    // 現在は簡易版（最近傍のみ）
    return findClosestLED(targetU, targetV);
}

void LEDSphereRenderer::clearLEDs() {
    FastLED.clear();
    // または：
    // for (size_t i = 0; i < LED_COUNT; i++) {
    //     leds_[i] = CRGB::Black;
    // }
}

void LEDSphereRenderer::setLED(uint16_t faceID, CRGB color) {
    if (faceID < LED_COUNT) {
        leds_[faceID] = color;
    }
}

uint16_t LEDSphereRenderer::getActiveLEDCount() const {
    uint16_t count = 0;
    for (size_t i = 0; i < LED_COUNT; i++) {
        if (leds_[i] != CRGB::Black) {
            count++;
        }
    }
    return count;
}

float LEDSphereRenderer::getLastRenderTimeMs() const {
    // TODO: 実際の測定値を返す実装
    return 0.0f;
}

// ---- FastPatternRenderer 実装 ----

void FastPatternRenderer::renderCoordinateAxis(const RenderParams& params) {
    if (!sphereRenderer_) return;
    
    // 高速座標軸レンダリング（最小LED数）
    sphereRenderer_->clearLEDs();
    
    // 各軸の代表LED 1個ずつのみ点灯
    uint16_t xLED = sphereRenderer_->findClosestLED(0.75f, 0.5f);
    uint16_t yLED = sphereRenderer_->findClosestLED(0.5f, 1.0f);
    uint16_t zLED = sphereRenderer_->findClosestLED(0.5f, 0.5f);
    
    sphereRenderer_->setLED(xLED, CRGB::Red);
    sphereRenderer_->setLED(yLED, CRGB::Green);
    sphereRenderer_->setLED(zLED, CRGB::Blue);
}

void FastPatternRenderer::renderLatitudeRing(float latitude, CRGB color, const RenderParams& params) {
    if (!sphereRenderer_) return;
    
    // 指定緯度の光の輪（高速版）
    sphereRenderer_->clearLEDs();
    
    float targetV = (latitude + 90.0f) / 180.0f;  // 緯度→v座標変換
    
    // 経度12分割で代表点のみ描画
    for (int i = 0; i < 12; i++) {
        float targetU = i / 12.0f;
        uint16_t ledID = sphereRenderer_->findClosestLED(targetU, targetV);
        sphereRenderer_->setLED(ledID, color);
    }
}

// ---- ImageTextureRenderer 実装 ----

void ImageTextureRenderer::renderFullTexture(const RenderParams& params) {
    if (!sphereRenderer_ || !params.imageData) return;
    
    // 画像を全LEDにマッピング（10fps標準品質）
    sphereRenderer_->clearLEDs();
    
    // TODO: JPEG画像デコード→テクスチャキャッシュ→LED描画
    // 実装複雑なため省略
}

CRGB ImageTextureRenderer::sampleTexture(float u, float v) const {
    // TODO: バイリニア補間テクスチャサンプリング
    return CRGB::Black;
}

} // namespace LEDSphere