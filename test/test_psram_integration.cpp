#include <Arduino.h>
#include <unity.h>
#include "../src/utils/MemoryUtils.h"
#include "../src/utils/CachedPSRAMBuffer.h"
#include "../src/utils/PSRAMAllocator.h"
#include <vector>
#include <map>

using namespace std;

// 集成测试：演示PSRAM性能优化功能的使用

// 测试1: 基本MemoryUtils功能
void test_memory_utils_integration(void) {
    Serial.println("\n=== Testing MemoryUtils Integration ===");

    // 检查PSRAM可用性
    bool psramAvailable = MemoryUtils::isPSRAMAvailable();
    Serial.printf("PSRAM available: %s\n", psramAvailable ? "YES" : "NO");

    if (!psramAvailable) {
        TEST_IGNORE_MESSAGE("PSRAM not available, skipping integration tests");
        return;
    }

    // 打印内存状态
    MemoryUtils::printMemoryStatus("integration_test_start");

    // 测试分配功能
    size_t testSize = 4096;
    void* psramBuffer = MemoryUtils::allocatePSRAM(testSize, "integration_test");
    TEST_ASSERT_NOT_NULL(psramBuffer);

    void* audioBuffer = MemoryUtils::allocateAudioBuffer(testSize);
    TEST_ASSERT_NOT_NULL(audioBuffer);

    void* networkBuffer = MemoryUtils::allocateNetworkBuffer(testSize);
    TEST_ASSERT_NOT_NULL(networkBuffer);

    // 使用内存
    memset(psramBuffer, 0xAA, testSize);
    memset(audioBuffer, 0xBB, testSize);
    memset(networkBuffer, 0xCC, testSize);

    // 检查碎片化评分
    size_t fragScore = MemoryUtils::getFragmentationScore();
    Serial.printf("Fragmentation score: %u/100\n", fragScore);
    TEST_ASSERT_LESS_OR_EQUAL(100, fragScore);

    // 检查是否需要碎片整理
    bool needsDefrag = MemoryUtils::needsDefragmentation();
    Serial.printf("Needs defragmentation: %s\n", needsDefrag ? "YES" : "NO");

    // 执行碎片整理
    MemoryUtils::defragmentPSRAM();

    // 清理内存
    free(psramBuffer);
    free(audioBuffer);
    free(networkBuffer);

    Serial.println("MemoryUtils integration test PASSED");
}

// 测试2: CachedPSRAMBuffer集成
void test_cached_buffer_integration(void) {
    Serial.println("\n=== Testing CachedPSRAMBuffer Integration ===");

    if (!MemoryUtils::isPSRAMAvailable()) {
        TEST_IGNORE_MESSAGE("PSRAM not available, skipping cached buffer test");
        return;
    }

    // 创建缓存缓冲区
    const size_t BUFFER_SIZE = 2048;
    CachedPSRAMBuffer cachedBuffer(BUFFER_SIZE, true, 512);
    TEST_ASSERT_TRUE(cachedBuffer.isValid());
    TEST_ASSERT_TRUE(cachedBuffer.isCacheEnabled());

    // 测试写入数据
    uint8_t writeData[256];
    for (int i = 0; i < 256; i++) {
        writeData[i] = i;
    }

    size_t written = cachedBuffer.write(0, writeData, 256, false);
    TEST_ASSERT_EQUAL(256, written);
    TEST_ASSERT_TRUE(cachedBuffer.isDirty());

    // 测试读取数据
    uint8_t readData[256];
    size_t read = cachedBuffer.read(0, readData, 256);
    TEST_ASSERT_EQUAL(256, read);

    // 验证读取的数据与写入的数据一致
    for (int i = 0; i < 256; i++) {
        TEST_ASSERT_EQUAL(writeData[i], readData[i]);
    }

    // 测试同步到PSRAM
    bool syncResult = cachedBuffer.syncToPSRAM();
    TEST_ASSERT_TRUE(syncResult);
    TEST_ASSERT_FALSE(cachedBuffer.isDirty());

    // 测试刷新从PSRAM
    bool refreshResult = cachedBuffer.refreshFromPSRAM();
    TEST_ASSERT_TRUE(refreshResult);

    // 测试清空缓冲区
    cachedBuffer.clear();
    TEST_ASSERT_TRUE(cachedBuffer.isDirty());

    // 获取内存使用统计
    size_t psramUsed, cacheUsed;
    cachedBuffer.getMemoryUsage(psramUsed, cacheUsed);
    Serial.printf("Cached buffer memory usage: PSRAM=%u bytes, Cache=%u bytes\n",
                  psramUsed, cacheUsed);
    TEST_ASSERT_EQUAL(BUFFER_SIZE, psramUsed);
    TEST_ASSERT_EQUAL(BUFFER_SIZE, cacheUsed);

    Serial.println("CachedPSRAMBuffer integration test PASSED");
}

