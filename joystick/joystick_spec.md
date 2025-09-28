# M5Stack Atom JoyStick プロジ## 2. 起動シーケンス

1. **LittleFS初期化**: NVSとLittleFSをマウントし、設定ファイルへアクセス可能に  
2. **設定読み込み**: `config.json`からシステム設定を読み込み  
3. **WiFi AP起動**: 「isolation-joystick」ネットワークを192.168.100.1で開始  
4. **MQTTブローカー起動**: ポート1883で内蔵ブローカーを開始  
5. **オーディオ初期化**: ブザーシステムを初期化し起動音を再生  
6. **コントローラー初期化**: ジョイスティックとボタン入力システムを初期化  
7. **メインループ開始**: マルチコアタスクで制御ループを開始## 1. プロジェクト概要
ESP32-S3（M5Stack Atom JoyStick K137）をベースにした無線コントローラーシステム。  
PlatformIO + Arduino フレームワーク上で動作し、`platformio.ini` 内の `env:atoms3r` 環境にビルド設定と依存ライブラリを集約する。  
WiFi AP機能でローカルネットワークを提供し、内蔵MQTTブローカーでリアルタイム通信を実現。  
デュアルジョイスティック、ファンクションボタン、内蔵ブザーで操作フィードバックを提供する。  

### メイン機能
- **無線コントローラー**: デュアルジョイスティックでisolation-sphereをリアルタイム制御
- **WiFi AP機能**: 独立したローカルネットワークを提供 (192.168.100.x)
- **内蔵MQTTブローカー**: リアルタイムメッセージングでシステム間連携
- **オーディオフィードバック**: 内蔵ブザーで操作確認音を再生
- 
### 開発環境
- PlatformIO CLI / VSCode 拡張からビルド・書き込み・モニタを実行する。
- 主要ライブラリ  
  - ストレージ：`LittleFS_esp32`（LittleFS）、`ESP32-PSRamFS`（RAMDisk）  
  - ディスプレイ：`LovyanGFX` + `M5GFX`  
  - LED 制御：`FastLED`（I2S DMA 出力対応）  
  - 通信：`AsyncMqttClient`、Arduino `WiFi` / `AsyncUDP`
  - 設定・ユーティリティ：`ArduinoJson`、`M5Unified`、`Adafruit BNO055` など `platformio.ini` の `lib_deps` を参照

---

---

## 2. 起動シーケンス
1. NVS と LittleFS を初期化し、`ESP32-PSRamFS` 経由で RAMDisk をマウント  
2. `config.json` を読み込み、周辺機能の設定を反映  
3. ブザーで起動音を再生  
4. LCD に「Starting...」表示（`M5GFX` / `LovyanGFX`）  
5. RAMDisk 上のオープニング動画を読み込み、`LovyanGFX` で再生（Core0）  
6. Core1 にてブザー再生と LCD「System Ready」を表示  
7. MQTT/UDP/IMU 初期化 → メインループへ  

---

## 3. マルチコアタスク構成

### **Core0 - ネットワークコア**  
- **WiFi AP管理**: アクセスポイントの維持と接続クライアント管理  
- **MQTTブローカー**: メッセージングサービスの提供とメッセージルーティング  
- **設定管理**: `ConfigManager`での設定ファイル管理  
- **システムステータス**: 定期的なシステム情報の収集と送信

### **Core1 - UIコア**  
- **ジョイスティック入力**: デュアルジョイスティックのリアルタイム読み取り  
- **ボタン入力**: ファンクションボタンとLCDボタンのイベント処理  
- **オーディオ出力**: ブザー制御と操作フィードバック  
- **UI状態管理**: 物理入力をUI論理状態に変換しMQTT送信

- **外部接続**  
  - Raspberry Pi：MQTT ブローカー + Web UI（アップロード機能含む）  
  - PC/スマホ：UI クライアント  

### ストレージ構成
- config.json
  - 起動時から変動しない設定項目を記載（Wi-Fi、MQTT、表示設定など）
  - `LittleFS` 上に配置し、`ArduinoJson` で読み込む
- led_layout.csv
  - LED の 3D 座標値、ストリップ ID、インデックスを格納
  - キャッシュが必要な処理のみ PSRamFS に展開する
- images/
  - opening : オープニング用ムービー（連番 .jpg）
  - demo** : デモ用ムービー（連番 .jpg）
