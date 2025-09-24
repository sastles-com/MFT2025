# 📝 Isolation Sphere 実装タスク表（優先度付き）

## 起動シーケンス
- [x] [P1] M5Unified のライフサイクル（`M5.begin()`）と LittleFS を統合初期化
- [x] [P1] LittleFS の初期化処理を実装
- [x] [P1] `config.json` 読み込み機能を実装
  - [x] [P1] `ArduinoJson` ベースの設定ローダーを整備
- [P1] `display/switch` 判定 → LCD表示のON/OFF制御 
- [P1] 起動音をブザーで再生
- [P1] オープニングムービー（LittleFS内JPEG連番）の再生処理を実装 
- [P1] 起動完了後に LCD に `"Main System Ready"` を表示
- [P2] LittleFS からオープニング／デモムービーを `ESP32-PSRamFS` RAMDisk にコピーするワークフローの検討
  - [P2] `ESP32-PSRamFS` の初期化サンプルを移植して `psramfs.begin()` を実装

## PlatformIO ビルド & デバッグ
- [P1] `pio run -e atoms3r` でビルドが通ることを CI 化
- [P1] `pio run -t upload -e atoms3r` で USB 書き込みシーケンスを整備
- [P1] `pio device monitor -b 115200` に例外デコーダーを連携（`platformio.ini` の `monitor_filters` を確認）
- [P2] `pio run -t clean` と `pio test -e atoms3r` を組み込んだメンテタスクを追加
- [P2] `pio run -t uploadfs`（LittleFS）をジョブに追加し、アセット更新も自動化

## LittleFS，RAMDisk
- [P1] LittleFS の初期化・ 3MB 目安の領域を確保（`board_build.partitions` を確認）
- [P2] `ESP32-PSRamFS` RAMDisk 容量確保（3MB 目安）
  - [P2] 起動時に Images フォルダの内容を LittleFS → PSRamFS にコピー
  - [P2] `File` API 互換ラッパーで RAMDisk から JPEG 連番を読み込み、`LovyanGFX` / `TJpg_Decoder` に渡すルーチンを検討



## ディスプレイ制御
- [P1] `M5Unified` + `M5GFX` / `LovyanGFX` を用いた LCD 初期化 
- [P1] `config.json` から解像度 / 回転 / カラーデプスを設定
- [P1] JPEG再生機能（LittleFS + `TJpg_Decoder`）を実装 
- [P2] UDP経由のJPEGフレーム受信・表示処理を実装
- [P2] ダブルバッファ / リングバッファの比較・評価

## LED 球体制御
- [P1] LED配置CSV（3D座標, Strip ID, Index）の読み込み
- [P1] MQTTのUIから取得した緯度・軽度のオフセット値とquaternion値を合成
- [P1] `FastLED` I2S DMA による WS2812 並列駆動へ移行（4 ストリップ同時更新を可能にする）
  - [P1] `FASTLED_ESP32_I2S` 実装の GPIO／DMA リソース確認（GPIO 5/6/7/8）
  - [P1] FastLED フレームバッファ最適化（PSRamFS からのデータ流し込み）
  - [P1] 既存 RMT 実装とのコンパイル時切り替えと段階的撤去
  - [P1] I2S 駆動のプロファイル取得と 30fps 目標達成に向けた最適化
- [P2] LED更新タイミングをIMU姿勢更新と同期

- [P1] arctan, sqrt　の近似計算の検討（提供された `_sqrtinv` / `_sqrt` / `_atan2` サンプルの精度を起点に検証）
  - [P1] https://github.com/sastles-com/testESP32
