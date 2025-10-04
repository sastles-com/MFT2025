#include "pattern/ProceduralPatternGenerator.h"
#include "led/LEDSphereManager.h"
#include <algorithm>
#include <cmath>
#include "pattern/TestStripPattern.h"

namespace ProceduralPattern {

// ---- SphereCoordinateSystem 実装 ----

SphereCoordinateSystem::SphericalCoord 
SphereCoordinateSystem::cartesianToSpherical(float x, float y, float z) {
    SphericalCoord coord;
    float r = sqrtf(x*x + y*y + z*z);
    if (r > 0.001f) {
        coord.phi = asinf(y / r);      // 緯度 [-π/2, π/2]
        coord.theta = atan2f(z, x);    // 経度 [-π, π]
    } else {
        coord.phi = 0.0f;
        coord.theta = 0.0f;
    }
    return coord;
}

SphereCoordinateSystem::UVCoord 
SphereCoordinateSystem::sphericalToUV(const SphericalCoord& coord) {
    UVCoord uv;
    uv.u = (coord.theta + PI) / (2.0f * PI);     // 経度を[0,1]に正規化
    uv.v = (coord.phi + PI/2.0f) / PI;           // 緯度を[0,1]に正規化
    return uv;
}

SphereCoordinateSystem::ScreenPoint 
SphereCoordinateSystem::sphericalToScreen(const SphericalCoord& coord, int centerX, int centerY, int radius) {
    ScreenPoint point;
    
    // 3D座標変換
    float x = cosf(coord.phi) * cosf(coord.theta);
    float y = sinf(coord.phi);
    float z = cosf(coord.phi) * sinf(coord.theta);
    
    // 前面表示チェック
    point.visible = (x > 0);
    
    if (point.visible) {
        point.x = centerX + (int)(z * radius * 0.9f);
        point.y = centerY - (int)(y * radius * 0.9f);
        point.intensity = x;  // 距離による強度
    } else {
        point.x = point.y = 0;
        point.intensity = 0.0f;
    }
    
    return point;
}

std::vector<SphereCoordinateSystem::ScreenPoint> 
SphereCoordinateSystem::getLatitudeLine(float latitude, int centerX, int centerY, int radius, int points) {
    std::vector<ScreenPoint> linePoints;
    linePoints.reserve(points);
    
    float latRad = latitude * PI / 180.0f;
    
    for (int i = 0; i < points; i++) {
        float lonRad = (float)i / points * 2.0f * PI;
        
        SphericalCoord coord;
        coord.phi = latRad;
        coord.theta = lonRad;
        
        ScreenPoint point = sphericalToScreen(coord, centerX, centerY, radius);
        if (point.visible) {
            linePoints.push_back(point);
        }
    }
    
    return linePoints;
}

std::vector<SphereCoordinateSystem::ScreenPoint> 
SphereCoordinateSystem::getLongitudeLine(float longitude, int centerX, int centerY, int radius, int points) {
    std::vector<ScreenPoint> linePoints;
    linePoints.reserve(points);
    
    float lonRad = longitude * PI / 180.0f;
    
    for (int i = 0; i < points; i++) {
        float latRad = ((float)i / (points - 1) - 0.5f) * PI;  // -π/2 to π/2
        
        SphericalCoord coord;
        coord.phi = latRad;
        coord.theta = lonRad;
        
        ScreenPoint point = sphericalToScreen(coord, centerX, centerY, radius);
        if (point.visible) {
            linePoints.push_back(point);
        }
    }
    
    return linePoints;
}

uint16_t SphereCoordinateSystem::interpolateColor(uint16_t color1, uint16_t color2, float t) {
    t = constrain(t, 0.0f, 1.0f);
    
    // RGB565を展開
    uint8_t r1 = (color1 >> 11) & 0x1F;
    uint8_t g1 = (color1 >> 5) & 0x3F;
    uint8_t b1 = color1 & 0x1F;
    
    uint8_t r2 = (color2 >> 11) & 0x1F;
    uint8_t g2 = (color2 >> 5) & 0x3F;
    uint8_t b2 = color2 & 0x1F;
    
    // 補間計算
    uint8_t r = (uint8_t)(r1 + (r2 - r1) * t);
    uint8_t g = (uint8_t)(g1 + (g2 - g1) * t);
    uint8_t b = (uint8_t)(b1 + (b2 - b1) * t);
    
    return (r << 11) | (g << 5) | b;
}

uint16_t SphereCoordinateSystem::adjustBrightness(uint16_t color, float brightness) {
    brightness = constrain(brightness, 0.0f, 1.0f);
    
    uint8_t r = ((color >> 11) & 0x1F) * brightness;
    uint8_t g = ((color >> 5) & 0x3F) * brightness;
    uint8_t b = (color & 0x1F) * brightness;
    
    return (r << 11) | (g << 5) | b;
}

SphereCoordinateSystem::ScreenPoint 
SphereCoordinateSystem::project3DPoint(float x, float y, float z, int centerX, int centerY, int radius, float rotateY) {
    ScreenPoint point;
    
    // Y軸回転適用
    float rotatedX = x * cosf(rotateY) - z * sinf(rotateY);
    float rotatedZ = x * sinf(rotateY) + z * cosf(rotateY);
    
    // 前面表示チェック
    point.visible = (rotatedX > 0);
    
    if (point.visible) {
        point.x = centerX + (int)(rotatedZ * radius * 0.9f);
        point.y = centerY - (int)(y * radius * 0.9f);
        point.intensity = rotatedX;  // 距離による強度
    } else {
        point.x = point.y = 0;
        point.intensity = 0.0f;
    }
    
    return point;
}

std::vector<SphereCoordinateSystem::ScreenPoint> 
SphereCoordinateSystem::get3DLine(float x1, float y1, float z1, float x2, float y2, float z2, 
                                 int centerX, int centerY, int radius, int segments) {
    std::vector<ScreenPoint> linePoints;
    linePoints.reserve(segments + 1);
    
    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments;
        float x = x1 + (x2 - x1) * t;
        float y = y1 + (y2 - y1) * t;
        float z = z1 + (z2 - z1) * t;
        
        ScreenPoint point = project3DPoint(x, y, z, centerX, centerY, radius);
        if (point.visible) {
            linePoints.push_back(point);
        }
    }
    
