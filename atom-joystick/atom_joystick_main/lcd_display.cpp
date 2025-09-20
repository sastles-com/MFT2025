/*
 * LCD表示機能実装
 * isolation-sphere分散MQTT制御システム
 */

#include "lcd_display.h"
#include <M5Unified.h>

// グローバル変数
static bool display_initialized = false;
static DisplayScreen current_screen = SCREEN_STARTUP;
static int display_brightness = 128; // 0-255
static unsigned long last_update = 0;
static unsigned long last_screen_change = 0;

// 内部関数プロトタイプ
static void draw_header();
static void draw_status_bar();
static void draw_screen_startup();
static void draw_screen_status();
static void draw_screen_joystick();
static void draw_screen_clients();
static void draw_screen_debug();

bool lcd_display_init() {
  Serial.println("📺 Initializing LCD Display...");
  
  // M5Unified LCD設定
  M5.Lcd.begin();
  M5.Lcd.setRotation(0); // 0度回転
  M5.Lcd.setBrightness(display_brightness);
  M5.Lcd.fillScreen(LCD_COLOR_BLACK);
  
  // フォント設定
  M5.Lcd.setTextSize(LCD_FONT_SIZE_NORMAL);
  M5.Lcd.setTextColor(LCD_COLOR_WHITE, LCD_COLOR_BLACK);
  
  display_initialized = true;
  
  Serial.println("✅ LCD Display initialized");
  Serial.printf("   Resolution: %dx%d\n", LCD_WIDTH, LCD_HEIGHT);
  Serial.printf("   Brightness: %d/255\n", display_brightness);
  
  return true;
}

void lcd_display_update() {
  if (!display_initialized) return;
  
  // 100ms間隔で更新
  if (millis() - last_update < 100) return;
  
  // 画面自動切り替え（30秒間隔）
  if (current_screen != SCREEN_STARTUP && millis() - last_screen_change > 30000) {
    lcd_display_next_screen();
    last_screen_change = millis();
  }
  
  // 現在の画面を描画
  switch (current_screen) {
    case SCREEN_STARTUP:
      draw_screen_startup();
      break;
    case SCREEN_STATUS:
      draw_screen_status();
      break;
    case SCREEN_JOYSTICK:
      draw_screen_joystick();
      break;
    case SCREEN_CLIENTS:
      draw_screen_clients();
      break;
    case SCREEN_DEBUG:
      draw_screen_debug();
      break;
  }
  
  last_update = millis();
}

void lcd_display_clear() {
  if (!display_initialized) return;
  M5.Lcd.fillScreen(LCD_COLOR_BLACK);
}

void lcd_display_set_brightness(int brightness) {
  display_brightness = constrain(brightness, 0, 255);
  if (display_initialized) {
    M5.Lcd.setBrightness(display_brightness);
  }
  Serial.printf("📺 LCD brightness set to: %d/255\n", display_brightness);
}

