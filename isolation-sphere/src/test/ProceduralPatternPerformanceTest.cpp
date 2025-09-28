/**
 * @file ProceduralPatternPerformanceTest.cpp
 * @brief ProceduralPatternパフォーマンステスト実装
 */

#include "test/ProceduralPatternPerformanceTest.h"
#include <cmath>
#include <algorithm>

namespace PerformanceTest {

// ========== ProceduralPatternPerformanceTester実装 ==========

ProceduralPatternPerformanceTester::ProceduralPatternPerformanceTester() 
    : sphereManager_(nullptr), isInitialized_(false),
      startTime_(0), frameCount_(0), lastFrameTime_(0),
      minFrameTime_(999999.0f), maxFrameTime_(0.0f),
      testDurationMs_(10000), enableSerialOutput_(true), enableDisplay_(true) {
}

ProceduralPatternPerformanceTester::~ProceduralPatternPerformanceTester() {
    // デストラクタ：特に処理なし
}

bool ProceduralPatternPerformanceTester::initialize(LEDSphere::LEDSphereManager* sphereManager) {
    if (!sphereManager) {
        Serial.println("[PerfTest] Error: sphereManager is null");
        return false;
    }
    
    sphereManager_ = sphereManager;
    isInitialized_ = true;
    
    Serial.println("[PerfTest] Performance tester initialized");
    return true;
}

void ProceduralPatternPerformanceTester::setTestConfig(uint32_t durationMs, bool enableSerial, bool enableDisplay) {
    testDurationMs_ = durationMs;
    enableSerialOutput_ = enableSerial;
    enableDisplay_ = enableDisplay;
    
    if (enableSerialOutput_) {
        Serial.printf("[PerfTest] Test config: %dms duration, Serial:%s, Display:%s\n", 
                     durationMs, enableSerial ? "ON" : "OFF", enableDisplay ? "ON" : "OFF");
    }
}

FrameRateResult ProceduralPatternPerformanceTester::testLatitudeRingPattern() {
    if (!isInitialized_) {
        Serial.println("[PerfTest] Error: Not initialized");
        return FrameRateResult();
    }
    
    ProceduralPattern::LatitudeRingPattern pattern;
    pattern.setSphereManager(sphereManager_);
    
    return testPattern(&pattern, "LatitudeRing");
}

FrameRateResult ProceduralPatternPerformanceTester::testLongitudeLinePattern() {
    if (!isInitialized_) {
        Serial.println("[PerfTest] Error: Not initialized");
        return FrameRateResult();
    }
    
    ProceduralPattern::LongitudeLinePattern pattern;
    pattern.setSphereManager(sphereManager_);
    
    return testPattern(&pattern, "LongitudeLine");
}

FrameRateResult ProceduralPatternPerformanceTester::testCombinedPatterns() {
    if (!isInitialized_) {
        Serial.println("[PerfTest] Error: Not initialized");
        return FrameRateResult();
    }
    
    ProceduralPattern::LatitudeRingPattern latPattern;
    ProceduralPattern::LongitudeLinePattern lonPattern;
    latPattern.setSphereManager(sphereManager_);
    lonPattern.setSphereManager(sphereManager_);
    
    if (enableSerialOutput_) {
        Serial.println("[PerfTest] Starting combined pattern test...");
    }
    
    startMeasurement();
    
    while (millis() - startTime_ < testDurationMs_) {
        uint32_t currentTime = millis();
        float progress = (float)(currentTime - startTime_) / testDurationMs_;
        
        ProceduralPattern::PatternParams params = generatePatternParams(progress);
        
        // 両パターンを順次実行
        latPattern.render(params);
        lonPattern.render(params);
        
        measureFrame();
        
        if (enableDisplay_ && (frameCount_ % 30 == 0)) {
            float currentFPS = 1000.0f / (currentTime - lastFrameTime_);
            showRealtimeData(currentFPS, progress);
        }
    }
    
    return finishMeasurement();
}

FrameRateResult ProceduralPatternPerformanceTester::testLEDSphereManagerOverhead() {
    if (!isInitialized_) {
        Serial.println("[PerfTest] Error: Not initialized");
        return FrameRateResult();
    }
    
    if (enableSerialOutput_) {
        Serial.println("[PerfTest] Starting LEDSphereManager overhead test...");
    }
    
    startMeasurement();
    
    while (millis() - startTime_ < testDurationMs_) {
        uint32_t currentTime = millis();
        float progress = (float)(currentTime - startTime_) / testDurationMs_;
        
        // LED基盤システムの基本操作のみテスト
        sphereManager_->clearAllLEDs();
        
        // 簡単なLED点灯（3点のみ）
        CRGB testColor = CRGB::Red;
        sphereManager_->setLED(0, testColor);
        sphereManager_->setLED(400, testColor);
        sphereManager_->setLED(799, testColor);
        
        sphereManager_->show();
        
        measureFrame();
        
        if (enableDisplay_ && (frameCount_ % 60 == 0)) {
            float currentFPS = 1000.0f / (currentTime - lastFrameTime_);
            showRealtimeData(currentFPS, progress);
        }
    }
    
    return finishMeasurement();
}

FrameRateResult ProceduralPatternPerformanceTester::testPattern(ProceduralPattern::IPattern* pattern, const char* patternName) {
    if (!isInitialized_ || !pattern) {
        Serial.printf("[PerfTest] Error: Invalid state for %s test\n", patternName);
        return FrameRateResult();
    }
    
    if (enableSerialOutput_) {
        Serial.printf("[PerfTest] Starting %s performance test...\n", patternName);
    }
    
    startMeasurement();
    
    while (millis() - startTime_ < testDurationMs_) {
        uint32_t currentTime = millis();
        float progress = (float)(currentTime - startTime_) / testDurationMs_;
        
        ProceduralPattern::PatternParams params = generatePatternParams(progress);
        
        // パターン実行
        pattern->render(params);
        
        measureFrame();
        
        // リアルタイムデータ表示（30フレーム毎）
        if (enableDisplay_ && (frameCount_ % 30 == 0)) {
            float currentFPS = 1000.0f / (currentTime - lastFrameTime_);
            showRealtimeData(currentFPS, progress);
        }
    }
    
    return finishMeasurement();
}

std::map<std::string, FrameRateResult> ProceduralPatternPerformanceTester::testAllPatterns() {
    std::map<std::string, FrameRateResult> results;
    
    if (enableSerialOutput_) {
        Serial.println("[PerfTest] === Starting comprehensive performance test ===");
    }
    
    // 各パターンをテスト
    results["LEDSphereManager_Overhead"] = testLEDSphereManagerOverhead();
    results["LatitudeRingPattern"] = testLatitudeRingPattern();
    results["LongitudeLinePattern"] = testLongitudeLinePattern();
    results["CombinedPatterns"] = testCombinedPatterns();
    
    if (enableSerialOutput_) {
        Serial.println("[PerfTest] === All tests completed ===");
        generatePerformanceReport(results);
    }
    
    return results;
}

void ProceduralPatternPerformanceTester::printResults(const FrameRateResult& result, const char* patternName) {
    Serial.printf("\n=== %s Performance Results ===\n", patternName);
    Serial.printf("Average FPS: %.2f\n", result.averageFPS);
    Serial.printf("Min FPS: %.2f\n", result.minFPS);
    Serial.printf("Max FPS: %.2f\n", result.maxFPS);
    Serial.printf("Frame Time: %.2f ms\n", result.frameTimeMs);
    Serial.printf("Total Frames: %d\n", result.totalFrames);
    Serial.printf("Test Duration: %d ms\n", result.testDurationMs);
    
    // 30fps達成度評価
    float achievement = evaluate30FPSAchievement(result);
    Serial.printf("30fps Achievement: %.1f%%\n", achievement * 100);
    
    if (result.averageFPS >= 30.0f) {
        Serial.println("✅ 30fps TARGET ACHIEVED!");
    } else if (result.averageFPS >= 25.0f) {
        Serial.println("⚠️  Near 30fps - Optimization needed");
    } else {
        Serial.println("❌ Below 25fps - Significant optimization required");
    }
    Serial.println();
}

void ProceduralPatternPerformanceTester::displayResults(const FrameRateResult& result, const char* patternName) {
    if (!enableDisplay_) return;
    
    M5.Display.clear();
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE);
    
