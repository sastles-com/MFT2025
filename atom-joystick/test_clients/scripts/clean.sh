#!/bin/bash
# clean.sh - 仮想環境・一時ファイルクリーンアップスクリプト
# isolation-sphere Atom-JoyStick テストクライアント環境清掃

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# カラー出力設定
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

log() {
    echo -e "${BLUE}[$(date '+%H:%M:%S')]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

confirm() {
    echo -e "${YELLOW}[CONFIRM]${NC} $1"
    read -p "続行しますか？ [y/N]: " -n 1 -r
    echo ""
    [[ $REPLY =~ ^[Yy]$ ]]
}

echo -e "${PURPLE}🧹 isolation-sphere テストクライアント環境クリーンアップ${NC}"
echo -e "${PURPLE}================================${NC}"

# プロジェクトディレクトリに移動
cd "$PROJECT_DIR"

# 実行中プロセス確認・終了
log "🔍 実行中テストプロセス確認..."
python_processes=$(pgrep -f "dummy_|mqtt_monitor|network_tester" 2>/dev/null || true)

if [ -n "$python_processes" ]; then
    warning "実行中のテストプロセスが見つかりました:"
    ps -p $python_processes -o pid,comm,args 2>/dev/null || true
    
    if confirm "これらのプロセスを終了しますか？"; then
        log "🛑 テストプロセス終了中..."
        echo "$python_processes" | xargs kill -TERM 2>/dev/null || true
        sleep 2
        
        # 強制終了が必要なプロセスがあるか確認
        still_running=$(pgrep -f "dummy_|mqtt_monitor|network_tester" 2>/dev/null || true)
        if [ -n "$still_running" ]; then
            log "🔨 一部プロセス強制終了..."
            echo "$still_running" | xargs kill -KILL 2>/dev/null || true
        fi
        
        success "テストプロセス終了完了"
    else
        warning "プロセスが実行中のため、一部クリーンアップをスキップします"
    fi
fi

# 仮想環境クリーンアップ
if [ -d ".venv" ]; then
    log "📁 仮想環境ディレクトリ: $(du -sh .venv 2>/dev/null | cut -f1)"
    
    if confirm "uv仮想環境 (.venv) を削除しますか？"; then
        log "🗑️ .venv削除中..."
        rm -rf .venv
        success "仮想環境削除完了"
    else
        warning "仮想環境を保持します"
    fi
fi

# .uvenv (uv仮想環境) クリーンアップ
if [ -d ".uvenv" ]; then
    log "📁 .uvenv ディレクトリ: $(du -sh .uvenv 2>/dev/null | cut -f1)"
    
    if confirm ".uvenv ディレクトリを削除しますか？"; then
        log "🗑️ .uvenv削除中..."
        rm -rf .uvenv
        success ".uvenv削除完了"
    fi
fi

# uv.lock ファイルクリーンアップ
if [ -f "uv.lock" ]; then
    if confirm "uv.lock ファイルを削除しますか？"; then
        log "🗑️ uv.lock削除中..."
        rm -f uv.lock
        success "uv.lock削除完了"
    fi
fi

# ログファイルクリーンアップ
if [ -d "logs" ] && [ "$(ls -A logs 2>/dev/null)" ]; then
    log "📜 ログファイル:"
    ls -lh logs/ 2>/dev/null || true
    
    if confirm "ログファイル (logs/) を削除しますか？"; then
        log "🗑️ ログファイル削除中..."
        rm -rf logs/*
        success "ログファイル削除完了"
    else
        warning "ログファイルを保持します"
    fi
fi

# Python キャッシュクリーンアップ
log "🧹 Pythonキャッシュ削除中..."
find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
find . -name "*.pyc" -delete 2>/dev/null || true
find . -name "*.pyo" -delete 2>/dev/null || true
success "Pythonキャッシュ削除完了"

# 一時ファイル・その他クリーンアップ
log "🧹 一時ファイル削除中..."
find . -name "*.tmp" -delete 2>/dev/null || true
find . -name ".DS_Store" -delete 2>/dev/null || true
find . -name "Thumbs.db" -delete 2>/dev/null || true
success "一時ファイル削除完了"

# 最終状態確認
echo ""
log "📊 クリーンアップ後の状態:"

# ディスクサイズ確認
total_size=$(du -sh . 2>/dev/null | cut -f1)
echo "   プロジェクトサイズ: $total_size"

# 残りファイル確認
echo "   残りファイル:"
ls -la . | head -10

if [ -d "logs" ]; then
    log_count=$(ls logs/ 2>/dev/null | wc -l)
    echo "   ログファイル: ${log_count}個"
fi

echo ""
success "🎉 クリーンアップ完了！"
echo ""
echo "💡 次のステップ:"
echo "   1. 環境再構築: ./scripts/setup.sh"
echo "   2. テスト実行: ./scripts/test-all.sh"
echo "   3. 個別実行: python [client_name].py"
echo ""
echo "📚 詳細: README.md を参照してください"