#!/usr/bin/env python3
"""
LED Layout Visualizer - CUBE-neon LED配置の3D可視化

Usage:
    python visualize_led_layout.py
"""

import pandas as pd
import plotly.graph_objects as go
import plotly.express as px
from pathlib import Path

def create_led_layout_figure(csv_path):
    """LED座標データから3D散布図を作成"""
    # CSV読み込み
    df = pd.read_csv(csv_path)
    print(f"📊 LED数: {len(df)} 個")
    print(f"📊 ストリップ数: {df['strip'].nunique()} 本")
    
    # ストリップごとに色分け
    fig = go.Figure()
    
    # カラーパレット
    colors = px.colors.qualitative.Set1
    
    for strip_id in sorted(df['strip'].unique()):
        strip_data = df[df['strip'] == strip_id]
        color = colors[strip_id % len(colors)]
        
        fig.add_trace(go.Scatter3d(
            x=strip_data['x'],
            y=strip_data['y'], 
            z=strip_data['z'],
            mode='markers',
            marker=dict(
                size=5,
                color=color,
                opacity=0.8
            ),
            name=f'Strip {strip_id}',
            text=strip_data['FaceID'],
            hovertemplate=
                '<b>FaceID: %{text}</b><br>' +
                'Strip: %{meta}<br>' +
                'X: %{x:.3f}<br>' +
                'Y: %{y:.3f}<br>' +
                'Z: %{z:.3f}<br>' +
                '<extra></extra>',
            meta=strip_id
        ))
    
    # レイアウト設定
    fig.update_layout(
        title={
            'text': 'CUBE-neon LED配置 (3D)',
            'x': 0.5,
            'font': {'size': 16}
        },
        scene=dict(
            xaxis_title='X座標',
            yaxis_title='Y座標', 
            zaxis_title='Z座標',
            bgcolor='rgb(240, 240, 240)',
            camera=dict(
                eye=dict(x=1.5, y=1.5, z=1.5)
            )
        ),
        legend=dict(
            yanchor="top",
            y=0.99,
            xanchor="left", 
            x=0.01
        ),
        width=1000,
        height=800
    )
    
    return fig

def analyze_led_distribution(csv_path):
    """LED分布の統計分析"""
    df = pd.read_csv(csv_path)
    
    print("\n📈 LED分布統計:")
    print(f"  総LED数: {len(df)}")
    print(f"  ストリップ数: {df['strip'].nunique()}")
    
    print("\n📊 ストリップ別LED数:")
    strip_counts = df['strip'].value_counts().sort_index()
    for strip_id, count in strip_counts.items():
        print(f"  Strip {strip_id}: {count} LED")
    
    print("\n📐 座標範囲:")
    for axis in ['x', 'y', 'z']:
        min_val = df[axis].min()
        max_val = df[axis].max()
        print(f"  {axis.upper()}: {min_val:.3f} ～ {max_val:.3f}")
    
    return df

def main():
    # LED座標ファイルのパス
    csv_path = Path("data/led_layout.csv")
    
    if not csv_path.exists():
        print(f"❌ ファイルが見つかりません: {csv_path}")
        print("   dataフォルダにled_layout.csvがあることを確認してください")
        return
    
    # 分析実行
    print("🎯 CUBE-neon LED配置可視化ツール")
    df = analyze_led_distribution(csv_path)
    
    # 3D可視化
    print("\n🎨 3D可視化を作成中...")
    fig = create_led_layout_figure(csv_path)
    
    # HTMLファイルとして保存
    output_path = "led_layout_3d.html"
    fig.write_html(output_path)
    print(f"✅ 3D可視化を保存: {output_path}")
    
    # ブラウザで表示
    try:
        fig.show()
        print("🌐 ブラウザで3D可視化を表示中...")
    except Exception as e:
        print(f"⚠️ ブラウザ表示エラー: {e}")
        print(f"   手動で開いてください: {output_path}")

if __name__ == "__main__":
    main()