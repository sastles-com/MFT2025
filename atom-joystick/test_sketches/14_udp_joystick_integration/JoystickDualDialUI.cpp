/**
 * @file JoystickDualDialUI.cpp
 * @brief M5Stack Atom-JoyStick デュアルダイアルUI統合システム実装
 */

#include "JoystickDualDialUI.h"

// ========== コンストラクタ・デストラクタ ==========

JoystickDualDialUI::JoystickDualDialUI() 
    : initialized_(false)
    , current_mode_(UI_MODE_LIVE)
    , state_changed_(false)
    , last_frame_time_(0)
    , value_change_callback_(nullptr)
    , confirm_callback_(nullptr) {
    
    // 操作状態初期化
    operation_state_ = {0};
    operation_state_.selected_item_index = 0;
    operation_state_.outer_dial_rotation = 0.0f;
    operation_state_.inner_dial_rotation = 0.0f;
    operation_state_.target_rotation = 0.0f;
    
    // 描画統計初期化
    draw_stats_.reset();
}

JoystickDualDialUI::~JoystickDualDialUI() {
    end();
}

// ========== 初期化・設定 ==========

bool JoystickDualDialUI::begin(const JoystickConfig& config) {
    if (initialized_) {
        return true;
    }
    
    config_ = config;
    
    // モード設定初期化
    initializeModeConfigs();
    
    // 描画統計リセット
    draw_stats_.reset();
    last_frame_time_ = millis();
    
    initialized_ = true;
    
    Serial.println("✅ JoystickDualDialUI初期化完了");
    Serial.printf("   画面サイズ: %dx%d\n", DIAL_UI_SCREEN_WIDTH, DIAL_UI_SCREEN_HEIGHT);
    Serial.printf("   外ダイアル半径: %dpx\n", OUTER_DIAL_RADIUS);
    Serial.printf("   内ダイアル半径: %dpx\n", INNER_DIAL_RADIUS);
    Serial.printf("   デッドゾーン閾値: %.2f\n", DEADZONE_THRESHOLD);
    
    return true;
}

void JoystickDualDialUI::end() {
    if (!initialized_) {
        return;
    }
    
    initialized_ = false;
    Serial.println("JoystickDualDialUI終了");
}

// ========== モード管理 ==========

void JoystickDualDialUI::setMode(UIOperationMode mode) {
    if (mode != current_mode_) {
        current_mode_ = mode;
        operation_state_.selected_item_index = 0; // 項目選択リセット
        operation_state_.outer_dial_rotation = 0.0f;
        operation_state_.target_rotation = 0.0f;
        state_changed_ = true;
        
        Serial.printf("🎛️ UI モード変更: %s\n", mode_configs_[mode].mode_name);
    }
}

// ========== 入力処理 ==========

void JoystickDualDialUI::updateInputs(
    float left_x, float left_y, bool left_pressed,
    float right_x, float right_y, bool right_pressed,
    bool l_button, bool r_button) {
    
    // 入力値保存
    operation_state_.left_stick_x = left_x;
    operation_state_.left_stick_y = left_y;
    operation_state_.left_stick_pressed = left_pressed;
    operation_state_.right_stick_x = right_x;
    operation_state_.right_stick_y = right_y;
    operation_state_.right_stick_pressed = right_pressed;
    operation_state_.left_button_pressed = l_button;
    operation_state_.right_button_pressed = r_button;
    
    // ダイアル回転更新
    updateDialRotations();
    
    // 項目選択更新
    updateItemSelection();
    
    // 値調整更新
    updateValueAdjustment();
    
    // ホールド確定更新
    updateHoldConfirmation();
    
    // 変更検出
    checkStateChanges();
}

// ========== 描画システム ==========

void JoystickDualDialUI::draw() {
    if (!initialized_) {
        return;
    }
    
    unsigned long draw_start = micros();
    
    // 背景クリア
    M5.Display.fillScreen(COLOR_BACKGROUND);
    
    // 各要素描画
    drawModeTitle();
    drawDualDials();
    drawCenterDisplay();
    drawHoldProgress();
    
    // フレーム統計更新
    unsigned long draw_time = micros() - draw_start;
    updateDrawStats(draw_time);
    
    draw_stats_.last_draw_time = millis();
}

