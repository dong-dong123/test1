#ifndef PSRAM_ALLOCATOR_H
#define PSRAM_ALLOCATOR_H

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <memory>
#include <type_traits>

/**
 * @class PSRAMAllocator
 * @brief STL兼容的PSRAM分配器，允许STL容器使用PSRAM内存
 *
 * 这个分配器可以用于std::vector、std::list等STL容器，
 * 使它们能够使用PSRAM而不是内部RAM。
 */
template <typename T>
class PSRAMAllocator {
private:
    static const char* TAG;

public:
    // STL分配器要求的类型定义
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    // STL分配器要求的rebind模板
    template <typename U>
    struct rebind {
        using other = PSRAMAllocator<U>;
    };

    /**
     * @brief 默认构造函数
     */
    PSRAMAllocator() noexcept {
        ESP_LOGD(TAG, "PSRAMAllocator created for type: %s", typeid(T).name());
    }

    /**
     * @brief 拷贝构造函数
     */
    template <typename U>
    PSRAMAllocator(const PSRAMAllocator<U>&) noexcept {}

    /**
     * @brief 分配内存
     * @param n 元素数量
     * @return 指向分配内存的指针
     */
    pointer allocate(size_type n) {
        if (n == 0) {
            return nullptr;
        }

        // 计算总字节数
        size_type totalBytes = n * sizeof(T);

        // 尝试在PSRAM中分配内存
        pointer ptr = static_cast<pointer>(heap_caps_malloc(totalBytes, MALLOC_CAP_SPIRAM));
        if (!ptr) {
            // PSRAM分配失败，回退到内部RAM
            ESP_LOGW(TAG, "PSRAM allocation failed for %u bytes of type %s, falling back to internal RAM",
                    totalBytes, typeid(T).name());
            ptr = static_cast<pointer>(malloc(totalBytes));
        }

        if (ptr) {
            ESP_LOGD(TAG, "Allocated %u bytes for %u elements of type %s at %p",
                    totalBytes, n, typeid(T).name(), ptr);
        } else {
            ESP_LOGE(TAG, "Allocation failed for %u bytes (%u elements of type %s)",
                    totalBytes, n, typeid(T).name());
            throw std::bad_alloc();
        }

        return ptr;
    }

    /**
     * @brief 释放内存
     * @param ptr 要释放的内存指针
     * @param n 元素数量（可选，用于调试）
     */
    void deallocate(pointer ptr, size_type n = 0) noexcept {
        if (ptr) {
            free(ptr);
            if (n > 0) {
                ESP_LOGD(TAG, "Deallocated %u elements of type %s at %p",
                        n, typeid(T).name(), ptr);
            }
        }
    }

    /**
     * @brief 构造对象
     * @param ptr 对象指针
     * @param args 构造函数参数
     */
    template <typename U, typename... Args>
    void construct(U* ptr, Args&&... args) {
        new (ptr) U(std::forward<Args>(args)...);
    }

    /**
     * @brief 销毁对象
     * @param ptr 对象指针
     */
    template <typename U>
    void destroy(U* ptr) {
        ptr->~U();
    }

    /**
     * @brief 获取最大可分配大小
     * @return 最大可分配的元素数量
     */
    size_type max_size() const noexcept {
        // 返回理论最大值（受size_type限制）
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    /**
     * @brief 获取分配器内存使用情况
     * @return PSRAM中的空闲内存（字节）
     */
    static size_t getFreePSRAM() {
        return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    }

    /**
     * @brief 检查PSRAM是否可用
     * @return true如果PSRAM可用
     */
    static bool isPSRAMAvailable() {
        return getFreePSRAM() > 0;
    }

    /**
     * @brief 获取分配器统计信息
     * @param[out] totalAllocated 总分配字节数
     * @param[out] totalFreed 总释放字节数
     * @note 这是一个简单的实现，实际项目中可能需要更复杂的跟踪
     */
    static void getStats(size_t& totalAllocated, size_t& totalFreed) {
        // 简单实现：返回0
        totalAllocated = 0;
        totalFreed = 0;
    }
};

// 静态成员初始化
template <typename T>
const char* PSRAMAllocator<T>::TAG = "PSRAMAllocator";

/**
 * @brief 比较两个分配器是否相等
 * @details STL要求相同类型的分配器总是相等
 */
template <typename T1, typename T2>
bool operator==(const PSRAMAllocator<T1>&, const PSRAMAllocator<T2>&) noexcept {
    return true;
}

template <typename T1, typename T2>
bool operator!=(const PSRAMAllocator<T1>& lhs, const PSRAMAllocator<T2>& rhs) noexcept {
    return !(lhs == rhs);
}

// 便捷类型别名
template <typename T>
using psram_vector = std::vector<T, PSRAMAllocator<T>>;

template <typename T>
using psram_list = std::list<T, PSRAMAllocator<T>>;

template <typename K, typename V, typename Compare = std::less<K>>
using psram_map = std::map<K, V, Compare, PSRAMAllocator<std::pair<const K, V>>>;

template <typename T, typename Compare = std::less<T>>
using psram_set = std::set<T, Compare, PSRAMAllocator<T>>;

#endif // PSRAM_ALLOCATOR_H