    M5.Display.setCursor(0, 0);
    M5.Display.printf("=== %s ===", patternName);
    
    M5.Display.setCursor(0, 20);
    M5.Display.printf("Avg FPS: %.1f", result.averageFPS);
    
    M5.Display.setCursor(0, 40);
    M5.Display.printf("Min/Max: %.1f/%.1f", result.minFPS, result.maxFPS);
    
    M5.Display.setCursor(0, 60);
    M5.Display.printf("Frame: %.1f ms", result.frameTimeMs);
    
    M5.Display.setCursor(0, 80);
    M5.Display.printf("Frames: %d", result.totalFrames);
    
    // 30fps達成度をバーで表示
    float achievement = evaluate30FPSAchievement(result);
    int barWidth = (int)(achievement * 120);
    
    M5.Display.setCursor(0, 100);
    M5.Display.printf("30fps: %.0f%%", achievement * 100);
    
    M5.Display.drawRect(0, 115, 120, 10, TFT_WHITE);
    uint16_t barColor = (achievement >= 1.0f) ? TFT_GREEN : 
                       (achievement >= 0.8f) ? TFT_YELLOW : TFT_RED;
    M5.Display.fillRect(0, 115, barWidth, 10, barColor);
}

void ProceduralPatternPerformanceTester::generatePerformanceReport(const std::map<std::string, FrameRateResult>& results) {
    Serial.println("\n📊 === PERFORMANCE ANALYSIS REPORT ===");
    
    // 最高/最低性能の特定
    float bestFPS = 0.0f;
    float worstFPS = 999999.0f;
    std::string bestPattern, worstPattern;
    
    for (const auto& pair : results) {
        if (pair.second.averageFPS > bestFPS) {
            bestFPS = pair.second.averageFPS;
            bestPattern = pair.first;
        }
        if (pair.second.averageFPS < worstFPS) {
            worstFPS = pair.second.averageFPS;
            worstPattern = pair.first;
        }
    }
    
    Serial.printf("🏆 Best Performance: %s (%.1f fps)\n", bestPattern.c_str(), bestFPS);
    Serial.printf("⚠️  Worst Performance: %s (%.1f fps)\n", worstPattern.c_str(), worstFPS);
    
    // ボトルネック分析
    std::string bottleneckAnalysis = analyzeBottlenecks(results);
    Serial.println(bottleneckAnalysis.c_str());
    
    Serial.println("=================================\n");
}

