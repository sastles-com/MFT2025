#!/usr/bin/env python3
"""
UV変換のデバッグスクリプト
LEDの物理座標からUV座標への変換を検証
"""

import csv
import math
import os

def spherical_to_uv(x, y, z):
    """
    C++コードと同じUV変換ロジック（標準版）
    """
    # 正規化
    length = math.sqrt(x*x + y*y + z*z)
    if length == 0:
        return 0.5, 0.5
    
    x /= length
    y /= length  
    z /= length
    
    # 標準的な球面座標変換
    longitude = math.atan2(z, x)  # -π to π
    latitude = math.asin(y)       # -π/2 to π/2
    
    # UV正規化 [0, 1]
    u = (longitude + math.pi) / (2.0 * math.pi)  # 0 to 1
    v = (latitude + math.pi/2.0) / math.pi        # 0 to 1
    
    # 境界クランプ
    u = max(0.0, min(1.0, u))
    v = max(0.0, min(1.0, v))
    
    return u, v

def spherical_to_uv_fast_approximation(x, y, z):
    """
    CUBE_neon高速近似版のUV変換
    """
    # 正規化
    length = math.sqrt(x*x + y*y + z*z)
    if length == 0:
        return 0.5, 0.5
    
    x /= length
    y /= length  
    z /= length
    
    # CUBE_neon近似計算手法
    # 高速近似：atan2の代わりに線形補間を使用
    PI = math.pi
    
    # 近似longitude計算（atan2の代わり）
    if abs(x) > abs(z):
        # |x| > |z|の場合
        if x > 0:
            longitude = math.atan(z / x)
        else:
            longitude = math.atan(z / x) + PI
    else:
        # |z| >= |x|の場合
        if z > 0:
            longitude = PI/2.0 - math.atan(x / z)
        else:
            longitude = -PI/2.0 - math.atan(x / z)
    
    # latitude計算（asinの代わりにより高速な近似を使用）
    # 簡単な近似: asin(y) ≈ y / sqrt(1 - y^2) for |y| < 0.7
    if abs(y) < 0.7:
        latitude = y / math.sqrt(1 - y*y) if abs(y) < 0.999 else math.asin(y)
    else:
        latitude = math.asin(y)  # 境界付近は正確な計算
    
    # UV正規化 [0, 1]
    u = (longitude + PI) / (2.0 * PI)
    v = (latitude + PI/2.0) / PI
    
    # 境界クランプ
    u = max(0.0, min(1.0, u))
    v = max(0.0, min(1.0, v))
    
    return u, v

