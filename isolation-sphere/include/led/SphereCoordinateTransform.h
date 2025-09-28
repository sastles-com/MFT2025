/**
 * @file SphereCoordinateTransform.h
 * @brief 球面座標変換システム
 * 
 * 3D→球面座標→UV座標の変換・IMU姿勢制御・オフセット管理
 */

#pragma once

#include <Arduino.h>
#include <cmath>
#include "led/LEDSphereManager.h"

namespace LEDSphere {

/**
 * @brief 球面座標変換制御クラス
 * 
 * IMU姿勢・UI制御オフセットを考慮した3D→UV座標変換
 * 高速数学演算・変更検出によるキャッシュ最適化
 */
class SphereCoordinateTransform {
private:
    // 姿勢パラメータ
    float quaternionW_, quaternionX_, quaternionY_, quaternionZ_;  // IMU姿勢
    float latitudeOffset_, longitudeOffset_;                      // UI制御オフセット（度）
    
    // 変更検出用
    float lastQW_, lastQX_, lastQY_, lastQZ_;
    float lastLatOffset_, lastLonOffset_;
    bool transformDirty_;
    
    // 高速演算設定
    bool useApproximation_;     // 近似計算使用フラグ
    bool enableSIMD_;           // SIMD最適化フラグ
    
    // 数学定数
    static constexpr float PI_F = 3.14159265359f;
    static constexpr float TWO_PI_F = 2.0f * PI_F;
    static constexpr float HALF_PI_F = PI_F * 0.5f;
    static constexpr float RAD_TO_DEG = 180.0f / PI_F;
    static constexpr float DEG_TO_RAD = PI_F / 180.0f;
    
public:
    SphereCoordinateTransform();
    ~SphereCoordinateTransform();
    
    /**
     * @brief IMU姿勢設定
     * @param qw,qx,qy,qz クォータニオン成分
     */
    void setPosture(float qw, float qx, float qy, float qz);
    
    /**
     * @brief UI制御オフセット設定
     * @param latOffset 緯度オフセット（度）
     * @param lonOffset 経度オフセット（度）
     */
    void setOffset(float latOffset, float lonOffset);
    
    /**
     * @brief 姿勢パラメータ一括設定
     * @param params 姿勢・オフセット情報
     */
    void setPostureParams(const PostureParams& params);
    
    /**
     * @brief 3D座標→UV座標変換（メイン変換関数）
     * @param x,y,z 3D座標 [-1.0, 1.0]
     * @return UV座標 [0.0, 1.0]
     */
    UVCoordinate transform3DToUV(float x, float y, float z) const;
    
    /**
     * @brief 3D座標→球面座標変換
     * @param x,y,z 3D座標
     * @param theta,phi 出力球面座標（ラジアン）
     */
    void cartesianToSpherical(float x, float y, float z, float& theta, float& phi) const;
    
    /**
     * @brief 球面座標→UV座標変換
     * @param theta,phi 球面座標（ラジアン）
     * @param u,v 出力UV座標 [0.0, 1.0]
     */
    void sphericalToUV(float theta, float phi, float& u, float& v) const;
    
    /**
     * @brief UV座標→3D座標逆変換
     * @param u,v UV座標
     * @param x,y,z 出力3D座標
     */
    void uvToCartesian(float u, float v, float& x, float& y, float& z) const;
    
    /**
     * @brief 姿勢変換適用（3D座標）
     * @param x,y,z 入出力3D座標
     */
    void applyPostureTransform(float& x, float& y, float& z) const;
    
    /**
     * @brief オフセット適用（球面座標）
     * @param theta,phi 入出力球面座標
     */
    void applyOffsetTransform(float& theta, float& phi) const;
    
    /**
     * @brief 変更検出（キャッシュ制御用）
     * @return 姿勢・オフセットが変更されたか
     */
    bool isTransformDirty() const { return transformDirty_; }
    
    /**
     * @brief 変更フラグクリア
     */
    void clearDirtyFlag() { transformDirty_ = false; }
    
    /**
     * @brief 現在の姿勢パラメータ取得
     * @return 姿勢・オフセット情報
     */
    PostureParams getCurrentParams() const;
    
    /**
     * @brief 高速演算モード設定
     * @param useApprox 近似計算使用フラグ
     */
    void setApproximationMode(bool useApprox) { useApproximation_ = useApprox; }
    
    /**
     * @brief SIMD最適化設定
     * @param enable SIMD使用フラグ
     */
    void setSIMDEnabled(bool enable) { enableSIMD_ = enable; }
    
