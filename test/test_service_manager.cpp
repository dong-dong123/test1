#include <Arduino.h>
#include <unity.h>
#include "../src/modules/ServiceManager.h"

// Mock服务实现
class MockSpeechService : public SpeechService {
private:
    String name;
    bool available;
    float cost;

public:
    MockSpeechService(const String& serviceName, bool isAvailable = true, float serviceCost = 0.01f)
        : name(serviceName), available(isAvailable), cost(serviceCost) {}

    // SpeechService接口实现
    virtual bool recognize(const uint8_t* audio_data, size_t length, String& text) override {
        text = "Mock recognition result from " + name;
        return available;
    }

    virtual bool recognizeStreamStart() override { return available; }
    virtual bool recognizeStreamChunk(const uint8_t* audio_chunk, size_t chunk_size, String& partial_text) override {
        partial_text = "Partial from " + name;
        return available;
    }
    virtual bool recognizeStreamEnd(String& final_text) override {
        final_text = "Final from " + name;
        return available;
    }

    virtual bool synthesize(const String& text, std::vector<uint8_t>& audio_data) override {
        audio_data.resize(100, 0); // 返回100字节的模拟音频数据
        return available;
    }

    virtual bool synthesizeStreamStart(const String& text) override { return available; }
    virtual bool synthesizeStreamGetChunk(std::vector<uint8_t>& chunk, bool& is_last) override {
        chunk.resize(50, 0);
        is_last = true;
        return available;
    }

    virtual String getName() const override { return name; }
    virtual bool isAvailable() const override { return available; }
    virtual float getCostPerRequest() const override { return cost; }

    void setAvailable(bool isAvailable) { available = isAvailable; }
};

class MockDialogueService : public DialogueService {
private:
    String name;
    bool available;
    float cost;
    std::vector<String> context;

public:
    MockDialogueService(const String& serviceName, bool isAvailable = true, float serviceCost = 0.02f)
        : name(serviceName), available(isAvailable), cost(serviceCost) {}

    // DialogueService接口实现
    virtual String chat(const String& input) override {
        return "Mock response from " + name + " to: " + input;
    }

    virtual String chatWithContext(const String& input, const std::vector<String>& ctx) override {
        context = ctx;
        return "Contextual response from " + name + " to: " + input;
    }

    virtual bool chatStreamStart(const String& input) override { return available; }
    virtual bool chatStreamGetChunk(String& chunk, bool& is_last) override {
        chunk = "Stream chunk from " + name;
        is_last = true;
        return available;
    }

    virtual String getName() const override { return name; }
    virtual bool isAvailable() const override { return available; }
    virtual float getCostPerRequest() const override { return cost; }

    virtual void clearContext() override { context.clear(); }
    virtual size_t getContextSize() const override { return context.size(); }

    void setAvailable(bool isAvailable) { available = isAvailable; }
};

// Mock配置管理器
class MockConfigManager : public ConfigManager {
private:
    String defaultSpeechService;
    String defaultDialogueService;

public:
    MockConfigManager(const String& speechDefault = "volcano", const String& dialogueDefault = "coze")
        : defaultSpeechService(speechDefault), defaultDialogueService(dialogueDefault) {}

    virtual String getString(const String& key, const String& defaultValue = "") override {
        if (key == "services.defaultSpeechService") {
            return defaultSpeechService;
        } else if (key == "services.defaultDialogueService") {
            return defaultDialogueService;
        }
        return defaultValue;
    }

    // 以下方法为简单实现，测试中不使用
    virtual bool load() override { return true; }
    virtual bool save() override { return true; }
    virtual bool resetToDefaults() override { return true; }
    virtual bool setString(const String& key, const String& value) override { return true; }
    virtual int getInt(const String& key, int defaultValue = 0) override { return defaultValue; }
    virtual bool setInt(const String& key, int value) override { return true; }
    virtual float getFloat(const String& key, float defaultValue = 0.0f) override { return defaultValue; }
    virtual bool setFloat(const String& key, float value) override { return true; }
    virtual bool getBool(const String& key, bool defaultValue = false) override { return defaultValue; }
    virtual bool setBool(const String& key, bool value) override { return true; }
    virtual std::vector<String> getStringArray(const String& key) override { return std::vector<String>(); }
    virtual bool setStringArray(const String& key, const std::vector<String>& values) override { return true; }
    virtual bool validate() const override { return true; }
    virtual std::vector<String> getValidationErrors() const override { return std::vector<String>(); }
};

// 测试变量
ServiceManager* serviceManager = nullptr;
MockConfigManager* mockConfig = nullptr;

