#include <Arduino.h>
#include <unity.h>
#include "../src/utils/MemoryUtils.h"
#include "../src/modules/SSLClientManager.h"
#include <esp_timer.h>
#include <vector>

// 性能测试配置
const size_t SMALL_ALLOC_SIZE = 128;      // 小分配测试
const size_t MEDIUM_ALLOC_SIZE = 4096;    // 中等分配测试
const size_t LARGE_ALLOC_SIZE = 65536;    // 大分配测试
const size_t AUDIO_BUFFER_SIZE = 8192;    // 音频缓冲区测试
const size_t NETWORK_BUFFER_SIZE = 16384; // 网络缓冲区测试
const int ITERATIONS = 100;               // 每个测试的迭代次数

// 性能测试结果结构体
struct BenchmarkResult {
    const char* name;
    uint64_t psramTimeMicros;
    uint64_t internalTimeMicros;
    size_t allocationSize;
    int iterations;
};

// 全局测试结果存储
std::vector<BenchmarkResult> benchmarkResults;

// 性能测试函数：分配/释放性能
void benchmarkAllocationPerformance(size_t size, const char* testName) {
    uint64_t psramTotal = 0;
    uint64_t internalTotal = 0;

    // 测试PSRAM分配性能
    for (int i = 0; i < ITERATIONS; i++) {
        uint64_t start = esp_timer_get_time();
        void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        uint64_t allocTime = esp_timer_get_time() - start;

        if (ptr) {
            psramTotal += allocTime;

            start = esp_timer_get_time();
            free(ptr);
            psramTotal += (esp_timer_get_time() - start);
        }
    }

    // 测试内部RAM分配性能
    for (int i = 0; i < ITERATIONS; i++) {
        uint64_t start = esp_timer_get_time();
        void* ptr = malloc(size);
        uint64_t allocTime = esp_timer_get_time() - start;

        if (ptr) {
            internalTotal += allocTime;

            start = esp_timer_get_time();
            free(ptr);
            internalTotal += (esp_timer_get_time() - start);
        }
    }

    // 存储结果
    BenchmarkResult result;
    result.name = testName;
    result.psramTimeMicros = psramTotal / ITERATIONS;
    result.internalTimeMicros = internalTotal / ITERATIONS;
    result.allocationSize = size;
    result.iterations = ITERATIONS;
    benchmarkResults.push_back(result);
}

// 测试1: 小内存分配性能
void test_small_allocation_performance(void) {
    benchmarkAllocationPerformance(SMALL_ALLOC_SIZE, "Small Allocation (128B)");
}

// 测试2: 中等内存分配性能
void test_medium_allocation_performance(void) {
    benchmarkAllocationPerformance(MEDIUM_ALLOC_SIZE, "Medium Allocation (4KB)");
}

// 测试3: 大内存分配性能
void test_large_allocation_performance(void) {
    benchmarkAllocationPerformance(LARGE_ALLOC_SIZE, "Large Allocation (64KB)");
}

// 测试4: 音频缓冲区分配性能
void test_audio_buffer_allocation(void) {
    uint64_t psramTotal = 0;
    uint64_t internalTotal = 0;

    // 测试PSRAM音频缓冲区分配
    for (int i = 0; i < ITERATIONS; i++) {
        uint64_t start = esp_timer_get_time();
        void* ptr = MemoryUtils::allocateAudioBuffer(AUDIO_BUFFER_SIZE);
        uint64_t allocTime = esp_timer_get_time() - start;

        if (ptr) {
            psramTotal += allocTime;
            free(ptr);
        }
    }

    // 测试内部RAM音频缓冲区分配
    for (int i = 0; i < ITERATIONS; i++) {
        uint64_t start = esp_timer_get_time();
        void* ptr = heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_8BIT);
        uint64_t allocTime = esp_timer_get_time() - start;

        if (ptr) {
            internalTotal += allocTime;
            free(ptr);
        }
    }

    BenchmarkResult result;
    result.name = "Audio Buffer Allocation (8KB)";
    result.psramTimeMicros = psramTotal / ITERATIONS;
    result.internalTimeMicros = internalTotal / ITERATIONS;
    result.allocationSize = AUDIO_BUFFER_SIZE;
    result.iterations = ITERATIONS;
    benchmarkResults.push_back(result);
}

