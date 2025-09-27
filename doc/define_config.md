# Configuration Definition (draft)

このドキュメントは `config.json` をモジュール横断で再構成する際の下書きです。現状の `isolation-sphere` / `joystick` 双方から利用できるよう、共通設定とモジュール別設定を明確に分けることを目的としています。

## 全体方針

- **共通項目はトップレベルに並べる。** `"common"` のようなラッパーは用いず、各モジュールが直接参照できるようにして冗長なネストを避けます。
- **モジュール固有の設定は中項目 (`"sphere"`, `"joystick"`, `"raspi"`) 配下に集約** し、共通定義との差分を明確化します。
- JSON スキーマは PlatformIO プロジェクト間で共有される `data/config.json` を基準とし、将来的には CI で JSON Schema バリデーションを行うことを想定します。

```
{
  "system": { ... },
  "wifi":   { ... },
  "mqtt":   { ... },
  "ota":    { ... },
  "movie":  { ... },
  "paths":  { ... },
  "sphere":   { ... },
  "joystick": { ... },
  "raspi":    { ... }
}
```

> 備考: `demo` セクションは名称を `movie` へ変更し、スフィア／ジョイスティック双方で利用できるようにします。

## 共通 (トップレベル) に配置する想定項目

| セクション | 役割 | 想定フィールド |
|-------------|------|----------------|
| `system` | 共通メタ情報 | `name`, `PSRAM`, `SPIFFS_size`, `debug`, `hardware_revision` *(追加検討)* |
| `wifi` | STA/AP 共通 Wi-Fi 設定 | `enabled`, `mode`, `ssid`, `password`, `max_retries`, `channel`, `ap_hidden` |
| `mqtt` | MQTT ブローカー共通設定 | `enabled`, `broker`, `port`, `username`, `password`, `keep_alive`, `topic` (共通 prefix など) |
| `ota` | OTA 共通設定 | `enabled`, `username`, `password`, `listen_port` |
| `movie` | メディア再生共通設定 | `frame_max`, `fps`, `play_mode`, `default_playlist` |
| `paths` | LittleFS/PSRamFS 上の共通パス | `config`, `images`, `opening`, `layout`, `logs` |

- `topic` は `status`, `ui`, `image`, `command` など共通で使用するトピック名を保持。必要に応じてモジュール側でプレフィックスを追加します。
- `wifi` の AP 専用設定（ローカル IP やサブネット等）は、共通セクションのサブオブジェクト `wifi.ap` として保持し、Sphere/Joystick で引き継げるようにします。

## `sphere` モジュール配下にまとめる項目

| サブセクション | 役割 | 既存フィールド | 備考 |
|-----------------|------|----------------|------|
| `display` | LCD/GFX 設定 | 幅・高さ・回転・オフセット・色深度・スイッチ | `debug` は `display.debug` に集約 |
| `image`   | 投影画像仕様 | `width`, `height`, `format`, `type` | `movie` と用途が重複するため整理が必要 |
| `paths` *(sphere固有)* | Sphere 特有のマウントポイント | LittleFS と PSRamFS の切り替え可否を含める |
| `imu`     | IMU 設定 | I2C ピン、ジェスチャ、更新周期、UI シェイク閾値 | `gesture_*` を `ui.gesture` へ寄せる案あり |
| `ui`      | Sphere UI 拡張 | `gesture_enabled`, `dim_on_entry`, `overlay_mode`, `brightness_profile` *(追加検討)* |
| `led`     | LED ストリップ設定 | `enabled`, `brightness`, `strip_gpios`, `num_strips`, `leds_per_strip`, `total_leds` | |
| `buzzer`  | サウンド設定 | `enabled`, `volume`, `toneset` *(追加検討)* |
| `storage` *(新設案)* | LittleFS / PSRamFS | `psram_size_mb`, `auto_format`, `mirror_assets` |

## `joystick` モジュール配下にまとめる項目

| サブセクション | 役割 | 既存フィールド | 備考 |
|-----------------|------|----------------|------|
| `wifi_ap` | AP 構成 | `ssid`, `password`, `local_ip`, `gateway`, `subnet`, `channel`, `hidden`, `max_connections` | `wifi` 共通設定と重複する項目は統合して冗長さを解消 |
| `udp` | Sphere へのストリーム転送 | `target_ip`, `port`, `update_interval_ms`, `joystick_read_interval_ms`, `max_retry_count`, `timeout_ms` | 共通セクションに `udp.enabled` を設ける案あり |
| `system` | Joystick 本体 | `buzzer_enabled`, `buzzer_volume`, `opening_animation_enabled`, `lcd_brightness`, `debug_mode`, `device_name` |
| `input` | 入力調整 | `deadzone`, `invert_left_y`, `invert_right_y`, `timestamp_offset_ms`, `sensitivity_profile` *(追加案)* |
| `ui` | UI レイヤ | `use_dual_dial`, `default_mode`, `button_debounce_ms`, `led_feedback` *(追加案)* |

## `raspi` モジュール配下に予定する項目

現状 `config.json` に項目は存在しませんが、以下のようなサブセクションを想定しています。

| サブセクション | 役割 | 想定フィールド |
|-----------------|------|----------------|
| `network` | Pi 側ブリッジ接続 | `ip`, `gateway`, `subnet`, `interface`, `mtu` |
| `sync` | Sphere/Joystick との同期 | `mode` (`mqtt`, `udp`, `serial`), `poll_interval_ms`, `retry` |
| `media` | Raspberry Pi 側メディア管理 | `playlist_path`, `thumbnail_cache`, `transcode_on_upload` |

## 今後のステップ

1. 上記構成で `config.json` のドラフトを作成し、Sphere/Joystick 双方のローダー (`ConfigManager`, 各モジュールの設定クラス) を対応させる。
2. `define_config.md` を基に JSON Schema (`schema/config.schema.json`) を用意し、ユニットテスト／CI で構造を検証する。
3. ドキュメント (`README`, `joystick/AGENTS.md` など) から当ドキュメントへの参照リンクを追加する。

> メモ: 実際の JSON 変更は段階的に進め、まずは Sphere 側ローダーと `joystick` プロジェクトの両方で後方互換を確保した状態で移行する。