void setUp(void) {
    // 在每个测试前创建新的ServiceManager实例
    mockConfig = new MockConfigManager("volcano", "coze");
    serviceManager = new ServiceManager(mockConfig);
}

void tearDown(void) {
    // 在每个测试后清理
    delete serviceManager;
    serviceManager = nullptr;
    delete mockConfig;
    mockConfig = nullptr;
}

void test_initialization(void) {
    TEST_ASSERT_NOT_NULL(serviceManager);
    TEST_ASSERT_FALSE(serviceManager->isReady());

    TEST_ASSERT_TRUE(serviceManager->initialize());
    TEST_ASSERT_TRUE(serviceManager->isReady());
}

void test_service_registration(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());

    // 创建模拟服务
    MockSpeechService* speechService1 = new MockSpeechService("volcano");
    MockSpeechService* speechService2 = new MockSpeechService("baidu");
    MockDialogueService* dialogueService1 = new MockDialogueService("coze");
    MockDialogueService* dialogueService2 = new MockDialogueService("openai");

    // 注册服务
    TEST_ASSERT_TRUE(serviceManager->registerSpeechService(speechService1));
    TEST_ASSERT_TRUE(serviceManager->registerSpeechService(speechService2));
    TEST_ASSERT_TRUE(serviceManager->registerDialogueService(dialogueService1));
    TEST_ASSERT_TRUE(serviceManager->registerDialogueService(dialogueService2));

    // 测试重复注册
    TEST_ASSERT_FALSE(serviceManager->registerSpeechService(speechService1));

    // 测试获取服务数量
    TEST_ASSERT_EQUAL(2, serviceManager->getTotalSpeechServices());
    TEST_ASSERT_EQUAL(2, serviceManager->getTotalDialogueServices());

    // 注意：ServiceManager不会删除服务对象，由测试清理
    delete speechService1;
    delete speechService2;
    delete dialogueService1;
    delete dialogueService2;
}

void test_service_discovery(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());

    // 注册服务
    MockSpeechService* speechService = new MockSpeechService("volcano");
    MockDialogueService* dialogueService = new MockDialogueService("coze");

    serviceManager->registerSpeechService(speechService);
    serviceManager->registerDialogueService(dialogueService);

    // 测试服务发现
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

    // 测试服务可用性检查
    TEST_ASSERT_TRUE(serviceManager->isSpeechServiceAvailable("volcano"));
    TEST_ASSERT_TRUE(serviceManager->isDialogueServiceAvailable("coze"));
    TEST_ASSERT_FALSE(serviceManager->isSpeechServiceAvailable("nonexistent"));
    TEST_ASSERT_FALSE(serviceManager->isDialogueServiceAvailable("nonexistent"));

    delete speechService;
    delete dialogueService;
}

void test_default_service_selection(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());

    // 注册多个服务
    MockSpeechService* volcanoService = new MockSpeechService("volcano");
    MockSpeechService* baiduService = new MockSpeechService("baidu");
    MockDialogueService* cozeService = new MockDialogueService("coze");
    MockDialogueService* openaiService = new MockDialogueService("openai");

    serviceManager->registerSpeechService(volcanoService);
    serviceManager->registerSpeechService(baiduService);
    serviceManager->registerDialogueService(cozeService);
    serviceManager->registerDialogueService(openaiService);

    // 获取默认服务（应该返回配置中指定的默认服务）
    SpeechService* defaultSpeech = serviceManager->getDefaultSpeechService();
    DialogueService* defaultDialogue = serviceManager->getDefaultDialogueService();

    TEST_ASSERT_NOT_NULL(defaultSpeech);
    TEST_ASSERT_NOT_NULL(defaultDialogue);

    TEST_ASSERT_EQUAL_STRING("volcano", defaultSpeech->getName().c_str());
    TEST_ASSERT_EQUAL_STRING("coze", defaultDialogue->getName().c_str());

    // 测试按名称获取服务
    SpeechService* speechByName = serviceManager->getSpeechService("baidu");
    DialogueService* dialogueByName = serviceManager->getDialogueService("openai");

    TEST_ASSERT_NOT_NULL(speechByName);
    TEST_ASSERT_NOT_NULL(dialogueByName);

    TEST_ASSERT_EQUAL_STRING("baidu", speechByName->getName().c_str());
    TEST_ASSERT_EQUAL_STRING("openai", dialogueByName->getName().c_str());

    // 测试不存在的服务
    TEST_ASSERT_NULL(serviceManager->getSpeechService("nonexistent"));
    TEST_ASSERT_NULL(serviceManager->getDialogueService("nonexistent"));

    delete volcanoService;
    delete baiduService;
    delete cozeService;
    delete openaiService;
}

