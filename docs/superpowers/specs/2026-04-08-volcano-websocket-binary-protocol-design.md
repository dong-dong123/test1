---
name: Volcano WebSocket Binary Protocol Integration
description: 实现Volcano语音识别服务的WebSocket二进制协议集成
type: design
---

# Volcano WebSocket二进制协议集成设计

## 概述

本项目需要实现Volcano语音识别服务的WebSocket二进制协议集成，以替代当前错误的HTTP API调用。基于API文档，我们需要在现有的`VolcanoSpeechService`类中添加对二进制协议的支持。同时，考虑语音合成的WebSocket协议支持，以提供统一的WebSocket体验。

## 设计目标

1. 实现完整的WebSocket二进制协议（header + payload size + payload）
2. 支持`bigmodel_nostream`流式输入模式
3. 保持现有WebSocket连接管理和认证流程
4. 最小化对现有代码的侵入
5. 在ESP32-S3资源约束下高效运行

## 协议详情

### 二进制协议头结构
```
Byte 0: [Version:4 bits][Header Size:4 bits]
Byte 1: [Message Type:4 bits][Flags:4 bits]  
Byte 2: [Serialization:4 bits][Compression:4 bits]
Byte 3: [Reserved:8 bits]
```

### 消息类型
- `0b0001` - Full Client Request (发送请求参数)
- `0b0010` - Audio Only Request (发送音频数据)
- `0b1001` - Full Server Response (接收识别结果)
- `0b1111` - Error Message (服务器错误)

### 请求流程
1. WebSocket连接（使用HTTP头部认证）
2. 发送Full Client Request（JSON格式）
3. 发送Audio Only Request（PCM音频数据）
4. 接收Full Server Response（解析结果）

## 语音合成协议详情

### API端点
- **V3 WebSocket双向流式**: `wss://openspeech.bytedance.com/api/v3/tts/bidirection`
- **V3 WebSocket单向流式**: `wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream`
- **V1 WebSocket单向流式**: `wss://openspeech.bytedance.com/api/v1/tts/ws_binary`（不推荐）
- **HTTP非流式**: `https://openspeech.bytedance.com/api/v1/tts`（现有实现）

### 认证方式
- Bearer Token认证: `Authorization: Bearer;${token}`（注意使用分号分隔）
- 需要在WebSocket连接的HTTP头部添加认证信息

### WebSocket协议需求
根据用户要求，语音合成也需要WebSocket版本。需要实现V3 WebSocket单向流式接口(`wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream`)，该接口可能使用类似的二进制协议。

### 请求JSON格式
```json
{
    "app": {
        "appid": "2015527679",
        "token": "access_token",
        "cluster": "volcano_tts"
    },
    "user": {
        "uid": "esp32_user"
    },
    "audio": {
        "voice_type": "zh-CN_female_standard",
        "encoding": "pcm",
        "rate": 16000,
        "speed_ratio": 1.0
    },
    "request": {
        "reqid": "unique_request_id",
        "text": "合成文本",
        "operation": "submit"
    }
}
```

### 响应格式
```json
{
    "reqid": "unique_request_id",
    "code": 3000,
    "operation": "query",
    "message": "Success",
    "sequence": -1,
    "data": "base64_encoded_audio_data",
    "addition": {
        "duration": "1960"
    }
}
```

## 系统架构

### 现有代码结构
```
VolcanoSpeechService
├── callWebSocketRecognitionAPI()  // 需要修改
├── webSocketClient               // 现有WebSocket客户端
├── handleWebSocketEvent()        // 现有事件处理
└── parseWebSocketMessage()       // 现有消息解析
```

### 新增组件
1. **BinaryProtocolEncoder** - 二进制协议编码器（语音识别）
2. **BinaryProtocolDecoder** - 二进制协议解码器（语音识别）
3. **VolcanoRequestBuilder** - JSON请求构建器（语音识别）
4. **ResponseParser** - 响应解析器（语音识别）
5. **TTSRequestBuilder** - 语音合成JSON请求构建器
6. **TTSResponseParser** - 语音合成响应解析器
7. **WebSocketSynthesisHandler** - 语音合成WebSocket处理器

## 详细设计

### 1. 二进制协议编码器

```cpp
class BinaryProtocolEncoder {
public:
    static std::vector<uint8_t> encodeFullClientRequest(
        const String& jsonRequest, 
        bool useCompression = false,
        uint32_t sequence = 0
    );
    
    static std::vector<uint8_t> encodeAudioOnlyRequest(
        const uint8_t* audioData,
        size_t length,
        bool isLastChunk = false,
        bool useCompression = false,
        uint32_t sequence = 0
    );
    
private:
    static std::vector<uint8_t> buildHeader(
        uint8_t messageType,
        uint8_t flags,
        uint8_t serialization,
        uint8_t compression,
        uint32_t sequence = 0
    );
    
    static void writeUint32BigEndian(std::vector<uint8_t>& buffer, uint32_t value);
    static uint8_t buildByte(uint8_t highBits, uint8_t lowBits);
};
```