    return linePoints;
}

std::vector<SphereCoordinateSystem::ScreenPoint> 
SphereCoordinateSystem::getGridCircle(float radius3D, int centerX, int centerY, int radius, int points) {
    std::vector<ScreenPoint> circlePoints;
    circlePoints.reserve(points);
    
    for (int i = 0; i < points; i++) {
        float angle = (float)i / points * 2.0f * PI;
        float x = radius3D * cosf(angle);
        float y = 0.0f;  // XZ平面上の円
        float z = radius3D * sinf(angle);
        
        ScreenPoint point = project3DPoint(x, y, z, centerX, centerY, radius);
        if (point.visible) {
            circlePoints.push_back(point);
        }
    }
    
    return circlePoints;
}

// ---- LatitudeRingPattern 実装 ----

FallingRingOpeningPattern::FallingRingOpeningPattern()
    : baseBrightness_(1.0f), ringWidth_(4) {
    rings_ = {
        {TFT_RED,   0.00f, 0.60f, 1.0f},
        {TFT_GREEN, 0.45f, 0.60f, 1.0f},
        {TFT_BLUE,  0.85f, 0.65f, 1.0f}
    };
}

CRGB FallingRingOpeningPattern::colorFromRGB565(uint16_t color, float brightnessScale) {
    brightnessScale = std::max(0.0f, std::min(brightnessScale, 1.0f));

    uint8_t r = ((color >> 11) & 0x1F) << 3;
    uint8_t g = ((color >> 5) & 0x3F) << 2;
    uint8_t b = (color & 0x1F) << 3;

    CRGB result(r, g, b);
    result.nscale8((uint8_t)(brightnessScale * 255.0f));
    return result;
}

void FallingRingOpeningPattern::render(const PatternParams& params) {
    if (!sphereManager_) {
        return;
    }

    sphereManager_->clearAllLEDs();

    const float brightnessScale = std::max(0.0f, std::min(baseBrightness_ * params.brightness, 1.0f));

    for (const auto& ring : rings_) {
        const float localTime = (params.progress - ring.startProgress) / ring.duration;
        if (localTime < 0.0f || localTime > 1.0f) {
            continue;
        }

        // 滑らかな落下感を出すために smoothstep を利用
        float eased = localTime * localTime * (3.0f - 2.0f * localTime);
        float latitude = 90.0f - eased * 180.0f;

        CRGB color = colorFromRGB565(ring.color, brightnessScale * ring.brightnessScale);
        sphereManager_->drawLatitudeLine(latitude, color, ringWidth_);
    }

    sphereManager_->show();
}

