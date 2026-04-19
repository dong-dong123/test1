# PSRAM优化实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 通过充分利用ESP32-S3的16MB PSRAM，解决test1项目的SSL内存分配失败问题，显著增加内部RAM可用空间，提升系统稳定性。

**Architecture:** 基于xiaozhi-ai-arduino项目的成功经验，保持现有模块架构不变，修改关键内存分配策略，将大内存对象（音频缓冲区、SSL缓冲区、网络缓冲区）迁移到PSRAM，敏感数据保留在内部RAM。

**Tech Stack:** ESP32-S3 Arduino Framework, PlatformIO, mbedTLS SSL, heap_caps API, PSRAM分配优化

---

## 背景分析

### 当前问题
1. **SSL内存分配频繁失败**：内部RAM仅~320KB，SSL连接占用大量内存
2. **音频缓冲区占用内部RAM**：`AudioDriver.cpp`使用`new[]`分配音频缓冲区
3. **SSL碎片整理使用错误内存**：`SSLClientManager.cpp:407`使用`MALLOC_CAP_INTERNAL`
4. **缺乏统一的PSRAM管理**：未充分利用ESP32-S3的16MB PSRAM

### xiaozhi-ai-arduino成功经验
1. **PSRAM优先策略**：关键大内存使用`ps_malloc()`
2. **简化SSL管理**：直接使用`WebSocketsClient`，无复杂连接池
3. **流式处理**：音频数据流式传输，避免大块内存驻留

---

## 文件结构

### 新增文件
```
src/utils/
├── MemoryUtils.h          # 内存分配辅助工具
├── MemoryUtils.cpp       # PSRAM分配实现
└── PSRAMAllocator.h      # STL容器PSRAM分配器（可选）
```

### 修改文件
```
src/modules/
├── SSLClientManager.cpp  # 修改碎片整理内存分配（行407）
└── NetworkManager.cpp    # 添加PSRAM内存监控

src/drivers/
└── AudioDriver.cpp       # 修改音频缓冲区分配（行98）

src/services/
├── VolcanoSpeechService.cpp  # 优化音频处理缓冲区
└── WebSocketSynthesisHandler.cpp  # 优化网络缓冲区

src/MainApplication.cpp   # 添加PSRAM初始化检查
```

### 配置文件
```
platformio.ini           # 验证PSRAM配置，调整mbedTLS设置
```

---

## 实施任务

### Task 1: 内存工具类实现

**Files:**
- Create: `src/utils/MemoryUtils.h`
- Create: `src/utils/MemoryUtils.cpp`

- [ ] **Step 1: 创建内存工具头文件**

```cpp
// src/utils/MemoryUtils.h
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
    static size_t getFreeInternal();
    static size_t getFreePSRAM();
    static size_t getLargestFreePSRAMBlock();
    
    // 检查PSRAM可用性
    static bool isPSRAMAvailable();
    
    // 内存碎片整理（PSRAM）
    static void defragmentPSRAM();
};

// 便捷宏
#define PS_MALLOC(size) MemoryUtils::allocatePSRAM(size, __FILE__)
#define PS_CALLOC(nmemb, size) MemoryUtils::allocatePSRAMClear(nmemb * size, __FILE__)

#endif
```

- [ ] **Step 2: 实现内存工具源文件**

```cpp
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

// 实现其他方法...
```

- [ ] **Step 3: 编译测试工具类**

```bash
pio run
```

期望：编译成功，无错误

- [ ] **Step 4: 提交工具类实现**

```bash
git add src/utils/MemoryUtils.h src/utils/MemoryUtils.cpp
git commit -m "feat: 添加PSRAM内存分配工具类"
```

### Task 2: SSLClientManager优化（最高优先级）

**Files:**
- Modify: `src/modules/SSLClientManager.cpp`

- [ ] **Step 1: 修改碎片整理内存分配（行407）**

```cpp
// 当前代码（第407行）：
blocks[i] = heap_caps_malloc(blockSize, MALLOC_CAP_INTERNAL);

// 修改为：
blocks[i] = MemoryUtils::allocatePSRAM(blockSize, "SSLDefrag");
// 或使用宏：
blocks[i] = PS_MALLOC(blockSize);
```

- [ ] **Step 2: 添加PSRAM内存监控到SSL连接函数**

在`SSLClientManager::getSSLClientForUrl()`中添加：
```cpp
// 在函数开始处添加内存状态日志
ESP_LOGI(TAG, "SSL连接前内存状态: Internal=%u, PSRAM=%u", 
        MemoryUtils::getFreeInternal(), MemoryUtils::getFreePSRAM());
```

- [ ] **Step 3: 优化SSL客户端池内存策略**

考虑为SSL缓冲区预分配PSRAM：
```cpp
// 在createClientPool()中
void* sslContextBuffer = MemoryUtils::allocateSSLBuffer(SSL_CONTEXT_SIZE);
if (sslContextBuffer) {
    // 配置客户端使用PSRAM缓冲区
}
```

