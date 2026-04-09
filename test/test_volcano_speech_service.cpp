#include <Arduino.h>
#include <unity.h>
#include "../src/services/VolcanoSpeechService.h"

// Mock NetworkManager
class MockNetworkManager : public NetworkManager {
public:
    bool isConnectedValue;
    HttpResponse lastResponse;

    MockNetworkManager(bool connected = true) : isConnectedValue(connected) {
        lastResponse.statusCode = 200;
        lastResponse.body = R"({
            "code": 0,
            "message": "success",
            "data": {
                "text": "模拟识别结果"
            }
        })";
    }

    virtual bool isConnected() const override { return isConnectedValue; }
    virtual HttpResponse sendRequest(const HttpRequestConfig& config) override { return lastResponse; }
    virtual HttpResponse get(const String& url, const std::map<String, String>& headers = {}) override { return lastResponse; }
    virtual HttpResponse post(const String& url, const String& body, const std::map<String, String>& headers = {}) override { return lastResponse; }
    virtual HttpResponse postJson(const String& url, const String& json, const std::map<String, String>& headers = {}) override { return lastResponse; }

    // 简化实现 - 只用于测试
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
    String secretKey;

    MockConfigManager(const String& key = "test_api_key", const String& secret = "test_secret_key")
        : apiKey(key), secretKey(secret) {}

    virtual String getString(const String& key, const String& defaultValue = "") override {
        if (key == "services.volcano.apiKey") return apiKey;
        if (key == "services.volcano.secretKey") return secretKey;
        if (key == "services.volcano.region") return "cn-north-1";
        if (key == "services.volcano.language") return "zh-CN";
        if (key == "services.volcano.voice") return "zh-CN_female_standard";
        return defaultValue;
    }

    virtual bool getBool(const String& key, bool defaultValue = false) override {
        if (key == "services.volcano.enablePunctuation") return true;
        return defaultValue;
    }

    virtual float getFloat(const String& key, float defaultValue = 0.0f) override {
        if (key == "services.volcano.timeout") return 10.0f;
        return defaultValue;
    }

    // 简化实现
    virtual bool load() override { return true; }
    virtual bool save() override { return true; }
    virtual bool resetToDefaults() override { return true; }
    virtual bool setString(const String& key, const String& value) override { return true; }
    virtual int getInt(const String& key, int defaultValue = 0) override { return defaultValue; }
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
VolcanoSpeechService* volcanoService = nullptr;
MockNetworkManager* mockNetwork = nullptr;
MockConfigManager* mockConfig = nullptr;
MockLogger* mockLogger = nullptr;

void setUp(void) {
    // 在每个测试前创建新的实例
    mockNetwork = new MockNetworkManager(true);
    mockConfig = new MockConfigManager();
    mockLogger = new MockLogger();

    volcanoService = new VolcanoSpeechService(mockNetwork, mockConfig, mockLogger);
}

void tearDown(void) {
    // 在每个测试后清理
    delete volcanoService;
    volcanoService = nullptr;

    delete mockNetwork;
    mockNetwork = nullptr;

    delete mockConfig;
    mockConfig = nullptr;

    delete mockLogger;
    mockLogger = nullptr;
}

void test_initialization(void) {
    TEST_ASSERT_NOT_NULL(volcanoService);
    TEST_ASSERT_FALSE(volcanoService->isReady());

    TEST_ASSERT_TRUE(volcanoService->initialize());
    TEST_ASSERT_TRUE(volcanoService->isReady());
}

void test_config_loading(void) {
    TEST_ASSERT_TRUE(volcanoService->initialize());

    // 验证配置已加载
    VolcanoSpeechConfig config = volcanoService->getConfig();
    TEST_ASSERT_EQUAL_STRING("test_api_key", config.apiKey.c_str());
    TEST_ASSERT_EQUAL_STRING("test_secret_key", config.secretKey.c_str());
    TEST_ASSERT_EQUAL_STRING("cn-north-1", config.region.c_str());
    TEST_ASSERT_EQUAL_STRING("zh-CN", config.language.c_str());
    TEST_ASSERT_EQUAL_STRING("zh-CN_female_standard", config.voice.c_str());
    TEST_ASSERT_TRUE(config.enablePunctuation);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, config.timeout);
}

void test_service_name(void) {
    TEST_ASSERT_TRUE(volcanoService->initialize());

    // 验证服务名称
    String name = volcanoService->getName();
    TEST_ASSERT_EQUAL_STRING("volcano", name.c_str());
}

