/*
 * dual_core_basic.ino
 * M5Stack Atom-JoyStick デュアルコア基本実装サンプル
 * isolation-sphere 分散MQTT制御システム
 * 
 * ESP32-S3 デュアルコア活用デモ:
 * - Core 0: UI制御・Joystick入力処理（リアルタイム）
 * - Core 1: MQTT通信・システム管理（バックグラウンド）
 * 
 * 開発環境: Arduino IDE 2.x + M5Stack ボードマネージャー
 * ハードウェア: M5Stack Atom-JoyStick (ESP32-S3)
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// =====================================
// デュアルコア共有データ構造
// =====================================

typedef struct {
    // Joystick入力状態
    int16_t left_stick_x;      // -4095 ~ 4095
    int16_t left_stick_y;      // -4095 ~ 4095  
    int16_t right_stick_x;     // -4095 ~ 4095
    int16_t right_stick_y;     // -4095 ~ 4095
    bool left_button;          // true: 押下中
    bool right_button;         // true: 押下中
    bool mode_button;          // LCD中央ボタン
    
    // UI状態
    uint8_t current_mode;      // 0:sphere, 1:video, 2:adjust, 3:system
    uint16_t theme_color;      // 現在のテーマカラー
    bool mode_changed;         // モード変更フラグ
    
    // システム状態
    uint8_t wifi_clients;      // 接続クライアント数
    float cpu_temperature;     // CPU温度
    uint32_t free_heap;        // フリープヒープサイズ
    uint32_t mqtt_messages;    // MQTT送信メッセージカウンタ
    
    // 同期制御
    SemaphoreHandle_t mutex;   // 排他制御用
    bool system_running;       // システム稼働フラグ
} DualCoreSharedData;

// グローバル共有データ
DualCoreSharedData g_shared_data;

// =====================================
// モード定義・テーマカラー
// =====================================

#define MODE_ISOLATION_SPHERE  0
#define MODE_VIDEO_MANAGEMENT  1  
#define MODE_ADJUSTMENT        2
#define MODE_SYSTEM           3

// テーマカラー（RGB565）
const uint16_t THEME_COLORS[] = {
    0x001F,  // Blue (isolation-sphere)
    0x07E0,  // Green (video)
    0xFFE0,  // Yellow (adjustment)  
    0xF81F   // Magenta/Purple (system)
};

const char* MODE_NAMES[] = {
    "Sphere", "Video", "Adjust", "System"
};

// =====================================
// WiFi・MQTT設定
// =====================================

const char* WIFI_SSID = "IsolationSphere-Direct";
const char* WIFI_PASS = "isolation-sphere-direct";
const char* MQTT_SERVER = "192.168.100.1";
const int MQTT_PORT = 1883;

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

// =====================================
// Core 0: UI制御・リアルタイム処理
// =====================================

void core0_ui_task(void *parameters) {
    // Core 0専用初期化
    Serial.println("[Core 0] UI制御タスク開始");
    
    // M5Unified初期化（Core 0で実行）
    M5.begin();
    M5.Display.setBrightness(100);
    M5.Display.fillScreen(TFT_BLACK);
    
    // 初期画面表示
    display_current_mode();
    
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(33); // 30Hz実行
    
    while (g_shared_data.system_running) {
        // Joystick・ボタン入力読み取り
        read_joystick_inputs();
        
        // モード切替処理
        handle_mode_switching();
        
        // LCD表示更新
        update_lcd_display();
        
        // Watchdog Timer更新
        esp_task_wdt_reset();
        
        // 30Hz周期で実行
        vTaskDelayUntil(&last_wake_time, frequency);
    }
    
    Serial.println("[Core 0] UI制御タスク終了");
    vTaskDelete(NULL);
}

void read_joystick_inputs() {
    // Mutex取得してデータ更新
    if (xSemaphoreTake(g_shared_data.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        M5.update();
        
        // アナログスティック読み取り（-4095 ~ 4095に正規化）
        g_shared_data.left_stick_x = map(analogRead(1), 0, 4095, -4095, 4095);
        g_shared_data.left_stick_y = map(analogRead(2), 0, 4095, -4095, 4095);
        g_shared_data.right_stick_x = map(analogRead(8), 0, 4095, -4095, 4095);
        g_shared_data.right_stick_y = map(analogRead(9), 0, 4095, -4095, 4095);
        
        // ボタン状態読み取り
        g_shared_data.left_button = !digitalRead(3);   // プルアップ入力
        g_shared_data.right_button = !digitalRead(5);  // プルアップ入力
        g_shared_data.mode_button = M5.BtnA.wasPressed();
        
        xSemaphoreGive(g_shared_data.mutex);
    }
}

void handle_mode_switching() {
    static bool last_mode_button = false;
    
    // モードボタン押下でモード切替
    if (g_shared_data.mode_button && !last_mode_button) {
        if (xSemaphoreTake(g_shared_data.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            g_shared_data.current_mode = (g_shared_data.current_mode + 1) % 4;
            g_shared_data.theme_color = THEME_COLORS[g_shared_data.current_mode];
            g_shared_data.mode_changed = true;
            
            Serial.printf("[Core 0] モード切替: %s\n", MODE_NAMES[g_shared_data.current_mode]);
            
            xSemaphoreGive(g_shared_data.mutex);
        }
    }
    
    last_mode_button = g_shared_data.mode_button;
}

void update_lcd_display() {
    static uint32_t last_display_update = 0;
    static uint8_t last_mode = 255;
    
    uint32_t now = millis();
    
    // 60FPS更新制限
    if (now - last_display_update < 16) {
        return;
    }
    
    // モード変更時は即座に更新
    if (g_shared_data.mode_changed || g_shared_data.current_mode != last_mode) {
        display_current_mode();
        g_shared_data.mode_changed = false;
        last_mode = g_shared_data.current_mode;
    }
    
    // ステータス情報更新（1秒間隔）
    if (now - last_display_update > 1000) {
        display_status_info();
        last_display_update = now;
    }
}

void display_current_mode() {
    // 背景色でクリア
    M5.Display.fillScreen(TFT_BLACK);
    
    // テーマカラーでモード表示
    M5.Display.setTextColor(g_shared_data.theme_color);
    M5.Display.setTextSize(2);
    M5.Display.drawCentreString(MODE_NAMES[g_shared_data.current_mode], 64, 40, 1);
    
    // モードアイコン表示
    draw_mode_icon(g_shared_data.current_mode);
    
    // 操作ヒント表示
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.drawCentreString("Press to switch", 64, 110, 1);
}

void draw_mode_icon(uint8_t mode) {
    uint16_t color = THEME_COLORS[mode];
    
    switch (mode) {
        case MODE_ISOLATION_SPHERE:
            // 球体アイコン
            M5.Display.fillCircle(64, 20, 12, color);
            break;
            
        case MODE_VIDEO_MANAGEMENT:
            // 再生アイコン（三角形）
            M5.Display.fillTriangle(54, 10, 54, 30, 74, 20, color);
            break;
            
        case MODE_ADJUSTMENT:
            // 調整アイコン（歯車風）
            M5.Display.fillRect(58, 14, 12, 4, color);
            M5.Display.fillRect(62, 10, 4, 12, color);
            break;
            
        case MODE_SYSTEM:
            // システムアイコン（四角）
            M5.Display.drawRect(56, 12, 16, 16, color);
            M5.Display.drawRect(58, 14, 12, 12, color);
            break;
    }
}

void display_status_info() {
    // 下部にシステム情報表示
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_DARKGRAY);
    
    // WiFi接続数表示
    char wifi_info[32];
    snprintf(wifi_info, sizeof(wifi_info), "WiFi: %d clients", g_shared_data.wifi_clients);
    M5.Display.drawString(wifi_info, 2, 90, 1);
    
    // MQTT統計表示  
    char mqtt_info[32];
    snprintf(mqtt_info, sizeof(mqtt_info), "MQTT: %lu msgs", g_shared_data.mqtt_messages);
    M5.Display.drawString(mqtt_info, 2, 100, 1);
    
    // CPU温度表示
    char temp_info[32];
    snprintf(temp_info, sizeof(temp_info), "Temp: %.1fC", g_shared_data.cpu_temperature);
    M5.Display.drawString(temp_info, 2, 110, 1);
}

// =====================================
// Core 1: MQTT通信・システム管理
// =====================================

void core1_comm_task(void *parameters) {
    Serial.println("[Core 1] 通信管理タスク開始");
    
    // WiFi初期化・接続
    setup_wifi();
    
    // MQTT初期化
    mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt_client.setCallback(mqtt_message_callback);
    
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(100); // 10Hz実行
    
    while (g_shared_data.system_running) {
        // MQTT接続維持
        maintain_mqtt_connection();
        
        // Joystick状態をMQTT配信
        publish_joystick_state();
        
        // システム統計更新
        update_system_statistics();
        
        // 10Hz周期で実行
        vTaskDelayUntil(&last_wake_time, frequency);
    }
    
    Serial.println("[Core 1] 通信管理タスク終了");
    vTaskDelete(NULL);
}

void setup_wifi() {
    Serial.print("[Core 1] WiFi接続中...");
    WiFi.mode(WIFI_AP_STA);
    
    // APモード設定
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    Serial.printf(" AP開始: %s\n", WIFI_SSID);
    
    IPAddress ap_ip = WiFi.softAPIP();
    Serial.printf("[Core 1] AP IP: %s\n", ap_ip.toString().c_str());
}

void maintain_mqtt_connection() {
    if (!mqtt_client.connected()) {
        Serial.print("[Core 1] MQTT再接続中...");
        
        String client_id = "atom-joystick-" + String(WiFi.macAddress());
        
        if (mqtt_client.connect(client_id.c_str())) {
            Serial.println(" 接続成功");
            
            // Topic購読
            mqtt_client.subscribe("isolation-sphere/cmd/+");
            mqtt_client.subscribe("isolation-sphere/device/+/status");
        } else {
            Serial.printf(" 失敗 (RC=%d)\n", mqtt_client.state());
        }
    }
    
    mqtt_client.loop();
}

void publish_joystick_state() {
    static uint32_t last_publish = 0;
    uint32_t now = millis();
    
    // 100ms間隔で配信
    if (now - last_publish < 100) {
        return;
    }
    
    if (mqtt_client.connected()) {
        // JSON形式でJoystick状態を配信
        StaticJsonDocument<512> doc;
        
        if (xSemaphoreTake(g_shared_data.mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            doc["mode"] = g_shared_data.current_mode;
            doc["left_stick"]["x"] = g_shared_data.left_stick_x;
            doc["left_stick"]["y"] = g_shared_data.left_stick_y;
            doc["right_stick"]["x"] = g_shared_data.right_stick_x;
            doc["right_stick"]["y"] = g_shared_data.right_stick_y;
            doc["buttons"]["left"] = g_shared_data.left_button;
            doc["buttons"]["right"] = g_shared_data.right_button;
            doc["timestamp"] = now;
            
            xSemaphoreGive(g_shared_data.mutex);
        }
        
        char json_buffer[512];
        serializeJson(doc, json_buffer);
        
        // モード別Topic配信
        String topic = "isolation-sphere/joystick/mode" + String(g_shared_data.current_mode);
        
        if (mqtt_client.publish(topic.c_str(), json_buffer)) {
            g_shared_data.mqtt_messages++;
        }
    }
    
    last_publish = now;
}

void mqtt_message_callback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("[Core 1] MQTT受信: %s\n", topic);
    
    // ペイロード処理（基本実装）
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.printf("[Core 1] メッセージ: %s\n", message.c_str());
}

void update_system_statistics() {
    static uint32_t last_stats_update = 0;
    uint32_t now = millis();
    
    // 1秒間隔で統計更新
    if (now - last_stats_update < 1000) {
        return;
    }
    
    if (xSemaphoreTake(g_shared_data.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // WiFi接続数
        g_shared_data.wifi_clients = WiFi.softAPgetStationNum();
        
        // CPU温度（模擬値、実際はセンサー読み取り）
        g_shared_data.cpu_temperature = 45.0 + (float)(esp_random() % 100) / 10.0;
        
        // ヒープメモリ
        g_shared_data.free_heap = ESP.getFreeHeap();
        
        xSemaphoreGive(g_shared_data.mutex);
    }
    
    last_stats_update = now;
    
    // デバッグ出力
    Serial.printf("[Core 1] Stats - WiFi:%d, Temp:%.1f, Heap:%lu\n", 
        g_shared_data.wifi_clients, g_shared_data.cpu_temperature, g_shared_data.free_heap);
}

// =====================================
// メイン setup/loop
// =====================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=================================");
    Serial.println("M5Stack Atom-JoyStick デュアルコア");
    Serial.println("isolation-sphere 分散制御システム");
    Serial.println("=================================");
    
    // 共有データ初期化
    memset(&g_shared_data, 0, sizeof(g_shared_data));
    g_shared_data.mutex = xSemaphoreCreateMutex();
    g_shared_data.system_running = true;
    g_shared_data.current_mode = MODE_ISOLATION_SPHERE;
    g_shared_data.theme_color = THEME_COLORS[MODE_ISOLATION_SPHERE];
    
    if (g_shared_data.mutex == NULL) {
        Serial.println("ERROR: Mutex作成失敗");
        while (1) delay(1000);
    }
    
    Serial.printf("ESP32-S3 デュアルコア: %d cores available\n", ESP.getChipCores());
    Serial.printf("CPU周波数: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("PSRAM: %d KB\n", ESP.getPsramSize() / 1024);
    
    // Core 0タスク開始（UI制御）- 高優先度
    xTaskCreatePinnedToCore(
        core0_ui_task,           // タスク関数
        "UI_Control",           // タスク名
        8192,                   // スタックサイズ
        NULL,                   // パラメータ
        24,                     // 優先度（高）
        NULL,                   // タスクハンドル
        0                       // Core 0に固定
    );
    
    // Core 1タスク開始（通信管理）- 中優先度  
    xTaskCreatePinnedToCore(
        core1_comm_task,        // タスク関数
        "Comm_Management",      // タスク名
        16384,                  // スタックサイズ（大）
        NULL,                   // パラメータ
        18,                     // 優先度（中）
        NULL,                   // タスクハンドル
        1                       // Core 1に固定
    );
    
    Serial.println("デュアルコア タスク開始完了");
}

void loop() {
    // メインループは最小限
    // 主要処理はCore別タスクで実行
    
    static uint32_t last_heartbeat = 0;
    uint32_t now = millis();
    
    // 10秒間隔でハートビート出力
    if (now - last_heartbeat > 10000) {
        Serial.printf("[Main] システム稼働中 - Uptime: %lu秒\n", now / 1000);
        last_heartbeat = now;
    }
    
    delay(1000);
}