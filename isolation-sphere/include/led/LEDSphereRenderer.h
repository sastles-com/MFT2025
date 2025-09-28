/**
 * @file LEDSphereRenderer.h
 * @brief LED球体統合レンダリングシステム
 * 
 * 画像系（10fps）とProceduralPatternGenerator（30fps）の統合アーキテクチャ
 * 800個のLED配列への最終出力を共通化し、パフォーマンス最適化を実現
 */

#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <vector>
#include <memory>
#include "pattern/ProceduralPatternGenerator.h"

/**
 * @brief LED球体レンダリング統合システム
 */
namespace LEDSphere {

/**
 * @brief LED位置情報（led_layout.csvから読み込み）
 */
struct LEDPosition {
    uint16_t faceID;        // LED ID (0-799)
    uint8_t strip;          // ストリップ番号 (0-3)
    uint8_t strip_num;      // ストリップ内位置
    float x, y, z;          // 3D正規化座標 [-1.0, 1.0]
    
    // キャッシュ用UV座標（回転変換後）
    mutable float u, v;     // UV座標 [0.0, 1.0]
    mutable bool uvDirty;   // UV座標更新フラグ
    
    LEDPosition() : faceID(0), strip(0), strip_num(0), x(0), y(0), z(0), u(0), v(0), uvDirty(true) {}
};

/**
 * @brief レンダリングソース種別
 */
enum class RenderSource {
    PROCEDURAL_PATTERN,     // ProceduralPatternGenerator（30fps高速）
    IMAGE_TEXTURE,          // 画像テクスチャ（10fps標準）
    HYBRID                  // ハイブリッド（画像＋プロシージャル重ね合わせ）
};

/**
 * @brief レンダリングパラメータ
 */
struct RenderParams {
    RenderSource source;
    float progress;         // アニメーション進行度 [0.0-1.0]
    float time;            // 経過時間（秒）
    float brightness;      // 全体輝度 [0.0-1.0]
    
    // 姿勢パラメータ（IMU + オフセット）
    float quaternionW, quaternionX, quaternionY, quaternionZ;
    float latitudeOffset, longitudeOffset;  // 緯度・経度オフセット（度）
    
    // プロシージャルパターン専用
    std::string patternName;
    
    // 画像テクスチャ専用
    const uint8_t* imageData;   // JPEG/RGB画像データ
    size_t imageSize;
    uint16_t imageWidth, imageHeight;
    
    RenderParams() : source(RenderSource::PROCEDURAL_PATTERN), progress(0.0f), time(0.0f), 
                     brightness(1.0f), quaternionW(1.0f), quaternionX(0.0f), quaternionY(0.0f), quaternionZ(0.0f),
                     latitudeOffset(0.0f), longitudeOffset(0.0f), patternName(""), 
                     imageData(nullptr), imageSize(0), imageWidth(0), imageHeight(0) {}
};

/**
 * @brief LED球体レンダラー（統合システム）
 */
class LEDSphereRenderer {
private:
    static constexpr size_t LED_COUNT = 800;
    static constexpr size_t STRIP_COUNT = 4;
    
    // LED配置データ
    std::vector<LEDPosition> ledPositions_;
    
    // FastLED出力バッファ
    CRGB leds_[LED_COUNT];
    
    // プロシージャルパターンジェネレータ
    std::unique_ptr<ProceduralPattern::PatternGenerator> patternGenerator_;
    
    // パフォーマンス最適化用
    bool enableSparseRendering_;    // スパースレンダリング（プロシージャル用）
    uint32_t lastFullRenderMs_;     // 最後の全LED更新時刻
    uint32_t lastSparseRenderMs_;   // 最後の部分LED更新時刻
    
    // UV座標キャッシュ
    bool uvCacheDirty_;
    float lastQuaternionW_, lastQuaternionX_, lastQuaternionY_, lastQuaternionZ_;
    float lastLatOffset_, lastLonOffset_;
    
public:
    LEDSphereRenderer();
    ~LEDSphereRenderer();
    