void JoystickDualDialUI::drawModeTitle() {
    ModeDialConfig& config = mode_configs_[current_mode_];
    
    // 30%の明るさテーマカラー背景
    uint16_t dim_color = DualDialUtils::getModeDimColor(current_mode_);
    M5.Display.fillRect(0, 0, DIAL_UI_SCREEN_WIDTH, DIAL_UI_TITLE_HEIGHT, dim_color);
    
    // 境界線追加（ヘッダーとダイアル領域の分離）
    uint16_t primary_color = DualDialUtils::getModeThemeColor(current_mode_);
    M5.Display.drawLine(0, DIAL_UI_TITLE_HEIGHT - 1, DIAL_UI_SCREEN_WIDTH, DIAL_UI_TITLE_HEIGHT - 1, primary_color);
    
    // モード名表示（白文字・影効果で視認性向上）
    M5.Display.setTextColor(COLOR_BACKGROUND);  // 影（黒）
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString(config.mode_name, DIAL_UI_SCREEN_WIDTH / 2 + 1, 14 + 1);
    
    M5.Display.setTextColor(COLOR_HEADER_TEXT);  // 白文字
    M5.Display.drawString(config.mode_name, DIAL_UI_SCREEN_WIDTH / 2, 14);
    
    // L/Rボタン機能表示（帯の下・グレー文字）
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(COLOR_TEXT_SECONDARY);
    M5.Display.setTextDatum(ML_DATUM);
    M5.Display.drawString("L:PLAY", 2, DIAL_UI_TITLE_HEIGHT + 2);
    M5.Display.setTextDatum(MR_DATUM);
    M5.Display.drawString("R:STOP", DIAL_UI_SCREEN_WIDTH - 2, DIAL_UI_TITLE_HEIGHT + 2);
}

void JoystickDualDialUI::drawDualDials() {
    drawOuterDial();
    drawInnerDial();
}

void JoystickDualDialUI::drawCenterDisplay() {
    ModeDialConfig& config = mode_configs_[current_mode_];
    DialItem& current_item = config.dial_items[operation_state_.selected_item_index];
    
    // 中央円背景（30%暗いテーマカラー）
    uint16_t dim_color = DualDialUtils::getModeDimColor(current_mode_);
    uint16_t primary_color = DualDialUtils::getModeThemeColor(current_mode_);
    
    M5.Display.fillCircle(DIAL_CENTER_X, DIAL_CENTER_Y, CENTER_DISPLAY_RADIUS, dim_color);
    M5.Display.drawCircle(DIAL_CENTER_X, DIAL_CENTER_Y, CENTER_DISPLAY_RADIUS, primary_color);
    
    // 機能名の背景四角を描画（テーマカラー）
    String function_name = String(current_item.name);
    M5.Display.setTextSize(2);
    int text_width = M5.Display.textWidth(function_name);
    int text_height = 16;  // フォントサイズ2の高さ
    
    // 背景四角の位置・サイズ計算（余白追加）
    int bg_x = DIAL_CENTER_X - (text_width / 2) - 4;
    int bg_y = DIAL_CENTER_Y - 8 - (text_height / 2) - 2;
    int bg_width = text_width + 8;
    int bg_height = text_height + 4;
    
    // 30%の明るさテーマカラー背景四角描画（ヘッダーと統一）
    uint16_t bg_dim_color = DualDialUtils::getModeDimColor(current_mode_);
    M5.Display.fillRoundRect(bg_x, bg_y, bg_width, bg_height, 3, bg_dim_color);
    M5.Display.drawRoundRect(bg_x, bg_y, bg_width, bg_height, 3, COLOR_TEXT_PRIMARY);
    
    // 機能名表示（黄色文字・テーマカラー背景に対してアクセント効果）
    M5.Display.setTextColor(COLOR_FUNCTION_NAME);  // 鮮やかな黄色
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString(current_item.name, DIAL_CENTER_X, DIAL_CENTER_Y - 8);
    
    // 現在値表示（白色・強調・拡大文字サイズ）
    String value_str = DualDialUtils::formatValue(current_item.current_value, current_item.unit);
    M5.Display.setTextColor(COLOR_TEXT_PRIMARY);
    M5.Display.setTextSize(2);  // サイズ1→2に拡大
    M5.Display.drawString(value_str, DIAL_CENTER_X, DIAL_CENTER_Y + 12);  // 位置調整
}

void JoystickDualDialUI::drawHoldProgress() {
    if (operation_state_.hold_in_progress) {
        unsigned long hold_duration = millis() - operation_state_.hold_start_time;
        float progress = (float)hold_duration / HOLD_CONFIRM_TIME_MS;
        progress = constrain(progress, 0.0f, 1.0f);
        
        drawProgressRing(DIAL_CENTER_X, DIAL_CENTER_Y, CENTER_DISPLAY_RADIUS + 5, progress, COLOR_HOLD_PROGRESS);
    }
}