LatitudeRingPattern::LatitudeRingPattern() 
    : speed_(1.0f), brightness_(1.0f), enableFlicker_(true), fadeStartLatitude_(-25.0f) {
    // デフォルトRGBリング設定
    rings_ = {
        {TFT_RED,   "R", 0.0f,  0.0f},
        {TFT_GREEN, "G", 0.15f, PI/3.0f},
        {TFT_BLUE,  "B", 0.35f, 2*PI/3.0f}
    };
}

void LatitudeRingPattern::render(const PatternParams& params) {
    if (!sphereManager_) return;  // LEDSphereManager必須
    
    // LED球体をクリア
    sphereManager_->clearAllLEDs();
    
    // 各色の光の輪を描画
    for (auto& ring : rings_) {
        // タイミング調整（揺らぎ付き）
        float flickerOffset = enableFlicker_ ? 
            0.05f * sinf(params.time * 6 * PI + ring.flickerPhase) : 0.0f;
        float ringProgress = params.progress - ring.delayOffset + flickerOffset;
        
        if (ringProgress > 0 && ringProgress <= 1.0f) {
            // 北極（+90度）から南極（-90度）への緯度移動
            float currentLat = 90.0f - (ringProgress * 180.0f * speed_);
            
            // フェードアウト計算
            float fadeStartProgress = (90.0f - fadeStartLatitude_) / 180.0f;
            float ringBrightness = brightness_;
            
            if (ringProgress > fadeStartProgress) {
                float fadeProgress = (ringProgress - fadeStartProgress) / (1.0f - fadeStartProgress);
                ringBrightness *= (1.0f - fadeProgress);
            }
            
            // 揺らぎ効果
            if (enableFlicker_) {
                ringBrightness *= 0.8f + 0.2f * sinf(params.time * 8 * PI + ring.flickerPhase);
            }
            
            drawLatitudeRing(currentLat, ring.color, ringBrightness, params);
        }
    }
    
    // LED出力
    sphereManager_->show();
}

void LatitudeRingPattern::drawLatitudeRing(float latitude, uint16_t color, float brightness, const PatternParams& params) {
    if (!sphereManager_) return;
    
    // RGB565→CRGB変換
    uint8_t r = ((color >> 11) & 0x1F) * 8;  // 5bit→8bit
    uint8_t g = ((color >> 5) & 0x3F) * 4;   // 6bit→8bit  
    uint8_t b = (color & 0x1F) * 8;          // 5bit→8bit
    
    CRGB ledColor = CRGB(r, g, b);
    ledColor.nscale8((uint8_t)(brightness * 255));
    
    // LEDSphereManagerを使って緯度線描画
    sphereManager_->drawLatitudeLine(latitude, ledColor, 2);  // 線幅2
}

// ---- YAxisRingPattern 実装 ----

YAxisRingPattern::YAxisRingPattern() 
    : globalSpeed_(1.0f), brightness_(1.0f), enablePulsing_(false), 
      enableColorRotation_(false), ringWidth_(2) {
    setupDefaultRings();
}

void YAxisRingPattern::setupDefaultRings() {
    rings_.clear();
    
    // x軸スタイルに合わせた0.5緑いリング設定
    // y軸周りの複数のリングを0.5緑色で統一（x軸の0.5緑に対応）
    CRGB halfGreen = CRGB(0, 127, 0);  // 0.5緑色
    rings_.push_back({60.0f, halfGreen, 1.0f, 0.0f});          // 北極寄り
    rings_.push_back({30.0f, halfGreen, 1.0f, PI/4});          // 中緯度北
    rings_.push_back({0.0f, halfGreen, 1.0f, PI/2});           // 赤道（メイン）
    rings_.push_back({-30.0f, halfGreen, 1.0f, 3*PI/4});       // 中緯度南
    rings_.push_back({-60.0f, halfGreen, 1.0f, PI});           // 南極寄り
}

void YAxisRingPattern::render(const PatternParams& params) {
    if (!sphereManager_) return;
    
    sphereManager_->clearAllLEDs();
    
    for (const auto& ring : rings_) {
        // 色計算
        CRGB color = calculateRingColor(ring, params);
        
        // 輝度計算
        float brightness = calculateRingBrightness(ring, params);
        
        // 最終色調整
        color.nscale8((uint8_t)(brightness * 255));
        
        // y軸周りのリング描画（特定緯度の全周）
        sphereManager_->drawLatitudeLine(ring.latitude, color, ringWidth_);
    }
    
    sphereManager_->show();
}

