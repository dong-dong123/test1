#include <Arduino.h>
#include <unity.h>
#include "../src/modules/SystemLogger.h"
#include "../src/config/SPIFFSConfigManager.h"

// 测试配置管理器模拟
class MockConfigManager : public ConfigManager {
private:
    String level;
    std::vector<String> outputs;

public:
    MockConfigManager(const String& lvl = "INFO", const std::vector<String>& outs = {"serial"})
        : level(lvl), outputs(outs) {}

    // 仅实现测试需要的方法
    virtual String getString(const String& key, const String& defaultValue = "") override {
        if (key == "logging.level") {
            return level.isEmpty() ? defaultValue : level;
        }
        return defaultValue;
    }

    virtual std::vector<String> getStringArray(const String& key) override {
        if (key == "logging.output") {
            return outputs;
        }
        return std::vector<String>();
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
    virtual bool setStringArray(const String& key, const std::vector<String>& values) override { return true; }
    virtual bool validate() const override { return true; }
    virtual std::vector<String> getValidationErrors() const override { return std::vector<String>(); }
};

SystemLogger* logger = nullptr;
MockConfigManager* mockConfig = nullptr;

void setUp(void) {
    // 在每个测试前创建新的logger实例
    mockConfig = new MockConfigManager();
    logger = new SystemLogger(mockConfig);
}

void tearDown(void) {
    // 在每个测试后清理
    delete logger;
    logger = nullptr;
    delete mockConfig;
    mockConfig = nullptr;
}

void test_logger_initialization(void) {
    TEST_ASSERT_NOT_NULL(logger);
    TEST_ASSERT_EQUAL(Logger::Level::INFO, logger->getLevel());
}

void test_level_conversion(void) {
    TEST_ASSERT_EQUAL(Logger::Level::DEBUG, SystemLogger::stringToLevel("DEBUG"));
    TEST_ASSERT_EQUAL(Logger::Level::INFO, SystemLogger::stringToLevel("INFO"));
    TEST_ASSERT_EQUAL(Logger::Level::WARN, SystemLogger::stringToLevel("WARN"));
    TEST_ASSERT_EQUAL(Logger::Level::WARN, SystemLogger::stringToLevel("warning"));
    TEST_ASSERT_EQUAL(Logger::Level::ERROR, SystemLogger::stringToLevel("ERROR"));
    TEST_ASSERT_EQUAL(Logger::Level::FATAL, SystemLogger::stringToLevel("FATAL"));
    TEST_ASSERT_EQUAL(Logger::Level::INFO, SystemLogger::stringToLevel("UNKNOWN")); // 默认INFO

    TEST_ASSERT_EQUAL_STRING("DEBUG", SystemLogger::levelToStringStatic(Logger::Level::DEBUG).c_str());
    TEST_ASSERT_EQUAL_STRING("INFO", SystemLogger::levelToStringStatic(Logger::Level::INFO).c_str());
    TEST_ASSERT_EQUAL_STRING("WARN", SystemLogger::levelToStringStatic(Logger::Level::WARN).c_str());
    TEST_ASSERT_EQUAL_STRING("ERROR", SystemLogger::levelToStringStatic(Logger::Level::ERROR).c_str());
    TEST_ASSERT_EQUAL_STRING("FATAL", SystemLogger::levelToStringStatic(Logger::Level::FATAL).c_str());
}

void test_level_filtering(void) {
    // 默认级别为INFO，DEBUG级别的日志应该被过滤
    logger->setLevel(Logger::Level::INFO);

    // 由于我们无法直接捕获串口输出，这里只测试级别过滤逻辑
    // 在实际硬件测试中，可以验证串口输出

    // 测试级别比较逻辑
    TEST_ASSERT_TRUE(static_cast<int>(Logger::Level::INFO) >= static_cast<int>(Logger::Level::INFO)); // INFO >= INFO
    TEST_ASSERT_TRUE(static_cast<int>(Logger::Level::WARN) >= static_cast<int>(Logger::Level::INFO)); // WARN >= INFO
    TEST_ASSERT_FALSE(static_cast<int>(Logger::Level::DEBUG) >= static_cast<int>(Logger::Level::INFO)); // DEBUG < INFO

    // 设置级别为DEBUG，所有日志都应通过
    logger->setLevel(Logger::Level::DEBUG);
    TEST_ASSERT_EQUAL(Logger::Level::DEBUG, logger->getLevel());
}

void test_log_with_tag(void) {
    // 测试带标签的日志记录
    // 由于无法验证输出，这里只确保函数不会崩溃
    logger->logWithTag(Logger::Level::INFO, "TEST", "Tagged message");
    logger->logWithTag(Logger::Level::WARN, "MODULE", "Another tagged message");

    // 如果程序运行到这里没有崩溃，测试通过
    TEST_ASSERT_TRUE(true);
}

void test_logf_formatting(void) {
    // 测试格式化日志
    logger->logf(Logger::Level::INFO, "Formatted: %s %d %.2f", "test", 123, 45.67f);
    // 没有崩溃即通过
    TEST_ASSERT_TRUE(true);
}

void test_config_integration(void) {
    // 测试从配置管理器读取设置
    std::vector<String> outputs = {"serial", "file"};
    MockConfigManager* customConfig = new MockConfigManager("WARN", outputs);
    SystemLogger* customLogger = new SystemLogger(customConfig);

    TEST_ASSERT_EQUAL(Logger::Level::WARN, customLogger->getLevel());
    // 注意：这里无法验证输出器是否创建成功，因为file输出器需要SPIFFS

    delete customLogger;
    delete customConfig;
}

void test_sink_management(void) {
    // 测试输出器管理
    TEST_ASSERT_EQUAL(1, logger->getActiveOutputs().size()); // 默认有一个串口输出器

    // 尝试添加第二个串口输出器（应该失败，因为类型重复）
    SerialLogSink* serialSink = new SerialLogSink();
    TEST_ASSERT_FALSE(logger->addSink(serialSink)); // 应该被拒绝并删除

    // 添加文件输出器
    FileLogSink* fileSink = new FileLogSink("/test.log", 1024);
    TEST_ASSERT_TRUE(logger->addSink(fileSink));
    TEST_ASSERT_EQUAL(2, logger->getActiveOutputs().size());

    // 移除文件输出器
    TEST_ASSERT_TRUE(logger->removeSink(LogOutputType::FILE));
    TEST_ASSERT_EQUAL(1, logger->getActiveOutputs().size());

    // 清空所有输出器
    logger->clearSinks();
    TEST_ASSERT_EQUAL(0, logger->getActiveOutputs().size());

    // 重新添加串口输出器以便后续测试
    logger->addSink(new SerialLogSink());
}

void test_buffer_management(void) {
    // 测试缓冲区使用情况
    size_t usage = logger->getBufferUsage();
    // 初始缓冲区应为空
    TEST_ASSERT_EQUAL(0, usage);

    // 刷新缓冲区（应该无操作）
    logger->flush();
    TEST_ASSERT_TRUE(true);
}

void test_configure_from_manager(void) {
    // 测试从配置管理器重新配置
    std::vector<String> outputs = {"serial"};
    MockConfigManager* config = new MockConfigManager("ERROR", outputs);
    logger->setConfigManager(config);

    // 配置应从管理器更新
    logger->configureFromManager();
    TEST_ASSERT_EQUAL(Logger::Level::ERROR, logger->getLevel());

    delete config;
}

void setup() {
    // 等待2秒让串口就绪
    delay(2000);

    UNITY_BEGIN();

    RUN_TEST(test_logger_initialization);
    RUN_TEST(test_level_conversion);
    RUN_TEST(test_level_filtering);
    RUN_TEST(test_log_with_tag);
    RUN_TEST(test_logf_formatting);
    RUN_TEST(test_config_integration);
    RUN_TEST(test_sink_management);
    RUN_TEST(test_buffer_management);
    RUN_TEST(test_configure_from_manager);

    UNITY_END();
}

void loop() {
    // 空循环
}