// ========== 描画ヘルパー ==========

void JoystickDualDialUI::drawOuterDial() {
    ModeDialConfig& config = mode_configs_[current_mode_];
    
    // 外ダイアル円描画
    M5.Display.drawCircle(DIAL_CENTER_X, DIAL_CENTER_Y, OUTER_DIAL_RADIUS, COLOR_DIAL_NORMAL);
    
    // 項目描画
    for (uint8_t i = 0; i < config.active_item_count; i++) {
        if (!config.dial_items[i].active) continue;
        
        float angle = calculateItemAngle(i, config.active_item_count) + operation_state_.outer_dial_rotation;
        bool selected = (i == operation_state_.selected_item_index);
        
        drawDialItem(i, angle, selected);
    }
    
    // 12時位置マーカー（選択位置指示）
    int marker_x, marker_y;
    polarToCartesian(-PI/2, OUTER_DIAL_RADIUS + 10, marker_x, marker_y);
    
    // より目立つ三角マーカー（テーマカラー）
    uint16_t primary_color = DualDialUtils::getModeThemeColor(current_mode_);
    M5.Display.fillTriangle(
        DIAL_CENTER_X + marker_x, DIAL_CENTER_Y + marker_y,
        DIAL_CENTER_X + marker_x - 4, DIAL_CENTER_Y + marker_y + 8,
        DIAL_CENTER_X + marker_x + 4, DIAL_CENTER_Y + marker_y + 8,
        primary_color
    );
    
    // 枠線で強調
    M5.Display.drawTriangle(
        DIAL_CENTER_X + marker_x, DIAL_CENTER_Y + marker_y,
        DIAL_CENTER_X + marker_x - 4, DIAL_CENTER_Y + marker_y + 8,
        DIAL_CENTER_X + marker_x + 4, DIAL_CENTER_Y + marker_y + 8,
        COLOR_FUNCTION_NAME
    );
}

void JoystickDualDialUI::drawInnerDial() {
    // 内ダイアル円描画
    M5.Display.drawCircle(DIAL_CENTER_X, DIAL_CENTER_Y, INNER_DIAL_RADIUS, COLOR_DIAL_ACTIVE);
    
    // 回転マーカー描画
    drawRotationMarkers();
}

void JoystickDualDialUI::drawDialItem(uint8_t index, float angle, bool selected) {
    int item_x, item_y;
    polarToCartesian(angle, OUTER_DIAL_RADIUS - 8, item_x, item_y);
    
    if (selected) {
        // 選択中項目: 鮮やかな原色テーマカラー・大きめ
        uint16_t primary_color = DualDialUtils::getModeThemeColor(current_mode_);
        M5.Display.fillCircle(DIAL_CENTER_X + item_x, DIAL_CENTER_Y + item_y, 5, primary_color);
        M5.Display.drawCircle(DIAL_CENTER_X + item_x, DIAL_CENTER_Y + item_y, 6, COLOR_FUNCTION_NAME);  // 黄色枠
        
        // 選択項目名を外側に表示
        ModeDialConfig& config = mode_configs_[current_mode_];
        M5.Display.setTextColor(COLOR_FUNCTION_NAME);
        M5.Display.setTextSize(1);
        M5.Display.setTextDatum(MC_DATUM);
        
        int text_x, text_y;
        polarToCartesian(angle, OUTER_DIAL_RADIUS + 15, text_x, text_y);
        M5.Display.drawString(config.dial_items[index].name, 
                            DIAL_CENTER_X + text_x, DIAL_CENTER_Y + text_y);
    } else {
        // 非選択項目: 控えめな灰色・小さめ
        M5.Display.fillCircle(DIAL_CENTER_X + item_x, DIAL_CENTER_Y + item_y, 2, COLOR_DIAL_NORMAL);
    }
}

void JoystickDualDialUI::drawRotationMarkers() {
    // 内ダイアルの回転を表現する複数マーカー
    for (int i = 0; i < 8; i++) {
        float marker_angle = (2 * PI * i / 8) + operation_state_.inner_dial_rotation;
        int marker_x, marker_y;
        polarToCartesian(marker_angle, INNER_DIAL_RADIUS - 3, marker_x, marker_y);
        
        uint16_t color = (i == 0) ? COLOR_DIAL_SELECTED : COLOR_DIAL_NORMAL;
        M5.Display.fillCircle(DIAL_CENTER_X + marker_x, DIAL_CENTER_Y + marker_y, 1, color);
    }
}

