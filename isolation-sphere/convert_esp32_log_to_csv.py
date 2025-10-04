import re
import csv

# ESP32ログファイルのパス
esp32_log_file = "esp32_log.txt"
output_csv_file = "esp32_led_colors.csv"

# ログからデータを抽出
led_data = []
with open(esp32_log_file, "r") as log_file:
    for line in log_file:
        match = re.search(r"LED\[(\d+)\]: xyz\(([-\d.]+),([-\d.]+),([-\d.]+)\) → UV\(([-\d.]+),([-\d.]+)\) → px\((\d+),(\d+)\) → RGB\((\d+),(\d+),(\d+)\)", line)
        if match:
            led_data.append([
                int(match.group(1)),  # LED番号
                float(match.group(2)), float(match.group(3)), float(match.group(4)),  # xyz
                float(match.group(5)), float(match.group(6)),  # UV
                int(match.group(7)), int(match.group(8)),  # px
                int(match.group(9)), int(match.group(10)), int(match.group(11))  # RGB
            ])

# CSVに書き出し
with open(output_csv_file, "w", newline="") as csv_file:
    writer = csv.writer(csv_file)
    writer.writerow(["LED", "x", "y", "z", "u", "v", "px_x", "px_y", "r", "g", "b"])
    writer.writerows(led_data)

print(f"ESP32ログを整形して {output_csv_file} に保存しました。")