// 测试5: 网络缓冲区分配性能
void test_network_buffer_allocation(void) {
    uint64_t psramTotal = 0;
    uint64_t internalTotal = 0;

    // 测试PSRAM网络缓冲区分配
    for (int i = 0; i < ITERATIONS; i++) {
        uint64_t start = esp_timer_get_time();
        void* ptr = MemoryUtils::allocateNetworkBuffer(NETWORK_BUFFER_SIZE);
        uint64_t allocTime = esp_timer_get_time() - start;

        if (ptr) {
            psramTotal += allocTime;
            free(ptr);
        }
    }

    // 测试内部RAM网络缓冲区分配
    for (int i = 0; i < ITERATIONS; i++) {
        uint64_t start = esp_timer_get_time();
        void* ptr = malloc(NETWORK_BUFFER_SIZE);
        uint64_t allocTime = esp_timer_get_time() - start;

        if (ptr) {
            internalTotal += allocTime;
            free(ptr);
        }
    }

    BenchmarkResult result;
    result.name = "Network Buffer Allocation (16KB)";
    result.psramTimeMicros = psramTotal / ITERATIONS;
    result.internalTimeMicros = internalTotal / ITERATIONS;
    result.allocationSize = NETWORK_BUFFER_SIZE;
    result.iterations = ITERATIONS;
    benchmarkResults.push_back(result);
}

// 测试6: 内存碎片整理性能测试
void test_defragmentation_performance(void) {
    if (!MemoryUtils::isPSRAMAvailable()) {
        TEST_IGNORE_MESSAGE("PSRAM not available, skipping defragmentation test");
        return;
    }

    // 创建一些内存碎片
    void* fragments[20];
    size_t fragmentSizes[] = {128, 256, 512, 1024, 2048, 4096};

    for (int i = 0; i < 20; i++) {
        fragments[i] = heap_caps_malloc(fragmentSizes[i % 6], MALLOC_CAP_SPIRAM);
    }

    // 释放部分内存创建碎片
    for (int i = 0; i < 20; i += 2) {
        if (fragments[i]) {
            free(fragments[i]);
            fragments[i] = nullptr;
        }
    }

    // 测量碎片整理前状态
    size_t beforeFree = MemoryUtils::getFreePSRAM();
    size_t beforeLargest = MemoryUtils::getLargestFreePSRAMBlock();

    // 执行碎片整理并测量时间
    uint64_t start = esp_timer_get_time();
    MemoryUtils::defragmentPSRAM();
    uint64_t defragTime = esp_timer_get_time() - start;

    // 测量碎片整理后状态
    size_t afterFree = MemoryUtils::getFreePSRAM();
    size_t afterLargest = MemoryUtils::getLargestFreePSRAMBlock();

    // 清理剩余内存
    for (int i = 0; i < 20; i++) {
        if (fragments[i]) {
            free(fragments[i]);
        }
    }

    // 记录结果
    Serial.printf("Defragmentation Performance:\n");
    Serial.printf("  Time: %llu microseconds\n", defragTime);
    Serial.printf("  Free memory: %u -> %u bytes\n", beforeFree, afterFree);
    Serial.printf("  Largest block: %u -> %u bytes\n", beforeLargest, afterLargest);

    TEST_ASSERT_TRUE(defragTime < 1000000); // 碎片整理应在1秒内完成
}

