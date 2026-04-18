#include <Arduino.h>
#include <unity.h>
#include "../src/config/SPIFFSConfigManager.h"

SPIFFSConfigManager* configManager = nullptr;

void setUp(void) {
    // 在每个测试前运行
    configManager = new SPIFFSConfigManager("/test_config.json");
}

void tearDown(void) {
    // 在每个测试后运行
    delete configManager;
    configManager = nullptr;
}

void test_initialization(void) {
    TEST_ASSERT_NOT_NULL(configManager);
}

void test_default_config(void) {
    // 测试默认配置
    TEST_ASSERT_TRUE(configManager->resetToDefaults());

    // 验证一些默认值
    TEST_ASSERT_EQUAL_STRING("", configManager->getString("wifi.ssid").c_str());
    TEST_ASSERT_EQUAL_STRING("YourWiFiSSID", configManager->getString("wifi.ssid", "YourWiFiSSID").c_str());
    TEST_ASSERT_TRUE(configManager->getBool("wifi.autoConnect", true));
    TEST_ASSERT_EQUAL(10000, configManager->getInt("wifi.timeout", 10000));
}

void test_set_get_string(void) {
    // 测试设置和获取字符串
    TEST_ASSERT_TRUE(configManager->setString("wifi.ssid", "TestSSID"));
    TEST_ASSERT_EQUAL_STRING("TestSSID", configManager->getString("wifi.ssid").c_str());

    TEST_ASSERT_TRUE(configManager->setString("wifi.password", "TestPassword"));
    TEST_ASSERT_EQUAL_STRING("TestPassword", configManager->getString("wifi.password").c_str());
}

void test_set_get_int(void) {
    // 测试设置和获取整数
    TEST_ASSERT_TRUE(configManager->setInt("wifi.timeout", 15000));
    TEST_ASSERT_EQUAL(15000, configManager->getInt("wifi.timeout"));

    TEST_ASSERT_TRUE(configManager->setInt("audio.volume", 75));
    TEST_ASSERT_EQUAL(75, configManager->getInt("audio.volume"));
}

void test_set_get_bool(void) {
    // 测试设置和获取布尔值
    TEST_ASSERT_TRUE(configManager->setBool("wifi.autoConnect", false));
    TEST_ASSERT_FALSE(configManager->getBool("wifi.autoConnect"));

    TEST_ASSERT_TRUE(configManager->setBool("display.showWaveform", true));
    TEST_ASSERT_TRUE(configManager->getBool("display.showWaveform"));
}

void test_set_get_float(void) {
    // 测试设置和获取浮点数
    TEST_ASSERT_TRUE(configManager->setFloat("audio.vadSpeechThreshold", 0.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.5f, configManager->getFloat("audio.vadSpeechThreshold"));

    TEST_ASSERT_TRUE(configManager->setFloat("audio.vadSilenceThreshold", 0.3f));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.3f, configManager->getFloat("audio.vadSilenceThreshold"));

    TEST_ASSERT_TRUE(configManager->setFloat("audio.wakeWordSensitivity", 0.7f));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.7f, configManager->getFloat("audio.wakeWordSensitivity"));
}

void test_validation(void) {
    // 测试配置验证
    TEST_ASSERT_TRUE(configManager->resetToDefaults());

    // 默认配置应该通过验证
    TEST_ASSERT_TRUE(configManager->validate());

    // 设置无效值并验证失败
    TEST_ASSERT_TRUE(configManager->setInt("audio.sampleRate", 5000)); // 低于最小值
    TEST_ASSERT_FALSE(configManager->validate());

    auto errors = configManager->getValidationErrors();
    TEST_ASSERT_TRUE(errors.size() > 0);

    // 恢复有效值
    TEST_ASSERT_TRUE(configManager->setInt("audio.sampleRate", 16000));
    TEST_ASSERT_TRUE(configManager->validate());
}

void test_save_load(void) {
    // 测试保存和加载配置
    TEST_ASSERT_TRUE(configManager->setString("wifi.ssid", "SavedSSID"));
    TEST_ASSERT_TRUE(configManager->setString("wifi.password", "SavedPassword"));
    TEST_ASSERT_TRUE(configManager->setInt("audio.volume", 85));

    // 保存配置
    TEST_ASSERT_TRUE(configManager->save());

    // 创建新的配置管理器并加载
    SPIFFSConfigManager* newManager = new SPIFFSConfigManager("/test_config.json");
    TEST_ASSERT_TRUE(newManager->load());

    // 验证加载的值
    TEST_ASSERT_EQUAL_STRING("SavedSSID", newManager->getString("wifi.ssid").c_str());
    TEST_ASSERT_EQUAL_STRING("SavedPassword", newManager->getString("wifi.password").c_str());
    TEST_ASSERT_EQUAL(85, newManager->getInt("audio.volume"));

    delete newManager;
}

void test_string_array(void) {
    // 测试字符串数组
    std::vector<String> speechServices = {"volcano", "baidu", "tencent"};
    TEST_ASSERT_TRUE(configManager->setStringArray("services.speech.available", speechServices));

    auto loadedServices = configManager->getStringArray("services.speech.available");
    TEST_ASSERT_EQUAL(3, loadedServices.size());
    if (loadedServices.size() >= 3) {
        TEST_ASSERT_EQUAL_STRING("volcano", loadedServices[0].c_str());
        TEST_ASSERT_EQUAL_STRING("baidu", loadedServices[1].c_str());
        TEST_ASSERT_EQUAL_STRING("tencent", loadedServices[2].c_str());
    }
}

void setup() {
    // 等待2秒让串口就绪
    delay(2000);

    UNITY_BEGIN();

    RUN_TEST(test_initialization);
    RUN_TEST(test_default_config);
    RUN_TEST(test_set_get_string);
    RUN_TEST(test_set_get_int);
    RUN_TEST(test_set_get_bool);
    RUN_TEST(test_set_get_float);
    RUN_TEST(test_validation);
    RUN_TEST(test_save_load);
    RUN_TEST(test_string_array);

    UNITY_END();
}

void loop() {
    // 空循环
}