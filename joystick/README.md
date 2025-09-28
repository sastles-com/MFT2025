# MFT2025 Joystick Controller

ESP32-S3ベースのM5Stack Atom JoyStickを使用したisolation-sphere制御用ジョイスティックコントローラー

## 🎮 Hardware Specifications

### M5Stack Atom JoyStick (K137)
**公式仕様**: [M5Stack Atom JoyStick Documentation](https://docs.m5stack.com/en/app/Atom%20JoyStick)

| 項目 | 仕様 |
|------|------|
| **MCU** | STM32F030F4P6 + AtomS3 (ESP32-S3) |
| **ブザー** | **内蔵パッシブブザー@5020** 🔊 |
| **ジョイスティック** | デュアル Hall センサー 5方向ジョイスティック × 2 |
| **ボタン** | 左右ファンクションボタン × 2 |
| **RGB LED** | WS2812C内蔵 |
| **バッテリー** | 300mAh高電圧リチウムバッテリー × 2 |
| **サイズ** | 84 x 60 x 31.5mm |
| **重量** | 63.5g |

## 🎵 Buzzer Implementation

### JoystickBuzzer Class (パッシブブザー制御)

**重要**: M5Stack Atom JoyStickには**内蔵パッシブブザー@5020**が搭載されているため、外部ブザー接続は不要です。

#### 実装仕様
- **制御方式**: ESP32 PWM制御 (GPIO5経由)  
- **対応周波数**: 200Hz～2000Hz (テスト範囲)
- **音量制御**: デューティ比による可変制御 (0-100%)

#### 機能一覧
| 機能 | メソッド | 説明 |
|------|----------|------|
| **音階テスト** | `playScaleTest()` | ドレミファソラシド (C4-C5) |
| **周波数スイープ** | `playFrequencySweep()` | 200-2000Hz連続スイープ |
| **メロディ再生** | `playStartupMelody()` | C-E-G-Cアルペジオ |
| **効果音** | `playClickTone()` | ボタンクリック音 |
| | `playErrorTone()` | エラー音（低音3回） |
| | `playCompletionTone()` | 完了音（上昇音程） |
| | `playConnectTone()` | 接続音（2音上昇） |
| | `playDisconnectTone()` | 切断音（2音下降） |
| | `playWarningTone()` | 警告音（高音2回） |

#### インタラクティブテスト
M5ボタンを押すと4種類のテストモードが順次切り替わります：

1. **Mode 0**: クリック音
2. **Mode 1**: 音階テスト (ドレミファソラシド)  
3. **Mode 2**: 周波数スイープテスト
4. **Mode 3**: 接続音

## 🏗️ Architecture

### コンポジションベース設計

従来の継承ベースから個別機能クラスの組み合わせに変更：

```cpp
// 個別機能クラス
ConfigManager*    gConfigManager     // 設定管理
WiFiManager*      gWiFiManager       // WiFi AP機能
JoystickBuzzer*   gJoystickBuzzer    // パッシブブザー制御
M5SpeakerBuzzer*  gM5Buzzer          // M5Unified Speaker (フォールバック)
```

### 初期化フロー

1. **M5Unified初期化** - ハードウェアベース設定
2. **LittleFS初期化** - ファイルシステム
3. **ConfigManager** - config.json読み込み
4. **WiFiManager** - AP mode (isolation-joystick)
5. **ブザー初期化** - M5Speaker → JoystickBuzzer自動フォールバック
6. **パッシブブザーテスト** - 音階・スイープ・メロディ

## 📡 Network Configuration

### WiFi Access Point
- **SSID**: `isolation-joystick`
- **IP Address**: `192.168.100.1`
- **Mode**: AP only (外部WiFi接続なし)
- **Purpose**: isolation-sphere直接制御用ローカルネットワーク

### MQTT Broker
- **Internal Broker**: 内蔵MQTTブローカー
- **Port**: 1883
- **Topics**: sphere/ui/# 階層構造

## 🔧 Build & Development

### Requirements
- **Platform**: PlatformIO with ESP32-S3 support
- **Board**: M5Stack AtomS3 (`atoms3r` environment)
- **Framework**: Arduino-ESP32

### Build Commands
```bash
# ビルド
pio run

# アップロード
pio run --target upload

# シリアルモニター  
pio device monitor -b 115200

# ファイルシステム (config.json等)
pio run --target uploadfs
```

### Project Structure
```
joystick/
├── src/
│   ├── main.cpp                 # メインロジック
│   ├── buzzer/
│   │   ├── JoystickBuzzer.cpp   # パッシブブザー制御
│   │   └── M5SpeakerBuzzer.cpp  # M5Unified Speaker
│   ├── config/
│   │   └── ConfigManager.cpp    # 設定管理
│   └── wifi/
│       └── WiFiManager.cpp      # WiFi AP管理
├── include/                     # ヘッダーファイル
├── data/
│   └── config.json             # 設定ファイル
└── test/                       # ユニットテスト
```

## ⚙️ Configuration

### config.json Structure
```json
{
  "joystick": {
    "system": {
      "buzzer_enabled": true,
      "buzzer_volume": 50,
      "device_name": "MFT2025-01"
    },
    "wifi": {
      "mode": "ap",
      "ap": {
        "ssid": "isolation-joystick",
        "local_ip": "192.168.100.1"
      }
    }
  }
}
```

## 🎯 Usage

### 基本動作確認

1. **電源投入** → 起動メロディ再生
2. **WiFi AP起動** → "isolation-joystick" ネットワーク開始  
3. **パッシブブザーテスト** → 音階・スイープ・メロディ自動再生
4. **ボタンテスト** → M5ボタンでテストモード切替

### シリアル出力例
```
[Main] M5SpeakerBuzzer failed, trying GPIO5 PWM...
[JoystickBuzzer] PWM initialized on GPIO5, channel 0
[Main] JoystickBuzzer (GPIO5) initialized
[Main] Testing Passive Buzzer on GPIO5...
[JoystickBuzzer] Playing musical scale test (passive buzzer)
[JoystickBuzzer] Playing C4 (262Hz)
[JoystickBuzzer] Playing D4 (294Hz)
...
```

## 🔗 References

- **M5Stack Atom JoyStick**: https://docs.m5stack.com/en/app/Atom%20JoyStick
- **Schematic**: [PDF](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/app/Atom%20JoyStick/Sch_AtomJoystick_v0.3.pdf)
- **GitHub Example**: [StampFly Controller](https://github.com/m5stack/Atom-JoyStick/tree/main/examples/StampFlyController)
- **Internal Firmware**: [Atom JoyStick Internal FW](https://github.com/m5stack/Atom-JoyStick-Internal-FW)

## 🏷️ License

MFT2025 Project - Isolation Sphere Joystick Controller

---
**Built with**: PlatformIO + Arduino-ESP32 + M5Unified  
**Hardware**: M5Stack Atom JoyStick (K137) with Built-in Passive Buzzer@5020