# テスト用ダミークライアント
## isolation-sphere Atom-JoyStick 動作確認用

本PCでAtom-JoyStickの動作確認を行うためのダミープログラム群です。

## 構成

### 1. dummy_esp32_client.py
- ESP32デバイスの動作をシミュレート
- MQTTクライアントとして動作
- LED制御・IMUデータ送信・状態報告を模擬

### 2. dummy_raspi_client.py  
- raspiシステムの動作をシミュレート
- WebUI・動画管理・UDP通信を模擬
- MQTTクライアント + HTTP APIサーバー

### 3. mqtt_monitor.py
- MQTT Topic監視・ログ出力
- リアルタイム通信確認・デバッグ支援

### 4. network_tester.py
- WiFi AP接続テスト
- ネットワーク性能測定・疎通確認

## 使用方法

### 前提条件
```bash
# uv仮想環境セットアップ（推奨）
uv venv
source .venv/bin/activate    # macOS/Linux
# source .venv/Scripts/activate  # Windows（PowerShell）

# 依存関係インストール
uv pip install -r requirements.txt
# または
uv sync  # pyproject.tomlから自動インストール

# 従来方式（pip）も併用可能
# pip install paho-mqtt flask requests psutil
```

### 基本テストシーケンス
1. **Atom-JoyStick起動**: Arduino IDEでフラッシュ・WiFi AP開始
2. **本PCをAtom-JoyStick WiFiに接続**: SSID `IsolationSphere-Direct`
3. **ダミークライアント起動**: 各種テストプログラム実行
4. **動作確認**: MQTT通信・LCD表示・Joystick操作の検証

### 個別プログラム実行

#### uv環境での実行（推奨）
```bash
# 仮想環境が有効化されていることを確認
source .venv/bin/activate

# ESP32ダミークライアント（LED制御受信・IMU送信）
python dummy_esp32_client.py
# または定義済みスクリプト使用
dummy-esp32

# raspiダミークライアント（WebUI・動画管理）
python dummy_raspi_client.py
# または定義済みスクリプト使用  
dummy-raspi

# MQTT通信監視
python mqtt_monitor.py
# または定義済みスクリプト使用
mqtt-monitor

# ネットワーク疎通確認
python network_tester.py --target 192.168.100.1
# または定義済みスクリプト使用
network-tester --target 192.168.100.1
```

#### 従来方式での実行
```bash
# システムPythonまたは他の仮想環境での実行
python dummy_esp32_client.py
python dummy_raspi_client.py  
python mqtt_monitor.py
python network_tester.py --target 192.168.100.1
```

## テスト項目

### 基本機能テスト
- [x] WiFi AP接続・IP取得
- [ ] MQTTブローカー接続・認証
- [ ] Topic購読・配信・QoS確認
- [ ] Joystick入力→MQTT配信
- [ ] LCD表示・モード切替

### 統合機能テスト  
- [ ] isolation-sphereコントロール: 明度調整・再生制御
- [ ] 動画管理モード: リスト選択・シーク操作
- [ ] 調整モード: パラメータ調整・設定保存
- [ ] システム管理モード: 監視・診断

### 性能テスト
- [ ] MQTT応答時間: Joystick入力→ダミーESP32応答 <30ms
- [ ] 同時接続: 複数ダミークライアント接続・安定性
- [ ] 長時間動作: 1時間連続動作・メモリリーク確認

## ログ・デバッグ

各プログラムは詳細ログを出力し、通信内容・タイミング・エラーを記録します。

```
logs/
├── mqtt_communication.log  # MQTT通信ログ
├── network_performance.log # ネットワーク性能ログ  
├── joystick_operations.log # Joystick操作ログ
└── system_status.log       # システム状態ログ
```