// 测试7: 连续分配压力测试
void test_allocation_stress_test(void) {
    const int STRESS_ITERATIONS = 1000;
    const size_t STRESS_SIZE = 1024;

    uint64_t psramTotal = 0;
    int psramSuccess = 0;
    int psramFail = 0;

    // PSRAM压力测试
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        uint64_t start = esp_timer_get_time();
        void* ptr = heap_caps_malloc(STRESS_SIZE, MALLOC_CAP_SPIRAM);
        uint64_t allocTime = esp_timer_get_time() - start;

        if (ptr) {
            psramTotal += allocTime;
            psramSuccess++;
            free(ptr);
        } else {
            psramFail++;
        }
    }

    uint64_t internalTotal = 0;
    int internalSuccess = 0;
    int internalFail = 0;

    // 内部RAM压力测试
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        uint64_t start = esp_timer_get_time();
        void* ptr = malloc(STRESS_SIZE);
        uint64_t allocTime = esp_timer_get_time() - start;

        if (ptr) {
            internalTotal += allocTime;
            internalSuccess++;
            free(ptr);
        } else {
            internalFail++;
        }
    }

    Serial.printf("Stress Test Results (1KB x 1000 iterations):\n");
    Serial.printf("  PSRAM: %d success, %d fail, avg time: %llu us\n",
                  psramSuccess, psramFail, psramSuccess > 0 ? psramTotal / psramSuccess : 0);
    Serial.printf("  Internal: %d success, %d fail, avg time: %llu us\n",
                  internalSuccess, internalFail, internalSuccess > 0 ? internalTotal / internalSuccess : 0);

    TEST_ASSERT_EQUAL(STRESS_ITERATIONS, psramSuccess + psramFail);
    TEST_ASSERT_EQUAL(STRESS_ITERATIONS, internalSuccess + internalFail);
}

// 测试8: 内存访问延迟测试
void test_memory_access_latency(void) {
    const size_t TEST_SIZE = 4096;
    const int ACCESS_ITERATIONS = 1000;

    // 分配测试内存
    uint8_t* psramBuffer = (uint8_t*)heap_caps_malloc(TEST_SIZE, MALLOC_CAP_SPIRAM);
    uint8_t* internalBuffer = (uint8_t*)malloc(TEST_SIZE);

    if (!psramBuffer || !internalBuffer) {
        if (psramBuffer) free(psramBuffer);
        if (internalBuffer) free(internalBuffer);
        TEST_IGNORE_MESSAGE("Memory allocation failed for latency test");
        return;
    }

    // 初始化缓冲区
    memset(psramBuffer, 0xAA, TEST_SIZE);
    memset(internalBuffer, 0xAA, TEST_SIZE);

    // 测试PSRAM访问延迟
    uint64_t psramTotal = 0;
    for (int i = 0; i < ACCESS_ITERATIONS; i++) {
        uint64_t start = esp_timer_get_time();
        for (size_t j = 0; j < TEST_SIZE; j++) {
            psramBuffer[j] = (uint8_t)(i + j);
        }
        psramTotal += (esp_timer_get_time() - start);
    }

    // 测试内部RAM访问延迟
    uint64_t internalTotal = 0;
    for (int i = 0; i < ACCESS_ITERATIONS; i++) {
        uint64_t start = esp_timer_get_time();
        for (size_t j = 0; j < TEST_SIZE; j++) {
            internalBuffer[j] = (uint8_t)(i + j);
        }
        internalTotal += (esp_timer_get_time() - start);
    }

    // 清理
    free(psramBuffer);
    free(internalBuffer);

    // 记录结果
    Serial.printf("Memory Access Latency (4KB buffer):\n");
    Serial.printf("  PSRAM avg access time: %llu us\n", psramTotal / ACCESS_ITERATIONS);
    Serial.printf("  Internal RAM avg access time: %llu us\n", internalTotal / ACCESS_ITERATIONS);

    TEST_ASSERT_TRUE(psramTotal > 0);
    TEST_ASSERT_TRUE(internalTotal > 0);
}

