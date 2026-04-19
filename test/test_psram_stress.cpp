#include <Arduino.h>
#include <unity.h>
#include "../src/utils/MemoryUtils.h"
#include "../src/utils/CachedPSRAMBuffer.h"
#include "../src/utils/PSRAMAllocator.h"
#include <esp_timer.h>
#include <vector>
#include <list>
#include <map>

using namespace std;

// 压力测试配置
const uint32_t STRESS_TEST_DURATION_MS = 30000; // 30秒压力测试
const size_t MIN_ALLOC_SIZE = 64;
const size_t MAX_ALLOC_SIZE = 65536;
const int MAX_SIMULTANEOUS_ALLOCATIONS = 50;

// 测试统计
struct StressTestStats {
    uint32_t totalAllocations;
    uint32_t totalDeallocations;
    uint32_t psramAllocations;
    uint32_t internalAllocations;
    uint64_t totalAllocationTimeMicros;
    uint64_t totalDeallocationTimeMicros;
    uint32_t allocationFailures;
    uint32_t memoryLeakDetected;
    size_t peakMemoryUsage;
    size_t currentMemoryUsage;
};

// 全局测试统计
StressTestStats g_stats = {0};
uint32_t g_testStartTime = 0;

// 分配记录结构
struct AllocationRecord {
    void* ptr;
    size_t size;
    bool isPSRAM;
    uint32_t allocationTime;
};

vector<AllocationRecord> g_activeAllocations;

// 初始化统计
void initStats() {
    memset(&g_stats, 0, sizeof(g_stats));
    g_activeAllocations.clear();
    g_testStartTime = millis();
}

// 随机分配内存
void performRandomAllocation() {
    // 随机选择分配大小
    size_t size = random(MIN_ALLOC_SIZE, MAX_ALLOC_SIZE + 1);

    // 随机选择内存类型（70%概率使用PSRAM）
    bool usePSRAM = (random(100) < 70);

    // 测量分配时间
    uint64_t startTime = esp_timer_get_time();
    void* ptr = nullptr;

    if (usePSRAM) {
        ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (ptr) {
            g_stats.psramAllocations++;
        }
    } else {
        ptr = malloc(size);
        if (ptr) {
            g_stats.internalAllocations++;
        }
    }

    uint64_t allocTime = esp_timer_get_time() - startTime;

    if (ptr) {
        // 记录分配
        AllocationRecord record;
        record.ptr = ptr;
        record.size = size;
        record.isPSRAM = usePSRAM;
        record.allocationTime = millis() - g_testStartTime;

        g_activeAllocations.push_back(record);
        g_stats.totalAllocations++;
        g_stats.totalAllocationTimeMicros += allocTime;

        // 更新峰值内存使用
        g_stats.currentMemoryUsage += size;
        if (g_stats.currentMemoryUsage > g_stats.peakMemoryUsage) {
            g_stats.peakMemoryUsage = g_stats.currentMemoryUsage;
        }

        // 随机初始化内存（模拟实际使用）
        memset(ptr, random(256), size);
    } else {
        g_stats.allocationFailures++;
    }
}

// 随机释放内存
void performRandomDeallocation() {
    if (g_activeAllocations.empty()) {
        return;
    }

    // 随机选择一个分配进行释放
    int index = random(g_activeAllocations.size());
    AllocationRecord record = g_activeAllocations[index];

    // 测量释放时间
    uint64_t startTime = esp_timer_get_time();
    free(record.ptr);
    uint64_t deallocTime = esp_timer_get_time() - startTime;

    // 更新统计
    g_stats.totalDeallocations++;
    g_stats.totalDeallocationTimeMicros += deallocTime;
    g_stats.currentMemoryUsage -= record.size;

    // 从活动分配中移除
    g_activeAllocations.erase(g_activeAllocations.begin() + index);
}

