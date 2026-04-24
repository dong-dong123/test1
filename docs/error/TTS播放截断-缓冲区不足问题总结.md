# TTS播放截断问题总结 — 环形缓冲区不足（2026-04-25）

## 问题描述

TTS合成长文本时，语音播放只播了开头一小段就结束了，约55秒的音频只播放了前5秒。

### 复现步骤

1. 用户提问触发较长的大模型回复
2. Coze对话服务返回完整文本
3. 火山TTS成功合成长为884654字节（~55秒@16kHz 16-bit）的PCM音频
4. 播放时只输出前~160KB就结束，剩余音频被丢弃

### 实际日志

```
合成成功: 884654 字节（~55秒音频）
Buffer state: readPos=0 writePos=159999 bufferSize=160000
Progress: 65536 bytes (readPos=65536 writePos=159999)
Progress: 131072 bytes (readPos=131072 writePos=159999)
Playback END: totalPlayed=159999, readPos=0 writePos=0
```

## 根因分析

### 直接原因：环形缓冲区容量不足

`MAIN_AUDIO_BUFFER_SIZE = 160000`（约160KB），对于16kHz 16-bit PCM音频来说仅能容纳约5秒。

`writeAudioData()` 的逻辑是：一次最多写入 `min(freeSpace, length)` 字节。当TTS合成输出884654字节、缓冲区只有160KB时：

| 步骤 | 操作 | 说明 |
|------|------|------|
| 1 | `writeAudioData(data, 884654)` | 尝试写入全部音频 |
| 2 | `freeSpace = 160000 - 0 = 160000` | 初始为空闲160KB |
| 3 | `toWrite = min(160000, 884654) = 160000` | 实际只写入160KB |
| 4 | 剩余724654字节被静默丢弃 | **关键问题点** |
| 5 | playTask播放完160KB后缓冲区空 | 播放结束，totalPlayed=159999 |

### 根本原因

设计上假设"缓冲区够大，一次写入全部音频"，但没有考虑长文本TTS输出超过缓冲区容量的情况。旧缓冲区160KB仅约5秒音频，对于完整的对话回复（10-60秒）远远不够。

### 关联因素

- `CORE_DEBUG_LEVEL=1` 屏蔽了 `ESP_LOGW` 和 `ESP_LOGI` 输出，导致 `writeAudioData` 中buffer full的警告日志在常规日志级别下不可见，增加了排查难度

## 修复方案

### 修复1：扩大缓冲区（MainApplication.h:53）

```cpp
// 旧值
static const size_t MAIN_AUDIO_BUFFER_SIZE = 160000;  // ~5秒

// 新值
static const size_t MAIN_AUDIO_BUFFER_SIZE = 1048576;  // 1MB PSRAM缓冲区（~65秒）
```

1MB利用PSRAM（16MB PSRAM on ESP32-S3 N16R8），足以覆盖绝大多数TTS场景。

### 修复2：边播边写模式（MainApplication.cpp:1183-1207）

当一次写入不完全时（`written < audioSize`），启动播放后台继续写入剩余数据：

```cpp
if (written < audioSize) {
    audioDriver.startPlay();
    while (written < audioSize) {
        size_t n = writeAudioData(data + written, audioSize - written);
        if (n > 0) {
            written += n;
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));  // 缓冲区满时等待播放消耗数据
        }
    }
}
```

### 修复3：playTask日志迁移到Serial.printf（辅助诊断）

将 `ESP_LOG*` 改为 `Serial.printf`，使其在 `CORE_DEBUG_LEVEL=1` 下仍然可见。

## 验证结果

### 修复前（日志摘要）

```
合成成功: 884654 字节
Buffer state: readPos=0 writePos=159999 bufferSize=160000
Playback END: totalPlayed=159999
播放占比: 159999/884654 = 18%（截断82%）
```

### 修复后（实际测试日志）

```
合成成功: 306466 字节
Buffer state: readPos=0 writePos=306466 bufferSize=1048576
Progress: 65536 → 131072 → 196608 → 262144 → 306466
Playback END: totalPlayed=306466
播放占比: 306466/306466 = 100%（完整播放✅）
```

306KB音频完整播放，bufferSize=1048576远高于实际数据量，Playback END时的totalPlayed等于写入量。

## 涉及文件

| 文件 | 修改 | 行号 |
|------|------|------|
| `src/MainApplication.h` | `MAIN_AUDIO_BUFFER_SIZE`: 160000 → 1048576 | :53 |
| `src/MainApplication.cpp` | playResponse()添加边播边写fallback | :1183-1207 |
| `src/drivers/AudioDriver.cpp` | playTask日志 `ESP_LOG` → `Serial.printf` | 多处 |

## 经验教训

1. **缓冲区设计必须覆盖最大负载，不能仅凭"典型场景"决定大小** — 典型的"今天天气怎么样"回复短，但复杂的对话可能产生数十秒的回复
2. `writeAudioData()` 返回值必须检查并处理不完全写入的情况 — 当前修复前是静默丢弃剩余数据
3. **调试日志级别** — 关键路径（如buffer full、写入截断）应该用 `Serial.printf` 或 `ESP_LOGE`，确保在任意日志级别下都能看到