- `data/` → `pio run --target uploadfs`（LittleFS）で書き込み、初期データを管理
- `partitions.csv` → OTA 用に `ota_0` / `ota_1` を保持しつつ、LittleFS 領域を 3MB 確保

### LED 球体制御
- LED は 4 本の WS2812 ストリップ（GPIO 5/6/7/8）に分割配置し、各ストリップの球数は `[180, 220, 180, 220]`、合計 800 球。
- 目標フレームレートを 30fps 以上とするため、`FastLED` の I2S DMA サポートを利用し、4 チャンネル同時出力を行う。
- 最長ストリップ 220 球分を 4 ライン同時に出力する構成で、FrameBuffer 変換は `FastLED` の `ESP32_I2S` 実装を拡張して管理する。
- DMA バッファは PSRAM 上で確保し、LittleFS/PSRamFS のフレームデータを逐次デコードして転送する。
- RMT ベース実装はデバッグ用に残し、`FASTLED_ESP32_I2S` が安定した段階でビルドターゲットから除外する。

### メディア処理
- JPEG 再生は `M5GFX` 内蔵のデコーダーまたは `TJpg_Decoder`（PlatformIO レジストリ: `bodmer/TJpg_Decoder`）を利用し、LittleFS 上のフレームを逐次デコードする。
- 大容量アセットは起動時に LittleFS → PSRamFS へコピーし、`FastLED` と `LovyanGFX` に共有する。
- オーディオ出力は `M5Unified::Speaker` を基盤とし、内蔵ブザー用に PCM / beeper API を整備する。

### OTA アップデート
- OTA 書き込みは `ota_0` / `ota_1` の二重アプリ領域を利用し、書き込み後にブートパーティションを切り替える。
- 実装は `AsyncElegantOTA` + `ESP Async WebServer` を採用し、`M5Unified` の Wi-Fi 管理と統合する。
- OTA ページは認証付きで提供し、アップロード完了後は `ESP.restart()` で自動再起動する。

## 4. WiFiアーキテクチャ仕様

### WiFi接続モード

- **固定APモード**: Joystickは常にアクセスポイント（AP）として動作
- **STAモード接続なし**: 外部WiFiネットワークへの接続は行わない
- **理由**: 独立した制御系として動作し、外部ネットワーク障害時でも直接制御を可能にする

### ネットワーク設定

```text
SSID: isolation-joystick (config.json設定可能)
IP範囲: 192.168.100.x
デフォルトIP: 192.168.100.1
チャンネル: 6 (config.json設定可能)
最大接続数: 8デバイス
```

### システム全体でのWiFi分担

- **Joystick**: ローカルAP提供、MQTTブローカー内蔵、直接制御UI
- **Isolation Sphere**: STAモードでRaspberry PiのローカルAPに接続
- **Raspberry Pi**:
  - WiFiモジュール1: STA（外部WiFi/インターネット接続）
  - WiFiモジュール2: AP（Isolation Sphere接続用）
  - MQTTブローカー、Web UI、データログ機能

---

## 5. アーキテクチャ図

```mermaid
flowchart TD
    subgraph "Joystick ESP32-S3"
        subgraph Core0
            A1[LittleFS Init] --> A2[config.json 読込]
            A2 --> A3[WiFi AP起動<br/>192.168.100.x]
            A3 --> A4[MQTT Broker起動]
            A4 --> A5[UDP通信処理]
            A5 --> A6[ジョイスティック制御]
        end

        subgraph Core1
            B1[ブザー制御<br>(buzzer_manager)]
            B2[LCD 表示<br>System Ready]
            B3[IMU センサー]
        end

        A2 --> B1
        A2 --> B2
    end

    subgraph "Isolation Sphere ESP32-S3"
        C1[WiFi STA接続]
        C2[LED球体制御]
        C3[姿勢制御]
        C1 --> C2
        C1 --> C3
    end
    
    subgraph "Raspberry Pi Hub"
        D1[WiFi Module 1<br/>STA - External]
        D2[WiFi Module 2<br/>AP - Local]
        D3[MQTT Broker]
        D4[Web UI Server]
        D5[Data Logger]
        
        D1 --> D3
        D2 --> D3
        D3 --> D4
        D3 --> D5
    end

    subgraph External
        E1[External WiFi<br/>Internet]
    end

    A6 -.->|直接UDP制御| C2
    A4 -.->|独立MQTT| B2
    D2 -->|ローカルAP| C1
    D1 -.->|STA接続| E1
```

---
