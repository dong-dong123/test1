# 音频格式修复与WebSocket连接成功测试（2026-04-11）

## 📊 **测试结果总结**
用户提供的测试总结（上一个窗口）：

**重大突破！WebSocket连接成功建立！**
测试结果显示V3 API认证方案完全正确，WebSocket连接成功建立（101 Switching Protocols）。核心问题已从认证失败转移到音频格式配置错误。

### ✅ **已取得的进展**
- **V3 API认证头部正确** - X-Api-*头部被服务器接受
- **Resource ID正确** - volc.bigasr.sauc.duration（ASR 1.0小时版）通过验证
- **WebSocket连接成功** - 连接建立时间：1390ms
- **音频数据成功发送** - 32776字节二进制音频数据发送成功
- **服务器响应接收** - 收到101字节二进制响应

### 🔍 **当前问题分析**
服务器返回的错误信息：
```
{"error":"[Invalid audio format] OperatorWrapper Process failed: unsupported format raw"}
```

**根本原因**：
- API文档要求format字段为：pcm / wav / ogg / mp3
- 当前代码使用`"format":"raw"`，服务器不支持
- codec字段应为`"raw"`（表示PCM编码）

### 🔧 **已实施的修复**
已修改所有format字段从`"raw"`改为`"pcm"`：
1. `VolcanoSpeechService.cpp:2157` - async recognition format
2. `VolcanoSpeechService.cpp:870` - sync recognition format  
3. `VolcanoSpeechService.cpp:662` - HTTP API audio format
4. `VolcanoSpeechService.cpp:1664` - WebSocket text mode format

codec字段保持`"raw"`不变（表示PCM编码，符合API文档）。

### 📊 **JSON请求结构对比**
**修改前（错误）**：
```json
"audio": {
  "format": "raw",     // ❌ 无效格式
  "codec": "raw",      // ✅ 正确（表示PCM）
  "rate": 16000,
  "bits": 16,
  "channel": 1,
  "language": "zh-CN"
}
```

**修改后（正确）**：
```json
"audio": {
  "format": "pcm",     // ✅ API支持：pcm/wav/ogg/mp3
  "codec": "raw",      // ✅ PCM编码
  "rate": 16000,
  "bits": 16,
  "channel": 1,
  "language": "zh-CN"
}
```

### 🚀 **预期效果**
- **音频格式兼容** - 服务器将正确识别PCM格式音频
- **识别流程继续** - 成功发送音频后应收到识别结果
- **完整语音识别** - 从录音到识别的完整流程应能正常工作

### 📋 **重新编译测试建议**
**编译上传**：
```
pio run           # 编译
pio run -t upload # 上传固件
```

**测试序列**：
```
start  # 测试完整语音识别流程
```

**关键观察点**：
1. 日志显示 `"format":"pcm"`（不再显示`"raw"`）
2. WebSocket连接成功后服务器返回识别结果（而非错误消息）
3. 音频数据被正确处理

### ⚠️ **注意事项**
- **音频格式正确性** - 确认ESP32录音输出为16位16000Hz单声道PCM（符合pcm_s16le要求）
- **数据对齐** - 音频数据包大小应符合服务器要求（建议100-200ms分包）
- **API版本** - 确认使用正确的bigmodel端点（已从bigmodel_async改为bigmodel）

### 📝 **错误记录建议**
无论测试成功或失败，请将结果记录到docs/error.md，包括：
- 修改前后的JSON请求对比
- 服务器响应变化
- 最终识别结果（如果成功）

**总结**：这是解决问题的关键一步！音频格式修复后，语音识别功能应能正常工作。

## ⏱️ **时间戳**
- **测试总结提供**：2026-04-11（具体时间未知）
- **记录添加**：2026-04-11
- **记录者**：Claude Code

