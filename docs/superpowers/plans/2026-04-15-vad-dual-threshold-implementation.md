# 双阈值迟滞VAD算法改进实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现双阈值迟滞VAD算法，解决单阈值算法在嘈杂环境中静音检测被噪音峰值中断的问题

**Architecture:** 使用双阈值和状态机实现迟滞效果，vadSpeechThreshold(0.50)用于语音检测，vadSilenceThreshold(0.30)用于静音确认，状态机在SILENCE和SPEECH状态间切换，只有在SILENCE状态下才开始累积静音时间

**Tech Stack:** ESP32 Arduino框架，C++17，SPIFFS配置文件系统，JSON配置

---

## 文件结构

### 修改现有文件
- `src/config/ConfigData.h` - 扩展AudioConfig结构体，添加新阈值字段
- `src/config/ConfigData.cpp` - 更新验证逻辑
- `src/MainApplication.h` - 添加新成员变量vadSpeechThreshold, vadSilenceThreshold, vadInSpeechState
- `src/MainApplication.cpp` - 修改processAudioData()函数实现双阈值算法，更新initializeAudio()加载新配置
- `src/config/SPIFFSConfigManager.cpp` - 修改loadFromJson()和saveToJson()支持新字段，添加向后兼容性处理

### 配置文件
- `config.json` - 模板文件，添加新字段
- `data/config.json` - 实际配置文件，需要更新以包含新字段

### 测试文件（如果需要）
- 现有测试文件可能需要更新以测试新功能

---

## 实施任务

### Task 1: 更新ConfigData.h中的AudioConfig结构体

**Files:**
- Modify: `src/config/ConfigData.h:81-94`

- [ ] **Step 1: 修改AudioConfig结构体定义**

```cpp
struct AudioConfig {
    uint32_t sampleRate;
    uint8_t bitsPerSample;
    uint8_t channels;
    float vadSpeechThreshold;    // 新增：语音检测阈值
    float vadSilenceThreshold;   // 新增：静音确认阈值
    uint32_t vadSilenceDuration; // 修改：从float改为uint32_t，单位为ms
    String wakeWord;
    float wakeWordSensitivity;
    uint8_t volume;

    AudioConfig() :
        sampleRate(16000), bitsPerSample(16), channels(1),
        vadSpeechThreshold(0.50f), vadSilenceThreshold(0.30f), // 新默认值
        vadSilenceDuration(800), wakeWord("小智小智"),
        wakeWordSensitivity(0.8), volume(80) {}
};
```

- [ ] **Step 2: 验证修改编译通过**

```bash
cd "C:\Users\Admin\Documents\PlatformIO\Projects\test1"
pio run -t check
```

Expected: 编译检查通过，没有语法错误

- [ ] **Step 3: 提交修改**

```bash
git add src/config/ConfigData.h
git commit -m "feat: extend AudioConfig struct with dual VAD thresholds"
```

### Task 2: 更新ConfigData.cpp中的验证逻辑

**Files:**
- Modify: `src/config/ConfigData.cpp:15-25`

- [ ] **Step 1: 修改validate()函数中的音频配置验证**

```cpp
bool SystemConfig::validate() const {
    // ... 其他验证 ...
    
    // 验证音频配置
    if (audio.vadSpeechThreshold < 0.0f || audio.vadSpeechThreshold > 1.0f) {
        Serial.printf("[ERROR] Invalid vadSpeechThreshold: %.2f\n", audio.vadSpeechThreshold);
        return false;
    }
    if (audio.vadSilenceThreshold < 0.0f || audio.vadSilenceThreshold > 1.0f) {
        Serial.printf("[ERROR] Invalid vadSilenceThreshold: %.2f\n", audio.vadSilenceThreshold);
        return false;
    }
    if (audio.vadSilenceThreshold >= audio.vadSpeechThreshold) {
        Serial.printf("[ERROR] vadSilenceThreshold (%.2f) must be less than vadSpeechThreshold (%.2f)\n",
                      audio.vadSilenceThreshold, audio.vadSpeechThreshold);
        return false;
    }
    
    // ... 其他验证 ...
    return true;
}
```

