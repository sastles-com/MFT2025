# MQTTブローカー動作テスト

M5Stack Atom JoystickでMQTTブローカー機能をテストするためのガイドです。

## 🚀 MQTTブローカー機能

### 実装状況
✅ **WiFi AP機能** - `isolation-joystick` (192.168.100.1)  
✅ **MQTTブローカー内蔵** - ポート1883で動作  
✅ **パッシブブザー** - 内蔵ブザー@5020対応  
✅ **コンポジション設計** - モジュール化されたアーキテクチャ

## 🔧 テスト手順

### 1. ファームウェア書き込み＆起動

```bash
# ビルド＆書き込み
pio run --target upload

# シリアルモニター開始
pio device monitor -b 115200
```

### 2. 期待される起動ログ

```
==============================
 MFT2025 Joystick (Composition)
==============================
[Main] LittleFS mounted
[Main] Config loaded successfully
[Main] WiFiManager initialized
[Main] MqttBroker initialized
[MQTT] Broker started on port 1883
[JoystickBuzzer] PWM initialized on GPIO5, channel 0
[Main] JoystickBuzzer (GPIO5) initialized
```

### 3. WiFi接続確認

WiFiネットワーク **`isolation-joystick`** が表示される：
- **SSID**: `isolation-joystick`
- **パスワード**: なし（オープンネットワーク）
- **IP**: `192.168.100.1`

### 4. MQTTクライアントテスト

#### スマートフォンでテスト
1. **MQTT Dash** (Android) または **MQTTAnalyzer** (iOS) をインストール
2. WiFi「isolation-joystick」に接続
3. MQTTブローカー設定：
   - **Host**: `192.168.100.1`
   - **Port**: `1883`
   - **Username/Password**: 不要

#### PCでテスト (mosquitto)
```bash
# Ubuntu/macOS/WSL
sudo apt install mosquitto-clients  # Ubuntu
brew install mosquitto              # macOS

# 接続テスト
mosquitto_sub -h 192.168.100.1 -t "joystick/#" -v

# 別ターミナルで送信テスト
mosquitto_pub -h 192.168.100.1 -t "joystick/test" -m "hello"
```

### 5. ボタンテスト（M5ボタン）

M5ボタンを押すたびに以下が実行されます：

**テストモード0**: クリック音 + MQTTメッセージ送信
```
→ Click tone
→ MQTT test messages published
```

**テストモード1**: 音階テスト + MQTTメッセージ
```
→ Musical scale test
→ MQTT test messages published  
```

**テストモード2**: 周波数スイープ + MQTTメッセージ
```
→ Frequency sweep test
→ MQTT test messages published
```

**テストモード3**: 接続音 + MQTTメッセージ
```
→ Connect tone
→ MQTT test messages published
```

### 6. MQTT Topic構造

実装されているトピック：

```
joystick/test                    # ボタンテスト時のメッセージ
joystick/state                   # ジョイスティック状態（ダミーデータ）
joystick/status                  # 定期システム状態（10秒間隔）
joystick/system/status           # システムステータス  
joystick/system/wifi_clients     # WiFi接続クライアント数
```

## 📱 MQTTメッセージ例

### ボタンテストメッセージ
```json
{
  "timestamp": 12345,
  "test_mode": 1,
  "button": "pressed"
}
```

### ジョイスティック状態
```json
{
  "leftX": 0.00,
  "leftY": 0.00, 
  "rightX": 0.00,
  "rightY": 0.00,
  "buttonA": true,
  "buttonB": false,
  "leftClick": false,
  "rightClick": false
}
```

### システム状態
```json
{
  "uptime": 60000,
  "clients": 0,
  "messages": 5,
  "topics": 2
}
```

## 🔍 トラブルシューティング

### MQTTブローカーが起動しない

**症状**: `[MQTT] WiFi AP must be active before starting MQTT broker`

**対処法**:
1. WiFiManager が先に初期化されているか確認
2. `config.json` の `mqtt.enabled` が `true` か確認

### MQTTクライアントが接続できない

**症状**: 接続タイムアウト

**対処法**:
1. WiFi「isolation-joystick」に正しく接続しているか確認
2. ファイアウォールがポート1883をブロックしていないか確認
3. シリアルログで `[MQTT] Broker started on port 1883` が出力されているか確認

### メッセージが受信されない

**症状**: MQTTクライアントで購読してもメッセージが来ない

**対処法**:
1. M5ボタンを押してテストメッセージを送信
2. シリアルログで `[MQTT] Publish:` が出力されているか確認
3. 正しいトピック（`joystick/#`）で購読しているか確認

## ✅ 成功時のシリアルログ例

```
==============================
 MFT2025 Joystick (Composition)
==============================
[Main] LittleFS mounted
[Main] Config loaded successfully
[Main] WiFiManager initialized
[Main] MqttBroker initialized
[MQTT] Broker started on port 1883
[JoystickBuzzer] PWM initialized on GPIO5, channel 0
[Main] JoystickBuzzer (GPIO5) initialized
[Main] Testing Passive Buzzer on GPIO5...
...
[Main] Button pressed - Test mode: 0
[JoystickBuzzer] Playing click tone (1500Hz, 100ms)
[MQTT] Publish: joystick/test = {"timestamp": 15432, "test_mode": 0, "button": "pressed"} (retain: false)
[MQTT] Publish: joystick/state = {"leftX":0.00,"leftY":0.00,"rightX":0.00,"rightY":0.00,"buttonA":true,"buttonB":false,"leftClick":false,"rightClick":false} (retain: false)
→ MQTT test messages published
[Main] Status check - playing completion tone
[MQTT] Publish: joystick/status = {"uptime": 20000, "clients": 1, "messages": 3, "topics": 2} (retain: true)
→ MQTT Status: 1 clients, 3 messages
```

---

**WiFi AP機能＋MQTTブローカー＋パッシブブザー** の統合機能が完全に動作している状態です！