void JoystickDualDialUI::drawProgressRing(int center_x, int center_y, int radius, float progress, uint16_t color) {
    int segments = 36; // 10度刻み
    int filled_segments = (int)(segments * progress);
    
    for (int i = 0; i < filled_segments; i++) {
        float angle = -PI/2 + (2 * PI * i / segments); // 12時位置から開始
        int x1, y1, x2, y2;
        
        polarToCartesian(angle, radius - 2, x1, y1);
        polarToCartesian(angle, radius + 2, x2, y2);
        
        M5.Display.drawLine(center_x + x1, center_y + y1, center_x + x2, center_y + y2, color);
    }
}

// ========== 入力処理ヘルパー ==========

void JoystickDualDialUI::updateDialRotations() {
    // 左スティック → 外ダイアル回転（項目選択）
    if (!isInDeadzone(operation_state_.left_stick_x, operation_state_.left_stick_y)) {
        float input_angle = atan2(-operation_state_.left_stick_y, operation_state_.left_stick_x);
        operation_state_.target_rotation = input_angle;
    }
    
    // 右スティック → 内ダイアル回転（値調整表現）
    if (!isInDeadzone(operation_state_.right_stick_x, operation_state_.right_stick_y)) {
        float rotation_speed = sqrt(operation_state_.right_stick_x * operation_state_.right_stick_x + 
                                  operation_state_.right_stick_y * operation_state_.right_stick_y);
        operation_state_.inner_dial_rotation += rotation_speed * 0.1f;
        operation_state_.inner_dial_rotation = normalizeAngle(operation_state_.inner_dial_rotation);
    }
    
    // 外ダイアルのスムーズ回転
    float rotation_diff = operation_state_.target_rotation - operation_state_.outer_dial_rotation;
    if (rotation_diff > PI) rotation_diff -= 2 * PI;
    if (rotation_diff < -PI) rotation_diff += 2 * PI;
    
    operation_state_.outer_dial_rotation += rotation_diff * 0.2f; // スムージング
    operation_state_.outer_dial_rotation = normalizeAngle(operation_state_.outer_dial_rotation);
}

void JoystickDualDialUI::updateItemSelection() {
    ModeDialConfig& config = mode_configs_[current_mode_];
    
    // 左スティックの角度から項目選択
    if (!isInDeadzone(operation_state_.left_stick_x, operation_state_.left_stick_y)) {
        float input_angle = atan2(-operation_state_.left_stick_y, operation_state_.left_stick_x);
        // 角度を0-2π範囲に正規化（ジャンプ回避）
        if (input_angle < 0) input_angle += 2 * PI;
        
        // 12時位置を0度とするため、90度（π/2）回転
        input_angle += PI/2;
        if (input_angle >= 2 * PI) input_angle -= 2 * PI;
        
        float item_angle_step = 2 * PI / config.active_item_count;
        uint8_t selected_index = (uint8_t)((input_angle + item_angle_step/2) / item_angle_step) % config.active_item_count;
        
        if (selected_index != operation_state_.selected_item_index) {
            operation_state_.selected_item_index = selected_index;
            state_changed_ = true;
        }
    }
}

void JoystickDualDialUI::updateValueAdjustment() {
    ModeDialConfig& config = mode_configs_[current_mode_];
    DialItem& current_item = config.dial_items[operation_state_.selected_item_index];
    
    // 右スティックX軸で値調整
    if (!isInDeadzone(operation_state_.right_stick_x, operation_state_.right_stick_y)) {
        float adjustment = operation_state_.right_stick_x * SENSITIVITY_MULTIPLIER;
        int value_range = current_item.max_value - current_item.min_value;
        int value_delta = (int)(adjustment * value_range * 0.01f); // 1%刻み
        
        if (abs(value_delta) > 0) {
            int new_value = current_item.current_value + value_delta;
            new_value = constrain(new_value, current_item.min_value, current_item.max_value);
            
            if (new_value != current_item.current_value) {
                current_item.current_value = new_value;
                state_changed_ = true;
                triggerValueChangeCallback();
            }
        }
    }
}