- [ ] **Step 4: 测试SSL连接稳定性**

```cpp
// 添加测试代码或使用现有测试
// 连续发起10次HTTPS请求，验证成功率
```

- [ ] **Step 5: 提交SSL模块优化**

```bash
git add src/modules/SSLClientManager.cpp
git commit -m "fix: SSLClientManager使用PSRAM优化内存分配"
```

### Task 3: AudioDriver音频缓冲区优化

**Files:**
- Modify: `src/drivers/AudioDriver.cpp`
- Modify: `src/drivers/AudioDriver.h`

- [ ] **Step 1: 修改音频缓冲区分配（行98）**

```cpp
// 当前代码：
audioBuffer = new uint8_t[config.bufferSize];

// 修改为：
audioBuffer = static_cast<uint8_t*>(
    MemoryUtils::allocateAudioBuffer(config.bufferSize)
);
if (!audioBuffer) {
    ESP_LOGE(TAG, "Failed to allocate audio buffer in PSRAM, falling back to internal RAM");
    audioBuffer = new uint8_t[config.bufferSize];
}
```

- [ ] **Step 2: 修改音频缓冲区释放逻辑**

在`~AudioDriver()`或相应清理函数中：
```cpp
// 修改前：
delete[] audioBuffer;

// 修改后：
if (audioBuffer) {
    // 需要判断是否从PSRAM分配
    // 简单方案：统一使用heap_caps_free
    heap_caps_free(audioBuffer);
    audioBuffer = nullptr;
}
```

- [ ] **Step 3: 测试音频功能**

1. 录音功能测试：录制10秒音频
2. 播放功能测试：播放测试音频
3. 内存监控：记录音频操作前后内存变化

- [ ] **Step 4: 提交音频驱动优化**

```bash
git add src/drivers/AudioDriver.cpp src/drivers/AudioDriver.h
git commit -m "feat: AudioDriver使用PSRAM分配音频缓冲区"
```

### Task 4: 网络缓冲区优化

**Files:**
- Modify: `src/services/VolcanoSpeechService.cpp`
- Modify: `src/services/WebSocketSynthesisHandler.cpp`
- Modify: `src/modules/NetworkManager.cpp`

- [ ] **Step 1: 识别大网络缓冲区**

搜索需要优化的缓冲区：
```bash
# 在项目中搜索大内存分配
grep -n "malloc\|calloc\|new.*\[" src/services/*.cpp | grep -v "//"
```

重点关注：
1. WebSocket接收缓冲区
2. HTTP响应缓冲区
3. 音频数据临时缓冲区

- [ ] **Step 2: 优化VolcanoSpeechService缓冲区**

查找并修改`VolcanoSpeechService.cpp`中的大缓冲区分配：
```cpp
// 示例：音频数据处理缓冲区
std::vector<uint8_t> audioData(MAX_AUDIO_SIZE);
// 考虑使用自定义分配器或直接PSRAM分配
```

- [ ] **Step 3: 优化NetworkManager网络缓冲区**

在`NetworkManager.cpp`中添加：
```cpp
// HTTP响应缓冲区使用PSRAM
void* responseBuffer = MemoryUtils::allocateNetworkBuffer(MAX_RESPONSE_SIZE);
```

- [ ] **Step 4: 测试网络功能**

1. WebSocket连接测试
2. HTTP API调用测试
3. 大数据量传输测试

- [ ] **Step 5: 提交网络缓冲区优化**

```bash
git add src/services/VolcanoSpeechService.cpp src/modules/NetworkManager.cpp
git commit -m "optimize: 网络缓冲区使用PSRAM优化"
```

### Task 5: 系统集成与监控

**Files:**
- Modify: `src/MainApplication.cpp`
- Modify: `platformio.ini`

- [ ] **Step 1: 添加PSRAM初始化检查**

在`MainApplication::setup()`中添加：
```cpp
// PSRAM可用性检查
if (!MemoryUtils::isPSRAMAvailable()) {
    ESP_LOGW("Main", "PSRAM not available or not configured properly");
    // 可以考虑降级策略
} else {
    ESP_LOGI("Main", "PSRAM available: %u bytes free", 
            MemoryUtils::getFreePSRAM());
}

// 打印初始内存状态
MemoryUtils::printMemoryStatus("System Startup");
```

- [ ] **Step 2: 调整PlatformIO配置**

检查并优化`platformio.ini`：
```ini
; 确保PSRAM配置正确
-D BOARD_HAS_PSRAM
-mfix-esp32-psram-cache-issue

; 考虑调整mbedTLS配置
; -DCONFIG_MBEDTLS_INTERNAL_MEM_SIZE=163840  # 评估是否需要调整或移除

; 添加PSRAM优化配置
-D CONFIG_SPIRAM_USE_CAPS_ALLOC=1
-D CONFIG_SPIRAM_USE_MALLOC=1
```

- [ ] **Step 3: 添加定期内存监控**

