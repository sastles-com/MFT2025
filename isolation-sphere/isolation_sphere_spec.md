# Isolation Sphere プロジェクト仕様書（拡張版）

## 1. プロジェクト概要
ESP32-S3（M5Atom S3R）をベースにした球体ディスプレイ制御システム。  
PlatformIO + Arduino フレームワーク上で動作し、`platformio.ini` 内の `env:atoms3r` 環境にビルド設定と依存ライブラリを集約する。  
起動時に LittleFS/PSRamFS からオープニング動画を読み出し、ブザーで起動音を鳴らす。  
ROS2/MQTT による外部制御、IMU による姿勢推定、UDP による動画フレーム転送を組み合わせ、リアルタイムなインタラクションを実現する。  

### やりたいこと
- センサーでボールの姿勢を検出 → テクスチャの描画座標を補正 → **ボールを転がしても画像の天地が保たれる球体ディスプレイ** という仕組み
- https://tajmahal0707.hatenablog.com/?_gl=1*zvi0tc*_gcl_au*OTQ2MTg4MjgxLjE3NTgyNjc3NzA.
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

## 3. コンポーネント構成

- **Core0**  
  - `LittleFS` からアセット取得し、`ESP32-PSRamFS` 経由で RAMDisk をロード  
  - オープニング動画再生（JPEG シーケンス + `LovyanGFX`）  
  - メイン制御ループ（MQTT/UDP 受信、LED 配置更新）

- **Core1**  
  - ブザー制御（`M5Unified::Speaker` + `buzzer_manager`）  
  - LCD テキスト描画（`M5GFX` / `LovyanGFX`）  
  - UI ステータス表示  
  - IMU 取得（`Adafruit_BNO055` でクォータニオン計測）

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

## 4. 姿勢制御機能付きテクスチャマッピング
- 

---

## 5. アーキテクチャ図

```mermaid
flowchart TD

subgraph ESP32-S3
    subgraph Core0
        A1[LittleFS Init] --> A2[config.json 読込]
        A2 --> A3[オープニング動画再生]
        A3 --> A4[MQTT/UDP 通信処理]
        A4 --> A5[LED 表示制御]
    end

    subgraph Core1
        B1[ブザー制御<br>(buzzer_manager)]
        B2[LCD 表示<br>System Ready]
    end

    A2 --> B1
    A2 --> B2
end

subgraph Raspberry Pi
    R1[MQTT Broker]
    R2[Web UI<br>(アップロード/制御)]
end

ESP32-S3 <--MQTT/UDP--> Raspberry Pi
```

---