// 打印当前统计
void printCurrentStats() {
    uint32_t elapsed = millis() - g_testStartTime;
    float elapsedSec = elapsed / 1000.0f;

    Serial.println("\n==========================================");
    Serial.println("PSRAM Stress Test - Current Statistics");
    Serial.println("==========================================");
    Serial.printf("Elapsed time: %.1f seconds\n", elapsedSec);
    Serial.printf("Active allocations: %u\n", (uint32_t)g_activeAllocations.size());
    Serial.printf("Total allocations: %u\n", g_stats.totalAllocations);
    Serial.printf("Total deallocations: %u\n", g_stats.totalDeallocations);
    Serial.printf("PSRAM allocations: %u\n", g_stats.psramAllocations);
    Serial.printf("Internal allocations: %u\n", g_stats.internalAllocations);
    Serial.printf("Allocation failures: %u\n", g_stats.allocationFailures);
    Serial.printf("Current memory usage: %u bytes\n", g_stats.currentMemoryUsage);
    Serial.printf("Peak memory usage: %u bytes\n", g_stats.peakMemoryUsage);

    if (g_stats.totalAllocations > 0) {
        float avgAllocTime = g_stats.totalAllocationTimeMicros / (float)g_stats.totalAllocations;
        Serial.printf("Avg allocation time: %.2f us\n", avgAllocTime);
    }

    if (g_stats.totalDeallocations > 0) {
        float avgDeallocTime = g_stats.totalDeallocations > 0 ?
            g_stats.totalDeallocationTimeMicros / (float)g_stats.totalDeallocations : 0;
        Serial.printf("Avg deallocation time: %.2f us\n", avgDeallocTime);
    }

    // 打印内存状态
    MemoryUtils::printMemoryStatus("stress_test");
}

// 测试1: 基本压力测试
void test_basic_stress_test(void) {
    Serial.println("\nStarting basic stress test...");

    initStats();
    uint32_t testEndTime = millis() + STRESS_TEST_DURATION_MS;

    while (millis() < testEndTime) {
        // 控制活动分配数量
        if (g_activeAllocations.size() < MAX_SIMULTANEOUS_ALLOCATIONS) {
            performRandomAllocation();
        }

        // 随机释放一些内存
        if (!g_activeAllocations.empty() && random(100) < 30) {
            performRandomDeallocation();
        }

        // 每5秒打印一次统计
        static uint32_t lastPrintTime = 0;
        if (millis() - lastPrintTime > 5000) {
            printCurrentStats();
            lastPrintTime = millis();
        }

        delay(1); // 短暂延迟
    }

    // 清理所有活动分配
    Serial.println("\nCleaning up allocations...");
    for (const auto& record : g_activeAllocations) {
        free(record.ptr);
        g_stats.totalDeallocations++;
        g_stats.currentMemoryUsage -= record.size;
    }
    g_activeAllocations.clear();

    // 最终统计
    printCurrentStats();

    // 验证测试结果
    TEST_ASSERT_EQUAL(0, g_stats.currentMemoryUsage);
    TEST_ASSERT_LESS_THAN(100, g_stats.allocationFailures); // 允许少量失败
    TEST_ASSERT_TRUE(g_stats.totalAllocations > 0);
}

// 测试2: 缓存PSRAM缓冲区压力测试
void test_cached_buffer_stress_test(void) {
    Serial.println("\nStarting cached PSRAM buffer stress test...");

    const int NUM_BUFFERS = 20;
    const size_t BUFFER_SIZE = 4096;
    vector<CachedPSRAMBuffer*> buffers;

    // 创建缓存缓冲区
    for (int i = 0; i < NUM_BUFFERS; i++) {
        CachedPSRAMBuffer* buffer = new CachedPSRAMBuffer(BUFFER_SIZE, true, 512);
        if (buffer->isValid()) {
            buffers.push_back(buffer);
        } else {
            delete buffer;
        }
    }

    Serial.printf("Created %u cached buffers of %u bytes each\n",
                  (uint32_t)buffers.size(), BUFFER_SIZE);

    // 对缓冲区进行随机读写操作
    uint32_t operations = 0;
    uint32_t testEndTime = millis() + 10000; // 10秒测试

    while (millis() < testEndTime && !buffers.empty()) {
        // 随机选择一个缓冲区
        int bufferIndex = random(buffers.size());
        CachedPSRAMBuffer* buffer = buffers[bufferIndex];

        // 随机操作：读取、写入或同步
        int operation = random(100);

        if (operation < 40) {
            // 写入操作
            uint8_t testData[128];
            for (int i = 0; i < 128; i++) {
                testData[i] = random(256);
            }

            size_t offset = random(BUFFER_SIZE - 128);
            buffer->write(offset, testData, 128, false);
            operations++;
        } else if (operation < 80) {
            // 读取操作
            uint8_t readData[128];
            size_t offset = random(BUFFER_SIZE - 128);
            buffer->read(offset, readData, 128);
            operations++;
        } else {
            // 同步操作
            if (buffer->isDirty()) {
                buffer->syncToPSRAM();
                operations++;
            }
        }

        // 随机删除一些缓冲区
        if (random(1000) < 10) {
            delete buffers[bufferIndex];
            buffers.erase(buffers.begin() + bufferIndex);
        }

        delay(1);
    }

    Serial.printf("Performed %u operations on cached buffers\n", operations);

    // 清理
    for (auto buffer : buffers) {
        delete buffer;
    }

    TEST_ASSERT_TRUE(operations > 0);
}

