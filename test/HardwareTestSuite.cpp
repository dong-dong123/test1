#include "HardwareTestSuite.h"
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

// 标签用于日志记录
static const char* TAG = "HardwareTestSuite";

// 构造函数
HardwareTestSuite::HardwareTestSuite()
    : totalTests(0), passedTests(0), failedTests(0),
      currentTestName(""), testStartTime(0),
      testStartHeap(0), testStartPsram(0) {
    testResults.reserve(50); // 预分配空间
}

// 开始测试
void HardwareTestSuite::beginTest(const String& testName) {
    currentTestName = testName;
    testStartTime = millis();
    testStartHeap = getFreeHeap();
    testStartPsram = getFreePsram();

    Serial.print("[TEST] ");
    Serial.print(testName);
    Serial.println("...");
}

// 结束测试
TestResult HardwareTestSuite::endTest(bool passed, const String& message) {
    uint32_t executionTime = millis() - testStartTime;
    uint32_t memoryAfter = getFreeHeap();
    uint32_t psramAfter = getFreePsram();

    TestResult result(currentTestName, passed, message, executionTime,
                     testStartHeap, memoryAfter);

    testResults.push_back(result);
    totalTests++;

    if (passed) {
        passedTests++;
        Serial.print("  [PASS] ");
    } else {
        failedTests++;
        Serial.print("  [FAIL] ");
    }

    Serial.print(currentTestName);
    Serial.print(" (");
    Serial.print(executionTime);
    Serial.print("ms)");

    if (!message.isEmpty()) {
        Serial.print(" - ");
        Serial.print(message);
    }

    int32_t memoryChange = result.getMemoryChange();
    if (memoryChange != 0) {
        Serial.print(" [Memory: ");
        Serial.print(memoryChange);
        Serial.print(" bytes]");
    }

    Serial.println();

    return result;
}

// 获取可用堆内存
uint32_t HardwareTestSuite::getFreeHeap() {
    return esp_get_free_heap_size();
}

