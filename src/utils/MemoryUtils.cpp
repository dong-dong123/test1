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

    ESP_LOGI(TAG, "PSRAM defragmentation completed:");
    ESP_LOGI(TAG, "  Free memory: %u -> %u bytes", beforeFree, afterFree);
    ESP_LOGI(TAG, "  Largest block: %u -> %u bytes", beforeLargest, afterLargest);

    if (afterLargest > beforeLargest) {
        ESP_LOGI(TAG, "  Improvement: +%u bytes in largest block", afterLargest - beforeLargest);
    } else {
        ESP_LOGI(TAG, "  No significant improvement in largest block size");
    }
}