#!/usr/bin/env python3
"""
FaceID座標のパノラマ画像分析
シリアルログからパノラマ画像の座標分布を可視化
"""

# シリアルログから抽出したFaceID座標データ
faceid_data = [
    {"faceID": 7, "xyz": (0.031, 0.094, -0.995), "uv": (0.255, 0.530), "pixel": (81, 84)},
    {"faceID": 79, "xyz": (0.031, -0.094, -0.995), "uv": (0.255, 0.470), "pixel": (81, 74)},
    {"faceID": 81, "xyz": (-0.023, -0.291, -0.956), "uv": (0.246, 0.406), "pixel": (78, 64)},
    {"faceID": 82, "xyz": (0.014, -0.413, -0.910), "uv": (0.252, 0.364), "pixel": (80, 57)},
    {"faceID": 87, "xyz": (0.027, -0.802, -0.597), "uv": (0.257, 0.204), "pixel": (82, 32)},
    {"faceID": 88, "xyz": (-0.009, -0.718, -0.696), "uv": (0.248, 0.245), "pixel": (79, 38)},
    {"faceID": 89, "xyz": (-0.048, -0.616, -0.786), "uv": (0.240, 0.289), "pixel": (76, 45)},
    {"faceID": 164, "xyz": (-0.023, 0.291, -0.956), "uv": (0.246, 0.594), "pixel": (78, 94)},
    {"faceID": 165, "xyz": (0.014, 0.413, -0.910), "uv": (0.252, 0.636), "pixel": (80, 101)},
    {"faceID": 172, "xyz": (-0.048, 0.616, -0.786), "uv": (0.240, 0.711), "pixel": (76, 113)},
]

def analyze_panorama_distribution():
    """パノラマ画像上の座標分布を分析"""
    
    # データ抽出
    face_ids = [d["faceID"] for d in faceid_data]
    x_coords = [d["xyz"][0] for d in faceid_data]
    y_coords = [d["xyz"][1] for d in faceid_data]
    z_coords = [d["xyz"][2] for d in faceid_data]
    u_coords = [d["uv"][0] for d in faceid_data]
    v_coords = [d["uv"][1] for d in faceid_data]
    pixel_x = [d["pixel"][0] for d in faceid_data]
    pixel_y = [d["pixel"][1] for d in faceid_data]
    
    # 統計情報
    print("=== FaceID座標のパノラマ分布分析 ===")
    print(f"データ数: {len(faceid_data)} 点")
    print(f"X座標範囲: {min(x_coords):.3f} ~ {max(x_coords):.3f}")
    print(f"U座標範囲: {min(u_coords):.3f} ~ {max(u_coords):.3f}")
    print(f"ピクセルX範囲: {min(pixel_x)} ~ {max(pixel_x)} (幅: {max(pixel_x) - min(pixel_x)} pixels)")
    print(f"V座標範囲: {min(v_coords):.3f} ~ {max(v_coords):.3f}")
    print(f"ピクセルY範囲: {min(pixel_y)} ~ {max(pixel_y)} (高さ: {max(pixel_y) - min(pixel_y)} pixels)")
    
    # 縦線パターンの確認
    unique_pixel_x = sorted(set(pixel_x))
    print(f"\n=== 縦線分析 ===")
    print(f"ユニークなX座標: {unique_pixel_x}")
    print(f"縦線の本数: {len(unique_pixel_x)}")
    
    for x in unique_pixel_x:
        y_values = [pixel_y[i] for i, px in enumerate(pixel_x) if px == x]
        face_ids_at_x = [face_ids[i] for i, px in enumerate(pixel_x) if px == x]
        print(f"  X={x}: Y座標 {min(y_values)}~{max(y_values)} (高さ: {max(y_values)-min(y_values)} pixels)")
        print(f"         FaceIDs: {face_ids_at_x}")
    
    # パノラマ画像の特徴
    print(f"\n=== パノラマ画像特徴 ===")
    print(f"画像サイズ: 320x160")
    print(f"U座標0.255付近に集中 -> ピクセルX座標81付近")
    print(f"経度で見ると: {0.255 * 360:.1f}度付近（X軸付近）")
    print(f"V座標は0.2~0.7に分布 -> 緯度-54度~+54度の範囲")
    
    # ASCII アート風の可視化
    print(f"\n=== パノラマ画像イメージ（ASCII） ===")
    print("320x160 panorama image:")
    print("0" + "-" * 78 + "320")
    
    # 簡易的な画像表現
    for row in range(16):  # 160を16行に圧縮
        line = "|"
        for col in range(78):  # 320を78列に圧縮
            # ピクセル位置計算
            actual_x = int(col * 320 / 78)
            actual_y = int(row * 160 / 16)
            
            # 該当する点があるかチェック
            has_point = False
            for px, py in zip(pixel_x, pixel_y):
                if abs(px - actual_x) <= 2 and abs(py - actual_y) <= 8:
                    has_point = True
                    break
            
            # 赤い水平ライン（中央付近）
            center_y = 80
            if abs(actual_y - center_y) <= 16:  # 20%幅
                if has_point:
                    line += "G"  # 緑の点
                else:
                    line += "r"  # 赤いライン
            else:
                if has_point:
                    line += "G"  # 緑の点
                else:
                    line += " "  # 背景
        
        line += "|"
        if row == 0:
            line += " 0"
        elif row == 8:
            line += " 80"
        elif row == 15:
            line += " 160"
        print(line)
    
    print("0" + "-" * 78 + "320")
    print("凡例: G=緑の点(FaceID), r=赤いライン(理論大円), ' '=黒背景")

