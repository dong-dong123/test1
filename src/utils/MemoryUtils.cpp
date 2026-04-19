// src/utils/MemoryUtils.cpp
#include "MemoryUtils.h"
#include <esp_log.h>

static const char* TAG = "MemoryUtils";

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
        ESP_LOGI(TAG, "Allocated audio buffer: %u bytes in PSRAM at %p", size, ptr);
        return ptr;
    }

    // 回退到内部RAM
    ESP_LOGW(TAG, "Audio buffer PSRAM allocation failed for %u bytes, using internal RAM", size);
    ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    if (ptr) {
        ESP_LOGI(TAG, "Allocated audio buffer: %u bytes in internal RAM at %p", size, ptr);
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
        ESP_LOGI(TAG, "Allocated network buffer: %u bytes in PSRAM at %p", size, ptr);
        return ptr;
    }

    // 回退到内部RAM
    ESP_LOGW(TAG, "Network buffer PSRAM allocation failed for %u bytes, using internal RAM", size);
    ptr = malloc(size);
    if (ptr) {
        ESP_LOGI(TAG, "Allocated network buffer: %u bytes in internal RAM at %p", size, ptr);
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
        ESP_LOGI(TAG, "Allocated SSL buffer: %u bytes in PSRAM at %p", size, ptr);
        return ptr;
    }

    // 回退到内部RAM
    ESP_LOGW(TAG, "SSL buffer PSRAM allocation failed for %u bytes, using internal RAM", size);
    ptr = malloc(size);
    if (ptr) {
        ESP_LOGI(TAG, "Allocated SSL buffer: %u bytes in internal RAM at %p", size, ptr);
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
    void* testBlocks[10];
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

    // 如果碎片化评分超过50或最大块小于64KB，建议进行碎片整理
    bool needsDefrag = (fragScore > 50) || (largestBlock < 65536);

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
    if (fragScore < 30) {
        // 轻度碎片化：简单整理
        ESP_LOGI(TAG, "Light fragmentation detected, using simple defragmentation");
        defragmentPSRAM();
    } else if (fragScore < 70) {
        // 中度碎片化：中等强度整理
        ESP_LOGI(TAG, "Medium fragmentation detected, using enhanced defragmentation");

        // 使用多种大小的块进行整理
        size_t blockSizes[] = {512, 1024, 2048, 4096, 8192};
        const int numSizes = sizeof(blockSizes) / sizeof(blockSizes[0]);

        for (int sizeIdx = 0; sizeIdx < numSizes; sizeIdx++) {
            size_t blockSize = blockSizes[sizeIdx];
            if (blockSize * 10 > totalFree * 0.5) {
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
            if (currentScore < 30) {
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
    static uint32_t checkInterval = 30000; // 30秒检查一次

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

        if (freeInternal > 102400) { // 内部RAM有100KB以上空闲
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