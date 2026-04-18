# 火山引擎语音识别API HTTP 400错误解决记录

## 问题描述
在调用火山引擎语音识别API（`https://openspeech.bytedance.com/api/v1/asr`）时，返回HTTP 400错误，错误信息为：

```
{"reqid":"","code":1001,"message":"pushing data to async workflow: unable to unmarshal request: buil...
```

错误代码1001表示API无法解析请求JSON格式。

## 根本原因
项目文档中缺少HTTP非流式语音识别API的详细说明（只有WebSocket流式识别和HTTP语音合成文档）。根据语音合成API文档推断的请求格式可能与实际API要求不匹配。

当前请求格式可能存在以下问题：
1. `audio_data`字段位置不正确（原放在根级别）
2. JSON结构可能与API期望的格式不一致
3. 可能存在缺少的必需字段

## 已尝试的解决方案

### 1. 认证头部修复
已确认认证头部格式为`Bearer;${token}`（使用分号），已在代码中正确实现：
```cpp
headers["Authorization"] = "Bearer;" + config.secretKey;
```

### 2. 时间同步修复
已实现NTP时间同步，解决SSL证书验证问题。

### 3. 请求格式调整
将`audio_data`字段从根级别移动到`request`对象内部：

**修改前：**
```json
{
  "app": {...},
  "user": {...},
  "audio": {...},
  "request": {...},
  "audio_data": "base64..."
}
```

**修改后：**
```json
{
  "app": {...},
  "user": {...},
  "audio": {...},
  "request": {
    ...,
    "audio_data": "base64..."
  }
}
```

### 4. 请求格式重构（根据WebSocket API示例）
基于用户提供的WebSocket API请求示例，移除`app`对象，调整`request`字段：

**修改前（基于语音合成API）：**
```json
{
  "app": {"appid": "...", "token": "...", "cluster": "volcano_asr"},
  "user": {"uid": "esp32_user"},
  "audio": {"format": "pcm", "codec": "raw", "rate": 16000, "bits": 16, "channel": 1, "language": "zh-CN"},
  "request": {"reqid": "...", "operation": "query", "audio_data": "base64..."}
}
```

**修改后（基于WebSocket API格式）：**
```json
{
  "user": {"uid": "esp32_user"},
  "audio": {"format": "pcm", "codec": "raw", "rate": 16000, "bits": 16, "channel": 1, "language": "zh-CN"},
  "request": {
    "reqid": "...",
    "model_name": "bigmodel",
    "enable_itn": true,
    "enable_punc": true,
    "enable_ddc": false
  },
  "audio_data": "base64..."  // 注意：此处放回根级别
}
```

### 5. 响应解析增强
- 支持多种成功码：3000（HTTP API）和20000000（WebSocket API）
- 支持多种响应格式：`data`字段、`result`字段、`result.text`嵌套对象

### 6. 调试增强
- 增加JSON文档大小从4096到8192字节
- 将请求体日志从500字符扩展到800字符
- 增加认证头部和URL的详细日志

## 文件修改
- `src/services/VolcanoSpeechService.cpp`:
  - 第579行：`DynamicJsonDocument requestDoc(4096)` → `DynamicJsonDocument requestDoc(8192)`
  - 第613-615行：将`audio_data`从`requestDoc`移动到`request`对象
  - 第628-633行：增加调试日志字符限制

## 下一步建议

### 1. 测试当前修改
重新编译并上传固件，观察新的调试日志：
- 完整的请求JSON结构（前800字符）
- API响应状态和错误信息

### 2. 进一步调试
如果仍然失败，建议：
1. 获取火山引擎HTTP语音识别API的官方文档
2. 使用Postman或curl测试API请求格式
3. 检查`cluster`字段值（当前为`volcano_asr`）是否正确
4. 尝试将音频数据放在`audio`对象内或根级别的`data`字段

### 3. 备用方案
如果HTTP API持续失败，考虑：
1. 使用WebSocket流式识别API（已有文档）
2. 切换到其他语音识别服务（如百度、腾讯）

## 相关文件
- `src/services/VolcanoSpeechService.cpp` - 主要实现文件
- `docs/API/语音合成api.md` - 火山引擎语音合成API文档
- `docs/API/流水语音识别api.md` - 火山引擎WebSocket流式识别API文档

## 时间戳
- 问题发现：2026-04-08
- 最后修改：2026-04-08
- 修改者：Claude Code