void JoystickDualDialUI::updateHoldConfirmation() {
    bool any_stick_pressed = operation_state_.left_stick_pressed || operation_state_.right_stick_pressed;
    
    if (any_stick_pressed && !operation_state_.hold_in_progress) {
        // ホールド開始
        operation_state_.hold_start_time = millis();
        operation_state_.hold_in_progress = true;
        operation_state_.hold_confirmed = false;
    } else if (!any_stick_pressed && operation_state_.hold_in_progress) {
        // ホールド中断
        operation_state_.hold_in_progress = false;
        operation_state_.hold_confirmed = false;
    } else if (operation_state_.hold_in_progress) {
        // ホールド中
        unsigned long hold_duration = millis() - operation_state_.hold_start_time;
        if (hold_duration >= HOLD_CONFIRM_TIME_MS && !operation_state_.hold_confirmed) {
            operation_state_.hold_confirmed = true;
            
            // 確定コールバック呼び出し
            if (confirm_callback_) {
                ModeDialConfig& config = mode_configs_[current_mode_];
                DialItem& current_item = config.dial_items[operation_state_.selected_item_index];
                confirm_callback_(operation_state_.selected_item_index, current_item.current_value);
            }
            
            Serial.printf("✅ ホールド確定: %s = %d\n", 
                        getCurrentDialItem().name, getCurrentDialItem().current_value);
        }
    }
}

// ========== ユーティリティ関数 ==========

float JoystickDualDialUI::normalizeAnalogInput(uint16_t raw_value) {
    return (raw_value - ANALOG_STICK_CENTER) / ANALOG_STICK_MAX;
}

bool JoystickDualDialUI::isInDeadzone(float x, float y) {
    return (sqrt(x*x + y*y) < DEADZONE_THRESHOLD);
}

void JoystickDualDialUI::polarToCartesian(float angle, float radius, int& x, int& y) {
    x = (int)(radius * cos(angle));
    y = (int)(radius * sin(angle));
}

float JoystickDualDialUI::calculateItemAngle(uint8_t index, uint8_t total_items) {
    return (2 * PI * index / total_items) - (PI / 2); // 12時位置を0度とする
}

float JoystickDualDialUI::normalizeAngle(float angle) {
    // 0から2πの範囲に正規化（ジャンプ回避）
    while (angle < 0) angle += 2 * PI;
    while (angle >= 2 * PI) angle -= 2 * PI;
    return angle;
}

// ========== モード設定初期化 ==========

void JoystickDualDialUI::initializeModeConfigs() {
    initializeLiveMode();
    initializeControlMode();
    initializeVideoMode(); 
    initializeMaintenanceMode();
    initializeSystemMode();
}

void JoystickDualDialUI::initializeLiveMode() {
    ModeDialConfig& config = mode_configs_[UI_MODE_LIVE];
    config.mode_name = "Live";
    config.theme_color = 0xFC00;  // オレンジ（Live用原色）
    config.active_item_count = 6;
    
    // ライブ操作項目設定
    config.dial_items[0] = {"Brightness", 0, 255, 200, "%", true};
    config.dial_items[1] = {"Speed", 0, 200, 100, "%", true};
    config.dial_items[2] = {"Effect", 0, 10, 0, "", true};
    config.dial_items[3] = {"Zoom", 50, 200, 100, "%", true};
    config.dial_items[4] = {"Rotate", -180, 180, 0, "°", true};
    config.dial_items[5] = {"Intensity", 0, 100, 75, "%", true};
}

void JoystickDualDialUI::initializeControlMode() {
    ModeDialConfig& config = mode_configs_[UI_MODE_CONTROL];
    config.mode_name = "Control";
    config.theme_color = COLOR_CONTROL_PRIMARY;
    config.active_item_count = 5;
    
    // 項目設定
    config.dial_items[0] = {"Brightness", 0, 255, 180, "%", true};
    config.dial_items[1] = {"ColorTemp", 2700, 6500, 4000, "K", true};
    config.dial_items[2] = {"RotationX", -180, 180, 0, "°", true};
    config.dial_items[3] = {"RotationY", -180, 180, 0, "°", true};
    config.dial_items[4] = {"Volume", 0, 100, 75, "%", true};
}

void JoystickDualDialUI::initializeVideoMode() {
    ModeDialConfig& config = mode_configs_[UI_MODE_VIDEO];
    config.mode_name = "Video";
    config.theme_color = COLOR_VIDEO_PRIMARY;
    config.active_item_count = 4;
    
    config.dial_items[0] = {"VideoID", 0, 10, 1, "", true};
    config.dial_items[1] = {"Volume", 0, 100, 75, "%", true};
    config.dial_items[2] = {"SeekPos", 0, 600, 0, "s", true};
    config.dial_items[3] = {"Speed", 50, 200, 100, "%", true};
}