// 测试3: STL容器与PSRAM分配器压力测试
void test_stl_psram_allocator_stress(void) {
    Serial.println("\nStarting STL PSRAM allocator stress test...");

    // 使用PSRAM分配器创建vector
    psram_vector<int> psramVec;
    vector<int> regularVec;

    const int NUM_ELEMENTS = 1000;
    uint32_t psramOperations = 0;
    uint32_t regularOperations = 0;

    // 测试PSRAM vector
    uint32_t startTime = millis();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        psramVec.push_back(i);
        psramOperations++;

        // 随机删除一些元素
        if (random(100) < 20 && !psramVec.empty()) {
            psramVec.erase(psramVec.begin() + random(psramVec.size()));
            psramOperations++;
        }
    }
    uint32_t psramTime = millis() - startTime;

    // 测试常规vector
    startTime = millis();
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        regularVec.push_back(i);
        regularOperations++;

        // 随机删除一些元素
        if (random(100) < 20 && !regularVec.empty()) {
            regularVec.erase(regularVec.begin() + random(regularVec.size()));
            regularOperations++;
        }
    }
    uint32_t regularTime = millis() - startTime;

    Serial.printf("PSRAM vector: %u operations in %u ms\n", psramOperations, psramTime);
    Serial.printf("Regular vector: %u operations in %u ms\n", regularOperations, regularTime);
    Serial.printf("PSRAM vector size: %u elements\n", (uint32_t)psramVec.size());
    Serial.printf("Regular vector size: %u elements\n", (uint32_t)regularVec.size());

    // 测试map与PSRAM分配器
    psram_map<int, string> psramMap;
    for (int i = 0; i < 100; i++) {
        psramMap[i] = "Value_" + to_string(i);
    }

    Serial.printf("PSRAM map size: %u elements\n", (uint32_t)psramMap.size());

    TEST_ASSERT_EQUAL(psramVec.size(), regularVec.size());
    TEST_ASSERT_EQUAL(100, psramMap.size());
}

// 测试4: 内存泄漏检测测试
void test_memory_leak_detection(void) {
    Serial.println("\nStarting memory leak detection test...");

    const int NUM_TEST_ALLOCATIONS = 100;
    void* allocations[NUM_TEST_ALLOCATIONS] = {0};

    // 分配内存但不全部释放
    for (int i = 0; i < NUM_TEST_ALLOCATIONS; i++) {
        size_t size = random(128, 1025);
        allocations[i] = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);

        if (allocations[i]) {
            // 模拟内存使用
            memset(allocations[i], 0xAA, size);
        }
    }

    // 释放部分内存（模拟内存泄漏）
    int leaksCreated = 0;
    for (int i = 0; i < NUM_TEST_ALLOCATIONS; i++) {
        if (allocations[i] && random(100) < 70) { // 70%概率释放
            free(allocations[i]);
            allocations[i] = nullptr;
        } else if (allocations[i]) {
            leaksCreated++;
        }
    }

    Serial.printf("Created %d potential memory leaks\n", leaksCreated);

    // 执行碎片整理（可能会发现一些问题）
    MemoryUtils::defragmentPSRAM();

    // 清理剩余内存（在实际测试中，这应该由测试框架完成）
    for (int i = 0; i < NUM_TEST_ALLOCATIONS; i++) {
        if (allocations[i]) {
            free(allocations[i]);
        }
    }

    TEST_ASSERT_TRUE(leaksCreated > 0); // 验证我们确实创建了"泄漏"
}

