# Docs Overview

共通ドキュメントはこの `doc/` ディレクトリに集約されています。まずは本リポジトリ全体像を把握し、各サブプロジェクトの README と合わせて参照してください。

## プロジェクト全体の概要

| プロジェクト | 役割 | 概要 |
|--------------|------|------|
| [MFT2025](../README.md) | ルート | 「Making Future Together 2025」のハードウェア／ソフトウェア試作群。球体ディスプレイやジョイスティック UI など、展示・遠隔体験のための実験的プロジェクトを格納。 |
| [isolation-sphere](../isolation-sphere/README.md) | ESP32-S3 球体ディスプレイ | BNO055 IMU＋LED 800 球構成の球体ディスプレイ。オープニング動画再生、姿勢補正、MQTT/UDP/OTA/Web UI 対応。Core0/1 を分担し、LittleFS＋PSRamFS でアセットを管理。 |
| [joystick](../joystick/AGENTS.md) | Sphere 向けリモートコントローラ | AtomS3R ベースのジョイスティック装置（README 整備中）。Wi-Fi AP を起動して Sphere からのアクセスを受け、MQTT／UDP コマンド送出を担当。`config.json` を共有し、UI/入力・ブザー・表示を制御。 |
| [`data/`](../data) | 共通アセット | LittleFS に焼き込む設定 (`config.json`)、LED レイアウト、画像アセットをまとめたディレクトリ。Sphere と Joystick の双方が参照する。 |

> **メモ:** Joystick プロジェクトの README は今後追加予定です。現状の開発ガイドラインは `joystick/AGENTS.md` と `doc/define_config.md` を参照してください。

## 共通ドキュメントの入り口

| ファイル | 役割 | 主な内容 |
|----------|------|----------|
| `define_config.md` | 設定スキーマ | `config.json` のトップレベル構成、モジュール別セクションの定義、運用ルール |
| `mqtt_rules.md` | MQTT ガイド | トピック設計、ペイロード形式、接続・再送のガイドライン |
| `ui_rules.md` | UI インタラクション | スフィア UI モード／LED フィードバック／操作ジェスチャに関する設計指針 |

## 運用メモ
- ドキュメントを更新した際は、関連するコードや `config.json` の変更と合わせてコミットしてください。特に `define_config.md` と `data/config.json` は常に同期させます。
- 各プロジェクト内の AGENTS.md（Sphere / Joystick）も `doc/` を参照しているため、リンク切れがないか定期的に確認してください。
- RasPi 向け仕様など新しいモジュールを追加する場合は、ここにサブセクションを追記し、対応する README と設定スキーマを整備してください。
