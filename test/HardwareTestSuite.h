#ifndef HARDWARE_TEST_SUITE_H
#define HARDWARE_TEST_SUITE_H

#include <Arduino.h>
#include <vector>
#include "../src/drivers/AudioDriver.h"
#include "../src/drivers/DisplayDriver.h"
#include "../src/drivers/PinDefinitions.h"

// 测试结果结构
struct TestResult {
    String testName;
    bool passed;
    String message;
    uint32_t executionTime;  // 执行时间（毫秒）
    uint32_t memoryBefore;   // 测试前内存（字节）
    uint32_t memoryAfter;    // 测试后内存（字节）

    TestResult(const String& name = "", bool pass = false,
               const String& msg = "", uint32_t time = 0,
               uint32_t memBefore = 0, uint32_t memAfter = 0)
        : testName(name), passed(pass), message(msg),
          executionTime(time), memoryBefore(memBefore),
          memoryAfter(memAfter) {}

    // 获取内存变化
    int32_t getMemoryChange() const {
        return static_cast<int32_t>(memoryAfter) - static_cast<int32_t>(memoryBefore);
    }
};

// 测试套件类
class HardwareTestSuite {
private:
    AudioDriver audioDriver;
    DisplayDriver displayDriver;

    // 测试统计
    uint32_t totalTests;
    uint32_t passedTests;
    uint32_t failedTests;
    std::vector<TestResult> testResults;

    // 当前测试状态
    String currentTestName;
    uint32_t testStartTime;
    uint32_t testStartHeap;
    uint32_t testStartPsram;

    // 内部方法
    void beginTest(const String& testName);
    TestResult endTest(bool passed, const String& message = "");
    void updateMemoryStats();
    void printTestHeader(const String& suiteName);
    void printTestSummary();

    // 内存测量辅助
    static uint32_t getFreeHeap();
    static uint32_t getFreePsram();

public:
    HardwareTestSuite();

    // 基础功能测试
    TestResult testAudioInitialization();
    TestResult testDisplayInitialization();
    TestResult testButtonInput();
    TestResult testSystemInfo();

    // 音频系统测试
    TestResult testMicrophoneRecording();
    TestResult testSpeakerPlayback();
    TestResult testAudioLoopback();
    TestResult testVolumeControl();
    TestResult testAudioQuality();

    // 显示系统测试
    TestResult testDisplayPatterns();
    TestResult testTextRendering();
    TestResult testColorRendering();
    TestResult testBrightnessControl();
    TestResult testRefreshRate();

    // 系统集成测试
    TestResult testAudioDisplayIntegration();
    TestResult testButtonAudioIntegration();
    TestResult testSystemStress();
    TestResult testErrorRecovery();

    // 性能测试
    TestResult testMemoryUsage();
    TestResult testCpuUsage();
    TestResult testResponseTime();

    // 运行完整测试套件
    void runAllTests();
    void runAudioTests();
    void runDisplayTests();
    void runIntegrationTests();
    void runPerformanceTests();

    // 工具方法
    const std::vector<TestResult>& getResults() const { return testResults; }
    uint32_t getTotalTests() const { return totalTests; }
    uint32_t getPassedTests() const { return passedTests; }
    uint32_t getFailedTests() const { return failedTests; }
    float getPassRate() const {
        return totalTests > 0 ? (static_cast<float>(passedTests) / totalTests * 100.0f) : 0.0f;
    }

    // 静态工具方法
    static void printSystemInfo();
    static void printPinConfiguration();
    static void waitForButtonPress(const String& prompt = "Press BOOT button to continue...");
};

#endif // HARDWARE_TEST_SUITE_H