    // ========== 座標系ユーティリティ ==========
    
    /**
     * @brief 座標軸方向取得（現在の姿勢考慮）
     * @param axis 軸種別 ('x', 'y', 'z')
     * @param direction 方向 (1:正, -1:負)
     * @return 軸方向のUV座標
     */
    UVCoordinate getAxisDirection(char axis, int direction = 1) const;
    
    /**
     * @brief 緯度線上の点取得
     * @param latitude 緯度（度）
     * @param longitude 経度（度）
     * @return UV座標
     */
    UVCoordinate getLatitudeLongitudePoint(float latitude, float longitude) const;
    
    /**
     * @brief 北極・南極座標取得
     * @param isNorthPole true:北極, false:南極
     * @return UV座標
     */
    UVCoordinate getPoleCoordinate(bool isNorthPole = true) const;
    
    /**
     * @brief 赤道上の点取得
     * @param longitude 経度（度）
     * @return UV座標
     */
    UVCoordinate getEquatorPoint(float longitude) const;
    
    // ========== デバッグ・検証 ==========
    
    /**
     * @brief 座標変換テスト（整合性確認）
     * @return テスト結果
     */
    bool validateTransform() const;
    
    /**
     * @brief 変換パラメータ出力
     */
    void printTransformParams() const;
    
    /**
     * @brief パフォーマンステスト実行
     * @param iterations テスト回数
     * @return 平均実行時間（マイクロ秒）
     */
    float benchmarkTransform(size_t iterations = 10000) const;

private:
    // ========== 内部数学関数 ==========
    
    /**
     * @brief 高速arctan2近似
     * @param y,x 座標成分
     * @return 角度（ラジアン）
     */
    float fast_atan2(float y, float x) const;
    
    /**
     * @brief 高速sqrt近似
     * @param x 入力値
     * @return 平方根
     */
    float fast_sqrt(float x) const;
    
    /**
     * @brief 高速asin近似  
     * @param x 入力値 [-1, 1]
     * @return 角度（ラジアン）
     */
    float fast_asin(float x) const;
    
    /**
     * @brief 正規化角度（ラップアラウンド処理）
     * @param angle 角度（ラジアン）
     * @param min,max 範囲
     * @return 正規化された角度
     */
    float normalizeAngle(float angle, float min = -PI_F, float max = PI_F) const;
    
    /**
     * @brief クォータニオン回転適用
     * @param x,y,z 入出力3D座標
     * @param qw,qx,qy,qz クォータニオン成分
     */
    void quaternionRotate(float& x, float& y, float& z, 
                         float qw, float qx, float qy, float qz) const;
    
    /**
     * @brief 変更検出・フラグ更新
     */
    void updateDirtyFlag();
    
    // SIMD最適化版関数（ESP32-S3対応）
    #ifdef ESP32_S3_SIMD_SUPPORT
    void simd_quaternionRotate(float* xyz, float* quat) const;
    void simd_cartesianToSpherical(float* xyz, float* thetaPhi) const;
    #endif
};

/**
 * @brief 座標変換ユーティリティ関数群
 */
namespace CoordinateUtils {
    
    /**
     * @brief UV距離計算（トーラス面対応）
     * @param u1,v1,u2,v2 UV座標
     * @return 距離
     */
    float calculateUVDistance(float u1, float v1, float u2, float v2);
    
    /**
     * @brief 緯度・経度→3D座標変換
     * @param lat,lon 緯度・経度（度）
     * @param x,y,z 出力3D座標
     */
    void latLonToCartesian(float lat, float lon, float& x, float& y, float& z);
    
    /**
     * @brief 3D座標→緯度・経度変換  
     * @param x,y,z 3D座標
     * @param lat,lon 出力緯度・経度（度）
     */
    void cartesianToLatLon(float x, float y, float z, float& lat, float& lon);
    
    /**
     * @brief 大圏距離計算（球面上の最短距離）
     * @param lat1,lon1,lat2,lon2 2点の緯度・経度（度）
     * @return 角距離（度）
     */
    float greatCircleDistance(float lat1, float lon1, float lat2, float lon2);
    
    /**
     * @brief 線形補間（球面座標用）
     * @param theta1,phi1 開始座標
     * @param theta2,phi2 終了座標
     * @param t 補間係数 [0, 1]
     * @param outTheta,outPhi 出力座標
     */
    void slerpSpherical(float theta1, float phi1, float theta2, float phi2, 
                       float t, float& outTheta, float& outPhi);
}

} // namespace LEDSphere