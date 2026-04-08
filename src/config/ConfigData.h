#ifndef CONFIG_DATA_H
#define CONFIG_DATA_H

#include <Arduino.h>
#include <vector>

// 配置数据结构定义
struct WiFiConfig {
    String ssid;
    String password;
    bool autoConnect;
    uint32_t timeout;
    uint8_t maxRetries;

    WiFiConfig() : ssid(""), password(""), autoConnect(true), timeout(10000), maxRetries(5) {}
};

struct SpeechServiceConfig {
    String apiKey;
    String secretKey;
    String endpoint;
    String language;
    String region;          // 区域，如 "cn-north-1"
    String voice;          // 语音合成音色
    bool enablePunctuation; // 是否启用标点预测
    float timeout;         // 超时时间（秒）

    SpeechServiceConfig() :
        apiKey(""), secretKey(""),
        endpoint("https://openspeech.bytedance.com"),
        language("zh-CN"),
        region("cn-north-1"),
        voice("zh-CN_female_standard"),
        enablePunctuation(true),
        timeout(10.0f) {}
};

struct DialogueServiceConfig {
    String botId;
    String apiKey;
    String endpoint;
    String streamEndpoint;  // 流式API端点
    String personality;
    String model;          // 模型名称
    String projectId;      // 项目ID（用于流式API）
    String sessionId;      // 会话ID（用于流式API）
    float temperature;     // 温度参数
    int maxTokens;         // 最大token数
    float timeout;         // 超时时间（秒）

    DialogueServiceConfig() :
        botId(""), apiKey(""),
        endpoint("https://api.coze.cn"),
        streamEndpoint("https://kfdcyyzqgx.coze.site/stream_run"),
        personality("friendly_assistant"),
        model("coze-model"),
        projectId("7625602998236004386"),
        sessionId("l_tnvlo49EWy6p9YUl8oC"),
        temperature(0.7f),
        maxTokens(1000),
        timeout(15.0f) {}
};

struct ServicesConfig {
    String defaultSpeechService;
    String defaultDialogueService;
    std::vector<String> availableSpeechServices;
    std::vector<String> availableDialogueServices;
    SpeechServiceConfig volcanoSpeech;
    DialogueServiceConfig cozeDialogue;

    ServicesConfig() :
        defaultSpeechService("volcano"),
        defaultDialogueService("coze"),
        availableSpeechServices({"volcano", "baidu", "tencent"}),
        availableDialogueServices({"coze", "openai"}) {}
};

struct AudioConfig {
    uint32_t sampleRate;
    uint8_t bitsPerSample;
    uint8_t channels;
    float vadThreshold;
    String wakeWord;
    float wakeWordSensitivity;
    uint8_t volume;

    AudioConfig() :
        sampleRate(16000), bitsPerSample(16), channels(1),
        vadThreshold(0.3), wakeWord("小智小智"),
        wakeWordSensitivity(0.8), volume(80) {}
};

struct DisplayConfig {
    // 硬件配置
    uint16_t width;           // 屏幕宽度
    uint16_t height;          // 屏幕高度
    uint8_t rotation;         // 旋转角度 (0-3)
    uint8_t brightness;       // 亮度 (0-100)
    uint16_t backgroundColor; // 背景颜色
    uint16_t textColor;       // 文本颜色
    uint8_t textSize;         // 文本大小
    bool backlightEnabled;    // 背光使能

    // 应用配置
    uint32_t timeout;         // 屏幕超时时间
    bool showWaveform;        // 显示波形
    bool showHistory;         // 显示历史记录
    uint8_t maxHistoryLines;  // 最大历史行数

    DisplayConfig() :
        width(242), height(240), rotation(1),
        brightness(100), backgroundColor(0x0000), // TFT_BLACK
        textColor(0xFFFF), // TFT_WHITE
        textSize(1), backlightEnabled(true), // 减小字体大小以避免显示不全
        timeout(30000), showWaveform(true), showHistory(true),
        maxHistoryLines(10) {}
};

struct LoggingConfig {
    String level;
    std::vector<String> output;

    LoggingConfig() : level("INFO"), output({"serial"}) {}
};

struct SystemConfig {
    // 配置版本
    uint32_t version;

    WiFiConfig wifi;
    ServicesConfig services;
    AudioConfig audio;
    DisplayConfig display;
    LoggingConfig logging;

    // 验证配置有效性
    bool validate() const;

    // 重置为默认值
    void resetToDefaults();
};

#endif // CONFIG_DATA_H