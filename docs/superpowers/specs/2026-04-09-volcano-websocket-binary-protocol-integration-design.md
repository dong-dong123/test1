# Volcano WebSocket Binary Protocol Integration Design

## Metadata
- **Date**: 2026-04-09
- **Author**: Claude Code
- **Project**: test1 - ESP32语音交互系统
- **Related Issues**: 完成Volcano WebSocket二进制协议集成剩余任务

## Overview

### Problem Statement
Volcano WebSocket二进制协议集成已完成约85%，剩余两个关键任务：
1. 添加二进制协议单元测试
2. 更新`synthesize()`函数以集成`WebSocketSynthesisHandler`

当前状态：6/7个二进制协议组件已实现并复制到主分支，但需要完成测试和最后的集成工作。

### Goals
1. **测试验证**：通过单元测试确保二进制协议组件功能正确
2. **功能集成**：使`synthesize()`函数支持WebSocket二进制协议路径
3. **向后兼容**：保持现有HTTP API路径作为备选方案
4. **质量保证**：确保所有现有测试继续通过

### Non-Goals
- 重构现有HTTP API实现（除非发现问题）
- 修改其他语音服务接口
- 优化WebSocket协议性能（超出当前范围）

## Design Decisions

### Approach Selection: Conditional Branch Integration (Method 1)
**选择理由**：
- **简单性**：最小化代码改动，降低引入错误的风险
- **一致性**：与现有代码模式匹配（类似`callWebSocketRecognitionAPI`的条件判断）
- **可维护性**：清晰的条件分支易于理解和调试
- **渐进性**：可以轻松扩展或修改，不影响现有功能

**替代方案考虑**：
- **策略模式**：过度设计，当前需求简单
- **混合方法**：可能使`callSynthesisAPI`函数过于复杂

### Configuration-Driven Protocol Selection
**决策**：使用`config.binaryProtocolEnabled`标志控制协议选择

**理由**：
1. **运行时可配置**：允许根据网络条件或服务可用性动态切换
2. **向后兼容**：默认启用但需要`appId`，否则回退到HTTP
3. **透明性**：配置明确，易于调试

### Testing Strategy
**双路径测试**：
1. **组件级测试**：验证`BinaryProtocolEncoder/Decoder`等独立组件
2. **集成测试**：验证`synthesize()`函数在两种协议路径下的行为
3. **回归测试**：确保现有HTTP路径功能不变

## Detailed Design

### 1. File Integration

#### Source Files to Copy
从工作树复制到主分支`src/services/`：
- `WebSocketSynthesisHandler.h`
- `WebSocketSynthesisHandler.cpp`

#### Header File Updates
在`VolcanoSpeechService.h`中添加：
```cpp
#include "WebSocketSynthesisHandler.h"
```

### 2. synthesize() Function Modification

#### Conditional Logic
```cpp
bool VolcanoSpeechService::synthesize(const String &text, std::vector<uint8_t> &audio_data) {
    // 现有验证逻辑保持不变...
    
    bool success = false;
    
    if (config.binaryProtocolEnabled && !config.appId.isEmpty()) {
        // WebSocket二进制协议路径
        success = synthesizeViaWebSocket(text, audio_data);
    } else {
        // 现有HTTP API路径（向后兼容）
        success = callSynthesisAPI(text, audio_data);
    }
    
    // 现有日志和返回逻辑保持不变...
}
```

#### New Helper Method
```cpp
bool VolcanoSpeechService::synthesizeViaWebSocket(const String &text, std::vector<uint8_t> &audio_data) {
    ESP_LOGI(TAG, "Using WebSocket binary protocol for synthesis");
    
    WebSocketSynthesisHandler handler(networkManager, configManager);
    
    // 配置传递
    handler.setConfiguration(
        config.appId,           // Application ID
        config.secretKey,       // Access Token
        config.cluster,         // Cluster identifier
        config.uid,             // User ID
        config.voice,           // Voice type
        config.encoding,        // Audio encoding
        config.sampleRate,      // Sample rate
        config.speedRatio       // Speed ratio
    );
    
    // 超时配置（使用现有配置）
    handler.setTimeouts(
        static_cast<uint32_t>(config.timeout * 1000),      // 连接超时
        static_cast<uint32_t>(config.timeout * 1000),      // 响应超时
        5000                                              // 块超时（固定值）
    );
    
    bool success = handler.synthesizeViaWebSocket(
        text,
        audio_data,
        config.webSocketSynthesisUnidirectionalEndpoint
    );
    
    if (!success) {
        lastError = "WebSocket synthesis failed: " + handler.getLastError();
    }
    
    return success;
}
```

### 3. Configuration Integration