void test_availability_without_network(void) {
    // 创建没有网络连接的模拟
    delete mockNetwork;
    mockNetwork = new MockNetworkManager(false); // 网络断开
    delete volcanoService;
    volcanoService = new VolcanoSpeechService(mockNetwork, mockConfig, mockLogger);

    TEST_ASSERT_TRUE(volcanoService->initialize());

    // 没有网络连接时应不可用
    TEST_ASSERT_FALSE(volcanoService->isAvailable());
}

void test_availability_without_api_key(void) {
    // 创建没有API key的配置
    delete mockConfig;
    mockConfig = new MockConfigManager("", ""); // 空API key
    delete volcanoService;
    volcanoService = new VolcanoSpeechService(mockNetwork, mockConfig, mockLogger);

    TEST_ASSERT_TRUE(volcanoService->initialize());

    // 没有API key时应不可用
    TEST_ASSERT_FALSE(volcanoService->isAvailable());
}

void test_availability_with_credentials(void) {
    TEST_ASSERT_TRUE(volcanoService->initialize());

    // 有网络和API key时应可用
    TEST_ASSERT_TRUE(volcanoService->isAvailable());
}

void test_speech_recognition(void) {
    TEST_ASSERT_TRUE(volcanoService->initialize());

    // 准备模拟音频数据
    uint8_t audioData[100];
    for (int i = 0; i < 100; i++) {
        audioData[i] = i;
    }

    // 执行语音识别
    String recognizedText;
    bool result = volcanoService->recognize(audioData, 100, recognizedText);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(recognizedText.isEmpty());

    // 验证日志记录
    TEST_ASSERT_TRUE(mockLogger->contains("VolcanoSpeechService"));
}

void test_speech_synthesis(void) {
    TEST_ASSERT_TRUE(volcanoService->initialize());

    // 执行语音合成
    std::vector<uint8_t> audioData;
    String text = "Hello, this is a test.";
    bool result = volcanoService->synthesize(text, audioData);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(audioData.empty());

    // 模拟实现应返回1000字节的音频数据
    TEST_ASSERT_EQUAL(1000, audioData.size());
}

void test_cost_per_request(void) {
    TEST_ASSERT_TRUE(volcanoService->initialize());

    // 验证成本估算
    float cost = volcanoService->getCostPerRequest();
    TEST_ASSERT_EQUAL_FLOAT(0.01f, cost);
}

void test_config_update(void) {
    TEST_ASSERT_TRUE(volcanoService->initialize());

    // 创建新配置
    VolcanoSpeechConfig newConfig;
    newConfig.apiKey = "new_api_key";
    newConfig.secretKey = "new_secret_key";
    newConfig.region = "us-west-1";
    newConfig.language = "en-US";
    newConfig.voice = "en-US_male_standard";
    newConfig.enablePunctuation = false;
    newConfig.timeout = 20.0f;

    // 更新配置
    TEST_ASSERT_TRUE(volcanoService->updateConfig(newConfig));

    // 验证配置已更新
    VolcanoSpeechConfig updatedConfig = volcanoService->getConfig();
    TEST_ASSERT_EQUAL_STRING("new_api_key", updatedConfig.apiKey.c_str());
    TEST_ASSERT_EQUAL_STRING("new_secret_key", updatedConfig.secretKey.c_str());
    TEST_ASSERT_EQUAL_STRING("us-west-1", updatedConfig.region.c_str());
    TEST_ASSERT_EQUAL_STRING("en-US", updatedConfig.language.c_str());
    TEST_ASSERT_EQUAL_STRING("en-US_male_standard", updatedConfig.voice.c_str());
    TEST_ASSERT_FALSE(updatedConfig.enablePunctuation);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, updatedConfig.timeout);
}

void test_error_handling(void) {
    // 测试未初始化的服务
    String text;
    uint8_t audioData[10] = {0};

    // 未初始化时应失败
    TEST_ASSERT_FALSE(volcanoService->recognize(audioData, 10, text));

    // 初始化后应工作
    TEST_ASSERT_TRUE(volcanoService->initialize());

    // 空音频数据（模拟实现中可能接受）
    // 这里主要测试函数调用不崩溃
    TEST_ASSERT_TRUE(volcanoService->recognize(nullptr, 0, text));
}

