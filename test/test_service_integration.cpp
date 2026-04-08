#include <Arduino.h>
#include <unity.h>
#include "../src/modules/ServiceManager.h"
#include "../src/services/VolcanoSpeechService.h"
#include "../src/services/CozeDialogueService.h"

// 复用现有的Mock类（与test_volcano_speech_service.cpp和test_coze_dialogue_service.cpp相同）
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

// Mock ConfigManager for Volcano
class MockVolcanoConfigManager : public ConfigManager {
public:
    String apiKey;
    String secretKey;

    MockVolcanoConfigManager(const String& key = "test_api_key", const String& secret = "test_secret_key")
        : apiKey(key), secretKey(secret) {}

    virtual String getString(const String& key, const String& defaultValue = "") override {
        if (key == "services.volcano.apiKey") return apiKey;
        if (key == "services.volcano.secretKey") return secretKey;
        if (key == "services.volcano.region") return "cn-north-1";
        if (key == "services.volcano.language") return "zh-CN";
        if (key == "services.volcano.voice") return "zh-CN_female_standard";
        if (key == "services.defaultSpeechService") return "volcano";
        if (key == "services.defaultDialogueService") return "coze";
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

// Mock ConfigManager for Coze (扩展以支持coze配置)
class MockCozeConfigManager : public ConfigManager {
public:
    String apiKey;
    String botId;
    String userId;

    MockCozeConfigManager(const String& key = "test_coze_api_key",
                         const String& bot = "test_bot_id",
                         const String& user = "test_user_id")
        : apiKey(key), botId(bot), userId(user) {}

    virtual String getString(const String& key, const String& defaultValue = "") override {
        if (key == "services.coze.apiKey") return apiKey;
        if (key == "services.coze.botId") return botId;
        if (key == "services.coze.userId") return userId;
        if (key == "services.coze.endpoint") return "https://api.coze.cn/v1/chat";
        if (key == "services.coze.model") return "coze-model";
        if (key == "services.defaultSpeechService") return "volcano";
        if (key == "services.defaultDialogueService") return "coze";
        return defaultValue;
    }

    virtual float getFloat(const String& key, float defaultValue = 0.0f) override {
        if (key == "services.coze.temperature") return 0.7f;
        if (key == "services.coze.timeout") return 15.0f;
        return defaultValue;
    }

    virtual int getInt(const String& key, int defaultValue = 0) override {
        if (key == "services.coze.maxTokens") return 1000;
        return defaultValue;
    }

    // 简化实现
    virtual bool load() override { return true; }
    virtual bool save() override { return true; }
    virtual bool resetToDefaults() override { return true; }
    virtual bool setString(const String& key, const String& value) override { return true; }
    virtual bool setInt(const String& key, int value) override { return true; }
    virtual bool setFloat(const String& key, float value) override { return true; }
    virtual bool getBool(const String& key, bool defaultValue = false) override { return defaultValue; }
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

// 全局测试变量
ServiceManager* serviceManager = nullptr;
MockNetworkManager* mockNetwork = nullptr;
MockVolcanoConfigManager* mockVolcanoConfig = nullptr;
MockCozeConfigManager* mockCozeConfig = nullptr;
MockLogger* mockLogger = nullptr;
VolcanoSpeechService* volcanoService = nullptr;
CozeDialogueService* cozeService = nullptr;

void setUp(void) {
    // 在每个测试前创建新的实例
    mockNetwork = new MockNetworkManager(true);
    mockVolcanoConfig = new MockVolcanoConfigManager();
    mockCozeConfig = new MockCozeConfigManager();
    mockLogger = new MockLogger();

    // 创建服务实例
    volcanoService = new VolcanoSpeechService(mockNetwork, mockVolcanoConfig, mockLogger);
    cozeService = new CozeDialogueService(mockNetwork, mockCozeConfig, mockLogger);

    // 创建ServiceManager，使用volcano配置作为主要配置（因为ServiceManager只需要一个ConfigManager）
    serviceManager = new ServiceManager(mockVolcanoConfig, mockLogger);
}

void tearDown(void) {
    // 在每个测试后清理
    delete serviceManager;
    serviceManager = nullptr;

    delete volcanoService;
    volcanoService = nullptr;

    delete cozeService;
    cozeService = nullptr;

    delete mockNetwork;
    mockNetwork = nullptr;

    delete mockVolcanoConfig;
    mockVolcanoConfig = nullptr;

    delete mockCozeConfig;
    mockCozeConfig = nullptr;

    delete mockLogger;
    mockLogger = nullptr;
}

// 测试1: ServiceManager初始化
void test_service_manager_initialization(void) {
    TEST_ASSERT_NOT_NULL(serviceManager);
    TEST_ASSERT_FALSE(serviceManager->isReady());

    TEST_ASSERT_TRUE(serviceManager->initialize());
    TEST_ASSERT_TRUE(serviceManager->isReady());
}

// 测试2: 服务注册和发现
void test_service_registration_and_discovery(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());

    // 确保服务初始化
    TEST_ASSERT_TRUE(volcanoService->initialize());
    TEST_ASSERT_TRUE(cozeService->initialize());

    // 注册服务
    TEST_ASSERT_TRUE(serviceManager->registerSpeechService(volcanoService));
    TEST_ASSERT_TRUE(serviceManager->registerDialogueService(cozeService));

    // 验证服务数量
    TEST_ASSERT_EQUAL(1, serviceManager->getTotalSpeechServices());
    TEST_ASSERT_EQUAL(1, serviceManager->getTotalDialogueServices());

    // 验证服务发现
    std::vector<String> speechServices = serviceManager->getAvailableSpeechServices();
    std::vector<String> dialogueServices = serviceManager->getAvailableDialogueServices();

    TEST_ASSERT_EQUAL(1, speechServices.size());
    TEST_ASSERT_EQUAL(1, dialogueServices.size());

    if (!speechServices.empty()) {
        TEST_ASSERT_EQUAL_STRING("volcano", speechServices[0].c_str());
    }

    if (!dialogueServices.empty()) {
        TEST_ASSERT_EQUAL_STRING("coze", dialogueServices[0].c_str());
    }

    // 验证服务可用性检查
    TEST_ASSERT_TRUE(serviceManager->isSpeechServiceAvailable("volcano"));
    TEST_ASSERT_TRUE(serviceManager->isDialogueServiceAvailable("coze"));
    TEST_ASSERT_FALSE(serviceManager->isSpeechServiceAvailable("nonexistent"));
    TEST_ASSERT_FALSE(serviceManager->isDialogueServiceAvailable("nonexistent"));
}

// 测试3: 默认服务选择
void test_default_service_selection(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());
    TEST_ASSERT_TRUE(volcanoService->initialize());
    TEST_ASSERT_TRUE(cozeService->initialize());

    TEST_ASSERT_TRUE(serviceManager->registerSpeechService(volcanoService));
    TEST_ASSERT_TRUE(serviceManager->registerDialogueService(cozeService));

    // 获取默认服务（应该返回配置中指定的默认服务）
    SpeechService* defaultSpeech = serviceManager->getDefaultSpeechService();
    DialogueService* defaultDialogue = serviceManager->getDefaultDialogueService();

    TEST_ASSERT_NOT_NULL(defaultSpeech);
    TEST_ASSERT_NOT_NULL(defaultDialogue);

    TEST_ASSERT_EQUAL_STRING("volcano", defaultSpeech->getName().c_str());
    TEST_ASSERT_EQUAL_STRING("coze", defaultDialogue->getName().c_str());

    // 测试按名称获取服务
    SpeechService* speechByName = serviceManager->getSpeechService("volcano");
    DialogueService* dialogueByName = serviceManager->getDialogueService("coze");

    TEST_ASSERT_NOT_NULL(speechByName);
    TEST_ASSERT_NOT_NULL(dialogueByName);
    TEST_ASSERT_EQUAL_STRING("volcano", speechByName->getName().c_str());
    TEST_ASSERT_EQUAL_STRING("coze", dialogueByName->getName().c_str());
}

// 测试4: 健康状态检查
void test_health_status(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());
    TEST_ASSERT_TRUE(volcanoService->initialize());
    TEST_ASSERT_TRUE(cozeService->initialize());