// ========== 内部測定機能 ==========

void ProceduralPatternPerformanceTester::startMeasurement() {
    startTime_ = millis();
    frameCount_ = 0;
    lastFrameTime_ = startTime_;
    minFrameTime_ = 999999.0f;
    maxFrameTime_ = 0.0f;
}

void ProceduralPatternPerformanceTester::measureFrame() {
    uint32_t currentTime = millis();
    float frameTime = currentTime - lastFrameTime_;
    
    frameCount_++;
    
    if (frameTime > 0) {
        minFrameTime_ = std::min(minFrameTime_, frameTime);
        maxFrameTime_ = std::max(maxFrameTime_, frameTime);
    }
    
    lastFrameTime_ = currentTime;
}

FrameRateResult ProceduralPatternPerformanceTester::finishMeasurement() {
    FrameRateResult result;
    
    uint32_t endTime = millis();
    result.testDurationMs = endTime - startTime_;
    result.totalFrames = frameCount_;
    
    if (result.testDurationMs > 0 && result.totalFrames > 0) {
        result.averageFPS = (float)result.totalFrames * 1000.0f / result.testDurationMs;
        result.frameTimeMs = (float)result.testDurationMs / result.totalFrames;
        
        if (minFrameTime_ < 999999.0f) {
            result.maxFPS = 1000.0f / minFrameTime_;
        }
        if (maxFrameTime_ > 0) {
            result.minFPS = 1000.0f / maxFrameTime_;
        }
    }
    
    return result;
}

