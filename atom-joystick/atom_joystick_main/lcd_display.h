/*
 * LCD表示機能
 * isolation-sphere分散MQTT制御システム
 */

#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <M5Unified.h>
#include <string>

// 表示色定義
#define LCD_COLOR_BLACK     0x0000
#define LCD_COLOR_WHITE     0xFFFF
#define LCD_COLOR_RED       0xF800
#define LCD_COLOR_GREEN     0x07E0
#define LCD_COLOR_BLUE      0x001F
#define LCD_COLOR_YELLOW    0xFFE0
#define LCD_COLOR_CYAN      0x07FF
#define LCD_COLOR_MAGENTA   0xF81F
#define LCD_COLOR_ORANGE    0xFC00
#define LCD_COLOR_GRAY      0x8410

// 表示レイアウト設定
#define LCD_WIDTH           128
#define LCD_HEIGHT          128
#define LCD_FONT_SIZE_SMALL 1
#define LCD_FONT_SIZE_NORMAL 2
#define LCD_FONT_SIZE_LARGE 3

// システム状態構造体（外部定義参照）
struct SystemState {
  bool wifi_ap_active;
  bool mqtt_broker_active;
  int connected_devices;
  unsigned long uptime_ms;
  float cpu_temperature;
  int battery_level;
};

// 表示画面種別
enum DisplayScreen {
  SCREEN_STARTUP,
  SCREEN_STATUS,
  SCREEN_JOYSTICK,
  SCREEN_CLIENTS,
  SCREEN_DEBUG
};

// 関数プロトタイプ
bool lcd_display_init();
void lcd_display_update();
void lcd_display_clear();
void lcd_display_set_brightness(int brightness);

// 画面表示関数
void lcd_display_show_startup(const char* title, const char* version);
void lcd_display_show_status(const char* module, const char* status, bool is_success);
void lcd_display_show_action(const char* action, const char* target);
void lcd_display_update_system_info(const SystemState* state);
void lcd_display_show_joystick_info(int left_x, int left_y, int right_x, int right_y);
void lcd_display_show_client_list(int client_count, const char* client_info);
void lcd_display_show_debug_info(const char* debug_msg);

// 画面切り替え
void lcd_display_set_screen(DisplayScreen screen);
DisplayScreen lcd_display_get_current_screen();
void lcd_display_next_screen();

// ユーティリティ関数
void lcd_display_draw_text(int x, int y, const char* text, int font_size, uint16_t color);
void lcd_display_draw_centered_text(int y, const char* text, int font_size, uint16_t color);
void lcd_display_draw_progress_bar(int x, int y, int width, int height, int progress, uint16_t color);
void lcd_display_draw_status_icon(int x, int y, bool is_active);
void lcd_display_draw_connection_indicator(int x, int y, int signal_strength);
String lcd_display_format_uptime(unsigned long uptime_ms);
String lcd_display_format_temperature(float temp_celsius);

#endif // LCD_DISPLAY_H