void test_health_status(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());

    // 注册服务
    MockSpeechService* speechService = new MockSpeechService("volcano");
    MockDialogueService* dialogueService = new MockDialogueService("coze");

    serviceManager->registerSpeechService(speechService);
    serviceManager->registerDialogueService(dialogueService);

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

    delete speechService;
    delete dialogueService;
}

void test_fallback_service(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());

    // 注册多个服务，但默认服务设置为不可用
    MockSpeechService* volcanoService = new MockSpeechService("volcano", false); // 不可用
    MockSpeechService* baiduService = new MockSpeechService("baidu", true);      // 可用
    MockDialogueService* cozeService = new MockDialogueService("coze", false);   // 不可用
    MockDialogueService* openaiService = new MockDialogueService("openai", true); // 可用

    serviceManager->registerSpeechService(volcanoService);
    serviceManager->registerSpeechService(baiduService);
    serviceManager->registerDialogueService(cozeService);
    serviceManager->registerDialogueService(openaiService);

    // 获取备用服务
    SpeechService* fallbackSpeech = serviceManager->getFallbackSpeechService();
    DialogueService* fallbackDialogue = serviceManager->getFallbackDialogueService();

    TEST_ASSERT_NOT_NULL(fallbackSpeech);
    TEST_ASSERT_NOT_NULL(fallbackDialogue);

    // 备用服务应该是可用的服务
    TEST_ASSERT_EQUAL_STRING("baidu", fallbackSpeech->getName().c_str());
    TEST_ASSERT_EQUAL_STRING("openai", fallbackDialogue->getName().c_str());

    // 测试切换到备用服务
    TEST_ASSERT_TRUE(serviceManager->switchToFallbackSpeechService());
    TEST_ASSERT_TRUE(serviceManager->switchToFallbackDialogueService());

    // 现在默认服务应该已切换
    SpeechService* newDefaultSpeech = serviceManager->getDefaultSpeechService();
    DialogueService* newDefaultDialogue = serviceManager->getDefaultDialogueService();

    TEST_ASSERT_EQUAL_STRING("baidu", newDefaultSpeech->getName().c_str());
    TEST_ASSERT_EQUAL_STRING("openai", newDefaultDialogue->getName().c_str());

    delete volcanoService;
    delete baiduService;
    delete cozeService;
    delete openaiService;
}

void test_config_reload(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());

    // 创建新的配置管理器，使用不同的默认服务
    MockConfigManager* newConfig = new MockConfigManager("baidu", "openai");
    serviceManager->setConfigManager(newConfig);

    // 重新加载配置
    TEST_ASSERT_TRUE(serviceManager->reloadConfig());

    // 注册服务
    MockSpeechService* volcanoService = new MockSpeechService("volcano");
    MockSpeechService* baiduService = new MockSpeechService("baidu");
    MockDialogueService* cozeService = new MockDialogueService("coze");
    MockDialogueService* openaiService = new MockDialogueService("openai");

    serviceManager->registerSpeechService(volcanoService);
    serviceManager->registerSpeechService(baiduService);
    serviceManager->registerDialogueService(cozeService);
    serviceManager->registerDialogueService(openaiService);

    // 现在默认服务应该是新配置中指定的
    SpeechService* defaultSpeech = serviceManager->getDefaultSpeechService();
    DialogueService* defaultDialogue = serviceManager->getDefaultDialogueService();

    TEST_ASSERT_EQUAL_STRING("baidu", defaultSpeech->getName().c_str());
    TEST_ASSERT_EQUAL_STRING("openai", defaultDialogue->getName().c_str());

    delete volcanoService;
    delete baiduService;
    delete cozeService;
    delete openaiService;
    delete newConfig;
}

void test_service_unregistration(void) {
    TEST_ASSERT_TRUE(serviceManager->initialize());

    // 注册服务
    MockSpeechService* speechService = new MockSpeechService("volcano");
    MockDialogueService* dialogueService = new MockDialogueService("coze");

    TEST_ASSERT_TRUE(serviceManager->registerSpeechService(speechService));
    TEST_ASSERT_TRUE(serviceManager->registerDialogueService(dialogueService));

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

    delete speechService;
    delete dialogueService;
}

void setup() {
    // 等待2秒让串口就绪
    delay(2000);

    UNITY_BEGIN();

    // 运行测试
    RUN_TEST(test_initialization);
    RUN_TEST(test_service_registration);
    RUN_TEST(test_service_discovery);
    RUN_TEST(test_default_service_selection);
    RUN_TEST(test_health_status);
    RUN_TEST(test_fallback_service);
    RUN_TEST(test_config_reload);
    RUN_TEST(test_service_unregistration);

    UNITY_END();
}

void loop() {
    // 空循环
}