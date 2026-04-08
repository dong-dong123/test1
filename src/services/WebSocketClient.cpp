#include "WebSocketClient.h"
#include <esp_log.h>

static const char* TAG = "WebSocketClient";

// 静态实例初始化
WebSocketClient* WebSocketClient::instance = nullptr;

WebSocketClient::WebSocketClient() : connected(false) {
    // 设置单例实例
    if (instance == nullptr) {
        instance = this;
    } else {
        ESP_LOGW(TAG, "Multiple WebSocketClient instances detected, only one can receive events");
    }

    // 初始化WebSocket客户端
    webSocket.begin("", 80, "/"); // 空地址，稍后通过connect设置
    webSocket.onEvent(webSocketEvent);

    // 设置重连间隔
    webSocket.setReconnectInterval(5000);

    ESP_LOGI(TAG, "WebSocketClient created");
}

WebSocketClient::~WebSocketClient() {
    if (instance == this) {
        instance = nullptr;
    }
    disconnect();
    ESP_LOGI(TAG, "WebSocketClient destroyed");
}

void WebSocketClient::setExtraHeaders(const String& headers) {
    extraHeaders = headers;
    ESP_LOGI(TAG, "Set extra headers: %s", headers.c_str());
}

bool WebSocketClient::connect(const String& url, const String& protocol) {
    if (connected) {
        ESP_LOGW(TAG, "Already connected, disconnecting first");
        disconnect();
    }

    ESP_LOGI(TAG, "Connecting to WebSocket: %s", url.c_str());

    // 解析URL（简单解析，支持ws://和wss://）
    String host;
    uint16_t port;
    String path;
    bool useSSL = false;

    if (url.startsWith("ws://")) {
        useSSL = false;
        host = url.substring(5);
    } else if (url.startsWith("wss://")) {
        useSSL = true;
        host = url.substring(6);
    } else {
        lastError = "Invalid URL scheme, must be ws:// or wss://";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    // 分离端口和路径
    int pathStart = host.indexOf('/');
    if (pathStart >= 0) {
        path = host.substring(pathStart);
        host = host.substring(0, pathStart);
    } else {
        path = "/";
    }

    // 分离主机和端口
    int portStart = host.indexOf(':');
    if (portStart >= 0) {
        String portStr = host.substring(portStart + 1);
        port = portStr.toInt();
        host = host.substring(0, portStart);
    } else {
        port = useSSL ? 443 : 80;
    }

    ESP_LOGI(TAG, "Parsed: host=%s, port=%u, path=%s, ssl=%s",
             host.c_str(), port, path.c_str(), useSSL ? "yes" : "no");

    // 重新初始化WebSocket客户端
    if (useSSL) {
        webSocket.beginSSL(host.c_str(), port, path.c_str());
    } else {
        webSocket.begin(host.c_str(), port, path.c_str());
    }
    webSocket.onEvent(webSocketEvent);

    // 设置额外HTTP头部
    String headers = "";
    if (!extraHeaders.isEmpty()) {
        headers = extraHeaders;
    }
    // 添加协议头部（如果提供）
    if (!protocol.isEmpty()) {
        if (!headers.isEmpty()) {
            headers += "\r\n";
        }
        headers += "Sec-WebSocket-Protocol: " + protocol;
    }
    if (!headers.isEmpty()) {
        webSocket.setExtraHeaders(headers.c_str());
        ESP_LOGI(TAG, "Setting extra headers: %s", headers.c_str());
    }

    // 开始连接（begin方法已启动连接）
    // webSocket.connect(); // WebSocketsClient库没有connect方法

    // 等待连接建立（简单轮询，实际应用应使用异步回调）
    unsigned long startTime = millis();
    while (!connected && millis() - startTime < 10000) {
        webSocket.loop();
        delay(10);
    }

    if (!connected) {
        lastError = "Connection timeout";
        ESP_LOGE(TAG, "WebSocket connection failed: %s", lastError.c_str());
        return false;
    }

    // 清除额外头部，避免影响后续连接
    extraHeaders = "";

    ESP_LOGI(TAG, "WebSocket connected successfully");
    return true;
}

bool WebSocketClient::disconnect() {
    if (!connected) {
        return true;
    }

    ESP_LOGI(TAG, "Disconnecting WebSocket...");
    webSocket.disconnect();

    // 等待断开连接（等待DISCONNECTED事件）
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) {
        webSocket.loop();
        if (!connected) {  // 等待事件回调设置connected = false
            break;
        }
        delay(10);
    }

    ESP_LOGI(TAG, "WebSocket disconnected");
    return true;
}

