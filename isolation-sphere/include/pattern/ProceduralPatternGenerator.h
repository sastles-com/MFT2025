#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#if !defined(UNIT_TEST)
#include <Arduino.h>
#include <M5Unified.h>
#include <FastLED.h>
#else
struct CRGB;
#endif

// LEDSphereManager統合
namespace LEDSphere {
    class LEDSphereManager;
    struct PostureParams;
}

/**
 * @brief プロシージャルパターン生成システム 🎨
 * 
 * 球体LED用の数学的パターンを動的に生成・管理するクラス群
 * LEDSphereManagerと統合してハードウェアLED制御を実現
 * パターンの追加・切り替え・パラメータ調整を統一インターフェースで提供
 */

namespace ProceduralPattern {

// 前方宣言
struct PatternParams;
class IPattern;
class PatternGenerator;

/**
 * @brief パターン実行時パラメータ
 */
struct PatternParams {
    float progress;        // 進行度 [0.0 - 1.0]
    float time;           // 経過時間 (秒)
    int screenWidth;      // 画面幅
    int screenHeight;     // 画面高さ
    int centerX;          // 中心X座標
    int centerY;          // 中心Y座標
    int radius;           // 球体半径
    
    // 拡張パラメータ
    float speed;          // アニメーション速度倍率
    float brightness;     // 明度倍率 [0.0 - 1.0]
    bool enableFlicker;   // 揺らぎ効果の有無
    
    PatternParams() : 
        progress(0.0f), time(0.0f), 
        screenWidth(128), screenHeight(128),
        centerX(64), centerY(64), radius(60),
        speed(1.0f), brightness(1.0f), enableFlicker(true) {}
};

/**
 * @brief パターンの基底インターフェース
 */
class IPattern {
protected:
    LEDSphere::LEDSphereManager* sphereManager_ = nullptr;  // LED球体制御
    
public:
    virtual ~IPattern() = default;
    
    /**
     * @brief LEDSphereManager設定
     * @param manager LED球体制御オブジェクト
     */
    void setSphereManager(LEDSphere::LEDSphereManager* manager) {
        sphereManager_ = manager;
    }
    
    // パターン描画の主関数
    virtual void render(const PatternParams& params) = 0;
    
    // パターン情報
    virtual const char* getName() const = 0;
    virtual const char* getDescription() const = 0;
    virtual float getDuration() const { return 3.0f; } // デフォルト3秒
    
    // パラメータ調整
    virtual void setSpeed(float speed) {}
    virtual void setBrightness(float brightness) {}
    virtual void setFlicker(bool enable) {}
};

/**
 * @brief 緯度線ベースパターン（光の輪降下系）
 */
class LatitudeRingPattern : public IPattern {
private:
    struct ColorRing {
        uint16_t color;
        const char* name;
        float delayOffset;
        float flickerPhase;
    };
    
    std::vector<ColorRing> rings_;
    float speed_;
    float brightness_;
    bool enableFlicker_;
    
public:
    LatitudeRingPattern();
    ~LatitudeRingPattern() = default;
    
    void render(const PatternParams& params) override;
    const char* getName() const override { return "Latitude Ring Descent"; }
    const char* getDescription() const override { return "RGB rings descending from North to South Pole"; }
    
    void setSpeed(float speed) override { speed_ = speed; }
    void setBrightness(float brightness) override { brightness_ = brightness; }
    void setFlicker(bool enable) override { enableFlicker_ = enable; }
    
    // 専用設定
    void setRingColors(const std::vector<uint16_t>& colors, const std::vector<float>& delays);
    void setFadeLatitude(float latitude) { fadeStartLatitude_ = latitude; }
    
private:
    float fadeStartLatitude_;
    void drawLatitudeRing(float latitude, uint16_t color, float brightness, const PatternParams& params);
};

/**
 * @brief 経度線ベースパターン（同経度同色系）🌊 
 */
class LongitudeLinePattern : public IPattern {
private:
    float speed_;
    float brightness_;
    bool enableFlicker_;
    int waveCount_;
    
public:
    LongitudeLinePattern();
    ~LongitudeLinePattern() = default;
    
    void render(const PatternParams& params) override;
    const char* getName() const override { return "Longitude Wave Flow"; }
    const char* getDescription() const override { return "Color waves flowing along longitude lines"; }
    
    void setSpeed(float speed) override { speed_ = speed; }
    void setBrightness(float brightness) override { brightness_ = brightness; }
    void setFlicker(bool enable) override { enableFlicker_ = enable; }
    