- [ ] **Step 2: 验证修改编译通过**

```bash
pio run -t check
```

Expected: 编译检查通过

- [ ] **Step 3: 提交修改**

```bash
git add src/config/ConfigData.cpp
git commit -m "feat: add validation for dual VAD thresholds"
```

### Task 3: 更新MainApplication.h中的成员变量

**Files:**
- Modify: `src/MainApplication.h:58-63`

- [ ] **Step 1: 修改VAD相关成员变量**

```cpp
// 静音检测
float vadSpeechThreshold;     // VAD语音检测阈值 (0.0-1.0, 默认0.50)
float vadSilenceThreshold;    // VAD静音确认阈值 (0.0-1.0, 默认0.30)
uint32_t vadSilenceDuration;  // 静音持续时间阈值 (ms, 默认800)
bool vadInSpeechState;        // 新增：当前是否在语音状态
bool vadSilenceDetected;      // 是否检测到静音
uint32_t vadSilenceStartTime; // 静音开始时间
uint32_t vadLastAudioTime;    // 最后有音频的时间
```

- [ ] **Step 2: 更新构造函数初始化列表（约第20行）**

找到MainApplication.h中的构造函数声明，确保包含新变量的初始化：

```cpp
MainApplication() : 
    currentState(SystemState::BOOTING),
    stateEntryTime(0),
    audioHardwareAvailable(false),
    vadSpeechThreshold(0.50f),
    vadSilenceThreshold(0.30f),
    vadSilenceDuration(800),
    vadInSpeechState(false),
    vadSilenceDetected(false),
    vadSilenceStartTime(0),
    vadLastAudioTime(0),
    audioBufferPos(0),
    audioCollectionStartTime(0),
    initState(INIT_NONE),
    lastError("") {}
```

- [ ] **Step 3: 验证修改编译通过**

```bash
pio run -t check
```

Expected: 编译检查通过

- [ ] **Step 4: 提交修改**

```bash
git add src/MainApplication.h
git commit -m "feat: add dual VAD threshold member variables"
```

### Task 4: 更新MainApplication.cpp中的initializeAudio()函数

**Files:**
- Modify: `src/MainApplication.cpp:198-203`

- [ ] **Step 1: 修改initializeAudio()函数中的VAD参数加载**

```cpp
// 加载VAD参数
vadSpeechThreshold = configManager.getFloat("audio.vadSpeechThreshold", 0.50f);
vadSilenceThreshold = configManager.getFloat("audio.vadSilenceThreshold", 0.30f);
vadSilenceDuration = configManager.getInt("audio.vadSilenceDuration", 800);
vadInSpeechState = false; // 初始状态：静音

Serial.printf("[VAD] 配置加载: speech_threshold=%.2f, silence_threshold=%.2f, silence_duration=%u ms\n",
             vadSpeechThreshold, vadSilenceThreshold, vadSilenceDuration);
```

- [ ] **Step 2: 验证修改编译通过**

```bash
pio run -t check
```

Expected: 编译检查通过

- [ ] **Step 3: 提交修改**

```bash
git add src/MainApplication.cpp
git commit -m "feat: update initializeAudio() to load dual VAD thresholds"
```

### Task 5: 实现双阈值VAD算法核心逻辑

**Files:**
- Modify: `src/MainApplication.cpp:670-730`

- [ ] **Step 1: 替换processAudioData()函数中的VAD检测逻辑**

找到原代码第672-701行的单阈值逻辑，替换为：

