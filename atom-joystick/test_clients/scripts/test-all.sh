#!/bin/bash
# test-all.sh - 全テストクライアント一括実行スクリプト
# isolation-sphere Atom-JoyStick 統合テスト支援

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# デフォルト設定
BROKER_IP="${BROKER_IP:-192.168.100.1}"
MQTT_PORT="${MQTT_PORT:-1883}"
WEB_PORT="${WEB_PORT:-8000}"
TEST_DURATION="${TEST_DURATION:-30}"  # テスト実行時間（秒）

# カラー出力設定
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

log() {
    echo -e "${BLUE}[$(date '+%H:%M:%S')]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

echo -e "${PURPLE}🧪 isolation-sphere テストクライアント統合実行${NC}"
echo -e "${PURPLE}================================${NC}"
echo "📡 MQTTブローカー: $BROKER_IP:$MQTT_PORT"
echo "🌐 WebUIポート: $WEB_PORT"
echo "⏱️ テスト時間: ${TEST_DURATION}秒"
echo ""

# プロジェクトディレクトリに移動
cd "$PROJECT_DIR"

# 仮想環境確認
if [ ! -d ".venv" ]; then
    error "仮想環境が見つかりません"
    echo "💡 セットアップ実行: ./scripts/setup.sh"
    exit 1
fi

# 仮想環境アクティベート
log "仮想環境アクティベート中..."
source .venv/bin/activate

# 依存関係確認
log "依存関係確認中..."

# 個別パッケージチェック
missing_packages=()

# paho-mqtt チェック
if ! python -c "import paho.mqtt.client" 2>/dev/null; then
    missing_packages+=("paho-mqtt")
fi

# requests チェック
if ! python -c "import requests" 2>/dev/null; then
    missing_packages+=("requests")
fi

# flask チェック
if ! python -c "import flask" 2>/dev/null; then
    missing_packages+=("flask")
fi

# psutil チェック
if ! python -c "import psutil" 2>/dev/null; then
    missing_packages+=("psutil")
fi

if [ ${#missing_packages[@]} -gt 0 ]; then
    error "依存関係が不足しています"
    echo ""
    echo "❌ 不足しているパッケージ:"
    for package in "${missing_packages[@]}"; do
        echo "   - $package"
    done
    echo ""
    echo "💡 修復方法（推奨順）:"
    echo ""
    echo "【方法1】自動セットアップスクリプト実行（推奨）"
    echo "   ./scripts/setup.sh"
    echo ""
    echo "【方法2】手動インストール"
    echo "   source .venv/bin/activate"
    echo "   uv sync                    # pyproject.tomlから自動インストール"
    echo ""
    echo "【方法3】個別パッケージインストール"
    echo "   source .venv/bin/activate"
    for package in "${missing_packages[@]}"; do
        echo "   uv pip install $package"
    done
    echo ""
    echo "🔍 現在の仮想環境状態:"
    if [[ -n "$VIRTUAL_ENV" ]]; then
        echo "   ✅ 仮想環境アクティブ: $VIRTUAL_ENV"
    else
        echo "   ⚠️ 仮想環境未アクティブ"
        echo "   → 'source .venv/bin/activate' を実行してください"
    fi
    
    # Pythonバージョン確認
    python_version=$(python --version 2>&1 || echo "Python取得エラー")
    echo "   Python: $python_version"
    
    exit 1
fi

success "依存関係確認完了"
echo "   ✅ paho-mqtt, requests, flask, psutil - インポート成功"

# ログディレクトリ確認
mkdir -p logs

# プロセスID記録用
PIDS=()
cleanup() {
    log "🛑 テストクライアント終了処理..."
    for pid in "${PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            log "プロセス $pid 終了中..."
            kill -TERM "$pid" 2>/dev/null || true
        fi
    done
    
    # 少し待機してからKILL
    sleep 2
    for pid in "${PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            log "プロセス $pid 強制終了中..."
            kill -KILL "$pid" 2>/dev/null || true
        fi
    done
    
    success "全テストクライアント終了完了"
}

# 終了時クリーンアップ
trap cleanup EXIT INT TERM

# 1. ネットワーク疎通確認
log "📶 ネットワーク疎通確認..."

# 詳細なWiFi接続状態確認
current_network=$(networksetup -getairportnetwork en0 2>/dev/null | sed 's/Current Wi-Fi Network: //' || echo 'WiFi取得エラー')
current_ip=$(ifconfig en0 2>/dev/null | grep 'inet ' | awk '{print $2}' | head -1 || echo 'IP取得エラー')

echo "📍 現在のネットワーク状態:"
echo "   WiFi接続: $current_network"
echo "   IPアドレス: $current_ip"
echo ""

# Pingテスト実行
log "Pingテスト実行中..."
ping_result=$(python network_tester.py --target "$BROKER_IP" --ping-only --count 5 2>&1)
ping_exit_code=$?

echo "$ping_result"

# Ping結果の詳細解析
if echo "$ping_result" | grep -q "❌ 全て失敗"; then
    error "ネットワーク接続に問題があります"
    
    echo ""
    echo "🔍 詳細診断:"
    echo "   対象IP: $BROKER_IP"
    echo "   現在のWiFi: $current_network"
    echo "   現在のIP: $current_ip"
    echo ""
    
    echo "💡 確認事項:"
    echo "   1. Atom-JoyStickデバイス状態:"
    echo "      - デバイスの電源がONになっているか"
    echo "      - Arduino IDEでプログラムが正常にフラッシュ済みか"
    echo "      - WiFi APモードが開始されているか"
    echo ""
    echo "   2. MacBookのWiFi設定:"
    if [[ "$current_network" != *"IsolationSphere-Direct"* ]]; then
        echo "      ⚠️ 現在のWiFi: $current_network"
        echo "      ✅ 推奨WiFi: IsolationSphere-Direct"
        echo "      → WiFi設定でIsolationSphere-Directに接続してください"
    else
        echo "      ✅ WiFi接続: IsolationSphere-Directに接続済み"
    fi
    echo ""
    
    echo "   3. IPアドレス範囲確認:"
    if [[ "$current_ip" =~ ^192\.168\.100\. ]]; then
        echo "      ✅ IPアドレス: $current_ip (想定範囲内)"
        echo "      → Atom-JoyStickの起動状態を確認してください"
    elif [[ "$current_ip" =~ ^192\.168\.49\. ]]; then
        echo "      ℹ️ IPアドレス: $current_ip (レガシー範囲)"
        echo "      → レガシーESP32-P2P-Directに接続中です"
    else
        echo "      ⚠️ IPアドレス: $current_ip (想定外の範囲)"
        echo "      → IsolationSphere-DirectのWiFiに接続し直してください"
    fi
    echo ""
    
    echo "🔧 修復手順:"
    echo "   1. Atom-JoyStickの起動確認・再起動"
    echo "   2. MacBookのWiFi設定確認"
    echo "      - 設定 → Wi-Fi → IsolationSphere-Direct選択"
    echo "   3. 接続後、IPアドレス確認"
    echo "      - 192.168.100.x 範囲のIPが割り当てられることを確認"
    echo "   4. 再度テスト実行: ./scripts/test-all.sh"
    echo ""
    echo "🆘 問題が解決しない場合:"
    echo "   - Arduino IDEのシリアルモニターでAtom-JoyStickのログ確認"
    echo "   - デバイス再フラッシュの検討"
    
    exit 1
    
elif echo "$ping_result" | grep -q "✅"; then
    # 成功パターンが含まれている場合
    success "ネットワーク疎通確認完了"
    if echo "$ping_result" | grep -q "🟢 優秀"; then
        log "ネットワーク品質: 優秀 (低遅延・安定)"
    elif echo "$ping_result" | grep -q "🟡 良好"; then
        log "ネットワーク品質: 良好 (実用レベル)"
    else
        log "ネットワーク品質: 通信可能"
    fi
    
else
    # 予期しない結果パターン
    warning "Ping結果の判定ができませんでした"
    echo "Pingテスト終了コード: $ping_exit_code"
    echo "結果内容を確認してください"
    
    # 継続するかユーザーに確認
    read -p "テストを継続しますか？ [y/N]: " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "テスト中断"
        exit 1
    fi
fi

echo ""

# 2. MQTTモニター開始（バックグラウンド）
log "📡 MQTT監視開始..."
python mqtt_monitor.py > logs/mqtt_monitor.log 2>&1 &
MQTT_PID=$!
PIDS+=($MQTT_PID)
success "MQTT監視開始 (PID: $MQTT_PID)"

sleep 2

# 3. ダミーESP32クライアント開始
log "🤖 ダミーESP32クライアント開始..."
python dummy_esp32_client.py \
    --broker "$BROKER_IP" \
    --port "$MQTT_PORT" \
    --device "esp32-test-01" \
    > logs/dummy_esp32.log 2>&1 &
ESP32_PID=$!
PIDS+=($ESP32_PID)
success "ダミーESP32開始 (PID: $ESP32_PID)"

sleep 3

# 4. ダミーraspiクライアント開始
log "🍓 ダミーraspiクライアント開始..."
python dummy_raspi_client.py \
    --broker "$BROKER_IP" \
    --mqtt-port "$MQTT_PORT" \
    --web-port "$WEB_PORT" \
    > logs/dummy_raspi.log 2>&1 &
RASPI_PID=$!
PIDS+=($RASPI_PID)
success "ダミーraspi開始 (PID: $RASPI_PID, WebUI: http://localhost:$WEB_PORT)"

sleep 5

# 5. 統合動作確認
log "🔍 統合動作確認中..."

# プロセス生存確認
log "プロセス動作状態確認..."

all_running=true
process_status=()

# PIDと対応するプロセス名のマッピング
if [ -n "${MQTT_PID:-}" ]; then
    if kill -0 "$MQTT_PID" 2>/dev/null; then
        process_status+=("MQTT監視: ✅ 動作中 (PID:$MQTT_PID)")
    else
        process_status+=("MQTT監視: ❌ 停止 (PID:$MQTT_PID)")
        all_running=false
    fi
fi

if [ -n "${ESP32_PID:-}" ]; then
    if kill -0 "$ESP32_PID" 2>/dev/null; then
        process_status+=("ESP32ダミー: ✅ 動作中 (PID:$ESP32_PID)")
    else
        process_status+=("ESP32ダミー: ❌ 停止 (PID:$ESP32_PID)")
        all_running=false
    fi
fi

if [ -n "${RASPI_PID:-}" ]; then
    if kill -0 "$RASPI_PID" 2>/dev/null; then
        process_status+=("raspiダミー: ✅ 動作中 (PID:$RASPI_PID)")
    else
        process_status+=("raspiダミー: ❌ 停止 (PID:$RASPI_PID)")
        all_running=false
    fi
fi

# プロセス状態表示
echo "📋 プロセス動作状態:"
for status in "${process_status[@]}"; do
    echo "   $status"
done

if [ "$all_running" = true ]; then
    success "全プロセス正常動作中"
    echo "   監視中のプロセス数: ${#PIDS[@]}"
else
    error "一部プロセスに問題があります"
    echo ""
    echo "🔍 トラブルシューティング:"
    echo "   1. ログファイル確認:"
    echo "      ls -la logs/"
    echo "      cat logs/mqtt_monitor.log"
    echo "      cat logs/dummy_esp32.log"
    echo "      cat logs/dummy_raspi.log"
    echo ""
    echo "   2. プロセス手動確認:"
    echo "      ps aux | grep python"
    echo "      ps aux | grep dummy"
    echo ""
    echo "   3. ポート使用状況確認:"
    echo "      lsof -i :$MQTT_PORT"
    echo "      lsof -i :$WEB_PORT"
    echo ""
    echo "💡 問題が続く場合は、./scripts/clean.sh でクリーンアップ後に再実行"
fi

# 6. テスト実行期間待機
log "⏳ ${TEST_DURATION}秒間テスト実行中..."
log "💡 以下を確認してください:"
log "   - MQTT通信ログ: tail -f logs/mqtt_monitor.log"
log "   - ESP32ダミー: tail -f logs/dummy_esp32.log"  
log "   - raspiダミー: tail -f logs/dummy_raspi.log"
log "   - WebUI: http://localhost:$WEB_PORT"

# カウントダウン表示
for ((i=TEST_DURATION; i>0; i--)); do
    printf "\r⏱️ 残り時間: %02d秒 " $i
    sleep 1
done
echo ""

# 7. 結果レポート生成
log "📊 テスト結果レポート生成中..."

# ログディレクトリ確認
if [ ! -d "logs" ]; then
    warning "ログディレクトリが存在しません"
    mkdir -p logs
fi

# テスト実行時刻記録
test_end_time=$(date '+%Y-%m-%d %H:%M:%S')
total_test_time=$((SECONDS))

echo ""
echo "============================================================"
echo "📋 isolation-sphere Atom-JoyStick 統合テスト結果"
echo "============================================================"
echo "🕐 テスト期間: $test_end_time ($total_test_time秒)"
echo "📍 対象システム: $BROKER_IP:$MQTT_PORT (WebUI:$WEB_PORT)"
echo ""

# ネットワーク状態サマリー
echo "🌐 ネットワーク状態:"
final_network=$(networksetup -getairportnetwork en0 2>/dev/null | sed 's/Current Wi-Fi Network: //' || echo '不明')
final_ip=$(ifconfig en0 2>/dev/null | grep 'inet ' | awk '{print $2}' | head -1 || echo '不明')
echo "   WiFi接続: $final_network"
echo "   IPアドレス: $final_ip"

if [[ "$final_network" == *"IsolationSphere-Direct"* ]] && [[ "$final_ip" =~ ^192\.168\.100\. ]]; then
    echo "   評価: 🟢 正常 (想定ネットワーク接続)"
elif [[ "$final_ip" =~ ^192\.168\.49\. ]]; then
    echo "   評価: 🟡 レガシー (ESP32-P2P-Direct接続)"
else
    echo "   評価: 🔴 要確認 (想定外ネットワーク)"
fi
echo ""

# プロセス実行結果サマリー
echo "🔧 プロセス実行結果:"
process_success_count=0
if [ -n "${MQTT_PID:-}" ] && kill -0 "$MQTT_PID" 2>/dev/null; then
    echo "   MQTT監視: 🟢 正常動作"
    process_success_count=$((process_success_count + 1))
else
    echo "   MQTT監視: 🔴 動作停止"
fi

if [ -n "${ESP32_PID:-}" ] && kill -0 "$ESP32_PID" 2>/dev/null; then
    echo "   ESP32ダミー: 🟢 正常動作"
    process_success_count=$((process_success_count + 1))
else
    echo "   ESP32ダミー: 🔴 動作停止"
fi

if [ -n "${RASPI_PID:-}" ] && kill -0 "$RASPI_PID" 2>/dev/null; then
    echo "   raspiダミー: 🟢 正常動作 (WebUI: http://localhost:$WEB_PORT)"
    process_success_count=$((process_success_count + 1))
else
    echo "   raspiダミー: 🔴 動作停止"
fi

echo "   実行成功率: $process_success_count/3 ($(( process_success_count * 100 / 3 ))%)"
echo ""

# ログファイル詳細統計
echo "📁 ログファイル統計:"
total_log_lines=0
log_files_found=0

for logfile in logs/*.log; do
    if [ -f "$logfile" ]; then
        size=$(wc -l < "$logfile" 2>/dev/null || echo "0")
        filesize=$(du -h "$logfile" 2>/dev/null | cut -f1)
        echo "   $(basename "$logfile"): ${size}行 (${filesize})"
        total_log_lines=$((total_log_lines + size))
        log_files_found=$((log_files_found + 1))
    fi
done

if [ $log_files_found -eq 0 ]; then
    echo "   ⚠️ ログファイルが見つかりません"
else
    echo "   合計: $log_files_found ファイル, $total_log_lines 行"
fi
echo ""

# MQTT通信統計（詳細）
if [ -f "logs/mqtt_monitor.log" ] && [ -s "logs/mqtt_monitor.log" ]; then
    echo "📡 MQTT通信統計:"
    
    # Topic別メッセージカウント
    joystick_count=$(grep -c "joystick\|control" logs/mqtt_monitor.log 2>/dev/null || echo "0")
    playback_count=$(grep -c "playback\|video" logs/mqtt_monitor.log 2>/dev/null || echo "0")
    brightness_count=$(grep -c "brightness\|led" logs/mqtt_monitor.log 2>/dev/null || echo "0")
    imu_count=$(grep -c "imu\|quaternion" logs/mqtt_monitor.log 2>/dev/null || echo "0")
    
    total_messages=$((joystick_count + playback_count + brightness_count + imu_count))
    
    echo "   制御メッセージ: $joystick_count"
    echo "   再生制御: $playback_count"
    echo "   LED制御: $brightness_count"
    echo "   IMUデータ: $imu_count"
    echo "   総メッセージ数: $total_messages"
    
    if [ $total_messages -gt 50 ]; then
        echo "   評価: 🟢 活発な通信 (十分なメッセージ交換)"
    elif [ $total_messages -gt 10 ]; then
        echo "   評価: 🟡 基本通信 (最小限の動作確認)"
    elif [ $total_messages -gt 0 ]; then
        echo "   評価: 🟠 限定的通信 (要確認)"
    else
        echo "   評価: 🔴 通信なし (問題あり)"
    fi
else
    echo "📡 MQTT通信統計: ⚠️ ログファイルが空または見つかりません"
fi

echo ""

# 総合評価
echo "🎯 総合評価:"
total_issues=0

if [[ ! "$final_network" == *"IsolationSphere-Direct"* ]]; then
    total_issues=$((total_issues + 1))
fi

if [ $process_success_count -lt 3 ]; then
    total_issues=$((total_issues + 1))
fi

if [ $log_files_found -eq 0 ]; then
    total_issues=$((total_issues + 1))
fi

if [ $total_issues -eq 0 ]; then
    echo "   🟢 優秀: 全項目で期待される動作を確認"
    echo "   → Arduino IDE実装準備完了レベル"
elif [ $total_issues -eq 1 ]; then
    echo "   🟡 良好: 小さな問題はあるが基本動作OK"
    echo "   → 基本機能は動作、細かい調整が必要"
else
    echo "   🔴 要改善: 複数の問題があります"
    echo "   → 基本設定の見直しが必要"
fi

echo ""
success "🎉 統合テスト完了！"
echo ""
echo "📚 次のステップ:"
echo "   1. ログ詳細確認:"
echo "      cat logs/mqtt_monitor.log"
echo "      cat logs/dummy_esp32.log"
echo "      cat logs/dummy_raspi.log"
echo ""
echo "   2. 個別テスト実行:"
echo "      python network_tester.py --target $BROKER_IP"
echo "      python mqtt_monitor.py"
echo "      python dummy_esp32_client.py"
echo ""
echo "   3. Arduino IDE開発準備:"
echo "      - M5Stack Atom-JoyStickデバイス準備"
echo "      - Arduino IDE環境設定"
echo "      - JOYSTICK.md 設計仕様確認"
echo ""
echo "   4. 環境管理:"
echo "      ./scripts/clean.sh           # 環境クリーンアップ"
echo "      ./scripts/setup.sh           # 環境再構築"
echo ""
echo "📖 詳細: README.md, docs/ARDUINO_IDE_INTEGRATION.md を参照"