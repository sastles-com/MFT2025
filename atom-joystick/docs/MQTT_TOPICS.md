# MQTT Topic階層設計書
## isolation-sphere分散制御システム

### 📋 概要
M5Stack Atom-JoyStick中央制御ハブによる分散MQTT制御システムのTopic階層設計。全デバイス間での統一的な状態管理・制御・同期を実現。

---

## 🏗️ Topic階層構造

### ベースプレフィックス
```
isolation-sphere/
```

### 主要カテゴリ
- `ui/` - ユーザーインターフェース状態
- `cmd/` - コマンド・制御指示
- `imu/` - IMUセンサーデータ
- `device/` - 個別デバイス制御
- `global/` - グローバル・システム共通
- `input/` - 入力デバイス（Joystick等）
- `hub/` - 中央ハブ（Atom-JoyStick）情報

---

## 📡 Topic詳細仕様

### 1. UI状態管理 (`ui/`)

#### 再生制御
```
isolation-sphere/ui/playback/state
- Payload: "play" | "pause" | "stop"
- QoS: 1, Retain: true
- 説明: 現在の再生状態
```

```
isolation-sphere/ui/playback/current_video
- Payload: {"id": 123, "name": "sample.mp4", "duration": 60}
- QoS: 1, Retain: true
- 説明: 現在再生中の動画情報
```

#### 表示設定
```
isolation-sphere/ui/settings/brightness
- Payload: 0-255
- QoS: 1, Retain: true
- 説明: LED明度設定
```

```
isolation-sphere/ui/settings/volume
- Payload: 0-100
- QoS: 1, Retain: true
- 説明: 音量設定（将来機能）
```

```
isolation-sphere/ui/settings/rotation_offset
- Payload: {"x": 0.0, "y": 0.0, "z": 0.0}
- QoS: 1, Retain: true
- 説明: 姿勢オフセット設定（度単位）
```

### 2. コマンド制御 (`cmd/`)

#### 再生制御コマンド
```
isolation-sphere/cmd/playback/toggle
- Payload: "1"
- QoS: 1, Retain: false
- 説明: 再生/一時停止切り替え
```

```
isolation-sphere/cmd/playback/next
- Payload: "1"
- QoS: 1, Retain: false
- 説明: 次の動画に切り替え
```

```
isolation-sphere/cmd/playback/previous
- Payload: "1"
- QoS: 1, Retain: false
- 説明: 前の動画に切り替え
```

#### 設定制御コマンド
```
isolation-sphere/cmd/settings/brightness_adjust
- Payload: "+10" | "-10" | "128"
- QoS: 1, Retain: false
- 説明: 明度調整（相対値・絶対値）
```

#### システム制御コマンド
```
isolation-sphere/cmd/system/restart
- Payload: "1"
- QoS: 1, Retain: false
- 説明: システム再起動
```

```
isolation-sphere/cmd/system/shutdown
- Payload: "1"
- QoS: 1, Retain: false
- 説明: システムシャットダウン
```

### 3. IMUセンサーデータ (`imu/`)

#### Quaternion姿勢データ
```
isolation-sphere/imu/quaternion
- Payload: {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0, "timestamp": 1625097600000}
- QoS: 0, Retain: false
- 説明: BNO055 Quaternionデータ（30Hz配信）
```

#### 校正・診断データ
```
isolation-sphere/imu/calibration
- Payload: {"accel": 3, "gyro": 3, "mag": 3, "system": 3}
- QoS: 1, Retain: true
- 説明: IMU校正状態（0-3スケール）
```

```
isolation-sphere/imu/temperature
- Payload: 25.3
- QoS: 1, Retain: false
- 説明: IMU温度（摂氏）
```

### 4. 個別デバイス制御 (`device/{device_id}/`)

#### デバイス固有制御
```
isolation-sphere/device/{device_id}/cmd/brightness
- Payload: 0-255
- QoS: 1, Retain: false
- 説明: 特定デバイスの明度制御
```

```
isolation-sphere/device/{device_id}/cmd/pattern
- Payload: {"type": "solid", "color": "#FF0000"}
- QoS: 1, Retain: false
- 説明: LED表示パターン制御
```

#### デバイス状態報告
```
isolation-sphere/device/{device_id}/status/online
- Payload: {"online": true, "last_seen": 1625097600000, "version": "1.0.0"}
- QoS: 1, Retain: true
- 説明: デバイス生存確認（30秒間隔）
```

