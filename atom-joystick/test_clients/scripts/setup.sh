#!/bin/bash
# setup.sh - uv環境セットアップ自動化スクリプト
# isolation-sphere Atom-JoyStick テストクライアント環境構築

set -e  # エラー時に終了

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "🚀 isolation-sphere テストクライアント環境セットアップ"
echo "📁 プロジェクト: $PROJECT_DIR"
echo ""

# uvインストール確認
if ! command -v uv &> /dev/null; then
    echo "❌ uvがインストールされていません"
    echo "💡 インストール方法:"
    echo "   curl -LsSf https://astral.sh/uv/install.sh | sh"
    echo "   または"
    echo "   brew install uv"
    exit 1
fi

echo "✅ uv $(uv --version) 確認完了"

# プロジェクトディレクトリに移動
cd "$PROJECT_DIR"

# 既存仮想環境確認・削除確認
if [ -d ".venv" ]; then
    echo "⚠️ 既存の仮想環境 (.venv) が存在します"
    read -p "削除して新しく作成しますか？ [y/N]: " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "🗑️ 既存仮想環境を削除中..."
        rm -rf .venv
    else
        echo "ℹ️ 既存環境を保持します"
    fi
fi

# uv仮想環境作成
if [ ! -d ".venv" ]; then
    echo "🏗️ uv仮想環境作成中..."
    uv venv
    echo "✅ 仮想環境作成完了: $PROJECT_DIR/.venv"
fi

# 仮想環境アクティベート
echo "🔧 仮想環境アクティベート中..."
source .venv/bin/activate

# 依存関係インストール
echo "📦 依存関係インストール中..."
if [ -f "pyproject.toml" ]; then
    echo "   pyproject.tomlから自動インストール..."
    uv sync
else
    echo "   requirements.txtから手動インストール..."
    uv pip install -r requirements.txt
fi

# 開発依存関係インストール（オプション）
read -p "開発用依存関係（pytest, black, mypy等）もインストールしますか？ [y/N]: " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "🛠️ 開発依存関係インストール中..."
    uv pip install -e ".[dev]"
fi

# Pythonファイル実行権限確認
echo "🔍 実行権限確認・設定..."
chmod +x *.py 2>/dev/null || true

# ログディレクトリ作成
echo "📁 ログディレクトリ作成..."
mkdir -p logs

# 設定確認
echo ""
echo "🎯 環境確認・テスト実行..."
echo "Python: $(python --version)"
echo "インストール済みパッケージ:"
uv pip list

# 基本インポートテスト
python -c "
import paho.mqtt.client as mqtt
import requests
import flask  
import psutil
print('✅ 全依存関係のインポート成功')
"

echo ""
echo "🎉 セットアップ完了！"
echo ""
echo "💡 次のステップ:"
echo "   1. 仮想環境アクティベート: source .venv/bin/activate"
echo "   2. テストプログラム実行: python dummy_esp32_client.py"
echo "   3. 全テスト実行: ./scripts/test-all.sh"
echo "   4. 環境クリーンアップ: ./scripts/clean.sh"
echo ""
echo "📚 詳細: README.md を参照してください"