- [P1] https://tajmahal0707.hatenablog.com/?_gl=1*zvi0tc*_gcl_au*OTQ2MTg4MjgxLjE3NTgyNjc3NzA.
- [P1] 球体座標系の定義：南極-北極方向を +z 軸、赤道面で右手系を構成（必要なら `led_layout.csv` を調整）
- [P1] 球体の姿勢によって回転したLEDの３次元位置をテクスチャのUV座標に変換する
  - [P1] pos' = quat.RotationVector(pos) の計算（オフセット含む）
  - [P1] (u, v) = (arctan2(sqrt(pos'.x^2+pos'.z^2), pos'.y), arctan2(pos'.x, pos'.z)) の計算
  - [P1] `_atan2` の戻り値（deg/180 単位）をラジアンへ変換し、球面座標として `u∈[-π, π)`（経度）、`v∈[-π/2, π/2]`（緯度）を原点0で扱う
  - [P1] テクスチャ参照時に u, v を `[0, 1)` に写像する（中心0→0.5、`v` は `π` 周期、`u` は `2π` 周期。負角や `2π` 超えを巻き戻し、境界でラップアラウンドする）
  - [P1] uv座標を使って，画像上のRGB値を取得

## IMU制御
- [P1] `Adafruit_BNO055` ライブラリでクォータニオンを取得 
- [P1] 30Hz以上で姿勢更新ループを実装 
- [P2] 「強く2回振る」でUIモード遷移 (LCDにSHAKE表示)（`Adafruit_BNO055::getEvent` ベース） 
- [P2] UIモード時 → 回転操作でUI制御

## ブザー制御
- [P1] `buzzer_manager.c / .h` を組み込み（`M5Unified::Speaker` 連携）
- [P1] 効果音APIを統合（起動音 / 成功音 / エラー音 / 通知音 / シャットダウン音）
- [P2] 音量制御・ミュート制御を `config.json` に対応（`M5Unified::Speaker`）
- [P2] Core1 に固定して並列再生を実装

## 通信制御
- [P1] ネットワーク接続機能
  - [P1] config.jsonから接続先を読み込み
  - [P1] SSID情報から接続
  - [P2] 再接続対応
- [P1] MQTTクライアントを実装（`AsyncMqttClient`）
  - [P1] config.jsonからMQTT情報を取得
  - [P1] ブローカー `192.168.10.1` に自動接続
  - [P1] topics
    - [P1] UIトピック (`sphere/ui`) を購読
    - [P1] ステータストピック (`sphere/status`) を定期送信
    - [P1] imageトピック（`sphere/image`）
  - [P1] `AsyncMqttClient` を利用し再接続と QoS 管理を実装
<!-- - [P2] UDPフレーム受信タスクを実装
  - [P1] config.jsonからP2P情報を取得
  - [P1] DMA受信できるかどうか確認
  - [P2] フレームをダブルバッファに保存
  - [P2] 表示タスクと非同期に動作   -->
- [P1] udp転送とMQTT共有のどちらの方式が良いのか比較
  - [P1] `AsyncUDP` / `ESPAsyncUDP` の利用を検討し、UDP ストリーム処理を非同期化

## OTA アップデート
- [P1] `AsyncTCP` + `ESP Async WebServer` を導入し、OTA 用の HTTPS/HTTP サーバを起動
- [P1] `AsyncElegantOTA` を組み込み、ブラウザ経由で `ota_0` / `ota_1` にファーム更新
- [P1] Basic 認証・CSRF 防止などアップロード保護を設定（資格情報は `config.json`）
- [P2] OTA 実行後に自動リブートし、動作確認フィードバック（MQTT 通知 / LCD 表示）を実装
- [P2] バッテリー駆動時の中断対策としてロールバック条件を検討（起動確認まで旧パーティション維持）

## マルチコア設計
- [P1] Core0: 起動処理 + 通信・受信（LittleFS, MQTT/UDP, config管理）
- [P1] Core1: センサー・表示系（IMU更新, LED制御, LCD/ブザー）
- [P2] タスク優先度と負荷分散の見直し (`xTaskCreatePinnedToCore`)

## UI機能
- [P1] system 
  - [P1] 表示オン/オフ
  - [P2] スピーカー　オンオフ
  - [P1] 再起動
  - [P1] ローカル/UDP（MQTT）通信切り替えスイッチ
- [P1] 表示系
  - [P1] 緯度経度のオフセット
  - [P2] 明るさ調整
- [P1] 動画制御
  - [P1] 再生•ストップ
  - [P1] next, prev
  - [P2] ループ　オンオフ
- [P1] デバッグ機能
  - [P2] LCDオンオフ
  - [P1] 座標系インジケータ：ｘ（赤），ｙ（緑），ｚ（青）の光点を球体表面上に表示
  - [P2] 北極から南極にグラデーション
  - [P2] 緯度と経度に沿って線を引いたデモ画像
- [P1] UIの状態を最新に保持して，マルチコアどちらからも参照できるようにして

### 座標系インジケータ
- [P1] 座標系インジケータとは，座標系のxyz軸と球体の交点にインジケータを表示する
  <!-- - [P1] x軸＋方向と球体が交差するところに赤光点（点ではなく範囲幅を持ってもいい）
  - [P1] y軸＋方向と球体が交差するところに緑光点（点ではなく範囲幅を持ってもいい）
  - [P1] z軸＋方向と球体が交差するところに青光点（点ではなく範囲幅を持ってもいい） -->
- [P1] 現実空間での座標系なので，球体が姿勢を変えても空間的に固定した方向に座標系インジケータを表示
- [P1] 実装方法
  - [P1] x軸（1.0,0.0,0.0）の緯度・経度にIMUとオフセットを使って変換をかけたuv座標を求める
  - [P1] y軸（0.0,1.0,0.0）の緯度・経度にIMUとオフセットを使って変換をかけたuv座標を求める
  - [P1] z軸（0.0,0.0,1.0）の緯度・経度にIMUとオフセットを使って変換をかけたuv座標を求める
- [P1] 各uv座標に最も近接した位置にあるLEDの数値をそれぞれR（0xff0000），G（0x00ff00），B（0x0000ff）とする
  - [P1] それ以外のLEDは黒（0x000000）とする
  - [P2] 表示する画像がある場合，黒にせずに画像を描画した後軸に相当するLEDだけRGB各色に変換
- [P1] 使い方は２通り
  - [P1] 一つはデバッグ用で，画像を表示しながらLEDの位置・変換を確認する
  - [P1] もう一つはUI用で，shakeしてUIモードに入ったらこの画面にする



<!-- ## 補助機能
- [P2] JSONロガー → 動作状態をUART出力
- [P2] エラー検出時にブザー警告音を鳴らす
- [P3] 将来拡張: WebUI経由で `config.json` 更新 → LittleFSへ保存   -->

✅ **P1 = 最低限動作に必須**
✅ **P2 = 安定運用のために必要**
✅ **P3 = 将来の拡張**