// 测试3: PSRAMAllocator与STL容器集成
void test_psram_allocator_integration(void) {
    Serial.println("\n=== Testing PSRAMAllocator Integration ===");

    if (!MemoryUtils::isPSRAMAvailable()) {
        TEST_IGNORE_MESSAGE("PSRAM not available, skipping allocator test");
        return;
    }

    // 使用PSRAM分配器创建vector
    psram_vector<int> psramVec;
    TEST_ASSERT_EQUAL(0, psramVec.size());

    // 添加元素
    for (int i = 0; i < 100; i++) {
        psramVec.push_back(i * 2);
    }
    TEST_ASSERT_EQUAL(100, psramVec.size());

    // 验证元素
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT_EQUAL(i * 2, psramVec[i]);
    }

    // 使用PSRAM分配器创建map
    psram_map<string, int> psramMap;
    psramMap["apple"] = 5;
    psramMap["banana"] = 3;
    psramMap["orange"] = 7;

    TEST_ASSERT_EQUAL(3, psramMap.size());
    TEST_ASSERT_EQUAL(5, psramMap["apple"]);
    TEST_ASSERT_EQUAL(3, psramMap["banana"]);
    TEST_ASSERT_EQUAL(7, psramMap["orange"]);

    // 测试内存使用
    size_t freeBefore = PSRAMAllocator<int>::getFreePSRAM();
    Serial.printf("Free PSRAM before vector operations: %u bytes\n", freeBefore);

    // 执行大量操作
    psram_vector<float> largeVector;
    for (int i = 0; i < 500; i++) {
        largeVector.push_back(i * 1.5f);
    }
    TEST_ASSERT_EQUAL(500, largeVector.size());

    size_t freeAfter = PSRAMAllocator<int>::getFreePSRAM();
    Serial.printf("Free PSRAM after vector operations: %u bytes\n", freeAfter);
    TEST_ASSERT_TRUE(freeAfter < freeBefore); // 内存应该被使用

    Serial.println("PSRAMAllocator integration test PASSED");
}

// 测试4: 智能碎片整理集成
void test_smart_defragmentation_integration(void) {
    Serial.println("\n=== Testing Smart Defragmentation Integration ===");

    if (!MemoryUtils::isPSRAMAvailable()) {
        TEST_IGNORE_MESSAGE("PSRAM not available, skipping defrag test");
        return;
    }

    // 创建一些内存碎片
    void* fragments[20];
    for (int i = 0; i < 20; i++) {
        size_t size = 128 * (i + 1);
        fragments[i] = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (fragments[i]) {
            memset(fragments[i], i, size);
        }
    }

    // 释放部分内存创建碎片
    for (int i = 0; i < 20; i += 3) {
        if (fragments[i]) {
            free(fragments[i]);
            fragments[i] = nullptr;
        }
    }

    // 检查碎片化程度
    size_t fragScore = MemoryUtils::getFragmentationScore();
    bool needsDefrag = MemoryUtils::needsDefragmentation();
    Serial.printf("Before defragmentation: score=%u, needs=%s\n",
                  fragScore, needsDefrag ? "YES" : "NO");

    // 执行智能碎片整理
    MemoryUtils::smartDefragmentPSRAM();

    // 检查整理后的状态
    size_t newFragScore = MemoryUtils::getFragmentationScore();
    Serial.printf("After defragmentation: score=%u\n", newFragScore);

    // 清理剩余内存
    for (int i = 0; i < 20; i++) {
        if (fragments[i]) {
            free(fragments[i]);
        }
    }

    // 执行周期性检查
    MemoryUtils::periodicDefragmentationCheck();

    Serial.println("Smart defragmentation integration test PASSED");
}

