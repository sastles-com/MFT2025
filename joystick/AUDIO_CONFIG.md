# 音響設定ガイド (Joystick Audio Configuration)

M5Stack Atom Joystickの詳細音響設定機能について説明します。

## 🎵 機能概要

### ✅ 実装済み機能

1. **詳細音響設定**: `config.json`の`joystick.audio`セクション
2. **マスター音量制御**: 全体音量の一括調整
3. **音種別音量制御**: クリック音、エラー音、成功音の個別設定
4. **起動音設定**: メロディ、音階テストの有効/無効
5. **ボタン音量調節**: リアルタイム音量変更（25%刻み）

## 📋 config.json 設定構造

### 基本設定
```json
{
  "joystick": {
    "audio": {
      "master_enabled": true,      // 音響機能全体のON/OFF
      "master_volume": 75,         // マスター音量 (0-100%)
      "buzzer": {
        "enabled": true,           // ブザー機能のON/OFF
        "volume": 50,              // ブザー基本音量 (0-100%)
        
        "test_sounds": {
          "enabled": true,         // テスト音のON/OFF
          "volume": 60             // テスト音専用音量 (0-100%)
        },
        
        "feedback_sounds": {
          "click_enabled": true,   // クリック音のON/OFF
          "click_volume": 40,      // クリック音音量 (0-100%)
          "error_enabled": true,   // エラー音のON/OFF
          "error_volume": 70,      // エラー音音量 (0-100%)
          "success_enabled": true, // 成功音のON/OFF
          "success_volume": 50     // 成功音音量 (0-100%)
        },
        
        "startup_sounds": {
          "enabled": true,         // 起動音のON/OFF
          "volume": 55,            // 起動音音量 (0-100%)
          "play_melody": true,     // 起動メロディの再生
          "play_scale_test": false // 音階テストの自動再生
        }
      }
    }
  }
}
```

## 🔧 音量計算方式

### 有効音量の算出
```
有効音量 = (マスター音量 × 個別音量) ÷ 100
```

### 設定例
- **マスター音量**: 75%
- **ブザー音量**: 50%
- **クリック音音量**: 40%

→ **実効クリック音量**: (75 × 50 × 40) ÷ 10000 = **15%**

## 🎮 ボタン操作による音量調節

M5ボタンを押すことで6つのテストモードが順次実行されます：

### モード0-3: 音響テスト
- **モード0**: クリック音 + 現在音量表示
- **モード1**: 音階テスト + 現在音量表示  
- **モード2**: 周波数スイープ + 現在音量表示
- **モード3**: 接続音 + 現在音量表示

### モード4-5: 音量調節
- **モード4**: 音量UP（25% → 50% → 75% → 100% → 25%...）
- **モード5**: 音量DOWN（100% → 75% → 50% → 25% → 100%...）

## 📊 シリアル出力例

### 音量調節時のログ
```
[Main] Button pressed - Test mode: 4
[JoystickBuzzer] Playing tone: 1500Hz, duty: 32, duration: 100ms
→ Volume UP: 50%
[MQTT] Publish: joystick/test = {"timestamp": 15432, "test_mode": 4, "button": "pressed", "volume": 50} (retain: false)

[Main] Button pressed - Test mode: 5  
[JoystickBuzzer] Playing tone: 1500Hz, duty: 16, duration: 100ms
→ Volume DOWN: 25%
[MQTT] Publish: joystick/test = {"timestamp": 18765, "test_mode": 5, "button": "pressed", "volume": 25} (retain: false)
```

### 音響テスト時のログ
```
[Main] Button pressed - Test mode: 1
[JoystickBuzzer] Playing C4 (262Hz)
[JoystickBuzzer] Playing tone: 262Hz, duty: 32, duration: 300ms
...
[JoystickBuzzer] Playing C5 (523Hz) 
[JoystickBuzzer] Playing tone: 523Hz, duty: 32, duration: 300ms
→ Musical scale test (Volume: 50%)
```

## 🛠️ 設定カスタマイズ例

### 静音設定
```json
{
  "master_enabled": false
}
```

### クリック音のみ有効
```json
{
  "feedback_sounds": {
    "click_enabled": true,
    "click_volume": 30,
    "error_enabled": false,
    "success_enabled": false
  }
}
```

### 起動音無効
```json
{
  "startup_sounds": {
    "enabled": false,
    "play_melody": false,
    "play_scale_test": false
  }
}
```

### 大音量設定
```json
{
  "master_volume": 100,
  "buzzer": {
    "volume": 80,
    "feedback_sounds": {
      "click_volume": 60,
      "error_volume": 90,
      "success_volume": 70
    }
  }
}
```

## 📡 MQTT連携

音量変更は自動的にMQTTで配信されます：

### トピック: `joystick/test`
```json
{
  "timestamp": 15432,
  "test_mode": 4,
  "button": "pressed", 
  "volume": 50
}
```

### トピック: `joystick/status` (10秒間隔)
```json
{
  "uptime": 60000,
  "clients": 1,
  "messages": 10,
  "topics": 3
}
```

## 🎯 技術仕様

- **PWM制御**: ESP32 LEDC PWM (GPIO5)
- **PWM分解能**: 8bit (0-255)
- **デフォルトデューティ**: 128 (50%)
- **周波数範囲**: 200Hz - 2000Hz（テスト済み）
- **音階範囲**: C4 (262Hz) - C5 (523Hz)
- **音量段階**: 4段階（25%, 50%, 75%, 100%）

---

**config.jsonでjoystick用音量設定とenable switch**が完全実装されました！🎵

設定ファイルで細かく音響制御でき、リアルタイムでボタン操作による音量調節も可能です。