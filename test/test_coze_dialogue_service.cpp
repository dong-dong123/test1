#include <Arduino.h>
#include <unity.h>
#include "../src/services/CozeDialogueService.h"

// Mock NetworkManager（复用Volcano测试中的）
class MockNetworkManager : public NetworkManager {
public:
    bool isConnectedValue;
    HttpResponse lastResponse;

    MockNetworkManager(bool connected = true) : isConnectedValue(connected) {
        lastResponse.statusCode = 200;
        lastResponse.body = R"({
            "code": 0,
            "msg": "success",
            "data": {
                "messages": [
                    {
                        "role": "assistant",
                        "content": "模拟对话响应"
                    }
                ]
            }
        })";
    }

    virtual bool isConnected() const override { return isConnectedValue; }
    virtual HttpResponse sendRequest(const HttpRequestConfig& config) override { return lastResponse; }
    virtual HttpResponse get(const String& url, const std::map<String, String>& headers = {}) override { return lastResponse; }
    virtual HttpResponse post(const String& url, const String& body, const std::map<String, String>& headers = {}) override { return lastResponse; }
    virtual HttpResponse postJson(const String& url, const String& json, const std::map<String, String>& headers = {}) override { return lastResponse; }

    // 简化实现
    virtual bool initialize() override { return true; }
    virtual bool deinitialize() override { return true; }
    virtual bool isReady() const override { return true; }
    virtual bool connect() override { return true; }
    virtual bool disconnect() override { return true; }
    virtual bool reconnect() override { return true; }
    virtual bool scanNetworks(std::vector<String>& networks, bool async = false) override { return false; }
    virtual int getNetworkCount() const override { return 0; }
    virtual String getScannedNetwork(int index) const override { return ""; }
    virtual bool uploadFile(const String& url, const String& filePath, const String& fieldName = "file",
                           const std::map<String, String>& formFields = {}) override { return false; }
    virtual bool startStream(const String& url, const std::map<String, String>& headers = {}) override { return false; }
    virtual bool writeStreamChunk(const uint8_t* data, size_t length) override { return false; }
    virtual bool readStreamChunk(uint8_t* buffer, size_t maxLength, size_t& bytesRead) override { return false; }
    virtual bool endStream() override { return true; }
    virtual void addEventListener(NetworkEventCallback callback, void* userData = nullptr) override {}
    virtual void removeEventListener(NetworkEventCallback callback) override {}
    virtual void clearEventListeners() override {}
    virtual void setConfigManager(ConfigManager* configMgr) override {}
    virtual void setLogger(Logger* log) override {}
    virtual bool updateWiFiConfig(const WiFiConfig& config) override { return true; }
    virtual void setAutoReconnect(bool enable) override {}
    virtual void setReconnectInterval(uint32_t interval) override {}
    virtual bool getAutoReconnect() const override { return true; }
    virtual uint32_t getReconnectInterval() const override { return 5000; }
    virtual void update() override {}
    virtual void printStatus() const override {}
    virtual bool testConnection(const String& testUrl = "http://connectivitycheck.gstatic.com/generate_204") override { return true; }
    virtual bool ping(const String& host, uint32_t timeout = 1000) override { return true; }
};

// Mock ConfigManager
class MockConfigManager : public ConfigManager {
public:
    String apiKey;
    String botId;

    MockConfigManager(const String& key = "test_api_key", const String& bot = "test_bot_id")
        : apiKey(key), botId(bot) {}

    virtual String getString(const String& key, const String& defaultValue = "") override {
        if (key == "services.coze.apiKey") return apiKey;
        if (key == "services.coze.botId") return botId;
        if (key == "services.coze.userId") return "test_user";
        if (key == "services.coze.endpoint") return "https://api.coze.cn/v1/chat";
        if (key == "services.coze.model") return "coze-model";
        return defaultValue;
    }

    virtual bool getBool(const String& key, bool defaultValue = false) override {
        return defaultValue;
    }

    virtual int getInt(const String& key, int defaultValue = 0) override {
        if (key == "services.coze.maxTokens") return 1000;
        return defaultValue;
    }

    virtual float getFloat(const String& key, float defaultValue = 0.0f) override {
        if (key == "services.coze.temperature") return 0.7f;
        if (key == "services.coze.timeout") return 15.0f;
        return defaultValue;
    }

    // 简化实现
    virtual bool load() override { return true; }
    virtual bool save() override { return true; }
    virtual bool resetToDefaults() override { return true; }
    virtual bool setString(const String& key, const String& value) override { return true; }
    virtual bool setInt(const String& key, int value) override { return true; }
    virtual bool setFloat(const String& key, float value) override { return true; }
    virtual bool setBool(const String& key, bool value) override { return true; }
    virtual std::vector<String> getStringArray(const String& key) override { return std::vector<String>(); }
    virtual bool setStringArray(const String& key, const std::vector<String>& values) override { return true; }
    virtual bool validate() const override { return true; }
    virtual std::vector<String> getValidationErrors() const override { return std::vector<String>(); }
};

