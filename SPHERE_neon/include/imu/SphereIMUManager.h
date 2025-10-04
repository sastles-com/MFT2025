#pragma once

#include <Arduino.h>
#include <M5Unified.h>

/**
 * @brief SPHERE_neon専用BMI270 IMU制御クラス
 * 
 * M5Stack AtomS3RのBMI270センサーを使用して、
 * 高精度な姿勢制御とジェスチャー検出を提供します。
 */
class SphereIMUManager {
public:
    /**
     * @brief 生センサーデータ構造体
     */
    struct RawData {
        float accel_x, accel_y, accel_z;  // 加速度 (g)
        float gyro_x, gyro_y, gyro_z;     // 角速度 (dps)
        float temp;                       // 温度 (℃)
        unsigned long timestamp;          // タイムスタンプ (μs)
    };
    
    /**
     * @brief フィルタ済みセンサーデータ構造体
     */
    struct FilteredData {
        float accel_x, accel_y, accel_z;  // フィルタ済み加速度 (g)
        float gyro_x, gyro_y, gyro_z;     // フィルタ済み角速度 (dps)
    };
    
    /**
     * @brief クォータニオン構造体
     */
    struct Quaternion {
        float w, x, y, z;
        
        Quaternion() : w(1.0f), x(0.0f), y(0.0f), z(0.0f) {}
        Quaternion(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}
        
        // クォータニオンの正規化
        void normalize() {
            float norm = sqrt(w*w + x*x + y*y + z*z);
            if (norm > 0.0001f) {
                w /= norm; x /= norm; y /= norm; z /= norm;
            }
        }
    };
    
    /**
     * @brief オイラー角構造体
     */
    struct EulerAngles {
        float roll;   // ロール角 (-180° ～ +180°)
        float pitch;  // ピッチ角 (-90° ～ +90°)
        float yaw;    // ヨー角 (-180° ～ +180°)
        
        EulerAngles() : roll(0.0f), pitch(0.0f), yaw(0.0f) {}
        EulerAngles(float r, float p, float y) : roll(r), pitch(p), yaw(y) {}
    };
    
    /**
     * @brief 傾き方向列挙
     */
    enum TiltDirection {
        TILT_NONE = 0,
        TILT_FORWARD,
        TILT_BACKWARD,
        TILT_LEFT,
        TILT_RIGHT
    };
    
    /**
     * @brief BMI270特殊機能列挙
     */
    enum BMI270Feature {
        FEATURE_STEP_COUNTER = 0,    // 歩数計機能
        FEATURE_STEP_DETECTOR,       // 歩行検出
        FEATURE_SIG_MOTION,          // 有意な動作検出
        FEATURE_ANY_MOTION,          // 任意動作検出
        FEATURE_NO_MOTION,           // 静止検出
        FEATURE_ORIENTATION,         // 向き検出
        FEATURE_HIGH_G,              // 高G検出
        FEATURE_LOW_G,               // 低G検出
        FEATURE_FLAT,                // 平面検出
        FEATURE_WRIST_GESTURE,       // 手首ジェスチャー
    };
    
    /**
     * @brief BMI270活動状態
     */
    enum ActivityState {
        ACTIVITY_STILL = 0,          // 静止
        ACTIVITY_WALKING,            // 歩行
        ACTIVITY_RUNNING,            // 走行
        ACTIVITY_UNKNOWN             // 不明
    };

private:
    bool initialized;                     // 初期化状態
    bool calibrated;                      // キャリブレーション状態
    uint16_t sample_rate;                 // サンプリングレート (Hz)
    
    // フィルタリング設定
    float lowpass_alpha;                  // ローパスフィルタ係数
    FilteredData filtered_data;           // フィルタ済みデータ
    
    // 姿勢計算
    Quaternion current_quaternion;        // 現在のクォータニオン
    EulerAngles current_euler;            // 現在のオイラー角
    
    // ジェスチャー検出
    float shake_threshold;                // シェイク検出閾値 (g)
    uint32_t shake_time_window;           // シェイク検出時間窓 (ms)
    float motion_threshold;               // モーション検出閾値 (g)
    