    // 専用設定
    void setWaveCount(int count) { waveCount_ = count; }
    
private:
    void drawLongitudeLine(float longitude, const CRGB& color, const PatternParams& params);
};

/**
 * @brief オープニング用リング降下パターン
 */
class FallingRingOpeningPattern : public IPattern {
private:
    struct RingTimeline {
        uint16_t color;
        float startProgress;
        float duration;
        float brightnessScale;
    };

    std::vector<RingTimeline> rings_;
    float baseBrightness_;
    uint8_t ringWidth_;

public:
    FallingRingOpeningPattern();
    ~FallingRingOpeningPattern() = default;

    void render(const PatternParams& params) override;
    const char* getName() const override { return "Falling Ring Opening"; }
    const char* getDescription() const override { return "Three colored rings descend from north to south"; }
    float getDuration() const override { return 3.5f; }

    void setBrightness(float brightness) override { baseBrightness_ = brightness; }
    void setRingWidth(uint8_t width) { ringWidth_ = width; }

private:
    static CRGB colorFromRGB565(uint16_t color, float brightnessScale);
};

/**
 * @brief Y軸周りリングパターン（緯度線リング群）🌍
 * 
 * 複数の緯度でリング状のLEDパターンを描画し、
 * 各リングが独立したアニメーション（脈動・色変化）を実行
 */
class YAxisRingPattern : public IPattern {
private:
    struct Ring {
        float latitude;     // 緯度（-90度〜+90度）
        CRGB baseColor;    // 基準色
        float speed;       // 個別アニメーション速度
        float phase;       // 位相オフセット
    };
    
    std::vector<Ring> rings_;
    float globalSpeed_;
    float brightness_;
    bool enablePulsing_;
    bool enableColorRotation_;
    uint8_t ringWidth_;
    
public:
    YAxisRingPattern();
    ~YAxisRingPattern() = default;
    
    void render(const PatternParams& params) override;
    const char* getName() const override { return "X-Axis Half Green Rings"; }
    const char* getDescription() const override { return "Half green rings around Y-axis representing X-axis system"; }
    float getDuration() const override { return 8.0f; }
    
    void setSpeed(float speed) override { globalSpeed_ = speed; }
    void setBrightness(float brightness) override { brightness_ = brightness; }
    
    // 専用設定
    void setPulsingEnabled(bool enable) { enablePulsing_ = enable; }
    void setColorRotationEnabled(bool enable) { enableColorRotation_ = enable; }
    void setRingWidth(uint8_t width) { ringWidth_ = width; }
    void addRing(float latitude, CRGB color, float speed = 1.0f, float phase = 0.0f);
    void clearRings() { rings_.clear(); }
    
    // テスト用アクセサ
    size_t getRingCount() const { return rings_.size(); }
    float getRingLatitude(size_t index) const;
    CRGB getRingColor(size_t index) const;

private:
    void setupDefaultRings();
    CRGB calculateRingColor(const Ring& ring, const PatternParams& params) const;
    float calculateRingBrightness(const Ring& ring, const PatternParams& params) const;
};

/**
 * @brief 螺旋軌道パターン
 */
class SpiralTrajectoryPattern : public IPattern {
private:
    float speed_;
    float brightness_;
    float spiralTurns_;
    int trailLength_;
    
public:
    SpiralTrajectoryPattern();
    ~SpiralTrajectoryPattern() = default;
    
    void render(const PatternParams& params) override;
    const char* getName() const override { return "Spiral Trajectory"; }
    const char* getDescription() const override { return "Spiral path from South to North Pole"; }
    
    void setSpeed(float speed) override { speed_ = speed; }
    void setBrightness(float brightness) override { brightness_ = brightness; }
    
    void setSpiralTurns(float turns) { spiralTurns_ = turns; }
    void setTrailLength(int length) { trailLength_ = length; }
};

/**
 * @brief 球面波動パターン
 */
class SphericalWavePattern : public IPattern {
private:
    float speed_;
    float brightness_;
    int waveCount_;
    
public:
    SphericalWavePattern();
    ~SphericalWavePattern() = default;
    
    void render(const PatternParams& params) override;
    const char* getName() const override { return "Spherical Wave"; }
    const char* getDescription() const override { return "Concentric waves on sphere surface"; }
    