```cpp
float rms_rel = rms / 32768.0;

// 双阈值迟滞状态机
if (rms_rel > vadSpeechThreshold) {
    // 进入或保持语音状态
    vadInSpeechState = true;
    if (vadSilenceDetected) {
        vadSilenceDetected = false;
        Serial.printf("[VAD] Speech detected (rms_rel=%.4f > %.2f), clearing silence flag\n",
                     rms_rel, vadSpeechThreshold);
    }
} else if (rms_rel < vadSilenceThreshold) {
    // 进入或保持静音状态
    vadInSpeechState = false;
    
    if (!vadSilenceDetected) {
        // 第一次检测到静音
        vadSilenceStartTime = currentTime;
        vadSilenceDetected = true;
        Serial.printf("[VAD] Silence start detected at %u ms (speech_thresh=%.2f, silence_thresh=%.2f, actual=%.4f)\n",
                     vadSilenceStartTime, vadSpeechThreshold, vadSilenceThreshold, rms_rel);
    } else {
        // 持续静音
        uint32_t silenceDuration = currentTime - vadSilenceStartTime;
        if (silenceDuration >= vadSilenceDuration) {
            Serial.printf("[VAD] Silence duration reached: %u ms >= %u ms, triggering recognition\n",
                         silenceDuration, vadSilenceDuration);
            shouldTrigger = true;
        }
    }
} else {
    // 中间区域：保持当前状态
    // 不改变vadInSpeechState和vadSilenceDetected
}

// 记录最后音频时间（无论状态）
vadLastAudioTime = currentTime;
```

- [ ] **Step 2: 更新触发识别时的日志（约第729-730行）**

```cpp
Serial.printf("[VAD] Triggering recognition: audio=%zu bytes, in_speech_state=%s, silence_detected=%s, silence_duration=%u ms, has_long_silence=%s\n",
             audioBufferPos, vadInSpeechState ? "yes" : "no", vadSilenceDetected ? "yes" : "no", silenceDuration, hasLongSilence ? "yes" : "no");
```

- [ ] **Step 3: 验证修改编译通过**

```bash
pio run -t check
```

Expected: 编译检查通过

- [ ] **Step 4: 提交修改**

```bash
git add src/MainApplication.cpp
git commit -m "feat: implement dual-threshold hysteresis VAD algorithm"
```

### Task 6: 更新SPIFFSConfigManager支持新配置字段

**Files:**
- Modify: `src/config/SPIFFSConfigManager.cpp:182-183, 280-281, 358-359, 402-410, 522-523, 611-612`

- [ ] **Step 1: 修改loadFromJson()函数中的音频配置加载**

找到约第182行附近的代码：

```cpp
config.audio.vadSpeechThreshold = audio["vadSpeechThreshold"] | 0.50f;
config.audio.vadSilenceThreshold = audio["vadSilenceThreshold"] | 0.30f;
config.audio.vadSilenceDuration = audio["vadSilenceDuration"] | 800;
```

- [ ] **Step 2: 在loadFromJson()中添加向后兼容性处理**

在加载新字段后添加：

```cpp
// 向后兼容性处理：如果存在旧vadThreshold字段但不存在新字段，则迁移配置
if (audio.containsKey("vadThreshold") && !audio.containsKey("vadSpeechThreshold")) {
    float oldThreshold = audio["vadThreshold"] | 0.3f;
    config.audio.vadSpeechThreshold = oldThreshold + 0.2f; // 增加0.2作为语音阈值
    config.audio.vadSilenceThreshold = oldThreshold - 0.1f; // 减少0.1作为静音阈值
    
    // 确保阈值有效
    if (config.audio.vadSpeechThreshold > 1.0f) config.audio.vadSpeechThreshold = 1.0f;
    if (config.audio.vadSilenceThreshold < 0.0f) config.audio.vadSilenceThreshold = 0.0f;
    
    Serial.printf("[CONFIG] Migrated old vadThreshold (%.2f) to dual thresholds: speech=%.2f, silence=%.2f\n",
                 oldThreshold, config.audio.vadSpeechThreshold, config.audio.vadSilenceThreshold);
}
```

- [ ] **Step 3: 修改saveToJson()函数中的音频配置保存**

找到约第280行附近的代码：

```cpp
audio["vadSpeechThreshold"] = config.audio.vadSpeechThreshold;
audio["vadSilenceThreshold"] = config.audio.vadSilenceThreshold;
audio["vadSilenceDuration"] = config.audio.vadSilenceDuration;
```