bool WebSocketClient::sendText(const String& text) {
    if (!connected) {
        lastError = "Not connected";
        return false;
    }

    ESP_LOGV(TAG, "Sending text message (length: %u): %s", text.length(), text.c_str());
    // 创建临时变量以匹配sendTXT的非const引用参数
    String temp = text;
    return webSocket.sendTXT(temp);
}

bool WebSocketClient::sendBinary(const uint8_t* data, size_t length) {
    if (!connected) {
        lastError = "Not connected";
        return false;
    }

    ESP_LOGV(TAG, "Sending binary data (length: %u)", length);
    return webSocket.sendBIN(data, length);
}

bool WebSocketClient::ping() {
    if (!connected) {
        lastError = "Not connected";
        return false;
    }

    return webSocket.sendPing();
}

void WebSocketClient::handleEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED: {
            ESP_LOGI(TAG, "WebSocket connected");
            connected = true;
            lastError = "";
            if (eventCallback) {
                eventCallback(WebSocketEvent::CONNECTED, "", nullptr, 0);
            }
            break;
        }

        case WStype_DISCONNECTED: {
            ESP_LOGI(TAG, "WebSocket disconnected");
            connected = false;
            if (eventCallback) {
                eventCallback(WebSocketEvent::DISCONNECTED, "", nullptr, 0);
            }
            break;
        }

        case WStype_ERROR: {
            ESP_LOGE(TAG, "WebSocket error");
            connected = false;
            lastError = "WebSocket error";
            if (eventCallback) {
                eventCallback(WebSocketEvent::ERROR, lastError, nullptr, 0);
            }
            break;
        }

        case WStype_TEXT: {
            String text((char*)payload, length);
            ESP_LOGV(TAG, "Received text message (length: %u): %s", length, text.c_str());
            if (eventCallback) {
                eventCallback(WebSocketEvent::TEXT_MESSAGE, text, nullptr, 0);
            }
            break;
        }

        case WStype_BIN: {
            ESP_LOGV(TAG, "Received binary data (length: %u)", length);
            if (eventCallback) {
                eventCallback(WebSocketEvent::BINARY_MESSAGE, "", payload, length);
            }
            break;
        }

        case WStype_PING:
            ESP_LOGV(TAG, "Received ping");
            if (eventCallback) {
                eventCallback(WebSocketEvent::PING, "", nullptr, 0);
            }
            break;

        case WStype_PONG:
            ESP_LOGV(TAG, "Received pong");
            if (eventCallback) {
                eventCallback(WebSocketEvent::PONG, "", nullptr, 0);
            }
            break;

        default:
            ESP_LOGW(TAG, "Unknown WebSocket event type: %d", type);
            break;
    }
}

String WebSocketClient::getRemoteIP() const {
    // WebSocketsClient库可能没有getRemoteIP方法
    // 返回空字符串或占位符
    return "";
}

uint16_t WebSocketClient::getRemotePort() const {
    // WebSocketsClient库可能没有getRemotePort方法
    return 0;
}

void WebSocketClient::loop() {
    webSocket.loop();
}

String WebSocketClient::eventToString(WebSocketEvent event) {
    switch (event) {
        case WebSocketEvent::CONNECTED: return "CONNECTED";
        case WebSocketEvent::DISCONNECTED: return "DISCONNECTED";
        case WebSocketEvent::ERROR: return "ERROR";
        case WebSocketEvent::TEXT_MESSAGE: return "TEXT_MESSAGE";
        case WebSocketEvent::BINARY_MESSAGE: return "BINARY_MESSAGE";
        case WebSocketEvent::PING: return "PING";
        case WebSocketEvent::PONG: return "PONG";
        default: return "UNKNOWN";
    }
}

String WebSocketClient::typeToString(WStype_t type) {
    switch (type) {
        case WStype_CONNECTED: return "CONNECTED";
        case WStype_DISCONNECTED: return "DISCONNECTED";
        case WStype_ERROR: return "ERROR";
        case WStype_TEXT: return "TEXT";
        case WStype_BIN: return "BINARY";
        case WStype_PING: return "PING";
        case WStype_PONG: return "PONG";
        default: return "UNKNOWN";
    }
}