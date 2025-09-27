# 📝 Joystick 実装タスク表（暫定）

## マルチコア環境設定
- [x] [P1] `/Users/katano/Documents/PlatformIO/Projects/MFT2025/isolation-sphere/src/core` モジュールを取り込み、ジョイスティック用に Core0/1 タスク初期化コードを移植する。
- [x] [P1] `sharestate` 機能をジョイスティック固有データ（スティック入力・ボタン状態・通信状態）向けに再構成し、Core 間の共有 API を整備する。
- [ ] [P2] マルチコアタスクと共有ステートの統合テストを追加し、`pio test -e atoms3r` に組み込む。
- [x] core0：config.json管理．wifiAP，MQTTブローカー
- [x] core1：UI，ジョイスティック管理

## 設定管理
- [x] [P1] `ConfigManager` を導入し、`config.json` の読み込みと共有ステート反映を実装する。
- [ ] [P2] LittleFS から設定・アセットを読み込み、`ESP32-PSRamFS` にコピーする RAMDisk ワークフローを `isolation-sphere` と同構成で整備する。



## wifi AP機能
- [x] [P1] `config.json` の `wifi` セクションから SSID / パスワード / IP 設定を読み込み、ジョイスティック本体を Wi-Fi AP として起動する。
- [x] [P1] 接続クライアント監視・ログ出力、チャンネル設定など AP 運用に必要な追加パラメータを整理し、必要なら `config.json` に項目を追加する。
- [x] [P1] config.jsonのwifi　にenabledのスイッチを追加して，APの使用/不使用を切り替え可能に
- [x] [P1] config.jsonのwifi　にvisibleのスイッチを追加して，APの可視/不可視を切り替え可能に
- [ ] [P2] 接続するデバイスのIPアドレスをコントロール


## MQTTブローカー機能
- [x] [P1] `config.json` の MQTT 項目（ブローカーアドレス、ポート、トピック）を読み込み、ジョイスティックからステータス・入力情報を publish する。
- [ ] [P1] MQTT ブローカー不在時のフェイルセーフ（リトライ間隔、オフライン時の UI 表示）を設計する。


## UI機能

## ブザー機能
- [ ] [P1] `isolation-sphere` の `BuzzerManager` を流用し、ジョイスティック向けに統合。
- [ ] [P1] 起動時に起動音を鳴らす．
- [x] [P1] config.jsonのjoystick項目に，soundのenable/disableのスイッチを追加
- [ ] [P2] ボタン操作や状態変化に応じた効果音を追加する。

## 共通ライブラリ化
- [x] [P1] `ConfigManager` / `BuzzerManager` などコア機能を `lib/` 配下に共通ライブラリ化し、`isolation-sphere` / `joystick` から参照できるよう整理する。
- [x] [P1] 移行するべきmanagersをリストアップ