CRGB YAxisRingPattern::calculateRingColor(const Ring& ring, const PatternParams& params) const {
    CRGB color = ring.baseColor;
    
    // x軸スタイル：シンプルで一定の0.5緑色表示
    // 色回転は無効にして、安定した0.5緑色を維持
    if (enableColorRotation_) {
        // 軽微な色調変化のみ（x軸らしい安定性）
        float timePhase = params.time * globalSpeed_ * 0.1f + ring.phase;
        float brightnessVariation = 0.9f + 0.1f * sinf(timePhase);
        color.nscale8((uint8_t)(brightnessVariation * 255));
    }
    
    return color;
}

float YAxisRingPattern::calculateRingBrightness(const Ring& ring, const PatternParams& params) const {
    float brightness = brightness_;
    
    // x軸スタイル：控えめなパルス効果
    if (enablePulsing_) {
        // より穏やかな脈動（x軸の安定性に合わせて）
        float timePhase = params.time * globalSpeed_ * ring.speed * 0.5f + ring.phase;
        float pulseFactor = 0.7f + 0.3f * (sinf(timePhase) + 1.0f) / 2.0f;  // [0.7, 1.0] より狭い範囲
        brightness *= pulseFactor;
    }
    
    return brightness;
}

void YAxisRingPattern::addRing(float latitude, CRGB color, float speed, float phase) {
    rings_.push_back({latitude, color, speed, phase});
}

float YAxisRingPattern::getRingLatitude(size_t index) const {
    return (index < rings_.size()) ? rings_[index].latitude : 0.0f;
}

CRGB YAxisRingPattern::getRingColor(size_t index) const {
    return (index < rings_.size()) ? rings_[index].baseColor : CRGB::Black;
}

// ---- LongitudeLinePattern 実装 ----

LongitudeLinePattern::LongitudeLinePattern() 
    : speed_(1.0f), brightness_(1.0f), enableFlicker_(true), waveCount_(6) {
}

void LongitudeLinePattern::render(const PatternParams& params) {
    if (!sphereManager_) return;  // LEDSphereManager必須
    
    // LED球体をクリア
    sphereManager_->clearAllLEDs();
    
    // 経度線波動パターン
    const float waveSpeed = 2.0f * speed_;
    const int totalLongitudes = 12;  // 30度刻み
    
    for (int i = 0; i < totalLongitudes; i++) {
        float longitude = i * 30.0f;  // 0, 30, 60, ... 330度
        
        // 時差効果（経度による位相差）
        float phaseShift = (longitude / 360.0f) * 2.0f * PI;
        float wavePhase = params.progress * waveSpeed * 2.0f * PI + phaseShift;
        
        // 波の強度計算
        float waveIntensity = (sinf(wavePhase) + 1.0f) / 2.0f;  // [0, 1]
        
        // 虹色グラデーション（HSV→RGB）
        float hue = fmodf(params.progress * 2.0f + (longitude / 360.0f), 1.0f);
        CRGB color = CHSV((uint8_t)(hue * 255), 255, 255);  // FastLED HSV変換
        
        
        float finalBrightness = brightness_ * waveIntensity;
        if (enableFlicker_) {
            finalBrightness *= 0.8f + 0.2f * sinf(params.time * 6 * PI + phaseShift);
        }
        
        // 明度調整
        color.nscale8((uint8_t)(finalBrightness * 255));
        
        drawLongitudeLine(longitude, color, params);
    }
    
    // LED出力
    sphereManager_->show();
}

void LongitudeLinePattern::drawLongitudeLine(float longitude, const CRGB& color, const PatternParams& params) {
    if (!sphereManager_) return;
    
    // LEDSphereManagerを使って経度線描画
    sphereManager_->drawLongitudeLine(longitude, color, 1);  // 線幅1
}

// ---- CoordinateAxisPattern 実装 ----

CoordinateAxisPattern::CoordinateAxisPattern() 
    : brightness_(1.0f), showLabels_(true), showGrid_(true), 
      animateRotation_(true), rotationSpeed_(0.5f) {
}

