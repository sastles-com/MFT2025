/**
 * @file JoystickDualDialUI.h
 * @brief M5Stack Atom-JoyStick デュアルダイアルUI統合システム
 * @description 左右アナログスティック連動デュアルダイアル操作UI
 * 
 * 機能:
 * - 外ダイアル: 機能選択（左スティック・8方向）
 * - 内ダイアル: 値調整（右スティック・連続値）  
 * - 自動整列: 選択項目常に12時位置
 * - MQTT統合: 状態同期・リアルタイム配信
 * - ホールド確定: スティック押し込み1秒確定
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @author Claude Code Assistant
 * @date 2025年9月5日
 */

#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include <ArduinoJson.h>
#include "JoystickConfig.h"

// ========== UI設定定数 ==========
#define DIAL_UI_SCREEN_WIDTH    128
#define DIAL_UI_SCREEN_HEIGHT   128
#define DIAL_UI_TITLE_HEIGHT    28
#define DIAL_UI_DIAL_AREA_TOP   30

// ダイアル描画設定
#define OUTER_DIAL_RADIUS       45
#define INNER_DIAL_RADIUS       25
#define CENTER_DISPLAY_RADIUS   20
#define DIAL_CENTER_X           64
#define DIAL_CENTER_Y           79   // (30 + 128) / 2

// 入力設定
#define ANALOG_STICK_CENTER     2048.0f
#define ANALOG_STICK_MAX        2048.0f
#define DEADZONE_THRESHOLD      0.15f
#define HOLD_CONFIRM_TIME_MS    1000
#define SENSITIVITY_MULTIPLIER  2.0f

// 色定義（5-6-5 RGB565形式）
#define COLOR_BACKGROUND        0x0000  // 黒
#define COLOR_DIAL_NORMAL       0x4208  // 灰色
#define COLOR_DIAL_ACTIVE       0x07FF  // 水色
#define COLOR_DIAL_SELECTED     0xFFE0  // 黄色
#define COLOR_TEXT_PRIMARY      0xFFFF  // 白
#define COLOR_TEXT_SECONDARY    0xC618  // 薄い灰色
#define COLOR_HOLD_PROGRESS     0xF800  // 赤

// テーマカラー定義（各モード）
#define COLOR_CONTROL_PRIMARY   0x001F  // 青（原色）
#define COLOR_VIDEO_PRIMARY     0x07E0  // 緑（原色）
#define COLOR_MAINTAIN_PRIMARY  0xFFE0  // 黄（原色）
#define COLOR_SYSTEM_PRIMARY    0xF81F  // マゼンタ（原色）

// 暗いテーマカラー（背景用・約10%明度）※従来版保持
#define COLOR_CONTROL_DARK      0x0003  // 暗い青
#define COLOR_VIDEO_DARK        0x0100  // 暗い緑
#define COLOR_MAINTAIN_DARK     0x1C00  // 暗い黄
#define COLOR_SYSTEM_DARK       0x1803  // 暗いマゼンタ

// 中明度テーマカラー（ヘッダー背景用・約30%明度）
#define COLOR_CONTROL_MEDIUM    0x0007  // 中程度の青
#define COLOR_VIDEO_MEDIUM      0x0300  // 中程度の緑
#define COLOR_MAINTAIN_MEDIUM   0x7E00  // 中程度の黄
#define COLOR_SYSTEM_MEDIUM     0x780F  // 中程度のマゼンタ

// 30%暗いテーマカラー（背景用）
#define COLOR_CONTROL_DIM       0x0015  // 30%暗い青
#define COLOR_VIDEO_DIM         0x04E0  // 30%暗い緑
#define COLOR_MAINTAIN_DIM      0xB5E0  // 30%暗い黄
#define COLOR_SYSTEM_DIM        0xB015  // 30%暗いマゼンタ

// 20%暗いテーマカラー（背景用・より明るく）
#define COLOR_CONTROL_LIGHT     0x001A  // 20%暗い青
#define COLOR_VIDEO_LIGHT       0x0640  // 20%暗い緑
#define COLOR_MAINTAIN_LIGHT    0xCCE0  // 20%暗い黄
#define COLOR_SYSTEM_LIGHT      0xC81A  // 20%暗いマゼンタ
#define COLOR_LIVE_LIGHT        0x7C1F  // 20%暗い橙（Liveモード用）

