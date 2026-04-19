#ifndef CACHED_PSRAM_BUFFER_H
#define CACHED_PSRAM_BUFFER_H

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

/**
 * @class CachedPSRAMBuffer
 * @brief 提供PSRAM缓存的缓冲区类，通过内部RAM缓存优化PSRAM访问性能
 *
 * 这个类实现了读写分离策略：
 * - 读取：从内部RAM缓存读取（快速）
 * - 写入：写入内部RAM缓存并标记为脏数据
 * - 同步：将脏数据批量写入PSRAM（减少PSRAM写入次数）
 */
class CachedPSRAMBuffer {
private:
    static const char* TAG;

    void* psramPtr;          // PSRAM中的实际数据
    void* cacheBuffer;       // 内部RAM缓存
    size_t bufferSize;       // 缓冲区大小（字节）
    bool dirty;              // 缓存是否脏（需要同步到PSRAM）
    bool useCache;           // 是否启用缓存
    uint32_t syncThreshold;  // 同步阈值（字节）

public:
    /**
     * @brief 构造函数
     * @param size 缓冲区大小（字节）
     * @param enableCache 是否启用缓存（默认启用）
     * @param syncThresholdBytes 同步阈值（字节），达到此阈值时自动同步
     */
    CachedPSRAMBuffer(size_t size, bool enableCache = true, uint32_t syncThresholdBytes = 1024)
        : psramPtr(nullptr), cacheBuffer(nullptr), bufferSize(size),
          dirty(false), useCache(enableCache), syncThreshold(syncThresholdBytes) {

        if (size == 0) {
            ESP_LOGE(TAG, "Cannot create buffer with zero size");
            return;
        }

        // 分配PSRAM内存
        psramPtr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (!psramPtr) {
            ESP_LOGE(TAG, "Failed to allocate %u bytes in PSRAM", size);
            return;
        }

        // 如果启用缓存，分配内部RAM缓存
        if (useCache) {
            cacheBuffer = malloc(size);
            if (!cacheBuffer) {
                ESP_LOGW(TAG, "Failed to allocate cache buffer, disabling cache");
                free(psramPtr);
                psramPtr = nullptr;
                bufferSize = 0;  // 确保缓冲区大小重置
                useCache = false;
                dirty = false;   // 确保脏标志重置
                return;
            }

            // 初始化缓存（从PSRAM读取数据）
            memcpy(cacheBuffer, psramPtr, size);
            ESP_LOGD(TAG, "Created cached buffer: %u bytes (cache: %u bytes)",
                    size, size);
        } else {
            ESP_LOGD(TAG, "Created uncached buffer: %u bytes in PSRAM", size);
        }
    }

    /**
     * @brief 析构函数
     */
    ~CachedPSRAMBuffer() {
        // 同步所有脏数据
        if (dirty && cacheBuffer && psramPtr) {
            syncToPSRAM();
        }

        // 释放内存
        if (cacheBuffer) {
            free(cacheBuffer);
        }
        if (psramPtr) {
            free(psramPtr);
        }
    }

    /**
     * @brief 检查缓冲区是否有效
     * @return true如果缓冲区有效
     */
    bool isValid() const {
        return psramPtr != nullptr && (!useCache || cacheBuffer != nullptr);
    }

    /**
     * @brief 从缓冲区读取数据
     * @param offset 偏移量（字节）
     * @param data 输出数据指针
     * @param size 要读取的大小（字节）
     * @return 实际读取的字节数
     */
    size_t read(size_t offset, void* data, size_t size) {
        if (!isValid() || offset >= bufferSize || size == 0) {
            return 0;
        }

        // 限制读取大小不超过缓冲区边界
        size_t actualSize = min(size, bufferSize - offset);

        if (useCache) {
            // 从缓存读取
            memcpy(data, (uint8_t*)cacheBuffer + offset, actualSize);
        } else {
            // 直接从PSRAM读取
            memcpy(data, (uint8_t*)psramPtr + offset, actualSize);
        }

        return actualSize;
    }

