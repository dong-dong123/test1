#ifndef VOLCANO_SPEECH_SERVICE_H
#define VOLCANO_SPEECH_SERVICE_H

#include <Arduino.h>
#include <vector>
#include "../interfaces/SpeechService.h"
#include "../modules/NetworkManager.h"
#include "../interfaces/ConfigManager.h"
#include "../interfaces/Logger.h"
#include "WebSocketClient.h"

// 火山引擎语音服务配置
struct VolcanoSpeechConfig {
    String apiKey;
    String secretKey;
    String endpoint;        // API端点，如 "https://openspeech.bytedance.com"
    String region;          // 区域，如 "cn-north-1"
    String language;        // 语言，如 "zh-CN"
    String voice;          // 语音合成音色
    bool enablePunctuation; // 是否启用标点预测
    float timeout;         // 超时时间（秒）

    VolcanoSpeechConfig() :
        endpoint("https://openspeech.bytedance.com"),
        region("cn-north-1"),
        language("zh-CN"),
        voice("zh-CN_female_standard"),
        enablePunctuation(true),
        timeout(10.0f) {}
};

// 火山引擎语音服务实现
class VolcanoSpeechService : public SpeechService {
private:
    // 配置和依赖
    VolcanoSpeechConfig config;
    NetworkManager* networkManager;
    ConfigManager* configManager;
    Logger* logger;

    // 状态
    bool isInitialized;
    bool isAvailableStatus;
    uint32_t lastAvailabilityCheck;
    String lastError;

    // WebSocket流式状态
    WebSocketClient* webSocketClient;
    bool isStreamingRecognition;
    bool isStreamingSynthesis;
    String streamRecognitionId;
    String streamSynthesisId;
    std::vector<uint8_t> pendingAudioChunks;
    String partialRecognitionText;
    std::vector<uint8_t> pendingSynthesisAudio;
    uint32_t streamStartTime;

    // API端点
    static const char* RECOGNITION_API;
    static const char* SYNTHESIS_API;
    static const char* STREAM_RECOGNITION_API;
    static const char* NOSTREAM_RECOGNITION_API;
    static const char* STREAM_SYNTHESIS_API;

    // 内部方法
    bool loadConfig();
    String generateSignature(const String& params, uint64_t timestamp) const;
    String generateAuthHeader(const String& params) const;
    bool updateAvailability();
    void logEvent(const String& event, const String& details = "") const;

    // WebSocket流式处理方法
    void initializeWebSocket();
    void cleanupWebSocket();
    bool connectWebSocketStream(const String& streamType);
    bool sendWebSocketAuth();
    bool sendWebSocketRecognitionStart();
    bool sendWebSocketSynthesisStart(const String& text);
    void handleWebSocketEvent(WebSocketEvent event, const String& message, const uint8_t* data, size_t length);
    void parseWebSocketMessage(const String& jsonMessage);
    void handleRecognitionResult(const String& text, bool isFinal);
    void handleSynthesisAudio(const uint8_t* data, size_t length, bool isFinal);

public:
    VolcanoSpeechService(NetworkManager* netMgr = nullptr,
                        ConfigManager* configMgr = nullptr,
                        Logger* log = nullptr);
    virtual ~VolcanoSpeechService();

    // 初始化/反初始化
    bool initialize();
    bool deinitialize();
    bool isReady() const { return isInitialized; }

    // SpeechService接口实现
    virtual bool recognize(const uint8_t* audio_data, size_t length, String& text) override;
    virtual bool recognizeStreamStart() override;
    virtual bool recognizeStreamChunk(const uint8_t* audio_chunk, size_t chunk_size, String& partial_text) override;
    virtual bool recognizeStreamEnd(String& final_text) override;
    virtual bool synthesize(const String& text, std::vector<uint8_t>& audio_data) override;
    virtual bool synthesizeStreamStart(const String& text) override;
    virtual bool synthesizeStreamGetChunk(std::vector<uint8_t>& chunk, bool& is_last) override;
    virtual String getName() const override { return "volcano"; }
    virtual bool isAvailable() const override;
    virtual float getCostPerRequest() const override;

    // 配置管理
    void setNetworkManager(NetworkManager* netMgr);
    void setConfigManager(ConfigManager* configMgr);
    void setLogger(Logger* log);
    bool updateConfig(const VolcanoSpeechConfig& newConfig);
    const VolcanoSpeechConfig& getConfig() const { return config; }

    // 状态信息
    String getLastError() const override { return lastError; }
    void clearError() { lastError = ""; }

    // 工具方法
    static String getDefaultConfigJSON();
    static VolcanoSpeechConfig createDefaultConfig();

private:
    // 实际API调用
    bool callRecognitionAPI(const uint8_t* audio_data, size_t length, String& text);
    bool callWebSocketRecognitionAPI(const uint8_t* audio_data, size_t length, String& text);
    bool callSynthesisAPI(const String& text, std::vector<uint8_t>& audio_data);
    bool callStreamRecognitionStart();
    bool callStreamRecognitionChunk(const uint8_t* audio_chunk, size_t chunk_size, String& partial_text);
    bool callStreamRecognitionEnd(String& final_text);
    bool callStreamSynthesisStart(const String& text);
    bool callStreamSynthesisGetChunk(std::vector<uint8_t>& chunk, bool& is_last);
};

#endif // VOLCANO_SPEECH_SERVICE_H