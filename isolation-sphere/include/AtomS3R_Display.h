#pragma once

#include <LovyanGFX.hpp>

class AtomS3R_Display : public lgfx::LGFX_Device
{
    lgfx::Panel_GC9107      _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    AtomS3R_Display(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 20000000;  // 保守的な速度設定
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = true;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            
            // M5Stack AtomS3R 公式ピン配置（M5GFXから確認済み）
            cfg.pin_sclk = 15;   // SPI_SCK
            cfg.pin_mosi = 21;   // SPI_MOSI  
            cfg.pin_miso = -1;   // not used
            cfg.pin_dc   = 42;   // DISP_RS (Data/Command)
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs    = 14;    // DISP_CS
            cfg.pin_rst   = 48;    // DISP_RST
            cfg.pin_busy  = -1;    // not used

            cfg.panel_width  = 128;
            cfg.panel_height = 128;
            cfg.offset_x     = 0;
            cfg.offset_y     = 32;  // AtomS3Rの画面オフセット
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable    = false;
            cfg.invert      = true;   // 表示反転
            cfg.rgb_order   = false;
            cfg.dlen_16bit  = false;
            cfg.bus_shared  = false;  // 専用バス使用

            _panel_instance.config(cfg);
        }

        // AtomS3R用の特別なバックライト制御は無効化
        // （M5GFXではI2C制御を使用しているが、LovyanGFXでは別途実装が必要）
        // {
        //     auto cfg = _light_instance.config();
        //     cfg.pin_bl = -1;     // バックライト制御無効
        //     cfg.invert = false;
        //     cfg.freq   = 500;
        //     cfg.pwm_channel = 1;
        //     _light_instance.config(cfg);
        //     _panel_instance.setLight(&_light_instance);
        // }

        setPanel(&_panel_instance);
    }
};