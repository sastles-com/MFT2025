/**
 * @file LEDLayoutManager.h  
 * @brief LED物理配置データ管理クラス
 * 
 * led_layout.csv読み込み・LED位置管理・検索機能を提供
 */

#pragma once

#include <Arduino.h>
#include <vector>
#include <map>
#include "led/LEDSphereManager.h"

namespace LEDSphere {

/**
 * @brief LED配置データ管理クラス
 * 
 * CSV形式のLED配置データを読み込み、効率的な検索・アクセス機能を提供
 */
class LEDLayoutManager {
private:
    std::vector<LEDPosition> positions_;        // 全LED位置データ
    std::map<uint16_t, size_t> faceIdToIndex_; // FaceID→インデックス高速マップ
    
    // 検索最適化用空間分割データ
    struct SpatialGrid {
        static constexpr int GRID_SIZE = 32;    // 32×32グリッド
        std::vector<std::vector<uint16_t>> grid_; // グリッド→LED IDリスト
        
        SpatialGrid() : grid_(GRID_SIZE * GRID_SIZE) {}
        
        int getGridIndex(float u, float v) const {
            int gu = static_cast<int>(u * GRID_SIZE);
            int gv = static_cast<int>(v * GRID_SIZE);
            gu = constrain(gu, 0, GRID_SIZE - 1);
            gv = constrain(gv, 0, GRID_SIZE - 1);
            return gv * GRID_SIZE + gu;
        }
    };
    
    mutable SpatialGrid spatialGrid_;
    mutable bool spatialGridBuilt_;
    
public:
    LEDLayoutManager();
    ~LEDLayoutManager();
    
    /**
     * @brief CSVファイルからLED配置データ読み込み
     * @param csvPath CSVファイルパス
     * @return 読み込み成功フラグ
     */
    bool loadFromCSV(const char* csvPath);
    
    /**
     * @brief LED位置情報取得
     * @param faceID LED ID
     * @return LED位置情報（無効な場合は nullptr）
     */
    const LEDPosition* getPosition(uint16_t faceID) const;
    
    /**
     * @brief 全LED位置データ取得
     * @return LED位置データの参照
     */
    const std::vector<LEDPosition>& getAllPositions() const { return positions_; }
    
    /**
     * @brief LED総数取得
     * @return LED数
     */
    size_t getLEDCount() const { return positions_.size(); }
    
    /**
     * @brief 最寄りLED検索（UV座標）
     * @param u,v UV座標 [0.0-1.0]
     * @return 最寄りLED ID（見つからない場合は LED_COUNT）
     */
    uint16_t findClosestLED(float u, float v) const;
    
    /**
     * @brief N番目に近いLED検索
     * @param u,v UV座標
     * @param n 順位（0=最寄り）
     * @return N番目に近いLED ID
     */
    uint16_t findNthClosestLED(float u, float v, int n) const;
    
    /**
     * @brief 範囲内LED検索
     * @param u,v 中心UV座標
     * @param radius 検索半径 [0.0-1.0]
     * @return 範囲内LED IDリスト
     */
    std::vector<uint16_t> findLEDsInRange(float u, float v, float radius) const;
    
    /**
     * @brief 緯度線上LED取得
     * @param latitude 緯度（度）[-90, 90]
     * @param tolerance 許容誤差（度）
     * @return 緯度線上のLED IDリスト
     */
    std::vector<uint16_t> getLatitudeLEDs(float latitude, float tolerance = 2.0f) const;
    
    /**
     * @brief 経度線上LED取得
     * @param longitude 経度（度）[-180, 180]
     * @param tolerance 許容誤差（度）
     * @return 経度線上のLED IDリスト
     */
    std::vector<uint16_t> getLongitudeLEDs(float longitude, float tolerance = 2.0f) const;
    
    /**
     * @brief ストリップ別LED取得
     * @param strip ストリップ番号 [0-3]
     * @return ストリップのLED IDリスト
     */
    std::vector<uint16_t> getStripLEDs(uint8_t strip) const;
    
    /**
     * @brief 座標軸インジケータ用LED取得
     * @param axis 軸種別 ('x', 'y', 'z')
     * @param direction 方向 (1:正方向, -1:負方向)
     * @return 軸上のLED IDリスト（近傍含む）
     */
    std::vector<uint16_t> getAxisLEDs(char axis, int direction = 1) const;
    
    /**
     * @brief データ検証（整合性チェック）
     * @return 検証結果
     */
    bool validateData() const;
    
    /**
     * @brief 統計情報出力
     */
    void printStatistics() const;
    
    /**
     * @brief メモリ使用量取得
     * @return メモリ使用量（バイト）
     */
    size_t getMemoryUsage() const;

private:
    /**
     * @brief 空間分割グリッド構築（検索高速化）
     */
    void buildSpatialGrid() const;
    
    /**
     * @brief CSV行解析
     * @param line CSV行文字列
     * @param position 解析結果格納先
     * @return 解析成功フラグ
     */
    bool parseCSVLine(const String& line, LEDPosition& position) const;
    
    /**
     * @brief 3D座標→UV座標変換（配置データ用）
     * @param x,y,z 3D座標
     * @param u,v 出力UV座標
     */
    void cartesianToUV(float x, float y, float z, float& u, float& v) const;
    
    /**
     * @brief UV距離計算（トーラス面考慮）
     * @param u1,v1 座標1
     * @param u2,v2 座標2  
     * @return UV距離
     */
    float calculateUVDistance(float u1, float v1, float u2, float v2) const;
    
    /**
     * @brief 緯度・経度計算（3D座標から）
     * @param x,y,z 3D座標
     * @param lat,lon 出力緯度・経度（度）
     */
    void cartesianToLatLon(float x, float y, float z, float& lat, float& lon) const;
};

} // namespace LEDSphere