void CoordinateAxisPattern::render(const PatternParams& params) {
    // 球体フレーム描画
    M5.Display.drawCircle(params.centerX, params.centerY, params.radius, TFT_DARKGREY);
    
    // 回転角度計算
    float rotateY = animateRotation_ ? params.progress * rotationSpeed_ * 2.0f * PI : 0.0f;
    
    // グリッド描画（背景）
    if (showGrid_) {
        drawGridLines(params);
    }
    
    // 座標軸描画
    const float axisLength = 1.0f;  // 正規化された軸長
    
    // X軸（赤） - 右方向
    drawAxis("X", axisLength, 0.0f, 0.0f, TFT_RED, params);
    drawAxis("", -axisLength, 0.0f, 0.0f, 
             SphereCoordinateSystem::adjustBrightness(TFT_RED, 0.5f), params);
    
    // Y軸（緑） - 上方向  
    drawAxis("Y", 0.0f, axisLength, 0.0f, TFT_GREEN, params);
    drawAxis("", 0.0f, -axisLength, 0.0f, 
             SphereCoordinateSystem::adjustBrightness(TFT_GREEN, 0.5f), params);
    
    // Z軸（青） - 手前方向
    drawAxis("Z", 0.0f, 0.0f, axisLength, TFT_BLUE, params);
    drawAxis("", 0.0f, 0.0f, -axisLength, 
             SphereCoordinateSystem::adjustBrightness(TFT_BLUE, 0.5f), params);
    
    // 原点マーカー
    drawOriginMarker(params);
    
    // 座標ラベル
    if (showLabels_) {
        drawCoordinateLabels(params);
    }
}

void CoordinateAxisPattern::drawAxis(const char* label, float x, float y, float z, uint16_t color, const PatternParams& params) {
    // 原点から指定座標への線分
    auto points = SphereCoordinateSystem::get3DLine(0.0f, 0.0f, 0.0f, x, y, z, 
                                                   params.centerX, params.centerY, params.radius, 20);
    
    uint16_t finalColor = SphereCoordinateSystem::adjustBrightness(color, brightness_);
    
    // 軸線描画
    for (size_t i = 0; i < points.size(); i++) {
        const auto& point = points[i];
        
        // 軸の太さ（原点に近いほど太い）
        float distanceFromOrigin = (float)i / points.size();
        int thickness = (distanceFromOrigin < 0.1f) ? 3 : 
                       (distanceFromOrigin < 0.5f) ? 2 : 1;
        
        for (int dx = -thickness/2; dx <= thickness/2; dx++) {
            for (int dy = -thickness/2; dy <= thickness/2; dy++) {
                int px = point.x + dx;
                int py = point.y + dy;
                if (px >= 0 && px < params.screenWidth && py >= 0 && py < params.screenHeight) {
                    M5.Display.drawPixel(px, py, finalColor);
                }
            }
        }
    }
    
    // 軸端点のラベル（正の方向のみ）
    if (strlen(label) > 0 && !points.empty()) {
        const auto& endPoint = points.back();
        M5.Display.setTextColor(finalColor);
        M5.Display.setTextSize(1);
        M5.Display.setCursor(endPoint.x + 5, endPoint.y - 4);
        M5.Display.print(label);
    }
}

void CoordinateAxisPattern::drawGridLines(const PatternParams& params) {
    uint16_t gridColor = SphereCoordinateSystem::adjustBrightness(TFT_DARKGREY, brightness_ * 0.3f);
    
    // XZ平面の同心円グリッド（Y=0）
    for (int r = 1; r <= 3; r++) {
        float radius3D = r * 0.33f;  // 0.33, 0.66, 1.0の同心円
        auto circlePoints = SphereCoordinateSystem::getGridCircle(radius3D, params.centerX, params.centerY, params.radius);
        
        for (const auto& point : circlePoints) {
            M5.Display.drawPixel(point.x, point.y, gridColor);
        }
    }
    
    // 放射状グリッド線（45度刻み）
    for (int angle = 0; angle < 360; angle += 45) {
        float angleRad = angle * PI / 180.0f;
        float x = cosf(angleRad);
        float z = sinf(angleRad);
        
        auto linePoints = SphereCoordinateSystem::get3DLine(0.0f, 0.0f, 0.0f, x, 0.0f, z, 
                                                           params.centerX, params.centerY, params.radius, 10);
        
        for (const auto& point : linePoints) {
            if (point.intensity > 0.3f) {  // 前面の線のみ
                M5.Display.drawPixel(point.x, point.y, gridColor);
            }
        }
    }
}