    /**
     * @brief 向缓冲区写入数据
     * @param offset 偏移量（字节）
     * @param data 输入数据指针
     * @param size 要写入的大小（字节）
     * @param immediateSync 是否立即同步到PSRAM
     * @return 实际写入的字节数
     */
    size_t write(size_t offset, const void* data, size_t size, bool immediateSync = false) {
        if (!isValid() || offset >= bufferSize || size == 0) {
            return 0;
        }

        // 限制写入大小不超过缓冲区边界
        size_t actualSize = min(size, bufferSize - offset);

        if (useCache) {
            // 写入缓存
            memcpy((uint8_t*)cacheBuffer + offset, data, actualSize);
            dirty = true;

            // 如果设置了立即同步或达到同步阈值，则同步到PSRAM
            if (immediateSync || actualSize >= syncThreshold) {
                syncToPSRAM();
            }
        } else {
            // 直接写入PSRAM
            memcpy((uint8_t*)psramPtr + offset, data, actualSize);
        }

        return actualSize;
    }

    /**
     * @brief 将缓存数据同步到PSRAM
     * @return true如果同步成功
     */
    bool syncToPSRAM() {
        if (!isValid() || !dirty || !useCache) {
            return false;
        }

        memcpy(psramPtr, cacheBuffer, bufferSize);
        dirty = false;

        ESP_LOGD(TAG, "Synced %u bytes to PSRAM", bufferSize);
        return true;
    }

    /**
     * @brief 从PSRAM刷新缓存数据
     * @return true如果刷新成功
     */
    bool refreshFromPSRAM() {
        if (!isValid() || !useCache) {
            return false;
        }

        memcpy(cacheBuffer, psramPtr, bufferSize);
        dirty = false;

        ESP_LOGD(TAG, "Refreshed %u bytes from PSRAM", bufferSize);
        return true;
    }

    /**
     * @brief 获取缓冲区大小
     * @return 缓冲区大小（字节）
     */
    size_t getSize() const {
        return bufferSize;
    }

    /**
     * @brief 检查是否有脏数据
     * @return true如果有脏数据需要同步
     */
    bool isDirty() const {
        return dirty;
    }

    /**
     * @brief 检查是否启用缓存
     * @return true如果启用缓存
     */
    bool isCacheEnabled() const {
        return useCache;
    }

    /**
     * @brief 获取PSRAM指针（直接访问）
     * @warning 直接访问PSRAM会绕过缓存
     */
    void* getPSRAMPointer() const {
        return psramPtr;
    }

    /**
     * @brief 获取缓存指针
     */
    void* getCachePointer() const {
        return cacheBuffer;
    }

    /**
     * @brief 清空缓冲区（设置为0）
     */
    void clear() {
        if (!isValid()) {
            return;
        }

        if (useCache) {
            memset(cacheBuffer, 0, bufferSize);
            dirty = true;
        } else {
            memset(psramPtr, 0, bufferSize);
        }
    }

    /**
     * @brief 获取内存使用统计
     * @param[out] psramUsed PSRAM使用量
     * @param[out] cacheUsed 缓存使用量
     */
    void getMemoryUsage(size_t& psramUsed, size_t& cacheUsed) const {
        psramUsed = (psramPtr != nullptr) ? bufferSize : 0;
        cacheUsed = (cacheBuffer != nullptr) ? bufferSize : 0;
    }

    // 禁用拷贝构造函数和赋值运算符
    CachedPSRAMBuffer(const CachedPSRAMBuffer&) = delete;
    CachedPSRAMBuffer& operator=(const CachedPSRAMBuffer&) = delete;

    // 允许移动语义
    CachedPSRAMBuffer(CachedPSRAMBuffer&& other) noexcept
        : psramPtr(other.psramPtr), cacheBuffer(other.cacheBuffer),
          bufferSize(other.bufferSize), dirty(other.dirty),
          useCache(other.useCache), syncThreshold(other.syncThreshold) {
        other.psramPtr = nullptr;
        other.cacheBuffer = nullptr;
        other.bufferSize = 0;
        other.dirty = false;
    }

    CachedPSRAMBuffer& operator=(CachedPSRAMBuffer&& other) noexcept {
        if (this != &other) {
            // 清理当前资源
            if (dirty && cacheBuffer && psramPtr) {
                syncToPSRAM();
            }
            if (cacheBuffer) free(cacheBuffer);
            if (psramPtr) free(psramPtr);

            // 转移资源
            psramPtr = other.psramPtr;
            cacheBuffer = other.cacheBuffer;
            bufferSize = other.bufferSize;
            dirty = other.dirty;
            useCache = other.useCache;
            syncThreshold = other.syncThreshold;

            // 清空原对象
            other.psramPtr = nullptr;
            other.cacheBuffer = nullptr;
            other.bufferSize = 0;
            other.dirty = false;
        }
        return *this;
    }
};

#endif // CACHED_PSRAM_BUFFER_H