    TEST_ASSERT_TRUE(serviceManager->registerSpeechService(volcanoService));
    TEST_ASSERT_TRUE(serviceManager->registerDialogueService(cozeService));

    // 获取健康状态
    ServiceStatus speechStatus = serviceManager->getSpeechServiceStatus("volcano");
    ServiceStatus dialogueStatus = serviceManager->getDialogueServiceStatus("coze");

    // 初始状态应该是INITIALIZING
    TEST_ASSERT_EQUAL(ServiceStatus::INITIALIZING, speechStatus);
    TEST_ASSERT_EQUAL(ServiceStatus::INITIALIZING, dialogueStatus);

    // 获取健康信息
    const ServiceHealth* speechHealth = serviceManager->getSpeechServiceHealth("volcano");
    const ServiceHealth* dialogueHealth = serviceManager->getDialogueServiceHealth("coze");

    TEST_ASSERT_NOT_NULL(speechHealth);
    TEST_ASSERT_NOT_NULL(dialogueHealth);

    // 测试不存在的服务的健康状态
    TEST_ASSERT_EQUAL(ServiceStatus::UNKNOWN, serviceManager->getSpeechServiceStatus("nonexistent"));
    TEST_ASSERT_EQUAL(ServiceStatus::UNKNOWN, serviceManager->getDialogueServiceStatus("nonexistent"));
    TEST_ASSERT_NULL(serviceManager->getSpeechServiceHealth("nonexistent"));
    TEST_ASSERT_NULL(serviceManager->getDialogueServiceHealth("nonexistent"));
}