ProceduralPattern::PatternParams ProceduralPatternPerformanceTester::generatePatternParams(float progress) {
    ProceduralPattern::PatternParams params;
    params.progress = progress;
    params.time = (millis() - startTime_) / 1000.0f;
    params.screenWidth = 128;
    params.screenHeight = 128;
    params.centerX = 64;
    params.centerY = 64;
    params.radius = 60;
    params.speed = 1.0f;
    params.brightness = 1.0f;
    params.enableFlicker = true;
    
    return params;
}

void ProceduralPatternPerformanceTester::showRealtimeData(float currentFPS, float progress) {
    if (!enableDisplay_) return;
    
    M5.Display.fillRect(0, 0, 128, 20, TFT_BLACK);
    M5.Display.setTextColor(TFT_GREEN);
    M5.Display.setCursor(0, 0);
    M5.Display.printf("FPS:%.1f P:%.0f%%", currentFPS, progress * 100);
}

// ========== 汎用関数 ==========

void runQuickPerformanceTest(LEDSphere::LEDSphereManager* sphereManager) {
    ProceduralPatternPerformanceTester tester;
    
    if (!tester.initialize(sphereManager)) {
        Serial.println("[PerfTest] Failed to initialize tester");
        return;
    }
    
    // 短時間テスト（5秒）
    tester.setTestConfig(5000, true, true);
    
    Serial.println("[PerfTest] 🚀 Quick Performance Test Started");
    
    // LatitudeRingPatternの簡易テスト
    auto result = tester.testLatitudeRingPattern();
    tester.printResults(result, "LatitudeRing");
    tester.displayResults(result, "LatitudeRing");
    
    delay(2000); // 結果表示待機
}

float evaluate30FPSAchievement(const FrameRateResult& result) {
    if (result.averageFPS <= 0) return 0.0f;
    
    // 30fps基準で評価
    float achievement = result.averageFPS / 30.0f;
    return std::min(achievement, 1.0f); // 上限1.0
}

std::string analyzeBottlenecks(const std::map<std::string, FrameRateResult>& results) {
    std::string analysis = "\n🔍 BOTTLENECK ANALYSIS:\n";
    
    auto overheadIt = results.find("LEDSphereManager_Overhead");
    auto latitudeIt = results.find("LatitudeRingPattern");
    auto longitudeIt = results.find("LongitudeLinePattern");
    
    if (overheadIt != results.end()) {
        if (overheadIt->second.averageFPS < 100.0f) {
            analysis += "⚠️  LED基盤システム自体が重い (< 100fps)\n";
        } else {
            analysis += "✅ LED基盤システムは軽量 (>= 100fps)\n";
        }
    }
    
    if (latitudeIt != results.end() && longitudeIt != results.end()) {
        float latFPS = latitudeIt->second.averageFPS;
        float lonFPS = longitudeIt->second.averageFPS;
        
        if (latFPS < 30.0f || lonFPS < 30.0f) {
            analysis += "❌ 単体パターンが30fps未達成\n";
            if (latFPS < lonFPS) {
                analysis += "   -> LatitudeRingがより重い\n";
            } else {
                analysis += "   -> LongitudeLineがより重い\n";
            }
        } else {
            analysis += "✅ 単体パターンは30fps達成\n";
        }
    }
    
    return analysis;
}

} // namespace PerformanceTest