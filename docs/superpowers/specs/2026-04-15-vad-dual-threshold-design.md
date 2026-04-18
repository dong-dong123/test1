# 双阈值迟滞VAD算法改进设计

## 问题分析

### 当前问题
基于2026-04-15测试日志，单能量阈值VAD在嘈杂环境中失效：

1. **阈值0.6时**：语音检测不触发（阈值太高，用户反馈）
2. **阈值0.3时**：`hasVoice`总是true（阈值太低，噪音误判）
3. **环境噪音RMS**：0.49-0.96（高于0.30阈值）
4. **静音检测被中断**：频繁出现`[VAD] Voice activity detected, clearing silence flag`
5. **从未触发**：没有`[VAD] Silence duration reached`日志

### 根本原因
单能量阈值VAD算法在持续噪音环境中效果差：
- 环境噪音RMS(0.49-0.96) > 阈值0.30 → `hasVoice=true`几乎一直成立
- 噪音峰值中断静音累积 → 无法达到1200ms静音要求
- 单阈值无法区分短暂噪音峰值和持续语音

## 解决方案：双阈值迟滞法

### 核心原理
使用两个阈值和状态机实现迟滞效果：

1. **语音检测阈值** (vadSpeechThreshold = 0.50)：开始认为有语音
2. **静音确认阈值** (vadSilenceThreshold = 0.30)：确认为静音
3. **状态机**：`SILENCE` → (能量>0.50) → `SPEECH` → (能量<0.30) → `SILENCE`

### 状态机设计
```
初始状态: SILENCE
    ↓ (rms_rel > vadSpeechThreshold)
进入SPEECH状态
    ↓ (rms_rel < vadSilenceThreshold) 
进入SILENCE状态，开始静音计时
    ↓ (静音持续vadSilenceDuration ms)
触发语音识别
```

### 预期效果
- **噪音峰值** (0.49)：不会触发语音状态（需要>0.50）
- **静音确认**：需要<0.30才确认，避免噪音误判
- **静音累积**：只有在SILENCE状态下才能累积静音时间
- **迟滞效应**：防止在阈值附近频繁切换状态

## 技术设计

### 配置扩展

#### 配置文件修改
**config.json** 和 **data/config.json**:
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

**变更说明**:
1. 移除旧的`vadThreshold`字段
2. 添加两个新阈值字段
3. 保持`vadSilenceDuration`字段（可配置为800ms）

### 数据结构修改

#### 1. ConfigData.h
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

#### 2. MainApplication.h
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

### 核心算法修改

#### MainApplication.cpp - processAudioData()函数

**当前逻辑**（单阈值）：
```cpp
bool hasVoice = (rms / 32768.0) > vadThreshold;
if (hasVoice) {
    // 有语音活动
    vadLastAudioTime = currentTime;
    if (vadSilenceDetected) {
        vadSilenceDetected = false;
    }
} else {
    // 静音段
    if (!vadSilenceDetected) {
        vadSilenceStartTime = currentTime;
        vadSilenceDetected = true;
    } else {
        uint32_t silenceDuration = currentTime - vadSilenceStartTime;
        if (silenceDuration >= vadSilenceDuration) {
            shouldTrigger = true;
        }
    }
}
```

**新逻辑**（双阈值迟滞）：
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

### 配置管理修改

#### SPIFFSConfigManager.cpp

**加载配置**（修改`loadFromJson()`）：
```cpp
config.audio.vadSpeechThreshold = audio["vadSpeechThreshold"] | 0.50f;
config.audio.vadSilenceThreshold = audio["vadSilenceThreshold"] | 0.30f;
config.audio.vadSilenceDuration = audio["vadSilenceDuration"] | 800;
```

**保存配置**（修改`saveToJson()`）：
```cpp
audio["vadSpeechThreshold"] = config.audio.vadSpeechThreshold;
audio["vadSilenceThreshold"] = config.audio.vadSilenceThreshold;
audio["vadSilenceDuration"] = config.audio.vadSilenceDuration;
```

**验证配置**（修改`validate()`）：
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

### 初始化修改

#### MainApplication.cpp - initializeAudio()
```cpp
// 加载VAD参数
vadSpeechThreshold = configManager.getFloat("audio.vadSpeechThreshold", 0.50f);
vadSilenceThreshold = configManager.getFloat("audio.vadSilenceThreshold", 0.30f);
vadSilenceDuration = configManager.getInt("audio.vadSilenceDuration", 800);
vadInSpeechState = false; // 初始状态：静音

Serial.printf("[VAD] 配置加载: speech_threshold=%.2f, silence_threshold=%.2f, silence_duration=%u ms\n",
             vadSpeechThreshold, vadSilenceThreshold, vadSilenceDuration);
```

## 兼容性处理