def analyze_led_coordinates():
    """
    LED座標のUV変換分析（標準版と近似版の比較）
    """
    u_025_count_std = 0  # 標準版 u≈0.25付近
    u_075_count_std = 0  # 標準版 u≈0.75付近
    u_025_count_fast = 0  # 近似版 u≈0.25付近
    u_075_count_fast = 0  # 近似版 u≈0.75付近
    total_count = 0
    
    # CSV出力用のデータリスト（比較版）
    output_data = []
    
    print("LEDのUV変換分析（標準版 vs 近似版比較）:")
    print("FaceID | X座標    | Y座標    | Z座標    | U_std    | V_std    | U_fast   | V_fast   | 差分(U)  | 差分(V)")
    print("-" * 110)
    
    max_u_diff = 0
    max_v_diff = 0
    total_u_diff = 0
    total_v_diff = 0
    
    with open('scripts/led_layout.csv', 'r') as f:
        reader = csv.DictReader(f)
        for i, row in enumerate(reader):
            face_id = int(row['FaceID'])
            x = float(row['x'])
            y = float(row['y'])
            z = float(row['z'])
            
            # 標準版と近似版でUV変換
            u_std, v_std = spherical_to_uv(x, y, z)
            u_fast, v_fast = spherical_to_uv_fast_approximation(x, y, z)
            
            # 差分計算
            u_diff = abs(u_std - u_fast)
            v_diff = abs(v_std - v_fast)
            
            max_u_diff = max(max_u_diff, u_diff)
            max_v_diff = max(max_v_diff, v_diff)
            total_u_diff += u_diff
            total_v_diff += v_diff
            
            # ピクセル位置計算
            px_std = int(u_std * 319)
            py_std = int(v_std * 159)
            px_fast = int(u_fast * 319)
            py_fast = int(v_fast * 159)
            
            # CSV出力用データに追加
            output_data.append({
                'FaceID': face_id,
                'X': x, 'Y': y, 'Z': z,
                'U_std': u_std, 'V_std': v_std,
                'U_fast': u_fast, 'V_fast': v_fast,
                'U_diff': u_diff, 'V_diff': v_diff,
                'PixelX_std': px_std, 'PixelY_std': py_std,
                'PixelX_fast': px_fast, 'PixelY_fast': py_fast,
                'URegion_std': 'u025' if abs(u_std - 0.25) < 0.02 else ('u075' if abs(u_std - 0.75) < 0.02 else 'other'),
                'URegion_fast': 'u025' if abs(u_fast - 0.25) < 0.02 else ('u075' if abs(u_fast - 0.75) < 0.02 else 'other')
            })
            
            # 最初の20個と大きな差分がある場合をログ出力
            if i < 20 or u_diff > 0.01 or v_diff > 0.01:
                print(f"{face_id:6d} | {x:8.3f} | {y:8.3f} | {z:8.3f} | {u_std:8.3f} | {v_std:8.3f} | {u_fast:8.3f} | {v_fast:8.3f} | {u_diff:8.3f} | {v_diff:8.3f}")
            
            # 統計集計
            if abs(u_std - 0.25) < 0.02:
                u_025_count_std += 1
            if abs(u_std - 0.75) < 0.02:
                u_075_count_std += 1
            if abs(u_fast - 0.25) < 0.02:
                u_025_count_fast += 1
            if abs(u_fast - 0.75) < 0.02:
                u_075_count_fast += 1
                
            total_count += 1
    
    # 平均差分計算
    avg_u_diff = total_u_diff / total_count
    avg_v_diff = total_v_diff / total_count
    
    # CSV出力
    output_filename = 'led_uv_comparison.csv'
    with open(output_filename, 'w', newline='', encoding='utf-8') as csvfile:
        fieldnames = ['FaceID', 'X', 'Y', 'Z', 
                     'U_std', 'V_std', 'U_fast', 'V_fast', 'U_diff', 'V_diff',
                     'PixelX_std', 'PixelY_std', 'PixelX_fast', 'PixelY_fast',
                     'URegion_std', 'URegion_fast']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        
        writer.writeheader()
        for data in output_data:
            writer.writerow(data)
    
    print(f"\n✅ CSV出力完了: {output_filename}")
    print(f"📁 出力パス: {os.path.abspath(output_filename)}")
    
    print(f"\n📊 精度比較結果:")
    print(f"最大U差分: {max_u_diff:.6f}")
    print(f"最大V差分: {max_v_diff:.6f}")
    print(f"平均U差分: {avg_u_diff:.6f}")
    print(f"平均V差分: {avg_v_diff:.6f}")
    
    print(f"\n統計比較:")
    print(f"総LED数: {total_count}")
    print(f"【標準版】u≈0.25付近: {u_025_count_std} ({u_025_count_std/total_count*100:.1f}%), u≈0.75付近: {u_075_count_std} ({u_075_count_std/total_count*100:.1f}%)")
    print(f"【近似版】u≈0.25付近: {u_025_count_fast} ({u_025_count_fast/total_count*100:.1f}%), u≈0.75付近: {u_075_count_fast} ({u_075_count_fast/total_count*100:.1f}%)")
    
    # 領域分類の一致率
    region_matches = sum(1 for d in output_data if d['URegion_std'] == d['URegion_fast'])
    print(f"領域分類一致率: {region_matches}/{total_count} ({region_matches/total_count*100:.1f}%)")
    
    return output_data

if __name__ == "__main__":
    data = analyze_led_coordinates()
    
    # 追加分析: X座標の分布確認
    print(f"\n📊 X座標分布分析:")
    x_coords = [d['X'] for d in data]
    x_min, x_max = min(x_coords), max(x_coords)
    print(f"X座標範囲: {x_min:.3f} ～ {x_max:.3f}")
    
    # X座標の区間別カウント
    x_ranges = [
        (-1.0, -0.8, "極西"),
        (-0.8, -0.6, "西"),
        (-0.6, -0.4, "西中"),
        (-0.4, -0.2, "中西"),
        (-0.2, 0.0, "西ゼロ"),
        (0.0, 0.2, "東ゼロ"),
        (0.2, 0.4, "中東"),
        (0.4, 0.6, "東中"),
        (0.6, 0.8, "東"),
        (0.8, 1.0, "極東")
    ]
    
    for x_min_range, x_max_range, label in x_ranges:
        count = sum(1 for x in x_coords if x_min_range <= x < x_max_range)
        print(f"{label}({x_min_range:4.1f}～{x_max_range:4.1f}): {count:3d}個 ({count/len(x_coords)*100:4.1f}%)")
    
    print(f"\n📈 グラフ描画のヒント:")
    print(f"MatplotlibでX座標をプロット: plt.scatter(range(len(data)), [d['X'] for d in data])")
    print(f"CSVファイルでExcel等にインポート可能です")