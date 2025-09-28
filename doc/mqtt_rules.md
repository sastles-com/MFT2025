# MFT2025 MQTT トピック設計規約

## トピック階層構造

### 基本原則
- デバイスタイプ/インスタンス/機能の3層構造
- 個別制御と一括制御の両対応
- JSON形式でのペイロード統一

### 階層設計パターン
```
{device_type}/{instance_id}/{function}
{device_type}/all/{function}        # 一括制御用
{device_type}/{function}            # 後方互換性(非推奨)
```

## Sphere（球体デバイス）トピック

### 制御系トピック

#### UI制御
- `sphere/ui` - 個別UI制御（後方互換性）
- `sphere/{instance_id}/ui` - 特定Sphere UI制御
- `sphere/all/ui` - 全Sphere同時UI制御

**用途**: 表示モード変更、パターン選択、輝度調整、UIモード切替

**ペイロード例**:
```json
{
  "mode": "pattern|demo|debug|off",
  "pattern": "spiral|rainbow|coordinate|wave",
  "brightness": 0-255,
  "ui_mode": "gesture|manual",
  "params": {
    "speed": 1.0,
    "color_offset": 120
  }
}
```

#### システムコマンド
- `sphere/command` - 個別システム制御（後方互換性）
- `sphere/{instance_id}/command` - 特定Sphereシステム制御
- `sphere/all/command` - 全Sphere同時システム制御

**用途**: 電源制御、リセット、設定保存、診断実行、ファームウェア更新

**ペイロード例**:
```json
{
  "action": "restart|shutdown|save_config|calibrate_imu|factory_reset",
  "params": {
    "force": true,
    "backup": false
  }
}
```

### データ系トピック

#### ステータス報告
- `sphere/status` - 全Sphereのステータス報告先
- `sphere/{instance_id}/status` - 特定Sphereのステータス報告

**ペイロード例**:
```json
{
  "device_id": "sphere001",
  "timestamp": 1640995200000,
  "uptime_ms": 3600000,
  "status": "ready|busy|error|offline",
  "imu": {
    "quaternion": [0.707, 0.0, 0.0, 0.707],
    "gesture": "shake|tilt|rotation|still"
  },
  "system": {
    "cpu_usage": 45.2,
    "memory_free": 245760,
    "temperature": 42.5
  },
  "led": {
    "current_pattern": "spiral",
    "brightness": 128,
    "fps": 30
  }
}
```

#### 画像データ
- `sphere/image` - 画像データ配信（後方互換性）
- `sphere/{instance_id}/image` - 特定Sphere画像データ
- `sphere/all/image` - 全Sphere同時画像配信

**用途**: JPEG画像、フレームデータ、テクスチャ配信

## Joystick（ジョイスティック）トピック

### 制御系トピック

#### UI制御
- `joystick/{instance_id}/ui` - 特定Joystick UI制御
- `joystick/all/ui` - 全Joystick同時UI制御

**用途**: 表示内容変更、モード切替、設定調整

**ペイロード例**:
```json
{
  "display_mode": "sphere_control|video_management|settings",
  "brightness": 0-255,
  "ui_layout": "dual_dial|single_stick|custom"
}
```

#### システムコマンド
- `joystick/{instance_id}/command` - 特定Joystickシステム制御
- `joystick/all/command` - 全Joystick同時システム制御

### データ系トピック

#### 入力データ
- `joystick/{instance_id}/input` - ジョイスティック入力データ

**ペイロード例**:
```json
{
  "timestamp": 1640995200123,
  "left_stick": {"x": 0.25, "y": -0.80},
  "right_stick": {"x": -0.50, "y": 0.10},
  "buttons": {
    "a": true, "b": false, "start": false, "select": false
  },
  "sequence": 12345
}
```

#### ステータス報告
- `joystick/{instance_id}/status` - Joystickステータス報告

## RasPi（制御ハブ）トピック

### システム管理
- `raspi/system/command` - システム全体の制御
- `raspi/system/status` - システム全体のステータス

### メディア管理
- `raspi/media/command` - メディアファイル管理
- `raspi/media/status` - メディアライブラリ状況
- `raspi/media/playlist` - プレイリスト配信

## クロスデバイス通信

### ブロードキャストトピック
- `system/all/sync` - 全デバイス同期コマンド
- `system/all/emergency` - 緊急停止・リセット

**同期コマンド例**:
```json
{
  "action": "sync_start|sync_stop|time_sync",
  "timestamp": 1640995200000,
  "params": {
    "pattern": "rainbow",
    "duration_ms": 30000,
    "sync_fps": 30
  }
}
```

## インスタンスID命名規則

### Sphere
- `001`, `002`, `003` ... - 3桁ゼロパディング数字
- 将来的に `main`, `sub1`, `sub2` などの名前付きIDも対応

### Joystick  
- `j001`, `j002` - ジョイスティック専用プレフィックス
- または `001`, `002` でSphereと共通番号体系

### RasPi
- `hub` - 制御ハブ固定名
- `main` - メインコントローラー

## QoS設定方針

| トピックタイプ | QoS | Retain | 説明 |
|--------------|-----|--------|------|
| UI制御 | 1 | false | 確実な配信、重複許可 |
| システムコマンド | 2 | false | 重要コマンド、重複禁止 |
| ステータス | 0 | true | 高頻度、最新状態保持 |
| 入力データ | 0 | false | リアルタイム、遅延厳禁 |
| 画像データ | 1 | false | 大容量、確実配信 |
| 緊急コマンド | 2 | false | 最重要、確実配信 |

## セキュリティ考慮

### 認証
- MQTT認証: 基本認証（開発時は無効）
- SSL/TLS: 本番時は必須

### アクセス制御
- Read権限: 全デバイス → ステータス系トピック
- Write権限: 管理者のみ → システムコマンド
- Write権限: ユーザー → UI制御

## 実装ガイドライン

### 送信側
1. 個別制御時は `{device_type}/{instance_id}/{function}` を使用
2. 一括制御時は `{device_type}/all/{function}` を使用
3. タイムスタンプを必ず含める
4. エラー処理で重複送信を避ける

### 受信側
1. 自分専用トピック + `/all/` トピックを購読
2. 重複メッセージは sequence または timestamp で検出
3. 不正なJSONペイロードは無視
4. 処理結果をステータスで報告

### 開発・デバッグ
- `debug/{device_type}/{instance_id}` - 開発時ログ出力
- `test/{any_topic}` - テスト専用トピック（本番環境では無効）

## 設定ファイル対応

各デバイスの `config.json` に以下の形式でトピック設定を記載：

```json
{
  "mqtt": {
    "topics": {
      "ui": "sphere/ui",
      "ui_all": "sphere/all/ui", 
      "ui_individual": "sphere/{instance_id}/ui",
      "command": "sphere/command",
      "command_all": "sphere/all/command",
      "command_individual": "sphere/{instance_id}/command",
      "status": "sphere/{instance_id}/status",
      "image": "sphere/{instance_id}/image"
    }
  }
}
```

## バージョン管理

- 本規約バージョン: v1.0
- 互換性: 後方互換性を重視、deprecation警告後削除
- 変更ログ: 各バージョンでの変更点を記録