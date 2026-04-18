# 流式语音识别VAD配置问题总结

## 📋 **问题概述**

2026年4月14日修复WebSocket流式识别后，测试发现：
1. ✅ WebSocket过早清理问题已修复：中间确认（log_id）响应后连接保持
2. ✅ 部分识别结果处理已修复：`definite=false`的部分结果被正确识别，连接保持
3. ❌ **VAD静音判停未触发**：服务器未返回`definite=true`的最终识别结果

## 🔍 **问题现象与测试日志**

### **测试流程时间线**
```
17:53:05 系统启动
17:53:16 WebSocket连接建立（V3 API，X-Api头部认证）
17:53:16 发送客户端请求（356字节JSON）
17:53:16 收到中间确认（log_id）
17:53:16-17:53:18 发送25个音频块（159,999字节）
17:53:18 收到部分识别结果：
        1. "什么" (definite=false)
        2. "什么时候" (definite=false)  
        3. "你" (definite=false)
        4. "你是不是" (definite=false)
17:53:18 所有音频发送完成，等待最终结果...
```

### **关键日志证据**
```
[VolcanoSpeechService] Received intermediate confirmation (log_id only) during audio streaming, waiting for final response...
[VolcanoSpeechService] Received partial recognition result: text='什么', waiting for final result...
[VolcanoSpeechService] Received partial recognition result: text='什么时候', waiting for final result...
[VolcanoSpeechService] Received partial recognition result: text='你', waiting for final result...
[VolcanoSpeechService] Received partial recognition result: text='你是不是', waiting for final result...
```

## 💬 **客服反馈与根因分析**

### **火山引擎客服答复关键点**
**结论**：你当前使用的是大模型流式语音识别（SAUC），必须**显式开启VAD相关配置**，默认情况下服务端不会自动进行静音判停。

### **根因分析**
1. **VAD未开启**：默认配置下，服务器不会自动检测静音并判停
2. **客户端请求缺失VAD参数**：Full Client Request中缺少`enable_nonstream`、`end_window_size`等关键参数
3. **服务器等待无限期**：由于未开启VAD，服务器等待客户端发送结束信号，但客户端在发送完音频后等待服务器判停

### **用户语音与识别结果对比**
- **用户实际语音**："说了挺多东西的"（完整句子）
- **服务器返回结果**：
  - "什么"（部分）
  - "什么时候"（部分）  
  - "你"（部分）
  - "你是不是"（部分）
- **问题**：识别结果不完整，且均为`definite=false`

## 🛠️ **解决方案**

### **方案1：开启自动VAD判停（推荐，已实施）**

在Full Client Request中添加以下VAD配置参数：

#### **修改文件**：[src/services/VolcanoRequestBuilder.cpp:68-84](src/services/VolcanoRequestBuilder.cpp#L68-L84)

```cpp
// VAD configuration for automatic silence detection
request["enable_nonstream"] = true;    // Required for VAD-based sentence segmentation
request["end_window_size"] = 800;      // Auto-stop after 800ms silence (min 200)
request["force_to_speech_time"] = 1000; // Don't stop before 1 second of speech
request["show_utterances"] = true;     // Already present, required for definite field
```

#### **参数说明**
| 参数 | 类型 | 说明 | 默认值 | 最小值 |
|------|------|------|--------|--------|
| `enable_nonstream` | bool | 开启二遍识别和VAD分句 | false | - |
| `end_window_size` | int | 静音判停阈值（ms） | 800 | 200 |
| `force_to_speech_time` | int | 最小语音时长（ms） | 1000 | 1 |
| `show_utterances` | bool | 返回分句信息 | true | - |

### **方案2：客户端静音检测（辅助实现）**

#### **修改文件**：[src/MainApplication.cpp:628-662](src/MainApplication.cpp#L628-L662)

```cpp
// 静音检测：基于能量阈值判断是否包含语音
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
            // 静音超时，可触发相关逻辑
        }
    }
}
```

#### **VAD配置参数**（[src/MainApplication.h:55-60](src/MainApplication.h#L55-L60)）
```cpp
float vadThreshold;           // VAD能量阈值 (0.0-1.0, 默认0.3)
uint32_t vadSilenceDuration;  // 静音持续时间阈值 (ms, 默认800)
bool vadSilenceDetected;      // 是否检测到静音
uint32_t vadSilenceStartTime; // 静音开始时间
uint32_t vadLastAudioTime;    // 最后有音频的时间
```

## 🔄 **服务器端VAD判停流程**

### **开启VAD后的预期流程**
```
客户端                          服务端
  |                               |
  |------ Full Client Request --->| (包含enable_nonstream=true)
  |<---- 中间确认（log_id） -------| 
  |                               |
  |------ 持续发送音频包 -------->|
  |<---- 部分识别结果 ------------| (definite=false)
  |                               |
  |------ 语音结束 ---------------|
  |           ↓                  |
  |       800ms静音              |
  |           ↓                  |
  |<---- 最终识别结果 ------------| (definite=true, 完整文本)
  |                               |
  |<------- 关闭连接 -------------|
```

