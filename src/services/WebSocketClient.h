#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <functional>
#include <vector>

// WebSocket事件类型
enum class WebSocketEvent {
    CONNECTED,
    DISCONNECTED,
    ERROR,
    TEXT_MESSAGE,
    BINARY_MESSAGE,
    PING,
    PONG
};

// WebSocket事件回调函数类型
typedef std::function<void(WebSocketEvent event, const String& message, const uint8_t* data, size_t length)> WebSocketEventCallback;

// WebSocket客户端包装类
class WebSocketClient {
private:
    WebSocketsClient webSocket;
    WebSocketEventCallback eventCallback;
    bool connected;
    String lastError;
    unsigned long connectionStartTime;

    // 静态事件处理函数（转发到实例）
    static void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
        if (instance) {
            instance->handleEvent(type, payload, length);
        }
    }

    void handleEvent(WStype_t type, uint8_t* payload, size_t length);

    // 单例实例指针（用于静态回调）
    static WebSocketClient* instance;

    // 额外HTTP头部
    String extraHeaders;

public:
    WebSocketClient();
    ~WebSocketClient();

    // 连接管理
    bool connect(const String& url, const String& protocol = "volcano-speech");
    bool disconnect();
    bool isConnected() const { return connected; }

    // 设置额外HTTP头部（必须在连接前调用）
    void setExtraHeaders(const String& headers);

    // 消息发送
    bool sendText(const String& text);
    bool sendTextChunked(const String& text, size_t chunkSize = 128);
    bool sendBinary(const uint8_t* data, size_t length);
    bool ping();

    // 事件回调设置
    void setEventCallback(WebSocketEventCallback callback) {
        eventCallback = callback;
    }

    // 错误处理
    String getLastError() const { return lastError; }
    void clearError() { lastError = ""; }

    // 连接信息
    String getRemoteIP() const;
    uint16_t getRemotePort() const;

    // 循环处理（需要定期调用）
    void loop();

    // 静态工具方法
    static String eventToString(WebSocketEvent event);
    static String typeToString(WStype_t type);
};

#endif // WEBSOCKET_CLIENT_H