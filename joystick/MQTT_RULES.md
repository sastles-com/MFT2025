# MFT2025 System 包括的MQTT設計ガイド

## 🎯 設計思想

**統合されたMQTTエコシステム**
- **Joystick**: 物理入力 → UI論理状態 → 制御コマンド送信  
- **isolation-sphere**: 制御コマンド受信 → 直接実行 + 画像・状態配信
- **システム全体**: 軽量・イベント駆動型の効率的通信

**データ種別による最適化**
- **UI制御**: リアルタイム性重視（QoS=0, retain=false）
- **画像転送**: JPEG圧縮 + バイナリ転送（2KB-30KB対応）
- **システム状態**: 最新状態保持（QoS=0, retain=true）

## 📋 全トピックリスト

### **実装済み（現在使用中）**
- `joystick/state` - ジョイスティック物理状態（MqttBroker::publishJoystickState実装済み）
- `joystick/test` - ボタンテスト時のメッセージ（main.cpp実装済み）
- `joystick/status` - ジョイスティック定期ステータス（main.cpp実装済み、10秒間隔）
- `system/status` - システム技術的状態（MqttBroker::publishSystemStatus実装済み）
- `system/clients` - WiFi接続クライアント数（MqttBroker::publishWiFiClients実装済み）

### **config.json設定済み（未使用）**
- `sphere/status` - config.jsonで定義済み（実装待ち）
- `sphere/ui` - config.jsonで定義済み（実装待ち）
- `sphere/image` - config.jsonで定義済み（実装待ち）  
- `sphere/command` - config.jsonで定義済み（実装待ち）

### **画像転送専用トピック**
- `sphere/images/frame` - JPEG画像データ（バイナリ）
- `sphere/images/thumbnail` - サムネイル画像
- `sphere/images/status` - 画像転送ステータス

### **設計済み（実装予定）**
- `sphere/ui/control` - UI制御状態（メイン）
- `sphere/ui/mode` - 現在のUIモード
- `sphere/ui/selection` - 選択中のアイテム
- `sphere/ui/notification` - 通知・アラート
- `sphere/ui/settings` - 設定変更イベント
- `system/diagnostics` - 診断情報

### **Migration予定**
- `joystick/state` → `sphere/ui/control` (物理データ → UI論理状態)
- `joystick/test` → 削除（開発専用）
- `joystick/status` → `system/status`に統合

## 📊 トピック階層設計

### **sphere/** - システム全体のコマンド・状態
```
sphere/ui/control          # UI制御状態（メイン）
sphere/ui/mode            # 現在のUIモード
sphere/ui/selection       # 選択中のアイテム
sphere/status             # sphere側からのシステム状態
sphere/image              # 画像データ
sphere/command            # 一次的なコマンド
```

### **system/** - 技術的・管理用情報
```
system/status             # システム技術的状態
system/clients            # WiFi接続クライアント数
system/diagnostics        # 診断情報
```

### **sphere/images/** - 画像転送・配信
```
sphere/images/frame       # JPEG画像データ（2KB-30KB）
sphere/images/thumbnail   # サムネイル画像
sphere/images/status      # 画像転送ステータス・統計
```

### **廃止予定** ~~joystick/~~ - 物理入力の生データ
```
joystick/state            # 物理スティック座標（Migration予定）
joystick/test             # 開発テスト用（削除予定）
joystick/status           # system/statusに統合予定
```

### **廃止予定** ~~joystick/~~ - 物理入力の生データ
```
❌ joystick/state         # 削除予定（物理データは内部処理のみ）
❌ joystick/test          # 削除予定（開発専用）
❌ joystick/status        # 削除予定（system/statusに統合）
```

## 🎮 階層ダイアル操作 UI制御ペイロード設計

### **sphere/ui/control** - メインUI状態（階層ダイアル対応）
```json
{
  "joystick_mode": "live",            // UIモード: live/control/video/maintenance/system
  "selected_function": "brightness",   // 選択中機能名
  "function_index": 0,                // 機能インデックス (0-N)
  "function_angle": 0,                // 左スティック角度 (0-360°)
  
  "value": {
    "type": "analog",                 // 値タイプ: analog/discrete/boolean
    "current": 185,                   // 現在値
    "min": 0,                         // 最小値 (analogの場合)
    "max": 255,                       // 最大値 (analogの場合)
    "choices": null,                  // 選択肢配列 (discreteの場合)
    "angle": 127.5                    // 右スティック角度 (0-360°)
  },
  
  "target": "sphere001",              // 制御対象sphere ID
  "timestamp": 1234567890
}
```

### **sphere/ui/mode** - モード切替専用

```json
{
  "previous": "live",                 // 前回モード
  "current": "control",               // 現在モード: live/control/video/maintenance/system  
  "function_count": 8,                // 現在モードの機能数
  "timestamp": 1234567890
}
```

### **sphere/ui/selection** - 選択状態