创建定时任务监控内存状态：
```cpp
xTaskCreate([](void* param) {
    while (true) {
        MemoryUtils::printMemoryStatus("Periodic Check");
        vTaskDelay(pdMS_TO_TICKS(30000)); // 每30秒
    }
}, "MemoryMonitor", 2048, nullptr, 1, nullptr);
```

- [ ] **Step 4: 系统集成测试**

完整语音交互流程测试：
1. 语音识别 → 文本显示
2. 对话服务 → 回复生成
3. 语音合成 → 音频播放
4. 错误恢复测试

- [ ] **Step 5: 提交系统集成**

```bash
git add src/MainApplication.cpp platformio.ini
git commit -m "feat: 添加PSRAM系统集成与监控"
```

### Task 6: 性能测试与优化

**Files:**
- Create: `test/psram_performance_test.cpp` (可选)

- [ ] **Step 1: 基准测试**

建立性能基准：
1. 内部RAM分配速度 vs PSRAM分配速度
2. 音频处理延迟测试
3. SSL连接建立时间对比

- [ ] **Step 2: PSRAM访问优化**

如果发现性能问题：
```cpp
// 实现缓存策略
class CachedPSRAMBuffer {
    void* psramPtr;
    void* cacheBuffer; // 内部RAM缓存
    size_t size;
    bool dirty;
    // 读写分离策略
};
```

- [ ] **Step 3: 内存碎片管理**

实现PSRAM碎片整理：
```cpp
void MemoryUtils::defragmentPSRAM() {
    // 通过分配释放策略减少碎片
    // 定期调用此函数
}
```

- [ ] **Step 4: 压力测试**

长时间运行测试：
1. 24小时连续运行
2. 内存泄漏检测
3. 稳定性监控

- [ ] **Step 5: 性能调优提交**

```bash
git add test/psram_performance_test.cpp
git commit -m "test: 添加PSRAM性能测试与优化"
```

---

## 完整任务列表

1. [ ] **Task 1: 内存工具类实现** - 基础工具
2. [ ] **Task 2: SSLClientManager优化** - 最高优先级
3. [ ] **Task 3: AudioDriver音频缓冲区优化** - 大内存对象
4. [ ] **Task 4: 网络缓冲区优化** - 网络模块
5. [ ] **Task 5: 系统集成与监控** - 整体集成
6. [ ] **Task 6: 性能测试与优化** - 性能保障

---

## 成功度量标准

### 量化目标
1. **内部RAM空闲增长**：从当前可能<100KB增加到>150KB（+50%）
2. **SSL连接成功率**：从可能的不稳定提升到>95%
3. **音频功能无退化**：所有测试用例100%通过
4. **内存泄漏可控**：24小时运行泄漏<50KB

### 监控指标
- 内部RAM空闲量（`heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`）
- PSRAM空闲量（`heap_caps_get_free_size(MALLOC_CAP_SPIRAM)`）
- SSL连接失败率
- 音频处理延迟

---

## 风险与缓解措施

### 风险1: PSRAM分配失败
- **影响**：系统无法获取足够内存
- **缓解**：`MemoryUtils`中的回退机制，降级到内部RAM
- **监控**：记录PSRAM失败率，设置阈值报警

### 风险2: 性能下降
- **影响**：音频延迟增加，用户体验下降
- **缓解**：关键路径使用内部RAM缓存，优化访问模式
- **测试**：基准测试对比优化前后性能

### 风险3: SSL安全性
- **影响**：敏感数据可能进入PSRAM
- **原则**：加密密钥、证书等敏感数据保留在内部RAM
- **验证**：SSL功能测试，安全审计

### 风险4: 内存碎片
- **影响**：PSRAM碎片导致分配失败
- **策略**：定期碎片整理，优化分配大小
- **监控**：跟踪最大连续块大小

---

## 时间估算

| 任务 | 工作量 | 预计时间 | 依赖关系 |
|------|--------|----------|----------|
| Task 1 | 低 | 2小时 | 无 |
| Task 2 | 中 | 3小时 | Task 1 |
| Task 3 | 低 | 2小时 | Task 1 |
| Task 4 | 中 | 4小时 | Task 1 |
| Task 5 | 低 | 2小时 | Task 2-4 |
| Task 6 | 高 | 5小时 | Task 1-5 |
| **总计** | | **18小时** | |

**实施周期**：3-4天（每天4-6小时）

---

## 更新记录

**2026-04-19** - 初始计划创建
- 基于xiaozhi-ai-arduino项目成功经验分析
- 识别test1项目PSRAM使用不足问题
- 制定6个任务的详细实施计划

---

## 执行交接

**计划已完成并保存至 `docs/superpowers/plans/2026-04-19-psram-optimization-implementation.md`**

**执行选项：**

**1. 子代理驱动（推荐）** - 为每个任务派遣新的子代理，任务间进行审查，快速迭代

**2. 内联执行** - 在当前会话中使用executing-plans按任务执行

**请选择执行方式并开始实施。**