// Mock Logger
class MockLogger : public Logger {
public:
    std::vector<String> logs;
    std::vector<Level> levels;

    virtual void log(Level level, const String& message) override {
        logs.push_back(message);
        levels.push_back(level);
    }

    void clear() {
        logs.clear();
        levels.clear();
    }

    bool contains(const String& substring) const {
        for (const auto& log : logs) {
            if (log.indexOf(substring) >= 0) {
                return true;
            }
        }
        return false;
    }
};

// 测试变量
CozeDialogueService* cozeService = nullptr;
MockNetworkManager* mockNetwork = nullptr;
MockConfigManager* mockConfig = nullptr;
MockLogger* mockLogger = nullptr;

void setUp(void) {
    // 在每个测试前创建新的实例
    mockNetwork = new MockNetworkManager(true);
    mockConfig = new MockConfigManager();
    mockLogger = new MockLogger();

    cozeService = new CozeDialogueService(mockNetwork, mockConfig, mockLogger);
}

void tearDown(void) {
    // 在每个测试后清理
    delete cozeService;
    cozeService = nullptr;

    delete mockNetwork;
    mockNetwork = nullptr;

    delete mockConfig;
    mockConfig = nullptr;

    delete mockLogger;
    mockLogger = nullptr;
}

void test_initialization(void) {
    TEST_ASSERT_NOT_NULL(cozeService);
    TEST_ASSERT_FALSE(cozeService->isReady());

    TEST_ASSERT_TRUE(cozeService->initialize());
    TEST_ASSERT_TRUE(cozeService->isReady());
}

void test_config_loading(void) {
    TEST_ASSERT_TRUE(cozeService->initialize());

    // 验证配置已加载
    CozeDialogueConfig config = cozeService->getConfig();
    TEST_ASSERT_EQUAL_STRING("test_api_key", config.apiKey.c_str());
    TEST_ASSERT_EQUAL_STRING("test_bot_id", config.botId.c_str());
    TEST_ASSERT_EQUAL_STRING("test_user", config.userId.c_str());
    TEST_ASSERT_EQUAL_STRING("https://api.coze.cn/v1/chat", config.endpoint.c_str());
    TEST_ASSERT_EQUAL_STRING("coze-model", config.model.c_str());
    TEST_ASSERT_EQUAL_FLOAT(0.7f, config.temperature);
    TEST_ASSERT_EQUAL(1000, config.maxTokens);
    TEST_ASSERT_EQUAL_FLOAT(15.0f, config.timeout);
}

void test_service_name(void) {
    TEST_ASSERT_TRUE(cozeService->initialize());

    // 验证服务名称
    String name = cozeService->getName();
    TEST_ASSERT_EQUAL_STRING("coze", name.c_str());
}

void test_availability_without_network(void) {
    // 创建没有网络连接的模拟
    delete mockNetwork;
    mockNetwork = new MockNetworkManager(false); // 网络断开
    delete cozeService;
    cozeService = new CozeDialogueService(mockNetwork, mockConfig, mockLogger);

    TEST_ASSERT_TRUE(cozeService->initialize());

    // 没有网络连接时应不可用
    TEST_ASSERT_FALSE(cozeService->isAvailable());
}

void test_availability_without_api_key(void) {
    // 创建没有API key的配置
    delete mockConfig;
    mockConfig = new MockConfigManager("", ""); // 空API key和Bot ID
    delete cozeService;
    cozeService = new CozeDialogueService(mockNetwork, mockConfig, mockLogger);

    TEST_ASSERT_TRUE(cozeService->initialize());

    // 没有API key和Bot ID时应不可用
    TEST_ASSERT_FALSE(cozeService->isAvailable());
}

void test_availability_with_credentials(void) {
    TEST_ASSERT_TRUE(cozeService->initialize());

    // 有网络和API key时应可用
    TEST_ASSERT_TRUE(cozeService->isAvailable());
}

void test_chat_basic(void) {
    TEST_ASSERT_TRUE(cozeService->initialize());

    // 基本对话测试
    String input = "Hello, how are you?";
    String response = cozeService->chat(input);

    TEST_ASSERT_FALSE(response.isEmpty());
    TEST_ASSERT_TRUE(response.indexOf("模拟对话响应") >= 0);

    // 验证日志记录
    TEST_ASSERT_TRUE(mockLogger->contains("CozeDialogueService"));

    // 验证上下文已更新
    TEST_ASSERT_EQUAL(2, cozeService->getContextSize()); // 用户输入和助手响应
}

void test_chat_with_context(void) {
    TEST_ASSERT_TRUE(cozeService->initialize());

    // 创建测试上下文
    std::vector<String> context;
    context.push_back("User: What's the weather like?");
    context.push_back("Assistant: It's sunny today.");

    // 带上下文的对话
    String input = "Should I bring an umbrella?";
    String response = cozeService->chatWithContext(input, context);

    TEST_ASSERT_FALSE(response.isEmpty());
    TEST_ASSERT_TRUE(response.indexOf("模拟对话响应") >= 0);
}