#### Required Configuration Fields
```cpp
// VolcanoSpeechConfig结构已包含以下字段：
struct VolcanoSpeechConfig {
    // ... 现有字段 ...
    
    // WebSocket二进制协议配置
    bool binaryProtocolEnabled; // 是否启用二进制协议（默认true）
    
    // TTS特定配置
    String appId;           // 应用ID（用于TTS WebSocket）
    String cluster;         // 集群标识（默认："volcano_tts"）
    String uid;             // 用户ID（默认："esp32_user"）
    String encoding;        // 音频编码格式（默认："pcm"）
    int sampleRate;         // 音频采样率（默认：16000）
    float speedRatio;       // 语速比例（默认：1.0）
    
    // WebSocket端点（已定义）
    String webSocketSynthesisUnidirectionalEndpoint;
};
```

#### Default Values
- `binaryProtocolEnabled = true`（默认启用）
- 但需要`appId`不为空才能实际使用WebSocket路径
- 如果`appId`为空，自动回退到HTTP API

### 4. Unit Test Extension

#### Test File: test_volcano_speech_service.cpp

**新增测试类别A：二进制协议组件测试**
```cpp
void test_binary_protocol_encoder_basic(void) {
    // 测试BinaryProtocolEncoder基本功能
    // - 创建编码器实例
    // - 编码简单消息
    // - 验证输出格式
}

void test_binary_protocol_decoder_basic(void) {
    // 测试BinaryProtocolDecoder基本功能
    // - 创建解码器实例
    // - 解码有效payload
    // - 验证文本提取
}

void test_tts_request_builder(void) {
    // 测试TTSRequestBuilder
    // - 构建合成请求
    // - 验证JSON格式
    // - 验证必需字段
}

void test_tts_response_parser(void) {
    // 测试TTSResponseParser
    // - 解析成功响应
    // - 解析错误响应
    // - 处理边界情况
}
```

**新增测试类别B：集成测试**
```cpp
void test_synthesis_with_binary_protocol_enabled(void) {
    // 启用二进制协议时的合成测试
    // 1. 设置config.binaryProtocolEnabled = true
    // 2. 设置有效的appId
    // 3. 调用synthesize()
    // 4. 验证使用WebSocket路径
    // 5. 验证结果
}

void test_synthesis_with_binary_protocol_disabled(void) {
    // 禁用二进制协议时的合成测试
    // 1. 设置config.binaryProtocolEnabled = false
    // 2. 调用synthesize()
    // 3. 验证使用HTTP API路径
    // 4. 验证结果
}

void test_websocket_synthesis_handler_integration(void) {
    // WebSocketSynthesisHandler集成测试
    // 1. 创建handler实例
    // 2. 配置参数
    // 3. 调用synthesizeViaWebSocket()
    // 4. 验证音频数据返回
}
```

#### Mock Requirements
需要扩展现有的mock类以支持：
- `WebSocketClient` mock（如果handler内部使用）
- 二进制协议消息的模拟响应
- WebSocket连接状态模拟

### 5. Backward Compatibility

#### Fallback Mechanism
1. **条件检查**：只有当`binaryProtocolEnabled = true` **且** `appId`不为空时才使用WebSocket路径
2. **自动回退**：如果WebSocket合成失败，可考虑回退到HTTP API（可选功能）
3. **配置验证**：确保两种路径的配置参数都正确加载

#### Default Behavior
- **新安装**：`binaryProtocolEnabled = true`，但需要用户配置`appId`
- **升级**：现有配置保持不变，`appId`默认为空，所以继续使用HTTP API
- **显式禁用**：用户可设置`binaryProtocolEnabled = false`强制使用HTTP API

## Test Plan

### Phase 1: Component Tests
**目标**：验证每个二进制协议组件的独立功能

1. **BinaryProtocolEncoder**
   - 测试消息编码
   - 测试压缩选项
   - 测试错误处理

2. **BinaryProtocolDecoder**
   - 测试消息解码
   - 测试文本提取
   - 测试错误响应处理

3. **TTSRequestBuilder/TTSResponseParser**
   - 测试请求构建
   - 测试响应解析
   - 测试边界情况

### Phase 2: Integration Tests
**目标**：验证组件之间的集成

1. **WebSocketSynthesisHandler集成**
   - 测试完整合成流程
   - 测试错误恢复
   - 测试超时处理

2. **VolcanoSpeechService协议切换**
   - 测试启用二进制协议路径
   - 测试禁用二进制协议路径
   - 测试配置验证

### Phase 3: End-to-End Tests
**目标**：验证整个系统行为

1. **向后兼容性**
   - 验证现有HTTP API功能不变
   - 验证配置加载正确

2. **协议切换验证**
   - 动态切换协议并验证行为
   - 测试回退机制

### Test Execution
```bash
# 运行所有测试
pio test

# 运行特定测试文件
pio test --environment native --filter test_volcano_speech_service
```

