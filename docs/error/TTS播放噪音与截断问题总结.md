# TTS播放噪音与截断问题总结（2026-04-24）

## 📊 问题演进时间线

### 初始状态：喇叭无声
- **症状**：TTS合成成功后喇叭无任何声音输出
- **根因**：`playTask` 创建时 SRAM 耗尽（SSL/HTTP 占用大量 SRAM 后无剩余空间创建任务）

### 第一轮修复：有声音但有噪音+截断

**修复内容**：
- 预创建 playTask（初始化时创建，避免内存紧张时动态创建失败）
- 环形缓冲区 256KB → 1MB
- 增加 playTask 栈 4096 → 8192
- 栈数组改为 heap_caps_malloc 堆分配
- 添加 16→32 bit PCM 转换（`sample << 16`）
- 添加 I2S DMA 缓冲区清零
- 添加 WAV 头部剥离
- 添加播放完成自动检测（dataSeen 标志）

**修复后症状**：
- 有声音输出，但有明显噪音
- 音频截断：只播放了约 20-30% 的内容（如只播到"查天气"就停止）
- 但比以前"好了一点点"

### 第二轮修复（本次）：两问题均有改善但仍未完全解决

**修复内容**：
- 禁用 APLL（`use_apll = false`）
- 播放完成前添加 100ms DMA 排空延迟
- 音量增益控制（`config.volume / 100.0f`，默认 80%）
- I2S 写入错误不再 break 循环
- 添加每 64KB 进度日志

**修复后症状**：
- 噪音仍存在，但有所减轻
- 截断仍存在，音频从 13.54 秒只播了 9.87 秒（约 73%）

## 🔍 根因分析

### 核心根因：playTask 栈溢出（已修复）

playTask 栈仅 4096 字节，但包含：
- 栈上数组：tempBuffer[512] + convertedBuffer[1024] = 1536 字节
- `i2s_write()` 内部调用链需要大量栈空间
- ESP-IDF 日志格式化也需要栈空间

**影响**：
- 栈溢出破坏邻近内存（`isPlaying` 标志、环形缓冲区指针等）
- 数据损坏 → 噪音
- `isPlaying` 标志被破坏 → 内层循环提前退出 → 截断

**修复**：栈 4096 → 8192，数组改为堆分配 ✓（已有改善）

### 持续噪音的可能原因

1. **APLL 时钟抖动**（已修复：`use_apll = false`）
   - APLL 可能引入时钟抖动，在 I2S DAC 输出端表现为可闻噪声
   - 改用主 PLL 时钟理论上更干净

2. **信号幅度过高/削波**（已修复：添加音量增益控制）
   - 16-bit PCM 满量程（±32767）左移 16 位后可能超过 MAX98357A 的线性输入范围
   - 添加 `config.volume / 100.0f` 增益系数（默认 80%）

3. **I2S 通信格式不匹配**（未验证）
   - MAX98357A 的实际模式取决于 GAIN/SD_MODE 引脚的硬件配置
   - 当前使用 `I2S_COMM_FORMAT_STAND_I2S`（标准 I2S 模式）
   - 如果硬件配置为左对齐模式（Left-Justified），需要改用 `I2S_COMM_FORMAT_STAND_MSB`
   - **待验证**：需确认硬件引脚配置

4. **I2S 双工模式串扰**（未修复）
   - I2S 配置为 RX|TX 双工模式，即使 recordTask 已停止，RX 通道仍启用
   - 麦克风 INMP441 在播放期间仍在 SDO 线上输出数据
   - 可能通过共享 BCLK/WS 线或电源耦合引入噪声

### 持续截断的可能原因

1. **playTask 在 I2S DMA 完全排空前已标记完成**
   - playTask 写完最后一块数据到 DMA 后立即检测到缓冲区空（`readAudioData` 返回 0）
   - 设置 `isPlaying = false` → 状态机进入 IDLE
   - 但 DMA 缓冲区中仍有约 64ms 的音频数据未输出
   - **修复已应用**：添加了 100ms DMA 排空延迟

2. **I2S 写入错误导致数据丢失**
   - `i2s_write` 返回错误时原本 `break` 退出内层循环
   - `isPlaying` 仍为 true → 状态机卡在 PLAYING 直到 60 秒超时
   - 超时后 `stopPlay()` 设置 `isPlaying = false` → 进入 IDLE
   - **修复已应用**：错误不再 break，改为继续处理下一块

3. **playTask 被 WiFi/FreeRTOS 任务抢占**
   - playTask 运行在核心 0，可能与 WiFi 栈共享核心
   - 如果 WiFi 任务长时间占用 CPU，I2S DMA 可能欠载运行
   - `tx_desc_auto_clear = true` 在欠载时自动发送静音

4. **样本率不匹配**
   - TTS 请求 rate=16000，但 I2S 实际时钟可能偏差
   - 对账：433196 字节 ÷ 2 ÷ 16000 = 13.54 秒，实际播放 9.87 秒
   - 差值 3.67 秒（27%）需进一步诊断

## 🛠️ 所有已实施修复措施

### 文件修改清单

| 文件 | 修改内容 |
|------|----------|
| `platformio.ini` | 添加 `-DCONFIG_ARDUINO_LOOP_STACK_SIZE=32768`（历史） |
| `src/drivers/AudioDriver.h` | 添加 `clearDMABuffers()`；添加 `resetBuffer()`；`bufferSize` 256KB→1MB |
| `src/drivers/AudioDriver.cpp` | 见下方详细列表 |
| `src/services/VolcanoSpeechService.cpp` | 添加 WAV 头剥离；修复日志信息 |
| `src/MainApplication.cpp` | PLAYING 状态添加完成检测；播放前 `resetBuffer()` |

### AudioDriver.cpp 详细修改