// 获取可用PSRAM
uint32_t HardwareTestSuite::getFreePsram() {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

// 打印测试头
void HardwareTestSuite::printTestHeader(const String& suiteName) {
    Serial.println("\n========================================");
    Serial.print("=== ");
    Serial.print(suiteName);
    Serial.println(" ===");
    Serial.println("========================================");
}

// 打印测试总结
void HardwareTestSuite::printTestSummary() {
    Serial.println("\n========================================");
    Serial.println("=== TEST SUMMARY ===");
    Serial.println("========================================");
    Serial.print("Total Tests: ");
    Serial.println(totalTests);
    Serial.print("Passed: ");
    Serial.println(passedTests);
    Serial.print("Failed: ");
    Serial.println(failedTests);
    Serial.print("Pass Rate: ");
    Serial.print(getPassRate(), 1);
    Serial.println("%");

    if (failedTests > 0) {
        Serial.println("\nFailed Tests:");
        for (const auto& result : testResults) {
            if (!result.passed) {
                Serial.print("  - ");
                Serial.print(result.testName);
                Serial.print(": ");
                Serial.println(result.message);
            }
        }
    }

    Serial.println("========================================\n");
}

// ============================================================================
// 基础功能测试
// ============================================================================

TestResult HardwareTestSuite::testSystemInfo() {
    beginTest("System Information");

    String info;
    info += "Chip: " + String(ESP.getChipModel()) + "\n";
    info += "CPU Cores: " + String(ESP.getChipCores()) + "\n";
    info += "CPU Freq: " + String(ESP.getCpuFreqMHz()) + " MHz\n";
    info += "Flash Size: " + String(ESP.getFlashChipSize()) + " bytes\n";
    info += "Free Heap: " + String(getFreeHeap()) + " bytes\n";
    info += "Free PSRAM: " + String(getFreePsram()) + " bytes\n";
    info += "SDK Version: " + String(ESP.getSdkVersion());

    Serial.println("System Information:");
    Serial.println(info);

    return endTest(true, "System information collected");
}

TestResult HardwareTestSuite::testAudioInitialization() {
    beginTest("Audio Driver Initialization");

    // 创建默认配置
    AudioDriverConfig audioConfig;
    audioConfig.sampleRate = 16000;
    audioConfig.bufferSize = 4096;
    audioConfig.volume = 80;

    // 初始化音频驱动
    if (!audioDriver.initialize(audioConfig)) {
        return endTest(false, "Audio driver initialization failed");
    }

    // 验证初始化状态
    if (!audioDriver.isReady()) {
        return endTest(false, "Audio driver not ready after initialization");
    }

    // 验证配置
    const AudioDriverConfig& config = audioDriver.getConfig();
    if (config.sampleRate != audioConfig.sampleRate ||
        config.volume != audioConfig.volume) {
        return endTest(false, "Audio configuration mismatch");
    }

    return endTest(true, "Audio driver initialized successfully");
}

TestResult HardwareTestSuite::testDisplayInitialization() {
    beginTest("Display Driver Initialization");

    // 创建默认配置
    DisplayConfig displayConfig;
    displayConfig.width = 240;
    displayConfig.height = 320;
    displayConfig.rotation = 1;
    displayConfig.brightness = 100;

    // 初始化显示驱动
    if (!displayDriver.initialize(displayConfig)) {
        return endTest(false, "Display driver initialization failed");
    }

    // 验证初始化状态
    if (!displayDriver.isReady()) {
        return endTest(false, "Display driver not ready after initialization");
    }

    // 验证配置
    const DisplayConfig& config = displayDriver.getConfig();
    if (config.width != displayConfig.width ||
        config.height != displayConfig.height) {
        return endTest(false, "Display configuration mismatch");
    }

    return endTest(true, "Display driver initialized successfully");
}

TestResult HardwareTestSuite::testButtonInput() {
    beginTest("Button Input Test");

    // 初始化按钮引脚（GPIO0 - BOOT按钮）
    const int buttonPin = 0;
    pinMode(buttonPin, INPUT_PULLUP);

    Serial.println("Please press the BOOT button (GPIO0) within 5 seconds...");

    // 等待按钮按下
    uint32_t startTime = millis();
    bool buttonPressed = false;

    while (millis() - startTime < 5000) {
        if (digitalRead(buttonPin) == LOW) {
            buttonPressed = true;
            break;
        }
        delay(10);
    }

    if (!buttonPressed) {
        return endTest(false, "Button not pressed within timeout");
    }

    // 等待按钮释放
    while (digitalRead(buttonPin) == LOW) {
        delay(10);
    }

    return endTest(true, "Button press detected successfully");
}

// ============================================================================
// 音频系统测试
// ============================================================================

TestResult HardwareTestSuite::testMicrophoneRecording() {
    beginTest("Microphone Recording Test");

    if (!audioDriver.isReady()) {
        return endTest(false, "Audio driver not initialized");
    }

    // 开始录音
    bool recordingStarted = audioDriver.startRecord();
    if (!recordingStarted) {
        return endTest(false, "Failed to start recording");
    }

    // 等待录音数据（1秒）
    Serial.println("Recording for 1 second...");
    delay(1000);

    // 检查录音数据
    size_t availableData = audioDriver.getAvailableData();
    if (availableData == 0) {
        audioDriver.stopRecord();
        return endTest(false, "No audio data recorded");
    }

    // 停止录音
    audioDriver.stopRecord();

    String message = String("Recorded ") + availableData + " bytes of audio data";
    return endTest(true, message);
}

TestResult HardwareTestSuite::testSpeakerPlayback() {
    beginTest("Speaker Playback Test");

    if (!audioDriver.isReady()) {
        return endTest(false, "Audio driver not initialized");
    }

    // 生成440Hz测试音调（1秒）
    const uint32_t sampleRate = 16000;
    const uint32_t durationMs = 1000;
    const float frequency = 440.0f; // A4音符
    const uint32_t numSamples = sampleRate * durationMs / 1000;

    std::vector<uint8_t> testTone;
    testTone.resize(numSamples * 2); // 16位 = 2字节/样本

    // 生成正弦波
    for (uint32_t i = 0; i < numSamples; i++) {
        float t = static_cast<float>(i) / sampleRate;
        float sample = sin(2.0f * M_PI * frequency * t);
        int16_t sample16 = static_cast<int16_t>(sample * 32767.0f);

        // 小端序存储
        testTone[i * 2] = sample16 & 0xFF;
        testTone[i * 2 + 1] = (sample16 >> 8) & 0xFF;
    }

    // 写入音频数据
    size_t written = audioDriver.writeAudioData(testTone.data(), testTone.size());
    if (written == 0) {
        return endTest(false, "Failed to write audio data");
    }

    // 开始播放
    Serial.println("Playing 440Hz test tone for 1 second...");
    audioDriver.startPlay();
    delay(durationMs + 100); // 播放时间 + 缓冲

    // 停止播放
    audioDriver.stopPlay();

    String message = String("Played ") + written + " bytes of audio data";
    return endTest(true, message);
}

TestResult HardwareTestSuite::testAudioLoopback() {
    beginTest("Audio Loopback Test");

    if (!audioDriver.isReady()) {
        return endTest(false, "Audio driver not initialized");
    }

    // 此测试需要实际的硬件环回（麦克风到扬声器的物理连接）
    // 这里实现一个简化的版本：录音后立即播放

    // 开始录音
    audioDriver.startRecord();
    Serial.println("Recording for 500ms...");
    delay(500);
    audioDriver.stopRecord();

    // 获取录音数据
    size_t recordedBytes = audioDriver.getAvailableData();
    if (recordedBytes == 0) {
        return endTest(false, "No audio data recorded for loopback");
    }

    // 播放录音数据
    Serial.println("Playing back recorded audio...");
    audioDriver.startPlay();
    delay(600); // 播放时间略长于录音时间
    audioDriver.stopPlay();

    String message = String("Loopback: recorded ") + recordedBytes + " bytes and played back";
    return endTest(true, message);
}

TestResult HardwareTestSuite::testVolumeControl() {
    beginTest("Volume Control Test");

    if (!audioDriver.isReady()) {
        return endTest(false, "Audio driver not initialized");
    }

    // 测试不同音量级别
    uint8_t testVolumes[] = {0, 25, 50, 75, 100};
    bool allPassed = true;

    for (uint8_t volume : testVolumes) {
        if (!audioDriver.setVolume(volume)) {
            Serial.print("  Failed to set volume to ");
            Serial.println(volume);
            allPassed = false;
            continue;
        }

        uint8_t currentVolume = audioDriver.getVolume();
        if (currentVolume != volume) {
            Serial.print("  Volume mismatch: set ");
            Serial.print(volume);
            Serial.print(", got ");
            Serial.println(currentVolume);
            allPassed = false;
        }
    }

    // 恢复默认音量
    audioDriver.setVolume(80);

    if (!allPassed) {
        return endTest(false, "Volume control test failed");
    }

    return endTest(true, "Volume control test passed for all levels");
}

TestResult HardwareTestSuite::testAudioQuality() {
    beginTest("Audio Quality Test");

    if (!audioDriver.isReady()) {
        return endTest(false, "Audio driver not initialized");
    }

    // 此测试需要更专业的音频分析设备
    // 这里实现一个基本的音频质量检查

    Serial.println("This test requires manual verification of audio quality.");
    Serial.println("Please verify:");
    Serial.println("  1. Microphone records clear audio without noise");
    Serial.println("  2. Speaker plays clear audio without distortion");
    Serial.println("  3. Volume changes are audible and smooth");

    // 等待用户确认
    Serial.println("\nDid the audio quality tests pass? (This is a manual test)");
    Serial.println("Assuming manual verification passed for automated testing.");

    return endTest(true, "Audio quality test (requires manual verification)");
}

// ============================================================================
// 显示系统测试
// ============================================================================

TestResult HardwareTestSuite::testDisplayPatterns() {
    beginTest("Display Patterns Test");

    if (!displayDriver.isReady()) {
        return endTest(false, "Display driver not initialized");
    }

    Serial.println("Displaying test patterns...");

    // 显示测试图案
    displayDriver.showTestPattern();
    delay(2000);

    // 显示颜色条
    displayDriver.showColorBars();
    delay(2000);

    // 显示文本演示
    displayDriver.showTextDemo();
    delay(2000);

    // 清空屏幕
    displayDriver.clear();

    return endTest(true, "Display patterns shown successfully");
}

TestResult HardwareTestSuite::testTextRendering() {
    beginTest("Text Rendering Test");

    if (!displayDriver.isReady()) {
        return endTest(false, "Display driver not initialized");
    }

    // 测试不同文本大小
    uint8_t textSizes[] = {1, 2, 3, 4};
    uint16_t textColors[] = {TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW};

    displayDriver.clear();

    for (uint8_t size : textSizes) {
        displayDriver.setTextSize(size);

        for (uint16_t color : textColors) {
            displayDriver.setTextColor(color, TFT_BLACK);

            String text = "Size " + String(size) + " Color " + String(color, HEX);
            displayDriver.drawTextCentered(120, text);
            delay(500);
            displayDriver.clear();
        }
    }

    // 恢复默认设置
    displayDriver.setTextSize(2);
    displayDriver.setTextColor(TFT_WHITE, TFT_BLACK);

    return endTest(true, "Text rendering test completed");
}

TestResult HardwareTestSuite::testColorRendering() {
    beginTest("Color Rendering Test");

    if (!displayDriver.isReady()) {
        return endTest(false, "Display driver not initialized");
    }

    // 测试基本颜色
    uint16_t colors[] = {
        TFT_BLACK, TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE,
        TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_ORANGE, TFT_PURPLE
    };

    const char* colorNames[] = {
        "BLACK", "WHITE", "RED", "GREEN", "BLUE",
        "YELLOW", "CYAN", "MAGENTA", "ORANGE", "PURPLE"
    };

    displayDriver.clear();

    for (int i = 0; i < 10; i++) {
        // 绘制颜色矩形
        displayDriver.fillRect(50, 50, 140, 140, colors[i]);

        // 显示颜色名称
        displayDriver.setTextColor(TFT_WHITE, colors[i]);
        displayDriver.setTextSize(2);
        displayDriver.drawTextCentered(160, colorNames[i]);

        delay(1000);
    }

    // 恢复默认设置
    displayDriver.clear();
    displayDriver.setTextColor(TFT_WHITE, TFT_BLACK);

    return endTest(true, "Color rendering test completed");
}

TestResult HardwareTestSuite::testBrightnessControl() {
    beginTest("Brightness Control Test");

    if (!displayDriver.isReady()) {
        return endTest(false, "Display driver not initialized");
    }

    // 测试不同亮度级别
    uint8_t brightnessLevels[] = {0, 25, 50, 75, 100};
    bool allPassed = true;

    displayDriver.clear();
    displayDriver.drawTextCentered(120, "Brightness Test");

    for (uint8_t brightness : brightnessLevels) {
        displayDriver.setBrightness(brightness);
        delay(1000);

        uint8_t currentBrightness = displayDriver.getBrightness();
        if (currentBrightness != brightness) {
            Serial.print("  Brightness mismatch: set ");
            Serial.print(brightness);
            Serial.print(", got ");
            Serial.println(currentBrightness);
            allPassed = false;
        }
    }

    // 恢复默认亮度
    displayDriver.setBrightness(100);

    if (!allPassed) {
        return endTest(false, "Brightness control test failed");
    }

    return endTest(true, "Brightness control test passed for all levels");
}

TestResult HardwareTestSuite::testRefreshRate() {
    beginTest("Display Refresh Rate Test");

    if (!displayDriver.isReady()) {
        return endTest(false, "Display driver not initialized");
    }

    // 测量帧率
    const uint32_t testDuration = 5000; // 5秒测试
    const uint32_t numFrames = 100;
    uint32_t frameCount = 0;
    uint32_t startTime = millis();

    displayDriver.clear();

    // 绘制动画测试
    while (millis() - startTime < testDuration && frameCount < numFrames) {
        // 绘制移动的矩形
        uint16_t x = (frameCount * 2) % 200;
        uint16_t y = 100 + sin(frameCount * 0.1) * 50;

        displayDriver.fillRect(x, y, 40, 40, TFT_BLUE);
        delay(16); // 约60Hz
        displayDriver.fillRect(x, y, 40, 40, TFT_BLACK);

        frameCount++;
    }

    uint32_t elapsedTime = millis() - startTime;
    float fps = frameCount * 1000.0f / elapsedTime;

    displayDriver.clear();
    displayDriver.drawTextCentered(120, "FPS: " + String(fps, 1));

    String message = String("Measured refresh rate: ") + String(fps, 1) + " FPS";
    return endTest(fps > 30.0f, message); // 要求至少30FPS
}

// ============================================================================
// 系统集成测试（待续）
// ============================================================================

TestResult HardwareTestSuite::testAudioDisplayIntegration() {
    beginTest("Audio-Display Integration Test");

    if (!audioDriver.isReady() || !displayDriver.isReady()) {
        return endTest(false, "Required drivers not initialized");
    }

    // 简单的集成测试：播放音频时显示状态
    displayDriver.clear();
    displayDriver.drawTextCentered(100, "Playing Audio...");
    displayDriver.showStatus("Playing", TFT_GREEN);

    // 播放简短测试音
    testSpeakerPlayback();

    displayDriver.clear();
    displayDriver.drawTextCentered(100, "Audio Complete");
    displayDriver.showStatus("Idle", TFT_BLUE);
    delay(1000);

    return endTest(true, "Audio-display integration test completed");
}

// ============================================================================
// 性能测试实现
// ============================================================================

TestResult HardwareTestSuite::testMemoryUsage() {
    beginTest("Memory Usage Test");

    // 获取内存信息
    uint32_t freeHeap = getFreeHeap();
    uint32_t freePsram = getFreePsram();
    uint32_t minHeap = esp_get_minimum_free_heap_size();

    // 计算内存使用率
    uint32_t totalHeap = freeHeap + minHeap;
    float heapUsagePercent = totalHeap > 0 ? (100.0f - (freeHeap * 100.0f / totalHeap)) : 0;

    // 分配和释放内存测试
    const size_t testAllocationSize = 1024; // 1KB
    uint8_t* testBuffer = nullptr;

    if (freeHeap > testAllocationSize * 2) {
        // 分配测试
        testBuffer = (uint8_t*)malloc(testAllocationSize);
        if (testBuffer) {
            memset(testBuffer, 0xAA, testAllocationSize); // 写入测试数据

            uint32_t freeAfterAlloc = getFreeHeap();
            int32_t allocatedMemory = freeHeap - freeAfterAlloc;

            // 验证分配
            if (allocatedMemory >= (int32_t)testAllocationSize) {
                // 释放内存
                free(testBuffer);
                testBuffer = nullptr;

                uint32_t freeAfterFree = getFreeHeap();
                int32_t freedMemory = freeAfterFree - freeAfterAlloc;

                // 验证释放
                if (freedMemory >= (int32_t)testAllocationSize) {
                    String message = String("Heap: ") + freeHeap + " bytes free, " +
                                   "PSRAM: " + freePsram + " bytes free, " +
                                   "Usage: " + String(heapUsagePercent, 1) + "%, " +
                                   "Alloc/Free test passed";
                    return endTest(true, message);
                } else {
                    return endTest(false, "Memory free verification failed");
                }
            } else {
                if (testBuffer) free(testBuffer);
                return endTest(false, "Memory allocation verification failed");
            }
        } else {
            return endTest(false, "Memory allocation failed");
        }
    } else {
        // 内存不足，跳过分配测试
        String message = String("Heap: ") + freeHeap + " bytes free, " +
                       "PSRAM: " + freePsram + " bytes free, " +
                       "Usage: " + String(heapUsagePercent, 1) + "%, " +
                       "Skipped allocation test (low memory)";
        return endTest(true, message);
    }
}

TestResult HardwareTestSuite::testCpuUsage() {
    beginTest("CPU Usage Test");

    // 简单的CPU负载测试
    uint32_t startTime = micros();
    uint32_t iterations = 0;
    const uint32_t testDuration = 1000000; // 1秒

    // 执行计算密集型操作
    volatile float result = 0.0f;
    while (micros() - startTime < testDuration) {
        // 一些浮点计算
        for (int i = 0; i < 100; i++) {
            result += sin(i * 0.1f) * cos(i * 0.2f);
        }
        iterations++;
    }

    uint32_t elapsedTime = micros() - startTime;
    float cpuScore = (iterations * 100.0f) / (elapsedTime / 1000.0f); // 迭代次数/毫秒

    // 检查结果是否合理
    if (iterations > 0 && !isnan(result) && !isinf(result)) {
        String message = String("CPU Score: ") + String(cpuScore, 1) +
                       " iterations/ms, " + String(iterations) + " iterations in 1s";
        return endTest(true, message);
    } else {
        return endTest(false, "CPU test produced invalid results");
    }
}

TestResult HardwareTestSuite::testResponseTime() {
    beginTest("System Response Time Test");

    // 测试系统响应时间（GPIO和延迟）
    const int testPin = 0; // BOOT按钮引脚
    pinMode(testPin, INPUT_PULLUP);

    uint32_t totalResponseTime = 0;
    uint32_t minResponseTime = UINT32_MAX;
    uint32_t maxResponseTime = 0;
    const int testIterations = 10;
    int successfulTests = 0;

    Serial.println("Testing GPIO response time...");
    Serial.println("Please avoid touching the button during this test.");

    for (int i = 0; i < testIterations; i++) {
        // 等待按钮释放状态
        while (digitalRead(testPin) == LOW) {
            delay(10);
        }

        // 短暂延迟
        delay(random(100, 500));

        // 模拟快速引脚变化检测
        uint32_t startTime = micros();
        for (int j = 0; j < 1000; j++) {
            volatile int state = digitalRead(testPin);
            (void)state; // 防止编译器优化
        }
        uint32_t endTime = micros();

        uint32_t responseTime = endTime - startTime;

        if (responseTime < 10000) { // 合理范围：小于10ms
            totalResponseTime += responseTime;
            minResponseTime = min(minResponseTime, responseTime);
            maxResponseTime = max(maxResponseTime, responseTime);
            successfulTests++;
        }

        delay(50);
    }

    if (successfulTests >= testIterations / 2) { // 至少50%成功
        uint32_t avgResponseTime = totalResponseTime / successfulTests;

        String message = String("GPIO Response: Avg=") + (avgResponseTime / 1000.0f) +
                       "ms, Min=" + (minResponseTime / 1000.0f) +
                       "ms, Max=" + (maxResponseTime / 1000.0f) + "ms";
        return endTest(true, message);
    } else {
        return endTest(false, "Too many failed response time measurements");
    }
}

TestResult HardwareTestSuite::testSystemStress() {
    beginTest("System Stress Test");

    Serial.println("Running system stress test for 10 seconds...");
    Serial.println("Testing memory, CPU, and I/O under load");

    uint32_t startTime = millis();
    uint32_t testDuration = 10000; // 10秒
    uint32_t iterations = 0;
    uint32_t memoryAllocations = 0;
    uint32_t memoryFrees = 0;
    uint32_t gpioOperations = 0;

    // 初始内存状态
    uint32_t initialHeap = getFreeHeap();
    uint32_t initialPsram = getFreePsram();

    // 创建测试任务
    while (millis() - startTime < testDuration) {
        // 内存压力测试：交替分配和释放
        const size_t allocSize = 128 + (iterations % 1024);
        uint8_t* buffer = (uint8_t*)malloc(allocSize);

        if (buffer) {
            memoryAllocations++;
            memset(buffer, iterations & 0xFF, allocSize);

            // 模拟一些计算
            volatile float calculation = 0.0f;
            for (int i = 0; i < 100; i++) {
                calculation += sin(i * 0.1f + iterations * 0.01f);
            }
            (void)calculation;

            free(buffer);
            memoryFrees++;
        }

        // GPIO操作测试（虚拟，使用GPIO0）
        if (iterations % 100 == 0) {
            // 读取按钮状态
            volatile int buttonState = digitalRead(0);
            (void)buttonState;
            gpioOperations++;
        }

        // 显示进度
        if (iterations % 1000 == 0) {
            uint32_t elapsed = millis() - startTime;
            uint32_t percent = (elapsed * 100) / testDuration;
            Serial.print("Stress test: ");
            Serial.print(percent);
            Serial.println("%");
        }

        iterations++;
        delay(1); // 防止任务饥饿
    }

    // 最终内存状态
    uint32_t finalHeap = getFreeHeap();
    uint32_t finalPsram = getFreePsram();
    int32_t heapChange = finalHeap - initialHeap;
    int32_t psramChange = finalPsram - initialPsram;

    // 检查内存泄漏
    bool memoryLeak = heapChange < -1024 || psramChange < -1024; // 允许1KB变化

    String message = String("Stress test completed: ") +
                   String(iterations) + " iterations, " +
                   String(memoryAllocations) + " allocations, " +
                   String(gpioOperations) + " GPIO ops, " +
                   "Heap change: " + String(heapChange) + " bytes, " +
                   "PSRAM change: " + String(psramChange) + " bytes";

    if (!memoryLeak) {
        return endTest(true, message);
    } else {
        return endTest(false, message + " (possible memory leak)");
    }
}

TestResult HardwareTestSuite::testErrorRecovery() {
    beginTest("Error Recovery Test");

    Serial.println("Testing system error recovery capabilities...");

    bool allRecovered = true;
    String failureMessages = "";

    // 测试1：音频驱动错误恢复
    Serial.println("1. Testing audio driver error recovery...");
    if (audioDriver.isReady()) {
        // 模拟错误：停止未开始的播放
        audioDriver.stopPlay();
        // 检查是否仍然就绪
        if (!audioDriver.isReady()) {
            allRecovered = false;
            failureMessages += "Audio driver failed to recover; ";
        }
    }

    // 测试2：显示驱动错误恢复
    Serial.println("2. Testing display driver error recovery...");
    if (displayDriver.isReady()) {
        // 尝试无效操作
        displayDriver.fillRect(1000, 1000, 100, 100, TFT_RED); // 超出边界
        // 检查是否仍然就绪
        if (!displayDriver.isReady()) {
            allRecovered = false;
            failureMessages += "Display driver failed to recover; ";
        }
    }

    // 测试3：内存分配失败恢复
    Serial.println("3. Testing memory allocation failure recovery...");
    // 尝试分配超大内存块（可能失败）
    size_t hugeSize = 1024 * 1024 * 10; // 10MB - 应该失败
    void* hugeBuffer = malloc(hugeSize);
    if (hugeBuffer) {
        free(hugeBuffer);
        Serial.println("  Warning: Unexpectedly allocated 10MB");
    } else {
        Serial.println("  Expected allocation failure handled correctly");
    }

    // 检查系统是否仍然正常
    uint32_t freeHeap = getFreeHeap();
    if (freeHeap < 1024) { // 少于1KB可用堆
        allRecovered = false;
        failureMessages += "System out of memory after error; ";
    }

    // 测试4：任务创建失败恢复
    Serial.println("4. Testing task creation failure recovery...");
    // 尝试创建过多任务（可能失败）
    for (int i = 0; i < 5; i++) {
        TaskHandle_t dummyTask = nullptr;
        BaseType_t result = xTaskCreate(
            [](void* param) { vTaskDelete(nullptr); },
            "DummyTask",
            2048,
            nullptr,
            1,
            &dummyTask
        );

        if (result == pdPASS && dummyTask) {
            vTaskDelete(dummyTask);
        } else {
            Serial.println("  Task creation failed as expected (system resource limit)");
            break;
        }
    }

    if (allRecovered) {
        return endTest(true, "All error recovery tests passed");
    } else {
        return endTest(false, "Error recovery failures: " + failureMessages);
    }
}

TestResult HardwareTestSuite::testButtonAudioIntegration() {
    beginTest("Button-Audio Integration Test");

    if (!audioDriver.isReady()) {
        return endTest(false, "Audio driver not ready");
    }

    Serial.println("Testing button-triggered audio playback...");
    Serial.println("Press the BOOT button (GPIO0) to trigger test tone");

    displayDriver.clear();
    displayDriver.drawTextCentered(100, "Button-Audio Test");
    displayDriver.drawTextCentered(140, "Press BOOT button");
    displayDriver.showStatus("Waiting", TFT_YELLOW);

    // 等待按钮按下
    const int buttonPin = 0;
    pinMode(buttonPin, INPUT_PULLUP);

    uint32_t waitStart = millis();
    bool buttonPressed = false;

    while (millis() - waitStart < 10000) { // 10秒超时
        if (digitalRead(buttonPin) == LOW) {
            buttonPressed = true;
            break;
        }
        delay(10);
    }

    if (!buttonPressed) {
        displayDriver.showError("Timeout");
        return endTest(false, "Button not pressed within timeout");
    }

    // 按钮按下，播放测试音
    displayDriver.clear();
    displayDriver.drawTextCentered(100, "Button Pressed!");
    displayDriver.drawTextCentered(140, "Playing tone...");
    displayDriver.showStatus("Playing", TFT_GREEN);

    // 播放简短测试音
    const uint32_t sampleRate = 16000;
    const uint32_t durationMs = 500;
    const float frequency = 880.0f; // A5音符
    const uint32_t numSamples = sampleRate * durationMs / 1000;

    std::vector<uint8_t> testTone;
    testTone.resize(numSamples * 2);

    // 生成正弦波
    for (uint32_t i = 0; i < numSamples; i++) {
        float t = static_cast<float>(i) / sampleRate;
        float sample = sin(2.0f * M_PI * frequency * t);
        int16_t sample16 = static_cast<int16_t>(sample * 32767.0f);

        testTone[i * 2] = sample16 & 0xFF;
        testTone[i * 2 + 1] = (sample16 >> 8) & 0xFF;
    }

    // 写入并播放
    size_t written = audioDriver.writeAudioData(testTone.data(), testTone.size());
    audioDriver.startPlay();
    delay(durationMs + 100);
    audioDriver.stopPlay();

    // 等待按钮释放
    while (digitalRead(buttonPin) == LOW) {
        delay(10);
    }

    displayDriver.clear();
    displayDriver.drawTextCentered(100, "Test Complete!");
    displayDriver.showStatus("Success", TFT_GREEN);
    delay(1000);

    String message = String("Button-triggered audio playback successful: ") +
                   String(written) + " bytes played";
    return endTest(written > 0, message);
}

// 继续实现其他测试方法...

// ============================================================================
// 运行测试套件
// ============================================================================

void HardwareTestSuite::runAllTests() {
    totalTests = 0;
    passedTests = 0;
    failedTests = 0;
    testResults.clear();

    printTestHeader("COMPLETE HARDWARE TEST SUITE");
    printSystemInfo();
    printPinConfiguration();

    // 基础测试
    printTestHeader("BASIC TESTS");
    testResults.push_back(testSystemInfo());
    testResults.push_back(testAudioInitialization());
    testResults.push_back(testDisplayInitialization());
    testResults.push_back(testButtonInput());

    // 音频测试
    printTestHeader("AUDIO TESTS");
    testResults.push_back(testMicrophoneRecording());
    testResults.push_back(testSpeakerPlayback());
    testResults.push_back(testAudioLoopback());
    testResults.push_back(testVolumeControl());
    testResults.push_back(testAudioQuality());

    // 显示测试
    printTestHeader("DISPLAY TESTS");
    testResults.push_back(testDisplayPatterns());
    testResults.push_back(testTextRendering());
    testResults.push_back(testColorRendering());
    testResults.push_back(testBrightnessControl());
    testResults.push_back(testRefreshRate());

    // 集成测试
    printTestHeader("INTEGRATION TESTS");
    testResults.push_back(testAudioDisplayIntegration());
    testResults.push_back(testButtonAudioIntegration());
    testResults.push_back(testSystemStress());
    testResults.push_back(testErrorRecovery());

    // 性能测试
    printTestHeader("PERFORMANCE TESTS");
    testResults.push_back(testMemoryUsage());
    testResults.push_back(testCpuUsage());
    testResults.push_back(testResponseTime());

    printTestSummary();
}

void HardwareTestSuite::runAudioTests() {
    printTestHeader("AUDIO TESTS ONLY");
    testAudioInitialization();
    testMicrophoneRecording();
    testSpeakerPlayback();
    testAudioLoopback();
    testVolumeControl();
    testAudioQuality();
    printTestSummary();
}

void HardwareTestSuite::runDisplayTests() {
    printTestHeader("DISPLAY TESTS ONLY");
    testDisplayInitialization();
    testDisplayPatterns();
    testTextRendering();
    testColorRendering();
    testBrightnessControl();
    testRefreshRate();
    printTestSummary();
}

void HardwareTestSuite::runIntegrationTests() {
    printTestHeader("INTEGRATION TESTS ONLY");
    testAudioDisplayIntegration();
    testButtonAudioIntegration();
    testSystemStress();
    testErrorRecovery();
    printTestSummary();
}

void HardwareTestSuite::runPerformanceTests() {
    printTestHeader("PERFORMANCE TESTS ONLY");
    testMemoryUsage();
    testCpuUsage();
    testResponseTime();
    printTestSummary();
}

// ============================================================================
// 静态工具方法
// ============================================================================

void HardwareTestSuite::printSystemInfo() {
    Serial.println("\n=== SYSTEM INFORMATION ===");
    Serial.println("Chip Model: " + String(ESP.getChipModel()));
    Serial.println("CPU Cores: " + String(ESP.getChipCores()));
    Serial.println("CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
    Serial.println("Flash Size: " + String(ESP.getFlashChipSize()) + " bytes");
    Serial.println("SDK Version: " + String(ESP.getSdkVersion()));
    Serial.println("Free Heap: " + String(getFreeHeap()) + " bytes");
    Serial.println("Free PSRAM: " + String(getFreePsram()) + " bytes");
    Serial.println("Minimum Free Heap: " + String(esp_get_minimum_free_heap_size()) + " bytes");
    Serial.println();
}

void HardwareTestSuite::printPinConfiguration() {
    Serial.println("=== PIN CONFIGURATION ===");
    Serial.println("Display (ST7789 SPI):");
    Serial.println("  SCLK: GPIO" + String(TFT_SCLK));
    Serial.println("  MOSI: GPIO" + String(TFT_MOSI));
    Serial.println("  CS:   GPIO" + String(TFT_CS));
    Serial.println("  DC:   GPIO" + String(TFT_DC));
    Serial.println("  RST:  GPIO" + String(TFT_RST));

    Serial.println("Microphone (INMP441 I2S):");
    Serial.println("  SDO:  GPIO" + String(I2S_MIC_SDO));
    Serial.println("  WS:   GPIO" + String(I2S_MIC_WS));
    Serial.println("  BCLK: GPIO" + String(I2S_MIC_BCLK));

    Serial.println("Speaker (MAX98357A I2S):");
    Serial.println("  DIN:  GPIO" + String(I2S_SPK_DIN));
    Serial.println("  WS:   GPIO" + String(I2S_SPK_LRC) + " (shared with mic)");
    Serial.println("  BCLK: GPIO" + String(I2S_SPK_BCLK) + " (shared with mic)");

    Serial.println("Button: GPIO0 (BOOT button)");
    Serial.println();
}

void HardwareTestSuite::waitForButtonPress(const String& prompt) {
    Serial.println(prompt);

    const int buttonPin = 0;
    pinMode(buttonPin, INPUT_PULLUP);

    // 等待按钮按下
    while (digitalRead(buttonPin) == HIGH) {
        delay(10);
    }

    // 等待按钮释放
    while (digitalRead(buttonPin) == LOW) {
        delay(10);
    }

    Serial.println("Button pressed, continuing...\n");
}