### **静音判停逻辑**
1. **VAD检测**：服务器实时检测音频能量
2. **静音计时**：连续静音超过`end_window_size`(800ms)开始计时
3. **语音时长检查**：总语音时长需≥`force_to_speech_time`(1000ms)
4. **判停触发**：条件满足后返回`definite=true`的最终结果
5. **二遍识别**：`enable_nonstream=true`时，使用非流式模型重新识别整个句子

## 📊 **修复验证计划**

### **验证步骤**
1. **编译上传**：包含VAD配置的新固件
2. **完整录音测试**：说完整句子（如"今天天气怎么样"）
3. **观察日志**：
   - `Full client request JSON`是否包含VAD参数
   - 是否收到`definite=true`的最终响应
   - 识别文本是否完整准确
4. **静音测试**：说完后保持静音，观察800ms后是否返回结果
5. **短语音测试**：短于1秒的语音是否不判停

### **预期日志输出**
```
✅ Full client request JSON: ... "enable_nonstream":true, "end_window_size":800 ...
✅ Received intermediate confirmation (log_id only) ...
✅ Received partial recognition result: text='今', waiting for final result...
✅ Received partial recognition result: text='今天', waiting for final result...
✅ Received partial recognition result: text='今天天', waiting for final result...
✅ Async recognition successful: text='今天天气怎么样', reqid=..., is_final=true
✅ WebSocket connection cleaned up after final result
```

### **验证要点**
| 测试场景 | 预期结果 | 验证方法 |
|----------|----------|----------|
| 正常句子（>3秒） | 返回完整文本，definite=true | 检查最终响应 |
| 说完后静音 | 800ms后返回结果 | 计时测量 |
| 短语音（<1秒） | 不触发判停 | 观察是否无definite=true |
| 中断说话 | 返回已识别部分 | 检查部分结果质量 |
| 嘈杂环境 | 正确识别 | 观察识别准确性 |

## 🚨 **注意事项与兼容性**

### **API版本兼容性**
1. **V3 API专用**：VAD参数仅适用于V3 WebSocket API
2. **资源ID要求**：`volc.bigasr.sauc.duration`支持VAD功能
3. **模型要求**：`model_name="bigmodel"`支持二遍识别

### **性能影响**
1. **延迟增加**：800ms静音等待会增加整体响应时间
2. **资源消耗**：二遍识别增加服务器计算量
3. **准确性提升**：VAD分句+二遍识别提高长句识别准确率

### **配置调优建议**
1. **环境嘈杂**：提高`vadThreshold`(0.3→0.5)，降低误触发
2. **实时性要求高**：降低`end_window_size`(800→400)，更快响应
3. **短语音场景**：降低`force_to_speech_time`(1000→500)，更快判停

## 📈 **状态与进展**

### **已完成工作**
1. ✅ WebSocket过早清理修复
2. ✅ 部分识别结果处理
3. ✅ VAD参数配置添加
4. ✅ 客户端静音检测框架

### **待测试验证**
1. ⏳ VAD配置生效验证
2. ⏳ 完整句子识别测试
3. ⏳ 静音判停计时验证
4. ⏳ 识别准确性评估

### **下一步计划**
1. **立即**：编译测试VAD配置
2. **短期**：优化静音检测参数
3. **中期**：添加VAD状态可视化
4. **长期**：实现自适应VAD阈值

## 🔗 **相关文档**

1. [音频数据流阻塞与WebSocket问题总结.md](音频数据流阻塞与WebSocket问题总结.md) - 前期问题总结
2. [docs/API/流水语音识别api.md](docs/API/流水语音识别api.md) - 火山API文档
3. [src/services/VolcanoRequestBuilder.cpp](src/services/VolcanoRequestBuilder.cpp) - VAD配置实现
4. [src/MainApplication.cpp](src/MainApplication.cpp) - 客户端静音检测

## 🎯 **结论**

流式语音识别VAD配置问题的**根本原因**是服务器端VAD功能未显式开启。通过添加`enable_nonstream`、`end_window_size`、`force_to_speech_time`三个关键参数，可以启用服务器自动静音判停功能。

**核心修复**：
1. **服务器端VAD配置**：确保服务器在静音800ms后自动返回最终结果
2. **客户端静音检测**：辅助监控音频能量，提供调试信息
3. **协议兼容性**：保持与火山V3 API的完全兼容

**预期效果**：
- 完整句子识别：返回`definite=true`的最终结果
- 自动静音判停：800ms静音后自动结束识别
- 识别准确性提升：二遍识别提高长句识别率

修复后系统将具备完整的流式语音识别能力，支持自然对话场景下的自动静音判停。

---
**分析者**：Claude Code  
**总结时间**：2026年4月14日  
**版本**：1.0  
**状态**：VAD配置已添加，待测试验证