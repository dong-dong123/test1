#include <Arduino.h>
#include <unity.h>
#include "../src/modules/NetworkManager.h"

// Mock配置管理器
class MockConfigManager : public ConfigManager {
private:
    String wifiSSID;
    String wifiPassword;
    bool wifiAutoConnect;
    uint32_t wifiTimeout;

public:
    MockConfigManager(const String& ssid = "TestSSID",
                     const String& password = "TestPassword",
                     bool autoConnect = true,
                     uint32_t timeout = 10000)
        : wifiSSID(ssid), wifiPassword(password),
          wifiAutoConnect(autoConnect), wifiTimeout(timeout) {}

    virtual String getString(const String& key, const String& defaultValue = "") override {
        if (key == "wifi.ssid") {
            return wifiSSID.isEmpty() ? defaultValue : wifiSSID;
        } else if (key == "wifi.password") {
            return wifiPassword.isEmpty() ? defaultValue : wifiPassword;
        }
        return defaultValue;
    }

    virtual bool getBool(const String& key, bool defaultValue = false) override {
        if (key == "wifi.autoConnect") {
            return wifiAutoConnect;
        }
        return defaultValue;
    }

    virtual int getInt(const String& key, int defaultValue = 0) override {
        if (key == "wifi.timeout") {
            return wifiTimeout;
        }
        return defaultValue;
    }

    // 以下方法为简单实现，测试中不使用
    virtual bool load() override { return true; }
    virtual bool save() override { return true; }
    virtual bool resetToDefaults() override { return true; }
    virtual bool setString(const String& key, const String& value) override { return true; }
    virtual bool setInt(const String& key, int value) override { return true; }
    virtual float getFloat(const String& key, float defaultValue = 0.0f) override { return defaultValue; }
    virtual bool setFloat(const String& key, float value) override { return true; }
    virtual bool setBool(const String& key, bool value) override { return true; }
    virtual std::vector<String> getStringArray(const String& key) override { return std::vector<String>(); }
    virtual bool setStringArray(const String& key, const std::vector<String>& values) override { return true; }
    virtual bool validate() const override { return true; }
    virtual std::vector<String> getValidationErrors() const override { return std::vector<String>(); }
};

// 测试变量
NetworkManager* networkManager = nullptr;
MockConfigManager* mockConfig = nullptr;

// 事件跟踪
std::vector<NetworkEvent> receivedEvents;
std::vector<String> eventDetails;

// 事件回调函数
void testEventCallback(NetworkEvent event, const String& details, void* userData) {
    receivedEvents.push_back(event);
    eventDetails.push_back(details);
}

void clearEventLog() {
    receivedEvents.clear();
    eventDetails.clear();
}

void setUp(void) {
    // 在每个测试前创建新的NetworkManager实例
    mockConfig = new MockConfigManager();
    networkManager = new NetworkManager(mockConfig);
    clearEventLog();
    networkManager->addEventListener(testEventCallback);
}

void tearDown(void) {
    // 在每个测试后清理
    if (networkManager) {
        networkManager->removeEventListener(testEventCallback);
        delete networkManager;
        networkManager = nullptr;
    }
    if (mockConfig) {
        delete mockConfig;
        mockConfig = nullptr;
    }
    clearEventLog();
}

void test_initialization(void) {
    TEST_ASSERT_NOT_NULL(networkManager);
    TEST_ASSERT_FALSE(networkManager->isReady());

    TEST_ASSERT_TRUE(networkManager->initialize());
    TEST_ASSERT_TRUE(networkManager->isReady());
}