| 行号 | 修改 | 目的 |
|------|------|------|
| 31 | `I2S_MODE_RX \| I2S_MODE_TX` | I2S 双工模式 |
| 39 | `use_apll = true` → `false` | 消除 APLL 时钟抖动噪声 |
| 121 | 栈 4096 → 8192 | 防止 playTask 栈溢出 |
| 378-387 | 新增 `clearDMABuffers()` | 播放前清 DMA 残留 |
| 401 | startPlay 调用 clearDMABuffers | 清除残余数据 |
| 1317-1323 | 栈数组 → `heap_caps_malloc` | 消除栈压力 |
| 1336-1397 | 重写 playTask 内层循环 | 见下方详细说明 |
| 1356-1362 | 添加音量增益 `sample * gain` | 避免信号过载削波 |
| 1370-1374 | I2S 错误 `break` → `continue` | 防止错误导致播放中断 |
| 1379-1390 | 添加 100ms DMA 排空延迟 | 确保 DMA 完全输出后再标记完成 |

### playTask 核心播放循环重写说明

```cpp
// 修复前（产生噪音和截断）：
while (driver->isPlaying) {
    size_t bytesRead = driver->readAudioData(tempBuffer, 512);
    if (bytesRead > 0) {
        // 16-bit PCM → 32-bit PCM
        for (size_t i = 0; i < sampleCount; i++) {
            convertedBuffer[i] = (int32_t)((int16_t*)tempBuffer)[i] << 16;
        }
        i2s_write(I2S_NUM_0, convertedBuffer, bytesToWrite, &bytesWritten, portMAX_DELAY);
    }
}

// 修复后：
while (driver->isPlaying) {
    size_t bytesRead = driver->readAudioData(tempBuffer, 512);
    if (bytesRead > 0) {
        dataSeen = true;
        totalPlayed += bytesRead;
        // 64KB 边界进度日志
        if ((totalPlayed >> 16) != ((totalPlayed - bytesRead) >> 16)) {
            ESP_LOGI(TAG, "Play progress: %u bytes", totalPlayed);
        }
        // 应用音量控制
        float gain = driver->config.volume / 100.0f;
        for (size_t i = 0; i < sampleCount; i++) {
            int16_t sample = ((int16_t*)tempBuffer)[i];
            convertedBuffer[i] = (int32_t)(sample * gain) << 16;
        }
        i2s_write(I2S_NUM_0, convertedBuffer, bytesToWrite, &bytesWritten, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "I2S write error: %s, continuing", esp_err_to_name(err));
            // 不再 break，防止数据丢失
        }
    } else if (dataSeen) {
        // 缓冲区已空 → 等 DMA 排空 → 标记完成
        vTaskDelay(pdMS_TO_TICKS(100));  // ← 新增排空延迟
        driver->isPlaying = false;
        driver->resetBuffer();
        i2s_zero_dma_buffer(I2S_NUM_0);
    } else {
        vTaskDelay(10 / portTICK_PERIOD_MS);  // 等待数据写入
    }
}
```

### VolcanoSpeechService.cpp 详细修改

| 行号 | 修改 | 目的 |
|------|------|------|
| 1297-1300 | 动态 JSON 容量 2MB→3MB | 防止 >2MB 响应体解析失败 |
| 1377-1407 | 新增 WAV 头剥离 | RIFF 头部检测和 data 块提取 |
| 1399 | 修复日志信息 | 正确显示原始/剥离后大小 |

### MainApplication.cpp 详细修改

| 行号 | 修改 | 目的 |
|------|------|------|
| 510-519 | PLAYING 状态添加完成检测 | `!isPlayingActive()` → 自动 IDLE |
| 1175-1176 | 播放前 resetBuffer | 防止残留数据混入新音频 |
| 1179-1191 | `std::vector().swap()` 释放 SRAM | 大幅降低播放时 SRAM 占用 |

## 📝 验证日志关键数据

```
合成成功: 433196 字节  (= 13.54秒 16-bit PCM @ 16kHz)
开始播放: [81424]
状态变更 PLAYING→IDLE: [91291]
播放时长: 91291 - 81424 = 9867ms (= 9.87秒)
缺失: 13.54 - 9.87 = 3.67秒 (= 27%)
```

## ❓ 未解决问题（需进一步排查）

### 1. I2S 通信格式
- 当前：`I2S_COMM_FORMAT_STAND_I2S`（标准 I2S 模式）
- 待尝试：`I2S_COMM_FORMAT_STAND_MSB`（左对齐模式）
- 取决于 MAX98357A 的 GAIN/SD_MODE 引脚硬件配置

### 2. 播放进度确认
- 当前固件已添加每 64KB 进度日志
- 上传后通过日志可确认 playTask 实际处理了多少字节
- 如果 totalPlayed 等于 synthesized 大小 → 问题在 I2S DMA→DAC 链路
- 如果 totalPlayed 小于 synthesized 大小 → 问题在 playTask 读取/处理环节

### 3. I2S 双工模式影响
- 考虑在播放时临时切换到 TX-only 模式，排除 RX 串扰

### 4. PSRAM 访问延迟
- 音频缓冲区位于 PSRAM，高延迟可能影响 DMA 时序
- 考虑在播放时将音频拷贝到内部 SRAM（如果空间允许）

## 🔧 待测试修复方案

```cpp
// 方案1：切换到左对齐模式（如果硬件配置为 Left-Justified）
.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),  // 仅 TX 模式
// .communication_format = I2S_COMM_FORMAT_STAND_I2S,
.communication_format = I2S_COMM_FORMAT_STAND_MSB,    // 左对齐
```

```cpp
// 方案2：播放临时切换到 TX-only 模式
i2s_set_clk(I2S_NUM_0, 16000, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_MONO);
```
