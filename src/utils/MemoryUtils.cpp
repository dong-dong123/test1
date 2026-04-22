// src/utils/MemoryUtils.cpp
#include "MemoryUtils.h"
#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "MemoryUtils";

// 碎片整理相关常量
namespace {
    constexpr size_t DEFRAG_THRESHOLD_SCORE = 50;          // 碎片化评分阈值，超过此值建议整理
    constexpr size_t DEFRAG_THRESHOLD_BLOCK_SIZE = 65536;  // 最大块大小阈值（64KB），小于此值建议整理
    constexpr size_t LIGHT_FRAGMENTATION_MAX = 30;         // 轻度碎片化最大评分
    constexpr size_t MEDIUM_FRAGMENTATION_MAX = 70;        // 中度碎片化最大评分
    constexpr size_t DEFRAG_BLOCK_SIZE_MULTIPLIER = 10;    // 块大小乘数
    constexpr size_t DEFRAG_FREE_MEMORY_RATIO = 2;         // 空闲内存比率（1/2 = 0.5）
    constexpr size_t MIN_FREE_INTERNAL_FOR_DEFRAG = 102400; // 执行碎片整理所需的最小内部RAM空闲（100KB）
    constexpr uint32_t PERIODIC_CHECK_INTERVAL_MS = 30000; // 周期性检查间隔（30秒）
}

void* MemoryUtils::allocatePSRAM(size_t size, const char* tag) {
    if (size == 0) return nullptr;

    // 优先尝试PSRAM
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr) {
        ESP_LOGD(TAG, "[%s] Allocated %u bytes in PSRAM at %p",
                tag ? tag : "unknown", size, ptr);
        return ptr;
    }

    // PSRAM失败，回退到内部RAM
    ESP_LOGW(TAG, "[%s] PSRAM allocation failed for %u bytes, falling back to internal RAM",
            tag ? tag : "unknown", size);
    ptr = malloc(size);
    if (ptr) {
        ESP_LOGD(TAG, "[%s] Allocated %u bytes in internal RAM at %p",
                tag ? tag : "unknown", size, ptr);
    } else {
        ESP_LOGE(TAG, "[%s] Allocation failed for %u bytes", tag ? tag : "unknown", size);
    }
    return ptr;
}

void* MemoryUtils::allocatePSRAMClear(size_t size, const char* tag) {
    if (size == 0) return nullptr;

    // 优先尝试PSRAM
    void* ptr = heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM);
    if (ptr) {
        ESP_LOGD(TAG, "[%s] Allocated and cleared %u bytes in PSRAM at %p",
                tag ? tag : "unknown", size, ptr);
        return ptr;
    }

    // PSRAM失败，回退到内部RAM
    ESP_LOGW(TAG, "[%s] PSRAM calloc failed for %u bytes, falling back to internal RAM",
            tag ? tag : "unknown", size);
    ptr = calloc(1, size);
    if (ptr) {
        ESP_LOGD(TAG, "[%s] Allocated and cleared %u bytes in internal RAM at %p",
                tag ? tag : "unknown", size, ptr);
    } else {
        ESP_LOGE(TAG, "[%s] Calloc failed for %u bytes", tag ? tag : "unknown", size);
    }
    return ptr;
}

void* MemoryUtils::allocateAudioBuffer(size_t size) {
    if (size == 0) return nullptr;

    // 音频缓冲区通常需要DMA能力，优先使用PSRAM
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr) {
        ESP_LOGD(TAG, "Allocated audio buffer: %u bytes in PSRAM at %p", size, ptr);
        return ptr;
    }

    // 回退到内部RAM
    ESP_LOGW(TAG, "Audio buffer PSRAM allocation failed for %u bytes, using internal RAM", size);
    ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    if (ptr) {
        ESP_LOGD(TAG, "Allocated audio buffer: %u bytes in internal RAM at %p", size, ptr);
    } else {
        ESP_LOGE(TAG, "Audio buffer allocation failed for %u bytes", size);
    }
    return ptr;
}