- [ ] **Step 4: 更新默认配置模板（约第358行）**

```cpp
"vadSpeechThreshold": 0.50,
"vadSilenceThreshold": 0.30,
"vadSilenceDuration": 800,
```

- [ ] **Step 5: 更新配置验证逻辑（约第402行）**

```cpp
if (config.audio.vadSpeechThreshold < 0.0f || config.audio.vadSpeechThreshold > 1.0f) {
    Serial.printf("[ERROR] Invalid vadSpeechThreshold: %.2f\n", config.audio.vadSpeechThreshold);
    return false;
}
if (config.audio.vadSilenceThreshold < 0.0f || config.audio.vadSilenceThreshold > 1.0f) {
    Serial.printf("[ERROR] Invalid vadSilenceThreshold: %.2f\n", config.audio.vadSilenceThreshold);
    return false;
}
if (config.audio.vadSilenceThreshold >= config.audio.vadSpeechThreshold) {
    Serial.printf("[ERROR] vadSilenceThreshold (%.2f) must be less than vadSpeechThreshold (%.2f)\n",
                  config.audio.vadSilenceThreshold, config.audio.vadSpeechThreshold);
    return false;
}
```

- [ ] **Step 6: 更新getString()函数支持新字段（约第522行）**

```cpp
if (path.field == "vadSpeechThreshold") return String(config.audio.vadSpeechThreshold, 2);
if (path.field == "vadSilenceThreshold") return String(config.audio.vadSilenceThreshold, 2);
if (path.field == "vadSilenceDuration") return String(config.audio.vadSilenceDuration);
```

- [ ] **Step 7: 更新setString()函数支持新字段（约第611行）**

```cpp
if (path.field == "vadSpeechThreshold") { config.audio.vadSpeechThreshold = value.toFloat(); return true; }
if (path.field == "vadSilenceThreshold") { config.audio.vadSilenceThreshold = value.toFloat(); return true; }
if (path.field == "vadSilenceDuration") { config.audio.vadSilenceDuration = value.toInt(); return true; }
```

- [ ] **Step 8: 验证修改编译通过**

```bash
pio run -t check
```

Expected: 编译检查通过

- [ ] **Step 9: 提交修改**

```bash
git add src/config/SPIFFSConfigManager.cpp
git commit -m "feat: update SPIFFSConfigManager for dual VAD thresholds with backward compatibility"
```

### Task 7: 更新配置文件模板

**Files:**
- Modify: `config.json:57-58`
- Modify: `data/config.json:57-58`

- [ ] **Step 1: 修改config.json中的音频配置**

```json
"audio": {
  "sampleRate": 16000,
  "bitsPerSample": 16,
  "channels": 1,
  "vadSpeechThreshold": 0.50,
  "vadSilenceThreshold": 0.30,
  "vadSilenceDuration": 800,
  "wakeWord": "小智小智",
  "wakeWordSensitivity": 0.8,
  "volume": 80
}
```

- [ ] **Step 2: 修改data/config.json中的音频配置**

```json
"audio": {
  "sampleRate": 16000,
  "bitsPerSample": 16,
  "channels": 1,
  "vadSpeechThreshold": 0.50,
  "vadSilenceThreshold": 0.30,
  "vadSilenceDuration": 800,
  "wakeWord": "小智小智",
  "wakeWordSensitivity": 0.8,
  "volume": 80
}
```

- [ ] **Step 3: 验证配置文件格式正确**

```bash
python -c "import json; json.load(open('config.json')); print('config.json valid')"
python -c "import json; json.load(open('data/config.json')); print('data/config.json valid')"
```

Expected: 两个文件都显示"valid"消息

- [ ] **Step 4: 提交修改**

```bash
git add config.json data/config.json
git commit -m "chore: update config files with dual VAD thresholds"
```

### Task 8: 完整编译测试

**Files:** 全部修改过的文件

- [ ] **Step 1: 完整编译项目**

