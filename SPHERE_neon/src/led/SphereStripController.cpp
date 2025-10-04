#include "led/SphereStripController.h"
#include <esp_log.h>

static const char* TAG = "SphereStripController";

// CRGB色定数の定義
const CRGB CRGB::Red(255, 0, 0);
const CRGB CRGB::Green(0, 255, 0);
const CRGB CRGB::Blue(0, 0, 255);
const CRGB CRGB::White(255, 255, 255);
const CRGB CRGB::Black(0, 0, 0);

SphereStripController::SphereStripController() 
    : leds(nullptr)
    , num_leds(0)
    , data_pin(GPIO_NUM_NC)
    , rmt_channel(RMT_CHANNEL_0)
    , initialized(false)
    , brightness(255) {
}

SphereStripController::~SphereStripController() {
    if (leds) {
        free(leds);
        leds = nullptr;
    }
    
    if (initialized) {
        rmt_driver_uninstall(rmt_channel);
    }
}

bool SphereStripController::initialize(gpio_num_t pin, uint16_t num) {
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return false;
    }
    
    if (pin < 0 || num == 0 || num > 2000) {
        ESP_LOGE(TAG, "Invalid parameters: pin=%d, num=%d", pin, num);
        return false;
    }
    
    data_pin = pin;
    num_leds = num;
    
    // LEDカラーバッファ確保
    leds = (CRGB*)malloc(sizeof(CRGB) * num_leds);
    if (!leds) {
        ESP_LOGE(TAG, "Failed to allocate LED buffer");
        return false;
    }
    
    // 初期化（全て黒）
    for (uint16_t i = 0; i < num_leds; i++) {
        leds[i] = CRGB::Black;
    }
    
    // RMT設定
    if (!configureRMT()) {
        free(leds);
        leds = nullptr;
        return false;
    }
    
    initialized = true;
    ESP_LOGI(TAG, "Initialized: pin=%d, num_leds=%d", data_pin, num_leds);
    return true;
}

bool SphereStripController::configureRMT() {
    rmt_config_t config = {};
    config.rmt_mode = RMT_MODE_TX;
    config.channel = rmt_channel;
    config.gpio_num = data_pin;
    config.mem_block_num = 1;
    config.clk_div = 2; // 80MHz / 2 = 40MHz
    
    config.tx_config.loop_en = false;
    config.tx_config.carrier_duty_percent = 50;
    config.tx_config.carrier_freq_hz = 38000;
    config.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
    config.tx_config.carrier_en = false;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    config.tx_config.idle_output_en = true;
    
    esp_err_t ret = rmt_config(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT config failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = rmt_driver_install(config.channel, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT driver install failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

void SphereStripController::colorToRMT(const CRGB& color, rmt_item32_t* rmt_data) {
    // WS2812Bは GRB順序
    uint32_t grb = ((uint32_t)color.g << 16) | ((uint32_t)color.r << 8) | color.b;
    
    // 輝度補正適用
    if (brightness != 255) {
        uint8_t scaled_g = (color.g * brightness) >> 8;
        uint8_t scaled_r = (color.r * brightness) >> 8;
        uint8_t scaled_b = (color.b * brightness) >> 8;
        grb = ((uint32_t)scaled_g << 16) | ((uint32_t)scaled_r << 8) | scaled_b;
    }
    
    // 24bitを RMTアイテムに変換（MSBファースト）
    for (int i = 0; i < 24; i++) {
        uint32_t bit = (grb >> (23 - i)) & 1;
        if (bit) {
            // 1bit: T1H=700ns, T1L=600ns (40MHz基準)
            rmt_data[i].duration0 = 28; // 700ns
            rmt_data[i].level0 = 1;
            rmt_data[i].duration1 = 24; // 600ns  
            rmt_data[i].level1 = 0;
        } else {
            // 0bit: T0H=350ns, T0L=800ns
            rmt_data[i].duration0 = 14; // 350ns
            rmt_data[i].level0 = 1;
            rmt_data[i].duration1 = 32; // 800ns
            rmt_data[i].level1 = 0;
        }
    }
}

bool SphereStripController::setLedColor(uint16_t index, const CRGB& color) {
    if (!initialized || index >= num_leds) {
        return false;
    }
    
    leds[index] = color;
    return true;
}

CRGB SphereStripController::getLedColor(uint16_t index) const {
    if (!initialized || index >= num_leds) {
        return CRGB::Black;
    }
    
    return leds[index];
}

void SphereStripController::clear() {
    if (!initialized) {
        return;
    }
    
    for (uint16_t i = 0; i < num_leds; i++) {
        leds[i] = CRGB::Black;
    }
}

bool SphereStripController::show() {
    if (!initialized) {
        return false;
    }
    
    // 全LEDデータをRMT形式に変換
    const size_t rmt_data_size = num_leds * 24 + 1; // +1 for reset
    rmt_item32_t* rmt_data = (rmt_item32_t*)malloc(sizeof(rmt_item32_t) * rmt_data_size);
    if (!rmt_data) {
        ESP_LOGE(TAG, "Failed to allocate RMT data buffer");
        return false;
    }
    
    // 各LEDをRMTデータに変換
    for (uint16_t i = 0; i < num_leds; i++) {
        colorToRMT(leds[i], &rmt_data[i * 24]);
    }
    
    // リセット信号（50us以上のLOW）
    rmt_data[num_leds * 24].duration0 = 2000; // 50us
    rmt_data[num_leds * 24].level0 = 0;
    rmt_data[num_leds * 24].duration1 = 0;
    rmt_data[num_leds * 24].level1 = 0;
    
    // RMT送信
    esp_err_t ret = rmt_write_items(rmt_channel, rmt_data, rmt_data_size, true);
    free(rmt_data);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT write failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

void SphereStripController::setBrightness(uint8_t new_brightness) {
    brightness = new_brightness;
}

uint8_t SphereStripController::getBrightness() const {
    return brightness;
}

uint16_t SphereStripController::getNumLeds() const {
    return num_leds;
}

gpio_num_t SphereStripController::getDataPin() const {
    return data_pin;
}

bool SphereStripController::isInitialized() const {
    return initialized;
}