    void setSpeed(float speed) override { speed_ = speed; }
    void setBrightness(float brightness) override { brightness_ = brightness; }
    
    void setWaveCount(int count) { waveCount_ = count; }
};

/**
 * @brief 座標軸インジケータパターン（メンテナンス・デバッグ用）🧭
 * LED基盤システムとの統合対応版
 */
class CoordinateAxisPattern : public IPattern {
private:
    float brightness_;
    bool showLabels_;
    bool showGrid_;
    bool animateRotation_;
    float rotationSpeed_;
    bool useLEDSphere_;         // LED基盤システム使用フラグ
    
public:
    CoordinateAxisPattern();
    ~CoordinateAxisPattern() = default;
    
    void render(const PatternParams& params) override;
    const char* getName() const override { return "Coordinate Axis"; }
    const char* getDescription() const override { return "XYZ axis indicators with grid and labels (LED Sphere compatible)"; }
    
    void setBrightness(float brightness) override { brightness_ = brightness; }
    
    // 専用設定
    void setShowLabels(bool show) { showLabels_ = show; }
    void setShowGrid(bool show) { showGrid_ = show; }
    void setAnimateRotation(bool animate) { animateRotation_ = animate; }
    void setRotationSpeed(float speed) { rotationSpeed_ = speed; }
    void setUseLEDSphere(bool use) { useLEDSphere_ = use; }
    
private:
    // LCD描画用（従来方式）
    void renderToLCD(const PatternParams& params);
    void drawAxis(const char* label, float x, float y, float z, uint16_t color, const PatternParams& params);
    void drawGridLines(const PatternParams& params);
    void drawOriginMarker(const PatternParams& params);
    void drawCoordinateLabels(const PatternParams& params);
    
    // LED球体描画用（LED基盤システム使用）
    void renderToLEDSphere(const PatternParams& params);
};

/**
 * @brief パターン生成・管理クラス（ファクトリーパターン使用）
 */
class PatternGenerator {
private:
    std::string currentPatternName_;
    PatternParams defaultParams_;
    
public:
    PatternGenerator();
    ~PatternGenerator() = default;
    
    // ファクトリーメソッド
    std::unique_ptr<IPattern> createPattern(const std::string& patternName);
    
    // 描画実行
    void renderPattern(const std::string& patternName, float progress, float time = 0.0f,
                      const PatternParams* customParams = nullptr);
    
    // パラメータ管理
    PatternParams getDefaultParams() const;
    void setDefaultParams(const PatternParams& params);
    
    // 利用可能なパターン一覧
    std::vector<std::string> getAvailablePatterns() const;
    
    // 現在のパターン名
    const std::string& getCurrentPatternName() const { return currentPatternName_; }
};

/**
 * @brief 球面座標系ユーティリティ
 */
class SphereCoordinateSystem {
public:
    struct SphericalCoord {
        float theta;  // 経度 [-π, π]
        float phi;    // 緯度 [-π/2, π/2]
    };
    
    struct UVCoord {
        float u;      // u座標 [0.0, 1.0]
        float v;      // v座標 [0.0, 1.0]
    };
    
    struct ScreenPoint {
        int x, y;
        float intensity;
        bool visible;
    };
    
    // 座標変換
    static SphericalCoord cartesianToSpherical(float x, float y, float z);
    static UVCoord sphericalToUV(const SphericalCoord& coord);
    static ScreenPoint sphericalToScreen(const SphericalCoord& coord, int centerX, int centerY, int radius);
    
    // 緯度線・経度線計算
    static std::vector<ScreenPoint> getLatitudeLine(float latitude, int centerX, int centerY, int radius, int points = 64);
    static std::vector<ScreenPoint> getLongitudeLine(float longitude, int centerX, int centerY, int radius, int points = 64);
    
    // 色補間
    static uint16_t interpolateColor(uint16_t color1, uint16_t color2, float t);
    static uint16_t adjustBrightness(uint16_t color, float brightness);
    
    // 座標軸・グリッド描画用
    static std::vector<ScreenPoint> get3DLine(float x1, float y1, float z1, float x2, float y2, float z2, 
                                             int centerX, int centerY, int radius, int segments = 20);
    static ScreenPoint project3DPoint(float x, float y, float z, int centerX, int centerY, int radius, float rotateY = 0.0f);
    static std::vector<ScreenPoint> getGridCircle(float radius3D, int centerX, int centerY, int radius, int points = 32);
};

} // namespace ProceduralPattern
