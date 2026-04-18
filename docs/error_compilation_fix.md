
# 编译错误修复

## 问题描述
在编译项目时出现两个关键错误：

1. **WebSocketClient编译错误**：`WebSocketsClient`类缺少`setInsecure()`方法
   ```
   src/services/WebSocketClient.cpp:92:19: error: 'class WebSocketsClient' has no member named 'setInsecure'
   ```

2. **状态枚举缺失**：`STATE_CONNECTED`未在`AsyncRecognitionState`枚举中定义
   ```
   src/services/VolcanoSpeechService.cpp:2327:19: error: 'STATE_CONNECTED' was not declared in this scope
   ```

## 根本原因

1. **WebSocket库版本兼容性**：使用的WebSockets库（Links2004/WebSockets@^2.3.7）可能不支持`setInsecure()`方法，或者方法名称不同
2. **状态机设计不完整**：`AsyncRecognitionState`枚举缺少`STATE_CONNECTED`状态，但代码中尝试使用该状态

## 修复方案

### 1. WebSocketClient SSL验证修复
修改`src/services/WebSocketClient.cpp`第92行，注释掉`setInsecure()`调用：
```cpp
// webSocket.setInsecure(); // Method not available in this library version
ESP_LOGW(TAG, "SSL certificate verification may fail - setInsecure not available");
```

### 2. 状态枚举扩展
在`src/services/VolcanoSpeechService.h`的`AsyncRecognitionState`枚举中添加`STATE_CONNECTED`状态：
```cpp
enum AsyncRecognitionState {
    STATE_IDLE,              // 空闲，无进行中的请求
    STATE_CONNECTING,        // 连接WebSocket
    STATE_CONNECTED,         // WebSocket连接已建立
    STATE_AUTHENTICATING,    // 发送认证
    // ... 其他状态
};
```

## 文件修改

- `src/services/WebSocketClient.cpp`:
  - 第92行：注释掉`webSocket.setInsecure()`调用
  - 第93行：更新日志消息
  
- `src/services/VolcanoSpeechService.h`:
  - 第114-124行：在`STATE_CONNECTING`后添加`STATE_CONNECTED`枚举值

## 验证
修复后项目应能成功编译，但SSL证书验证问题仍需通过时间同步解决。

## 时间戳
- 问题发现：2026-04-10
- 修复实施：2026-04-10
- 记录创建：2026-04-10
- 修改者：Claude Code