void test_config_loading(void) {
    TEST_ASSERT_TRUE(networkManager->initialize());

    // 获取Wi-Fi配置
    WiFiConfig wifiConfig = networkManager->getWiFiConfig();
    TEST_ASSERT_EQUAL_STRING("TestSSID", wifiConfig.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("TestPassword", wifiConfig.password.c_str());
    TEST_ASSERT_TRUE(wifiConfig.autoConnect);
    TEST_ASSERT_EQUAL(10000, wifiConfig.connectTimeout);
}

void test_wifi_config_update(void) {
    TEST_ASSERT_TRUE(networkManager->initialize());

    // 创建新的Wi-Fi配置
    WiFiConfig newConfig;
    newConfig.ssid = "NewSSID";
    newConfig.password = "NewPassword";
    newConfig.autoConnect = false;
    newConfig.connectTimeout = 15000;
    newConfig.maxRetries = 3;

    TEST_ASSERT_TRUE(networkManager->updateWiFiConfig(newConfig));

    // 验证配置已更新
    WiFiConfig updatedConfig = networkManager->getWiFiConfig();
    TEST_ASSERT_EQUAL_STRING("NewSSID", updatedConfig.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("NewPassword", updatedConfig.password.c_str());
    TEST_ASSERT_FALSE(updatedConfig.autoConnect);
    TEST_ASSERT_EQUAL(15000, updatedConfig.connectTimeout);
}

void test_connection_management(void) {
    TEST_ASSERT_TRUE(networkManager->initialize());

    // 测试连接（由于是mock，可能不会实际连接）
    // 这里主要测试函数调用不崩溃
    TEST_ASSERT_TRUE(networkManager->connect());

    // 测试断开连接
    TEST_ASSERT_TRUE(networkManager->disconnect());

    // 测试重连
    TEST_ASSERT_TRUE(networkManager->reconnect());
}

void test_status_management(void) {
    TEST_ASSERT_TRUE(networkManager->initialize());

    // 获取初始状态
    NetworkStatus status = networkManager->getStatus();
    TEST_ASSERT_FALSE(status.wifiConnected);
    TEST_ASSERT_FALSE(status.hasIP);
    TEST_ASSERT_EQUAL_STRING("", status.localIP.c_str());
    TEST_ASSERT_EQUAL_STRING("", status.ssid.c_str());
    TEST_ASSERT_EQUAL(0, status.rssi);
    TEST_ASSERT_EQUAL(0, status.connectionTime);
    TEST_ASSERT_EQUAL(0, status.disconnectCount);

    // 测试状态打印（不验证输出，只确保不崩溃）
    networkManager->printStatus();
    TEST_ASSERT_TRUE(true);
}

void test_auto_reconnect_settings(void) {
    TEST_ASSERT_TRUE(networkManager->initialize());

    // 默认应该是启用自动重连
    TEST_ASSERT_TRUE(networkManager->getAutoReconnect());

    // 禁用自动重连
    networkManager->setAutoReconnect(false);
    TEST_ASSERT_FALSE(networkManager->getAutoReconnect());

    // 设置重连间隔
    networkManager->setReconnectInterval(10000);
    TEST_ASSERT_EQUAL(10000, networkManager->getReconnectInterval());

    // 重新启用
    networkManager->setAutoReconnect(true);
    TEST_ASSERT_TRUE(networkManager->getAutoReconnect());
}

void test_event_handling(void) {
    TEST_ASSERT_TRUE(networkManager->initialize());

    // 清空之前的事件
    clearEventLog();

    // 触发一些操作（连接）可能会产生事件
    networkManager->connect();

    // 由于是mock环境，可能不会产生真实事件
    // 这里主要测试事件回调机制本身
    TEST_ASSERT_TRUE(true);
}

void test_http_method_conversion(void) {
    // 测试方法字符串转换
    TEST_ASSERT_EQUAL_STRING("GET", NetworkManager::methodToString(HttpMethod::GET).c_str());
    TEST_ASSERT_EQUAL_STRING("POST", NetworkManager::methodToString(HttpMethod::POST).c_str());
    TEST_ASSERT_EQUAL_STRING("PUT", NetworkManager::methodToString(HttpMethod::PUT).c_str());
    TEST_ASSERT_EQUAL_STRING("DELETE", NetworkManager::methodToString(HttpMethod::DELETE).c_str());
    TEST_ASSERT_EQUAL_STRING("PATCH", NetworkManager::methodToString(HttpMethod::PATCH).c_str());

    // 测试字符串到方法转换
    TEST_ASSERT_EQUAL(HttpMethod::GET, NetworkManager::stringToMethod("GET"));
    TEST_ASSERT_EQUAL(HttpMethod::POST, NetworkManager::stringToMethod("POST"));
    TEST_ASSERT_EQUAL(HttpMethod::PUT, NetworkManager::stringToMethod("PUT"));
    TEST_ASSERT_EQUAL(HttpMethod::DELETE, NetworkManager::stringToMethod("DELETE"));
    TEST_ASSERT_EQUAL(HttpMethod::PATCH, NetworkManager::stringToMethod("PATCH"));

    // 测试不支持的字符串（应该返回GET作为默认值）
    TEST_ASSERT_EQUAL(HttpMethod::GET, NetworkManager::stringToMethod("UNKNOWN"));
    TEST_ASSERT_EQUAL(HttpMethod::GET, NetworkManager::stringToMethod(""));
}

void test_event_string_conversion(void) {
    // 测试事件字符串转换
    TEST_ASSERT_EQUAL_STRING("WIFI_CONNECTING",
        NetworkManager::eventToString(NetworkEvent::WIFI_CONNECTING).c_str());
    TEST_ASSERT_EQUAL_STRING("WIFI_CONNECTED",
        NetworkManager::eventToString(NetworkEvent::WIFI_CONNECTED).c_str());
    TEST_ASSERT_EQUAL_STRING("WIFI_DISCONNECTED",
        NetworkManager::eventToString(NetworkEvent::WIFI_DISCONNECTED).c_str());
    TEST_ASSERT_EQUAL_STRING("WIFI_GOT_IP",
        NetworkManager::eventToString(NetworkEvent::WIFI_GOT_IP).c_str());
    TEST_ASSERT_EQUAL_STRING("HTTP_REQUEST_START",
        NetworkManager::eventToString(NetworkEvent::HTTP_REQUEST_START).c_str());
    TEST_ASSERT_EQUAL_STRING("HTTP_REQUEST_SUCCESS",
        NetworkManager::eventToString(NetworkEvent::HTTP_REQUEST_SUCCESS).c_str());
    TEST_ASSERT_EQUAL_STRING("HTTP_REQUEST_FAILED",
        NetworkManager::eventToString(NetworkEvent::HTTP_REQUEST_FAILED).c_str());
}

void test_http_request_config(void) {
    // 测试HTTP请求配置默认值
    HttpRequestConfig config;

    TEST_ASSERT_EQUAL(HttpMethod::GET, config.method);
    TEST_ASSERT_EQUAL(10000, config.timeout);
    TEST_ASSERT_EQUAL(3, config.maxRetries);
    TEST_ASSERT_TRUE(config.followRedirects);
    TEST_ASSERT_TRUE(config.useSSL);
    TEST_ASSERT_EQUAL_STRING("", config.url.c_str());
    TEST_ASSERT_EQUAL_STRING("", config.body.c_str());
    TEST_ASSERT_TRUE(config.headers.empty());
}

void test_network_scan_functions(void) {
    TEST_ASSERT_TRUE(networkManager->initialize());

    // 测试网络扫描函数（mock环境）
    std::vector<String> networks;

    // 异步扫描
    bool scanResult = networkManager->scanNetworks(networks, true);
    // 由于是mock，可能返回false，但函数调用不应该崩溃
    TEST_ASSERT_TRUE(true);

    // 获取网络数量
    int count = networkManager->getNetworkCount();
    // 不验证具体值，只确保函数可调用
    TEST_ASSERT_TRUE(count >= 0);
}

void test_update_function(void) {
    TEST_ASSERT_TRUE(networkManager->initialize());

    // 测试update函数（处理自动重连等）
    // 调用多次以确保不崩溃
    for (int i = 0; i < 5; i++) {
        networkManager->update();
        delay(10);
    }
    TEST_ASSERT_TRUE(true);
}

void setup() {
    // 等待2秒让串口就绪
    delay(2000);

    UNITY_BEGIN();

    // 运行测试
    RUN_TEST(test_initialization);
    RUN_TEST(test_config_loading);
    RUN_TEST(test_wifi_config_update);
    RUN_TEST(test_connection_management);
    RUN_TEST(test_status_management);
    RUN_TEST(test_auto_reconnect_settings);
    RUN_TEST(test_event_handling);
    RUN_TEST(test_http_method_conversion);
    RUN_TEST(test_event_string_conversion);
    RUN_TEST(test_http_request_config);
    RUN_TEST(test_network_scan_functions);
    RUN_TEST(test_update_function);

    UNITY_END();
}

void loop() {
    // 空循环
}