void* MemoryUtils::allocateNetworkBuffer(size_t size) {
    if (size == 0) return nullptr;

    // 网络缓冲区通常较大，优先使用PSRAM
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr) {
        ESP_LOGD(TAG, "Allocated network buffer: %u bytes in PSRAM at %p", size, ptr);
        return ptr;
    }

    // 回退到内部RAM
    ESP_LOGW(TAG, "Network buffer PSRAM allocation failed for %u bytes, using internal RAM", size);
    ptr = malloc(size);
    if (ptr) {
        ESP_LOGD(TAG, "Allocated network buffer: %u bytes in internal RAM at %p", size, ptr);
    } else {
        ESP_LOGE(TAG, "Network buffer allocation failed for %u bytes", size);
    }
    return ptr;
}

void* MemoryUtils::allocateSSLBuffer(size_t size) {
    if (size == 0) return nullptr;

    // SSL缓冲区（非敏感数据）优先使用PSRAM
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr) {
        ESP_LOGD(TAG, "Allocated SSL buffer: %u bytes in PSRAM at %p", size, ptr);
        return ptr;
    }

    // 回退到内部RAM
    ESP_LOGW(TAG, "SSL buffer PSRAM allocation failed for %u bytes, using internal RAM", size);
    ptr = malloc(size);
    if (ptr) {
        ESP_LOGD(TAG, "Allocated SSL buffer: %u bytes in internal RAM at %p", size, ptr);
    } else {
        ESP_LOGE(TAG, "SSL buffer allocation failed for %u bytes", size);
    }
    return ptr;
}

void MemoryUtils::printMemoryStatus(const char* tag) {
    size_t freeInternal = getFreeInternal();
    size_t freePSRAM = getFreePSRAM();
    size_t largestPSRAM = getLargestFreePSRAMBlock();

    ESP_LOGI(TAG, "[%s] Memory Status:", tag ? tag : "system");
    ESP_LOGI(TAG, "  Internal RAM free: %u bytes (%.1f KB)", freeInternal, freeInternal / 1024.0);
    ESP_LOGI(TAG, "  PSRAM free: %u bytes (%.1f KB)", freePSRAM, freePSRAM / 1024.0);
    ESP_LOGI(TAG, "  Largest PSRAM block: %u bytes (%.1f KB)", largestPSRAM, largestPSRAM / 1024.0);
    ESP_LOGI(TAG, "  PSRAM available: %s", isPSRAMAvailable() ? "YES" : "NO");
}

void MemoryUtils::printDetailedMemoryStatus(const char* tag) {
    size_t freeInternal = getFreeInternal();
    size_t totalInternal = getTotalInternal();
    size_t usedInternal = (totalInternal > freeInternal) ? (totalInternal - freeInternal) : 0;
    size_t largestInternal = getLargestFreeInternalBlock();
    size_t minHeap = getMinFreeHeap();

    ESP_LOGI(TAG, "========== [%s] 详细内存状态 ==========", tag ? tag : "system");
    ESP_LOGI(TAG, "【内部SRAM】总计=%u KB, 空闲=%u KB, 已用=%u KB, 最大空闲块=%u KB",
             totalInternal / 1024, freeInternal / 1024, usedInternal / 1024,
             largestInternal / 1024);
    ESP_LOGI(TAG, "【内部SRAM】历史最小空闲=%u KB (%.1f%%)",
             minHeap / 1024,
             totalInternal > 0 ? (minHeap * 100.0 / totalInternal) : 0);

    if (isPSRAMAvailable()) {
        size_t freePSRAM = getFreePSRAM();
        size_t totalPSRAM = getTotalPSRAM();
        size_t usedPSRAM = (totalPSRAM > freePSRAM) ? (totalPSRAM - freePSRAM) : 0;
        size_t largestPSRAM = getLargestFreePSRAMBlock();
        size_t fragScore = getFragmentationScore();

        ESP_LOGI(TAG, "【PSRAM】总计=%u KB, 空闲=%u KB, 已用=%u KB, 最大空闲块=%u KB",
                 totalPSRAM / 1024, freePSRAM / 1024, usedPSRAM / 1024,
                 largestPSRAM / 1024);
        ESP_LOGI(TAG, "【PSRAM】碎片评分=%u/100", fragScore);
    } else {
        ESP_LOGI(TAG, "【PSRAM】不可用");
    }

    // 栈使用信息（简化版，只监控高水位标记）
    // 注意：由于FreeRTOS版本差异，无法直接获取所有任务状态，只监控当前任务
    ESP_LOGI(TAG, "【栈监控】监控当前任务栈高水位标记（剩余空间）");

    // 监控当前任务栈
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    if (currentTask) {
        UBaseType_t highWaterMarkWords = uxTaskGetStackHighWaterMark(currentTask);
        size_t highWaterMarkBytes = highWaterMarkWords * sizeof(StackType_t);
        const char* taskName = pcTaskGetTaskName(currentTask);

        ESP_LOGI(TAG, "  当前任务: %s, 栈剩余空间: %u 字节",
                taskName ? taskName : "unknown", highWaterMarkBytes);

        // 警告低栈空间（高水位标记太小）
        // 阈值：小于1KB（256字，假设StackType_t为4字节）
        if (highWaterMarkBytes < 1024) {
            ESP_LOGW(TAG, "【栈警告】当前任务栈剩余空间不足1KB!");
        }
    }

    ESP_LOGI(TAG, "========================================");
}

