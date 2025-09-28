#pragma once

#include <cstdint>
#include "config/ConfigManager.h"

/**
 * @brief M5Unified Speakerを使用したブザー制御クラス
 * 
 * M5UnifiedライブラリのSpeaker機能を使用して音声出力を制御
 * M5AtomS3Rで内蔵スピーカーが利用可能かテストするためのクラス
 */
class M5SpeakerBuzzer {
public:
    /**
     * @brief コンストラクタ
     * @param config ブザー設定
     */
    explicit M5SpeakerBuzzer(const ConfigManager::BuzzerConfig& config);
    
    /**
     * @brief デストラクタ
     */
    ~M5SpeakerBuzzer();
    
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
    /**
     * @brief 実際のハードウェアに音を出力
     * @param frequency 周波数 (Hz)
     * @param duration 持続時間 (ms)
     */
    void playToneInternal(int frequency, int duration);
    
    /**
     * @brief 音量を考慮して実際に音を再生するかチェック
     * @return 再生する場合true
     */
    bool shouldPlaySound() const;

private:
    ConfigManager::BuzzerConfig config_;
    bool initialized_;
};