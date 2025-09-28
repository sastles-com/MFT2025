/**
 * @file LEDSphereManager.h
 * @brief LED球体統合制御システム - 基盤システム中核クラス
 * 
 * ProceduralPatternGeneratorと画像系の共通基盤として動作
 * 800個LED配置・座標変換・FastLED出力を統合管理
 */

#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <vector>
#include <memory>
#include <map>

namespace LEDSphere {

/**
 * @brief LED物理位置情報（led_layout.csvデータ）
 */
struct LEDPosition {
    uint16_t faceID;        // LED ID (0-799)
    uint8_t strip;          // ストリップ番号 (0-3) 
    uint8_t strip_num;      // ストリップ内位置
    float x, y, z;          // 3D正規化座標 [-1.0, 1.0]
    
    LEDPosition() : faceID(0), strip(0), strip_num(0), x(0), y(0), z(0) {}
    LEDPosition(uint16_t id, uint8_t s, uint8_t sn, float px, float py, float pz)
        : faceID(id), strip(s), strip_num(sn), x(px), y(py), z(pz) {}
};

/**
 * @brief UV座標（球面座標系）
 */
struct UVCoordinate {
    float u, v;             // UV座標 [0.0, 1.0]
    bool valid;             // 有効性フラグ
    
    UVCoordinate() : u(0), v(0), valid(false) {}
    UVCoordinate(float pu, float pv) : u(pu), v(pv), valid(true) {}
};

/**
 * @brief 姿勢・オフセット情報
 */
struct PostureParams {
    float quaternionW, quaternionX, quaternionY, quaternionZ;  // IMU姿勢
    float latitudeOffset, longitudeOffset;                     // UI制御オフセット（度）
    
    PostureParams() : quaternionW(1.0f), quaternionX(0.0f), quaternionY(0.0f), quaternionZ(0.0f),
                      latitudeOffset(0.0f), longitudeOffset(0.0f) {}
};

/**
 * @brief パフォーマンス統計情報
 */
struct PerformanceStats {
    float currentFPS;
    float averageRenderTime;
    uint32_t frameCount;
    uint16_t activeLEDCount;
    size_t memoryUsage;
    
    PerformanceStats() : currentFPS(0), averageRenderTime(0), frameCount(0), 
                        activeLEDCount(0), memoryUsage(0) {}
};

// 前方宣言
class LEDLayoutManager;
class SphereCoordinateTransform; 
class FastLEDController;
class UVCoordinateCache;
class PerformanceMonitor;

/**
 * @brief LED球体統合管理クラス - システムの中核
 * 
 * ProceduralPatternGeneratorと画像系の共通基盤として動作
 * LED配置・座標変換・出力制御を統合管理
 */
class LEDSphereManager {
public:
    static constexpr size_t LED_COUNT = 800;
    static constexpr size_t STRIP_COUNT = 4;
    static constexpr size_t LEDS_PER_STRIP = 200;

private:
    // コンポーネント管理
    std::unique_ptr<LEDLayoutManager> layoutManager_;
    std::unique_ptr<SphereCoordinateTransform> coordinateTransform_;
    std::unique_ptr<FastLEDController> ledController_;
    std::unique_ptr<UVCoordinateCache> uvCache_;
    std::unique_ptr<PerformanceMonitor> performanceMonitor_;
    
    // システム状態
    bool initialized_;
    bool sparseMode_;           // スパース描画モード（30fps用）
    uint8_t targetFPS_;         // 目標FPS
    PostureParams lastPosture_; // 前回姿勢（変化検出用）

public:
    LEDSphereManager();
    ~LEDSphereManager();
    
    // ========== 初期化・設定 ==========
    
    /**
     * @brief システム初期化
     * @param csvPath LED配置CSVファイルパス
     * @return 初期化成功フラグ
     */
    bool initialize(const char* csvPath = "/led_layout.csv");
    
    /**
     * @brief パフォーマンスモード設定
     * @param sparse true:スパース描画(30fps), false:フル描画(10fps)
     */
    void setSparseMode(bool sparse) { sparseMode_ = sparse; }
    
    /**
     * @brief 目標FPS設定
     * @param fps 目標フレームレート
     */
    void setTargetFPS(uint8_t fps) { targetFPS_ = fps; }
    
    // ========== 姿勢・座標制御 ==========
    
    /**
     * @brief IMU姿勢更新
     * @param qw,qx,qy,qz クォータニオン成分
     */
    void setIMUPosture(float qw, float qx, float qy, float qz);
    
    /**
     * @brief UI制御オフセット設定
     * @param latOffset 緯度オフセット（度）
     * @param lonOffset 経度オフセット（度）
     */
    void setUIOffset(float latOffset, float lonOffset);
    
    /**
     * @brief 姿勢パラメータ一括設定
     * @param params 姿勢・オフセット情報
     */
    void setPostureParams(const PostureParams& params);
    