// 测试5: 故障切换
void test_fallback_service(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());

    // 创建第二个服务作为备用
    VolcanoSpeechService* backupSpeech = new VolcanoSpeechService(mockNetwork, mockVolcanoConfig, mockLogger);
    CozeDialogueService* backupDialogue = new CozeDialogueService(mockNetwork, mockCozeConfig, mockLogger);

    // 设置主服务为不可用（通过模拟网络断开）
    MockNetworkManager* disconnectedNetwork = new MockNetworkManager(false);
    delete volcanoService;
    delete cozeService;
    volcanoService = new VolcanoSpeechService(disconnectedNetwork, mockVolcanoConfig, mockLogger);
    cozeService = new CozeDialogueService(disconnectedNetwork, mockCozeConfig, mockLogger);

    TEST_ASSERT_TRUE(volcanoService->initialize());
    TEST_ASSERT_TRUE(cozeService->initialize());
    TEST_ASSERT_TRUE(backupSpeech->initialize());
    TEST_ASSERT_TRUE(backupDialogue->initialize());

    // 注册服务
    TEST_ASSERT_TRUE(serviceManager->registerSpeechService(volcanoService));
    TEST_ASSERT_TRUE(serviceManager->registerSpeechService(backupSpeech));
    TEST_ASSERT_TRUE(serviceManager->registerDialogueService(cozeService));
    TEST_ASSERT_TRUE(serviceManager->registerDialogueService(backupDialogue));

    // 获取备用服务
    SpeechService* fallbackSpeech = serviceManager->getFallbackSpeechService();
    DialogueService* fallbackDialogue = serviceManager->getFallbackDialogueService();

    TEST_ASSERT_NOT_NULL(fallbackSpeech);
    TEST_ASSERT_NOT_NULL(fallbackDialogue);

    // 备用服务应该是可用的服务（backupSpeech和backupDialogue）
    TEST_ASSERT_EQUAL_STRING("volcano", fallbackSpeech->getName().c_str()); // 注意：两个都是"volcano"，但backupSpeech使用连接的网络
    TEST_ASSERT_EQUAL_STRING("coze", fallbackDialogue->getName().c_str());

    // 测试切换到备用服务
    TEST_ASSERT_TRUE(serviceManager->switchToFallbackSpeechService());
    TEST_ASSERT_TRUE(serviceManager->switchToFallbackDialogueService());

    // 现在默认服务应该已切换
    SpeechService* newDefaultSpeech = serviceManager->getDefaultSpeechService();
    DialogueService* newDefaultDialogue = serviceManager->getDefaultDialogueService();

    TEST_ASSERT_NOT_NULL(newDefaultSpeech);
    TEST_ASSERT_NOT_NULL(newDefaultDialogue);

    // 清理
    delete disconnectedNetwork;
    delete backupSpeech;
    delete backupDialogue;
}