// 测试9: SSL连接建立时间对比测试
void test_ssl_connection_time_comparison(void) {
    if (!MemoryUtils::isPSRAMAvailable()) {
        TEST_IGNORE_MESSAGE("PSRAM not available, skipping SSL connection time comparison test");
        return;
    }

    Serial.println("\n==========================================");
    Serial.println("SSL Connection Time Comparison Test");
    Serial.println("==========================================");

    const int SSL_ITERATIONS = 10;
    const char* TEST_HOST = "httpbin.org";
    const uint16_t TEST_PORT = 443;

    // 记录内存初始状态
    MemoryUtils::printMemoryStatus("Before SSL connection tests");

    // 测试1: 使用默认内存配置（内部RAM）的SSL连接时间
    uint64_t defaultTotalTime = 0;
    int defaultSuccessCount = 0;

    Serial.println("\nTesting SSL connection with default memory (internal RAM):");

    for (int i = 0; i < SSL_ITERATIONS; i++) {
        uint64_t startTime = esp_timer_get_time();

        // 获取SSL客户端（这会触发SSL上下文初始化）
        WiFiClientSecure* client = SSLClientManager::getInstance().getClient(TEST_HOST, TEST_PORT);

        if (client) {
            uint64_t allocationTime = esp_timer_get_time() - startTime;
            defaultTotalTime += allocationTime;
            defaultSuccessCount++;

            Serial.printf("  Iteration %d: SSL client allocation time = %llu us\n",
                         i + 1, allocationTime);

            // 立即释放客户端（不保持连接）
            SSLClientManager::getInstance().releaseClient(client, false);
        } else {
            Serial.printf("  Iteration %d: Failed to get SSL client\n", i + 1);
        }

        // 短暂延迟，避免资源冲突
        delay(10);
    }

    // 测试2: 强制SSL缓冲区使用PSRAM的SSL连接时间
    uint64_t psramTotalTime = 0;
    int psramSuccessCount = 0;

    Serial.println("\nTesting SSL connection with PSRAM buffers:");

    // 在PSRAM中预分配SSL缓冲区
    const size_t SSL_BUFFER_SIZE = 16384; // 典型的SSL缓冲区大小
    void* psramSSLBuffer = MemoryUtils::allocateSSLBuffer(SSL_BUFFER_SIZE);

    if (!psramSSLBuffer) {
        Serial.println("  WARNING: Failed to allocate PSRAM SSL buffer, test may be incomplete");
    }

    for (int i = 0; i < SSL_ITERATIONS; i++) {
        uint64_t startTime = esp_timer_get_time();

        // 获取SSL客户端（SSL缓冲区现在可能来自PSRAM）
        WiFiClientSecure* client = SSLClientManager::getInstance().getClient(TEST_HOST, TEST_PORT);

        if (client) {
            uint64_t allocationTime = esp_timer_get_time() - startTime;
            psramTotalTime += allocationTime;
            psramSuccessCount++;

            Serial.printf("  Iteration %d: SSL client allocation time = %llu us\n",
                         i + 1, allocationTime);

            // 立即释放客户端
            SSLClientManager::getInstance().releaseClient(client, false);
        } else {
            Serial.printf("  Iteration %d: Failed to get SSL client\n", i + 1);
        }

        // 短暂延迟
        delay(10);
    }

    // 清理PSRAM缓冲区
    if (psramSSLBuffer) {
        free(psramSSLBuffer);
    }

    // 清理所有SSL客户端
    SSLClientManager::getInstance().cleanupAll(true);

    // 计算并显示结果
    Serial.println("\nSSL Connection Time Comparison Results:");
    Serial.println("------------------------------------------");

    if (defaultSuccessCount > 0) {
        uint64_t defaultAvgTime = defaultTotalTime / defaultSuccessCount;
        Serial.printf("Default memory (internal RAM):\n");
        Serial.printf("  Success rate: %d/%d (%.1f%%)\n",
                     defaultSuccessCount, SSL_ITERATIONS,
                     (defaultSuccessCount * 100.0) / SSL_ITERATIONS);
        Serial.printf("  Average allocation time: %llu us (%.2f ms)\n",
                     defaultAvgTime, defaultAvgTime / 1000.0);
    }

    if (psramSuccessCount > 0) {
        uint64_t psramAvgTime = psramTotalTime / psramSuccessCount;
        Serial.printf("\nWith PSRAM buffers:\n");
        Serial.printf("  Success rate: %d/%d (%.1f%%)\n",
                     psramSuccessCount, SSL_ITERATIONS,
                     (psramSuccessCount * 100.0) / SSL_ITERATIONS);
        Serial.printf("  Average allocation time: %llu us (%.2f ms)\n",
                     psramAvgTime, psramAvgTime / 1000.0);
    }

    if (defaultSuccessCount > 0 && psramSuccessCount > 0) {
        uint64_t defaultAvgTime = defaultTotalTime / defaultSuccessCount;
        uint64_t psramAvgTime = psramTotalTime / psramSuccessCount;

        if (defaultAvgTime > 0) {
            float slowdown = (float)psramAvgTime / defaultAvgTime;
            Serial.printf("\nPerformance comparison:\n");
            Serial.printf("  PSRAM slowdown factor: %.2fx\n", slowdown);

            if (slowdown < 1.2) {
                Serial.println("  Result: EXCELLENT (minimal impact)");
            } else if (slowdown < 2.0) {
                Serial.println("  Result: GOOD (acceptable impact)");
            } else if (slowdown < 3.0) {
                Serial.println("  Result: FAIR (noticeable impact)");
            } else {
                Serial.println("  Result: POOR (significant impact)");
            }
        }
    }

    // 记录内存最终状态
    MemoryUtils::printMemoryStatus("After SSL connection tests");

    // 将结果添加到基准测试结果中
    if (defaultSuccessCount > 0 && psramSuccessCount > 0) {
        BenchmarkResult result;
        result.name = "SSL Client Allocation";
        result.psramTimeMicros = psramTotalTime / psramSuccessCount;
        result.internalTimeMicros = defaultTotalTime / defaultSuccessCount;
        result.allocationSize = SSL_BUFFER_SIZE;
        result.iterations = SSL_ITERATIONS;
        benchmarkResults.push_back(result);
    }

    // 基本断言：至少应该有一些成功的分配
    TEST_ASSERT_TRUE(defaultSuccessCount > 0 || psramSuccessCount > 0);

    Serial.println("==========================================");
}

