#ifndef VOLCANO_SPEECH_SERVICE_H
#define VOLCANO_SPEECH_SERVICE_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../interfaces/SpeechService.h"
#include "../modules/NetworkManager.h"
#include "../interfaces/ConfigManager.h"
#include "../interfaces/Logger.h"
#include "WebSocketClient.h"
#include "BinaryProtocolEncoder.h"
#include "BinaryProtocolDecoder.h"
#include "VolcanoRequestBuilder.h"
#include "TTSRequestBuilder.h"
#include "TTSResponseParser.h"
#include "WebSocketSynthesisHandler.h"

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
    float asyncTimeout;    // 异步请求超时时间（秒）

    // WebSocket二进制协议配置
    bool binaryProtocolEnabled; // 是否启用二进制协议（默认true）
    bool useCompression;        // 是否启用GZIP压缩（默认false）

    // WebSocket端点配置
    String webSocketRecognitionNoStreamEndpoint;     // 非流式语音识别端点
    String webSocketRecognitionAsyncEndpoint;        // 异步流式语音识别端点
    String webSocketSynthesisUnidirectionalEndpoint; // 单向流式语音合成端点
    String webSocketSynthesisBidirectionalEndpoint;  // 双向流式语音合成端点

    // 语音合成特定配置
    String appId;           // 应用ID（用于TTS）
    String cluster;         // 集群标识（用于TTS，默认："volcano_tts"）
    String uid;             // 用户ID（用于TTS，默认："esp32_user"")
    String encoding;        // 音频编码格式（默认："pcm"）
    int sampleRate;         // 音频采样率（默认：16000）
    float speedRatio;       // 语速比例（默认：1.0）

    VolcanoSpeechConfig() :
        endpoint("https://openspeech.bytedance.com"),
        region("cn-north-1"),
        language("zh-CN"),
        voice("zh-CN_female_standard"),
        enablePunctuation(true),
        timeout(10.0f),
        asyncTimeout(5.0f),
        binaryProtocolEnabled(true),
        useCompression(false),
        webSocketRecognitionNoStreamEndpoint("wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_nostream"),
        webSocketRecognitionAsyncEndpoint("wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async"),
        webSocketSynthesisUnidirectionalEndpoint("wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream"),
        webSocketSynthesisBidirectionalEndpoint("wss://openspeech.bytedance.com/api/v3/tts/bidirection"),
        appId(""),
        cluster("volcano_tts"),
        uid("esp32_user"),
        encoding("pcm"),
        sampleRate(16000),
        speedRatio(1.0f) {}
};

// 异步识别结果
struct AsyncRecognitionResult {
    bool success;           // 是否识别成功
    String text;            // 识别文本（成功时有效）
    int errorCode;          // 错误码（0 = 无错误）
    String errorMessage;    // 人类可读的错误信息

    AsyncRecognitionResult() : success(false), text(""), errorCode(0), errorMessage("") {}
    AsyncRecognitionResult(bool s, const String& t, int ec, const String& em)
        : success(s), text(t), errorCode(ec), errorMessage(em) {}
};

// 重试策略配置
struct RetryPolicy {
    uint8_t maxRetries;          // 最大重试次数 (0 = 不重试)
    uint32_t initialBackoffMs;   // 初始退避时间（毫秒）
    float backoffMultiplier;     // 退避乘数（例如2.0表示每次翻倍）
    uint32_t maxBackoffMs;       // 最大退避时间（毫秒）

    RetryPolicy() : maxRetries(3), initialBackoffMs(1000), backoffMultiplier(2.0f), maxBackoffMs(10000) {}
    RetryPolicy(uint8_t maxRetries, uint32_t initialBackoffMs, float backoffMultiplier, uint32_t maxBackoffMs)
        : maxRetries(maxRetries), initialBackoffMs(initialBackoffMs), backoffMultiplier(backoffMultiplier), maxBackoffMs(maxBackoffMs) {}
};

// 异步识别回调函数类型
typedef std::function<void(const AsyncRecognitionResult&)> RecognitionCallback;