    // シェイク検出用バッファ
    static const size_t SHAKE_BUFFER_SIZE = 10;
    float shake_buffer[SHAKE_BUFFER_SIZE];
    size_t shake_buffer_index;
    unsigned long last_shake_time;
    
    // BMI270特殊機能
    uint32_t step_count;                  // 歩数カウント
    bool features_enabled[10];            // 特殊機能有効フラグ
    ActivityState current_activity;       // 現在の活動状態
    float high_g_threshold;               // 高G検出閾値
    float low_g_threshold;                // 低G検出閾値
    uint16_t orientation_hysteresis;      // 向き検出ヒステリシス
    
    // 高度なジェスチャー検出
    bool wrist_gesture_enabled;           // 手首ジェスチャー有効
    float tap_sensitivity;                // タップ感度
    bool double_tap_enabled;              // ダブルタップ有効
    
    // キャリブレーション
    static const size_t CALIB_SAMPLES = 100;
    float accel_offset[3];                // 加速度オフセット
    float gyro_offset[3];                 // ジャイロオフセット
    
    // 内部メソッド
    /**
     * @brief BMI270センサー設定
     */
    bool configureBMI270();
    
    /**
     * @brief ローパスフィルタ適用
     */
    void applyLowPassFilter(const RawData& raw);
    
    /**
     * @brief クォータニオン更新（Madgwickフィルタ）
     */
    void updateQuaternion(const FilteredData& data, float dt);
    
    /**
     * @brief クォータニオン→オイラー角変換
     */
    void quaternionToEuler(const Quaternion& q, EulerAngles& euler);
    
    /**
     * @brief シェイク検出アルゴリズム
     */
    bool detectShake(float magnitude);
    
    /**
     * @brief 傾き方向計算
     */
    TiltDirection calculateTiltDirection(const EulerAngles& euler);

public:
    /**
     * @brief コンストラクタ
     */
    SphereIMUManager();
    
    /**
     * @brief デストラクタ
     */
    ~SphereIMUManager();
    
    /**
     * @brief IMUシステムの初期化
     * @return true 成功, false 失敗
     */
    bool initialize();
    
    /**
     * @brief サンプリングレート設定
     * @param rate サンプリングレート (25, 50, 100, 200, 400Hz)
     * @return true 成功, false 失敗
     */
    bool setSampleRate(uint16_t rate);
    
    /**
     * @brief 現在のサンプリングレート取得
     * @return サンプリングレート (Hz)
     */
    uint16_t getSampleRate() const;
    
    /**
     * @brief 生センサーデータ読み取り
     * @param data 出力先データ構造体
     * @return true 成功, false 失敗
     */
    bool readRawData(RawData& data);
    
    /**
     * @brief フィルタ済みデータ取得
     * @return フィルタ済みデータ
     */
    const FilteredData& getFilteredData() const;
    
    /**
     * @brief 現在の姿勢（クォータニオン）取得
     * @return クォータニオン
     */
    const Quaternion& getOrientation() const;
    
    /**
     * @brief 現在の姿勢（オイラー角）取得
     * @return オイラー角
     */
    const EulerAngles& getEulerAngles() const;
    
    /**
     * @brief キャリブレーション開始
     * @return true 成功, false 失敗
     */
    bool startCalibration();
    
    /**
     * @brief キャリブレーション状態確認
     * @return true 完了, false 未完了
     */
    bool isCalibrated() const;
    
    /**
     * @brief 初期化状態確認
     * @return true 初期化済み, false 未初期化
     */
    bool isInitialized() const;
    
    /**
     * @brief シェイク検出閾値設定
     * @param threshold 閾値 (g)
     */
    void setShakeThreshold(float threshold);
    
    /**
     * @brief シェイク検出時間窓設定
     * @param timeWindow 時間窓 (ms)
     */
    void setShakeTimeWindow(uint32_t timeWindow);
    
