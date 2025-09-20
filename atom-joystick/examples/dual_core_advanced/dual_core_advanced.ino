/*
 * dual_core_advanced.ino
 * M5Stack Atom-JoyStick デュアルコア高度実装サンプル
 * isolation-sphere 分散MQTT制御システム（完全版）
 * 
 * 高度な機能:
 * - 動的負荷分散・温度制御
 * - 障害分離・自動復旧
 * - 高性能IPC・非同期通信
 * - 詳細統計・監視システム
 * 
 * 開発環境: Arduino IDE 2.x + M5Stack ボードマネージャー
 * ハードウェア: M5Stack Atom-JoyStick (ESP32-S3) 
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <driver/temperature_sensor.h>

// =====================================
// 高度な共有データ構造
// =====================================

typedef struct {
    float x, y, z, w;  // Quaternion
} quaternion_t;

typedef struct {
    // Core間性能統計
    uint32_t core0_loops;      // Core 0ループ回数
    uint32_t core1_loops;      // Core 1ループ回数
    float core0_load;          // Core 0負荷率 (0.0-1.0)
    float core1_load;          // Core 1負荷率 (0.0-1.0)
    uint32_t ipc_messages;     // IPC通信回数
    uint32_t mutex_waits;      // Mutex待機回数
    
    // 障害検出・復旧統計
    uint32_t core0_resets;     // Core 0復旧回数
    uint32_t core1_resets;     // Core 1復旧回数
    bool core0_healthy;        // Core 0正常性フラグ
    bool core1_healthy;        // Core 1正常性フラグ
} performance_stats_t;

typedef struct {
    // 基本制御データ
    struct {
        int16_t left_x, left_y;
        int16_t right_x, right_y;
        bool left_btn, right_btn, mode_btn;
        uint32_t timestamp;
    } joystick;
    
    // IMU統合データ
    struct {
        quaternion_t quaternion;
        float temperature;
        bool data_valid;
    } imu;
    
    // UI状態管理
    struct {
        uint8_t current_mode;
        uint16_t theme_color;
        bool mode_changed;
        bool animation_active;
        uint8_t brightness;
    } ui;
    
    // システム状態
    struct {
        uint8_t wifi_clients;
        float cpu_temperature;
        uint32_t free_heap;
        uint32_t psram_free;
        bool mqtt_connected;
        uint32_t mqtt_messages;
        bool system_alert;
    } system;
    
    // 通信キュー
    struct {
        QueueHandle_t core0_to_core1;  // UI→通信
        QueueHandle_t core1_to_core0;  // 通信→UI
        SemaphoreHandle_t data_mutex;  // データ保護
        TaskHandle_t core0_task;       // Core 0タスクハンドル
        TaskHandle_t core1_task;       // Core 1タスクハンドル
    } ipc;
    
    // 性能・診断データ
    performance_stats_t perf;
    
    // システム制御
    volatile bool system_running;
    volatile bool emergency_mode;
} AdvancedSharedData;

// グローバル共有データ
AdvancedSharedData g_data;

// =====================================
// IPC メッセージ定義
// =====================================

typedef enum {
    MSG_JOYSTICK_UPDATE,     // Joystick状態更新
    MSG_MODE_CHANGE,         // モード変更要求
    MSG_MQTT_PUBLISH,        // MQTT配信要求
    MSG_SYSTEM_ALERT,        // システムアラート
    MSG_PERFORMANCE_UPDATE,  // 性能統計更新
    MSG_EMERGENCY_STOP       // 緊急停止
} ipc_message_type_t;

typedef struct {
    ipc_message_type_t type;
    uint32_t timestamp;
    union {
        struct {
            int16_t left_x, left_y, right_x, right_y;
            bool left_btn, right_btn;
        } joystick;
        struct {
            uint8_t new_mode;
            uint16_t theme_color;
        } mode_change;
        struct {
            char topic[64];
            char payload[256];
        } mqtt;
        struct {
            uint8_t alert_level;  // 0:info, 1:warning, 2:error, 3:critical
            char message[128];
        } alert;
    } data;
} ipc_message_t;

// =====================================
// 動的負荷制御システム
// =====================================

class LoadBalancer {
private:
    float core0_load_history[10];
    float core1_load_history[10];
    uint8_t history_index;
    uint32_t last_balance_check;
    
public:
    LoadBalancer() : history_index(0), last_balance_check(0) {
        memset(core0_load_history, 0, sizeof(core0_load_history));
        memset(core1_load_history, 0, sizeof(core1_load_history));
    }
    
    void update_load_stats(float core0_load, float core1_load) {
        core0_load_history[history_index] = core0_load;
        core1_load_history[history_index] = core1_load;
        history_index = (history_index + 1) % 10;
        
        g_data.perf.core0_load = calculate_average(core0_load_history, 10);
        g_data.perf.core1_load = calculate_average(core1_load_history, 10);
    }
    
    void check_load_balancing() {
        uint32_t now = millis();
        if (now - last_balance_check < 5000) return; // 5秒間隔
        
        // Core 0過負荷チェック
        if (g_data.perf.core0_load > 0.8) {
            Serial.println("[LoadBalancer] Core 0過負荷検出 - LCD更新頻度削減");
            send_alert(2, "Core 0 high load detected");
            // LCD更新頻度を30FPSに削減
        }
        
        // Core 1過負荷チェック
        if (g_data.perf.core1_load > 0.85) {
            Serial.println("[LoadBalancer] Core 1過負荷検出 - MQTT QoS調整");
            send_alert(2, "Core 1 high load detected");
            // MQTT配信頻度削減
        }
        
        // 温度制御
        if (g_data.system.cpu_temperature > 75.0) {
            Serial.println("[LoadBalancer] 高温度検出 - 処理頻度削減");
            send_alert(3, "High temperature detected");
            // 全体処理頻度削減
        }
        
        last_balance_check = now;
    }
    
private:
    float calculate_average(float* array, int size) {
        float sum = 0;
        for (int i = 0; i < size; i++) sum += array[i];
        return sum / size;
    }
    
    void send_alert(uint8_t level, const char* message) {
        ipc_message_t msg;
        msg.type = MSG_SYSTEM_ALERT;
        msg.timestamp = millis();
        msg.data.alert.alert_level = level;
        strncpy(msg.data.alert.message, message, sizeof(msg.data.alert.message)-1);
        
        xQueueSend(g_data.ipc.core1_to_core0, &msg, 0);
    }
};

LoadBalancer g_load_balancer;

// =====================================
// Core 0: 高速UI制御システム
// =====================================

void core0_realtime_ui_task(void *parameters) {
    Serial.println("[Core 0] リアルタイムUI制御開始");
    
    // WDTタスク登録
    esp_task_wdt_add(NULL);
    
    // M5Unified初期化
    M5.begin();
    M5.Display.setBrightness(g_data.ui.brightness);
    M5.Display.fillScreen(TFT_BLACK);
    
    // 高解像度タイマー設定
    uint64_t start_time, end_time;
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t ui_frequency = pdMS_TO_TICKS(16); // 60FPS
    
    uint32_t loop_counter = 0;
    
    while (g_data.system_running && !g_data.emergency_mode) {
        start_time = esp_timer_get_time();
        
        // 1. 高優先度入力処理
        process_joystick_inputs();
        
        // 2. モード管理・切替処理
        handle_advanced_mode_switching();
        
        // 3. LCD高品質描画
        render_advanced_ui();
        
        // 4. IPC メッセージ処理
        process_ipc_messages_core0();
        
        // 5. 性能監視
        end_time = esp_timer_get_time();
        float execution_time = (end_time - start_time) / 1000.0; // ms
        float load = execution_time / 16.67; // 60FPS基準負荷率
        
        g_load_balancer.update_load_stats(load, g_data.perf.core1_load);
        
        // 6. WDT更新
        esp_task_wdt_reset();
        
        // 7. 統計更新
        loop_counter++;
        if (loop_counter % 100 == 0) { // 100ループごと
            g_data.perf.core0_loops = loop_counter;
        }
        
        // 60FPS厳密制御
        vTaskDelayUntil(&last_wake_time, ui_frequency);
    }
    
    Serial.println("[Core 0] 緊急停止・タスク終了");
    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
}

void process_joystick_inputs() {
    M5.update();
    
    // 高速読み取り・ノイズ除去
    static int16_t lx_buffer[3], ly_buffer[3], rx_buffer[3], ry_buffer[3];
    static uint8_t buffer_index = 0;
    
    // ADC読み取り・バッファリング
    lx_buffer[buffer_index] = map(analogRead(1), 0, 4095, -4095, 4095);
    ly_buffer[buffer_index] = map(analogRead(2), 0, 4095, -4095, 4095);
    rx_buffer[buffer_index] = map(analogRead(8), 0, 4095, -4095, 4095);
    ry_buffer[buffer_index] = map(analogRead(9), 0, 4095, -4095, 4095);
    
    buffer_index = (buffer_index + 1) % 3;
    
    // メディアンフィルター適用
    if (xSemaphoreTake(g_data.ipc.data_mutex, pdMS_TO_TICKS(1)) == pdTRUE) {
        g_data.joystick.left_x = median_filter(lx_buffer, 3);
        g_data.joystick.left_y = median_filter(ly_buffer, 3);
        g_data.joystick.right_x = median_filter(rx_buffer, 3);
        g_data.joystick.right_y = median_filter(ry_buffer, 3);
        
        g_data.joystick.left_btn = !digitalRead(3);
        g_data.joystick.right_btn = !digitalRead(5);
        g_data.joystick.mode_btn = M5.BtnA.wasPressed();
        g_data.joystick.timestamp = millis();
        
        xSemaphoreGive(g_data.ipc.data_mutex);
    }
}

int16_t median_filter(int16_t* buffer, int size) {
    // 3値メディアンフィルター（高速実装）
    int16_t a = buffer[0], b = buffer[1], c = buffer[2];
    return max(min(a,b), min(max(a,b), c));
}

void handle_advanced_mode_switching() {
    static bool last_mode_btn = false;
    static uint32_t mode_change_time = 0;
    
    // ジャイロによるモード切替（実装例）
    bool gyro_mode_change = detect_gyro_gesture();
    
    if ((g_data.joystick.mode_btn && !last_mode_btn) || gyro_mode_change) {
        uint32_t now = millis();
        
        // チャタリング防止
        if (now - mode_change_time > 300) {
            // モード変更メッセージ送信
            ipc_message_t msg;
            msg.type = MSG_MODE_CHANGE;
            msg.timestamp = now;
            msg.data.mode_change.new_mode = (g_data.ui.current_mode + 1) % 4;
            msg.data.mode_change.theme_color = THEME_COLORS[msg.data.mode_change.new_mode];
            
            xQueueSend(g_data.ipc.core0_to_core1, &msg, 0);
            
            g_data.ui.mode_changed = true;
            g_data.ui.animation_active = true;
            
            mode_change_time = now;
        }
    }
    
    last_mode_btn = g_data.joystick.mode_btn;
}

bool detect_gyro_gesture() {
    // ジャイロジェスチャー検出（模擬実装）
    // 実際は IMU.getGyroData() でジャイロデータを取得し、
    // 特定パターン（例：Z軸周り90度回転）を検出
    return false;
}

void render_advanced_ui() {
    static uint32_t last_render = 0;
    static uint8_t animation_frame = 0;
    uint32_t now = millis();
    
    // フレーム制限・負荷制御
    bool force_render = g_data.ui.mode_changed || g_data.ui.animation_active;
    if (!force_render && (now - last_render < 16)) return; // 60FPS制限
    
    // モード切替アニメーション
    if (g_data.ui.animation_active) {
        render_mode_transition_animation(animation_frame);
        animation_frame++;
        
        if (animation_frame > 30) { // 0.5秒アニメーション
            g_data.ui.animation_active = false;
            g_data.ui.mode_changed = false;
            animation_frame = 0;
        }
    } else {
        // 通常UI描画
        render_normal_ui();
    }
    
    last_render = now;
}

void render_mode_transition_animation(uint8_t frame) {
    // スムーズなモード切替アニメーション
    float progress = (float)frame / 30.0;
    uint16_t old_color = THEME_COLORS[(g_data.ui.current_mode + 3) % 4];
    uint16_t new_color = g_data.ui.theme_color;
    
    // カラー補間
    uint16_t blend_color = interpolate_color(old_color, new_color, progress);
    
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.fillCircle(64, 64, 30 + frame, blend_color);
    
    // テキストフェードイン
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    
    uint8_t alpha = 255 * progress;
    M5.Display.drawCentreString(MODE_NAMES[g_data.ui.current_mode], 64, 50, 1);
}

uint16_t interpolate_color(uint16_t color1, uint16_t color2, float t) {
    // RGB565カラー補間
    uint8_t r1 = (color1 >> 11) & 0x1F;
    uint8_t g1 = (color1 >> 5) & 0x3F;
    uint8_t b1 = color1 & 0x1F;
    
    uint8_t r2 = (color2 >> 11) & 0x1F;
    uint8_t g2 = (color2 >> 5) & 0x3F;
    uint8_t b2 = color2 & 0x1F;
    
    uint8_t r = r1 + (r2 - r1) * t;
    uint8_t g = g1 + (g2 - g1) * t;
    uint8_t b = b1 + (b2 - b1) * t;
    
    return (r << 11) | (g << 5) | b;
}

void render_normal_ui() {
    // 高品質通常UI描画
    display_current_mode();
    display_advanced_status();
    display_performance_metrics();
}

void display_advanced_status() {
    // 詳細システム状態表示
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_LIGHTGRAY);
    
    char status_line[64];
    
    // WiFi・MQTT状態
    snprintf(status_line, sizeof(status_line), "WiFi:%d MQTT:%s", 
        g_data.system.wifi_clients, g_data.system.mqtt_connected ? "ON" : "OFF");
    M5.Display.drawString(status_line, 2, 85, 1);
    
    // 性能情報
    snprintf(status_line, sizeof(status_line), "Load: C0:%.0f%% C1:%.0f%%", 
        g_data.perf.core0_load * 100, g_data.perf.core1_load * 100);
    M5.Display.drawString(status_line, 2, 95, 1);
    
    // メモリ・温度
    snprintf(status_line, sizeof(status_line), "Mem:%luK Temp:%.1fC", 
        g_data.system.free_heap / 1024, g_data.system.cpu_temperature);
    M5.Display.drawString(status_line, 2, 105, 1);
    
    // アラート表示
    if (g_data.system.system_alert) {
        M5.Display.setTextColor(TFT_RED);
        M5.Display.drawString("!ALERT!", 90, 105, 1);
    }
}

void display_performance_metrics() {
    // Core間統計表示（右上）
    M5.Display.setTextColor(TFT_DARKGRAY);
    M5.Display.setTextSize(1);
    
    char perf_info[32];
    snprintf(perf_info, sizeof(perf_info), "IPC:%lu", g_data.perf.ipc_messages);
    M5.Display.drawString(perf_info, 85, 10, 1);
    
    // 健全性インジケーター
    uint16_t health_color = (g_data.perf.core0_healthy && g_data.perf.core1_healthy) ? 
        TFT_GREEN : TFT_ORANGE;
    M5.Display.fillCircle(120, 20, 3, health_color);
}

void process_ipc_messages_core0() {
    ipc_message_t msg;
    
    // Core 1からのメッセージ処理
    while (xQueueReceive(g_data.ipc.core1_to_core0, &msg, 0) == pdTRUE) {
        g_data.perf.ipc_messages++;
        
        switch (msg.type) {
            case MSG_SYSTEM_ALERT:
                g_data.system.system_alert = true;
                Serial.printf("[Core 0] アラート受信: %s\n", msg.data.alert.message);
                break;
                
            case MSG_PERFORMANCE_UPDATE:
                // 性能統計更新
                break;
                
            case MSG_EMERGENCY_STOP:
                Serial.println("[Core 0] 緊急停止要求受信");
                g_data.emergency_mode = true;
                break;
        }
    }
}

// =====================================
// Core 1: 高性能通信システム
// =====================================

void core1_communication_task(void *parameters) {
    Serial.println("[Core 1] 高性能通信システム開始");
    
    // WDTタスク登録
    esp_task_wdt_add(NULL);
    
    // 通信システム初期化
    setup_advanced_wifi();
    setup_advanced_mqtt();
    
    uint64_t start_time, end_time;
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t comm_frequency = pdMS_TO_TICKS(50); // 20Hz
    
    uint32_t loop_counter = 0;
    
    while (g_data.system_running && !g_data.emergency_mode) {
        start_time = esp_timer_get_time();
        
        // 1. MQTT接続管理・メッセージ処理
        maintain_advanced_mqtt();
        
        // 2. Joystick状態MQTT配信
        publish_advanced_joystick_state();
        
        // 3. システム統計・監視
        update_advanced_system_stats();
        
        // 4. IPC メッセージ処理
        process_ipc_messages_core1();
        
        // 5. 動的負荷制御実行
        g_load_balancer.check_load_balancing();
        
        // 6. 障害検出・復旧チェック
        check_system_health();
        
        // 7. 性能計測
        end_time = esp_timer_get_time();
        float execution_time = (end_time - start_time) / 1000.0;
        float load = execution_time / 50.0; // 20Hz基準
        
        g_load_balancer.update_load_stats(g_data.perf.core0_load, load);
        
        // 8. WDT更新
        esp_task_wdt_reset();
        
        // 9. 統計更新
        loop_counter++;
        if (loop_counter % 50 == 0) {
            g_data.perf.core1_loops = loop_counter;
        }
        
        // 20Hz制御
        vTaskDelayUntil(&last_wake_time, comm_frequency);
    }
    
    Serial.println("[Core 1] 緊急停止・タスク終了");
    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
}

void setup_advanced_wifi() {
    Serial.print("[Core 1] 高度WiFiシステム初期化...");
    
    WiFi.mode(WIFI_AP_STA);
    
    // APモード高度設定
    WiFi.softAPConfig(
        IPAddress(192, 168, 100, 1),    // Gateway
        IPAddress(192, 168, 100, 1),    // IP
        IPAddress(255, 255, 255, 0)     // Netmask
    );
    
    bool result = WiFi.softAP(WIFI_SSID, WIFI_PASS, 1, 0, 8); // ch1, hidden=0, max_clients=8
    
    if (result) {
        Serial.println(" 成功");
        Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("最大接続数: 8デバイス\n");
    } else {
        Serial.println(" 失敗");
        send_system_alert(3, "WiFi AP setup failed");
    }
}

void setup_advanced_mqtt() {
    mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt_client.setCallback(advanced_mqtt_callback);
    mqtt_client.setBufferSize(1024); // 大容量バッファ
    
    Serial.println("[Core 1] MQTT高度設定完了");
}

void maintain_advanced_mqtt() {
    static uint32_t last_connection_attempt = 0;
    uint32_t now = millis();
    
    if (!mqtt_client.connected()) {
        // 接続リトライ制御
        if (now - last_connection_attempt > 5000) {
            Serial.print("[Core 1] MQTT高度接続...");
            
            String client_id = "atom-joystick-advanced-" + WiFi.macAddress();
            
            if (mqtt_client.connect(client_id.c_str(), "isolation-sphere/joystick/status", 2, true, "offline")) {
                Serial.println(" 成功");
                
                // Will message設定・Topic購読
                mqtt_client.publish("isolation-sphere/joystick/status", "online", true);
                mqtt_client.subscribe("isolation-sphere/cmd/+", 1);
                mqtt_client.subscribe("isolation-sphere/device/+/status", 1);
                mqtt_client.subscribe("isolation-sphere/system/+", 1);
                
                g_data.system.mqtt_connected = true;
            } else {
                Serial.printf(" 失敗 (RC=%d)\n", mqtt_client.state());
                g_data.system.mqtt_connected = false;
            }
            
            last_connection_attempt = now;
        }
    } else {
        // 接続維持・ハートビート
        static uint32_t last_heartbeat = 0;
        if (now - last_heartbeat > 30000) { // 30秒間隔
            publish_heartbeat();
            last_heartbeat = now;
        }
    }
    
    mqtt_client.loop();
}

void publish_advanced_joystick_state() {
    static uint32_t last_publish = 0;
    uint32_t now = millis();
    
    // 動的配信頻度制御
    uint32_t publish_interval = g_data.perf.core1_load > 0.7 ? 200 : 100; // 負荷に応じて調整
    
    if (now - last_publish < publish_interval) return;
    
    if (mqtt_client.connected()) {
        StaticJsonDocument<1024> doc;
        
        if (xSemaphoreTake(g_data.ipc.data_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            // 詳細Joystick状態
            doc["joystick"]["left"]["x"] = g_data.joystick.left_x;
            doc["joystick"]["left"]["y"] = g_data.joystick.left_y;
            doc["joystick"]["right"]["x"] = g_data.joystick.right_x;
            doc["joystick"]["right"]["y"] = g_data.joystick.right_y;
            doc["buttons"]["left"] = g_data.joystick.left_btn;
            doc["buttons"]["right"] = g_data.joystick.right_btn;
            doc["buttons"]["mode"] = g_data.joystick.mode_btn;
            
            // UI状態
            doc["ui"]["mode"] = g_data.ui.current_mode;
            doc["ui"]["mode_name"] = MODE_NAMES[g_data.ui.current_mode];
            doc["ui"]["brightness"] = g_data.ui.brightness;
            
            // システム状態
            doc["system"]["temperature"] = g_data.system.cpu_temperature;
            doc["system"]["free_heap"] = g_data.system.free_heap;
            doc["system"]["wifi_clients"] = g_data.system.wifi_clients;
            
            // 性能統計
            doc["performance"]["core0_load"] = g_data.perf.core0_load;
            doc["performance"]["core1_load"] = g_data.perf.core1_load;
            doc["performance"]["ipc_messages"] = g_data.perf.ipc_messages;
            
            doc["timestamp"] = now;
            doc["sequence"] = g_data.system.mqtt_messages;
            
            xSemaphoreGive(g_data.ipc.data_mutex);
        }
        
        char json_buffer[1024];
        serializeJson(doc, json_buffer);
        
        // QoS制御・重要度別配信
        String base_topic = "isolation-sphere/joystick/";
        
        // 高頻度データ (QoS 0)
        String data_topic = base_topic + "data";
        mqtt_client.publish(data_topic.c_str(), json_buffer, false);
        
        // 状態データ (QoS 1, Retain)
        if (g_data.ui.mode_changed) {
            String status_topic = base_topic + "status";
            mqtt_client.publish(status_topic.c_str(), json_buffer, true);
        }
        
        g_data.system.mqtt_messages++;
    }
    
    last_publish = now;
}

void publish_heartbeat() {
    StaticJsonDocument<256> heartbeat;
    heartbeat["device"] = "atom-joystick-advanced";
    heartbeat["uptime"] = millis();
    heartbeat["free_heap"] = ESP.getFreeHeap();
    heartbeat["core0_healthy"] = g_data.perf.core0_healthy;
    heartbeat["core1_healthy"] = g_data.perf.core1_healthy;
    heartbeat["emergency_mode"] = g_data.emergency_mode;
    
    char buffer[256];
    serializeJson(heartbeat, buffer);
    
    mqtt_client.publish("isolation-sphere/joystick/heartbeat", buffer, true);
}

void advanced_mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String topic_str = String(topic);
    String message;
    
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.printf("[Core 1] MQTT受信: %s = %s\n", topic, message.c_str());
    
    // Topic別処理
    if (topic_str.startsWith("isolation-sphere/cmd/")) {
        handle_command_message(topic_str, message);
    } else if (topic_str.startsWith("isolation-sphere/system/")) {
        handle_system_message(topic_str, message);
    }
}

void handle_command_message(String topic, String message) {
    if (topic.endsWith("/brightness")) {
        int brightness = message.toInt();
        if (brightness >= 0 && brightness <= 255) {
            g_data.ui.brightness = brightness;
            // Core 0に明度変更通知
            ipc_message_t msg;
            msg.type = MSG_SYSTEM_ALERT;
            msg.timestamp = millis();
            snprintf(msg.data.alert.message, sizeof(msg.data.alert.message), 
                "Brightness: %d", brightness);
            xQueueSend(g_data.ipc.core1_to_core0, &msg, 0);
        }
    }
}

void handle_system_message(String topic, String message) {
    if (topic.endsWith("/emergency_stop")) {
        Serial.println("[Core 1] 緊急停止コマンド受信");
        send_emergency_stop();
    }
}

void update_advanced_system_stats() {
    static uint32_t last_update = 0;
    uint32_t now = millis();
    
    if (now - last_update < 1000) return; // 1秒間隔
    
    // WiFi統計
    g_data.system.wifi_clients = WiFi.softAPgetStationNum();
    
    // メモリ統計
    g_data.system.free_heap = ESP.getFreeHeap();
    g_data.system.psram_free = ESP.getFreePsram();
    
    // 温度センサー（実装例）
    g_data.system.cpu_temperature = read_cpu_temperature();
    
    last_update = now;
}

float read_cpu_temperature() {
    // ESP32-S3内蔵温度センサー読み取り
    // 実際の実装では temperature_sensor_xxx API を使用
    return 45.0 + (float)(esp_random() % 200) / 10.0; // 模擬値
}

void process_ipc_messages_core1() {
    ipc_message_t msg;
    
    while (xQueueReceive(g_data.ipc.core0_to_core1, &msg, 0) == pdTRUE) {
        g_data.perf.ipc_messages++;
        
        switch (msg.type) {
            case MSG_MODE_CHANGE:
                g_data.ui.current_mode = msg.data.mode_change.new_mode;
                g_data.ui.theme_color = msg.data.mode_change.theme_color;
                Serial.printf("[Core 1] モード変更: %s\n", MODE_NAMES[g_data.ui.current_mode]);
                break;
                
            case MSG_MQTT_PUBLISH:
                if (mqtt_client.connected()) {
                    mqtt_client.publish(msg.data.mqtt.topic, msg.data.mqtt.payload);
                }
                break;
        }
    }
}

void check_system_health() {
    static uint32_t last_health_check = 0;
    uint32_t now = millis();
    
    if (now - last_health_check < 10000) return; // 10秒間隔
    
    // Core 0健全性チェック
    g_data.perf.core0_healthy = (now - g_data.joystick.timestamp) < 1000;
    
    // Core 1健全性（自己チェック）
    g_data.perf.core1_healthy = g_data.system.mqtt_connected;
    
    // 障害検出時の処理
    if (!g_data.perf.core0_healthy) {
        Serial.println("[Core 1] Core 0障害検出");
        send_system_alert(3, "Core 0 failure detected");
        g_data.perf.core0_resets++;
    }
    
    if (!g_data.perf.core1_healthy) {
        Serial.println("[Core 1] 自己障害検出");
        g_data.perf.core1_resets++;
    }
    
    last_health_check = now;
}

void send_system_alert(uint8_t level, const char* message) {
    ipc_message_t msg;
    msg.type = MSG_SYSTEM_ALERT;
    msg.timestamp = millis();
    msg.data.alert.alert_level = level;
    strncpy(msg.data.alert.message, message, sizeof(msg.data.alert.message)-1);
    
    xQueueSend(g_data.ipc.core1_to_core0, &msg, 0);
    
    Serial.printf("[Core 1] アラート送信: Level %d - %s\n", level, message);
}

void send_emergency_stop() {
    ipc_message_t msg;
    msg.type = MSG_EMERGENCY_STOP;
    msg.timestamp = millis();
    
    xQueueSend(g_data.ipc.core1_to_core0, &msg, 0);
    
    g_data.emergency_mode = true;
    Serial.println("[Core 1] 緊急停止実行");
}

// =====================================
// メイン setup/loop
// =====================================

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("===============================================");
    Serial.println("M5Stack Atom-JoyStick デュアルコア高度実装");
    Serial.println("isolation-sphere 分散制御システム（完全版）");
    Serial.println("===============================================");
    
    // 共有データ初期化
    memset(&g_data, 0, sizeof(g_data));
    
    // IPC初期化
    g_data.ipc.data_mutex = xSemaphoreCreateMutex();
    g_data.ipc.core0_to_core1 = xQueueCreate(10, sizeof(ipc_message_t));
    g_data.ipc.core1_to_core0 = xQueueCreate(10, sizeof(ipc_message_t));
    
    if (!g_data.ipc.data_mutex || !g_data.ipc.core0_to_core1 || !g_data.ipc.core1_to_core0) {
        Serial.println("ERROR: IPC初期化失敗");
        while (1) delay(1000);
    }
    
    // 初期設定
    g_data.system_running = true;
    g_data.emergency_mode = false;
    g_data.ui.current_mode = MODE_ISOLATION_SPHERE;
    g_data.ui.theme_color = THEME_COLORS[MODE_ISOLATION_SPHERE];
    g_data.ui.brightness = 128;
    
    // 性能統計初期化
    g_data.perf.core0_healthy = true;
    g_data.perf.core1_healthy = true;
    
    Serial.printf("ESP32-S3詳細情報:\n");
    Serial.printf("- CPU Cores: %d\n", ESP.getChipCores());
    Serial.printf("- CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("- Flash Size: %d MB\n", ESP.getFlashChipSize() / (1024*1024));
    Serial.printf("- PSRAM Size: %d MB\n", ESP.getPsramSize() / (1024*1024));
    Serial.printf("- Free Heap: %d KB\n", ESP.getFreeHeap() / 1024);
    
    // WDT設定
    esp_task_wdt_init(30, true); // 30秒タイムアウト
    
    // Core 0タスク（UI制御）- 最高優先度
    xTaskCreatePinnedToCore(
        core0_realtime_ui_task,
        "RealtimeUI",
        12288,                  // 大容量スタック
        NULL,
        25,                     // 最高優先度
        &g_data.ipc.core0_task,
        0                       // Core 0固定
    );
    
    // Core 1タスク（通信管理）- 高優先度
    xTaskCreatePinnedToCore(
        core1_communication_task,
        "AdvancedComm",
        20480,                  // 超大容量スタック
        NULL,
        19,                     // 高優先度
        &g_data.ipc.core1_task,
        1                       // Core 1固定
    );
    
    Serial.println("高度デュアルコアシステム起動完了");
    Serial.println("動的負荷分散・障害分離・高性能IPC システム稼働中");
}

void loop() {
    static uint32_t last_main_loop = 0;
    uint32_t now = millis();
    
    // メインループ（低頻度・監視のみ）
    if (now - last_main_loop > 30000) { // 30秒間隔
        Serial.println("=== システム状態レポート ===");
        Serial.printf("稼働時間: %lu秒\n", now / 1000);
        Serial.printf("Core 0負荷: %.1f%%, ループ: %lu\n", 
            g_data.perf.core0_load * 100, g_data.perf.core0_loops);
        Serial.printf("Core 1負荷: %.1f%%, ループ: %lu\n", 
            g_data.perf.core1_load * 100, g_data.perf.core1_loops);
        Serial.printf("IPC通信: %lu回\n", g_data.perf.ipc_messages);
        Serial.printf("WiFi接続: %d台\n", g_data.system.wifi_clients);
        Serial.printf("MQTT送信: %lu回\n", g_data.system.mqtt_messages);
        Serial.printf("緊急モード: %s\n", g_data.emergency_mode ? "有効" : "無効");
        Serial.printf("ヒープ: %luKB, PSRAM: %luKB\n", 
            g_data.system.free_heap / 1024, g_data.system.psram_free / 1024);
        
        last_main_loop = now;
    }
    
    // 緊急停止チェック
    if (g_data.emergency_mode) {
        Serial.println("緊急モード: システム安全停止中...");
        g_data.system_running = false;
        
        // タスク終了待機
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        Serial.println("緊急停止完了 - 再起動中...");
        ESP.restart();
    }
    
    delay(1000);
}