### 2. 二进制协议解码器

```cpp
class BinaryProtocolDecoder {
public:
    struct DecodedMessage {
        uint8_t version;
        uint8_t messageType;
        uint8_t flags;
        uint8_t serialization;
        uint8_t compression;
        uint32_t sequence;
        uint32_t payloadSize;
        std::vector<uint8_t> payload;
    };
    
    static DecodedMessage decode(const uint8_t* data, size_t length);
    static uint32_t readUint32BigEndian(const uint8_t* data);
    static String extractTextFromResponse(const std::vector<uint8_t>& payload);
};
```

### 3. Volcano请求构建器

```cpp
class VolcanoRequestBuilder {
public:
    static String buildFullClientRequest(
        const String& uid = "esp32_user",
        const String& language = "zh-CN",
        bool enablePunctuation = true,
        bool enableITN = false,
        bool enableDDC = false
    );
    
private:
    static const char* DEFAULT_AUDIO_CONFIG;
    static const char* DEFAULT_REQUEST_CONFIG;
};
```

### 4. 修改现有函数

```cpp
bool VolcanoSpeechService::callWebSocketRecognitionAPI(
    const uint8_t* audio_data, 
    size_t length, 
    String& text
) {
    // 1. 连接WebSocket（现有）
    // 2. 构建Full Client Request JSON
    String requestJson = VolcanoRequestBuilder::buildFullClientRequest(
        "esp32_user", config.language, config.enablePunctuation
    );
    
    // 3. 编码并发送Full Client Request
    auto fullRequest = BinaryProtocolEncoder::encodeFullClientRequest(
        requestJson, false, 1
    );
    webSocketClient->sendBinary(fullRequest.data(), fullRequest.size());
    
    // 4. 编码并发送Audio Only Request（单次发送）
    auto audioRequest = BinaryProtocolEncoder::encodeAudioOnlyRequest(
        audio_data, length, true, false, 2
    );
    webSocketClient->sendBinary(audioRequest.data(), audioRequest.size());
    
    // 5. 等待并解析响应
    // （需要修改事件处理来解析二进制协议响应）
    return parseBinaryResponse(text);
}
```

### 5. WebSocket事件处理修改

```cpp
void VolcanoSpeechService::handleWebSocketEvent(
    WebSocketEvent event, 
    const String& message, 
    const uint8_t* data, 
    size_t length
) {
    if (event == WebSocketEvent::BINARY_MESSAGE) {
        // 解析二进制协议消息
        auto decoded = BinaryProtocolDecoder::decode(data, length);
        
        if (decoded.messageType == 0b1001) { // Full Server Response
            String text = BinaryProtocolDecoder::extractTextFromResponse(decoded.payload);
            handleRecognitionResult(text, true);
        } else if (decoded.messageType == 0b1111) { // Error Message
            handleErrorMessage(decoded.payload);
        }
    }
    // ... 现有其他事件处理
}
```

## 数据格式

### Full Client Request JSON
```json
{
    "user": {
        "uid": "esp32_user",
        "platform": "ESP32",
        "sdk_version": "1.0"
    },
    "audio": {
        "format": "pcm",
        "rate": 16000,
        "bits": 16,
        "channel": 1,
        "language": "zh-CN"
    },
    "request": {
        "model_name": "bigmodel",
        "enable_itn": false,
        "enable_ddc": false,
        "enable_punc": true
    }
}
```

### Full Server Response JSON
```json
{
    "result": {
        "text": "识别结果文本",
        "utterances": [...]
    },
    "audio_info": {
        "duration": 1000
    }
}
```

### 语音合成请求JSON
```json
{
    "app": {
        "appid": "appid123",
        "token": "access_token",
        "cluster": "volcano_tts"
    },
    "user": {
        "uid": "uid123"
    },
    "audio": {
        "voice_type": "zh_male_M392_conversation_wvae_bigtts",
        "encoding": "mp3",
        "speed_ratio": 1.0
    },
    "request": {
        "reqid": "uuid",
        "text": "字节跳动语音合成",
        "operation": "query"
    }
}
```

### 语音合成响应JSON
```json
{
    "reqid": "reqid",
    "code": 3000,
    "operation": "query",
    "message": "Success",
    "sequence": -1,
    "data": "base64 encoded binary data",
    "addition": {
        "duration": "1960"
    }
}
```

## 语音合成详细设计