void JoystickDualDialUI::initializeMaintenanceMode() {
    ModeDialConfig& config = mode_configs_[UI_MODE_MAINTENANCE];
    config.mode_name = "Maintain";
    config.theme_color = COLOR_MAINTAIN_PRIMARY;
    config.active_item_count = 5;
    
    config.dial_items[0] = {"Param0", 0, 255, 128, "", true};
    config.dial_items[1] = {"Param1", 0, 255, 64, "", true};
    config.dial_items[2] = {"Param2", 0, 255, 192, "", true};
    config.dial_items[3] = {"Param3", 0, 255, 32, "", true};
    config.dial_items[4] = {"Param4", 0, 255, 255, "", true};
}

void JoystickDualDialUI::initializeSystemMode() {
    ModeDialConfig& config = mode_configs_[UI_MODE_SYSTEM];
    config.mode_name = "System";
    config.theme_color = COLOR_SYSTEM_PRIMARY;
    config.active_item_count = 4;
    
    config.dial_items[0] = {"CPUTemp", 20, 80, 45, "°C", true};
    config.dial_items[1] = {"WiFi", 0, 8, 3, "dev", true};
    config.dial_items[2] = {"Memory", 0, 100, 60, "%", true};
    config.dial_items[3] = {"Uptime", 0, 86400, 3600, "s", true};
}

// ========== 状態取得 ==========

const DialItem& JoystickDualDialUI::getCurrentDialItem() const {
    return mode_configs_[current_mode_].dial_items[operation_state_.selected_item_index];
}

int JoystickDualDialUI::getCurrentSelectedValue() const {
    return getCurrentDialItem().current_value;
}

// ========== MQTT状態同期 ==========

void JoystickDualDialUI::syncFromMQTTState(const String& topic, int value) {
    ModeDialConfig* config = nullptr;
    DialItem* target_item = nullptr;
    
    // トピックに基づいて対象項目を特定
    if (topic.startsWith("control/")) {
        config = &mode_configs_[UI_MODE_CONTROL];
        if (topic == "control/brightness") target_item = &config->dial_items[0];
        else if (topic == "control/color_temp") target_item = &config->dial_items[1];
        else if (topic == "control/rotation_x") target_item = &config->dial_items[2];
        else if (topic == "control/rotation_y") target_item = &config->dial_items[3];
    } else if (topic.startsWith("video/")) {
        config = &mode_configs_[UI_MODE_VIDEO];
        if (topic == "video/selected_id") target_item = &config->dial_items[0];
        else if (topic == "video/volume") target_item = &config->dial_items[1];
        else if (topic == "video/seek_position") target_item = &config->dial_items[2];
        else if (topic == "video/playback_speed") target_item = &config->dial_items[3];
    } else if (topic.startsWith("adjust/")) {
        config = &mode_configs_[UI_MODE_MAINTENANCE];
        if (topic == "adjust/param_0") target_item = &config->dial_items[0];
        else if (topic == "adjust/param_1") target_item = &config->dial_items[1];
        else if (topic == "adjust/param_2") target_item = &config->dial_items[2];
        else if (topic == "adjust/param_3") target_item = &config->dial_items[3];
        else if (topic == "adjust/param_4") target_item = &config->dial_items[4];
    } else if (topic.startsWith("system/")) {
        config = &mode_configs_[UI_MODE_SYSTEM];
        if (topic == "system/cpu_temp") target_item = &config->dial_items[0];
        else if (topic == "system/wifi_clients") target_item = &config->dial_items[1];
        // system項目は基本的に読み取り専用
    }
    
    // 値更新
    if (target_item != nullptr && target_item->active) {
        int clamped_value = constrain(value, target_item->min_value, target_item->max_value);
        if (target_item->current_value != clamped_value) {
            target_item->current_value = clamped_value;
            state_changed_ = true;
            
            Serial.printf("🔄 MQTT同期更新: %s = %d\n", topic.c_str(), clamped_value);
        }
    }
}

void JoystickDualDialUI::syncFromMQTTState(const String& topic, float value) {
    // float値の場合は適切にint変換
    int int_value;
    
    if (topic == "video/playback_speed") {
        // 再生速度は0.5-2.0を50-200%として扱う
        int_value = (int)(value * 100);
    } else if (topic == "system/cpu_temp") {
        // 温度はそのまま
        int_value = (int)value;
    } else {
        // その他は四捨五入
        int_value = (int)(value + 0.5f);
    }
    
    syncFromMQTTState(topic, int_value);
}