// 测试5: 实际应用场景模拟
void test_real_world_scenario(void) {
    Serial.println("\n=== Testing Real-World Scenario ===");

    if (!MemoryUtils::isPSRAMAvailable()) {
        TEST_IGNORE_MESSAGE("PSRAM not available, skipping real-world test");
        return;
    }

    Serial.println("Simulating audio processing scenario...");

    // 模拟音频处理：使用缓存缓冲区处理音频数据
    const size_t AUDIO_BUFFER_SIZE = 8192;
    CachedPSRAMBuffer audioBuffer(AUDIO_BUFFER_SIZE, true, 1024);

    // 模拟网络通信：使用PSRAM分配器存储接收到的数据
    psram_vector<uint8_t> networkData;

    // 模拟处理循环
    for (int frame = 0; frame < 10; frame++) {
        // 生成模拟音频数据
        uint8_t audioFrame[512];
        for (int i = 0; i < 512; i++) {
            audioFrame[i] = (frame * 50 + i) % 256;
        }

        // 写入音频缓冲区
        size_t offset = (frame * 512) % AUDIO_BUFFER_SIZE;
        audioBuffer.write(offset, audioFrame, 512, false);

        // 模拟网络数据接收
        for (int i = 0; i < 100; i++) {
            networkData.push_back((frame * 10 + i) % 256);
        }

        // 每3帧同步一次音频数据
        if (frame % 3 == 0 && audioBuffer.isDirty()) {
            audioBuffer.syncToPSRAM();
        }

        // 每5帧检查一次内存碎片
        if (frame % 5 == 0) {
            MemoryUtils::periodicDefragmentationCheck();
        }
    }

    // 最终同步所有数据
    if (audioBuffer.isDirty()) {
        audioBuffer.syncToPSRAM();
    }

    // 验证结果
    TEST_ASSERT_TRUE(audioBuffer.isValid());
    TEST_ASSERT_FALSE(audioBuffer.isDirty());
    TEST_ASSERT_TRUE(networkData.size() > 0);

    Serial.printf("Real-world scenario completed:\n");
    Serial.printf("  Audio buffer size: %u bytes\n", audioBuffer.getSize());
    Serial.printf("  Network data items: %u\n", (uint32_t)networkData.size());

    // 打印最终内存状态
    MemoryUtils::printMemoryStatus("real_world_test_end");

    Serial.println("Real-world scenario test PASSED");
}

// 打印集成测试摘要
void printIntegrationTestSummary() {
    Serial.println("\n==========================================");
    Serial.println("PSRAM PERFORMANCE OPTIMIZATION INTEGRATION TEST");
    Serial.println("==========================================");
    Serial.println("\nImplemented Features:");
    Serial.println("1. ✅ MemoryUtils with enhanced fragmentation management");
    Serial.println("2. ✅ CachedPSRAMBuffer for optimized PSRAM access");
    Serial.println("3. ✅ PSRAMAllocator for STL container support");
    Serial.println("4. ✅ Smart defragmentation strategies");
    Serial.println("5. ✅ Real-world application scenarios");
    Serial.println("\nUsage Examples:");
    Serial.println("- Use MemoryUtils::allocatePSRAM() for large buffers");
    Serial.println("- Use CachedPSRAMBuffer for frequently accessed data");
    Serial.println("- Use psram_vector<T> for large STL containers");
    Serial.println("- Call MemoryUtils::periodicDefragmentationCheck() in main loop");
    Serial.println("- Monitor fragmentation with MemoryUtils::getFragmentationScore()");
}

// 设置函数
void setUp(void) {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nStarting PSRAM Performance Optimization Integration Tests...");
    Serial.println("==========================================================");
}

// 清理函数
void tearDown(void) {
    // 执行最终优化
    if (MemoryUtils::isPSRAMAvailable()) {
        Serial.println("\nPerforming final system optimization...");
        MemoryUtils::smartDefragmentPSRAM();
        MemoryUtils::printMemoryStatus("integration_test_complete");
    }

    // 打印摘要
    printIntegrationTestSummary();
}

// 主测试函数
void runPSRAMIntegrationTests() {
    UNITY_BEGIN();

    RUN_TEST(test_memory_utils_integration);
    RUN_TEST(test_cached_buffer_integration);
    RUN_TEST(test_psram_allocator_integration);
    RUN_TEST(test_smart_defragmentation_integration);
    RUN_TEST(test_real_world_scenario);

    UNITY_END();
}

// Arduino setup函数
void setup() {
    delay(2000); // 等待串口连接
    runPSRAMIntegrationTests();
}

// Arduino loop函数
void loop() {
    // 测试完成后保持空闲
    delay(1000);
}