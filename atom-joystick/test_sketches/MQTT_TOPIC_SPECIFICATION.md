# M5Stack Atom-JoyStick MQTT トピック仕様書

## 概要

M5Stack Atom-JoyStick分散制御システムにおけるMQTTトピック階層・通信仕様の詳細定義。
4つのJoystickモード（Control、Video、Adjust、System）に対応した項目別配信システム。

## アーキテクチャ設計方針

### 🎯 中央集権型MQTTブローカーシステム
- **中央ハブ**: M5Stack Atom-JoyStick (192.168.100.1:1883)
- **軽量MQTTブローカー**: EmbeddedMqttBroker (最大8クライアント)
- **WiFiルーター機能**: IsolationSphere-Direct (192.168.100.x)
- **Retain機能**: 全項目で最新値を保持・新規接続時自動同期

### 🔄 項目別配信システムの利点
- **変更検出**: 前回値との比較による効率配信
- **拡張性**: 新規項目追加・削除に柔軟対応
- **軽量性**: 必要な項目のみ配信・帯域節約
- **同期性**: Retain機能で遅れて接続したクライアントも最新状態取得

## MQTT トピック階層定義

### 📱 Control モード  
基本制御システム（LED明度・色温度・3D回転制御）

```
control/brightness        # LED明度制御 (0-255)
control/color_temp        # 色温度制御 (2700K-6500K)
control/playback          # 再生状態制御 (true/false)
control/rotation_x        # X軸回転制御 (-1.0~1.0)
control/rotation_y        # Y軸回転制御 (-1.0~1.0)
```

**Joystick マッピング仕様:**
- **左スティック X軸** → `control/brightness` (0-255範囲正規化)
- **左スティック Y軸** → `control/color_temp` (2700-6500K範囲変換)
- **右スティック X軸** → `control/rotation_x` (-1.0~1.0直接送信)
- **右スティック Y軸** → `control/rotation_y` (-1.0~1.0直接送信)
- **左ボタン** → `control/playback` (トグル動作)

### 🎬 Play モード
動画再生・音量・シーク・速度制御システム

```
video/selected_id         # 選択動画ID (0-999)
video/volume             # 音量制御 (0-100)
video/seek_position      # シーク位置 (秒単位)
video/playback_speed     # 再生速度 (0.5-2.0)
```

**Joystick マッピング仕様:**
- **左スティック X軸** → `video/selected_id` (動画選択)
- **左スティック Y軸** → `video/volume` (0-100音量調整)
- **右スティック X軸** → `video/seek_position` (シーク操作)
- **右スティック Y軸** → `video/playback_speed` (0.5-2.0速度調整)
- **右ボタン** → 動画再生トグル

### ⚙️ Maintenance モード  
システムパラメータ調整・保守・診断システム

```
adjust/selected_param     # 選択パラメータ番号 (0-4)
adjust/param_0           # パラメータ0の値 (0-255)
adjust/param_1           # パラメータ1の値 (0-255)
adjust/param_2           # パラメータ2の値 (0-255)
adjust/param_3           # パラメータ3の値 (0-255)
adjust/param_4           # パラメータ4の値 (0-255)
```

**Joystick マッピング仕様:**
- **左スティック X軸** → `adjust/selected_param` (0-4パラメータ選択)
- **左スティック Y軸** → 選択されたパラメータ値調整
- **右スティック Y軸** → 微調整機能（±1単位調整）

### 🖥️ System モード
システム状態監視・ネットワーク管理・診断システム

```
system/current_mode       # 現在のモード名 ("control"/"video"/"adjust"/"system")
system/wifi_clients       # WiFiクライアント数 (0-8)
system/cpu_temp          # CPU温度 (摂氏)
system/uptime            # 稼働時間 (秒)
```

**Joystick マッピング仕様:**
- **左スティック** → モード切り替え
- **右スティック** → システム設定調整
- **ボタン** → システムコマンド実行

## 🔧 技術実装詳細

### データフォーマット
```json
// 数値データ例
{
  "topic": "control/brightness",
  "payload": "180",
  "retain": true,
  "timestamp": 1725441234
}

// 文字列データ例
{
  "topic": "system/current_mode", 
  "payload": "control",
  "retain": true,
  "timestamp": 1725441234
}

// 浮動小数点データ例
{
  "topic": "control/rotation_x",
  "payload": "-0.75",
  "retain": true,
  "timestamp": 1725441234  
}
```