// 测试6: 完整工作流程（语音识别 -> 对话 -> 语音合成）
void test_complete_workflow(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());
    TEST_ASSERT_TRUE(volcanoService->initialize());
    TEST_ASSERT_TRUE(cozeService->initialize());

    TEST_ASSERT_TRUE(serviceManager->registerSpeechService(volcanoService));
    TEST_ASSERT_TRUE(serviceManager->registerDialogueService(cozeService));

    // 步骤1: 语音识别
    uint8_t audioData[100];
    for (int i = 0; i < 100; i++) {
        audioData[i] = i % 256;
    }
    String recognizedText;
    TEST_ASSERT_TRUE(volcanoService->recognize(audioData, 100, recognizedText));
    TEST_ASSERT_FALSE(recognizedText.isEmpty());

    // 步骤2: 对话
    String dialogueResponse = cozeService->chat(recognizedText);
    TEST_ASSERT_FALSE(dialogueResponse.isEmpty());

    // 步骤3: 语音合成
    std::vector<uint8_t> synthesizedAudio;
    TEST_ASSERT_TRUE(volcanoService->synthesize(dialogueResponse, synthesizedAudio));
    TEST_ASSERT_FALSE(synthesizedAudio.empty());

    // 验证日志记录
    TEST_ASSERT_TRUE(mockLogger->contains("VolcanoSpeechService"));
    TEST_ASSERT_TRUE(mockLogger->contains("CozeDialogueService"));
}

// 测试7: 配置重新加载
void test_config_reload(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());

    // 创建新的配置管理器，使用不同的默认服务名称
    MockVolcanoConfigManager* newConfig = new MockVolcanoConfigManager();
    // 注意：由于MockVolcanoConfigManager返回的默认服务名称是"volcano"，保持不变
    serviceManager->setConfigManager(newConfig);

    // 重新加载配置
    TEST_ASSERT_TRUE(serviceManager->reloadConfig());

    // 注册服务
    TEST_ASSERT_TRUE(volcanoService->initialize());
    TEST_ASSERT_TRUE(cozeService->initialize());
    TEST_ASSERT_TRUE(serviceManager->registerSpeechService(volcanoService));
    TEST_ASSERT_TRUE(serviceManager->registerDialogueService(cozeService));

    // 默认服务应该仍然是"volcano"和"coze"
    SpeechService* defaultSpeech = serviceManager->getDefaultSpeechService();
    DialogueService* defaultDialogue = serviceManager->getDefaultDialogueService();

    TEST_ASSERT_NOT_NULL(defaultSpeech);
    TEST_ASSERT_NOT_NULL(defaultDialogue);
    TEST_ASSERT_EQUAL_STRING("volcano", defaultSpeech->getName().c_str());
    TEST_ASSERT_EQUAL_STRING("coze", defaultDialogue->getName().c_str());

    delete newConfig;
}

// 测试8: 服务注销
void test_service_unregistration(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());
    TEST_ASSERT_TRUE(volcanoService->initialize());
    TEST_ASSERT_TRUE(cozeService->initialize());

    TEST_ASSERT_TRUE(serviceManager->registerSpeechService(volcanoService));
    TEST_ASSERT_TRUE(serviceManager->registerDialogueService(cozeService));

    TEST_ASSERT_EQUAL(1, serviceManager->getTotalSpeechServices());
    TEST_ASSERT_EQUAL(1, serviceManager->getTotalDialogueServices());

    // 注销服务
    TEST_ASSERT_TRUE(serviceManager->unregisterSpeechService("volcano"));
    TEST_ASSERT_TRUE(serviceManager->unregisterDialogueService("coze"));

    TEST_ASSERT_EQUAL(0, serviceManager->getTotalSpeechServices());
    TEST_ASSERT_EQUAL(0, serviceManager->getTotalDialogueServices());

    // 测试注销不存在的服务
    TEST_ASSERT_FALSE(serviceManager->unregisterSpeechService("nonexistent"));
    TEST_ASSERT_FALSE(serviceManager->unregisterDialogueService("nonexistent"));
}

void setup() {
    // 等待2秒让串口就绪
    delay(2000);

    UNITY_BEGIN();

    // 运行测试
    RUN_TEST(test_service_manager_initialization);
    RUN_TEST(test_service_registration_and_discovery);
    RUN_TEST(test_default_service_selection);
    RUN_TEST(test_health_status);
    RUN_TEST(test_fallback_service);
    RUN_TEST(test_complete_workflow);
    RUN_TEST(test_config_reload);
    RUN_TEST(test_service_unregistration);

    UNITY_END();
}

void loop() {
    // 空循环
}