# M5Stack Atom-JoyStick 開発ワークフロー
## isolation-sphere分散MQTT制御システム

### 🎯 開発環境概要

**現在のPC（isolation-sphere本体）**:
- ESP32-S3開発環境（ESP-IDF）
- raspi制御システム（Python FastAPI）
- 既存UDP通信システム（30Hz IMUデータ）

**別PC（Atom-JoyStick開発）**:
- Arduino IDE開発環境
- M5Stack Atom-JoyStick実装
- 分散MQTT制御システム

---

## 🔄 GitHub連携ワークフロー

### 1. 初回セットアップ（別PC）
```bash
# リポジトリクローン
git clone https://github.com/sastles-com/sastle.git
cd sastle

# 分散MQTTブランチ取得（プッシュ完了後）
git fetch origin
git checkout -b feature/distributed-mqtt-integration origin/feature/distributed-mqtt-integration

# 開発環境確認
ls isolation-sphere/atom-joystick/
```

### 2. 日常開発サイクル
```bash
# 作業開始前の同期
git pull origin feature/distributed-mqtt-integration

# Arduino IDE開発
# - コード編集
# - コンパイル・フラッシュ
# - 動作テスト

# 変更のコミット
git add atom-joystick/
git commit -m "M5Stack Atom-JoyStick: [機能名] 実装"

# リモートへプッシュ
git push origin feature/distributed-mqtt-integration
```

### 3. 両PC間での同期
```bash
# 現在のPC側での取得
git pull origin feature/distributed-mqtt-integration

# 統合テスト実行
# - ESP32 ↔ Atom-JoyStick MQTT通信
# - raspi WebUI ↔ MQTT連携テスト
```

---

## 🛠️ Arduino IDE開発手順

### 1. プロジェクト準備
1. Arduino IDE起動
2. File → Open → `sastle/isolation-sphere/atom-joystick/atom_joystick_main/atom_joystick_main.ino`
3. Tools → Board → M5Stack-Atom
4. Tools → Port → 接続ポート選択

### 2. 段階的実装

#### Phase 1: 基本機能
- [ ] WiFiアクセスポイント機能テスト
- [ ] LCD表示動作確認
- [ ] Joystick入力読み取り確認
- [ ] シリアル出力でデバッグ

#### Phase 2: MQTT統合
- [ ] 簡易MQTTブローカー実装
- [ ] Topic階層テスト
- [ ] Publish/Subscribe動作確認
- [ ] 複数クライアント接続テスト

#### Phase 3: ESP32統合
- [ ] ESP32クライアント接続テスト
- [ ] リアルタイム制御テスト
- [ ] 状態同期動作確認
- [ ] 障害耐性テスト

### 3. デバッグ・テスト手順
```cpp
// シリアルモニター設定
Serial.begin(115200);
Serial.setDebugOutput(true);

// WiFi接続確認
Serial.println(WiFi.softAPIP());
Serial.printf("Connected clients: %d\n", WiFi.softAPgetStationNum());

// MQTT動作確認
Serial.printf("MQTT clients: %d\n", mqtt_broker_get_client_count());

// Joystick値確認
JoystickState js = joystick_get_state();
Serial.printf("Left: (%d, %d), Right: (%d, %d)\n", js.left_x, js.left_y, js.right_x, js.right_y);
```

---

## 🧪 テスト環境構築

### 1. 単体テスト
```bash
# WiFiアクセスポイントテスト
# スマホ・PCでSSID「IsolationSphere-Direct」接続確認

# MQTT接続テスト
mosquitto_pub -h 192.168.100.1 -t "test/topic" -m "hello"
mosquitto_sub -h 192.168.100.1 -t "isolation-sphere/#"
```

### 2. 統合テスト
```bash
# ESP32クライアント接続テスト（現在のPC）
cd /home/yakatano/work/isolation-sphere/esp32/test_hello_world
# ESP32にMQTTクライアント実装を追加
idf.py build flash monitor

# raspi WebUIテスト
cd /home/yakatano/work/isolation-sphere/raspi/project02
python simple_webui_poc.py
# http://localhost:8000 でUI確認
```

### 3. 性能テスト
- **制御レスポンス**: Joystick→ESP32制御 15ms目標
- **接続安定性**: 8デバイス同時接続テスト
- **長時間動作**: 24時間連続稼働テスト

---

## 📊 開発進捗管理

### マイルストーン
- [x] **M1**: Arduino IDE基本環境構築
- [x] **M2**: WiFi AP + LCD表示実装
- [x] **M3**: Joystick入力システム実装
- [x] **M4**: 簡易MQTTブローカー実装
- [ ] **M5**: ESP32クライアント統合
- [ ] **M6**: raspi WebUI統合
- [ ] **M7**: 複数デバイス制御
- [ ] **M8**: 障害耐性・自動復旧

### 品質チェックリスト
- [ ] コンパイルエラーゼロ
- [ ] メモリリークなし（24時間稼働）
- [ ] WiFi接続安定性（切断・再接続テスト）
- [ ] MQTT通信信頼性（パケットロス対応）
- [ ] Joystick精度・レスポンス（デッドゾーン調整）
- [ ] LCD表示・UI操作性

---

## 🚀 デプロイメント手順

### 1. リリースビルド
```cpp
// リリース設定
#define DEBUG_MODE 0
#define SERIAL_BAUDRATE 115200
#define WIFI_AP_HIDDEN false
#define MQTT_MAX_CLIENTS 8
```

### 2. フラッシュ・設定
```bash
# Arduino IDE設定
Tools → CPU Frequency → 240MHz
Tools → Flash Size → 4MB
Tools → Partition Scheme → Default 4MB with spiffs

# フラッシュ実行
Sketch → Upload
```

### 3. 運用確認
- WiFiアクセスポイント自動起動
- MQTT接続受付開始
- LCD表示正常動作
- Joystick応答確認

---

## 🔧 トラブルシューティング

### よくある問題と解決方法

#### WiFiアクセスポイントが起動しない
```cpp
// デバッグ出力追加
Serial.println("WiFi mode: " + String(WiFi.getMode()));
Serial.println("AP IP: " + WiFi.softAPIP().toString());
Serial.println("AP clients: " + String(WiFi.softAPgetStationNum()));
```

#### MQTTクライアント接続できない
```cpp
// ポート・プロトコル確認
Serial.println("MQTT broker listening on port: " + String(MQTT_PORT));
// ファイアウォール・ネットワーク設定確認
```

#### Joystick値が不安定
```cpp
// キャリブレーション実行
joystick_calibrate();
// デッドゾーン調整
joystick_set_deadzone(30); // デフォルト20から30に変更
```

#### LCD表示されない
```cpp
// M5Unified初期化確認
M5.begin();
M5.Lcd.setBrightness(255); // 最大輝度
M5.Lcd.fillScreen(LCD_COLOR_WHITE); // 白画面テスト
```

---

## 📚 参考資料

### 公式ドキュメント
- [M5Stack Atom-JoyStick](https://docs.m5stack.com/en/app/Atom%20JoyStick)
- [Arduino IDE ESP32](https://docs.espressif.com/projects/arduino-esp32/)
- [PubSubClient MQTT Library](https://pubsubclient.knolleary.net/)

### isolation-sphereプロジェクト
- [プロジェクト仕様](../CLAUDE.md)
- [分散MQTTアーキテクチャ](./MQTT_TOPICS.md)
- [開発環境セットアップ](../ATOM_JOYSTICK_DEV_SETUP.md)

**この開発ワークフローに従って、isolation-sphereの革新的分散制御システムを効率的に実装できます。**