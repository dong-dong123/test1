#ifndef COZE_DIALOGUE_SERVICE_H
#define COZE_DIALOGUE_SERVICE_H

#include <Arduino.h>
#include <vector>
#include "../interfaces/DialogueService.h"
#include "../modules/NetworkManager.h"
#include "../interfaces/ConfigManager.h"
#include "../interfaces/Logger.h"
#include <ArduinoJson.h>

// Coze对话服务配置
struct CozeDialogueConfig {
    String apiKey;
    String botId;
    String userId;
    String endpoint;        // API端点
    String streamEndpoint;  // 流式API端点
    String model;          // 模型名称
    float temperature;     // 温度参数
    int maxTokens;         // 最大token数
    float timeout;         // 超时时间（秒）
    String projectId;      // 项目ID（用于流式API）
    String sessionId;      // 会话ID（用于流式API）

    CozeDialogueConfig() :
        endpoint("https://api.coze.cn/v1/chat"),
        streamEndpoint("https://kfdcyyzqgx.coze.site/stream_run"),
        model("coze-model"),
        temperature(0.95f),
        maxTokens(8192),
        timeout(15.0f),
        projectId("7625602998236004386") {} // 默认项目ID
};

// Coze对话服务实现
class CozeDialogueService : public DialogueService {
private:
    // 配置和依赖
    CozeDialogueConfig config;
    NetworkManager* networkManager;
    ConfigManager* configManager;
    Logger* logger;

    // 状态
    bool isInitialized;
    bool isAvailableStatus;
    uint32_t lastAvailabilityCheck;
    String lastError;

    // 对话上下文
    std::vector<String> contextHistory;
    size_t maxContextSize;

    // 流式会话状态
    bool isStreaming;
    String currentStreamSessionId;
    String streamEndpoint;           // 流式端点URL
    String accumulatedAnswer;        // 累积的答案文本
    std::vector<uint8_t> streamBuffer; // 流式数据缓冲区
    uint32_t streamStartTime;        // 流式开始时间
    uint32_t lastStreamDataTime;     // 最后收到数据时间

    // 内部方法
    bool loadConfig();
    String generateAuthHeader() const;
    bool updateAvailability();
    void logEvent(const String& event, const String& details = "") const;
    void trimContext();

public:
    CozeDialogueService(NetworkManager* netMgr = nullptr,
                       ConfigManager* configMgr = nullptr,
                       Logger* log = nullptr);
    virtual ~CozeDialogueService();

    // 初始化/反初始化
    bool initialize();
    bool deinitialize();
    bool isReady() const { return isInitialized; }

    // DialogueService接口实现
    virtual String chat(const String& input) override;
    virtual String chatWithContext(const String& input, const std::vector<String>& ctx) override;
    virtual bool chatStreamStart(const String& input) override;
    virtual bool chatStreamGetChunk(String& chunk, bool& is_last) override;
    virtual String getName() const override { return "coze"; }
    virtual bool isAvailable() const override;
    virtual float getCostPerRequest() const override;
    virtual void clearContext() override;
    virtual size_t getContextSize() const override;

    // 配置管理
    void setNetworkManager(NetworkManager* netMgr);
    void setConfigManager(ConfigManager* configMgr);
    void setLogger(Logger* log);
    bool updateConfig(const CozeDialogueConfig& newConfig);
    const CozeDialogueConfig& getConfig() const { return config; }

    // 状态信息
    String getLastError() const { return lastError; }
    void clearError() { lastError = ""; }

    // 上下文管理
    void addToContext(const String& message);
    const std::vector<String>& getContext() const { return contextHistory; }
    void setMaxContextSize(size_t size) { maxContextSize = size; trimContext(); }
    size_t getMaxContextSize() const { return maxContextSize; }

    // 工具方法
    static String getDefaultConfigJSON();
    static CozeDialogueConfig createDefaultConfig();

private:
    // 实际API调用
    bool callChatAPI(const String& input, const std::vector<String>& context, String& response);
    bool callStreamChatStart(const String& input, const std::vector<String>& context);
    bool callStreamChatGetChunk(String& chunk, bool& is_last);
    bool callStreamChatEnd();

    // 请求/响应处理
    String buildChatRequestJSON(const String& input, const std::vector<String>& context) const;
    bool parseChatResponse(const String& jsonResponse, String& output) const;
    bool parseStreamChunk(const String& chunkData, String& text, bool& isLast) const;

    // 流式API处理
    String buildStreamRequestJSON(const String& input, const std::vector<String>& context) const;
    bool processStreamEvent(const String& eventJson);
    bool handleStreamMessageStart(const JsonObject& data);
    bool handleStreamMessageEnd(const JsonObject& data);
    bool handleStreamAnswer(const JsonObject& data);
    bool handleStreamToolRequest(const JsonObject& data);
    bool handleStreamToolResponse(const JsonObject& data);
    bool handleStreamError(const JsonObject& data);
};

#endif // COZE_DIALOGUE_SERVICE_H