### 変更検出アルゴリズム
```cpp
// 整数値の変更検出・配信
bool publishIntValue(const char* topic, int value, int& cache_value) {
  if (value != cache_value) {
    String payload = String(value);
    bool success = publishWithRetain(topic, payload.c_str());
    if (success) {
      cache_value = value;  // 成功時のみキャッシュ更新
    }
    return success;
  }
  return true; // 変更なしは成功扱い
}

// 浮動小数点の変更検出（0.01精度）
bool publishFloatValue(const char* topic, float value, float& cache_value) {
  if (abs(value - cache_value) > 0.01f) {
    String payload = String(value, 2);
    bool success = publishWithRetain(topic, payload.c_str());
    if (success) {
      cache_value = value;
    }
    return success;
  }
  return true;
}
```

### 統計管理システム
```cpp
struct MQTTStats {
  unsigned long total_messages_published;   // 総配信メッセージ数
  unsigned long total_messages_received;    // 総受信メッセージ数  
  unsigned long total_clients_connected;    // 総接続クライアント数
  unsigned long last_publish_time;          // 最終配信時刻
  unsigned long broker_start_time;          // ブローカー開始時刻
  int current_connected_clients;            // 現在接続中クライアント数
};
```

## 📊 性能・容量仕様

### 通信性能
- **配信頻度**: 33.3Hz (30msec間隔)
- **最大クライアント**: 8デバイス同時接続
- **応答時間**: 15-30ms (Joystick → ESP32)
- **帯域使用量**: 平均4KB/sec (変更検出による効率化)

### メモリ使用量
- **フラッシュメモリ**: 1,205,527 bytes (91% ESP32-S3)
- **RAM**: 53,676 bytes (16% グローバル変数)
- **MQTT キャッシュ**: 約2KB (UI状態管理)

## 🔗 デバイス間連携

### ESP32-S3 (LED制御デバイス)
```cpp
// MQTTクライアントとしてJoystick状態を受信
void onMqttMessage(String topic, String payload) {
  if (topic == "control/brightness") {
    led_brightness = payload.toInt();
    updateLEDBrightness(led_brightness);
  } else if (topic == "control/rotation_x") {
    rotation_x = payload.toFloat(); 
    updateRotation(rotation_x, rotation_y);
  }
}
```

### raspi (WebUIシステム)
```python
# MQTT状態受信・WebUI同期
def on_mqtt_message(client, userdata, message):
    topic = message.topic.decode('utf-8')
    payload = message.payload.decode('utf-8')
    
    # WebUI状態更新
    if topic == "video/volume":
        update_ui_volume(int(payload))
    elif topic == "video/selected_id":
        update_ui_video_selection(int(payload))
```

## 🚀 将来拡張予定

### 複数ESP32対応
```
device/{device_id}/control/brightness    # デバイス個別制御
device/{device_id}/system/status         # デバイス個別状態
global/sync/command                      # 全デバイス同期コマンド
```

### 高度機能統合
```
audio/volume_master                      # マスター音量
audio/equalizer/{band}                   # イコライザー設定  
network/discovery/{device_type}          # プラグアンドプレイ
diagnostic/performance/{metric}          # 詳細診断情報
```

---

## 🎮 **統一操作体系仕様（Phase 5確定）**

### **4モード体系（名称更新）**
1. **Control**: 基本制御（LED明度・色温度・3D回転制御）
2. **Play**: 動画再生制御（動画選択・音量・シーク・速度）  
3. **Maintenance**: 保守調整（パラメータ微調整・診断・メンテナンス）
4. **System**: システム監視（統計・ネットワーク・設定・状態管理）

### **統一操作方法（全モード共通）**
- **Aボタン**: モード切り替え（Control→Play→Maintenance→System循環）
- **Aボタン長押し**: Bボタン機能（将来機能・現在保留）
- **アナログスティック方向**: 8方向機能選択（大きさ無関係・方向のみ）
- **スティック押し込み**: 決定実行（LCD選択項目表示付き）
- **左右ボタン**: 固定機能割り当て（将来拡張用・現在未定義）
- **決定時動作**: MQTT状態変更コマンド自動送信

### **LCD表示仕様**
```
Mode: Control           # 現在のモード名表示
Select: brightness      # 選択中の機能表示  
Value: 180             # 現在値表示
Press stick to confirm  # 操作ガイド
```

### **状態同期プロトコル（Phase 5新規）**
```
state/display/brightness         # システム状態：LED明度同期
state/playback/current_video     # システム状態：現在動画同期  
state/maintenance/param_0        # システム状態：保守パラメータ同期
state/system/current_mode        # システム状態：現在モード同期
```

---

**📅 作成日**: 2025年9月4日  
**🔄 更新日**: 2025年9月4日  
**👤 作成者**: Claude Code Assistant  
**📋 ステータス**: Phase 5 分散状態同期システム設計完成