```json
{
  "mode": "control",                  // 現在UIモード
  "selected_function": "brightness",   // 選択中機能名
  "function_index": 0,                // 機能インデックス (0-N)
  "function_type": "analog",          // analog/discrete/boolean
  "value_range": {"min": 0, "max": 255}, // 値範囲 (analogの場合)
  "choices": null,                    // 選択肢 (discreteの場合)
  "left_stick_angle": 0,             // 左スティック角度 (0-360°)
  "timestamp": 1234567890
}

### **sphere/ui/selection** - 選択状態
```json
{
  "type": "playlist",                 // 選択対象タイプ
  "item": "opening",                  // 選択中アイテム
  "index": 2,                         // リスト内位置
  "total": 5,                         // 総アイテム数
  "timestamp": 1234567890
}
```

## 📋 UIモード一覧

## 🎯 5モード8機能固定 UI制御詳細ペイロード

### **1. Live Mode (UI_MODE_LIVE: 0)** - リアルタイムisolation-sphere制御
```json
{
  "mode": "live",
  "selected_function": "brightness",     // 8機能: brightness/color_temp/rotate_speed/effect/x_axis/y_axis/z_axis/reset
  "function_index": 0,                   // 0-7 (45°等分割)
  "value": {
    "type": "analog",                    // brightness: analog (0-255)
    "current": 185,
    "min": 0, "max": 255
  },
  "target": "sphere001",
  "timestamp": 1234567890
}
```

### **2. Control Mode (UI_MODE_CONTROL: 1)** - 照明・表示の基本設定  
```json
{
  "mode": "control", 
  "selected_function": "rgb_red",        // 8機能: brightness/color_temp/rgb_red/rgb_green/rgb_blue/update_rate/auto_dimming/power_save
  "function_index": 2,                   // 0-7 (45°等分割)
  "value": {
    "type": "analog",                    // rgb_red: analog (0-255)
    "current": 128,
    "min": 0, "max": 255
  },
  "timestamp": 1234567890
}
```

### **3. Video Mode (UI_MODE_VIDEO: 2)** - メディア再生・プレイリスト管理
```json
{
  "mode": "video",
  "selected_function": "playback_speed", // 8機能: volume/playback_speed/video_id/repeat/shuffle/playlist/effect/playback_control
  "function_index": 1,                   // 0-7 (45°等分割)  
  "value": {
    "type": "discrete",                  // playback_speed: 8択 (0.25x-2.0x)
    "choices": ["0.25x", "0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "1.75x", "2.0x"],
    "selected_index": 3,                 // 1.0x選択
    "current": "1.0x"
  },
  "timestamp": 1234567890
}
```

### **4. Maintenance Mode (UI_MODE_MAINTENANCE: 3)** - システム調整・キャリブレーション
```json
{
  "mode": "maintenance",
  "selected_function": "led_calibration", // 8機能: led_calibration/imu_correction/timeout/update_freq/log_level/diagnostics/backup/factory_reset
  "function_index": 0,                    // 0-7 (45°等分割)
  "value": {
    "type": "analog",                     // led_calibration: analog (0.8x-1.2x)
    "current": 1.05,
    "min": 0.8, "max": 1.2
  },
  "timestamp": 1234567890
}
```

### **5. System Mode (UI_MODE_SYSTEM: 4)** - システム状態監視・統計表示
```json
{
  "mode": "system",
  "selected_function": "cpu_usage",      // 8機能: cpu_usage/memory_usage/temperature/battery/wifi_signal/mqtt_connections/error_log/system_control
  "function_index": 0,                   // 0-7 (45°等分割)
  "value": {
    "type": "display_only",              // cpu_usage: 表示のみ
    "current": 67,                       // 67%使用率
    "unit": "%",
    "period_selection": 5                // 右スティックで期間選択 (1m/5m/15m/1h/6h)
  },
  "timestamp": 1234567890
}

### **2. playlist_control** - プレイリスト制御
```json
{
  "mode": "playlist_control", 
  "playlist": "opening",
  "frame": 25,
  "playback": "playing",              // playing/paused/stopped
  "loop": true,
  "fps": 10
}
```

### **3. system_config** - システム設定
```json
{
  "mode": "system_config",
  "wifi": {
    "ap_enabled": true,
    "visible": true,
    "max_clients": 8
  },
  "mqtt": {
    "enabled": true,
    "port": 1883
  },
  "debug_mode": false
}
```

### **4. multi_sphere** - 複数Sphere制御
```json
{
  "mode": "multi_sphere",
  "targets": ["sphere001", "sphere002"],
  "sync_mode": "synchronized",        // synchronized/independent
  "led": {...}
}
```

## 🔄 メッセージフロー

### **階層ダイアル操作フロー**

```text
1. LCDボタン短押し → モード切替 (5モード循環)
   live → control → video → maintenance → system → live...
   
2. 左スティック方向操作 → 機能選択 (8方向45°分割)
   各モード8機能を等間隔配置、12時位置自動整列
   
3. 右スティック方向操作 → 値調整 (タイプ別制御)  
   - Boolean: 左右分割 (左=false, 右=true)
   - 離散値: N等分割 (選択肢数による)
   - 連続値: 360°回転マッピング (0-360° → min-max)
   
4. スティック押下1秒 → 設定確定
   ホールド進行表示 → 確定音 → MQTT送信

MQTTトピック送信:
├ sphere/ui/mode: モード変更時
├ sphere/ui/selection: 機能選択時  
└ sphere/ui/control: 値確定時
```
```
1. 物理ボタン入力検出
   ↓
2. UIStateManager で論理状態更新
   ↓  
3. sphere/ui/control へ送信
   ↓
4. isolation-sphere が受信・実行
```

### **QoS・Retain設定**
- `sphere/ui/*`: QoS=0, retain=false (リアルタイム性重視)
- `sphere/images/frame`: QoS=0, retain=true (最新画像保持)
- `sphere/images/thumbnail`: QoS=0, retain=true (UI表示用)
- `sphere/images/status`: QoS=0, retain=false (転送統計)
- `sphere/status`: QoS=0, retain=true (最新状態保持)
- `system/*`: QoS=0, retain=true (監視・管理用)

## 🛡️ エラーハンドリング

### **sphere未応答時**
```json
// system/status
{
  "sphere_connection": "timeout",
  "last_response": 1234567890,
  "retry_count": 3
}
```

### **不正なコマンド**
```json
// system/diagnostics  
{
  "error": "invalid_mode",
  "received_mode": "invalid_mode_name",
  "valid_modes": ["sphere_control", "playlist_control", ...]
}
```

## 📝 MFT2025システム実装ガイドライン

### **UI制御システム**
1. **物理入力は内部処理のみ** - MQTTに生データを送信しない
2. **UI状態は常にJSONで構造化** - 文字列値は避ける
3. **モード切替は明示的** - sphere/ui/mode で通知
4. **タイムスタンプ必須** - 同期・デバッグ用
5. **retain使い分け** - 状態保持が必要かで判断

### **画像転送システム**
6. **PSRAM有効化必須** - ESP32で大容量画像バッファ確保
7. **JPEG形式推奨** - 高圧縮効率でサイズ削減
8. **バッファサイズ計算** - 最大画像サイズ+5KB程度
9. **DMA活用** - CPU負荷軽減の直接メモリ転送
10. **イベント駆動** - データ更新時のみ送信で効率化

### **統合システム運用**
11. **トピック階層統一** - sphere/で統一、用途別サブパス
12. **QoS適材適所** - リアルタイム=0、確実性重視=1/2
13. **retain戦略** - UI表示用データは保持、ログ系は非保持
14. **エラーハンドリング** - system/diagnostics でシステム全体監視
15. **パフォーマンス監視** - 定期的な転送統計・システム状態取得

## � ESP32画像転送技術ガイド

### **画像転送の前提条件**

**ESP32ハードウェア要件**:
- **PSRAM（Pseudo-Static RAM）必須**: 画像データの大量メモリ消費に対応
- **MQTTバッファサイズ**: 転送画像の最大サイズ+5KB程度（例: 30KB画像→35KBバッファ）
- **画像形式**: JPEG推奨（高圧縮効率）
- **対応サイズ**: 2KB～30KB（単一メッセージ送信）

### **画像転送ワークフロー**

```text
1. 画像キャプチャ・準備
   ├ カメラモジュールでキャプチャ
   ├ JPEG形式に圧縮
   └ ファイルサイズ最適化

2. MQTT配信
   ├ バイナリデータ（バイト列）変換
   ├ sphere/images/frame トピックで配信
   ├ QoS設定: 確実配信=QoS1/2, リアルタイム=QoS0
   └ retain=true (最新画像のUI表示用)

3. 受信・表示
   ├ subscriber側でバイト列受信
   ├ JPEG画像に復元
   └ 表示・保存処理
```

### **パフォーマンス最適化技術**

**DMA (Direct Memory Access)**:
- CPUを介さずWi-Fiバッファへ直接転送
- CPU負荷軽減＋転送速度向上

**ダブルバッファリング**:
- 2つのバッファで送信中に次画像準備
- 連続画像転送のスムーズ化

**イベント駆動設計**:
- ポーリング回避→データ更新時のみ送信
- ネットワーク帯域幅・電力消費最小化

### **画像転送ペイロード例**

```json
// sphere/images/status - 転送統計
{
  "frame_count": 1247,
  "fps": 15.2,
  "avg_size_kb": 18.5,
  "compression_ratio": 0.12,
  "buffer_usage": 67,
  "timestamp": 1234567890
}
```

## �🔄 Migration Plan

### **Phase 1: 既存コード整理**
- [ ] `joystick/state` の送信停止
- [ ] `joystick/status` を `system/status` に統合
- [ ] UI状態管理クラス作成

### **Phase 2: 新トピック実装**
- [ ] `sphere/ui/control` 実装
- [ ] モード別ペイロード定義
- [ ] UIStateManager 実装
- [ ] 画像転送機能統合

### **Phase 3: 検証・最適化**
- [ ] sphere との連携テスト
- [ ] 画像転送パフォーマンス測定
- [ ] エラーハンドリング強化
- [ ] PSRAM・バッファサイズ最適化