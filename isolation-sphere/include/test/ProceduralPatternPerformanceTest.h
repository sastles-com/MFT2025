/**
 * @file ProceduralPatternPerformanceTest.h
 * @brief ProceduralPatternのパフォーマンステスト
 * 
 * LEDに実際に表示せずにFastLEDに送信してフレームレートを測定
 * 30fps目標の検証とボトルネック特定
 */

#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include "pattern/ProceduralPatternGenerator.h"
#include "led/LEDSphereManager.h"

namespace PerformanceTest {

/**
 * @brief フレームレート測定結果
 */
struct FrameRateResult {
    float averageFPS;      // 平均FPS
    float minFPS;          // 最小FPS
    float maxFPS;          // 最大FPS
    uint32_t totalFrames;  // 総フレーム数
    uint32_t testDurationMs; // テスト時間(ms)
    float frameTimeMs;     // 平均フレーム時間(ms)
    
    FrameRateResult() : 
        averageFPS(0), minFPS(0), maxFPS(0), 
        totalFrames(0), testDurationMs(0), frameTimeMs(0) {}
};

/**
 * @brief ProceduralPatternパフォーマンステスタークラス
 */
class ProceduralPatternPerformanceTester {
private:
    LEDSphere::LEDSphereManager* sphereManager_;
    bool isInitialized_;
    
    // 測定データ
    uint32_t startTime_;
    uint32_t frameCount_;
    uint32_t lastFrameTime_;
    float minFrameTime_;
    float maxFrameTime_;
    
    // テスト設定
    uint32_t testDurationMs_;
    bool enableSerialOutput_;
    bool enableDisplay_;
    
public:
    ProceduralPatternPerformanceTester();
    ~ProceduralPatternPerformanceTester();
    
    // ========== 初期化・設定 ==========
    
    /**
     * @brief テスター初期化
     * @param sphereManager LEDSphereManagerインスタンス
     * @return 初期化成功フラグ
     */
    bool initialize(LEDSphere::LEDSphereManager* sphereManager);
    
    /**
     * @brief テスト設定
     * @param durationMs テスト時間(ms) デフォルト10秒
     * @param enableSerial シリアル出力有効フラグ
     * @param enableDisplay M5.Display表示有効フラグ
     */
    void setTestConfig(uint32_t durationMs = 10000, 
                       bool enableSerial = true, 
                       bool enableDisplay = true);
    
    // ========== パフォーマンステスト ==========
    
    /**
     * @brief LatitudeRingPatternのパフォーマンステスト
     * @return 測定結果
     */
    FrameRateResult testLatitudeRingPattern();
    
    /**
     * @brief LongitudeLinePatternのパフォーマンステスト
     * @return 測定結果
     */
    FrameRateResult testLongitudeLinePattern();
    
    /**
     * @brief 複合パターンのパフォーマンステスト
     * @return 測定結果
     */
    FrameRateResult testCombinedPatterns();
    
    /**
     * @brief LED基盤システムのオーバーヘッドテスト
     * @return 測定結果
     */
    FrameRateResult testLEDSphereManagerOverhead();
    
    // ========== 汎用テスト機能 ==========
    
    /**
     * @brief 任意のパターンをテスト
     * @param pattern テスト対象パターン
     * @param patternName パターン名（ログ用）
     * @return 測定結果
     */
    FrameRateResult testPattern(ProceduralPattern::IPattern* pattern, 
                               const char* patternName);
    
    /**
     * @brief 全パターンの一括テスト
     * @return 各パターンの結果マップ
     */
    std::map<std::string, FrameRateResult> testAllPatterns();
    
    // ========== 結果表示・分析 ==========
    
    /**
     * @brief 測定結果をシリアル出力
     * @param result 測定結果
     * @param patternName パターン名
     */
    void printResults(const FrameRateResult& result, const char* patternName);
    
    /**
     * @brief M5.Displayに結果表示
     * @param result 測定結果
     * @param patternName パターン名
     */
    void displayResults(const FrameRateResult& result, const char* patternName);
    
    /**
     * @brief パフォーマンス分析レポート生成
     * @param results 複数パターンの結果
     */
    void generatePerformanceReport(const std::map<std::string, FrameRateResult>& results);
    
private:
    // ========== 内部測定機能 ==========
    
    /**
     * @brief 測定開始
     */
    void startMeasurement();
    
    /**
     * @brief フレーム測定
     */
    void measureFrame();
    
    /**
     * @brief 測定終了と結果計算
     * @return 測定結果
     */
    FrameRateResult finishMeasurement();
    
    /**
     * @brief パターンパラメータ生成
     * @param progress 進行度 [0.0-1.0]
     * @return パターンパラメータ
     */
    ProceduralPattern::PatternParams generatePatternParams(float progress);
    
    /**
     * @brief リアルタイム測定データ表示
     * @param currentFPS 現在のFPS
     * @param progress 進行度
     */
    void showRealtimeData(float currentFPS, float progress);
};

/**
 * @brief パフォーマンステストの簡易実行関数
 * @param sphereManager LEDSphereManagerインスタンス
 */
void runQuickPerformanceTest(LEDSphere::LEDSphereManager* sphereManager);

/**
 * @brief 30fps目標の達成度評価
 * @param result 測定結果
 * @return 30fps達成度 [0.0-1.0]
 */
float evaluate30FPSAchievement(const FrameRateResult& result);

/**
 * @brief ボトルネック分析
 * @param results 複数パターンの結果
 * @return 分析レポート文字列
 */
std::string analyzeBottlenecks(const std::map<std::string, FrameRateResult>& results);

} // namespace PerformanceTest