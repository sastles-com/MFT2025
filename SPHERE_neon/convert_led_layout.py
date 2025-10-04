#!/usr/bin/env python3
"""
LED layout CSV to C header file converter
CSVファイルを読み込んでC言語のヘッダーファイル形式に変換します
"""

import csv
import os

def convert_csv_to_header(csv_file_path, output_file_path=None):
    """
    CSVファイルを読み込んでC言語のヘッダーファイル形式に変換
    
    Args:
        csv_file_path: 入力CSVファイルのパス
        output_file_path: 出力ファイルのパス（省略時は標準出力）
    """
    
    # CSVファイルの存在確認
    if not os.path.exists(csv_file_path):
        print(f"Error: CSVファイルが見つかりません: {csv_file_path}")
        return
    
    layout_data = []
    
    try:
        with open(csv_file_path, 'r', encoding='utf-8') as csvfile:
            reader = csv.DictReader(csvfile)
            
            # ヘッダーの確認
            expected_headers = ['x', 'y', 'z', 'FaceID', 'strip', 'strip_num']
            if not all(header in reader.fieldnames for header in expected_headers):
                print(f"Warning: CSVヘッダーが期待された形式と異なります")
                print(f"期待: {expected_headers}")
                print(f"実際: {reader.fieldnames}")
            
            for row_num, row in enumerate(reader, start=1):
                try:
                    # データの取得（浮動小数点座標をそのまま使用）
                    x = float(row.get('x', 0))
                    y = float(row.get('y', 0))
                    z = float(row.get('z', 0))
                    
                    face_id = int(float(row.get('FaceID', 0)))
                    strip = int(float(row.get('strip', 0)))
                    strip_num = int(float(row.get('strip_num', 0)))
                    
                    layout_data.append((x, y, z, face_id, strip, strip_num))
                    
                except (ValueError, TypeError) as e:
                    print(f"Warning: 行 {row_num} でデータ変換エラー: {e}")
                    print(f"  データ: {row}")
                    continue
    
    except FileNotFoundError:
        print(f"Error: ファイルが見つかりません: {csv_file_path}")
        return
    except Exception as e:
        print(f"Error: CSVファイル読み込みエラー: {e}")
        return
    
    if not layout_data:
        print("Error: 有効なデータが見つかりませんでした")
        return
    
    # C言語ヘッダーファイル形式で出力
    total_pixels = len(layout_data)
    
    output_content = f"""#define TOTAL_PIXELS {total_pixels}

float layout[TOTAL_PIXELS][6] = {{
"""
    
    for i, (x, y, z, face_id, strip, strip_num) in enumerate(layout_data):
        if i == len(layout_data) - 1:  # 最後の要素
            output_content += f"\t{{{x:.6f}f, {y:.6f}f, {z:.6f}f, {face_id}, {strip}, {strip_num}}}\n"
        else:
            output_content += f"\t{{{x:.6f}f, {y:.6f}f, {z:.6f}f, {face_id}, {strip}, {strip_num}}},\n"
    
    output_content += "};\n"
    
    # 出力
    if output_file_path:
        try:
            with open(output_file_path, 'w', encoding='utf-8') as f:
                f.write(output_content)
            print(f"出力完了: {output_file_path}")
            print(f"総ピクセル数: {total_pixels}")
        except Exception as e:
            print(f"Error: ファイル書き込みエラー: {e}")
    else:
        print(output_content)
        print(f"# 総ピクセル数: {total_pixels}")

def main():
    """メイン関数"""
    csv_file_path = "/Users/katano/Documents/PlatformIO/Projects/MFT2025/SPHERE_neon/data/led_layout.csv"
    output_file_path = "/Users/katano/Documents/PlatformIO/Projects/MFT2025/SPHERE_neon/include/layout_new.h"
    
    print("=== LED Layout CSV to C Header Converter ===")
    print(f"入力ファイル: {csv_file_path}")
    print(f"出力ファイル: {output_file_path}")
    print()
    
    convert_csv_to_header(csv_file_path, output_file_path)

if __name__ == "__main__":
    main()