### 向后兼容性
1. **配置迁移**：在SPIFFSConfigManager中添加旧配置迁移逻辑
2. **默认值**：如果旧配置存在`vadThreshold`，将其同时用于两个新阈值
3. **逐步升级**：用户可以先只修改配置文件，稍后更新固件

### 迁移代码示例
```cpp
// 在loadFromJson()中处理旧配置
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

## 测试计划

### 测试场景
1. **场景A：安静环境测试**
   - 环境噪音RMS < 0.30
   - 预期：静音检测正常，说完话后800ms触发识别

2. **场景B：嘈杂环境测试**
   - 环境噪音RMS = 0.49-0.96
   - 预期：噪音峰值不会触发语音状态，静音检测能正常累积

3. **场景C：语音测试**
   - 说话时RMS > 0.50
   - 预期：正确检测语音，说完后能切换到静音状态

### 关键日志验证
1. ✅ `[VAD] 配置加载: speech_threshold=0.50, silence_threshold=0.30, silence_duration=800 ms`
2. ✅ `[VAD] Speech detected (rms_rel=0.XX > 0.50), clearing silence flag`
3. ✅ `[VAD] Silence start detected at X ms (speech_thresh=0.50, silence_thresh=0.30, actual=0.XX)`
4. ✅ `[VAD] Silence duration reached: XXX ms >= 800 ms, triggering recognition`
5. ❌ **不应出现**：频繁的`Voice activity detected, clearing silence flag`

### 性能指标
1. **状态切换频率**：应显著减少（对比单阈值）
2. **静音累积成功率**：应能完整累积800ms静音
3. **识别触发可靠性**：应在说完话后800ms内触发识别

## 风险评估

### 技术风险
1. **状态机死锁**：中间区域(rms_rel在0.30-0.50之间)可能导致状态不更新
   - 缓解：添加超时机制，长时间在中间区域则重置状态
2. **阈值配置错误**：用户可能设置`vadSilenceThreshold >= vadSpeechThreshold`
   - 缓解：添加配置验证逻辑
3. **内存增加**：新增状态变量增加内存使用
   - 评估：仅增加1个bool和2个float，影响可忽略

### 兼容性风险
1. **旧配置文件**：需要处理只包含`vadThreshold`的旧配置
   - 缓解：实现自动迁移逻辑
2. **固件版本**：新旧固件配置不兼容
   - 缓解：保持向后兼容，逐步淘汰旧字段

### 性能风险
1. **计算开销**：双阈值比较vs单阈值
   - 评估：仅增加一次浮点比较，开销可忽略
2. **状态维护**：需要维护额外状态变量
   - 评估：状态机逻辑简单，不影响实时性

## 实施步骤

### 第一阶段：设计验证（1小时）
1. 创建测试配置文件
2. 模拟算法逻辑，验证状态机正确性
3. 编写单元测试用例

### 第二阶段：代码实现（2小时）
1. 修改`ConfigData.h` - 添加新字段
2. 修改`MainApplication.h` - 添加新成员变量
3. 修改`MainApplication.cpp` - 实现双阈值算法
4. 修改`SPIFFSConfigManager.cpp` - 支持新配置
5. 更新配置文件模板

### 第三阶段：测试验证（1小时）
1. 编译并上传固件
2. 测试安静环境场景
3. 测试嘈杂环境场景
4. 分析日志，验证算法效果

### 第四阶段：文档更新（0.5小时）
1. 更新配置文件文档
2. 更新VAD参数调优指南
3. 更新错误文档

## 成功标准

### 技术成功标准
1. **静音检测正常**：能在说完话后800ms内触发识别
2. **噪音鲁棒性**：环境噪音峰值不会中断静音累积
3. **配置灵活性**：支持独立调整两个阈值
4. **向后兼容**：旧配置文件能正常工作

### 用户体验标准
1. **响应速度**：说完话后800ms内开始识别
2. **准确性**：不会因环境噪音误触发或漏触发
3. **可调性**：用户可根据环境调整阈值

## 备选方案

### 方案A失败后的备选
1. **三阈值方案**：添加中间阈值用于状态保持
2. **动态阈值**：根据环境噪音动态调整阈值
3. **多特征VAD**：结合频谱特征和过零率

### 紧急回滚
如果双阈值方案出现问题，可快速回滚到单阈值：
1. 保留旧`vadThreshold`字段作为后备
2. 在代码中添加`#ifdef DUAL_THRESHOLD`条件编译
3. 提供回滚配置文件

## 结论

双阈值迟滞VAD算法能有效解决当前单阈值算法在嘈杂环境中的问题。通过引入迟滞效应，系统能更好地区分短暂噪音峰值和持续语音活动，提高静音检测的可靠性和识别触发的准确性。

**推荐实施此方案**，预计需要4-5小时完成设计、实现和测试。

---
**设计者**：Claude Code  
**日期**：2026年4月15日  
**版本**：1.0  
**状态**：待用户审核