// 识别错误码
enum RecognitionErrorCode {
    ERROR_NONE = 0,                 // 无错误
    ERROR_TIMEOUT = 1001,           // 请求超时
    ERROR_NETWORK = 1002,           // 网络连接失败
    ERROR_AUTHENTICATION = 1003,    // 认证失败
    ERROR_PROTOCOL = 1004,          // 协议解析错误
    ERROR_SERVER = 1005,            // 服务器返回错误
    ERROR_INVALID_STATE = 1006,     // 无效状态（例如重复调用）
    ERROR_AUDIO_ENCODING = 1007,    // 音频编码失败
    ERROR_WEBSOCKET = 1008,         // WebSocket特定错误,
};

// 异步识别状态
enum AsyncRecognitionState {
    STATE_IDLE,              // 空闲，无进行中的请求
    STATE_CONNECTING,        // 连接WebSocket
    STATE_AUTHENTICATING,    // 发送认证
    STATE_SENDING_REQUEST,   // 发送完整客户端请求
    STATE_SENDING_AUDIO,     // 发送音频数据
    STATE_WAITING_RESPONSE,  // 等待服务器响应
    STATE_COMPLETED,         // 识别完成（成功或失败）
    STATE_TIMEOUT,           // 请求超时
    STATE_ERROR              // 发生错误
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

    // 异步语音识别API
    bool recognizeAsync(const uint8_t* audio_data, size_t length, RecognitionCallback callback);
    bool isAsyncRecognitionInProgress() const;
    void cancelAsyncRecognition();
    uint32_t getLastAsyncRequestId() const;
    uint32_t getAsyncRequestTimeout() const { return static_cast<uint32_t>(config.asyncTimeout * 1000); } // 毫秒超时时间

    // 更新方法（用于定期超时检查）
    void update();

    // 重试策略配置
    void setAsyncRetryPolicy(const RetryPolicy& policy);
    const RetryPolicy& getAsyncRetryPolicy() const { return asyncRetryPolicy; }
    void resetAsyncRetryState();
    bool shouldRetryAsyncRequest(int errorCode) const;
    bool isRetryableError(int errorCode) const;
    uint32_t calculateNextRetryDelay() const;

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
    bool synthesizeViaWebSocket(const String &text, std::vector<uint8_t> &audio_data);

    // 二进制协议消息处理
    void handleBinaryRecognitionResponse(const std::vector<uint8_t>& payload);
    void handleBinaryErrorMessage(const std::vector<uint8_t>& payload);

    // 异步识别状态
    volatile bool asyncRecognitionInProgress;
    AsyncRecognitionState asyncState;
    uint32_t asyncRequestStartTime;
    uint32_t lastAsyncRequestId;
    RecognitionCallback currentCallback;
    String asyncRecognitionText;
    int asyncRecognitionErrorCode;
    String asyncRecognitionErrorMessage;

    // 异步重试状态
    RetryPolicy asyncRetryPolicy;
    uint8_t currentRetryCount;
    uint32_t nextRetryTimeMs;
    int lastRetryableErrorCode;

    // 线程安全保护
    SemaphoreHandle_t asyncStateMutex;

    // 异步识别辅助方法
    void setupAsyncRecognitionState(RecognitionCallback callback = nullptr);
    void cleanupAsyncRecognitionState();
    bool setupWebSocketForAsyncRequest();
    void handleAsyncBinaryRecognitionResponse(const std::vector<uint8_t>& payload);
    void handleAsyncBinaryErrorMessage(const std::vector<uint8_t>& payload);
    void invokeAsyncCallback(const AsyncRecognitionResult& result);

    // 重试辅助方法
    void initializeAsyncRetryState();
    bool attemptAsyncRetry();
    void handleAsyncRetry();
    void scheduleAsyncRetry();
    void cancelAsyncRetry();
    void handleAsyncError(int errorCode, const String& errorMessage, bool immediateCleanup = true);

    // 线程安全辅助方法
    bool lockAsyncState(int timeoutMs = 100);
    void unlockAsyncState();
    bool getAsyncRecognitionInProgress() const;
    void setAsyncRecognitionInProgress(bool value);
    AsyncRecognitionState getAsyncState() const;
    void setAsyncState(AsyncRecognitionState state);
    void checkAsyncTimeout();
};

#endif // VOLCANO_SPEECH_SERVICE_H