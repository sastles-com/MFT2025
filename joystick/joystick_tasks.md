# 📝 Joystick 実装タスク表（暫定）

## マルチコア環境設定
- [ ] [P1] `/Users/katano/Documents/PlatformIO/Projects/MFT2025/isolation-sphere/src/core` モジュールを取り込み、ジョイスティック用に Core0/1 タスク初期化コードを移植する。
- [ ] [P1] `sharestate` 機能をジョイスティック固有データ（スティック入力・ボタン状態・通信状態）向けに再構成し、Core 間の共有 API を整備する。
- [ ] [P2] マルチコアタスクと共有ステートの統合テストを追加し、`pio test -e atoms3r` に組み込む。
- [ ] core0：config.json管理．wifiAP，MQTTブローカー
- [ ] core1：UI，ジョイスティック管理

## 設定管理
- [ ] [P1] `ConfigManager` を導入し、`config.json` の読み込みと共有ステート反映を実装する。
- [ ] [P2] LittleFS から設定・アセットを読み込み、`ESP32-PSRamFS` にコピーする RAMDisk ワークフローを `isolation-sphere` と同構成で整備する。



## wifi AP機能
- [ ] [P1] `config.json` の `wifi` セクション（`mode`, `ssid`, `password`, `max_retries`, `ap.*`）を読み込み、ジョイスティック本体を Wi-Fi AP として起動する。
- [ ] [P1] 接続クライアント監視・ログ出力、チャンネル設定など AP 運用に必要な追加パラメータを整理し、必要なら `define_config.md` に反映する。
- [ ] [P1] `wifi.enabled` / `wifi.mode` / `wifi.ap.hidden` を尊重した起動制御（使用/不使用・可視/不可視切り替え）を実装する。
- [ ] [P2] クライアント IP の固定化は行わず、接続端末情報（MAC / 割当 IP）の取得ロジックを整理してログへ反映する。


## MQTTブローカー機能
- [ ] [P1] `config.json` の MQTT 項目（ブローカーアドレス、ポート、トピック）を読み込み、ジョイスティックからステータス・入力情報を publish する。
- [ ] [P1] MQTT ブローカー不在時のフェイルセーフ（リトライ間隔、オフライン時の UI 表示）を設計する。


## UI機能

## ブザー機能
- [ ] [P1] `isolation-sphere` の `BuzzerManager` を流用し、ジョイスティック向けに統合。
- [ ] [P1] 起動時に起動音を鳴らす．
- [ ] [P1] config.jsonのjoystick項目に，soundのenable/disableのスイッチを追加
- [ ] [P2] ボタン操作や状態変化に応じた効果音を追加する。

## 共通ライブラリ化
- [ ] [P1] `ConfigManager` / `BuzzerManager` などコア機能を `lib/` 配下に共通ライブラリ化し、`isolation-sphere` / `joystick` から参照できるよう整理する。
- [ ] [P1] 移行するべきmanagersをリストアップ