    // ========== LED制御 ==========
    
    /**
     * @brief 個別LED設定
     * @param faceID LED ID (0-799)
     * @param color RGB色
     */
    void setLED(uint16_t faceID, CRGB color);
    
    /**
     * @brief UV座標によるLED設定
     * @param u,v UV座標 [0.0-1.0]
     * @param color RGB色
     * @param radius 影響半径（近傍LEDも点灯）
     */
    void setLEDByUV(float u, float v, CRGB color, float radius = 0.02f);
    
    /**
     * @brief 全LED消去
     */
    void clearAllLEDs();
    
    /**
     * @brief 全体輝度設定
     * @param brightness 輝度 [0-255]
     */
    void setBrightness(uint8_t brightness);
    
    /**
     * @brief LED出力実行（FastLED.show()）
     */
    void show();
    
    // ========== 高速パターン描画（ProceduralPattern用）==========
    
    /**
     * @brief 座標軸インジケータ描画
     * @param showGrid グリッド表示フラグ
     * @param brightness 輝度係数 [0.0-1.0]
     */
    void drawCoordinateAxis(bool showGrid = true, float brightness = 1.0f);
    
    /**
     * @brief 緯度線描画
     * @param latitude 緯度（度）[-90, 90]
     * @param color RGB色
     * @param lineWidth 線幅（LED個数）
     */
    void drawLatitudeLine(float latitude, CRGB color, uint8_t lineWidth = 1);
    
    /**
     * @brief 経度線描画  
     * @param longitude 経度（度）[-180, 180]
     * @param color RGB色
     * @param lineWidth 線幅（LED個数）
     */
    void drawLongitudeLine(float longitude, CRGB color, uint8_t lineWidth = 1);
    
    /**
     * @brief スパースパターン描画（高速）
     * @param points LED ID→色のマップ
     */
    void drawSparsePattern(const std::map<uint16_t, CRGB>& points);
    
    // ========== 検索・クエリ機能 ==========
    
    /**
     * @brief 最寄りLED検索（UV座標）
     * @param u,v UV座標
     * @return LED ID（見つからない場合は LED_COUNT）
     */
    uint16_t findClosestLED(float u, float v) const;
    
    /**
     * @brief 範囲内LED検索
     * @param u,v 中心UV座標
     * @param radius 検索半径
     * @return LED IDリスト
     */
    std::vector<uint16_t> findLEDsInRange(float u, float v, float radius) const;
    
    /**
     * @brief 3D座標→UV座標変換
     * @param x,y,z 3D座標
     * @return UV座標
     */
    UVCoordinate transformToUV(float x, float y, float z) const;
    
    /**
     * @brief LED位置情報取得
     * @param faceID LED ID
     * @return LED位置情報（無効な場合は nullptr）
     */
    const LEDPosition* getLEDPosition(uint16_t faceID) const;
    
    // ========== パフォーマンス監視 ==========
    
    /**
     * @brief フレーム開始（性能測定用）
     */
    void frameStart();
    
    /**
     * @brief フレーム終了（性能測定用）
     */
    void frameEnd();
    
    /**
     * @brief パフォーマンス統計取得
     * @return 統計情報
     */
    PerformanceStats getPerformanceStats() const;
    
    /**
     * @brief 現在のFPS取得
     * @return FPS値
     */
    float getCurrentFPS() const;
    
    /**
     * @brief アクティブLED数取得
     * @return 点灯LED数
     */
    uint16_t getActiveLEDCount() const;
    
    // ========== デバッグ・ユーティリティ ==========
    
    /**
     * @brief システム状態出力
     */
    void printSystemStatus() const;
    
    /**
     * @brief メモリ使用量出力
     */
    void printMemoryUsage() const;
    
    /**
     * @brief LED配置情報出力（デバッグ用）
     * @param maxCount 出力する最大LED数
     */
    void printLEDLayout(size_t maxCount = 10) const;

private:
    // 内部初期化メソッド
    bool initializeFastLED();
    bool initializeComponents();
    
    // 姿勢変化検出
    bool hasPostureChanged(const PostureParams& params) const;
    
    // UV座標更新制御
    void updateUVCacheIfNeeded();
};

/**
 * @brief LED基盤システム・シングルトンアクセス
 * ProceduralPatternGeneratorからの簡易アクセス用
 */
class SpherePatternInterface {
private:
    static LEDSphereManager* instance_;

public:
    /**
     * @brief シングルトンインスタンス取得
     * @return LEDSphereManager インスタンス
     */
    static LEDSphereManager* getInstance();
    
    /**
     * @brief システム初期化（最初に1回呼び出し）
     * @param csvPath LED配置CSVパス  
     * @return 初期化成功フラグ
     */
    static bool initialize(const char* csvPath = "/led_layout.csv");
    
    /**
     * @brief システム終了処理
     */
    static void shutdown();
};

} // namespace LEDSphere