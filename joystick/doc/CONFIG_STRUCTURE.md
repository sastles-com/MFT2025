# MFT2025 プロジェクト共通設定ガイド

## config.json 構造仕様

MFT2025プロジェクトでは、全モジュール（Isolation Sphere、Joystick、Raspberry Pi）で共通の`config.json`ファイルを使用します。

### 📋 基本原則

- **単一ファイル**: 全モジュールが同一の`config.json`を参照
- **階層構造**: `system`, `wifi`, `mqtt`, `sphere`, `joystick`, `raspi`の6つの主要セクション
- **モジュール固有設定**: 各モジュールは対応するセクションのみ使用
- **共通設定**: `system`, `wifi`, `mqtt`は全モジュールで共有

### 🏗️ 設定構造

```json
{
  "system": { /* 全モジュール共通システム設定 */ },
  "wifi": { /* 全モジュール共通WiFi設定 */ },
  "mqtt": { /* 全モジュール共通MQTT設定 */ },
  "ota": { /* 全モジュール共通OTA設定 */ },
  "movie": { /* 全モジュール共通メディア設定 */ },
  "paths": { /* 全モジュール共通パス設定 */ },
  "sphere": { /* Isolation Sphere固有設定 */ },
  "joystick": { /* Joystick固有設定 */ },
  "raspi": { /* Raspberry Pi固有設定 */ }
}
```

### 📖 各セクション詳細

#### `system` - システム共通設定
```json
{
  "name": "joystick-001",
  "PSRAM": true,
  "SPIFFS_size": 3.0,
  "debug": true,
  "hardware_revision": "v1.0"
}
```

#### `wifi` - WiFi共通設定
```json
{
  "enabled": true,
  "mode": "ap",
  "visible": true,
  "max_retries": 0,
  "ap": {
    "ssid": "isolation-joystick",
    "password": "",
    "local_ip": "192.168.100.1",
    "gateway": "192.168.100.1",
    "subnet": "255.255.255.0",
    "channel": 6,
    "hidden": false,
    "max_connections": 8
  }
}
```

#### `mqtt` - MQTT共通設定
```json
{
  "enabled": true,
  "broker": "192.168.100.1",
  "port": 1883,
  "username": "",
  "password": "",
  "keep_alive": 60,
  "topic": {
    "status": "sphere/status",
    "ui": "sphere/ui",
    "image": "sphere/image",
    "command": "sphere/command"
  }
}
```

#### `sphere` - Isolation Sphere固有設定
```json
{
  "instances": [ /* 複数Sphere管理配列 */ ],
  "display": { /* ディスプレイ設定 */ },
  "image": { /* 画像処理設定 */ },
  "imu": { /* IMUセンサー設定 */ },
  "ui": { /* UI設定 */ },
  "led": { /* LED制御設定 */ },
  "buzzer": { /* ブザー設定 */ },
  "storage": { /* ストレージ設定 */ },
  "paths": { /* パス設定 */ }
}
```

#### `joystick` - Joystick固有設定
```json
{
  "udp": {
    "target_ip": "192.168.100.100",
    "port": 8000,
    "update_interval_ms": 33,
    "joystick_read_interval_ms": 16,
    "max_retry_count": 3,
    "timeout_ms": 1000
  },
  "system": {
    "buzzer_enabled": true,
    "buzzer_volume": 50,
    "opening_animation_enabled": true,
    "lcd_brightness": 128,
    "debug_mode": false,
    "device_name": "joystick-001"
  },
  "audio": {
    "enabled": true,
    "master_volume": 20,
    "sounds": {
      "startup": true,
      "click": true,
      "error": true,
      "test": true
    },
    "volumes": {
      "startup": 15,
      "click": 40,
      "error": 70,
      "test": 60
    }
  },
  "input": {
    "deadzone": 0.1,
    "invert_left_y": false,
    "invert_right_y": false,
    "timestamp_offset_ms": 0,
    "sensitivity_profile": "normal"
  },
  "ui": {
    "use_dual_dial": true,
    "default_mode": "sphere_control",
    "button_debounce_ms": 50,
    "led_feedback": true
  }
}

**注意**: `instances`配列はsphereセクションにのみ存在。joystickとraspiは単一デバイス想定。

#### `raspi` - Raspberry Pi固有設定
```json
{
  "network": { /* ネットワーク設定 */ },
  "sync": { /* 同期設定 */ },
  "media": { /* メディア管理設定 */ }
}
```

## 🔧 実装での利用方法

### ConfigManager での読み込み

各モジュールのConfigManagerは、共通設定 + 対応するモジュール固有設定を読み込みます：

```cpp
// 全モジュール共通
config_.system = /* system セクション */;
config_.wifi = /* wifi セクション */;
config_.mqtt = /* mqtt セクション */;

// Joystick の場合
config_.joystick = /* joystick セクション */;

// Sphere の場合  
config_.sphere = /* sphere セクション */;
```

### 設定項目の追加・変更ルール

1. **全モジュール共通**: `system`, `wifi`, `mqtt`, `ota`, `paths` セクション
2. **モジュール固有**: 対応するセクション（`sphere`, `joystick`, `raspi`）
3. **階層の深さ**: 最大3階層まで（可読性維持）
4. **命名規則**: snake_case使用、略語は避ける

### 📝 変更手順

1. 設定を追加・変更する際は、このREADMEを更新
2. ConfigManagerの構造体定義を更新
3. ConfigManagerの読み込み処理を更新
4. テストケースを追加・更新

## ⚠️ 注意事項

- **破壊的変更**: config.jsonの構造変更時は全モジュールへの影響を考慮
- **デフォルト値**: ConfigManagerで適切なデフォルト値を設定
- **バリデーション**: 不正な設定値に対するエラーハンドリング実装
- **ドキュメント**: 新機能追加時はこのREADMEの更新必須

---

最終更新: 2025年9月28日  
この構造に関する質問や提案は開発チームまで。