// ========== 統計・デバッグ ==========

void JoystickDualDialUI::updateDrawStats(unsigned long draw_time_us) {
    draw_stats_.total_draws++;
    
    if (draw_time_us > draw_stats_.max_draw_time_us) {
        draw_stats_.max_draw_time_us = draw_time_us;
    }
    
    // 移動平均計算
    draw_stats_.avg_draw_time_us = 
        (draw_stats_.avg_draw_time_us * 9 + draw_time_us) / 10;
        
    // フレーム落ち検出（16.67ms = 60fps）
    if (draw_time_us > 16670) {
        draw_stats_.frame_drops++;
    }
}

void JoystickDualDialUI::checkStateChanges() {
    // 変更検出ロジック（必要に応じて実装）
}

void JoystickDualDialUI::triggerValueChangeCallback() {
    if (value_change_callback_) {
        // MQTT Topicを生成してコールバック
        String topic = "";
        switch (current_mode_) {
            case UI_MODE_CONTROL:
                if (operation_state_.selected_item_index == 0) topic = "control/brightness";
                else if (operation_state_.selected_item_index == 1) topic = "control/color_temp";
                break;
            case UI_MODE_VIDEO:
                if (operation_state_.selected_item_index == 1) topic = "video/volume";
                break;
            // 他のモードも同様
        }
        
        if (topic.length() > 0) {
            value_change_callback_(topic.c_str(), getCurrentSelectedValue());
        }
    }
}

// ========== コールバック設定 ==========

void JoystickDualDialUI::setValueChangeCallback(void (*callback)(const char* topic, int value)) {
    value_change_callback_ = callback;
}

void JoystickDualDialUI::setConfirmCallback(void (*callback)(uint8_t item_index, int value)) {
    confirm_callback_ = callback;
}

// ========== デバッグ関数 ==========

void JoystickDualDialUI::printDebugInfo() const {
    Serial.println("========== DualDialUI Debug Info ==========");
    Serial.printf("Mode: %s\n", mode_configs_[current_mode_].mode_name);
    Serial.printf("Selected Item: %d (%s)\n", 
                 operation_state_.selected_item_index,
                 getCurrentDialItem().name);
    Serial.printf("Current Value: %d %s\n", 
                 getCurrentDialItem().current_value,
                 getCurrentDialItem().unit);
    Serial.printf("Stick: L(%.2f,%.2f) R(%.2f,%.2f)\n",
                 operation_state_.left_stick_x, operation_state_.left_stick_y,
                 operation_state_.right_stick_x, operation_state_.right_stick_y);
    Serial.printf("Rotation: Outer=%.2f Inner=%.2f\n",
                 operation_state_.outer_dial_rotation, operation_state_.inner_dial_rotation);
    Serial.printf("Draw Stats: %lu draws, avg=%.1fms, max=%.1fms\n",
                 draw_stats_.total_draws, 
                 draw_stats_.avg_draw_time_us / 1000.0f,
                 draw_stats_.max_draw_time_us / 1000.0f);
}

// ========== ユーティリティ名前空間 ==========

namespace DualDialUtils {
    
    float degreesToRadians(float degrees) {
        return degrees * PI / 180.0f;
    }
    
    float radiansToDegrees(float radians) {
        return radians * 180.0f / PI;
    }
    
    uint16_t interpolateColor(uint16_t color1, uint16_t color2, float ratio) {
        ratio = constrain(ratio, 0.0f, 1.0f);
        
        uint8_t r1 = (color1 >> 11) & 0x1F;
        uint8_t g1 = (color1 >> 5) & 0x3F;
        uint8_t b1 = color1 & 0x1F;
        
        uint8_t r2 = (color2 >> 11) & 0x1F;
        uint8_t g2 = (color2 >> 5) & 0x3F;
        uint8_t b2 = color2 & 0x1F;
        
        uint8_t r = r1 + (uint8_t)((r2 - r1) * ratio);
        uint8_t g = g1 + (uint8_t)((g2 - g1) * ratio);
        uint8_t b = b1 + (uint8_t)((b2 - b1) * ratio);
        
        return (r << 11) | (g << 5) | b;
    }
    