    // 初期化
    bool initialize();
    bool loadLEDLayout(const char* csvPath);
    
    // レンダリング実行
    void render(const RenderParams& params);
    void show();    // FastLED出力実行
    
    // パフォーマンス設定
    void setSparseRenderingEnabled(bool enabled) { enableSparseRendering_ = enabled; }
    void setTargetFPS(uint8_t fps);
    
    // 統計情報
    float getLastRenderTimeMs() const;
    uint16_t getActiveLEDCount() const;  // 最後のレンダリングで点灯したLED数
    
private:
    // 座標変換（IMU姿勢反映）
    void updateUVCoordinates(const RenderParams& params);
    void transformPosition(float& x, float& y, float& z, const RenderParams& params) const;
    void cartesianToUV(float x, float y, float z, float& u, float& v) const;
    
    // レンダリング実装
    void renderProceduralPattern(const RenderParams& params);
    void renderImageTexture(const RenderParams& params);
    void renderHybrid(const RenderParams& params);
    
    // スパースレンダリング（プロシージャル高速化）
    void renderSparsePattern(const RenderParams& params);
    std::vector<uint16_t> getActivePatternLEDs(const std::string& patternName, const RenderParams& params);
    
    // 画像処理（10fps標準レンダリング）
    CRGB sampleImageTexture(float u, float v, const RenderParams& params) const;
    void decodeImageToTexture(const RenderParams& params);
    
    // FastLED出力最適化
    void clearLEDs();
    void setLED(uint16_t faceID, CRGB color);
    void setLEDRange(uint16_t startID, uint16_t count, CRGB color);
};

/**
 * @brief 高速パターンレンダリング特化クラス
 * 
 * ProceduralPatternGeneratorの30fps動作に特化
 * 必要なLEDのみ計算・更新してパフォーマンス向上
 */
class FastPatternRenderer {
private:
    LEDSphereRenderer* sphereRenderer_;
    
public:
    FastPatternRenderer(LEDSphereRenderer* renderer) : sphereRenderer_(renderer) {}
    
    // 座標軸インジケータ（最小3点のみ）
    void renderCoordinateAxis(const RenderParams& params);
    
    // 緯度線光の輪（1本の輪のみ）
    void renderLatitudeRing(float latitude, CRGB color, const RenderParams& params);
    
    // 経度線波動（12本の線のみ）
    void renderLongitudeWaves(const RenderParams& params);
    
    // スパース描画（必要なLEDのみ）
    void renderSparsePattern(const std::string& patternName, const RenderParams& params);
};

/**
 * @brief 標準画像レンダリング特化クラス
 * 
 * 画像テクスチャの10fps動作に特化
 * 全800LEDの計算が必要だが品質重視
 */
class ImageTextureRenderer {
private:
    LEDSphereRenderer* sphereRenderer_;
    
    // 画像キャッシュ
    std::vector<uint8_t> textureCache_;
    uint16_t cacheWidth_, cacheHeight_;
    
public:
    ImageTextureRenderer(LEDSphereRenderer* renderer) : sphereRenderer_(renderer), cacheWidth_(0), cacheHeight_(0) {}
    
    // JPEG画像の全LED描画
    void renderFullTexture(const RenderParams& params);
    
    // バイリニア補間サンプリング
    CRGB sampleTexture(float u, float v) const;
    
    // 画像キャッシュ管理
    bool loadImageToCache(const uint8_t* imageData, size_t imageSize);
    void clearCache();
};

/**
 * @brief パフォーマンス統計クラス
 */
class RenderStats {
private:
    uint32_t frameCount_;
    uint32_t totalRenderTimeMs_;
    uint32_t lastFPSUpdateMs_;
    float currentFPS_;
    
public:
    RenderStats() : frameCount_(0), totalRenderTimeMs_(0), lastFPSUpdateMs_(0), currentFPS_(0.0f) {}
    
    void recordFrame(uint32_t renderTimeMs);
    float getCurrentFPS() const { return currentFPS_; }
    float getAverageRenderTime() const;
    void reset();
};

} // namespace LEDSphere