void lcd_display_show_startup(const char* title, const char* version) {
  if (!display_initialized) return;
  
  current_screen = SCREEN_STARTUP;
  M5.Lcd.fillScreen(LCD_COLOR_BLACK);
  
  // タイトル表示
  lcd_display_draw_centered_text(30, title, LCD_FONT_SIZE_NORMAL, LCD_COLOR_CYAN);
  lcd_display_draw_centered_text(50, "Control Hub", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  
  // バージョン表示
  String version_str = "v" + String(version);
  lcd_display_draw_centered_text(70, version_str.c_str(), LCD_FONT_SIZE_SMALL, LCD_COLOR_GRAY);
  
  // M5Stack Atom-JoyStick表示
  lcd_display_draw_centered_text(90, "M5Stack", LCD_FONT_SIZE_SMALL, LCD_COLOR_ORANGE);
  lcd_display_draw_centered_text(105, "Atom-JoyStick", LCD_FONT_SIZE_SMALL, LCD_COLOR_ORANGE);
}

void lcd_display_show_status(const char* module, const char* status, bool is_success) {
  if (!display_initialized) return;
  
  static int status_y = 20;
  
  // モジュール名表示
  M5.Lcd.setTextSize(LCD_FONT_SIZE_SMALL);
  M5.Lcd.setCursor(5, status_y);
  M5.Lcd.setTextColor(LCD_COLOR_WHITE, LCD_COLOR_BLACK);
  M5.Lcd.printf("%s:", module);
  
  // ステータス表示
  M5.Lcd.setCursor(70, status_y);
  uint16_t status_color = is_success ? LCD_COLOR_GREEN : LCD_COLOR_RED;
  M5.Lcd.setTextColor(status_color, LCD_COLOR_BLACK);
  M5.Lcd.print(status);
  
  status_y += 15;
  if (status_y > 110) status_y = 20; // リセット
}

void lcd_display_show_action(const char* action, const char* target) {
  if (!display_initialized) return;
  
  // 一時的なアクション表示
  M5.Lcd.fillRect(0, 100, LCD_WIDTH, 28, LCD_COLOR_BLUE);
  
  String action_text = String(action) + " " + String(target);
  lcd_display_draw_centered_text(110, action_text.c_str(), LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  
  delay(1000); // 1秒間表示
  M5.Lcd.fillRect(0, 100, LCD_WIDTH, 28, LCD_COLOR_BLACK); // クリア
}

void lcd_display_update_system_info(const SystemState* state) {
  if (!display_initialized || state == nullptr) return;
  
  // システム情報を内部で保持（画面描画で使用）
  static SystemState cached_state;
  cached_state = *state;
}

void lcd_display_show_joystick_info(int left_x, int left_y, int right_x, int right_y) {
  if (!display_initialized) return;
  
  // Joystick情報を内部で保持
  static int cached_left_x = left_x, cached_left_y = left_y;
  static int cached_right_x = right_x, cached_right_y = right_y;
  
  cached_left_x = left_x;
  cached_left_y = left_y;
  cached_right_x = right_x;
  cached_right_y = right_y;
}

void lcd_display_show_client_list(int client_count, const char* client_info) {
  // クライアント情報を内部で保持
  static int cached_client_count = client_count;
  static String cached_client_info = String(client_info ? client_info : "");
  
  cached_client_count = client_count;
  if (client_info) cached_client_info = String(client_info);
}

void lcd_display_show_debug_info(const char* debug_msg) {
  // デバッグ情報を内部で保持
  static String cached_debug_msg = String(debug_msg ? debug_msg : "");
  if (debug_msg) cached_debug_msg = String(debug_msg);
}

void lcd_display_set_screen(DisplayScreen screen) {
  if (current_screen != screen) {
    current_screen = screen;
    last_screen_change = millis();
    M5.Lcd.fillScreen(LCD_COLOR_BLACK); // 画面クリア
  }
}

DisplayScreen lcd_display_get_current_screen() {
  return current_screen;
}

void lcd_display_next_screen() {
  DisplayScreen next = (DisplayScreen)((int)current_screen + 1);
  if (next > SCREEN_DEBUG) next = SCREEN_STATUS;
  lcd_display_set_screen(next);
}

// 内部描画関数
static void draw_header() {
  // ヘッダー描画
  M5.Lcd.fillRect(0, 0, LCD_WIDTH, 15, LCD_COLOR_GRAY);
  lcd_display_draw_text(2, 2, "isolation-sphere", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  
  // 現在時刻表示（簡易）
  unsigned long uptime_sec = millis() / 1000;
  String time_str = String(uptime_sec / 60) + ":" + String(uptime_sec % 60, DEC);
  M5.Lcd.setCursor(90, 2);
  M5.Lcd.setTextSize(LCD_FONT_SIZE_SMALL);
  M5.Lcd.setTextColor(LCD_COLOR_WHITE);
  M5.Lcd.print(time_str);
}

static void draw_status_bar() {
  // ステータスバー描画
  M5.Lcd.fillRect(0, LCD_HEIGHT - 15, LCD_WIDTH, 15, LCD_COLOR_GRAY);
  
  // WiFi状態アイコン
  lcd_display_draw_status_icon(5, LCD_HEIGHT - 12, true); // WiFi AP
  
  // MQTT状態アイコン
  lcd_display_draw_status_icon(20, LCD_HEIGHT - 12, true); // MQTT Broker
  
  // 接続デバイス数
  M5.Lcd.setCursor(35, LCD_HEIGHT - 12);
  M5.Lcd.setTextSize(LCD_FONT_SIZE_SMALL);
  M5.Lcd.setTextColor(LCD_COLOR_WHITE);
  M5.Lcd.print("Dev:0"); // 実際の値は別途設定
}

static void draw_screen_startup() {
  // スタートアップ画面（既に実装済み）
}

static void draw_screen_status() {
  draw_header();
  
  // システム状態表示
  lcd_display_draw_text(5, 20, "System Status", LCD_FONT_SIZE_NORMAL, LCD_COLOR_CYAN);
  
  lcd_display_draw_text(5, 40, "WiFi AP:", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  lcd_display_draw_text(60, 40, "ACTIVE", LCD_FONT_SIZE_SMALL, LCD_COLOR_GREEN);
  
  lcd_display_draw_text(5, 55, "MQTT:", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  lcd_display_draw_text(60, 55, "ACTIVE", LCD_FONT_SIZE_SMALL, LCD_COLOR_GREEN);
  
  lcd_display_draw_text(5, 70, "Clients:", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  lcd_display_draw_text(60, 70, "0/8", LCD_FONT_SIZE_SMALL, LCD_COLOR_YELLOW);
  
  lcd_display_draw_text(5, 85, "Uptime:", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  String uptime = lcd_display_format_uptime(millis());
  lcd_display_draw_text(60, 85, uptime.c_str(), LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  
  draw_status_bar();
}

static void draw_screen_joystick() {
  draw_header();
  
  // Joystick状態表示
  lcd_display_draw_text(5, 20, "Joystick Input", LCD_FONT_SIZE_NORMAL, LCD_COLOR_CYAN);
  
  // 左スティック
  lcd_display_draw_text(5, 40, "Left:", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  lcd_display_draw_text(5, 55, "X: 0", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  lcd_display_draw_text(5, 70, "Y: 0", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  
  // 右スティック
  lcd_display_draw_text(65, 40, "Right:", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  lcd_display_draw_text(65, 55, "X: 0", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  lcd_display_draw_text(65, 70, "Y: 0", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  
  // Joystick可視化（円形表示）
  int center_x = LCD_WIDTH / 2;
  int center_y = 100;
  int radius = 15;
  
  // 左スティック円
  M5.Lcd.drawCircle(center_x - 30, center_y, radius, LCD_COLOR_WHITE);
  M5.Lcd.fillCircle(center_x - 30, center_y, 2, LCD_COLOR_RED); // センター点
  
  // 右スティック円
  M5.Lcd.drawCircle(center_x + 30, center_y, radius, LCD_COLOR_WHITE);
  M5.Lcd.fillCircle(center_x + 30, center_y, 2, LCD_COLOR_RED); // センター点
  
  draw_status_bar();
}

static void draw_screen_clients() {
  draw_header();
  
  // クライアント一覧表示
  lcd_display_draw_text(5, 20, "MQTT Clients", LCD_FONT_SIZE_NORMAL, LCD_COLOR_CYAN);
  
  lcd_display_draw_text(5, 40, "Connected: 0/8", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  
  // クライアント詳細（簡易）
  for (int i = 0; i < 4; i++) {
    int y = 55 + (i * 12);
    lcd_display_draw_text(5, y, "- No client", LCD_FONT_SIZE_SMALL, LCD_COLOR_GRAY);
  }
  
  draw_status_bar();
}

static void draw_screen_debug() {
  draw_header();
  
  // デバッグ情報表示
  lcd_display_draw_text(5, 20, "Debug Info", LCD_FONT_SIZE_NORMAL, LCD_COLOR_CYAN);
  
  lcd_display_draw_text(5, 40, "Heap Free:", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  lcd_display_draw_text(5, 55, String(ESP.getFreeHeap()).c_str(), LCD_FONT_SIZE_SMALL, LCD_COLOR_YELLOW);
  
  lcd_display_draw_text(5, 70, "CPU Temp:", LCD_FONT_SIZE_SMALL, LCD_COLOR_WHITE);
  lcd_display_draw_text(5, 85, "25.0C", LCD_FONT_SIZE_SMALL, LCD_COLOR_YELLOW);
  
  draw_status_bar();
}

// ユーティリティ関数実装
void lcd_display_draw_text(int x, int y, const char* text, int font_size, uint16_t color) {
  if (!display_initialized || !text) return;
  
  M5.Lcd.setCursor(x, y);
  M5.Lcd.setTextSize(font_size);
  M5.Lcd.setTextColor(color, LCD_COLOR_BLACK);
  M5.Lcd.print(text);
}

void lcd_display_draw_centered_text(int y, const char* text, int font_size, uint16_t color) {
  if (!display_initialized || !text) return;
  
  M5.Lcd.setTextSize(font_size);
  int text_width = strlen(text) * 6 * font_size; // 概算幅
  int x = (LCD_WIDTH - text_width) / 2;
  
  lcd_display_draw_text(x, y, text, font_size, color);
}

void lcd_display_draw_progress_bar(int x, int y, int width, int height, int progress, uint16_t color) {
  if (!display_initialized) return;
  
  progress = constrain(progress, 0, 100);
  int fill_width = (width * progress) / 100;
  
  // 外枠
  M5.Lcd.drawRect(x, y, width, height, LCD_COLOR_WHITE);
  
  // プログレス部分
  if (fill_width > 0) {
    M5.Lcd.fillRect(x + 1, y + 1, fill_width - 2, height - 2, color);
  }
}

void lcd_display_draw_status_icon(int x, int y, bool is_active) {
  if (!display_initialized) return;
  
  uint16_t color = is_active ? LCD_COLOR_GREEN : LCD_COLOR_RED;
  M5.Lcd.fillCircle(x, y, 3, color);
}

void lcd_display_draw_connection_indicator(int x, int y, int signal_strength) {
  if (!display_initialized) return;
  
  signal_strength = constrain(signal_strength, 0, 4);
  
  for (int i = 0; i < 4; i++) {
    uint16_t color = (i < signal_strength) ? LCD_COLOR_GREEN : LCD_COLOR_GRAY;
    int bar_height = (i + 1) * 2;
    M5.Lcd.fillRect(x + (i * 3), y - bar_height, 2, bar_height, color);
  }
}

String lcd_display_format_uptime(unsigned long uptime_ms) {
  unsigned long uptime_sec = uptime_ms / 1000;
  unsigned long hours = uptime_sec / 3600;
  unsigned long minutes = (uptime_sec % 3600) / 60;
  unsigned long seconds = uptime_sec % 60;
  
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours, minutes, seconds);
  return String(buffer);
}

String lcd_display_format_temperature(float temp_celsius) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%.1fC", temp_celsius);
  return String(buffer);
}