## Implementation Plan

### Parallel Task 1: WebSocketSynthesisHandler Integration
**步骤**：
1. 复制`WebSocketSynthesisHandler.h/.cpp`到`src/services/`
2. 在`VolcanoSpeechService.h`中添加头文件包含
3. 在`VolcanoSpeechService.cpp`中添加`synthesizeViaWebSocket()`辅助方法
4. 修改`synthesize()`函数添加条件逻辑
5. 验证编译通过

### Parallel Task 2: Binary Protocol Unit Tests
**步骤**：
1. 扩展`test_volcano_speech_service.cpp`添加组件测试
2. 添加集成测试用例
3. 扩展mock类支持WebSocket测试
4. 运行测试验证功能

### Integration Phase
**步骤**：
1. 合并两个任务的更改
2. 运行完整测试套件
3. 修复任何测试失败
4. 验证向后兼容性

## Acceptance Criteria

### Must Have
- [ ] `WebSocketSynthesisHandler`成功复制到主分支
- [ ] `synthesize()`函数正确集成条件逻辑
- [ ] 二进制协议启用时使用WebSocket路径
- [ ] 二进制协议禁用时使用HTTP路径
- [ ] 单元测试覆盖两种协议路径
- [ ] 所有现有测试继续通过

### Should Have
- [ ] 清晰的错误消息，区分协议失败原因
- [ ] 配置验证，防止无效配置导致运行时错误
- [ ] 适当的日志记录，帮助调试协议选择

### Could Have (Future Enhancements)
- [ ] 自动回退机制：WebSocket失败时自动尝试HTTP API
- [ ] 性能比较：记录两种协议的响应时间
- [ ] 配置验证工具：检查TTS WebSocket配置的完整性

## Risks and Mitigations

### Risk 1: WebSocket合成失败导致服务不可用
**缓解**：
- 条件检查确保只有在配置完整时才使用WebSocket
- 保持HTTP API作为可靠后备
- 清晰的错误消息帮助用户诊断配置问题

### Risk 2: 测试覆盖不足
**缓解**：
- 双路径测试：分别测试两种协议
- 组件级测试：验证每个二进制协议组件
- 集成测试：验证端到端流程

### Risk 3: 向后兼容性破坏
**缓解**：
- 默认配置保持现有行为（`appId`为空时使用HTTP）
- 全面运行现有测试套件
- 仔细验证配置加载逻辑

## Open Questions

1. **回退机制**：WebSocket失败时是否应该自动尝试HTTP API？
   - **当前决策**：不实现，保持简单。用户可配置`binaryProtocolEnabled = false`

2. **性能影响**：WebSocket连接建立时间可能比HTTP长
   - **缓解**：超时配置可调，用户可根据网络条件调整

3. **配置复杂度**：用户需要配置更多TTS-specific参数
   - **文档**：需要更新配置文档说明新字段

## Dependencies

### Internal Dependencies
- `BinaryProtocolEncoder/Decoder`：必须功能正常
- `TTSRequestBuilder/TTSResponseParser`：必须功能正常
- `WebSocketClient`基础设施：必须可用

### External Dependencies
- Volcano WebSocket TTS API：服务端必须支持二进制协议
- 网络连接：稳定网络对于WebSocket连接至关重要

## Appendix

### File Structure After Implementation
```
src/services/
├── BinaryProtocolEncoder.h/.cpp      ✓ 已存在
├── BinaryProtocolDecoder.h/.cpp      ✓ 已存在
├── VolcanoRequestBuilder.h/.cpp      ✓ 已存在
├── TTSRequestBuilder.h/.cpp          ✓ 已存在
├── TTSResponseParser.h               ✓ 已存在
├── WebSocketSynthesisHandler.h/.cpp  ⬆ 本次添加
├── VolcanoSpeechService.h/.cpp       ⬆ 本次修改
└── ...其他服务文件

test/
├── test_volcano_speech_service.cpp   ⬆ 本次扩展
└── ...其他测试文件
```

### Configuration Example
```json
{
  "services": {
    "volcano": {
      "apiKey": "2015527679",
      "secretKey": "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB",
      "binaryProtocolEnabled": true,
      "appId": "2015527679",
      "cluster": "volcano_tts",
      "uid": "esp32_user",
      "voice": "zh-CN_female_standard",
      "encoding": "pcm",
      "sampleRate": 16000,
      "speedRatio": 1.0,
      "webSocketSynthesisUnidirectionalEndpoint": "wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream"
    }
  }
}
```

### Success Metrics
1. **测试通过率**：100%单元测试通过
2. **编译成功**：无编译错误或警告
3. **功能正确**：两种协议路径都能成功合成语音
4. **向后兼容**：现有配置无需修改继续工作

---

*文档版本：1.0*
*最后更新：2026-04-09*