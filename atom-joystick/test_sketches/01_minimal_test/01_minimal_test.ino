/**
 * @file 01_minimal_test.ino
 * @brief M5Stack Atom-JoyStick最小動作確認テスト
 * @description Arduino CLI環境での基本動作確認
 * 
 * Phase 1: 環境動作確認
 * - ESP32-S3基本動作
 * - シリアル通信確認
 * - 基本GPIO動作
 * 
 * @target M5Stack Atom-JoyStick (ESP32-S3)
 * @board esp32:esp32:esp32s3_family
 */

void setup() {
    // シリアル通信初期化（115200bps）
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=================================");
    Serial.println("M5Stack Atom-JoyStick 最小テスト");
    Serial.println("=================================");
    Serial.println("Phase 1: Arduino CLI環境確認");
    
    // ESP32-S3基本情報表示
    Serial.printf("チップモデル: %s\n", ESP.getChipModel());
    Serial.printf("チップリビジョン: %d\n", ESP.getChipRevision());
    Serial.printf("CPUコア数: %d\n", ESP.getChipCores());
    Serial.printf("CPU周波数: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("フラッシュサイズ: %d KB\n", ESP.getFlashChipSize() / 1024);
    Serial.printf("フリーヒープ: %d KB\n", ESP.getFreeHeap() / 1024);
    
    Serial.println("\n✅ 基本動作確認完了");
    Serial.println("✅ Arduino CLI環境正常");
    Serial.println("✅ シリアル通信確立");
    Serial.println("\n次のテスト: 02_hardware_test.ino");
}

void loop() {
    // ハートビート表示（生存確認）
    static unsigned long last_heartbeat = 0;
    static int heartbeat_count = 0;
    
    if (millis() - last_heartbeat >= 5000) {  // 5秒間隔
        heartbeat_count++;
        Serial.printf("[%02d] ❤️ システム稼働中 (Uptime: %lu秒)\n", 
                      heartbeat_count, millis() / 1000);
        last_heartbeat = millis();
    }
    
    delay(100);
}