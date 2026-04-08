#include <Arduino.h>
#include <unity.h>
#include "../src/drivers/AudioDriver.h"
#include "../src/drivers/DisplayDriver.h"

AudioDriver* audioDriver = nullptr;
DisplayDriver* displayDriver = nullptr;

void setUp(void) {
    // 在每个测试前创建驱动实例
    // 注意：实际硬件初始化可能需要时间
}

void tearDown(void) {
    // 在每个测试后清理
    if (audioDriver) {
        delete audioDriver;
        audioDriver = nullptr;
    }
    if (displayDriver) {
        delete displayDriver;
        displayDriver = nullptr;
    }
}

void test_audio_driver_creation(void) {
    audioDriver = new AudioDriver();
    TEST_ASSERT_NOT_NULL(audioDriver);
    TEST_ASSERT_FALSE(audioDriver->isReady());
}

void test_display_driver_creation(void) {
    displayDriver = new DisplayDriver();
    TEST_ASSERT_NOT_NULL(displayDriver);
    TEST_ASSERT_FALSE(displayDriver->isReady());
}

void test_audio_config_defaults(void) {
    audioDriver = new AudioDriver();

    AudioDriverConfig config = audioDriver->getConfig();
    TEST_ASSERT_EQUAL(16000, config.sampleRate);
    TEST_ASSERT_EQUAL(I2S_BITS_PER_SAMPLE_16BIT, config.bitsPerSample);
    TEST_ASSERT_EQUAL(4096, config.bufferSize);
    TEST_ASSERT_EQUAL(80, config.volume);
}

void test_display_config_defaults(void) {
    displayDriver = new DisplayDriver();

    DisplayConfig config = displayDriver->getConfig();
    TEST_ASSERT_EQUAL(240, config.width);
    TEST_ASSERT_EQUAL(320, config.height);
    TEST_ASSERT_EQUAL(1, config.rotation); // 纵向模式
    TEST_ASSERT_EQUAL(100, config.brightness);
    TEST_ASSERT_EQUAL(TFT_BLACK, config.backgroundColor);
    TEST_ASSERT_EQUAL(TFT_WHITE, config.textColor);
    TEST_ASSERT_EQUAL(2, config.textSize);
}

void test_audio_volume_control(void) {
    audioDriver = new AudioDriver();

    TEST_ASSERT_TRUE(audioDriver->setVolume(50));
    TEST_ASSERT_EQUAL(50, audioDriver->getVolume());

    TEST_ASSERT_TRUE(audioDriver->setVolume(150)); // 超过100
    TEST_ASSERT_EQUAL(100, audioDriver->getVolume()); // 应该限制为100

    TEST_ASSERT_TRUE(audioDriver->setVolume(0));
    TEST_ASSERT_EQUAL(0, audioDriver->getVolume());
}

void test_display_brightness_control(void) {
    displayDriver = new DisplayDriver();

    displayDriver->setBrightness(50);
    TEST_ASSERT_EQUAL(50, displayDriver->getBrightness());

    displayDriver->setBrightness(150); // 超过100
    TEST_ASSERT_EQUAL(100, displayDriver->getBrightness()); // 应该限制为100
}

void test_audio_buffer_management(void) {
    audioDriver = new AudioDriver();

    // 初始化后缓冲区应为空
    TEST_ASSERT_EQUAL(0, audioDriver->getAvailableData());

    // 测试写入数据
    uint8_t testData[100] = {0};
    size_t written = audioDriver->writeAudioData(testData, sizeof(testData));
    // 注意：未初始化的驱动不会实际写入缓冲区
    // 这里只测试函数调用不崩溃
    TEST_ASSERT_TRUE(true);
}

void test_display_text_functions(void) {
    displayDriver = new DisplayDriver();

    // 测试文本颜色设置
    displayDriver->setTextColor(TFT_RED, TFT_BLACK);
    // 如果函数没有崩溃，测试通过
    TEST_ASSERT_TRUE(true);

    // 测试文本大小设置
    displayDriver->setTextSize(3);
    TEST_ASSERT_TRUE(true);
}

void test_config_update(void) {
    audioDriver = new AudioDriver();
    displayDriver = new DisplayDriver();

    // 测试音频配置更新
    AudioDriverConfig audioConfig;
    audioConfig.sampleRate = 48000;
    audioConfig.volume = 60;

    // 注意：未初始化的驱动也可以更新配置
    TEST_ASSERT_TRUE(audioDriver->updateConfig(audioConfig));

    // 测试显示配置更新
    DisplayConfig displayConfig;
    displayConfig.width = 320;
    displayConfig.height = 240;
    displayConfig.rotation = 2;

    TEST_ASSERT_TRUE(displayDriver->updateConfig(displayConfig));

    // 验证配置已更新
    AudioDriverConfig updatedAudioConfig = audioDriver->getConfig();
    TEST_ASSERT_EQUAL(48000, updatedAudioConfig.sampleRate);
    TEST_ASSERT_EQUAL(60, updatedAudioConfig.volume);

    DisplayConfig updatedDisplayConfig = displayDriver->getConfig();
    TEST_ASSERT_EQUAL(320, updatedDisplayConfig.width);
    TEST_ASSERT_EQUAL(240, updatedDisplayConfig.height);
    TEST_ASSERT_EQUAL(2, updatedDisplayConfig.rotation);
}

void setup() {
    // 等待2秒让串口就绪
    delay(2000);

    UNITY_BEGIN();

    // 运行测试
    RUN_TEST(test_audio_driver_creation);
    RUN_TEST(test_display_driver_creation);
    RUN_TEST(test_audio_config_defaults);
    RUN_TEST(test_display_config_defaults);
    RUN_TEST(test_audio_volume_control);
    RUN_TEST(test_display_brightness_control);
    RUN_TEST(test_audio_buffer_management);
    RUN_TEST(test_display_text_functions);
    RUN_TEST(test_config_update);

    UNITY_END();
}

void loop() {
    // 空循环
}