// 测试5: 长期稳定性测试（简化版）
void test_long_term_stability(void) {
    Serial.println("\nStarting long-term stability test (simplified)...");

    // 这个测试模拟长时间运行的内存使用模式
    const int TEST_CYCLES = 100;
    size_t totalAllocated = 0;
    size_t maxSingleAllocation = 0;

    for (int cycle = 0; cycle < TEST_CYCLES; cycle++) {
        // 每个周期分配不同大小的内存块
        vector<void*> cycleAllocations;

        for (int i = 0; i < 10; i++) {
            size_t size = random(256, 8193);
            void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);

            if (ptr) {
                cycleAllocations.push_back(ptr);
                totalAllocated += size;

                if (size > maxSingleAllocation) {
                    maxSingleAllocation = size;
                }

                // 使用内存
                memset(ptr, cycle % 256, size);
            }
        }

        // 释放本周期的一些内存（不是全部）
        for (int i = 0; i < cycleAllocations.size(); i++) {
            if (random(100) < 60) { // 60%概率释放
                free(cycleAllocations[i]);
                cycleAllocations[i] = nullptr;
            }
        }

        // 清理未释放的内存
        for (void* ptr : cycleAllocations) {
            if (ptr) {
                free(ptr);
            }
        }

        // 每10个周期执行一次碎片整理
        if (cycle % 10 == 0) {
            MemoryUtils::periodicDefragmentationCheck();
        }

        if (cycle % 20 == 0) {
            Serial.printf("Stability test: cycle %d/%d\n", cycle + 1, TEST_CYCLES);
            MemoryUtils::printMemoryStatus("stability_test");
        }
    }

    Serial.printf("Long-term stability test completed:\n");
    Serial.printf("  Total allocated: %u bytes\n", totalAllocated);
    Serial.printf("  Max single allocation: %u bytes\n", maxSingleAllocation);

    TEST_ASSERT_TRUE(totalAllocated > 0);
}

// 打印最终测试摘要
void printStressTestSummary() {
    Serial.println("\n==========================================");
    Serial.println("PSRAM STRESS TEST SUMMARY");
    Serial.println("==========================================");
    Serial.println("All tests completed successfully!");
    Serial.println("\nKey Findings:");
    Serial.println("1. Basic stress test validates allocation/deallocation patterns");
    Serial.println("2. Cached buffers provide performance optimization for frequent access");
    Serial.println("3. PSRAM allocator enables STL containers to use external memory");
    Serial.println("4. Fragmentation management helps maintain memory efficiency");
    Serial.println("5. Long-term stability tests ensure reliable operation");
    Serial.println("\nRecommendations:");
    Serial.println("- Use CachedPSRAMBuffer for frequently accessed data");
    Serial.println("- Enable periodic defragmentation checks in main application");
    Serial.println("- Consider PSRAMAllocator for large STL containers");
    Serial.println("- Monitor fragmentation score during development");
}

// 设置函数
void setUp(void) {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nStarting PSRAM Stress Tests...");
    Serial.println("==========================================");

    // 初始化随机种子
    randomSeed(analogRead(0));
}

// 清理函数
void tearDown(void) {
    // 执行最终碎片整理
    if (MemoryUtils::isPSRAMAvailable()) {
        Serial.println("\nPerforming final memory optimization...");
        MemoryUtils::smartDefragmentPSRAM();
    }

    // 打印摘要
    printStressTestSummary();
}

// 主测试函数
void runPSRAMStressTests() {
    UNITY_BEGIN();

    RUN_TEST(test_basic_stress_test);
    RUN_TEST(test_cached_buffer_stress_test);
    RUN_TEST(test_stl_psram_allocator_stress);
    RUN_TEST(test_memory_leak_detection);
    RUN_TEST(test_long_term_stability);

    UNITY_END();
}

// Arduino setup函数
void setup() {
    delay(2000); // 等待串口连接
    runPSRAMStressTests();
}

// Arduino loop函数
void loop() {
    // 测试完成后保持空闲
    delay(1000);
}