    /**
     * @brief シェイク検出
     * @return true 検出, false 未検出
     */
    bool isShakeDetected();
    
    /**
     * @brief シェイク閾値取得
     * @return 現在の閾値 (g)
     */
    float getShakeThreshold() const;
    
    /**
     * @brief シェイク時間窓取得
     * @return 現在の時間窓 (ms)
     */
    uint32_t getShakeTimeWindow() const;
    
    /**
     * @brief 傾き方向取得
     * @return 傾き方向
     */
    TiltDirection getTiltDirection();
    
    /**
     * @brief モーション検出閾値設定
     * @param threshold 閾値 (g)
     */
    void setMotionThreshold(float threshold);
    
    /**
     * @brief モーション検出
     * @return true 動作中, false 静止中
     */
    bool isInMotion();
    
    /**
     * @brief ローパスフィルタ係数設定
     * @param alpha フィルタ係数 (0.0～1.0)
     */
    void setLowPassFilterAlpha(float alpha);
    
    /**
     * @brief センサーデータ更新（メインループで呼び出し）
     */
    void update();
    
    /**
     * @brief デバッグ情報出力
     */
    void printDebugInfo() const;
    
    // =============BMI270特殊機能============= //
    
    /**
     * @brief BMI270特殊機能有効化
     * @param feature 機能種別
     * @param enable 有効/無効
     * @return true 成功, false 失敗
     */
    bool enableFeature(BMI270Feature feature, bool enable = true);
    
    /**
     * @brief 歩数カウント取得
     * @return 現在の歩数
     */
    uint32_t getStepCount();
    
    /**
     * @brief 歩数カウントリセット
     * @return true 成功, false 失敗
     */
    bool resetStepCount();
    
    /**
     * @brief 活動状態取得
     * @return 現在の活動状態
     */
    ActivityState getActivityState();
    
    /**
     * @brief 高G検出閾値設定
     * @param threshold 閾値 (g)
     */
    void setHighGThreshold(float threshold);
    
    /**
     * @brief 低G検出閾値設定
     * @param threshold 閾値 (g)
     */
    void setLowGThreshold(float threshold);
    
    /**
     * @brief 向き検出ヒステリシス設定
     * @param hysteresis ヒステリシス値
     */
    void setOrientationHysteresis(uint16_t hysteresis);
    
    /**
     * @brief 手首ジェスチャー検出有効化
     * @param enable 有効/無効
     */
    void enableWristGesture(bool enable);
    
    /**
     * @brief タップ感度設定
     * @param sensitivity 感度 (0.0～1.0)
     */
    void setTapSensitivity(float sensitivity);
    
    /**
     * @brief ダブルタップ検出有効化
     * @param enable 有効/無効
     */
    void enableDoubleTap(bool enable);
    
    /**
     * @brief シングルタップ検出
     * @return true 検出, false 未検出
     */
    bool isSingleTapDetected();
    
    /**
     * @brief ダブルタップ検出
     * @return true 検出, false 未検出
     */
    bool isDoubleTapDetected();
    
    /**
     * @brief 有意な動作検出
     * @return true 検出, false 未検出
     */
    bool isSignificantMotionDetected();
    
    /**
     * @brief 平面検出（デバイスが平らに置かれているか）
     * @return true 平面状態, false 非平面状態
     */
    bool isFlatDetected();
    
    /**
     * @brief 自由落下検出
     * @return true 落下中, false 通常状態
     */
    bool isFreeFallDetected();
    
    /**
     * @brief BMI270内部温度取得（較正済み）
     * @return 温度 (℃)
     */
    float getCalibratedTemperature();
    
    /**
     * @brief 電力管理モード設定
     * @param low_power 低電力モード有効
     * @return true 成功, false 失敗
     */
    bool setPowerMode(bool low_power);
    
    /**
     * @brief ファーストイン・ファーストアウト（FIFO）データ取得
     * @param buffer データバッファ
     * @param buffer_size バッファサイズ
     * @return 読み取ったデータ数
     */
    size_t readFifoData(RawData* buffer, size_t buffer_size);
};