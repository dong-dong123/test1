#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <Arduino.h>
#include <esp_heap_caps.h>

class MemoryUtils {
public:
    // PSRAM分配（首选），失败时回退到内部RAM
    static void* allocatePSRAM(size_t size, const char* tag = "");
    static void* allocatePSRAMClear(size_t size, const char* tag = ""); // calloc版本

    // 专用分配器：音频数据、网络缓冲区等
    static void* allocateAudioBuffer(size_t size);
    static void* allocateNetworkBuffer(size_t size);
    static void* allocateSSLBuffer(size_t size); // SSL非敏感数据

    // 内存诊断工具
    static void printMemoryStatus(const char* tag = "");
    static void printDetailedMemoryStatus(const char* tag = "");  // 增强版内存状态
    static size_t getFreeInternal();
    static size_t getFreePSRAM();
    static size_t getTotalInternal();          // 总内部SRAM
    static size_t getTotalPSRAM();             // 总PSRAM
    static size_t getLargestFreeInternalBlock(); // 内部SRAM最大空闲块
    static size_t getLargestFreePSRAMBlock();
    static size_t getMinFreeHeap();            // 历史最小空闲堆
    static void logHeapUsage(const char* tag = ""); // 打印堆使用概览

    // 检查PSRAM可用性
    static bool isPSRAMAvailable();
    static bool verifyPSRAMAllocation(size_t testSize = 1024);  // 验证PSRAM分配能力
    static void printPSRAMStatus(const char* tag = "");         // 打印PSRAM详细状态

    // 内存碎片整理（PSRAM）
    static void defragmentPSRAM();

    // 增强的内存碎片管理
    static size_t getFragmentationScore();  // 获取碎片化评分（0-100，越高越碎片化）
    static bool needsDefragmentation();     // 检查是否需要碎片整理
    static void smartDefragmentPSRAM();     // 智能碎片整理（根据碎片程度调整策略）
    static void periodicDefragmentationCheck(); // 周期性碎片检查（应在主循环中调用）

    // 栈使用监控
    static void monitorTaskStacks(const char* tag = ""); // 监控所有任务栈使用情况
    static size_t getTaskStackHighWaterMark(TaskHandle_t task = nullptr); // 获取任务栈高水位标记
    static void printTaskStackInfo(const char* tag = ""); // 打印所有任务栈信息
    static size_t getTotalStackUsage(); // 获取总栈使用量（估算）
    static size_t getPeakStackUsage(); // 获取峰值栈使用量
};

// 便捷宏
#define PS_MALLOC(size) MemoryUtils::allocatePSRAM(size, __FILE__)
#define PS_CALLOC(nmemb, size) MemoryUtils::allocatePSRAMClear(nmemb * size, __FILE__)

#endif // MEMORY_UTILS_H