#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <map>
#include <vector>
#include "../interfaces/ConfigManager.h"
#include "../interfaces/Logger.h"
#include "../config/ConfigData.h"

// 网络事件类型
enum class NetworkEvent {
    WIFI_CONNECTING,        // Wi-Fi连接中
    WIFI_CONNECTED,         // Wi-Fi已连接
    WIFI_DISCONNECTED,      // Wi-Fi断开
    WIFI_GOT_IP,           // 获取到IP地址
    WIFI_LOST_IP,          // 丢失IP地址
    WIFI_CONNECTION_FAILED, // Wi-Fi连接失败
    WIFI_SCAN_COMPLETED,    // Wi-Fi扫描完成
    HTTP_REQUEST_START,     // HTTP请求开始
    HTTP_REQUEST_SUCCESS,   // HTTP请求成功
    HTTP_REQUEST_FAILED,    // HTTP请求失败
    NETWORK_ERROR           // 网络错误
};

// HTTP请求方法
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH
};

// HTTP请求配置
struct HttpRequestConfig {
    String url;
    HttpMethod method;
    String body;
    std::map<String, String> headers;
    uint32_t timeout;           // 超时时间（毫秒）
    uint8_t maxRetries;         // 最大重试次数
    bool followRedirects;       // 是否跟随重定向
    bool useSSL;                // 是否使用SSL

    HttpRequestConfig() :
        method(HttpMethod::GET),
        timeout(10000),
        maxRetries(3),
        followRedirects(true),
        useSSL(true) {}
};

// HTTP响应
struct HttpResponse {
    int statusCode;
    String body;
    std::map<String, String> headers;
    uint32_t responseTime;      // 响应时间（毫秒）
    String errorMessage;

    HttpResponse() : statusCode(-1), responseTime(0) {}
};

// 网络状态
struct NetworkStatus {
    bool wifiConnected;
    bool hasIP;
    String localIP;
    String ssid;
    int32_t rssi;
    uint32_t connectionTime;
    uint32_t lastDisconnectTime;
    uint32_t disconnectCount;

    NetworkStatus() :
        wifiConnected(false),
        hasIP(false),
        rssi(0),
        connectionTime(0),
        lastDisconnectTime(0),
        disconnectCount(0) {}
};

// 网络事件回调函数类型
typedef void (*NetworkEventCallback)(NetworkEvent event, const String& details, void* userData);


// 网络管理器类 - 管理Wi-Fi连接和HTTP请求
class NetworkManager {
private:
    // 配置和状态
    ConfigManager* configManager;
    Logger* logger;
    NetworkStatus status;
    WiFiConfig wifiConfig;

    // 事件回调
    std::vector<NetworkEventCallback> eventCallbacks;
    std::vector<void*> callbackUserData;

    // 连接管理
    bool isInitialized;
    bool autoReconnect;
    uint32_t reconnectInterval;
    uint32_t lastReconnectAttempt;
    uint32_t lastStatusCheck;

    // HTTP客户端池
    std::vector<HTTPClient*> httpClients;
    uint8_t maxHttpClients;

    // WiFiManager智能配网
    WiFiManager* wifiManager;
    bool useWiFiManager;

    // 内部方法
    bool connectToWiFi();
    void disconnectWiFi();
    void handleWiFiEvent(arduino_event_id_t event, arduino_event_info_t info);
    void notifyEvent(NetworkEvent event, const String& details = "");
    void updateStatus();
    bool loadConfig();
    bool startWiFiManagerAutoConnect();

    // WiFi事件ID（用于移除事件回调）
    int32_t wifiEventId;

    // 静态事件处理函数（用于WiFi事件回调）
    static void wifiEventHandler(void* arg, WiFiEvent_t event);

public:
    NetworkManager(ConfigManager* configMgr = nullptr, Logger* log = nullptr);
    virtual ~NetworkManager();

    // 初始化/反初始化
    bool initialize();
    bool deinitialize();
    bool isReady() const { return isInitialized; }

    // Wi-Fi管理
    bool connect();
    bool disconnect();
    bool reconnect();
    bool startWiFiManagerHotspot();  // 启动AP热点模式进行WiFi配置
    bool isConnected() const { return status.wifiConnected && status.hasIP; }
    const NetworkStatus& getStatus() const { return status; }

    // 网络扫描
    bool scanNetworks(std::vector<String>& networks, bool async = false);
    int getNetworkCount() const;
    String getScannedNetwork(int index) const;

    // HTTP请求
    HttpResponse sendRequest(const HttpRequestConfig& config);
    HttpResponse get(const String& url, const std::map<String, String>& headers = {});
    HttpResponse post(const String& url, const String& body, const std::map<String, String>& headers = {});
    HttpResponse postJson(const String& url, const String& json, const std::map<String, String>& headers = {});

    // 文件上传（多部分表单）
    bool uploadFile(const String& url, const String& filePath, const String& fieldName = "file",
                    const std::map<String, String>& formFields = {});

    // 流式传输（用于音频流）
    bool startStream(const String& url, const std::map<String, String>& headers = {});
    bool writeStreamChunk(const uint8_t* data, size_t length);
    bool readStreamChunk(uint8_t* buffer, size_t maxLength, size_t& bytesRead);
    bool endStream();

    // 事件管理
    void addEventListener(NetworkEventCallback callback, void* userData = nullptr);
    void removeEventListener(NetworkEventCallback callback);
    void clearEventListeners();

    // 配置管理
    void setConfigManager(ConfigManager* configMgr);
    void setLogger(Logger* log);
    bool updateWiFiConfig(const WiFiConfig& config);
    const WiFiConfig& getWiFiConfig() const { return wifiConfig; }

    // 连接参数
    void setAutoReconnect(bool enable);
    void setReconnectInterval(uint32_t interval);
    bool getAutoReconnect() const { return autoReconnect; }
    uint32_t getReconnectInterval() const { return reconnectInterval; }

    // 清除保存的网络配置
    void clearSavedNetworks();

    // 状态监控
    void update(); // 需要定期调用以处理自动重连等任务
    void printStatus() const;

    // 工具方法
    static String methodToString(HttpMethod method);
    static HttpMethod stringToMethod(const String& methodStr);
    static String eventToString(NetworkEvent event);

    // 诊断
    bool testConnection(const String& testUrl = "http://connectivitycheck.gstatic.com/generate_204");
    bool ping(const String& host, uint32_t timeout = 1000);
    bool checkWiFiHardware(); // 检查Wi-Fi硬件状态
    bool ensureWiFiHardwareReady(); // 确保Wi-Fi硬件就绪

private:
    // 内部HTTP客户端管理
    HTTPClient* getHttpClient();
    void releaseHttpClient(HTTPClient* client);
    void cleanupHttpClients();
};

#endif // NETWORK_MANAGER_H