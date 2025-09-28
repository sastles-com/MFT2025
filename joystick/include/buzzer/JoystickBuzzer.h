#pragma once

#include <cstdint>
#include "config/ConfigManager.h"

/**
 * @brief GPIO5を使用したPWM制御によるjoystick専用ブザー制御クラス
 * 
 * ESP32のPWM機能を直接制御してGPIO5経由でブザー音を再生
 * 複数のメロディパターンと単音再生機能を提供し、joystickの操作フィードバックに最適化
 */
class JoystickBuzzer {
public:
    /**
     * @brief コンストラクタ (従来の設定用)
     * @param config ブザー設定
     */
    explicit JoystickBuzzer(const ConfigManager::BuzzerConfig& config);
    
    /**
     * @brief コンストラクタ (詳細音響設定用)
     * @param audioConfig 詳細音響設定
     */
    explicit JoystickBuzzer(const ConfigManager::JoystickConfig::AudioConfig& audioConfig);
    
    /**
     * @brief デストラクタ
     */
    ~JoystickBuzzer();
    
    /**
     * @brief 初期化処理
     * @return 初期化成功の場合true
     */
    bool initialize();
    
    /**
     * @brief 起動メロディを再生
     */
    void playStartupMelody();
    
    /**
     * @brief ボタンクリック音を再生
     */
    void playClickTone();
    
    /**
     * @brief エラー音を再生
     */
    void playErrorTone();
    
    /**
     * @brief 完了音を再生
     */
    void playCompletionTone();
    
    /**
     * @brief 接続音を再生
     */
    void playConnectTone();
    
    /**
     * @brief 切断音を再生
     */
    void playDisconnectTone();
    
    /**
     * @brief 警告音を再生
     */
    void playWarningTone();
    
    /**
     * @brief カスタム音程で音を再生
     * @param frequency 周波数 (Hz)
     * @param duration 持続時間 (ms)
     */
    void playTone(int frequency, int duration);
    
    /**
     * @brief パッシブブザーテスト - 周波数スイープ
     */
    void playFrequencySweep();
    
    /**
     * @brief パッシブブザーテスト - ドレミファソラシド
     */
    void playScaleTest();
    
    /**
     * @brief ブザーの有効/無効状態を取得
     * @return 有効の場合true
     */
    bool isEnabled() const;
    
    /**
     * @brief 現在の音量設定を取得
     * @return 音量 (0-100)
     */
    uint8_t getVolume() const;
    
    /**
     * @brief 設定を更新
     * @param config 新しい設定
     */
    void updateConfig(const ConfigManager::BuzzerConfig& config);

private:
    static const int BUZZER_PIN = 5;           ///< ブザー用GPIO番号
    static const int PWM_CHANNEL = 0;          ///< PWMチャンネル番号
    static const int PWM_RESOLUTION = 8;       ///< PWM分解能 (8bit = 0-255)
    static const int DEFAULT_DUTY_CYCLE = 128; ///< デフォルトデューティ比 (50%)
    
    /**
     * @brief 実際のハードウェアに音を出力
     * @param frequency 周波数 (Hz)
     * @param duration 持続時間 (ms)
     */
    void playToneInternal(int frequency, int duration);
    
    /**
     * @brief 音種別音量を考慮して音を出力
     * @param frequency 周波数 (Hz)
     * @param duration 持続時間 (ms)
     * @param soundType 音の種類 ("startup", "click", "error", "test")
     */
    void playToneWithVolume(int frequency, int duration, const char* soundType);
    
    /**
     * @brief 音を停止
     */
    void stopTone();

    
    /**
     * @brief 音量を考慮して実際に音を再生するかチェック
     * @return 再生する場合true
     */
    bool shouldPlaySound() const;
    
    /**
     * @brief PWMハードウェアの初期化
     */
    void initializeHardware();
    
    /**
     * @brief 有効音量を計算（マスター音量と個別設定を考慮）
     * @return 有効音量 (0-100)
     */
    uint8_t calculateEffectiveVolume() const;
    
    /**
     * @brief 音種別の有効音量を計算
     * @param soundType 音の種類 ("startup", "click", "error", "test")
     * @return 有効音量 (0-100)
     */
    uint8_t calculateSoundVolume(const char* soundType) const;

private:
    ConfigManager::BuzzerConfig config_;  // 従来の設定
    ConfigManager::JoystickConfig::AudioConfig audioConfig_;  // 詳細音響設定
    bool hasAudioConfig_;  // 詳細設定が利用可能かどうか
    bool initialized_;
};