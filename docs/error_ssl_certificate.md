# SSL证书验证失败问题分析与修复

根据日志分析，WebSocket SSL认证失败的主要原因是系统时间未同步，导致SSL证书验证失败。以下是详细的诊断和修复方案：

## 📊 问题分析

1. **时间同步失败**：从日志可见，WiFi连接后NTP时间同步失败
   ```
   Time synchronization failed (attempt 1/5)
   Time synchronization failed (attempt 2/5)
   ...
   WARNING: Failed to synchronize time after multiple attempts!
   SSL certificate validation may fail!
   ```

2. **SSL错误码-80**：具体错误发生在`ssl_client.cpp:37`
   ```
   [125356][E][ssl_client.cpp:37] _handle_error(): [send_ssl_data():382]: (-80) UNKNOWN ERROR CODE (0050)
   ```

3. **WebSocket认证失败**：由于SSL握手失败，导致后续WebSocket认证无法进行
   ```
   [125366][E][VolcanoSpeechService.cpp:1586] sendWebSocketAuth(): Failed to send authentication message
   ```

## 🔧 已实施的修复方案

### 1. 重构时间同步逻辑 `main.cpp:24-296`
- **移除过早的时间同步**：将`setup()`中在WiFi连接前的同步代码移除
- **添加自动时间同步检查**：在`loop()`中自动检查并同步时间
- **添加手动同步命令**：新增`synctime`串口命令

```cpp
// 时间同步函数
bool syncTimeWithNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot synchronize time");
    return false;
  }
  // ... NTP同步逻辑
}
```

### 2. VolcanoSpeechService时间检查 `VolcanoSpeechService.cpp:2241-2254`
在WebSocket连接前添加系统时间检查：

```cpp
// 检查系统时间是否已同步（SSL证书验证需要正确的时间）
time_t now = time(nullptr);
struct tm timeinfo;
localtime_r(&now, &timeinfo);

if (timeinfo.tm_year + 1900 < 2020) {
    ESP_LOGW(TAG, "System time appears to be unsynchronized (year: %d). SSL certificate validation may fail.",
             timeinfo.tm_year + 1900);
    logEvent("async_ws_time_warning", "System time not synchronized");
} else {
    ESP_LOGI(TAG, "System time synchronized (year: %d)", timeinfo.tm_year + 1900);
}
```

### 3. 自动时间同步机制 `main.cpp:73-92`
在`loop()`函数中添加自动时间同步：

```cpp
// 检查时间同步（如果WiFi已连接但时间未同步）
static bool timeSyncChecked = false;
if (!timeSyncChecked && WiFi.status() == WL_CONNECTED) {
    // 检查当前时间，如果年份是1970（默认值），则需要同步
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    if (timeinfo.tm_year + 1900 < 2020) { // 时间未同步（年份早于2020）
        Serial.println("Time appears to be unsynchronized, attempting NTP sync...");
        if (syncTimeWithNTP()) {
            timeSyncChecked = true;
        }
    } else {
        timeSyncChecked = true;
        Serial.println("Time already synchronized");
    }
}
```

## 🚀 使用说明

### 1. 重新编译上传
```
pio run -t upload
```

### 2. 系统启动后
- 系统会自动检查时间并尝试同步
- 等待WiFi连接完成（约112秒，如日志所示）

### 3. 手动时间同步
如果自动同步失败，可使用串口命令：
```
synctime
```

### 4. 检查时间状态
```
status
```
观察是否有时间同步相关的警告信息

## 🔍 验证步骤
1. 启动设备并等待WiFi连接完成
2. 观察串口日志，确认时间同步成功
3. 发送`start`命令测试语音识别功能
4. 如仍有SSL错误，使用`synctime`手动同步

## 📋 其他改进
- **WebSocket认证消息格式验证**：确保符合火山引擎API规范
- **SSL证书验证改进**：时间同步是SSL证书验证的前提条件
- **错误重试机制**：系统已包含3次重试逻辑

## ⚠️ 注意事项
- **WiFi连接耗时**：从日志看，WiFi连接过程约112秒（包含热点配置时间）
- **NTP服务器可用性**：使用`pool.ntp.org`和`time.nist.gov`作为备用
- **时区设置**：已配置为东八区（8 * 3600）
- **SSL证书有效期**：火山引擎证书需要正确的时间才能验证

**修复后预期效果**：时间同步成功后，WebSocket SSL握手将正常进行，语音识别功能可以正常工作。

## 时间戳
- 问题发现：2026-04-09
- 修复实施：2026-04-10
- 记录创建：2026-04-10
- 修改者：Claude Code