void CoordinateAxisPattern::drawOriginMarker(const PatternParams& params) {
    // 原点を白い十字で表示
    uint16_t originColor = SphereCoordinateSystem::adjustBrightness(TFT_WHITE, brightness_);
    
    auto originPoint = SphereCoordinateSystem::project3DPoint(0.0f, 0.0f, 0.0f, 
                                                             params.centerX, params.centerY, params.radius);
    
    if (originPoint.visible) {
        // 十字マーカー
        int size = 4;
        for (int i = -size; i <= size; i++) {
            M5.Display.drawPixel(originPoint.x + i, originPoint.y, originColor);
            M5.Display.drawPixel(originPoint.x, originPoint.y + i, originColor);
        }
        
        // 中心点強調
        M5.Display.fillCircle(originPoint.x, originPoint.y, 2, originColor);
    }
}

void CoordinateAxisPattern::drawCoordinateLabels(const PatternParams& params) {
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextSize(1);
    
    // 画面上部に座標系情報
    M5.Display.setCursor(5, 5);
    M5.Display.print("Coordinate System");
    
    M5.Display.setCursor(5, 15);
    M5.Display.print("X:Red  Y:Green  Z:Blue");
    
    M5.Display.setCursor(5, 25);
    M5.Display.printf("Origin: Center");
    
    // 右手系説明
    M5.Display.setCursor(5, params.screenHeight - 20);
    M5.Display.print("Right-handed system");
    
    M5.Display.setCursor(5, params.screenHeight - 10);
    if (animateRotation_) {
        M5.Display.printf("Rotating: %.1fx", rotationSpeed_);
    } else {
        M5.Display.print("Static view");
    }
}

// ---- PatternGenerator 実装 ----

PatternGenerator::PatternGenerator() : currentPatternName_("") {
    defaultParams_.screenWidth = 128;
    defaultParams_.screenHeight = 128;
    defaultParams_.centerX = 64;
    defaultParams_.centerY = 64;
    defaultParams_.radius = 60;
}

std::unique_ptr<IPattern> PatternGenerator::createPattern(const std::string& patternName) {
    if (patternName == "latitude_rings") {
        return std::unique_ptr<LatitudeRingPattern>(new LatitudeRingPattern());
    } else if (patternName == "ring_fall_opening") {
        return std::unique_ptr<FallingRingOpeningPattern>(new FallingRingOpeningPattern());
    } else if (patternName == "x_axis_half_green_rings") {
        return std::unique_ptr<YAxisRingPattern>(new YAxisRingPattern());
    } else if (patternName == "longitude_lines") {
        return std::unique_ptr<LongitudeLinePattern>(new LongitudeLinePattern());
    } else if (patternName == "coordinate_axis") {
        return std::unique_ptr<CoordinateAxisPattern>(new CoordinateAxisPattern());
    } else if (patternName == "spiral_trajectory") {
        return std::unique_ptr<SpiralTrajectoryPattern>(new SpiralTrajectoryPattern());
    } else if (patternName == "spherical_wave") {
        return std::unique_ptr<SphericalWavePattern>(new SphericalWavePattern());
    }
    return nullptr;
}

void PatternGenerator::renderPattern(const std::string& patternName, float progress, float time,
                                   const PatternParams* customParams) {
    auto pattern = createPattern(patternName);
    if (pattern) {
        // カスタムパラメータまたはデフォルトパラメータを使用
        PatternParams params = customParams ? *customParams : defaultParams_;
        params.progress = progress;
        params.time = time;
        
        // 背景クリア
        M5.Display.fillScreen(TFT_BLACK);
        
        // パターン描画
        pattern->render(params);
        currentPatternName_ = patternName;
    }
}

std::vector<std::string> PatternGenerator::getAvailablePatterns() const {
    return {
        "latitude_rings",
        "ring_fall_opening",
        "x_axis_half_green_rings",
        "longitude_lines", 
        "coordinate_axis",
        "spiral_trajectory",
        "spherical_wave"
    };
}

PatternParams PatternGenerator::getDefaultParams() const {
    return defaultParams_;
}

void PatternGenerator::setDefaultParams(const PatternParams& params) {
    defaultParams_ = params;
}

// 未実装クラスのプレースホルダー
SpiralTrajectoryPattern::SpiralTrajectoryPattern() : speed_(1.0f), brightness_(1.0f), spiralTurns_(3.0f), trailLength_(20) {}
void SpiralTrajectoryPattern::render(const PatternParams& params) {
    // TODO: 実装予定
}

SphericalWavePattern::SphericalWavePattern() : speed_(1.0f), brightness_(1.0f), waveCount_(3) {}
void SphericalWavePattern::render(const PatternParams& params) {
    // TODO: 実装予定
}

} // namespace ProceduralPattern