// 10%暗いテーマカラー（背景用・最も明るく）
#define COLOR_CONTROL_BRIGHT    0x001D  // 10%暗い青
#define COLOR_VIDEO_BRIGHT      0x0720  // 10%暗い緑
#define COLOR_MAINTAIN_BRIGHT   0xE7E0  // 10%暗い黄
#define COLOR_SYSTEM_BRIGHT     0xE01D  // 10%暗いマゼンタ
#define COLOR_LIVE_BRIGHT       0xBC1F  // 10%暗い橙（Liveモード用）

// 機能名用黄色文字
#define COLOR_FUNCTION_NAME     0xFFE0  // 鮮やかな黄色

// 高コントラスト文字色（視認性最適化）
#define COLOR_HEADER_TEXT       0xFFFF  // 白文字（最高視認性）
#define COLOR_CONTRAST_TEXT     0x0000  // 黒文字（明るい背景用）

/**
 * @brief UI操作モード
 */
enum UIOperationMode {
    UI_MODE_LIVE = 0,        // ライブ操作（リアルタイム制御）
    UI_MODE_CONTROL = 1,     // 基本制御（明度・色温度）
    UI_MODE_VIDEO = 2,       // 動画制御（音量・選択）
    UI_MODE_MAINTENANCE = 3, // 保守調整（パラメータ）
    UI_MODE_SYSTEM = 4       // システム監視
};

/**
 * @brief ダイアル項目定義
 */
struct DialItem {
    const char* name;        // 表示名
    int min_value;          // 最小値
    int max_value;          // 最大値  
    int current_value;      // 現在値
    const char* unit;       // 単位（%、Hz等）
    bool active;            // 有効/無効
};

/**
 * @brief モード別ダイアル設定
 */
struct ModeDialConfig {
    const char* mode_name;           // モード名
    uint16_t theme_color;           // テーマカラー
    DialItem dial_items[8];         // 8方向ダイアル項目
    uint8_t active_item_count;      // 有効項目数
};

/**
 * @brief UI操作状態
 */
struct UIOperationState {
    uint8_t selected_item_index;    // 選択中項目インデックス
    float left_stick_x;             // 左スティックX（正規化済み）
    float left_stick_y;             // 左スティックY（正規化済み）
    float right_stick_x;            // 右スティックX（正規化済み）
    float right_stick_y;            // 右スティックY（正規化済み）
    bool left_stick_pressed;        // 左スティック押し込み
    bool right_stick_pressed;       // 右スティック押し込み
    bool left_button_pressed;       // Lボタン
    bool right_button_pressed;      // Rボタン
    
    // ホールド確定システム
    unsigned long hold_start_time;  // ホールド開始時刻
    bool hold_in_progress;          // ホールド中
    bool hold_confirmed;            // ホールド確定完了
    
    // 回転制御
    float outer_dial_rotation;      // 外ダイアル回転角度（ラジアン）
    float inner_dial_rotation;      // 内ダイアル回転角度（ラジアン）
    float target_rotation;          // 目標回転角度（12時位置整列）
};

/**
 * @brief UI描画統計
 */
struct UIDrawStats {
    unsigned long total_draws;       // 総描画回数
    unsigned long last_draw_time;    // 最終描画時刻
    unsigned long avg_draw_time_us;  // 平均描画時間（マイクロ秒）
    unsigned long max_draw_time_us;  // 最大描画時間
    unsigned long frame_drops;       // フレーム落ち回数
    
    void reset() {
        total_draws = 0;
        last_draw_time = 0;
        avg_draw_time_us = 0;
        max_draw_time_us = 0;
        frame_drops = 0;
    }
};

/**
 * @brief デュアルダイアルUI統合クラス
 */
class JoystickDualDialUI {
public:
    JoystickDualDialUI();
    ~JoystickDualDialUI();
    