def main():
    """メイン関数"""
    print("FaceID座標のパノラマ画像分析")
    print("=" * 50)
    
    # 分析実行
    analyze_panorama_distribution()

if __name__ == "__main__":
    main()
faceid_data = [
    {"faceID": 7, "xyz": (0.031, 0.094, -0.995), "uv": (0.255, 0.530), "pixel": (81, 84)},
    {"faceID": 79, "xyz": (0.031, -0.094, -0.995), "uv": (0.255, 0.470), "pixel": (81, 74)},
    {"faceID": 81, "xyz": (-0.023, -0.291, -0.956), "uv": (0.246, 0.406), "pixel": (78, 64)},
    {"faceID": 82, "xyz": (0.014, -0.413, -0.910), "uv": (0.252, 0.364), "pixel": (80, 57)},
    {"faceID": 87, "xyz": (0.027, -0.802, -0.597), "uv": (0.257, 0.204), "pixel": (82, 32)},
    {"faceID": 88, "xyz": (-0.009, -0.718, -0.696), "uv": (0.248, 0.245), "pixel": (79, 38)},
    {"faceID": 89, "xyz": (-0.048, -0.616, -0.786), "uv": (0.240, 0.289), "pixel": (76, 45)},
    {"faceID": 164, "xyz": (-0.023, 0.291, -0.956), "uv": (0.246, 0.594), "pixel": (78, 94)},
    {"faceID": 165, "xyz": (0.014, 0.413, -0.910), "uv": (0.252, 0.636), "pixel": (80, 101)},
    {"faceID": 172, "xyz": (-0.048, 0.616, -0.786), "uv": (0.240, 0.711), "pixel": (76, 113)},
]

def analyze_panorama_distribution():
    """パノラマ画像上の座標分布を分析"""
    
    # データ抽出
    face_ids = [d["faceID"] for d in faceid_data]
    x_coords = [d["xyz"][0] for d in faceid_data]
    y_coords = [d["xyz"][1] for d in faceid_data]
    z_coords = [d["xyz"][2] for d in faceid_data]
    u_coords = [d["uv"][0] for d in faceid_data]
    v_coords = [d["uv"][1] for d in faceid_data]
    pixel_x = [d["pixel"][0] for d in faceid_data]
    pixel_y = [d["pixel"][1] for d in faceid_data]
    
    # 統計情報
    print("=== FaceID座標のパノラマ分布分析 ===")
    print(f"データ数: {len(faceid_data)} 点")
    print(f"X座標範囲: {min(x_coords):.3f} ~ {max(x_coords):.3f}")
    print(f"U座標範囲: {min(u_coords):.3f} ~ {max(u_coords):.3f}")
    print(f"ピクセルX範囲: {min(pixel_x)} ~ {max(pixel_x)} (幅: {max(pixel_x) - min(pixel_x)} pixels)")
    print(f"V座標範囲: {min(v_coords):.3f} ~ {max(v_coords):.3f}")
    print(f"ピクセルY範囲: {min(pixel_y)} ~ {max(pixel_y)} (高さ: {max(pixel_y) - min(pixel_y)} pixels)")
    
    # 縦線パターンの確認
    unique_pixel_x = sorted(set(pixel_x))
    print(f"\n縦線分析:")
    print(f"ユニークなX座標: {unique_pixel_x}")
    print(f"縦線の本数: {len(unique_pixel_x)}")
    
    for x in unique_pixel_x:
        y_values = [pixel_y[i] for i, px in enumerate(pixel_x) if px == x]
        print(f"  X={x}: Y座標 {min(y_values)}~{max(y_values)} (高さ: {max(y_values)-min(y_values)} pixels)")
    
    return {
        'u_coords': u_coords,
        'v_coords': v_coords,
        'pixel_x': pixel_x,
        'pixel_y': pixel_y,
        'face_ids': face_ids
    }

def main():
    """メイン関数"""
    print("FaceID座標のパノラマ画像分析")
    print("=" * 50)
    
    # 分析実行
    analyze_panorama_distribution()

if __name__ == "__main__":
    main()