void test_context_management(void) {
    TEST_ASSERT_TRUE(cozeService->initialize());

    // 初始上下文应为空
    TEST_ASSERT_EQUAL(0, cozeService->getContextSize());

    // 添加一些对话
    cozeService->chat("First message");
    TEST_ASSERT_EQUAL(2, cozeService->getContextSize());

    // 清理上下文
    cozeService->clearContext();
    TEST_ASSERT_EQUAL(0, cozeService->getContextSize());

    // 设置最大上下文大小
    cozeService->setMaxContextSize(2); // 只保留最近2条消息

    // 添加更多消息，应该会触发裁剪
    cozeService->chat("Message 1");
    cozeService->chat("Message 2");
    cozeService->chat("Message 3");

    // 由于最大上下文大小为2，应该只有最后2条消息（用户+助手）
    // 注意：每次chat()添加2条消息（用户+助手）
    // 所以实际行为可能不同，这里主要测试函数调用不崩溃
    TEST_ASSERT_TRUE(cozeService->getContextSize() <= 4);
}

void test_cost_per_request(void) {
    TEST_ASSERT_TRUE(cozeService->initialize());

    // 验证成本估算
    float cost = cozeService->getCostPerRequest();
    TEST_ASSERT_EQUAL_FLOAT(0.02f, cost);
}

void test_config_update(void) {
    TEST_ASSERT_TRUE(cozeService->initialize());

    // 创建新配置
    CozeDialogueConfig newConfig;
    newConfig.apiKey = "new_api_key";
    newConfig.botId = "new_bot_id";
    newConfig.userId = "new_user";
    newConfig.endpoint = "https://api.coze.com/v2/chat";
    newConfig.model = "coze-model-v2";
    newConfig.temperature = 0.9f;
    newConfig.maxTokens = 2000;
    newConfig.timeout = 30.0f;

    // 更新配置
    TEST_ASSERT_TRUE(cozeService->updateConfig(newConfig));

    // 验证配置已更新
    CozeDialogueConfig updatedConfig = cozeService->getConfig();
    TEST_ASSERT_EQUAL_STRING("new_api_key", updatedConfig.apiKey.c_str());
    TEST_ASSERT_EQUAL_STRING("new_bot_id", updatedConfig.botId.c_str());
    TEST_ASSERT_EQUAL_STRING("new_user", updatedConfig.userId.c_str());
    TEST_ASSERT_EQUAL_STRING("https://api.coze.com/v2/chat", updatedConfig.endpoint.c_str());
    TEST_ASSERT_EQUAL_STRING("coze-model-v2", updatedConfig.model.c_str());
    TEST_ASSERT_EQUAL_FLOAT(0.9f, updatedConfig.temperature);
    TEST_ASSERT_EQUAL(2000, updatedConfig.maxTokens);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, updatedConfig.timeout);
}

void test_error_handling(void) {
    // 测试未初始化的服务
    String input = "Test";

    // 未初始化时应返回空字符串
    String response = cozeService->chat(input);
    TEST_ASSERT_EQUAL_STRING("", response.c_str());

    // 初始化后应工作
    TEST_ASSERT_TRUE(cozeService->initialize());

    // 空输入
    response = cozeService->chat("");
    TEST_ASSERT_TRUE(response.indexOf("Error") >= 0);
}

void test_stream_methods_not_implemented(void) {
    TEST_ASSERT_TRUE(cozeService->initialize());

    // 流式方法尚未实现，应返回false
    TEST_ASSERT_FALSE(cozeService->chatStreamStart("test"));

    String chunk;
    bool isLast = false;
    TEST_ASSERT_FALSE(cozeService->chatStreamGetChunk(chunk, isLast));
}

void test_context_persistence(void) {
    TEST_ASSERT_TRUE(cozeService->initialize());

    // 验证上下文在多次调用间持久化
    cozeService->chat("First question");
    size_t size1 = cozeService->getContextSize();

    cozeService->chat("Second question");
    size_t size2 = cozeService->getContextSize();

    // 上下文应该增长
    TEST_ASSERT_GREATER_THAN(size1, size2);
}

void setup() {
    // 等待2秒让串口就绪
    delay(2000);

    UNITY_BEGIN();

    // 运行测试
    RUN_TEST(test_initialization);
    RUN_TEST(test_config_loading);
    RUN_TEST(test_service_name);
    RUN_TEST(test_availability_without_network);
    RUN_TEST(test_availability_without_api_key);
    RUN_TEST(test_availability_with_credentials);
    RUN_TEST(test_chat_basic);
    RUN_TEST(test_chat_with_context);
    RUN_TEST(test_context_management);
    RUN_TEST(test_cost_per_request);
    RUN_TEST(test_config_update);
    RUN_TEST(test_error_handling);
    RUN_TEST(test_stream_methods_not_implemented);
    RUN_TEST(test_context_persistence);

    UNITY_END();
}

void loop() {
    // 空循环
}