```
isolation-sphere/device/{device_id}/status/battery
- Payload: {"level": 85, "charging": false, "voltage": 3.7}
- QoS: 1, Retain: false
- 説明: バッテリー状態
```

#### デバイス設定
```
isolation-sphere/device/{device_id}/config/update
- Payload: {"wifi_ssid": "NewNetwork", "brightness": 200}
- QoS: 2, Retain: true
- 説明: デバイス設定更新
```

### 5. グローバル・システム共通 (`global/`)

#### 時刻同期
```
isolation-sphere/global/sync/timestamp
- Payload: 1625097600000
- QoS: 1, Retain: true
- 説明: システム標準時刻（Unix timestamp）
```

#### デバイス発見
```
isolation-sphere/global/discovery/announce
- Payload: {"device_id": "esp32_001", "type": "display", "capabilities": ["led", "imu"]}
- QoS: 1, Retain: false
- 説明: 新デバイス参加アナウンス
```

```
isolation-sphere/global/discovery/request
- Payload: "1"
- QoS: 1, Retain: false
- 説明: デバイス発見要求
```

#### システム設定
```
isolation-sphere/global/config/system
- Payload: {"default_brightness": 128, "sync_interval_ms": 100}
- QoS: 2, Retain: true
- 説明: システム全体設定
```

### 6. 入力デバイス (`input/`)

#### Joystick入力
```
isolation-sphere/input/joystick
- Payload: {"left": {"x": 0, "y": 0}, "right": {"x": 0, "y": 0}, "buttons": {"a": false, "b": false}}
- QoS: 0, Retain: false
- 説明: リアルタイムJoystick状態（100Hz）
```

```
isolation-sphere/input/joystick/events
- Payload: {"type": "button_press", "button": "a", "timestamp": 1625097600000}
- QoS: 1, Retain: false
- 説明: Joystickイベント通知
```

### 7. 中央ハブ (`hub/`)

#### ハブ状態
```
isolation-sphere/hub/status
- Payload: {"uptime_ms": 3600000, "connected_devices": 3, "wifi_clients": 2}
- QoS: 1, Retain: true
- 説明: Atom-JoyStickハブ状態（5秒間隔）
```

```
isolation-sphere/hub/wifi/clients
- Payload: [{"ip": "192.168.100.10", "mac": "AA:BB:CC:DD:EE:FF", "connected": 1625097600000}]
- QoS: 1, Retain: true
- 説明: WiFi接続クライアント一覧
```

---

## 🎯 Topic使用パターン

### 基本制御フロー
1. **Joystick入力** → `isolation-sphere/input/joystick/events`
2. **コマンド生成** → `isolation-sphere/cmd/playback/toggle`
3. **UI状態更新** → `isolation-sphere/ui/playback/state`
4. **デバイス同期** → 全ESP32デバイスが状態同期

### デバイス参加フロー
1. **発見要求** → `isolation-sphere/global/discovery/request`
2. **デバイス応答** → `isolation-sphere/global/discovery/announce`
3. **設定配信** → `isolation-sphere/device/{device_id}/config/update`
4. **状態同期開始** → 各種状態Topicを購読

### 障害対応フロー
1. **生存確認停止** → `isolation-sphere/device/{device_id}/status/online`
2. **自動フェイルオーバー** → 他デバイスへの制御移管
3. **復旧時自動復帰** → 設定・状態の自動同期

---

## ⚙️ QoS・Retain設計指針

### QoS設定
- **QoS 0**: リアルタイムデータ（IMU、Joystick）
- **QoS 1**: 制御コマンド・状態更新
- **QoS 2**: 重要な設定データ

### Retain設定
- **Retain true**: 永続的な状態・設定データ
- **Retain false**: 一時的なコマンド・イベントデータ

---

## 🔧 実装時の注意点

### Topic命名規則
- 小文字とアンダースコアのみ使用
- 階層は最大6レベルまで
- デバイスIDはMAC addressベース推奨

### ペイロード形式
- JSON形式を基本とする
- 数値のみの場合は文字列化
- タイムスタンプはUnix timestamp（ミリ秒）

### パフォーマンス考慮
- 高頻度データ（IMU、Joystick）はQoS 0
- 重複配信を避けるため適切なRetain設定
- Topic購読は必要最小限に留める

**この設計により、isolation-sphereの革新的分散制御システムが実現されます。**