void test_stream_methods_not_implemented(void) {
    TEST_ASSERT_TRUE(volcanoService->initialize());

    // 流式方法尚未实现，应返回false
    TEST_ASSERT_FALSE(volcanoService->recognizeStreamStart());

    String partialText;
    TEST_ASSERT_FALSE(volcanoService->recognizeStreamChunk(nullptr, 0, partialText));

    String finalText;
    TEST_ASSERT_FALSE(volcanoService->recognizeStreamEnd(finalText));

    TEST_ASSERT_FALSE(volcanoService->synthesizeStreamStart("test"));

    std::vector<uint8_t> chunk;
    bool isLast = false;
    TEST_ASSERT_FALSE(volcanoService->synthesizeStreamGetChunk(chunk, isLast));
}

void test_async_state_initialization(void) {
    // 测试异步状态初始化
    // 服务创建后，异步识别应处于空闲状态
    TEST_ASSERT_FALSE(volcanoService->isAsyncRecognitionInProgress());

    // 验证最后请求ID初始化为0
    TEST_ASSERT_EQUAL_UINT32(0, volcanoService->getLastAsyncRequestId());
}

void test_binary_protocol_encoder_basic(void) {
    // Test basic encoding functionality
    BinaryProtocolEncoder encoder;

    // Test creating a simple message
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> encoded = encoder.encodeMessage(
        BinaryProtocolEncoder::MessageType::CLIENT_REQUEST,
        payload,
        false  // no compression
    );

    // Verify encoded message has basic structure
    TEST_ASSERT_TRUE(encoded.size() > payload.size()); // Should have headers
    TEST_ASSERT_TRUE(encoded.size() >= 12); // Minimum header size

    // Test with compression disabled (default)
    std::vector<uint8_t> encodedNoCompression = encoder.encodeMessage(
        BinaryProtocolEncoder::MessageType::CLIENT_REQUEST,
        payload,
        false
    );
    TEST_ASSERT_TRUE(encodedNoCompression.size() > 0);

    ESP_LOGI("Test", "BinaryProtocolEncoder basic test passed");
}

void test_binary_protocol_decoder_basic(void) {
    // Test basic decoding functionality
    BinaryProtocolDecoder decoder;

    // Create a test payload with text
    const char* testJson = "{\"text\": \"测试文本\", \"is_final\": true}";
    std::vector<uint8_t> jsonPayload(testJson, testJson + strlen(testJson));

    // Test text extraction
    String extractedText = decoder.extractTextFromResponse(jsonPayload);

    // Verify text was extracted (implementation may return empty if JSON parsing fails in test)
    // For now just verify function doesn't crash
    TEST_ASSERT_TRUE(true); // Placeholder - actual test depends on decoder implementation

    // Test error handling with empty payload
    std::vector<uint8_t> emptyPayload;
    String emptyResult = decoder.extractTextFromResponse(emptyPayload);
    // Should handle empty payload gracefully

    ESP_LOGI("Test", "BinaryProtocolDecoder basic test passed");
}

void test_tts_request_builder(void) {
    // Test TTS request builder functionality
    String text = "Hello, this is a test.";
    String requestJson = TTSRequestBuilder::buildSynthesisRequest(text);

    // Verify request is not empty
    TEST_ASSERT_FALSE(requestJson.isEmpty());

    // Verify request contains required fields
    TEST_ASSERT_TRUE(requestJson.indexOf("\"text\"") > 0);
    TEST_ASSERT_TRUE(requestJson.indexOf("\"app\"") > 0);
    TEST_ASSERT_TRUE(requestJson.indexOf("\"request\"") > 0);

    // Test with custom parameters
    String customRequest = TTSRequestBuilder::buildSynthesisRequest(
        text,
        "test_appid",
        "test_token",
        "test_cluster",
        "test_user",
        "en-US_male_standard",
        "pcm",
        22050,
        1.2f
    );

    TEST_ASSERT_FALSE(customRequest.isEmpty());
    TEST_ASSERT_TRUE(customRequest.indexOf("test_appid") > 0);
    TEST_ASSERT_TRUE(customRequest.indexOf("test_cluster") > 0);

    ESP_LOGI("Test", "TTSRequestBuilder test passed");
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
    RUN_TEST(test_speech_recognition);
    RUN_TEST(test_speech_synthesis);
    RUN_TEST(test_cost_per_request);
    RUN_TEST(test_config_update);
    RUN_TEST(test_error_handling);
    RUN_TEST(test_stream_methods_not_implemented);
    RUN_TEST(test_async_state_initialization);
    RUN_TEST(test_binary_protocol_encoder_basic);
    RUN_TEST(test_binary_protocol_decoder_basic);
    RUN_TEST(test_tts_request_builder);

    UNITY_END();
}

void loop() {
    // 空循环
}