    // 初期化・設定
    bool begin(const JoystickConfig& config);
    void end();
    bool isInitialized() const { return initialized_; }
    
    // モード管理
    void setMode(UIOperationMode mode);
    UIOperationMode getCurrentMode() const { return current_mode_; }
    
    // 入力処理
    void updateInputs(
        float left_x, float left_y, bool left_pressed,
        float right_x, float right_y, bool right_pressed,
        bool l_button, bool r_button
    );
    
    // 描画システム
    void draw();
    void drawModeTitle();
    void drawDualDials();
    void drawCenterDisplay();
    void drawHoldProgress();
    
    // MQTT状態同期
    void syncFromMQTTState(const String& topic, int value);
    void syncFromMQTTState(const String& topic, float value);
    bool hasStateChanged() const { return state_changed_; }
    void resetStateChanged() { state_changed_ = false; }
    
    // 状態取得
    const UIOperationState& getOperationState() const { return operation_state_; }
    const DialItem& getCurrentDialItem() const;
    int getCurrentSelectedValue() const;
    bool isConfirmationReady() const { return operation_state_.hold_confirmed; }
    
    // 統計・デバッグ
    const UIDrawStats& getDrawStats() const { return draw_stats_; }
    void printDebugInfo() const;
    
    // コールバック設定
    void setValueChangeCallback(void (*callback)(const char* topic, int value));
    void setConfirmCallback(void (*callback)(uint8_t item_index, int value));

private:
    // 初期化状態
    bool initialized_;
    JoystickConfig config_;
    
    // UI状態
    UIOperationMode current_mode_;
    ModeDialConfig mode_configs_[5];  // 5モード設定（Live追加）
    UIOperationState operation_state_;
    bool state_changed_;
    
    // 描画システム
    UIDrawStats draw_stats_;
    unsigned long last_frame_time_;
    
    // コールバック
    void (*value_change_callback_)(const char* topic, int value);
    void (*confirm_callback_)(uint8_t item_index, int value);
    
    // 初期化ヘルパー
    void initializeModeConfigs();
    void initializeLiveMode();
    void initializeControlMode();
    void initializeVideoMode(); 
    void initializeMaintenanceMode();
    void initializeSystemMode();
    
    // 入力処理ヘルパー
    float normalizeAnalogInput(uint16_t raw_value);
    bool isInDeadzone(float x, float y);
    void updateDialRotations();
    void updateItemSelection();
    void updateValueAdjustment();
    void updateHoldConfirmation();
    
    // 描画ヘルパー
    void drawOuterDial();
    void drawInnerDial();
    void drawDialItem(uint8_t index, float angle, bool selected);
    void drawRotationMarkers();
    void drawProgressRing(int center_x, int center_y, int radius, float progress, uint16_t color);
    
    // 座標計算
    void polarToCartesian(float angle, float radius, int& x, int& y);
    float calculateItemAngle(uint8_t index, uint8_t total_items);
    float normalizeAngle(float angle);
    
    // 状態管理
    void updateDrawStats(unsigned long draw_time_us);
    void checkStateChanges();
    void triggerValueChangeCallback();
    
    // デバッグ・ログ
    void logUIOperation(const char* operation, const char* detail);
    void printError(const char* message, const char* detail = nullptr);
};

// グローバル関数・ユーティリティ
namespace DualDialUtils {
    // 角度変換
    float degreesToRadians(float degrees);
    float radiansToDegrees(float radians);
    
    // 色計算
    uint16_t interpolateColor(uint16_t color1, uint16_t color2, float ratio);
    uint16_t getModeThemeColor(UIOperationMode mode);
    uint16_t getModeDarkColor(UIOperationMode mode);
    uint16_t getModeMediumColor(UIOperationMode mode);
    uint16_t getModeDimColor(UIOperationMode mode);
    uint16_t getModeLightColor(UIOperationMode mode);  // 20%暗い色用
    uint16_t getModeBrightColor(UIOperationMode mode); // 10%暗い色用
    uint16_t getOptimalTextColor(uint16_t background_color);
    
    // 文字列フォーマット
    String formatValue(int value, const char* unit);
    String formatAngle(float radians);
}