    uint16_t getModeThemeColor(UIOperationMode mode) {
        switch (mode) {
            case UI_MODE_LIVE: return 0xFC00;  // オレンジ（Live用原色）
            case UI_MODE_CONTROL: return COLOR_CONTROL_PRIMARY;
            case UI_MODE_VIDEO: return COLOR_VIDEO_PRIMARY;
            case UI_MODE_MAINTENANCE: return COLOR_MAINTAIN_PRIMARY;
            case UI_MODE_SYSTEM: return COLOR_SYSTEM_PRIMARY;
            default: return COLOR_TEXT_PRIMARY;
        }
    }
    
    uint16_t getModeDarkColor(UIOperationMode mode) {
        switch (mode) {
            case UI_MODE_LIVE: return 0x1800;  // 10%暗いオレンジ（Live用）
            case UI_MODE_CONTROL: return COLOR_CONTROL_DARK;
            case UI_MODE_VIDEO: return COLOR_VIDEO_DARK;
            case UI_MODE_MAINTENANCE: return COLOR_MAINTAIN_DARK;
            case UI_MODE_SYSTEM: return COLOR_SYSTEM_DARK;
            default: return COLOR_BACKGROUND;
        }
    }
    
    uint16_t getModeMediumColor(UIOperationMode mode) {
        switch (mode) {
            case UI_MODE_LIVE: return 0x3800;  // 20%明度オレンジ（Live用）
            case UI_MODE_CONTROL: return COLOR_CONTROL_MEDIUM;
            case UI_MODE_VIDEO: return COLOR_VIDEO_MEDIUM;
            case UI_MODE_MAINTENANCE: return COLOR_MAINTAIN_MEDIUM;
            case UI_MODE_SYSTEM: return COLOR_SYSTEM_MEDIUM;
            default: return COLOR_DIAL_NORMAL;
        }
    }
    
    uint16_t getModeDimColor(UIOperationMode mode) {
        switch (mode) {
            case UI_MODE_LIVE: return 0x5800;  // 30%明度オレンジ（Live用）
            case UI_MODE_CONTROL: return COLOR_CONTROL_DIM;
            case UI_MODE_VIDEO: return COLOR_VIDEO_DIM;
            case UI_MODE_MAINTENANCE: return COLOR_MAINTAIN_DIM;
            case UI_MODE_SYSTEM: return COLOR_SYSTEM_DIM;
            default: return COLOR_DIAL_NORMAL;
        }
    }
    
    uint16_t getModeLightColor(UIOperationMode mode) {
        switch (mode) {
            case UI_MODE_LIVE: return COLOR_LIVE_LIGHT;
            case UI_MODE_CONTROL: return COLOR_CONTROL_LIGHT;
            case UI_MODE_VIDEO: return COLOR_VIDEO_LIGHT;
            case UI_MODE_MAINTENANCE: return COLOR_MAINTAIN_LIGHT;
            case UI_MODE_SYSTEM: return COLOR_SYSTEM_LIGHT;
            default: return COLOR_DIAL_NORMAL;
        }
    }
    
    uint16_t getModeBrightColor(UIOperationMode mode) {
        switch (mode) {
            case UI_MODE_LIVE: return COLOR_LIVE_BRIGHT;
            case UI_MODE_CONTROL: return COLOR_CONTROL_BRIGHT;
            case UI_MODE_VIDEO: return COLOR_VIDEO_BRIGHT;
            case UI_MODE_MAINTENANCE: return COLOR_MAINTAIN_BRIGHT;
            case UI_MODE_SYSTEM: return COLOR_SYSTEM_BRIGHT;
            default: return COLOR_DIAL_NORMAL;
        }
    }
    
    uint16_t getOptimalTextColor(uint16_t background_color) {
        // RGB565から輝度を計算
        uint8_t r = (background_color >> 11) & 0x1F;
        uint8_t g = (background_color >> 5) & 0x3F;
        uint8_t b = background_color & 0x1F;
        
        // 輝度計算（人間の視覚特性を考慮）
        float luminance = (0.299f * r + 0.587f * g + 0.114f * b) / 31.0f;
        
        // 輝度が0.5以下なら白文字、それ以上なら黒文字
        return (luminance < 0.5f) ? COLOR_HEADER_TEXT : COLOR_CONTRAST_TEXT;
    }
    
    String formatValue(int value, const char* unit) {
        return String(value) + String(unit);
    }
    
    String formatAngle(float radians) {
        float degrees = radiansToDegrees(radians);
        return String((int)degrees) + "°";
    }
}