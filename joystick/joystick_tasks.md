# 📝 Joystick 実装タスク表

## 🎉 実装完了サマリー

✅ **WiFi AP機能**: 独立ローカルネットワーク提供 (192.168.100.x)  
✅ **内蔵MQTTブローカー**: ポート1883でのフル機能MQTT実装  
✅ **設定管理システム**: ConfigManagerでのconfig.json完全対応  
✅ **オーディオシステム**: ブザー制御・音量管理・開発用無音化  
✅ **マルチコア制御**: Core0(ネットワーク)/Core1(UI)タスク分担  
✅ **共通ライブラリ化**: 再利用可能なライブラリ構造  

**🚧 残り作業**: UI論理状態管理とMQTTトピック高度化のみ

## マルチコア環境設定
- [x] [P1] `/Users/katano/Documents/PlatformIO/Projects/MFT2025/isolation-sphere/src/core` モジュールを取り込み、ジョイスティック用に Core0/1 タスク初期化コードを移植する。
- [x] [P1] `sharestate` 機能をジョイスティック固有データ（スティック入力・ボタン状態・通信状態）向けに再構成し、Core 間の共有 API を整備する。
- [ ] [P2] マルチコアタスクと共有ステートの統合テストを追加し、`pio test -e atoms3r` に組み込む。
- [x] core0：config.json管理．wifiAP，MQTTブローカー
- [x] core1：UI，ジョイスティック管理

## 設定管理
- [x] [P1] **ConfigManager実装完了**: `config.json`読み込み、全セクション対応、共有ステート反映完了
- [x] [P1] **LittleFS統合**: 設定ファイルのLittleFSからの読み込み機能実装完了
- [x] [P1] **設定構造文書化**: CONFIG_STRUCTURE.mdでjoystick設定項目を完全文書化
- [ ] [P2] LittleFS から設定・アセットを読み込み、`ESP32-PSRamFS` にコピーする RAMDisk ワークフローを `isolation-sphere` と同構成で整備する。



## WiFi AP機能 ✅ **実装完了**
- [x] [P1] **WiFi AP基本機能**: config.json設定に基づくSSID/パスワード/IP自動設定
- [x] [P1] **接続クライアント管理**: リアルタイム監視、ログ出力、最大接続数制御
- [x] [P1] **WiFi制御スイッチ**: config.jsonのenabledでAP使用/不使用切替
- [x] [P1] **可視性制御**: config.jsonのvisibleでAP可視/不可視切替
- [x] [P1] **ネットワーク設定**: 192.168.100.xネットワーク、チャンネル6、最大8接続
- [ ] [P2] 接続するデバイスのIPアドレスをコントロール


## MQTTブローカー機能 ✅ **基本実装完了**
- [x] [P1] **内蔵MQTTブローカー**: ポート1883での完全なMQTTブローカー機能
- [x] [P1] **config.json統合**: MQTT設定の自動読み込みと反映
- [x] [P1] **基本トピック実装**: joystick/status, joystick/test, joystick/state送信
- [x] [P1] **リアルタイム送信**: ジョイスティック状態とシステム状態の定期送信
- [x] [P1] **MQTTテスト完了**: 全トピックでの送受信動作を検証完了
- [x] [P1] **MQTT_RULES.md文書化**: 完全なトピック設計とペイロード仕様を文書化
- [ ] [P2] MQTT ブローカー不在時のフェイルセーフ（リトライ間隔、オフライン時の UI 表示）を設計する。


## UI統合・MQTT高度化 🚧 **実装予定**

### UI論理状態管理
- [ ] [P1] **UIStateManager** クラス実装: ボタン入力をUI論理状態に変換
- [ ] [P1] **UIモード管理**: sphere_control, playlist_control, system_config, multi_sphere
- [ ] [P2] **UI状態の永続化**: 設定変更の保存・復元

### MQTT トピック高度化  
- [ ] [P1] **MQTT_RULES.md に基づくトピック構造の再設計**: `joystick/*` から `sphere/ui/*` への移行
- [ ] [P1] **sphere/ui/control トピック**: メインUI状態の送信
- [ ] [P2] **sphere/ui/mode, sphere/ui/selection**: モード切替・選択状態の送信

## オーディオ機能
- [x] [P1] **JoystickBuzzerとM5SpeakerBuzzer実装完了**: GPIO5 PWMとM5Speakerの両方に対応
- [x] [P1] **起動音システム**: 起動時に起動音を自動再生
- [x] [P1] **config.jsonオーディオ設定**: audioセクションで詳細音量制御を実装
- [x] [P1] **音種別音量制御**: startup, click, error, testの個別音量調整機能
- [x] [P1] **開発用無音化**: audio.enabled=falseで開発時の静音環境を実現
- [ ] [P2] ボタン操作や状態変化に応じた効果音を追加する。

## 共通ライブラリ化 ✅ **実装完了**
- [x] [P1] **ConfigManager共通化**: config.json読み込み機能をlib/config/で共通化
- [x] [P1] **CoreTask共通化**: マルチコア管理機能をlib/core/で共通化
- [x] [P1] **SharedState共通化**: Core間データ共有機能をlib/core/で共通化
- [x] [P1] **ライブラリ構造設計**: include/とsrc/の適切な分離と依存関係管理
- [x] [P1] **isolation-sphereとの互換性**: 既存プロジェクトとの共通ライブラリ利用