### 1. TTS请求构建器
```cpp
class TTSRequestBuilder {
public:
    static String buildSynthesisRequest(
        const String& text,
        const String& voiceType = "zh-CN_female_standard",
        const String& encoding = "pcm",
        int rate = 16000,
        float speedRatio = 1.0f,
        const String& uid = "esp32_user"
    );
    
    static String generateUniqueReqId();
    
private:
    static const char* DEFAULT_APP_CONFIG;
    static const char* DEFAULT_USER_CONFIG;
};
```

### 2. TTS响应解析器
```cpp
class TTSResponseParser {
public:
    struct SynthesisResult {
        String reqid;
        int code;
        String message;
        int sequence;
        std::vector<uint8_t> audioData;
        int durationMs;
        bool success;
    };
    
    static SynthesisResult parseResponse(const String& jsonResponse);
    static std::vector<uint8_t> decodeBase64Audio(const String& base64Data);
};
```

### 3. WebSocket语音合成处理器
```cpp
class WebSocketSynthesisHandler {
public:
    bool synthesizeViaWebSocket(
        const String& text,
        std::vector<uint8_t>& audioData,
        const String& endpoint = "wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream"
    );
    
private:
    bool connectWithAuth(const String& endpoint);
    bool sendSynthesisRequest(const String& jsonRequest);
    bool receiveAudioResponse(std::vector<uint8_t>& audioData);
};
```

### 4. 修改现有synthesize函数
```cpp
bool VolcanoSpeechService::synthesize(const String& text, std::vector<uint8_t>& audio_data) {
    // 方案1: 继续使用现有HTTP API（向后兼容）
    // return callSynthesisAPI(text, audio_data);
    
    // 方案2: 使用WebSocket流式API（新实现）
    WebSocketSynthesisHandler handler;
    return handler.synthesizeViaWebSocket(text, audio_data);
}
```

## 配置参数

### WebSocket端点
- **语音识别非流式模式**: `wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_nostream`
- **语音识别双向流式模式**: `wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async`
- **语音合成单向流式**: `wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream`
- **语音合成双向流式**: `wss://openspeech.bytedance.com/api/v3/tts/bidirection`

### 认证头部
- **通用认证**:
  - `X-Api-App-Key`: APP ID (2015527679)
  - `X-Api-Access-Key`: Access Token (R23gVDqaVB_j-TaRfNywkJnerpGGJtcB)
  - `X-Api-Connect-Id`: 随机生成的UUID
  
- **语音识别特定**:
  - `X-Api-Resource-Id`: `volc.bigasr.sauc.duration` (小时版)
  
- **语音合成特定**:
  - `Authorization`: `Bearer;${token}` (Bearer Token认证)

## 资源考虑

### 内存使用
- 二进制编码器: ~2KB RAM
- JSON构建: 使用ArduinoJson，需要4KB动态文档
- 音频缓冲: 单次发送，不需要持续缓冲

### 性能考虑
- 大端转换: 使用位操作，避免浮点运算
- JSON序列化: 使用ArduinoJson的高效序列化
- 网络延迟: 单次请求-响应模式，不需要流式处理

## 测试策略

1. **单元测试**: 二进制编码/解码函数
2. **集成测试**: 完整的请求-响应流程模拟
3. **硬件测试**: 在ESP32-S3上测试内存和性能
4. **API测试**: 实际调用Volcano API验证

## 风险与缓解

### 风险1: 协议实现错误
- **缓解**: 严格遵循API文档，使用官方示例验证

### 风险2: 内存不足
- **缓解**: 使用流式处理，避免大内存分配

### 风险3: 网络不稳定
- **缓解**: 实现重试机制和超时处理

## 实施计划

### 阶段1: 协议基础实现
1. 实现BinaryProtocolEncoder/Decoder
2. 实现VolcanoRequestBuilder
3. 单元测试验证

### 阶段2: 集成到现有服务
1. 修改callWebSocketRecognitionAPI
2. 更新WebSocket事件处理
3. 集成测试

### 阶段3: 测试与优化
1. 硬件测试
2. API集成测试
3. 性能优化

## 成功标准

1. ✅ 正确实现二进制协议
2. ✅ 成功调用Volcano API并获得识别结果
3. ✅ 内存使用在ESP32-S3限制内
4. ✅ 识别准确率与预期一致
5. ✅ 错误处理完善

## 附录

### 参考资料
1. [流水语音识别API文档](docs/API/流水语音识别api.md)
2. [Volcano语音识别API文档](https://www.volcengine.com/docs/6561/1079478)
3. [现有VolcanoSpeechService实现](src/services/VolcanoSpeechService.cpp)

### 相关文件
- `src/services/VolcanoSpeechService.cpp` - 主实现文件
- `src/services/VolcanoSpeechService.h` - 头文件
- `src/services/WebSocketClient.h` - WebSocket客户端
- `data/config.json` - 配置文件