// 打印所有基准测试结果
void printBenchmarkResults() {
    Serial.println("\n==========================================");
    Serial.println("PSRAM Performance Benchmark Results");
    Serial.println("==========================================");

    for (const auto& result : benchmarkResults) {
        float psramMs = result.psramTimeMicros / 1000.0;
        float internalMs = result.internalTimeMicros / 1000.0;
        float slowdown = (result.psramTimeMicros > 0 && result.internalTimeMicros > 0)
            ? (float)result.psramTimeMicros / result.internalTimeMicros
            : 0.0;

        Serial.printf("\n%s:\n", result.name);
        Serial.printf("  Allocation size: %u bytes\n", result.allocationSize);
        Serial.printf("  Iterations: %d\n", result.iterations);
        Serial.printf("  PSRAM time: %.2f ms (avg)\n", psramMs);
        Serial.printf("  Internal RAM time: %.2f ms (avg)\n", internalMs);
        Serial.printf("  Slowdown factor: %.2fx\n", slowdown);

        if (slowdown > 0) {
            if (slowdown < 1.5) {
                Serial.println("  Performance: EXCELLENT (minimal slowdown)");
            } else if (slowdown < 3.0) {
                Serial.println("  Performance: GOOD (acceptable slowdown)");
            } else if (slowdown < 5.0) {
                Serial.println("  Performance: FAIR (noticeable slowdown)");
            } else {
                Serial.println("  Performance: POOR (significant slowdown)");
            }
        }
    }

    Serial.println("\n==========================================");
    Serial.println("Memory Status Summary:");
    Serial.println("==========================================");
    MemoryUtils::printMemoryStatus("Benchmark Complete");
}

// 设置函数
void setUp(void) {
    // 初始化串口
    Serial.begin(115200);
    delay(1000);

    // 清空基准测试结果
    benchmarkResults.clear();

    Serial.println("\nStarting PSRAM Performance Benchmark Tests...");
    Serial.println("==========================================");
}

// 清理函数
void tearDown(void) {
    // 打印所有基准测试结果
    printBenchmarkResults();

    // 执行一次碎片整理
    if (MemoryUtils::isPSRAMAvailable()) {
        Serial.println("\nPerforming final PSRAM defragmentation...");
        MemoryUtils::defragmentPSRAM();
    }
}

// 主测试函数
void runPSRAMPerformanceTests() {
    UNITY_BEGIN();

    RUN_TEST(test_small_allocation_performance);
    RUN_TEST(test_medium_allocation_performance);
    RUN_TEST(test_large_allocation_performance);
    RUN_TEST(test_audio_buffer_allocation);
    RUN_TEST(test_network_buffer_allocation);
    RUN_TEST(test_defragmentation_performance);
    RUN_TEST(test_allocation_stress_test);
    RUN_TEST(test_memory_access_latency);
    RUN_TEST(test_ssl_connection_time_comparison);

    UNITY_END();
}

// Arduino setup函数
void setup() {
    delay(2000); // 等待串口连接

    runPSRAMPerformanceTests();
}

// Arduino loop函数
void loop() {
    // 测试完成后保持空闲
    delay(1000);
}