```bash
pio run
```

Expected: 编译成功，没有错误

- [ ] **Step 2: 检查编译输出**

```bash
pio run -t size
```

Expected: 显示固件大小信息，确认没有显著增加

- [ ] **Step 3: 运行现有测试（如果有）**

```bash
pio test -e test
```

Expected: 所有现有测试通过

- [ ] **Step 4: 提交最终状态**

```bash
git add -u
git commit -m "build: complete dual VAD threshold implementation"
```

## 测试验证计划

### 验证步骤1: 配置加载验证

1. **重新上传SPIFFS文件系统**
   ```bash
   pio run -t uploadfs
   ```

2. **启动设备并观察串口日志**
   - 预期日志: `[VAD] 配置加载: speech_threshold=0.50, silence_threshold=0.30, silence_duration=800 ms`
   - 预期日志: `[CONFIG] Migrated old vadThreshold (0.30) to dual thresholds: speech=0.50, silence=0.30` (如果使用旧配置)

### 验证步骤2: 安静环境测试

1. **在安静环境中测试**
   - 说短句"你好"
   - 观察VAD日志:
     - `[VAD] Speech detected (rms_rel=0.XX > 0.50), clearing silence flag`
     - `[VAD] Silence start detected at X ms (speech_thresh=0.50, silence_thresh=0.30, actual=0.XX)`
     - `[VAD] Silence duration reached: 800 ms >= 800 ms, triggering recognition`

### 验证步骤3: 嘈杂环境测试

1. **在嘈杂环境中测试**
   - 环境噪音RMS ≈ 0.49-0.96
   - 观察VAD日志:
     - 不应出现频繁的`Speech detected`日志
     - 静音检测不应被频繁中断
     - 最终应能累积800ms静音并触发识别

### 验证步骤4: 边缘情况测试

1. **测试中间区域(rms_rel=0.40)**
   - 预期: 保持当前状态，不切换
   - 验证: 没有状态切换日志

## 成功标准

### 技术成功标准
1. ✅ 编译通过，没有错误
2. ✅ 配置正确加载，显示双阈值
3. ✅ 安静环境中能正常触发识别
4. ✅ 嘈杂环境中静音检测不被噪音峰值中断
5. ✅ 向后兼容性：旧配置能自动迁移

### 日志验证标准
1. ✅ `[VAD] 配置加载: speech_threshold=0.50, silence_threshold=0.30, silence_duration=800 ms`
2. ✅ `[VAD] Speech detected (rms_rel=0.XX > 0.50), clearing silence flag` (说话时)
3. ✅ `[VAD] Silence start detected at X ms (speech_thresh=0.50, silence_thresh=0.30, actual=0.XX)` (静音开始时)
4. ✅ `[VAD] Silence duration reached: XXX ms >= 800 ms, triggering recognition` (触发识别时)
5. ❌ **不应出现**：频繁的`Voice activity detected, clearing silence flag` (旧算法的日志)

## 回滚计划

如果双阈值算法出现问题，可快速回滚到单阈值：

1. **配置文件回滚**：
   ```json
   "audio": {
     "vadThreshold": 0.30,
     "vadSilenceDuration": 800
   }
   ```

2. **代码回滚**：恢复MainApplication.cpp中的processAudioData()函数为单阈值逻辑

3. **重新编译上传**：
   ```bash
   pio run -t clean
   pio run -t upload
   pio run -t uploadfs
   ```

## 注意事项

1. **配置验证**：确保`vadSilenceThreshold < vadSpeechThreshold`，否则配置加载会失败
2. **状态机初始化**：`vadInSpeechState`初始化为false（静音状态）
3. **日志分析**：关注中间区域(rms_rel在0.30-0.50之间)的状态保持行为
4. **性能监控**：双阈值算法应无明显性能影响

---
**计划创建者**: Claude Code  
**创建日期**: 2026年4月15日  
**预计实施时间**: 2-3小时  
**风险等级**: 低（核心算法修改，但影响范围有限）