size_t MemoryUtils::getTotalInternal() {
    return heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
}

size_t MemoryUtils::getTotalPSRAM() {
    if (!isPSRAMAvailable()) return 0;
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
}

size_t MemoryUtils::getLargestFreeInternalBlock() {
    return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
}

size_t MemoryUtils::getMinFreeHeap() {
    return esp_get_minimum_free_heap_size();
}

void MemoryUtils::logHeapUsage(const char* tag) {
    size_t freeInternal = getFreeInternal();
    size_t totalInternal = getTotalInternal();
    size_t minHeap = getMinFreeHeap();

    ESP_LOGI(TAG, "[%s] SRAM: 空闲=%uKB/%uKB, 最小=%uKB, 最大块=%uKB",
             tag ? tag : "heap",
             freeInternal / 1024, totalInternal / 1024,
             minHeap / 1024,
             getLargestFreeInternalBlock() / 1024);
}

size_t MemoryUtils::getFreeInternal() {
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
}

size_t MemoryUtils::getFreePSRAM() {
    if (!isPSRAMAvailable()) {
        return 0;
    }
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

size_t MemoryUtils::getLargestFreePSRAMBlock() {
    if (!isPSRAMAvailable()) {
        return 0;
    }
    return heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}

bool MemoryUtils::isPSRAMAvailable() {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0;
}

bool MemoryUtils::verifyPSRAMAllocation(size_t testSize) {
    if (!isPSRAMAvailable()) {
        ESP_LOGI(TAG, "PSRAM验证: 不可用 (isPSRAMAvailable返回false)");
        return false;
    }

    // 尝试分配一个小块来验证PSRAM实际可用性
    void* testBlock = heap_caps_malloc(testSize, MALLOC_CAP_SPIRAM);
    if (!testBlock) {
        ESP_LOGW(TAG, "PSRAM验证: 分配 %u 字节失败 (PSRAM可能存在但配置有问题)", testSize);
        return false;
    }

    // 写入测试数据
    memset(testBlock, 0xAA, testSize);

    // 读取验证
    uint8_t* data = (uint8_t*)testBlock;
    bool verificationPassed = true;
    for (size_t i = 0; i < testSize; i++) {
        if (data[i] != 0xAA) {
            verificationPassed = false;
            break;
        }
    }

    // 释放测试块
    free(testBlock);

    if (verificationPassed) {
        ESP_LOGI(TAG, "PSRAM验证: 通过 (成功分配、写入和读取 %u 字节)", testSize);
        return true;
    } else {
        ESP_LOGW(TAG, "PSRAM验证: 失败 (数据完整性检查未通过)");
        return false;
    }
}

void MemoryUtils::printPSRAMStatus(const char* tag) {
    ESP_LOGI(TAG, "========== [%s] PSRAM详细状态 ==========", tag ? tag : "PSRAM");

    bool available = isPSRAMAvailable();
    ESP_LOGI(TAG, "PSRAM可用性: %s", available ? "是" : "否");

    if (available) {
        size_t total = getTotalPSRAM();
        size_t free = getFreePSRAM();
        size_t largest = getLargestFreePSRAMBlock();
        size_t used = (total > free) ? (total - free) : 0;

        ESP_LOGI(TAG, "总计: %u 字节 (%.1f MB)", total, total / (1024.0 * 1024.0));
        ESP_LOGI(TAG, "已用: %u 字节 (%.1f MB)", used, used / (1024.0 * 1024.0));
        ESP_LOGI(TAG, "空闲: %u 字节 (%.1f MB)", free, free / (1024.0 * 1024.0));
        ESP_LOGI(TAG, "最大空闲块: %u 字节 (%.1f KB)", largest, largest / 1024.0);
        ESP_LOGI(TAG, "使用率: %.1f%%", total > 0 ? (used * 100.0 / total) : 0.0);

        // 验证分配能力
        bool canAllocate = verifyPSRAMAllocation(1024);
        ESP_LOGI(TAG, "分配验证: %s", canAllocate ? "通过" : "失败");
    } else {
        ESP_LOGI(TAG, "PSRAM未检测到或未正确配置");
        ESP_LOGI(TAG, "提示: 检查ESP32-S3的PSRAM配置:");
        ESP_LOGI(TAG, "  1. 确保硬件连接正确");
        ESP_LOGI(TAG, "  2. 检查platformio.ini中的PSRAM设置");
        ESP_LOGI(TAG, "  3. 确认板卡支持PSRAM");
    }

    ESP_LOGI(TAG, "==========================================");
}

void MemoryUtils::defragmentPSRAM() {
    if (!isPSRAMAvailable()) {
        ESP_LOGW(TAG, "PSRAM not available, cannot defragment");
        return;
    }

    ESP_LOGI(TAG, "Starting PSRAM defragmentation...");

    // 获取当前内存状态
    size_t beforeFree = getFreePSRAM();
    size_t beforeLargest = getLargestFreePSRAMBlock();
    size_t beforeFragScore = getFragmentationScore();

    // 执行碎片整理（通过分配和释放内存来整理碎片）
    // 注意：这是一个简单的实现，实际效果有限
    const size_t testSize = 1024; // 1KB测试块
    void* testBlocks[10] = {nullptr}; // 初始化为nullptr
    size_t allocatedCount = 0;

    // 分配一些测试块
    for (int i = 0; i < 10; i++) {
        testBlocks[i] = heap_caps_malloc(testSize, MALLOC_CAP_SPIRAM);
        if (testBlocks[i]) {
            allocatedCount++;
        }
    }

    // 释放测试块（以不同顺序释放，帮助整理碎片）
    for (int i = 0; i < 10; i += 2) {
        if (testBlocks[i]) {
            free(testBlocks[i]);
            testBlocks[i] = nullptr;
        }
    }

    for (int i = 1; i < 10; i += 2) {
        if (testBlocks[i]) {
            free(testBlocks[i]);
            testBlocks[i] = nullptr;
        }
    }

    // 获取整理后的内存状态
    size_t afterFree = getFreePSRAM();
    size_t afterLargest = getLargestFreePSRAMBlock();
    size_t afterFragScore = getFragmentationScore();

    ESP_LOGI(TAG, "PSRAM defragmentation completed:");
    ESP_LOGI(TAG, "  Free memory: %u -> %u bytes", beforeFree, afterFree);
    ESP_LOGI(TAG, "  Largest block: %u -> %u bytes", beforeLargest, afterLargest);
    ESP_LOGI(TAG, "  Fragmentation score: %u -> %u", beforeFragScore, afterFragScore);

    if (afterLargest > beforeLargest) {
        ESP_LOGI(TAG, "  Improvement: +%u bytes in largest block", afterLargest - beforeLargest);
    } else {
        ESP_LOGI(TAG, "  No significant improvement in largest block size");
    }
}

size_t MemoryUtils::getFragmentationScore() {
    if (!isPSRAMAvailable()) {
        return 0;
    }

    // 获取总空闲内存和最大空闲块
    size_t totalFree = getFreePSRAM();
    size_t largestBlock = getLargestFreePSRAMBlock();

    if (totalFree == 0) {
        return 100; // 没有空闲内存，完全碎片化
    }

    // 计算碎片化评分：最大块占总空闲内存的比例
    // 比例越低，碎片化越严重
    float fragmentationRatio = (float)largestBlock / totalFree;

    // 转换为0-100的评分（0=无碎片，100=完全碎片化）
    size_t score = (size_t)((1.0f - fragmentationRatio) * 100.0f);

    return min(score, (size_t)100);
}

bool MemoryUtils::needsDefragmentation() {
    if (!isPSRAMAvailable()) {
        return false;
    }

    size_t fragScore = getFragmentationScore();
    size_t largestBlock = getLargestFreePSRAMBlock();

    // 如果碎片化评分超过阈值或最大块小于阈值，建议进行碎片整理
    bool needsDefrag = (fragScore > DEFRAG_THRESHOLD_SCORE) || (largestBlock < DEFRAG_THRESHOLD_BLOCK_SIZE);

    if (needsDefrag) {
        ESP_LOGD(TAG, "Defragmentation recommended: fragScore=%u, largestBlock=%u",
                fragScore, largestBlock);
    }

    return needsDefrag;
}

void MemoryUtils::smartDefragmentPSRAM() {
    if (!isPSRAMAvailable()) {
        return;
    }

    size_t fragScore = getFragmentationScore();
    size_t totalFree = getFreePSRAM();

    ESP_LOGI(TAG, "Starting smart defragmentation (fragScore=%u, free=%u bytes)...",
            fragScore, totalFree);

    // 根据碎片化程度调整策略
    if (fragScore < LIGHT_FRAGMENTATION_MAX) {
        // 轻度碎片化：简单整理
        ESP_LOGI(TAG, "Light fragmentation detected, using simple defragmentation");
        defragmentPSRAM();
    } else if (fragScore < MEDIUM_FRAGMENTATION_MAX) {
        // 中度碎片化：中等强度整理
        ESP_LOGI(TAG, "Medium fragmentation detected, using enhanced defragmentation");

        // 使用多种大小的块进行整理
        size_t blockSizes[] = {512, 1024, 2048, 4096, 8192};
        const int numSizes = sizeof(blockSizes) / sizeof(blockSizes[0]);

        for (int sizeIdx = 0; sizeIdx < numSizes; sizeIdx++) {
            size_t blockSize = blockSizes[sizeIdx];
            // 检查块是否太大：如果10个块会占用超过一半的空闲内存，则跳过
            if (blockSize * DEFRAG_BLOCK_SIZE_MULTIPLIER * 2 > totalFree) {
                continue; // 块太大，跳过
            }

            void* blocks[5];
            for (int i = 0; i < 5; i++) {
                blocks[i] = heap_caps_malloc(blockSize, MALLOC_CAP_SPIRAM);
            }

            // 交错释放
            for (int i = 0; i < 5; i += 2) {
                if (blocks[i]) free(blocks[i]);
            }
            for (int i = 1; i < 5; i += 2) {
                if (blocks[i]) free(blocks[i]);
            }
        }

        // 最后执行一次标准整理
        defragmentPSRAM();
    } else {
        // 严重碎片化：强力整理
        ESP_LOGI(TAG, "Severe fragmentation detected, using aggressive defragmentation");

        // 记录当前状态
        size_t beforeLargest = getLargestFreePSRAMBlock();
        size_t beforeScore = getFragmentationScore();

        // 尝试多次整理
        for (int attempt = 0; attempt < 3; attempt++) {
            defragmentPSRAM();

            size_t currentScore = getFragmentationScore();
            if (currentScore < LIGHT_FRAGMENTATION_MAX) {
                ESP_LOGI(TAG, "Fragmentation improved to %u after %d attempts",
                        currentScore, attempt + 1);
                break;
            }

            delay(10); // 短暂延迟
        }

        size_t afterLargest = getLargestFreePSRAMBlock();
        size_t afterScore = getFragmentationScore();

        ESP_LOGI(TAG, "Aggressive defragmentation results:");
        ESP_LOGI(TAG, "  Largest block: %u -> %u bytes", beforeLargest, afterLargest);
        ESP_LOGI(TAG, "  Fragmentation: %u -> %u", beforeScore, afterScore);
    }
}

void MemoryUtils::periodicDefragmentationCheck() {
    static uint32_t lastCheckTime = 0;
    static uint32_t checkInterval = PERIODIC_CHECK_INTERVAL_MS; // 30秒检查一次

    uint32_t currentTime = millis();

    // 检查是否到达检查时间
    if (currentTime - lastCheckTime < checkInterval) {
        return;
    }

    lastCheckTime = currentTime;

    if (!isPSRAMAvailable()) {
        return;
    }

    // 检查是否需要碎片整理
    if (needsDefragmentation()) {
        ESP_LOGI(TAG, "Periodic check: PSRAM needs defragmentation");

        // 根据系统负载决定是否立即整理
        size_t freeInternal = getFreeInternal();

        if (freeInternal > MIN_FREE_INTERNAL_FOR_DEFRAG) { // 内部RAM有足够空闲
            ESP_LOGI(TAG, "System load is low, performing defragmentation now");
            smartDefragmentPSRAM();
        } else {
            ESP_LOGI(TAG, "System load is high, deferring defragmentation");
        }
    } else {
        ESP_LOGD(TAG, "Periodic check: PSRAM fragmentation is acceptable");
    }

    // 打印内存状态（调试信息）
    printMemoryStatus("periodic_check");
}

// ============================================================================
// 栈使用监控
// ============================================================================

void MemoryUtils::monitorTaskStacks(const char* tag) {
    ESP_LOGI(TAG, "========== [%s] 任务栈监控 ==========", tag ? tag : "Stack");

    // 获取任务数量
    UBaseType_t taskCount = uxTaskGetNumberOfTasks();
    ESP_LOGI(TAG, "总任务数: %u", taskCount);

    // 简化版：只监控当前任务
    // 注意：由于FreeRTOS版本差异，无法直接获取所有任务状态
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    if (currentTask) {
        UBaseType_t highWaterMarkWords = uxTaskGetStackHighWaterMark(currentTask);
        size_t highWaterMarkBytes = highWaterMarkWords * sizeof(StackType_t);
        const char* taskName = pcTaskGetTaskName(currentTask);

        ESP_LOGI(TAG, "当前任务: %s", taskName ? taskName : "unknown");
        ESP_LOGI(TAG, "  栈高水位标记: %u 字 (最低空闲: %u 字节)",
                highWaterMarkWords, highWaterMarkBytes);
        ESP_LOGI(TAG, "  注意: 由于FreeRTOS版本限制，只能监控当前任务");

        // 警告低栈空间（高水位标记太小）
        // 阈值：小于1KB（256字，假设StackType_t为4字节）
        if (highWaterMarkBytes < 1024) {
            ESP_LOGW(TAG, "  警告: 栈剩余空间不足1KB!");
        }
    }

    ESP_LOGI(TAG, "======================================");
}

size_t MemoryUtils::getTaskStackHighWaterMark(TaskHandle_t task) {
    if (task == nullptr) {
        task = xTaskGetCurrentTaskHandle();
    }

    UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(task);
    return highWaterMark * sizeof(StackType_t); // 转换为字节
}

void MemoryUtils::printTaskStackInfo(const char* tag) {
    ESP_LOGI(TAG, "========== [%s] 任务栈信息 ==========", tag ? tag : "TaskStacks");

    // 获取当前任务
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    const char* currentTaskName = pcTaskGetTaskName(currentTask);

    // 获取当前任务栈信息
    size_t currentHighWaterMark = getTaskStackHighWaterMark(currentTask);

    ESP_LOGI(TAG, "当前任务: %s", currentTaskName ? currentTaskName : "unknown");
    ESP_LOGI(TAG, "栈高水位标记: %u 字节 (最低空闲)", currentHighWaterMark);

    // 监控所有任务栈
    monitorTaskStacks(tag);
}

size_t MemoryUtils::getTotalStackUsage() {
    // 注意：由于FreeRTOS版本差异，无法直接获取栈大小
    // 返回0表示功能不可用
    ESP_LOGW(TAG, "getTotalStackUsage: 功能不可用（FreeRTOS版本差异）");
    return 0;
}

size_t MemoryUtils::getPeakStackUsage() {
    // 注意：由于FreeRTOS版本差异，无法直接获取栈大小
    // 返回0表示功能不可用
    ESP_LOGW(TAG, "getPeakStackUsage: 功能不可用（FreeRTOS版本差异）");
    return 0;
}