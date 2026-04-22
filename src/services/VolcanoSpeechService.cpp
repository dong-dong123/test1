#include "VolcanoSpeechService.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include "../utils/MemoryUtils.h"
#include <ArduinoJson.h>
#include <time.h>
#include <esp_system.h>
#include <math.h>

// Check if zlib is available for gzip decompression
// Temporarily disabled due to compilation issues
// #ifdef HAS_ZLIB
// #include <zlib.h>
// #endif

// 生成Connect ID（UUID格式）
static String generateConnectId()
{
    // 生成UUID版本4格式：xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    // 使用esp_random()作为随机源
    String uuid;
    uuid.reserve(36);

    // 生成32个随机十六进制字符
    char hexChars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++)
    {
        uuid += hexChars[esp_random() & 0xF];
    }

    // 插入连字符
    uuid = uuid.substring(0, 8) + '-' + uuid.substring(8, 12) + '-' + uuid.substring(12, 16) + '-' + uuid.substring(16, 20) + '-' + uuid.substring(20);

    // 设置UUID版本4（第13个字符为4）
    uuid[14] = '4';

    // 设置变体1（第17个字符为8、9、A或B）
    char variants[] = {'8', '9', 'a', 'b'};
    uuid[19] = variants[esp_random() & 0x3];

    return uuid;
}

static const char *TAG = "VolcanoSpeechService";

// V2 API模式标志 - 用于跳过WebSocket认证消息（V2 API使用HTTP头部认证）
static bool g_v2APIInProgress = false;

// Base64编码辅助函数（简化实现）
static String base64Encode(const uint8_t *data, size_t length)
{
    // 简单的base64编码实现
    // 注意：生产环境应使用更健壮的base64库
    static const char *base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    String result;
    int i = 0;
    int j = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];

    while (length--)
    {
        char_array_3[i++] = *(data++);
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
            {
                result += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++)
        {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++)
        {
            result += base64_chars[char_array_4[j]];
        }

        while (i++ < 3)
        {
            result += '=';
        }
    }

    return result;
}

// Base64解码辅助函数
static std::vector<uint8_t> base64Decode(const String &encoded)
{
    // 简单的base64解码实现
    // 注意：生产环境应使用更健壮的base64库
    static const char *base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::vector<uint8_t> result;
    size_t in_len = encoded.length();
    if (in_len == 0)
        return result;

    const char *in = encoded.c_str();
    size_t i = 0;
    size_t j = 0;
    uint8_t char_array_4[4], char_array_3[3];

    // 创建反向查找表
    uint8_t reverse_table[256];
    for (int k = 0; k < 256; k++)
        reverse_table[k] = 0xFF;
    for (int k = 0; k < 64; k++)
        reverse_table[(uint8_t)base64_chars[k]] = k;

    while (in_len-- && (in[j] != '='))
    {
        char_array_4[i++] = in[j++];
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
            {
                char_array_4[i] = reverse_table[(uint8_t)char_array_4[i]];
                if (char_array_4[i] == 0xFF)
                {
                    // 无效base64字符
                    result.clear();
                    return result;
                }
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0x0F) << 4) + ((char_array_4[2] & 0x3C) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x03) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++)
            {
                result.push_back(char_array_3[i]);
            }
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 4; j++)
        {
            char_array_4[j] = 0;
        }

        for (j = 0; j < 4; j++)
        {
            if (char_array_4[j] != 0)
            {
                char_array_4[j] = reverse_table[(uint8_t)char_array_4[j]];
                if (char_array_4[j] == 0xFF)
                {
                    result.clear();
                    return result;
                }
            }
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0x0F) << 4) + ((char_array_4[2] & 0x3C) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x03) << 6) + char_array_4[3];

        for (j = 0; j < i - 1; j++)
        {
            result.push_back(char_array_3[j]);
        }
    }

    return result;
}

// API端点定义
const char *VolcanoSpeechService::RECOGNITION_API = "https://openspeech.bytedance.com/api/v1/asr";
const char *VolcanoSpeechService::SYNTHESIS_API = "https://openspeech.bytedance.com/api/v1/tts";
const char *VolcanoSpeechService::STREAM_RECOGNITION_API = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async";
const char *VolcanoSpeechService::NOSTREAM_RECOGNITION_API = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_nostream";
const char *VolcanoSpeechService::V2_RECOGNITION_API = "wss://openspeech.bytedance.com/api/v2/asr";
const char *VolcanoSpeechService::STREAM_SYNTHESIS_API = "wss://openspeech.bytedance.com/api/v1/tts/stream";

// ============================================================================
// 构造函数/析构函数
// ============================================================================

VolcanoSpeechService::VolcanoSpeechService(NetworkManager *netMgr,
                                           ConfigManager *configMgr,
                                           Logger *log)
    : networkManager(netMgr),
      configManager(configMgr),
      logger(log),
      isInitialized(false),
      isAvailableStatus(false),
      lastAvailabilityCheck(0),
      webSocketClient(nullptr),
      isStreamingRecognition(false),
      isStreamingSynthesis(false),
      streamStartTime(0),
      asyncRecognitionInProgress(false),
      asyncState(STATE_IDLE),
      asyncRequestStartTime(0),
      lastAsyncRequestId(0),
      asyncRecognitionErrorCode(ERROR_NONE),
      asyncRecognitionText(""),
      asyncRecognitionErrorMessage(""),
      asyncRetryPolicy(),
      currentRetryCount(0),
      nextRetryTimeMs(0),
      lastRetryableErrorCode(ERROR_NONE),
      asyncStateMutex(nullptr),
      lastLogIdTime(0),
      awaitingTextResponse(false),
      responseTimeoutMs(5000)   // 默认5秒响应超时（服务器应在收到log_id后5秒内返回text）
{
    // 参数验证和日志记录
    if (!networkManager)
    {
        ESP_LOGW(TAG, "NetworkManager is nullptr - service may not function properly");
    }

    if (!configManager)
    {
        ESP_LOGW(TAG, "ConfigManager is nullptr - using default configuration only");
    }

    if (!logger)
    {
        ESP_LOGW(TAG, "Logger is nullptr - logging will be limited to ESP_LOG");
    }

    // 创建互斥锁用于异步状态保护
    asyncStateMutex = xSemaphoreCreateMutex();
    if (!asyncStateMutex)
    {
        ESP_LOGE(TAG, "Failed to create async state mutex");
    }

    ESP_LOGI(TAG, "VolcanoSpeechService created");
    config = createDefaultConfig();
}

VolcanoSpeechService::~VolcanoSpeechService()
{
    deinitialize();

    // 删除互斥锁
    if (asyncStateMutex)
    {
        vSemaphoreDelete(asyncStateMutex);
        asyncStateMutex = nullptr;
    }
}

// ============================================================================
// 初始化/反初始化
// ============================================================================

bool VolcanoSpeechService::initialize()
{
    if (isInitialized)
    {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing VolcanoSpeechService...");

    // 检查必要的依赖
    if (!networkManager)
    {
        ESP_LOGE(TAG, "NetworkManager not set");
        lastError = "NetworkManager not set";
        return false;
    }

    // 加载配置
    if (!loadConfig())
    {
        ESP_LOGW(TAG, "Failed to load configuration, using defaults");
    }

    // 检查服务可用性
    updateAvailability();

    isInitialized = true;
    ESP_LOGI(TAG, "VolcanoSpeechService initialized successfully");

    logEvent("initialized", "Volcano speech service ready");

    return true;
}

bool VolcanoSpeechService::deinitialize()
{
    if (!isInitialized)
    {
        return true;
    }

    ESP_LOGI(TAG, "Deinitializing VolcanoSpeechService...");

    // 清理任何正在进行的流
    cleanupWebSocket();

    isInitialized = false;
    isAvailableStatus = false;
    ESP_LOGI(TAG, "VolcanoSpeechService deinitialized");

    logEvent("deinitialized", "Volcano speech service stopped");

    return true;
}

// ============================================================================
// 配置管理
// ============================================================================

bool VolcanoSpeechService::loadConfig()
{
    if (!configManager)
    {
        ESP_LOGE(TAG, "No ConfigManager available");
        return false;
    }

    // 从配置管理器读取火山引擎配置
    config.apiKey = configManager->getString("services.speech.volcano.apiKey", "");
    config.secretKey = configManager->getString("services.speech.volcano.secretKey", "");
    config.resourceId = configManager->getString("services.speech.volcano.resourceId", "volc.bigasr.sauc.duration");
    config.ttsResourceId = configManager->getString("services.speech.volcano.ttsResourceId", "seed-tts-2.0");
    config.endpoint = configManager->getString("services.speech.volcano.endpoint", "https://openspeech.bytedance.com");
    config.region = configManager->getString("services.speech.volcano.region", "cn-north-1");
    config.language = configManager->getString("services.speech.volcano.language", "zh-CN");
    config.voice = configManager->getString("services.speech.volcano.voice", "zh-CN_female_standard");
    config.appId = configManager->getString("services.speech.volcano.appId", "");
    config.cluster = configManager->getString("services.speech.volcano.cluster", "volcano_tts");
    config.encoding = configManager->getString("services.speech.volcano.encoding", "pcm");
    config.sampleRate = configManager->getInt("services.speech.volcano.sampleRate", 16000);
    config.speedRatio = configManager->getFloat("services.speech.volcano.speedRatio", 1.0f);
    config.enablePunctuation = configManager->getBool("services.speech.volcano.enablePunctuation", true);
    config.timeout = configManager->getFloat("services.speech.volcano.timeout", 10.0f);
    config.asyncTimeout = configManager->getFloat("services.speech.volcano.asyncTimeout", config.asyncTimeout);

    if (config.apiKey.isEmpty())
    {
        ESP_LOGW(TAG, "No API key configured for Volcano service");
    }

    ESP_LOGI(TAG, "Loaded Volcano config: Endpoint=%s, Region=%s, Language=%s, ResourceId=%s",
             config.endpoint.c_str(), config.region.c_str(), config.language.c_str(), config.resourceId.c_str());

    return true;
}

bool VolcanoSpeechService::updateConfig(const VolcanoSpeechConfig &newConfig)
{
    config = newConfig;

    // 如果已初始化，重新检查可用性
    if (isInitialized)
    {
        updateAvailability();
    }

    return true;
}

void VolcanoSpeechService::setNetworkManager(NetworkManager *netMgr)
{
    networkManager = netMgr;
    if (isInitialized && networkManager)
    {
        updateAvailability();
    }
}

void VolcanoSpeechService::setConfigManager(ConfigManager *configMgr)
{
    configManager = configMgr;
    if (isInitialized && configManager)
    {
        loadConfig();
        updateAvailability();
    }
}

void VolcanoSpeechService::setLogger(Logger *log)
{
    logger = log;
}

// ============================================================================
// SpeechService接口实现
// ============================================================================

bool VolcanoSpeechService::recognize(const uint8_t *audio_data, size_t length, String &text)
{
    if (!isInitialized)
    {
        ESP_LOGE(TAG, "Service not initialized");
        lastError = "Service not initialized";
        return false;
    }

    if (!isAvailable())
    {
        ESP_LOGE(TAG, "Service not available");
        lastError = "Service not available";
        return false;
    }

    ESP_LOGI(TAG, "Recognizing speech (length: %u bytes)...", length);

    bool success = callRecognitionAPI(audio_data, length, text);

    if (success)
    {
        ESP_LOGI(TAG, "Recognition successful: %s", text.c_str());
        logEvent("recognition_success", "Length: " + String(length) + " bytes");
    }
    else
    {
        ESP_LOGE(TAG, "Recognition failed: %s", lastError.c_str());
        logEvent("recognition_failed", lastError);
    }

    return success;
}

bool VolcanoSpeechService::recognizeStreamStart()
{
    if (!isInitialized)
    {
        lastError = "Service not initialized";
        return false;
    }

    ESP_LOGI(TAG, "Starting stream recognition...");
    return callStreamRecognitionStart();
}

bool VolcanoSpeechService::recognizeStreamChunk(const uint8_t *audio_chunk, size_t chunk_size, String &partial_text)
{
    if (!isInitialized)
    {
        lastError = "Service not initialized";
        return false;
    }

    return callStreamRecognitionChunk(audio_chunk, chunk_size, partial_text);
}

bool VolcanoSpeechService::recognizeStreamEnd(String &final_text)
{
    if (!isInitialized)
    {
        lastError = "Service not initialized";
        return false;
    }

    ESP_LOGI(TAG, "Ending stream recognition...");
    return callStreamRecognitionEnd(final_text);
}

bool VolcanoSpeechService::synthesize(const String &text, std::vector<uint8_t> &audio_data)
{
    if (!isInitialized)
    {
        ESP_LOGE(TAG, "Service not initialized");
        lastError = "Service not initialized";
        return false;
    }

    if (!isAvailable())
    {
        ESP_LOGE(TAG, "Service not available");
        lastError = "Service not available";
        return false;
    }

    if (text.isEmpty())
    {
        ESP_LOGE(TAG, "Text is empty");
        lastError = "Text is empty";
        return false;
    }

    ESP_LOGI(TAG, "Synthesizing speech: %s", text.c_str());

    bool success = callSynthesisAPI(text, audio_data);

    if (success)
    {
        ESP_LOGI(TAG, "Synthesis successful, audio size: %u bytes", audio_data.size());
        logEvent("synthesis_success", "Text length: " + String(text.length()));
    }
    else
    {
        ESP_LOGE(TAG, "Synthesis failed: %s", lastError.c_str());
        logEvent("synthesis_failed", lastError);
    }

    return success;
}

bool VolcanoSpeechService::synthesizeStreamStart(const String &text)
{
    if (!isInitialized)
    {
        lastError = "Service not initialized";
        return false;
    }

    ESP_LOGI(TAG, "Starting stream synthesis...");
    return callStreamSynthesisStart(text);
}

bool VolcanoSpeechService::synthesizeStreamGetChunk(std::vector<uint8_t> &chunk, bool &is_last)
{
    if (!isInitialized)
    {
        lastError = "Service not initialized";
        return false;
    }

    return callStreamSynthesisGetChunk(chunk, is_last);
}

bool VolcanoSpeechService::synthesizeViaWebSocket(const String &text, std::vector<uint8_t> &audio_data)
{
    ESP_LOGI(TAG, "Using WebSocket binary protocol for synthesis");

    // Create WebSocketSynthesisHandler instance
    WebSocketSynthesisHandler handler(networkManager, configManager);

    // Configure handler with TTS parameters from config
    handler.setConfiguration(
        config.appId.isEmpty() ? config.apiKey : config.appId,  // Application ID
        config.secretKey,       // Access Token (stored in secretKey)
        config.cluster,         // Cluster identifier
        config.uid,             // User ID
        config.voice,           // Voice type
        config.encoding,        // Audio encoding
        config.sampleRate,      // Sample rate
        config.speedRatio,      // Speed ratio
        config.ttsResourceId    // TTS resource ID
    );

    // Set timeouts from config (convert seconds to milliseconds)
    uint32_t connectTimeoutMs = static_cast<uint32_t>(config.timeout * 1000);
    uint32_t responseTimeoutMs = static_cast<uint32_t>(config.timeout * 1000);
    uint32_t chunkTimeoutMs = 5000;  // Fixed chunk timeout

    handler.setTimeouts(connectTimeoutMs, responseTimeoutMs, chunkTimeoutMs);

    // Call WebSocket synthesis
    bool success = handler.synthesizeViaWebSocket(
        text,
        audio_data,
        config.webSocketSynthesisUnidirectionalEndpoint
    );

    if (!success) {
        lastError = "WebSocket synthesis failed: " + handler.getLastError();
        ESP_LOGE(TAG, "WebSocket synthesis failed: %s", handler.getLastError().c_str());
    } else {
        ESP_LOGI(TAG, "WebSocket synthesis successful, audio size: %u bytes", audio_data.size());
    }

    return success;
}

bool VolcanoSpeechService::isAvailable() const
{
    // 如果最近检查过，返回缓存状态
    uint32_t currentTime = millis();
    if (currentTime - lastAvailabilityCheck < 60000)
    { // 1分钟缓存
        return isAvailableStatus;
    }

    // 非const版本更新状态
    const_cast<VolcanoSpeechService *>(this)->updateAvailability();
    return isAvailableStatus;
}

float VolcanoSpeechService::getCostPerRequest() const
{
    // 火山引擎成本估算（单位：元/请求）
    // 实际成本可能根据使用量变化
    return 0.01f; // 假设每请求0.01元
}

// ============================================================================
// 可用性检查
// ============================================================================

bool VolcanoSpeechService::updateAvailability()
{
    if (!networkManager || !networkManager->isConnected())
    {
        isAvailableStatus = false;
        lastError = "Network not connected";
        return false;
    }

    if (config.apiKey.isEmpty())
    {
        isAvailableStatus = false;
        lastError = "API key not configured";
        return false;
    }

    // 简单的心跳检查 - 发送一个测试请求或检查网络连接
    // 这里简化处理，假设有网络和API key就可用
    // 实际应该发送一个轻量级API请求验证

    isAvailableStatus = true;
    lastAvailabilityCheck = millis();
    lastError = "";

    return true;
}

// ============================================================================
// API调用实现（框架）
// ============================================================================

bool VolcanoSpeechService::callRecognitionAPI(const uint8_t *audio_data, size_t length, String &text)
{
    // 根据火山引擎API文档实现语音识别
    // 参考文档: https://www.volcengine.com/docs/6561/1079478

    if (!networkManager || !networkManager->isConnected())
    {
        lastError = "Network not available";
        ESP_LOGE(TAG, "Network not available for API call");
        return false;
    }

    if (config.apiKey.isEmpty())
    {
        lastError = "API credentials not configured (missing API Key/APP ID)";
        ESP_LOGE(TAG, "API key not configured");
        return false;
    }

    // secretKey可为空（如果使用Bearer Token认证）
    // 如果secretKey为空且apiKey不是Access Token格式，则可能需要其他认证方式
    if (config.secretKey.isEmpty() && config.apiKey.indexOf('.') < 0)
    {
        ESP_LOGW(TAG, "Secret key empty and API key doesn't look like Access Token");
        // 仍可继续，可能使用简单的APP ID认证
    }

    ESP_LOGI(TAG, "Calling Volcano speech recognition API (audio length: %u bytes)", length);

    // 构建API URL（使用配置的endpoint）
    String recognitionUrl = config.endpoint;
    if (!recognitionUrl.endsWith("/api/v1/asr"))
    {
        recognitionUrl += "/api/v1/asr";
    }

    // 构建请求头 - 与语音合成API保持一致
    std::map<String, String> headers;

    // 使用Bearer Token认证（Access Token）
    // config.secretKey字段存储Access Token
    // 火山API使用"Bearer;"格式（带分号）
    if (!config.secretKey.isEmpty())
    {
        headers["Authorization"] = "Bearer;" + config.secretKey;
        ESP_LOGI(TAG, "Using Bearer token authentication (secretKey) with semicolon");
    }
    else
    {
        // 如果没有配置Access Token，尝试使用API Key作为备用
        headers["Authorization"] = "Bearer;" + config.apiKey;
        ESP_LOGW(TAG, "Using API key as Access Token (may fail) with semicolon");
    }

    // 添加火山API特定的头部（与语音合成API保持一致）
    // X-Api-App-Id: APP ID
    if (!config.apiKey.isEmpty())
    {
        headers["X-Api-App-Id"] = config.apiKey;
    }
    else if (!config.appId.isEmpty())
    {
        headers["X-Api-App-Id"] = config.appId;
    }

    // X-Api-Access-Key: Access Token
    if (!config.secretKey.isEmpty())
    {
        headers["X-Api-Access-Key"] = config.secretKey;
    }
    else
    {
        // 如果没有配置Access Token，尝试使用API Key作为备用
        headers["X-Api-Access-Key"] = config.apiKey;
        ESP_LOGW(TAG, "Using API key as X-Api-Access-Key (may fail)");
    }

    headers["Content-Type"] = "application/json";

    // 调试：打印认证信息（隐藏完整token）
    ESP_LOGI(TAG, "Auth config - API Key: %s, Secret Key length: %d",
             config.apiKey.c_str(), config.secretKey.length());
    if (!config.secretKey.isEmpty())
    {
        ESP_LOGI(TAG, "Secret Key starts with: %c", config.secretKey.charAt(0));
    }

    // 基于火山引擎WebSocket语音识别API文档构建请求格式
    // 参考用户提供的API示例格式
    DynamicJsonDocument requestDoc(8192); // 需要更大空间因为base64音频数据

    // user对象（必需）
    JsonObject user = requestDoc.createNestedObject("user");
    user["uid"] = "esp32_user"; // 用户标识

    // audio对象（必需）
    JsonObject audio = requestDoc.createNestedObject("audio");
    audio["format"] = "pcm";
    audio["codec"] = "raw"; // PCM格式使用raw编码
    audio["rate"] = 16000;
    audio["bits"] = 16;
    audio["channel"] = 1;
    audio["language"] = config.language;

    // request对象（必需）
    JsonObject request = requestDoc.createNestedObject("request");
    // 生成唯一请求ID（使用时间戳）
    uint32_t timestamp = (uint32_t)time(nullptr);
    String reqid = "esp32_" + String(timestamp) + "_" + String(rand());
    request["reqid"] = reqid;                          // 请求ID
    request["model_name"] = "bigmodel";                // 模型名称
    request["enable_itn"] = true;                      // 启用文本规范化
    request["enable_punc"] = config.enablePunctuation; // 标点符号
    request["enable_ddc"] = false;                     // 禁用语义顺滑

    // 音频数据（base64编码）- 放在根级别（根据语音合成API模式）
    String audioBase64 = base64Encode(audio_data, length);
    requestDoc["audio_data"] = audioBase64;

    ESP_LOGI(TAG, "Audio data length: %u bytes, Base64 length: %u chars",
             length, audioBase64.length());
    ESP_LOGI(TAG, "First 20 chars of base64: %.20s", audioBase64.c_str());

    String requestBody;
    serializeJson(requestDoc, requestBody);

    // 打印请求摘要信息
    ESP_LOGI(TAG, "Request body size: %u bytes, Base64 audio: %u chars",
             requestBody.length(), audioBase64.length());

    // 打印完整的请求体（前800字符）
    if (requestBody.length() > 800)
    {
        ESP_LOGI(TAG, "First 800 chars of request body: %.800s", requestBody.c_str());
        ESP_LOGI(TAG, "... (truncated, total %u chars)", requestBody.length());
    }
    else
    {
        ESP_LOGI(TAG, "Full request body: %s", requestBody.c_str());
    }

    // 打印认证头部信息
    ESP_LOGI(TAG, "Sending request to: %s", recognitionUrl.c_str());
    for (const auto &header : headers)
    {
        ESP_LOGI(TAG, "Header: %s: %s", header.first.c_str(), header.second.c_str());
    }

    // 发送HTTP请求
    HttpResponse response = networkManager->postJson(recognitionUrl, requestBody, headers);

    // 检查响应
    if (response.statusCode != 200)
    {
        lastError = "API request failed with status: " + String(response.statusCode);
        if (!response.body.isEmpty())
        {
            lastError += ", response: " + response.body.substring(0, 100); // 截断长响应
        }
        ESP_LOGE(TAG, "Recognition API failed: %s", lastError.c_str());
        return false;
    }

    // 解析响应JSON
    DynamicJsonDocument responseDoc(2048);
    DeserializationError error = deserializeJson(responseDoc, response.body);

    if (error)
    {
        lastError = "Failed to parse API response: " + String(error.c_str());
        ESP_LOGE(TAG, "JSON parse error: %s", error.c_str());
        return false;
    }

    // 根据火山引擎API响应格式提取识别结果
    // 可能格式：
    // 1. HTTP语音合成API格式: {"code": 3000, "message": "Success", "data": "文本"}
    // 2. WebSocket响应格式: {"result": {"text": "...", "utterances": [...]}, "audio_info": {...}}
    // 3. 错误格式: {"code": 1001, "message": "错误信息"}

    // 检查是否有错误码
    int code = responseDoc["code"] | -1;
    if (code != -1 && code != 3000 && code != 20000000) // 3000=HTTP成功, 20000000=WebSocket成功
    {
        String message = responseDoc["message"] | "Unknown error";
        lastError = "API error " + String(code) + ": " + message;
        ESP_LOGE(TAG, "API returned error: %s", lastError.c_str());
        return false;
    }

    // 提取识别文本
    // 尝试多种可能的响应字段
    if (responseDoc.containsKey("data"))
    {
        // data字段可能是字符串（文本）或对象
        if (responseDoc["data"].is<String>())
        {
            text = responseDoc["data"].as<String>();
            ESP_LOGI(TAG, "Recognition successful (data as string): %s", text.c_str());
            return true;
        }
        else if (responseDoc["data"].is<JsonObject>() && responseDoc["data"].containsKey("text"))
        {
            text = responseDoc["data"]["text"].as<String>();
            ESP_LOGI(TAG, "Recognition successful (data.text): %s", text.c_str());
            return true;
        }
    }
    else if (responseDoc.containsKey("result"))
    {
        text = responseDoc["result"].as<String>();
        ESP_LOGI(TAG, "Recognition successful (result field): %s", text.c_str());
        return true;
    }
    else if (responseDoc.containsKey("text"))
    {
        text = responseDoc["text"].as<String>();
        ESP_LOGI(TAG, "Recognition successful (root text field): %s", text.c_str());
        return true;
    }
    else
    {
        lastError = "No recognition result in API response";
        ESP_LOGE(TAG, "Response missing text field: %s", response.body.substring(0, 200).c_str());
        return false;
    }
    return false;
}

bool VolcanoSpeechService::callWebSocketRecognitionAPI(const uint8_t *audio_data, size_t length, String &text)
{
    ESP_LOGI(TAG, "Starting WebSocket non-stream recognition (audio length: %u bytes)", length);

    if (!networkManager || !networkManager->isConnected())
    {
        lastError = "Network not available";
        ESP_LOGE(TAG, "Network not available for WebSocket recognition");
        return false;
    }

    if (config.apiKey.isEmpty())
    {
        lastError = "API credentials not configured";
        ESP_LOGE(TAG, "API key not configured for WebSocket recognition");
        return false;
    }

    // 连接WebSocket non-stream端点
    if (!webSocketClient)
    {
        initializeWebSocket();
    }

    // 设置WebSocket头部（与流式识别相同）
    String headers = "";
    if (!config.apiKey.isEmpty())
    {
        headers += "X-Api-App-Key: " + config.apiKey + "\r\n";
    }
    if (!config.secretKey.isEmpty())
    {
        headers += "X-Api-Access-Key: " + config.secretKey + "\r\n";
    }
    else
    {
        headers += "X-Api-Access-Key: " + config.apiKey + "\r\n";
    }
    // 资源ID：使用种子模型流式语音识别小时版（根据用户实例类型）
    // volc.seedasr.sauc.duration - 种子模型小时版
    // volc.bigasr.sauc.duration - ASR 1.0小时版
    headers += "X-Api-Resource-Id: " + config.resourceId + "\r\n";
    // 生成连接ID
    String uuid = generateConnectId();
    headers += "X-Api-Connect-Id: " + uuid + "\r\n";
    // 添加协议协商头部，明确要求不压缩的JSON格式
    // 临时注释协议协商头部，以恢复服务器响应
    // headers += "Accept-Encoding: identity\r\n"; // 要求服务器不要压缩响应
    // headers += "Accept: application/json";      // 明确请求JSON格式响应

    ESP_LOGI(TAG, "Connecting to WebSocket non-stream recognition API: %s", config.webSocketRecognitionNoStreamEndpoint.c_str());
    webSocketClient->setExtraHeaders(headers);

    if (!webSocketClient->connect(config.webSocketRecognitionNoStreamEndpoint))
    {
        lastError = "Failed to connect to WebSocket: " + webSocketClient->getLastError();
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    // 等待连接建立（简化处理）
    delay(100);

    // 发送认证消息（根据现有实现）
    if (!sendWebSocketAuth())
    {
        ESP_LOGE(TAG, "WebSocket authentication failed");
        return false;
    }

    // 清空之前的结果
    partialRecognitionText.clear();

    // 使用VolcanoRequestBuilder构建Full Client Request JSON
    String fullClientRequestJson = VolcanoRequestBuilder::buildFullClientRequest(
        "esp32_user",                 // uid
        config.language,              // language
        config.enablePunctuation,     // enablePunctuation
        true,                         // enableITN (逆文本归一化)
        false,                        // enableDDC (数字转换)
        "pcm",                        // format (根据API文档：pcm / wav / ogg / mp3)
        16000,                        // rate
        16,                           // bits
        1,                            // channel
        "raw",                        // codec (same as format)
        config.apiKey,                // appid
        config.secretKey,             // token
        config.resourceId,            // resourceId (added to JSON for server resource validation)
        "volcengine_streaming_common" // cluster (参考代码格式)
    );

    if (fullClientRequestJson.isEmpty())
    {
        lastError = "Failed to build full client request JSON";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        webSocketClient->disconnect();
        return false;
    }

    ESP_LOGI(TAG, "Full client request JSON size: %u bytes", fullClientRequestJson.length());
    ESP_LOGV(TAG, "Full client request JSON: %s", fullClientRequestJson.c_str());

    // 使用BinaryProtocolEncoder编码Full Client Request
    std::vector<uint8_t> encodedFullRequest = BinaryProtocolEncoder::encodeFullClientRequest(
        fullClientRequestJson,
        config.useCompression, // useCompression
        0                      // sequence字段省略，服务器自动分配
    );

    if (encodedFullRequest.empty())
    {
        lastError = "Failed to encode full client request";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        webSocketClient->disconnect();
        return false;
    }

    ESP_LOGI(TAG, "Encoded full client request size: %u bytes", encodedFullRequest.size());

    // 发送编码后的Full Client Request二进制消息
    if (!webSocketClient->sendBinary(encodedFullRequest.data(), encodedFullRequest.size()))
    {
        lastError = "Failed to send encoded full client request";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        webSocketClient->disconnect();
        return false;
    }

    ESP_LOGI(TAG, "Full client request sent successfully");

    // 使用BinaryProtocolEncoder编码Audio Only Request
    std::vector<uint8_t> encodedAudioRequest = BinaryProtocolEncoder::encodeAudioOnlyRequest(
        audio_data,
        length,
        true,                  // isLastChunk (这是唯一的音频块)
        config.useCompression, // useCompression
        static_cast<uint32_t>(static_cast<int32_t>(-1)) // 单包序列号为-1
    );

    if (encodedAudioRequest.empty())
    {
        lastError = "Failed to encode audio only request";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        webSocketClient->disconnect();
        return false;
    }

    ESP_LOGI(TAG, "Encoded audio only request size: %u bytes (original audio: %u bytes)",
             encodedAudioRequest.size(), length);

    // 发送编码后的Audio Only Request二进制消息
    if (!webSocketClient->sendBinary(encodedAudioRequest.data(), encodedAudioRequest.size()))
    {
        lastError = "Failed to send encoded audio only request";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        webSocketClient->disconnect();
        return false;
    }

    ESP_LOGI(TAG, "Audio only request sent successfully");

    // 等待服务器响应
    ESP_LOGI(TAG, "Waiting for response (timeout: %.1f seconds)...", config.timeout);
    uint32_t timeoutMs = static_cast<uint32_t>(config.timeout * 1000);
    uint32_t startTime = millis();
    bool receivedResponse = false;

    while (millis() - startTime < timeoutMs)
    {
        // 检查是否收到响应
        if (!partialRecognitionText.isEmpty())
        {
            ESP_LOGI(TAG, "Received recognition response: %s", partialRecognitionText.c_str());
            text = partialRecognitionText;
            receivedResponse = true;
            break;
        }

        // 检查WebSocket错误
        if (webSocketClient->getLastError() != 0)
        {
            lastError = "WebSocket error during response waiting: " + String(webSocketClient->getLastError());
            ESP_LOGE(TAG, "%s", lastError.c_str());
            break;
        }

        // 短暂延迟以避免忙等待
        delay(100);
    }

    webSocketClient->disconnect();

    if (receivedResponse)
    {
        ESP_LOGI(TAG, "WebSocket recognition succeeded");
        return true;
    }
    else
    {
        if (partialRecognitionText.isEmpty())
        {
            lastError = "WebSocket recognition timeout - no response received";
            ESP_LOGE(TAG, "%s", lastError.c_str());
        }
        else
        {
            lastError = "WebSocket recognition failed";
            ESP_LOGE(TAG, "%s", lastError.c_str());
        }
        return false;
    }
}

bool VolcanoSpeechService::callSynthesisAPI(const String &text, std::vector<uint8_t> &audio_data)
{
    // 根据火山引擎语音合成API文档实现（V1 HTTP非流式接口）
    // 参考文档：用户提供的API文档
    // 用户提供：APP ID: 2015527679, Access Token: R23gVDqaVB_j-TaRfNywkJnerpGGJtcB

    if (!networkManager || !networkManager->isConnected())
    {
        lastError = "Network not available";
        ESP_LOGE(TAG, "Network not available for synthesis API call");
        return false;
    }

    if (config.apiKey.isEmpty())
    { // apiKey字段存储APP ID
        lastError = "APP ID not configured";
        ESP_LOGE(TAG, "APP ID not configured for synthesis");
        return false;
    }

    ESP_LOGI(TAG, "Calling Volcano speech synthesis API for text: %s", text.c_str());

    // 根据火山引擎API文档构建请求
    // 认证：使用Bearer Token (Access Token)，格式为 "Bearer;${token}"
    // 端点：https://openspeech.bytedance.com/api/v1/tts

    // 构建请求头 - 使用火山引擎V1 TTS API的X-Api-*头部格式
    // 根据用户验证，TTS API使用以下头部：
    // X-Api-App-Id: APP ID (不是X-Api-App-Key)
    // X-Api-Access-Key: Access Token
    // X-Api-Resource-Id: TTS资源ID (如seed-tts-2.0)
    std::map<String, String> headers;

    // 使用X-Api-App-Id头部（TTS API专用）
    if (!config.apiKey.isEmpty())
    {
        headers["X-Api-App-Id"] = config.apiKey;
    }
    else if (!config.appId.isEmpty())
    {
        headers["X-Api-App-Id"] = config.appId;
    }

    // 使用X-Api-Access-Key头部
    if (!config.secretKey.isEmpty())
    {
        headers["X-Api-Access-Key"] = config.secretKey;
    }
    else
    {
        // 如果没有配置Access Token，尝试使用API Key作为备用
        headers["X-Api-Access-Key"] = config.apiKey;
        ESP_LOGW(TAG, "Using API key as Access Token (may fail)");
    }

    // 使用X-Api-Resource-Id头部（TTS资源ID）
    String resourceId = config.ttsResourceId.isEmpty() ? config.resourceId : config.ttsResourceId;
    headers["X-Api-Resource-Id"] = resourceId;

    headers["Content-Type"] = "application/json";

    // 添加标准的Authorization头（火山API可能需要）
    // 根据文档，格式为"Bearer;${token}"（带分号）
    if (!config.secretKey.isEmpty()) {
        headers["Authorization"] = "Bearer;" + config.secretKey;
        ESP_LOGI(TAG, "Added Authorization header with Bearer; format");
    } else if (!config.apiKey.isEmpty()) {
        // 备用：使用API Key作为token
        headers["Authorization"] = "Bearer;" + config.apiKey;
        ESP_LOGW(TAG, "Added Authorization header using API key as token");
    }

    // 构建请求体，根据火山引擎API文档格式
    // 参考请求示例：
    // {
    //   "app": {"appid": "appid123", "token": "access_token", "cluster": "volcano_tts"},
    //   "user": {"uid": "uid123"},
    //   "audio": {"voice_type": "...", "encoding": "pcm", "speed_ratio": 1.0},
    //   "request": {"reqid": "uuid", "text": "...", "operation": "query"}
    // }

    // 使用堆分配避免栈溢出
    DynamicJsonDocument* requestDoc = new DynamicJsonDocument(1024);
    if (!requestDoc) {
        lastError = "Failed to allocate memory for JSON request";
        ESP_LOGE(TAG, "Failed to allocate DynamicJsonDocument for request");
        return false;
    }

    // app对象
    JsonObject appObj = requestDoc->createNestedObject("app");
    appObj["appid"] = config.apiKey; // APP ID
    // token字段：文档说明是"无实际鉴权作用的Fake token，可传任意非空字符串"
    // 但实际可能需传Access Token，这里使用secretKey或apiKey
    appObj["token"] = !config.secretKey.isEmpty() ? config.secretKey : config.apiKey;
    appObj["cluster"] = "volcano_tts"; // 固定值

    // user对象
    JsonObject userObj = requestDoc->createNestedObject("user");
    userObj["uid"] = "esp32_user"; // 固定用户ID，可配置

    // audio对象
    JsonObject audioObj = requestDoc->createNestedObject("audio");
    audioObj["voice_type"] = config.voice; // 音色类型
    audioObj["encoding"] = "pcm";          // 音频编码格式，与音频驱动匹配
    audioObj["rate"] = 16000;              // 采样率，应与音频驱动匹配
    audioObj["speed_ratio"] = 1.0;         // 语速
    // 可添加其他参数：volume, loudness_ratio等

    // request对象
    JsonObject requestObj = requestDoc->createNestedObject("request");
    // 生成唯一请求ID（简化：使用时间戳）
    uint32_t timestamp = (uint32_t)time(nullptr);
    requestObj["reqid"] = "esp32_" + String(timestamp) + "_" + String(rand());
    requestObj["text"] = text;
    requestObj["operation"] = "query"; // 非流式操作

    String requestBody;
    serializeJson(*requestDoc, requestBody);
    delete requestDoc; // 清理堆分配，不再需要

    ESP_LOGI(TAG, "Sending synthesis request to: %s", SYNTHESIS_API);
    ESP_LOGV(TAG, "Request body: %s", requestBody.c_str());

    // 发送HTTP请求前的内存监控（特别关注栈使用）
    ESP_LOGI(TAG, "=== 语音合成API调用前内存状态 ===");
    MemoryUtils::printDetailedMemoryStatus("Pre-Synthesis API Call");
    MemoryUtils::monitorTaskStacks("Pre-Synthesis API Call");

    // 发送HTTP请求
    HttpResponse response = networkManager->postJson(SYNTHESIS_API, requestBody, headers);

    // 发送HTTP请求后的内存监控
    ESP_LOGI(TAG, "=== 语音合成API调用后内存状态 ===");
    MemoryUtils::printDetailedMemoryStatus("Post-Synthesis API Call");
    MemoryUtils::monitorTaskStacks("Post-Synthesis API Call");

    // 检查响应
    ESP_LOGI(TAG, "Synthesis API response status: %d, time: %dms",
             response.statusCode, response.responseTime);

    if (response.statusCode != 200)
    {
        lastError = "Synthesis API request failed with status: " + String(response.statusCode);
        if (!response.body.isEmpty())
        {
            // 尝试解析错误信息
            DynamicJsonDocument errorDoc(512);
            if (deserializeJson(errorDoc, response.body) == DeserializationError::Ok)
            {
                String message = errorDoc["message"] | "Unknown error";
                lastError += ", message: " + message;
            }
            else
            {
                lastError += ", response: " + response.body.substring(0, 100);
            }
            ESP_LOGE(TAG, "API error response: %s", response.body.c_str());
        }
        ESP_LOGE(TAG, "Synthesis API failed: %s", lastError.c_str());
        return false;
    }

    // 解析响应JSON
    // 根据文档，响应格式为：
    // {
    //   "reqid": "...",
    //   "code": 3000,
    //   "operation": "query",
    //   "message": "Success",
    //   "sequence": -1,
    //   "data": "base64 encoded binary data",
    //   "addition": {"duration": "1960"}
    // }

    // 使用堆分配避免栈溢出 - 火山API响应可能包含大量base64数据
    DynamicJsonDocument* responseDoc = new DynamicJsonDocument(8192); // 8KB堆分配
    if (!responseDoc) {
        lastError = "Failed to allocate memory for JSON parsing";
        ESP_LOGE(TAG, "Failed to allocate DynamicJsonDocument on heap");
        return false;
    }

    DeserializationError error = deserializeJson(*responseDoc, response.body);

    if (error)
    {
        lastError = "Failed to parse API response: " + String(error.c_str());
        ESP_LOGE(TAG, "JSON parse error: %s", error.c_str());
        return false;
    }

    // 检查API返回码
    int code = (*responseDoc)["code"] | -1;
    if (code != 3000)
    { // 3000表示成功
        String message = (*responseDoc)["message"] | "Unknown error";
        lastError = "API error " + String(code) + ": " + message;
        ESP_LOGE(TAG, "Synthesis API returned error: %s", lastError.c_str());
        delete responseDoc; // 清理堆分配
        return false;
    }

    // 提取base64编码的音频数据
    if (!responseDoc->containsKey("data") || !(*responseDoc)["data"].is<String>())
    {
        lastError = "No audio data in API response";
        ESP_LOGE(TAG, "Response missing data field: %s", response.body.substring(0, 200).c_str());
        delete responseDoc; // 清理堆分配
        return false;
    }

    String audioBase64 = (*responseDoc)["data"].as<String>();
    if (audioBase64.isEmpty())
    {
        lastError = "Empty audio data in API response";
        ESP_LOGE(TAG, "Empty data field in response");
        delete responseDoc; // 清理堆分配
        return false;
    }

    // 解码base64音频数据
    audio_data = base64Decode(audioBase64);

    if (audio_data.empty())
    {
        lastError = "Base64 decoding failed or empty result";
        ESP_LOGE(TAG, "Failed to decode base64 audio data (input length: %u)", audioBase64.length());

        // 调试：检查base64字符串的前几个字符
        if (audioBase64.length() > 0)
        {
            ESP_LOGE(TAG, "First 10 chars: %.10s", audioBase64.c_str());
        }
        delete responseDoc; // 清理堆分配
        return false;
    }

    ESP_LOGI(TAG, "Successfully decoded base64 audio data, size: %u bytes", audio_data.size());
    delete responseDoc; // 清理堆分配
    return true;
}

bool VolcanoSpeechService::callStreamRecognitionStart()
{
    if (!networkManager || !networkManager->isConnected())
    {
        lastError = "Network not available";
        ESP_LOGE(TAG, "Network not available for stream recognition");
        return false;
    }

    if (config.apiKey.isEmpty())
    {
        lastError = "API credentials not configured";
        ESP_LOGE(TAG, "API key not configured for stream recognition");
        return false;
    }

    // 连接WebSocket流
    if (!connectWebSocketStream("recognition"))
    {
        return false;
    }

    // 等待连接建立（已包含在connectWebSocketStream中）
    // 发送认证消息
    if (!sendWebSocketAuth())
    {
        return false;
    }

    // 发送开始识别消息
    if (!sendWebSocketRecognitionStart())
    {
        return false;
    }

    // 清空之前的音频数据
    pendingAudioChunks.clear();
    partialRecognitionText = "";

    ESP_LOGI(TAG, "Stream recognition started successfully");
    return true;
}

bool VolcanoSpeechService::callStreamRecognitionChunk(const uint8_t *audio_chunk, size_t chunk_size, String &partial_text)
{
    if (!isStreamingRecognition || !webSocketClient || !webSocketClient->isConnected())
    {
        lastError = "Stream recognition not started or WebSocket not connected";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    // 发送二进制音频数据
    if (!webSocketClient->sendBinary(audio_chunk, chunk_size))
    {
        lastError = "Failed to send audio chunk via WebSocket";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    // 更新部分识别文本（从回调中获取）
    partial_text = partialRecognitionText;

    ESP_LOGV(TAG, "Sent audio chunk: %u bytes, partial text: %s", chunk_size, partial_text.c_str());
    return true;
}

bool VolcanoSpeechService::callStreamRecognitionEnd(String &final_text)
{
    if (!isStreamingRecognition || !webSocketClient || !webSocketClient->isConnected())
    {
        lastError = "Stream recognition not started or WebSocket not connected";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    // 发送结束识别消息
    DynamicJsonDocument endDoc(128);
    endDoc["type"] = "stop";
    String endJson;
    serializeJson(endDoc, endJson);

    if (!webSocketClient->sendTextChunked(endJson, 128))
    {
        lastError = "Failed to send recognition stop message";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    // 等待最终结果（简化处理：使用当前部分文本）
    // 实际应该等待服务器返回最终结果
    final_text = partialRecognitionText;

    // 断开WebSocket连接
    webSocketClient->disconnect();
    isStreamingRecognition = false;
    streamRecognitionId = "";

    ESP_LOGI(TAG, "Stream recognition ended, final text: %s", final_text.c_str());
    return true;
}

bool VolcanoSpeechService::callStreamSynthesisStart(const String &text)
{
    if (!networkManager || !networkManager->isConnected())
    {
        lastError = "Network not available";
        ESP_LOGE(TAG, "Network not available for stream synthesis");
        return false;
    }

    if (config.apiKey.isEmpty())
    {
        lastError = "API credentials not configured";
        ESP_LOGE(TAG, "API key not configured for stream synthesis");
        return false;
    }

    // 连接WebSocket流
    if (!connectWebSocketStream("synthesis"))
    {
        return false;
    }

    // 发送认证消息
    if (!sendWebSocketAuth())
    {
        return false;
    }

    // 发送开始合成消息
    if (!sendWebSocketSynthesisStart(text))
    {
        return false;
    }

    // 清空之前的音频数据
    pendingSynthesisAudio.clear();

    ESP_LOGI(TAG, "Stream synthesis started successfully");
    return true;
}

bool VolcanoSpeechService::callStreamSynthesisGetChunk(std::vector<uint8_t> &chunk, bool &is_last)
{
    if (!isStreamingSynthesis || !webSocketClient)
    {
        lastError = "Stream synthesis not started";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    // 处理WebSocket消息（接收音频数据）
    webSocketClient->loop();

    // 检查是否有可用的音频数据
    if (pendingSynthesisAudio.empty())
    {
        // 没有数据，检查是否还在连接状态
        if (!webSocketClient->isConnected())
        {
            is_last = true;
            isStreamingSynthesis = false;
            ESP_LOGI(TAG, "Stream synthesis ended (disconnected)");
            return true; // 没有数据但流已结束
        }
        return false; // 没有数据，需要重试
    }

    // 返回音频数据（一次返回所有缓冲数据，或分块返回）
    // 这里简化处理：返回所有缓冲数据，并标记为最后一块（假设一次接收完整音频）
    chunk = pendingSynthesisAudio;
    pendingSynthesisAudio.clear();
    is_last = true; // 假设合成音频一次性接收完成

    ESP_LOGI(TAG, "Returning synthesis audio chunk: %u bytes, is_last: %s",
             chunk.size(), is_last ? "yes" : "no");
    return true;
}

// ============================================================================
// 签名生成（火山引擎API签名算法）
// ============================================================================

String VolcanoSpeechService::generateSignature(const String &params, uint64_t timestamp) const
{
    // TODO: 实现火山引擎API签名算法
    // 根据secretKey和参数生成HMAC-SHA256签名

    if (config.secretKey.isEmpty())
    {
        ESP_LOGW(TAG, "No secret key available for signature generation");
        return "";
    }

    // 检查secretKey是否是Access Token（以Bearer Token形式使用）
    // 如果secretKey看起来像JWT或Access Token（包含点或特定格式），则不使用HMAC签名
    if (config.secretKey.indexOf('.') >= 0 || config.secretKey.length() > 50)
    {
        // 可能是Access Token，不是用于HMAC签名的secret key
        ESP_LOGW(TAG, "Secret key appears to be an Access Token, not suitable for HMAC signature");
        return "";
    }

    // 火山引擎API签名算法示例：
    // 1. 构建规范请求字符串
    // 2. 使用secretKey计算HMAC-SHA256
    // 3. 将结果转换为十六进制字符串

    // 由于具体签名算法需要参考API文档，这里提供框架实现
    ESP_LOGW(TAG, "HMAC-SHA256 signature not fully implemented (needs API docs)");

    // 伪代码：
    // String stringToSign = "POST\\n" + RECOGNITION_API + "\\n" + params + "\\n" + String(timestamp);
    // String signature = hmacSha256(stringToSign, config.secretKey);
    // return signature;

    return "mock_signature_placeholder";
}

String VolcanoSpeechService::generateAuthHeader(const String &params) const
{
    // 生成认证头
    // 支持两种认证方式：
    // 1. Bearer Token (使用Access Token)
    // 2. HMAC签名 (使用API Key + Secret Key)

    if (config.apiKey.isEmpty())
    {
        ESP_LOGE(TAG, "No API key/APP ID available for authentication");
        return "";
    }

    // 方法1: 如果secretKey看起来像Access Token，使用Bearer Token
    if (!config.secretKey.isEmpty() &&
        (config.secretKey.indexOf('.') >= 0 || config.secretKey.length() > 20))
    {
        // 可能是JWT或Access Token格式
        ESP_LOGI(TAG, "Using Bearer Token authentication");
        // 火山引擎API使用"Bearer;"格式（带分号）
        return "Bearer;" + config.secretKey;
    }

    // 方法2: 使用HMAC签名认证（火山引擎传统方式）
    if (!config.secretKey.isEmpty())
    {
        ESP_LOGI(TAG, "Using HMAC signature authentication");
        uint64_t timestamp = (uint64_t)time(nullptr) * 1000;
        String signature = generateSignature(params, timestamp);

        if (!signature.isEmpty())
        {
            // 火山引擎HMAC认证头格式
            return "HMAC-SHA256 Credential=" + config.apiKey +
                   ",SignedHeaders=content-type;host;x-content-sha256;x-timestamp" +
                   ",Signature=" + signature;
        }
    }

    // 方法3: 简单的APP ID认证（备用）
    ESP_LOGW(TAG, "Using simple APP ID authentication");
    return "App " + config.apiKey;
}

// ============================================================================
// 日志记录
// ============================================================================

void VolcanoSpeechService::logEvent(const String &event, const String &details) const
{
    if (logger)
    {
        String message = "VolcanoSpeechService: " + event;
        if (!details.isEmpty())
        {
            message += " - " + details;
        }
        logger->log(Logger::Level::INFO, message);
    }
}

// ============================================================================
// 静态工具方法
// ============================================================================

String VolcanoSpeechService::getDefaultConfigJSON()
{
    // 返回默认配置的JSON字符串
    return R"({
        "services": {
            "volcano": {
                "apiKey": "",
                "secretKey": "",
                "region": "cn-north-1",
                "language": "zh-CN",
                "voice": "zh-CN_female_standard",
                "enablePunctuation": true,
                "timeout": 10.0
            }
        }
    })";
}

VolcanoSpeechConfig VolcanoSpeechService::createDefaultConfig()
{
    VolcanoSpeechConfig config;
    config.apiKey = "";
    config.secretKey = "";
    config.region = "cn-north-1";
    config.language = "zh-CN";
    config.voice = "zh-CN_female_standard";
    config.enablePunctuation = true;
    config.timeout = 10.0f;
    config.asyncTimeout = 120.0f;
    return config;
}

// ============================================================================
// WebSocket流式处理方法实现
// ============================================================================

void VolcanoSpeechService::initializeWebSocket()
{
    if (webSocketClient)
    {
        ESP_LOGW(TAG, "WebSocket client already initialized");
        return;
    }

    // 内存监控：记录SSL连接前的空闲堆大小
    ESP_LOGI(TAG, "Free heap before SSL connection: %u bytes", esp_get_free_heap_size());

    webSocketClient = new WebSocketClient();
    webSocketClient->setEventCallback([this](WebSocketEvent event, const String &message, const uint8_t *data, size_t length)
                                      { this->handleWebSocketEvent(event, message, data, length); });

    ESP_LOGI(TAG, "WebSocket client initialized");
}

void VolcanoSpeechService::cleanupWebSocket()
{
    ESP_LOGI(TAG, "cleanupWebSocket() called, webSocketClient=%p", webSocketClient);
    if (webSocketClient)
    {
        // 先断开连接（如果已连接）
        if (webSocketClient->isConnected())
        {
            ESP_LOGI(TAG, "Disconnecting WebSocket before cleanup");
            webSocketClient->disconnect();

            // 关键：给TLS堆栈时间清理内存，避免SSL内存分配失败
            ESP_LOGI(TAG, "Waiting for SSL resource cleanup...");

            // 记录清理前的内存状态（包括SPIRAM）
            ESP_LOGI(TAG, "SSL memory before cleanup - Total: %u, Internal: %u, SPIRAM: %u, Min free: %u",
                     esp_get_free_heap_size(),
                     esp_get_free_internal_heap_size(),
                     heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                     esp_get_minimum_free_heap_size());

            delay(3000);  // 增加到3秒，确保SSL资源完全释放

            // 记录清理后的内存状态（包括SPIRAM）
            ESP_LOGI(TAG, "SSL memory after cleanup - Total: %u, Internal: %u, SPIRAM: %u, Min free: %u",
                     esp_get_free_heap_size(),
                     esp_get_free_internal_heap_size(),
                     heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                     esp_get_minimum_free_heap_size());
        }

        ESP_LOGI(TAG, "Deleting WebSocket client");
        delete webSocketClient;
        webSocketClient = nullptr;
        ESP_LOGI(TAG, "WebSocket client set to nullptr");
    }
    isStreamingRecognition = false;
    isStreamingSynthesis = false;
    streamRecognitionId = "";
    streamSynthesisId = "";
    pendingAudioChunks.clear();
    partialRecognitionText = "";
    pendingSynthesisAudio.clear();
    streamStartTime = 0;

    ESP_LOGI(TAG, "WebSocket client cleaned up");

    // 内存监控：记录SSL清理后的空闲堆大小
    ESP_LOGI(TAG, "Free heap after SSL cleanup: %u bytes", esp_get_free_heap_size());
}

bool VolcanoSpeechService::connectWebSocketStream(const String &streamType)
{
    if (!webSocketClient)
    {
        initializeWebSocket();
    }

    String url;
    if (streamType == "recognition")
    {
        url = STREAM_RECOGNITION_API;
    }
    else if (streamType == "synthesis")
    {
        url = STREAM_SYNTHESIS_API;
    }
    else
    {
        lastError = "Unknown stream type: " + streamType;
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Connecting to WebSocket stream: %s", url.c_str());

    // 根据火山引擎API文档设置WebSocket HTTP头部
    // 需要设置以下头部：
    // X-Api-App-Key: APP ID
    // X-Api-Access-Key: Access Token
    // X-Api-Resource-Id: 资源ID（例如 volc.bigasr.sauc.duration）
    // X-Api-Connect-Id: UUID
    String headers = "";
    if (!config.apiKey.isEmpty())
    {
        headers += "X-Api-App-Key: " + config.apiKey + "\r\n";
    }
    if (!config.secretKey.isEmpty())
    {
        headers += "X-Api-Access-Key: " + config.secretKey + "\r\n";
    }
    else
    {
        // 如果没有Access Token，使用API Key作为备用
        headers += "X-Api-Access-Key: " + config.apiKey + "\r\n";
    }
    // 资源ID：使用种子模型流式语音识别小时版（根据用户实例类型）
    // volc.seedasr.sauc.duration - 种子模型小时版
    // volc.bigasr.sauc.duration - ASR 1.0小时版
    headers += "X-Api-Resource-Id: " + config.resourceId + "\r\n";
    // 生成简单UUID
    String uuid = generateConnectId();
    headers += "X-Api-Connect-Id: " + uuid + "\r\n";
    // 添加协议协商头部，明确要求不压缩的JSON格式
    // 临时注释协议协商头部，以恢复服务器响应
    // headers += "Accept-Encoding: identity\r\n"; // 要求服务器不要压缩响应
    // headers += "Accept: application/json";      // 明确请求JSON格式响应

    ESP_LOGI(TAG, "Setting WebSocket headers for authentication");
    webSocketClient->setExtraHeaders(headers);

    // 使用原始URL（不再需要查询参数）
    String fullUrl = url;

    if (!webSocketClient->connect(fullUrl))
    {
        lastError = "Failed to connect to WebSocket: " + webSocketClient->getLastError();
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    streamStartTime = millis();
    ESP_LOGI(TAG, "WebSocket connected for %s stream", streamType.c_str());
    return true;
}

bool VolcanoSpeechService::sendWebSocketAuth()
{
    if (!webSocketClient || !webSocketClient->isConnected())
    {
        lastError = "WebSocket not connected";
        return false;
    }

    // 构建认证消息（根据火山引擎API文档）
    // 示例：{"type": "auth", "data": {"appid": "xxx", "token": "xxx"}}
    DynamicJsonDocument authDoc(512);
    authDoc["type"] = "auth";

    JsonObject dataObj = authDoc.createNestedObject("data");
    dataObj["appid"] = config.apiKey;
    dataObj["token"] = config.secretKey.isEmpty() ? config.apiKey : config.secretKey;
    dataObj["language"] = config.language;
    dataObj["enable_punctuation"] = config.enablePunctuation;

    String authJson;
    serializeJson(authDoc, authJson);

    if (!webSocketClient->sendTextChunked(authJson, 200))
    {
        lastError = "Failed to send authentication message";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    ESP_LOGI(TAG, "WebSocket authentication sent");
    return true;
}

bool VolcanoSpeechService::sendWebSocketRecognitionStart()
{
    if (!webSocketClient || !webSocketClient->isConnected())
    {
        lastError = "WebSocket not connected";
        return false;
    }

    // 构建开始识别消息
    // 示例：{"type": "start", "data": {"format": "pcm", "rate": 16000, "language": "zh-CN"}}
    DynamicJsonDocument startDoc(256);
    startDoc["type"] = "start";

    JsonObject dataObj = startDoc.createNestedObject("data");
    dataObj["format"] = "pcm";
    dataObj["rate"] = 16000;
    dataObj["language"] = config.language;
    dataObj["enable_punctuation"] = config.enablePunctuation;

    String startJson;
    serializeJson(startDoc, startJson);

    if (!webSocketClient->sendTextChunked(startJson, 128))
    {
        lastError = "Failed to send recognition start message";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    isStreamingRecognition = true;
    streamRecognitionId = String(millis()); // 简单ID生成
    ESP_LOGI(TAG, "Recognition stream started, ID: %s", streamRecognitionId.c_str());
    return true;
}

bool VolcanoSpeechService::sendWebSocketSynthesisStart(const String &text)
{
    if (!webSocketClient || !webSocketClient->isConnected())
    {
        lastError = "WebSocket not connected";
        return false;
    }

    // 构建开始合成消息
    // 示例：{"type": "synthesis_start", "data": {"text": "...", "voice": "...", "rate": 16000}}
    DynamicJsonDocument startDoc(512);
    startDoc["type"] = "synthesis_start";

    JsonObject dataObj = startDoc.createNestedObject("data");
    dataObj["text"] = text;
    dataObj["voice"] = config.voice;
    dataObj["rate"] = 16000;
    dataObj["language"] = config.language;
    dataObj["speed_ratio"] = 1.0;

    String startJson;
    serializeJson(startDoc, startJson);

    if (!webSocketClient->sendTextChunked(startJson, 128))
    {
        lastError = "Failed to send synthesis start message";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    isStreamingSynthesis = true;
    streamSynthesisId = String(millis());
    ESP_LOGI(TAG, "Synthesis stream started, ID: %s, text: %s",
             streamSynthesisId.c_str(), text.substring(0, 50).c_str());
    return true;
}

void VolcanoSpeechService::handleWebSocketEvent(WebSocketEvent event, const String &message, const uint8_t *data, size_t length)
{
    ESP_LOGI(TAG, "WebSocket event received: %s, message length: %u, data length: %u",
             WebSocketClient::eventToString(event).c_str(), message.length(), length);

    switch (event)
    {
    case WebSocketEvent::CONNECTED:
        ESP_LOGI(TAG, "WebSocket connected");
        // 连接后发送认证消息（V2 API使用HTTP头部认证，跳过WebSocket认证）
        if (g_v2APIInProgress)
        {
            ESP_LOGI(TAG, "Skipping WebSocket authentication for V2 API (authentication via HTTP headers)");
            g_v2APIInProgress = false; // 清除标志，认证已完成
        }
        else
        {
            // 对于V3 API，发送WebSocket认证消息
            sendWebSocketAuth();
        }
        break;

    case WebSocketEvent::DISCONNECTED:
        ESP_LOGI(TAG, "WebSocket disconnected");
        isStreamingRecognition = false;
        isStreamingSynthesis = false;
        break;

    case WebSocketEvent::ERROR:
        ESP_LOGE(TAG, "WebSocket error: %s", message.c_str());
        lastError = "WebSocket error: " + message;
        break;

    case WebSocketEvent::TEXT_MESSAGE:
    {
        ESP_LOGI(TAG, "Received TEXT_MESSAGE from server, length: %u bytes", message.length());
        // 诊断：检查消息是否包含非ASCII字符（可能是二进制数据被错误标记为文本）
        bool containsNonAscii = false;
        for (size_t i = 0; i < message.length() && i < 32; i++)
        {
            if (static_cast<uint8_t>(message[i]) > 127)
            {
                containsNonAscii = true;
                break;
            }
        }
        if (containsNonAscii)
        {
            ESP_LOGW(TAG, "Text message contains non-ASCII characters (may be binary/gzip data)");
            // 打印前几个字节的十六进制
            String hexDump = "First 16 bytes hex: ";
            for (size_t i = 0; i < 16 && i < message.length(); i++)
            {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X ", static_cast<uint8_t>(message[i]));
                hexDump += hex;
            }
            ESP_LOGI(TAG, "%s", hexDump.c_str());
        }

        if (!parseWebSocketMessage(message))
        {
            // 如果JSON解析失败，尝试将文本消息作为二进制数据处理
            ESP_LOGW(TAG, "Text message parsing failed, attempting to process as binary data (length: %u)", message.length());

            // 将String转换为std::vector<uint8_t>
            std::vector<uint8_t> binaryData;
            binaryData.reserve(message.length());
            for (size_t i = 0; i < message.length(); i++)
            {
                binaryData.push_back(static_cast<uint8_t>(message[i]));
            }

            // 尝试解码为二进制协议消息
            try
            {
                BinaryProtocolDecoder::DecodedMessage decoded = BinaryProtocolDecoder::decode(binaryData.data(), binaryData.size());
                ESP_LOGI(TAG, "Binary protocol message decoded from text: type=%d, seq=%u, payload=%u bytes",
                         static_cast<int>(decoded.messageType), decoded.sequence, decoded.payloadSize);

                // 根据消息类型处理
                switch (decoded.messageType)
                {
                case BinaryProtocolDecoder::MessageType::FULL_SERVER_RESPONSE:
                    ESP_LOGI(TAG, "Received Full Server Response from text message (sequence: %u)", decoded.sequence);
                    // 根据当前状态选择处理方法
                    if (getAsyncRecognitionInProgress())
                    {
                        handleAsyncBinaryRecognitionResponse(decoded.payload, decoded.sequence);
                    }
                    else
                    {
                        handleBinaryRecognitionResponse(decoded.payload, decoded.sequence);
                    }
                    break;
                case BinaryProtocolDecoder::MessageType::ERROR_MESSAGE:
                    ESP_LOGE(TAG, "Received error message from text message (sequence: %u)", decoded.sequence);
                    // 根据当前状态选择处理方法
                    if (getAsyncRecognitionInProgress())
                    {
                        handleAsyncBinaryErrorMessage(decoded.payload, decoded.sequence);
                    }
                    else
                    {
                        handleBinaryErrorMessage(decoded.payload, decoded.sequence);
                    }
                    break;
                default:
                    ESP_LOGW(TAG, "Unhandled binary message type from text: %d", static_cast<int>(decoded.messageType));
                    break;
                }
            }
            catch (const std::exception &e)
            {
                ESP_LOGE(TAG, "Failed to decode text as binary protocol: %s", e.what());

                // 如果二进制解码也失败，尝试直接作为JSON处理（可能是V2 API的原始JSON响应）
                ESP_LOGI(TAG, "Attempting to process text as raw JSON response");
                // 根据当前状态选择处理方法
                if (getAsyncRecognitionInProgress())
                {
                    handleAsyncBinaryRecognitionResponse(binaryData, 0);
                }
                else
                {
                    handleBinaryRecognitionResponse(binaryData, 0);
                }
            }
        }
    }
    break;

    case WebSocketEvent::BINARY_MESSAGE:
        ESP_LOGI(TAG, "Received BINARY_MESSAGE from server, length: %u bytes", length);
        // 首先尝试解码为二进制协议消息（火山引擎WebSocket协议）
        try
        {
            BinaryProtocolDecoder::DecodedMessage decoded = BinaryProtocolDecoder::decode(data, length);

            ESP_LOGI(TAG, "Binary protocol message decoded: type=%d, seq=%u, payload=%u bytes",
                     static_cast<int>(decoded.messageType), decoded.sequence, decoded.payloadSize);

            // 根据消息类型处理
            switch (decoded.messageType)
            {
            case BinaryProtocolDecoder::MessageType::FULL_SERVER_RESPONSE:
                // 服务器完整响应，包含识别结果
                ESP_LOGI(TAG, "Received Full Server Response (sequence: %u)", decoded.sequence);
                // 根据当前状态选择处理方法
                if (getAsyncRecognitionInProgress())
                {
                    handleAsyncBinaryRecognitionResponse(decoded.payload, decoded.sequence);
                }
                else
                {
                    handleBinaryRecognitionResponse(decoded.payload, decoded.sequence);
                }
                break;

            case BinaryProtocolDecoder::MessageType::ERROR_MESSAGE:
                // 服务器错误消息
                ESP_LOGE(TAG, "Received Error Message from server (sequence: %u)", decoded.sequence);
                // 根据当前状态选择处理方法
                if (getAsyncRecognitionInProgress())
                {
                    handleAsyncBinaryErrorMessage(decoded.payload, decoded.sequence);
                }
                else
                {
                    handleBinaryErrorMessage(decoded.payload, decoded.sequence);
                }
                break;

            default:
                // 其他二进制协议消息类型（如AUDIO_ONLY_REQUEST等，不应从服务器收到）
                ESP_LOGW(TAG, "Unexpected binary protocol message type: %d",
                         static_cast<int>(decoded.messageType));
                // 如果不是协议消息，可能是合成音频数据，继续原有处理
                if (isStreamingSynthesis)
                {
                    handleSynthesisAudio(data, length, false);
                }
                break;
            }
        }
        catch (const std::invalid_argument &e)
        {
            // 解码失败，不是有效的二进制协议消息，可能是合成音频数据
            ESP_LOGW(TAG, "Binary message is not a valid protocol message: %s", e.what());
            // 记录原始字节用于协议分析
            ESP_LOGI(TAG, "Binary message length: %u bytes", length);
            if (length > 0)
            {
                String hexDump = "First 32 bytes: ";
                const size_t dumpSize = length < 32 ? length : 32;
                for (size_t i = 0; i < dumpSize; i++)
                {
                    char hex[4];
                    snprintf(hex, sizeof(hex), "%02X ", data[i]);
                    hexDump += hex;
                }
                ESP_LOGI(TAG, "%s", hexDump.c_str());
            }
            if (isStreamingSynthesis)
            {
                handleSynthesisAudio(data, length, false);
            }
        }
        catch (...)
        {
            // 其他异常，安全处理
            ESP_LOGW(TAG, "Unexpected error decoding binary message");
            if (isStreamingSynthesis)
            {
                handleSynthesisAudio(data, length, false);
            }
        }
        break;

    default:
        // 忽略其他事件
        break;
    }
}

bool VolcanoSpeechService::parseWebSocketMessage(const String &jsonMessage)
{
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, jsonMessage);

    if (error)
    {
        ESP_LOGE(TAG, "Failed to parse WebSocket message: %s", error.c_str());
        return false;
    }

    String type = doc["type"] | "";
    int code = doc["code"] | 0;
    String message = doc["message"] | "";

    if (code != 0 && code != 3000)
    { // 0或3000表示成功
        ESP_LOGE(TAG, "WebSocket API error: code=%d, message=%s", code, message.c_str());
        lastError = "API error " + String(code) + ": " + message;
        return false;
    }

    if (type == "auth_result")
    {
        ESP_LOGI(TAG, "Authentication successful");
        // 认证成功，可以开始识别或合成
    }
    else if (type == "recognition_result")
    {
        String text = doc["data"]["text"] | "";
        bool isFinal = doc["data"]["is_final"] | false;
        handleRecognitionResult(text, isFinal);
    }
    else if (type == "synthesis_result")
    {
        // 合成结果可能是文本消息，指示合成开始或结束
        ESP_LOGI(TAG, "Synthesis result: %s", message.c_str());
    }
    else if (type == "synthesis_audio")
    {
        // 音频数据可能在二进制消息中发送，这里处理元数据
        ESP_LOGI(TAG, "Synthesis audio metadata received");
    }
    else
    {
        ESP_LOGW(TAG, "Unhandled WebSocket message type: %s", type.c_str());
    }

    return true;
}

void VolcanoSpeechService::handleRecognitionResult(const String &text, bool isFinal)
{
    if (isFinal)
    {
        ESP_LOGI(TAG, "Final recognition result: %s", text.c_str());
        partialRecognitionText = text; // 存储最终结果
    }
    else
    {
        ESP_LOGI(TAG, "Partial recognition result: %s", text.c_str());
        partialRecognitionText = text; // 更新部分结果
    }

    // 如果有回调，可以通知调用者
}

void VolcanoSpeechService::handleSynthesisAudio(const uint8_t *data, size_t length, bool isFinal)
{
    // 将音频数据添加到缓冲区
    pendingSynthesisAudio.insert(pendingSynthesisAudio.end(), data, data + length);
    ESP_LOGI(TAG, "Received synthesis audio chunk: %u bytes, total: %u bytes",
             length, pendingSynthesisAudio.size());

    if (isFinal)
    {
        ESP_LOGI(TAG, "Synthesis audio complete, total size: %u bytes", pendingSynthesisAudio.size());
        // 可以开始播放或处理完整音频
    }
}

void VolcanoSpeechService::handleBinaryRecognitionResponse(const std::vector<uint8_t> &payload, uint32_t sequence)
{
    // PSRAM内存监控 - 音频数据处理缓冲区
    if (MemoryUtils::isPSRAMAvailable()) {
        size_t freePSRAM = MemoryUtils::getFreePSRAM();
        size_t largestPSRAM = MemoryUtils::getLargestFreePSRAMBlock();
        ESP_LOGI(TAG, "PSRAM available: free=%u bytes, largest block=%u bytes", freePSRAM, largestPSRAM);

    }

    if (payload.empty())
    {
        ESP_LOGW(TAG, "Empty payload in binary recognition response");
        return;
    }

    ESP_LOGI(TAG, "Processing binary recognition response, payload size: %u bytes", payload.size());
    ESP_LOGI(TAG, "Sequence number: %u (0x%08X)", sequence, sequence);

    // 检查是否为结束标记（根据火山引擎协议，负数sequence表示识别结束）
    if (static_cast<int32_t>(sequence) < 0) {
        ESP_LOGI(TAG, "Received end marker sequence (negative), recognition complete");
    }

    // 尝试将payload解析为JSON（假设payload是JSON字符串）
    // 注意：payload可能包含二进制数据，但根据协议，Full Server Response使用JSON序列化
    String jsonString;
    jsonString.reserve(payload.size() + 1);
    for (uint8_t byte : payload)
    {
        jsonString += static_cast<char>(byte);
    }

    ESP_LOGV(TAG, "Binary recognition response JSON: %s", jsonString.c_str());

    // 解析JSON
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error)
    {
        ESP_LOGE(TAG, "Failed to parse binary recognition response JSON: %s", error.c_str());
        return;
    }

    // 提取识别文本（根据火山引擎API响应格式）
    // 可能格式：{"result": {"text": "...", "utterances": [...]}, "audio_info": {...}}
    String recognizedText;
    if (doc.containsKey("result"))
    {
        JsonObject result = doc["result"];
        if (result.containsKey("text"))
        {
            recognizedText = result["text"].as<String>();
        }
    }
    else if (doc.containsKey("text"))
    {
        recognizedText = doc["text"].as<String>();
    }
    else if (doc.containsKey("data") && doc["data"].is<String>())
    {
        recognizedText = doc["data"].as<String>();
    }

    if (!recognizedText.isEmpty())
    {
        ESP_LOGI(TAG, "Binary recognition result: %s", recognizedText.c_str());
        // 存储结果，供callWebSocketRecognitionAPI使用
        partialRecognitionText = recognizedText;
        // 可以设置标志表示识别完成
        // binaryRecognitionComplete = true;
    }
    else
    {
        ESP_LOGW(TAG, "No recognition text found in binary response");
        // 打印响应内容以便调试
        ESP_LOGV(TAG, "Full response: %s", jsonString.c_str());
    }
}

void VolcanoSpeechService::handleBinaryErrorMessage(const std::vector<uint8_t> &payload, uint32_t sequence)
{
    if (payload.empty())
    {
        ESP_LOGW(TAG, "Empty payload in binary error message");
        return;
    }

    ESP_LOGI(TAG, "Processing binary error message, payload size: %u bytes", payload.size());
    ESP_LOGI(TAG, "Sequence number: %u (0x%08X)", sequence, sequence);

    // 尝试解析错误消息（可能是JSON或纯文本）
    String errorString;
    errorString.reserve(payload.size() + 1);
    for (uint8_t byte : payload)
    {
        errorString += static_cast<char>(byte);
    }

    // 尝试解析为JSON
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, errorString);

    if (!error)
    {
        // JSON格式错误消息
        int code = doc["code"] | -1;
        String message = doc["message"] | "Unknown error";
        String reqid = doc["reqid"] | "";

        ESP_LOGE(TAG, "Binary protocol error: code=%d, message=%s, reqid=%s",
                 code, message.c_str(), reqid.c_str());
        lastError = "Binary protocol error " + String(code) + ": " + message;
    }
    else
    {
        // 纯文本错误消息
        ESP_LOGE(TAG, "Binary protocol error: %s", errorString.c_str());
        lastError = "Binary protocol error: " + errorString;
    }
}

// ============================================================================
// Async recognition API
// ============================================================================

bool VolcanoSpeechService::recognizeAsync(const uint8_t *audio_data, size_t length, RecognitionCallback callback)
{
    // 验证输入
    if (audio_data == nullptr || length == 0)
    {
        ESP_LOGE(TAG, "Async recognition error: Invalid audio data");
        logEvent("async_recognize_error", "Invalid audio data");
        return false;
    }

    // 音频数据诊断 - 检查音频能量和静音
    ESP_LOGI(TAG, "Audio data diagnostic: length=%zu bytes (%.1f seconds at 16kHz 16-bit mono)",
             length, length / 32000.0f); // 32000 = 16000Hz * 2 bytes

    // 计算音频能量（16位PCM）
    int16_t* audioSamples = (int16_t*)audio_data;
    size_t sampleCount = length / 2;
    int64_t sumSquares = 0;
    int16_t maxSample = 0;
    int16_t minSample = 0;
    int32_t zeroCrossings = 0;

    for (size_t i = 0; i < sampleCount; i++) {
        int16_t sample = audioSamples[i];
        sumSquares += (int64_t)sample * sample;
        if (sample > maxSample) maxSample = sample;
        if (sample < minSample) minSample = sample;

        // 计算过零率（仅统计从负到正的过零）
        if (i > 0) {
            int16_t prevSample = audioSamples[i-1];
            if (prevSample < 0 && sample >= 0) {
                zeroCrossings++;
            }
        }
    }

    double rms = (sampleCount > 0) ? sqrt((double)sumSquares / sampleCount) : 0.0;
    double dbFS = (rms > 0) ? 20 * log10(rms / 32768.0) : -100.0; // 相对于满量程
    double zeroCrossingRate = (sampleCount > 0) ? (zeroCrossings * 16000.0) / sampleCount : 0.0;

    ESP_LOGI(TAG, "Audio energy: RMS=%.1f (%.1f dBFS), range=[%d, %d], zero-crossings=%d (%.1f Hz)",
             rms, dbFS, minSample, maxSample, zeroCrossings, zeroCrossingRate);

    // 检查是否可能是静音（RMS < 100，或者动态范围很小）
    bool likelySilence = (rms < 100.0) || (abs(maxSample - minSample) < 100);
    if (likelySilence) {
        ESP_LOGW(TAG, "Audio appears to be silent or very quiet (RMS=%.1f, range=%d)",
                 rms, abs(maxSample - minSample));
    }

    if (!callback)
    {
        ESP_LOGE(TAG, "Async recognition error: Callback is null");
        logEvent("async_recognize_error", "Callback is null");
        return false;
    }

    // 检查服务状态
    if (!isInitialized || !isAvailable())
    {
        ESP_LOGE(TAG, "Async recognition error: Service not ready");
        logEvent("async_recognize_error", "Service not ready");
        return false;
    }

    // 检查是否已有异步识别在进行中
    if (isAsyncRecognitionInProgress())
    {
        ESP_LOGE(TAG, "Async recognition error: Async recognition already in progress");
        logEvent("async_recognize_error", "Async recognition already in progress");
        return false;
    }

    // 设置异步状态并存储回调
    setupAsyncRecognitionState(callback);

    ESP_LOGI(TAG, "Async recognition started: length=%zu, request_id=%u", length, lastAsyncRequestId);
    logEvent("async_recognize_start", "length=" + String(length) + ", request_id=" + String(lastAsyncRequestId));

    // 设置WebSocket连接
    if (!setupWebSocketForAsyncRequest())
    {
        ESP_LOGE(TAG, "Failed to setup WebSocket for async recognition");
        handleAsyncError(ERROR_WEBSOCKET, "Failed to setup WebSocket connection");
        return true; // 返回true表示请求已接受（尽管失败）
    }

    // 使用VolcanoRequestBuilder构建Full Client Request JSON
    String fullClientRequestJson = VolcanoRequestBuilder::buildFullClientRequest(
        "esp32_user",                 // uid
        config.language,              // language
        config.enablePunctuation,     // enablePunctuation
        true,                         // enableITN (逆文本归一化)
        false,                        // enableDDC (数字转换)
        "pcm",                        // format (根据API文档：pcm / wav / ogg / mp3)
        16000,                        // rate
        16,                           // bits
        1,                            // channel
        "raw",                        // codec (same as format)
        config.apiKey,                // appid
        config.secretKey,             // token
        config.resourceId,            // resourceId (added to JSON for server resource validation)
        "volcengine_streaming_common" // cluster (参考代码格式)
    );

    if (fullClientRequestJson.isEmpty())
    {
        ESP_LOGE(TAG, "Failed to build full client request JSON for async recognition");
        handleAsyncError(ERROR_PROTOCOL, "Failed to build request JSON");
        return true;
    }

    ESP_LOGI(TAG, "Full client request JSON size: %u bytes", fullClientRequestJson.length());
    ESP_LOGV(TAG, "Full client request JSON: %s", fullClientRequestJson.c_str());

    // 使用BinaryProtocolEncoder编码Full Client Request
    std::vector<uint8_t> encodedFullRequest = BinaryProtocolEncoder::encodeFullClientRequest(
        fullClientRequestJson,
        config.useCompression, // useCompression
        0                      // sequence字段省略，服务器自动分配
    );

    if (encodedFullRequest.empty())
    {
        ESP_LOGE(TAG, "Failed to encode full client request for async recognition");
        handleAsyncError(ERROR_PROTOCOL, "Failed to encode request");
        return true;
    }

    ESP_LOGI(TAG, "Encoded full client request size: %u bytes", encodedFullRequest.size());

    // 发送Full Client Request - 服务器期望二进制协议格式（根据测试结果调整）
    ESP_LOGI(TAG, "Sending full client request (binary size: %u bytes, JSON size: %u bytes)...",
             encodedFullRequest.size(), fullClientRequestJson.length());

    // 关键：发送前调用loop维持SSL状态
    if (!webSocketClient)
    {
        ESP_LOGE(TAG, "WebSocket client is null, cannot send full client request");
        handleAsyncError(ERROR_WEBSOCKET, "WebSocket client null");
        return true;
    }
    webSocketClient->loop();

    // 首选：尝试二进制协议格式（服务器期望的格式，根据测试结果）
    ESP_LOGI(TAG, "Attempting binary protocol send (server expected format)...");
    if (webSocketClient->sendBinary(encodedFullRequest.data(), encodedFullRequest.size()))
    {
        ESP_LOGI(TAG, "Binary protocol data sent successfully (server expected format)");

        // 关键：发送后立即调用loop维持SSL状态
        webSocketClient->loop();
    }
    else
    {
        // 二进制发送失败，回退到JSON文本格式（API文档格式）
        ESP_LOGW(TAG, "Binary send failed, falling back to JSON text format...");

        // 尝试JSON文本发送
        ESP_LOGI(TAG, "Attempting JSON text send (API doc pattern)...");
        if (webSocketClient->sendText(fullClientRequestJson))
        {
            ESP_LOGI(TAG, "JSON text sent successfully via WebSocket (API doc pattern)");

            // 关键：发送后立即调用loop维持SSL状态
            webSocketClient->loop();
        }
        else
        {
            // JSON文本发送失败，回退到base64文本格式
            ESP_LOGW(TAG, "JSON text send failed, falling back to base64 text format...");

            // 将二进制数据转换为base64编码
            String base64Data = base64Encode(encodedFullRequest.data(), encodedFullRequest.size());
            ESP_LOGI(TAG, "Base64 encoded data length: %u characters", base64Data.length());

            // 构建文本消息格式
            DynamicJsonDocument textDoc(512);
            textDoc["type"] = "binary_data";
            textDoc["data"] = base64Data;
            textDoc["encoding"] = "base64";
            textDoc["size"] = encodedFullRequest.size();

            String textMessage;
            serializeJson(textDoc, textMessage);

            ESP_LOGI(TAG, "Sending base64 text message (length: %u)...", textMessage.length());

            // 再次调用loop维持SSL状态
            webSocketClient->loop();

            if (webSocketClient->sendTextChunked(textMessage, 200))
            {
                ESP_LOGI(TAG, "Base64 encoded data sent successfully via chunked text message (fallback)");

                // 关键：发送后立即调用loop维持SSL状态
                webSocketClient->loop();
            }
            else
            {
                ESP_LOGE(TAG, "Failed to send base64 text message (chunked)");
                handleAsyncError(ERROR_WEBSOCKET, "Failed to send request via WebSocket");
                return true;
            }
        }
    }

    // 火山建议的音频分包策略：6400字节（200ms音频）分包，递增序列号，200ms间隔
    const size_t CHUNK_SIZE = 6400;  // 火山建议的200ms音频包大小
    uint32_t totalChunks = (length + CHUNK_SIZE - 1) / CHUNK_SIZE; // 总块数
    uint32_t sequence = 1;           // 起始序列号（从1递增）
    size_t remaining = length;
    const uint8_t* audioPtr = audio_data;
    bool binarySendSuccessful = true;
    String lastBinaryError = "";

    // 音频音量检测（火山建议的调试步骤）
    if (length >= 4) {
        int16_t* samples = (int16_t*)audio_data;
        size_t sampleCount = length / 2;
        int64_t sum = 0;
        int16_t maxSample = 0;
        int16_t minSample = 0;

        for (size_t i = 0; i < sampleCount && i < 100; i++) { // 检查前100个样本
            int16_t sample = samples[i];
            sum += abs(sample);
            if (sample > maxSample) maxSample = sample;
            if (sample < minSample) minSample = sample;
        }
        int avgAmplitude = (sampleCount > 0) ? (sum / (sampleCount > 100 ? 100 : sampleCount)) : 0;

        // 记录前5个样本的十六进制值用于调试
        String hexSamples = "First 5 samples hex: ";
        for (size_t i = 0; i < 5 && i < sampleCount; i++) {
            char hex[7];
            snprintf(hex, sizeof(hex), "0x%04X ", samples[i] & 0xFFFF);
            hexSamples += hex;
        }

        ESP_LOGI(TAG, "Audio amplitude check: length=%zu, samples=%zu, avg_amplitude=%d (正常说话: 1000-10000)",
                 length, sampleCount, avgAmplitude);
        ESP_LOGI(TAG, "  Sample range: [%d, %d], %s", minSample, maxSample, hexSamples.c_str());

        if (avgAmplitude < 100) {
            ESP_LOGW(TAG, "WARNING: Audio amplitude too low (<100), microphone may not be capturing properly");
        }
    }

    ESP_LOGI(TAG, "Starting audio分包发送: total=%zu bytes, chunk_size=%zu, estimated_chunks=%u",
             length, CHUNK_SIZE, (length + CHUNK_SIZE - 1) / CHUNK_SIZE);

    // 设置状态为音频发送中，防止中间响应过早清理WebSocket
    setAsyncState(STATE_SENDING_AUDIO);

    // 关键：发送前调用loop维持SSL状态
    if (!webSocketClient)
    {
        ESP_LOGE(TAG, "WebSocket client is null, cannot send audio data");
        handleAsyncError(ERROR_WEBSOCKET, "WebSocket client null after response failure");
        return true;
    }
    if (webSocketClient && webSocketClient->isConnected())
    {
        webSocketClient->loop();
    }
    else
    {
        ESP_LOGW(TAG, "WebSocket not connected or null, skipping loop() call");
    }

    // 首选：尝试二进制分包发送（火山建议的分包策略）
    ESP_LOGI(TAG, "Attempting binary chunked send for audio (火山建议分包策略)...");

    while (remaining > 0 && binarySendSuccessful)
    {
        size_t chunkSize = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        bool isLastChunk = (remaining <= CHUNK_SIZE);

        // 计算序列号：从1递增，最后一包为负数（火山客服指导）
        uint32_t seqNum = isLastChunk ?
            static_cast<uint32_t>(static_cast<int32_t>(-totalChunks)) :
            sequence;

        // 编码当前音频分块
        std::vector<uint8_t> encodedChunk = BinaryProtocolEncoder::encodeAudioOnlyRequest(
            audioPtr,
            chunkSize,
            isLastChunk,             // 只有最后一包为true
            config.useCompression,
            seqNum                   // 序列号（从1递增，最后一包为负数）
        );

        if (encodedChunk.empty())
        {
            ESP_LOGE(TAG, "Failed to encode audio chunk %u (size: %zu)", sequence, chunkSize);
            lastBinaryError = "Failed to encode audio chunk " + String(sequence);
            binarySendSuccessful = false;
            break;
        }

        // 发送当前分块 - 添加安全检查和详细日志
        if (!webSocketClient)
        {
            ESP_LOGE(TAG, "WebSocket client is null, cannot send audio chunk %u", sequence);
            lastBinaryError = "WebSocket client null during audio streaming";
            binarySendSuccessful = false;
            break;
        }

        if (!webSocketClient->isConnected())
        {
            ESP_LOGE(TAG, "WebSocket not connected, cannot send audio chunk %u", sequence);
            lastBinaryError = "WebSocket disconnected during audio streaming";
            binarySendSuccessful = false;
            break;
        }

        if (!webSocketClient->sendBinary(encodedChunk.data(), encodedChunk.size()))
        {
            ESP_LOGE(TAG, "Failed to send audio chunk %u", sequence);
            lastBinaryError = "Failed to send audio chunk " + String(sequence);
            binarySendSuccessful = false;
            break;
        }

        ESP_LOGI(TAG, "Audio chunk %u sent: seqNum=%d, size=%zu bytes, last: %s",
                 sequence, static_cast<int32_t>(seqNum), chunkSize, isLastChunk ? "yes" : "no");

        // 更新指针和剩余大小
        audioPtr += chunkSize;
        remaining -= chunkSize;

        // 200ms间隔（最后一包不需要等待）
        if (!isLastChunk)
        {
            sequence++;  // 准备下一个序列号
            if (webSocketClient && webSocketClient->isConnected())
            {
                webSocketClient->loop();  // 维持SSL连接
                delay(200);               // 200ms发包间隔（火山建议）
            }
        }
        // 注意：最后一包的sequence不递增，因为循环结束
    }

    if (binarySendSuccessful)
    {
        ESP_LOGI(TAG, "All audio chunks sent successfully: total=%zu bytes, chunks=%u",
                 length, sequence); // sequence是最后一包的编号

        // 关键：发送后立即调用loop维持SSL状态
        if (webSocketClient && webSocketClient->isConnected())
        {
            webSocketClient->loop();
        }
        else
        {
            ESP_LOGW(TAG, "WebSocket not connected or null, skipping loop() call after audio send");
        }
    }
    else
    {
        // 二进制分包发送失败，回退到base64文本格式（单包）
        ESP_LOGW(TAG, "Audio binary chunked send failed, falling back to base64 text format (single chunk)...");
        ESP_LOGW(TAG, "Error: %s", lastBinaryError.c_str());

        // 编码整个音频为单包（回退方案）
        std::vector<uint8_t> encodedAudioRequest = BinaryProtocolEncoder::encodeAudioOnlyRequest(
            audio_data,
            length,
            true,                  // isLastChunk (这是唯一的音频块)
            config.useCompression, // useCompression
            static_cast<uint32_t>(static_cast<int32_t>(-1)) // 单包序列号为-1
        );

        if (encodedAudioRequest.empty())
        {
            ESP_LOGE(TAG, "Failed to encode audio for base64 fallback");
            handleAsyncError(ERROR_PROTOCOL, "Failed to encode audio for fallback");
            return true;
        }

        // 将二进制数据转换为base64编码
        String audioBase64Data = base64Encode(encodedAudioRequest.data(), encodedAudioRequest.size());
        ESP_LOGI(TAG, "Audio base64 encoded data length: %u characters", audioBase64Data.length());

        // 构建音频文本消息格式
        DynamicJsonDocument audioTextDoc(2048); // 更大的文档以适应音频数据
        audioTextDoc["type"] = "audio_data";
        audioTextDoc["data"] = audioBase64Data;
        audioTextDoc["encoding"] = "base64";
        audioTextDoc["size"] = encodedAudioRequest.size();
        audioTextDoc["original_size"] = length;
        audioTextDoc["is_last_chunk"] = true; // 这是唯一的音频块
        audioTextDoc["sequence"] = 1;         // 序列号，与Full Client Request保持一致

        String audioTextMessage;
        serializeJson(audioTextDoc, audioTextMessage);

        ESP_LOGI(TAG, "Sending audio base64 text message (length: %u)...", audioTextMessage.length());

        // 再次调用loop维持SSL状态
        if (!webSocketClient)
        {
            ESP_LOGE(TAG, "WebSocket client is null, cannot send base64 audio data");
            handleAsyncError(ERROR_WEBSOCKET, "WebSocket client null before fallback send");
            return true;
        }
        webSocketClient->loop();

        if (webSocketClient->sendTextChunked(audioTextMessage, 200))
        {
            ESP_LOGI(TAG, "Audio base64 encoded data sent successfully via chunked text message (fallback)");

            // 关键：发送后立即调用loop维持SSL状态
            webSocketClient->loop();
        }
        else
        {
            ESP_LOGE(TAG, "Failed to send audio base64 text message (chunked)");
            handleAsyncError(ERROR_WEBSOCKET, "Failed to send audio");
            return true;
        }
    }

    // 更新状态为等待响应
    setAsyncState(STATE_WAITING_RESPONSE);
    logEvent("async_request_sent", "Both requests sent, waiting for response");

    return true;
}

// ============================================================================
// Async callback handling
// ============================================================================

void VolcanoSpeechService::invokeAsyncCallback(const AsyncRecognitionResult &result)
{
    if (currentCallback)
    {
        try
        {
            currentCallback(result);
        }
        catch (...)
        {
            ESP_LOGE(TAG, "Exception in async recognition callback");
            logEvent("async_callback_error", "Exception in callback");
        }
    }
    else
    {
        ESP_LOGW(TAG, "Async recognition callback is null");
        logEvent("async_callback_error", "Callback is null");
    }
}

// ============================================================================
// Async recognition state management
// ============================================================================

bool VolcanoSpeechService::isAsyncRecognitionInProgress() const
{
    return getAsyncRecognitionInProgress();
}

void VolcanoSpeechService::setupAsyncRecognitionState(RecognitionCallback callback)
{
    if (lockAsyncState())
    {
        asyncRecognitionInProgress = true;
        asyncState = STATE_CONNECTING;
        asyncRequestStartTime = millis();
        lastAsyncRequestId++;
        asyncRecognitionText = "";
        asyncRecognitionErrorCode = ERROR_NONE;
        asyncRecognitionErrorMessage = "";
        // Initialize retry state
        currentRetryCount = 0;
        nextRetryTimeMs = 0;
        lastRetryableErrorCode = ERROR_NONE;
        // Initialize response timeout detection
        awaitingTextResponse = false;
        lastLogIdTime = 0;
        // Set callback if provided
        if (callback)
        {
            currentCallback = callback;
        }
        unlockAsyncState();
    }
    else
    {
        ESP_LOGE(TAG, "Failed to lock async state for setup");
        // 仍然设置关键状态，但可能有竞争条件
        asyncRecognitionInProgress = true;
        asyncState = STATE_CONNECTING;
        asyncRequestStartTime = millis();
        lastAsyncRequestId++;
        // 在没有锁的情况下设置callback，可能有竞争条件
        if (callback)
        {
            currentCallback = callback;
        }
        // 在没有锁的情况下初始化重试状态
        currentRetryCount = 0;
        nextRetryTimeMs = 0;
        lastRetryableErrorCode = ERROR_NONE;
        // 在没有锁的情况下初始化响应超时检测
        awaitingTextResponse = false;
        lastLogIdTime = 0;
    }
}

void VolcanoSpeechService::cleanupAsyncRecognitionState()
{
    if (lockAsyncState())
    {
        asyncRecognitionInProgress = false;
        asyncState = STATE_IDLE;
        asyncRequestStartTime = 0;
        // Don't reset lastAsyncRequestId - keep for logging
        asyncRecognitionText = "";
        asyncRecognitionErrorCode = ERROR_NONE;
        asyncRecognitionErrorMessage = "";
        // Clear callback
        currentCallback = nullptr;

        // 清除响应超时检测标志
        awaitingTextResponse = false;
        lastLogIdTime = 0;

        unlockAsyncState();
    }
    else
    {
        ESP_LOGE(TAG, "Failed to lock async state for cleanup");
        // 仍然设置关键状态，但可能有竞争条件
        asyncRecognitionInProgress = false;
        asyncState = STATE_IDLE;
    }
}

uint32_t VolcanoSpeechService::getLastAsyncRequestId() const
{
    return lastAsyncRequestId;
}

// ============================================================================
// 线程安全辅助方法
// ============================================================================

bool VolcanoSpeechService::lockAsyncState(int timeoutMs)
{
    if (!asyncStateMutex)
    {
        ESP_LOGE(TAG, "Async state mutex not initialized");
        return false;
    }

    TickType_t timeoutTicks = (timeoutMs == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
    return xSemaphoreTake(asyncStateMutex, timeoutTicks) == pdTRUE;
}

void VolcanoSpeechService::unlockAsyncState()
{
    if (asyncStateMutex)
    {
        xSemaphoreGive(asyncStateMutex);
    }
}

bool VolcanoSpeechService::getAsyncRecognitionInProgress() const
{
    // volatile变量可以直接读取，但为了线程安全，我们使用互斥锁
    if (asyncStateMutex && xSemaphoreTake(asyncStateMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        bool result = asyncRecognitionInProgress;
        xSemaphoreGive(asyncStateMutex);
        return result;
    }
    // 如果无法获取锁，返回当前值（可能不是最新的）
    return asyncRecognitionInProgress;
}

void VolcanoSpeechService::setAsyncRecognitionInProgress(bool value)
{
    if (lockAsyncState())
    {
        asyncRecognitionInProgress = value;
        unlockAsyncState();
    }
    else
    {
        ESP_LOGW(TAG, "Failed to lock async state for setting progress flag");
        asyncRecognitionInProgress = value; // 仍然设置，但可能有竞争条件
    }
}

AsyncRecognitionState VolcanoSpeechService::getAsyncState() const
{
    if (asyncStateMutex && xSemaphoreTake(asyncStateMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        AsyncRecognitionState result = asyncState;
        xSemaphoreGive(asyncStateMutex);
        return result;
    }
    return asyncState;
}

void VolcanoSpeechService::setAsyncState(AsyncRecognitionState state)
{
    if (lockAsyncState())
    {
        asyncState = state;
        unlockAsyncState();
    }
    else
    {
        ESP_LOGW(TAG, "Failed to lock async state for setting state");
        asyncState = state; // 仍然设置，但可能有竞争条件
    }
}

// ============================================================================
// Async WebSocket setup and request handling
// ============================================================================

bool VolcanoSpeechService::setupWebSocketForAsyncRequest()
{
    // 设置V2 API模式标志（用于跳过WebSocket认证消息）
    g_v2APIInProgress = true;

    // 检查网络连接
    if (!networkManager || !networkManager->isConnected())
    {
        ESP_LOGE(TAG, "Network not available for async WebSocket request");
        logEvent("async_ws_setup_error", "Network not available");
        g_v2APIInProgress = false; // 清除标志
        return false;
    }

    // 检查系统时间是否已同步（SSL证书验证需要正确的时间）
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // 详细的时间诊断信息
    ESP_LOGI(TAG, "Time diagnostic - raw time: %lld, year: %d, month: %d, day: %d, hour: %d, min: %d, sec: %d",
             (long long)now, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
             timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // 检查时间是否合理（不在1970年或未来太远）
    bool timeReasonable = true;
    if (timeinfo.tm_year + 1900 < 2020)
    {
        ESP_LOGW(TAG, "System time appears to be unsynchronized (year: %d). SSL certificate validation may fail.",
                 timeinfo.tm_year + 1900);
        logEvent("async_ws_time_warning", "System time not synchronized");
        timeReasonable = false;
        // 继续尝试连接，但SSL可能会失败
    }
    else if (timeinfo.tm_year + 1900 > 2030)
    {
        ESP_LOGW(TAG, "System time appears to be in distant future (year: %d). SSL certificate validation may fail.",
                 timeinfo.tm_year + 1900);
        logEvent("async_ws_time_warning", "System time in future");
        timeReasonable = false;
    }
    else
    {
        ESP_LOGI(TAG, "System time synchronized (year: %d, date: %04d-%02d-%02d %02d:%02d:%02d)",
                 timeinfo.tm_year + 1900, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                 timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }

    // 记录时间合理性状态
    if (!timeReasonable)
    {
        logEvent("async_ws_time_check", String("time_unreasonable_year_") + String(timeinfo.tm_year + 1900));
    }

    // 检查API凭证
    if (config.apiKey.isEmpty())
    {
        ESP_LOGE(TAG, "API credentials not configured for async WebSocket request");
        logEvent("async_ws_setup_error", "API credentials not configured");
        g_v2APIInProgress = false; // 清除标志
        return false;
    }

    // 初始化WebSocket客户端（如果尚未初始化）
    if (!webSocketClient)
    {
        initializeWebSocket();
    }

    // 设置WebSocket头部 - 使用火山引擎V3 API的X-Api-*头部格式
    String resourceId = config.resourceId.isEmpty() ? "volc.bigasr.sauc.duration" : config.resourceId;
    String connectId = generateConnectId();

    // 构建头部：使用火山引擎V3 API的X-Api-*头部格式
    // X-Api-App-Key: APP ID, X-Api-Access-Key: Access Token, X-Api-Resource-Id: 资源ID
    // 注意：标准WebSocket头部（Upgrade, Connection, Sec-WebSocket-Version, Sec-WebSocket-Key）由WebSocket库自动添加
    String headers = "";
    // 火山引擎V3 API特定头部
    if (!config.apiKey.isEmpty())
    {
        headers += "X-Api-App-Key: " + config.apiKey + "\r\n";
    }
    if (!config.secretKey.isEmpty())
    {
        headers += "X-Api-Access-Key: " + config.secretKey + "\r\n";
    }
    else
    {
        // 如果没有Access Token，使用API Key作为备用
        headers += "X-Api-Access-Key: " + config.apiKey + "\r\n";
    }
    headers += "X-Api-Resource-Id: " + resourceId + "\r\n";
    headers += "X-Api-Connect-Id: " + connectId + "\r\n";
    headers += "X-Api-Sequence: -1\r\n";
    headers += "X-Api-Request-Id: " + connectId;

    ESP_LOGI(TAG, "Setting up WebSocket for V3 async request to: %s", STREAM_RECOGNITION_API);
    ESP_LOGI(TAG, "Using X-Api-* headers (V3 API format) - WebSocket standard headers auto-added by library:");
    ESP_LOGI(TAG, "  X-Api-App-Key: %s", config.apiKey.c_str());
    ESP_LOGI(TAG, "  X-Api-Access-Key: ... (length: %u)", config.secretKey.length());
    ESP_LOGI(TAG, "  X-Api-Resource-Id: %s", resourceId.c_str());
    ESP_LOGI(TAG, "  X-Api-Connect-Id: %s", connectId.c_str());
    ESP_LOGI(TAG, "  X-Api-Sequence: -1");
    ESP_LOGI(TAG, "  X-Api-Request-Id: %s", connectId.c_str());
    logEvent("async_ws_setup_v3", "url=" + String(STREAM_RECOGNITION_API) + ", headers_xapi");

    webSocketClient->setExtraHeaders(headers);

    // 连接到V3识别API（不设置子协议，火山引擎V3流式API）
    if (!webSocketClient->connect(STREAM_RECOGNITION_API, ""))
    {
        String error = webSocketClient->getLastError();
        ESP_LOGE(TAG, "Failed to connect to WebSocket for async request: %s", error.c_str());
        logEvent("async_ws_connect_failed", "error=" + error);
        g_v2APIInProgress = false; // 清除标志
        return false;
    }

    // 更新状态
    setAsyncState(STATE_CONNECTING);

    // 等待连接建立 - 使用高频loop调用（参考代码模式）
    unsigned int maxWaitMs = 10000; // 增加到10秒超时
    unsigned int maxRetries = 3;    // 最大重试次数
    bool connected = false;

    for (int retry = 0; retry < maxRetries && !connected; retry++) {
        if (retry > 0) {
            ESP_LOGI(TAG, "WebSocket connection retry %d/%d", retry, maxRetries);
            webSocketClient->disconnect();
            delay(2000); // 重试前等待 - 增加延迟以允许SSL内存清理

            // 重新连接
            if (!webSocketClient->connect(STREAM_RECOGNITION_API, "")) {
                String error = webSocketClient->getLastError();
                ESP_LOGE(TAG, "Failed to reconnect on retry %d: %s", retry, error.c_str());
                logEvent("async_ws_retry_failed", "retry=" + String(retry) + ", error=" + error);
                continue; // 继续下一次重试
            }
        }

        ESP_LOGI(TAG, "Waiting for WebSocket connection... (attempt %d)", retry + 1);
        unsigned int startTime = millis();
        unsigned int loopCount = 0;

        while (!webSocketClient->isConnected() && (millis() - startTime < maxWaitMs))
        {
            webSocketClient->loop(); // 关键：高频调用维持SSL状态
            loopCount++;
            if (loopCount % 100 == 0)
            {
                ESP_LOGI(TAG, "Still waiting for connection, elapsed: %d ms", millis() - startTime);
            }
            delay(1); // 短暂延迟，减少等待时间
        }

        // 检查连接状态
        if (webSocketClient->isConnected()) {
            connected = true;
            ESP_LOGI(TAG, "WebSocket connection established after %d ms, loop called %d times (attempt %d)", millis() - startTime, loopCount, retry + 1);
            break;
        } else {
            ESP_LOGW(TAG, "WebSocket connection attempt %d failed after %d ms (max wait: %d ms)", retry + 1, millis() - startTime, maxWaitMs);
            logEvent("async_ws_attempt_failed", "attempt=" + String(retry + 1) + ", elapsed=" + String(millis() - startTime));
        }
    }

    // 检查连接状态
    if (!connected)
    {
        ESP_LOGE(TAG, "WebSocket connection failed after %d retries", maxRetries);
        logEvent("async_ws_connect_failed", "All retries exhausted");
        g_v2APIInProgress = false; // 清除标志
        return false;
    }

    // 连接成功，更新状态
    setAsyncState(STATE_CONNECTED);
    ESP_LOGI(TAG, "WebSocket connected successfully, state updated to CONNECTED");

    // 对于V3 API，不需要发送额外的认证消息（认证通过HTTP头部完成）
    // 直接更新状态为已连接
    ESP_LOGI(TAG, "V3 WebSocket connected successfully, authentication via X-Api headers");
    logEvent("async_ws_v3_connected", "V3 WebSocket connected via X-Api headers");

    ESP_LOGI(TAG, "WebSocket setup for async request completed successfully");
    logEvent("async_ws_setup_complete", "WebSocket ready for async request");

    // 确保V2 API标志被清除（如果事件处理程序未调用）
    if (g_v2APIInProgress)
    {
        ESP_LOGW(TAG, "V2 API flag still set at end of setup, clearing");
        g_v2APIInProgress = false;
    }

    return true;
}

// ============================================================================
// GZIP compression handling utilities
// ============================================================================

/**
 * @brief Check if data appears to be gzip compressed
 * @param data Pointer to data buffer
 * @param length Size of data in bytes
 * @return true if data starts with gzip magic bytes (0x1F 0x8B)
 */
static bool isGzipCompressed(const uint8_t *data, size_t length)
{
    return length >= 2 && data[0] == 0x1F && data[1] == 0x8B;
}

/**
 * @brief Attempt to decompress gzip data (stub implementation)
 * @param compressed Compressed input data
 * @param decompressed Output buffer for decompressed data
 * @return true if decompression successful, false otherwise
 *
 * @note This is a stub implementation. In production, this should use
 *       zlib or another gzip decompression library.
 */
static bool decompressGzip(const std::vector<uint8_t> &compressed, std::vector<uint8_t> &decompressed)
{
    ESP_LOGI("GZIP", "Attempting gzip decompression of %u bytes", compressed.size());

    // Check if it's actually gzip
    if (!isGzipCompressed(compressed.data(), compressed.size()))
    {
        ESP_LOGE("GZIP", "Data does not appear to be gzip compressed");
        return false;
    }

#ifdef HAS_ZLIB
    // Use zlib for gzip decompression
    z_stream stream;
    memset(&stream, 0, sizeof(stream));

    // Initialize for gzip format (16 + MAX_WBITS)
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK)
    {
        ESP_LOGE("GZIP", "Failed to initialize zlib inflate");
        return false;
    }

    stream.next_in = const_cast<uint8_t *>(compressed.data());
    stream.avail_in = compressed.size();

    // Decompress in chunks
    const size_t CHUNK_SIZE = 4096;
    uint8_t out[CHUNK_SIZE];
    int ret;

    do
    {
        stream.next_out = out;
        stream.avail_out = CHUNK_SIZE;

        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR)
        {
            ESP_LOGE("GZIP", "zlib inflate error: %d, msg: %s", ret, stream.msg ? stream.msg : "no message");
            inflateEnd(&stream);
            return false;
        }

        size_t have = CHUNK_SIZE - stream.avail_out;
        decompressed.insert(decompressed.end(), out, out + have);
    } while (stream.avail_out == 0);

    inflateEnd(&stream);

    ESP_LOGI("GZIP", "GZIP decompression successful: %u -> %u bytes",
             compressed.size(), decompressed.size());
    return true;
#else
    // zlib not available
    ESP_LOGW("GZIP", "zlib not available for gzip decompression (HAS_ZLIB not defined)");
    ESP_LOGW("GZIP", "Compressed data starts with: 0x%02X 0x%02X",
             compressed[0], compressed[1]);

    // Provide more detailed diagnostics
    if (compressed.size() > 10)
    {
        ESP_LOGI("GZIP", "First 10 bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                 compressed[0], compressed[1], compressed[2], compressed[3], compressed[4],
                 compressed[5], compressed[6], compressed[7], compressed[8], compressed[9]);
    }

    return false;
#endif
}

// ============================================================================
// Async binary recognition response handling
// ============================================================================

void VolcanoSpeechService::handleAsyncBinaryRecognitionResponse(const std::vector<uint8_t> &payload, uint32_t sequence)
{
    if (payload.empty())
    {
        ESP_LOGW(TAG, "Empty payload in async binary recognition response");
        return;
    }

    ESP_LOGI(TAG, "Processing async binary recognition response, payload size: %u bytes", payload.size());
    ESP_LOGI(TAG, "Sequence number: %u (0x%08X)", sequence, sequence);

    // 检查是否为结束标记（根据火山引擎协议，负数sequence表示识别结束）
    if (static_cast<int32_t>(sequence) < 0) {
        ESP_LOGI(TAG, "Received end marker sequence (negative), recognition should be complete");
        // 这里可以添加结束处理逻辑，但当前方法已经处理最终结果
    }

    // 尝试将payload解析为JSON
    String jsonString;
    jsonString.reserve(payload.size() + 1);
    for (uint8_t byte : payload)
    {
        jsonString += static_cast<char>(byte);
    }

    ESP_LOGV(TAG, "Async binary recognition response JSON: %s", jsonString.c_str());

    // 解析JSON
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error)
    {
        ESP_LOGE(TAG, "Failed to parse async binary recognition response JSON: %s", error.c_str());

        // 添加详细的响应数据诊断
        ESP_LOGI(TAG, "Response payload size: %u bytes", payload.size());

        // 检测是否是gzip压缩数据
        bool isGzip = (payload.size() >= 2 && payload[0] == 0x1F && payload[1] == 0x8B);
        if (isGzip)
        {
            ESP_LOGI(TAG, "Payload appears to be gzip compressed (magic bytes: 0x%02X 0x%02X)",
                     payload[0], payload[1]);

            // 尝试解压gzip数据
            ESP_LOGI(TAG, "Attempting gzip decompression...");
            std::vector<uint8_t> decompressed;
            if (decompressGzip(payload, decompressed) && !decompressed.empty())
            {
                ESP_LOGI(TAG, "Gzip decompression successful: %u -> %u bytes",
                         payload.size(), decompressed.size());

                // 尝试解析解压后的数据
                String decompressedJson;
                decompressedJson.reserve(decompressed.size() + 1);
                for (uint8_t byte : decompressed)
                {
                    decompressedJson += static_cast<char>(byte);
                }

                DynamicJsonDocument decompressedDoc(2048);
                DeserializationError decompressedError = deserializeJson(decompressedDoc, decompressedJson);

                if (!decompressedError)
                {
                    ESP_LOGI(TAG, "Successfully parsed decompressed JSON response");
                    // 成功解析，继续处理解压后的文档
                    doc = decompressedDoc;
                    error = DeserializationError::Ok;
                    goto process_response; // 跳转到响应处理部分
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to parse decompressed JSON: %s", decompressedError.c_str());
                }
            }
            else
            {
                ESP_LOGW(TAG, "Gzip decompression failed or not available");
            }
        }

        // 尝试跳过可能的二进制协议头部
        ESP_LOGI(TAG, "Attempting to skip binary protocol header...");
        size_t jsonStart = 0;
        const size_t maxHeaderSkip = 32; // 最大跳过字节数

        for (size_t i = 0; i < maxHeaderSkip && i < payload.size(); i++)
        {
            if (payload[i] == '{' || payload[i] == '[')
            {
                jsonStart = i;
                ESP_LOGI(TAG, "Found JSON start at offset %u (char: 0x%02X '%c')",
                         jsonStart, payload[i], static_cast<char>(payload[i]));
                break;
            }
        }

        if (jsonStart > 0)
        {
            ESP_LOGI(TAG, "Skipping %u bytes of binary header", jsonStart);

            // 从jsonStart位置创建新的JSON字符串
            String jsonStringWithHeader;
            jsonStringWithHeader.reserve(payload.size() - jsonStart + 1);
            for (size_t i = jsonStart; i < payload.size(); i++)
            {
                jsonStringWithHeader += static_cast<char>(payload[i]);
            }

            DynamicJsonDocument docWithHeader(2048);
            DeserializationError headerError = deserializeJson(docWithHeader, jsonStringWithHeader);

            if (!headerError)
            {
                ESP_LOGI(TAG, "Successfully parsed JSON after skipping %u bytes of header", jsonStart);
                doc = docWithHeader;
                error = DeserializationError::Ok;
                goto process_response;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to parse JSON even after skipping %u bytes: %s",
                         jsonStart, headerError.c_str());
            }
        }
        else
        {
            ESP_LOGI(TAG, "No JSON start character found in first %u bytes", maxHeaderSkip);
        }

        // 打印前16字节的十六进制用于调试
        if (payload.size() > 0)
        {
            String hexDump = "First 16 bytes: ";
            for (size_t i = 0; i < 16 && i < payload.size(); i++)
            {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X ", payload[i]);
                hexDump += hex;
            }
            ESP_LOGI(TAG, "%s", hexDump.c_str());
        }

        // 检查是否是有效的UTF-8
        bool isPrintable = true;
        for (size_t i = 0; i < payload.size() && i < 32; i++)
        {
            if (payload[i] < 32 && payload[i] != '\n' && payload[i] != '\r' && payload[i] != '\t')
            {
                isPrintable = false;
                break;
            }
        }
        ESP_LOGI(TAG, "Payload contains printable characters: %s", isPrintable ? "yes" : "no");

        String errorMsg = "Failed to parse response JSON";
        if (isGzip)
        {
            errorMsg += " (data appears to be gzip compressed)";
        }

        AsyncRecognitionResult result(false, "", ERROR_PROTOCOL, errorMsg);
        invokeAsyncCallback(result);
        cleanupAsyncRecognitionState();
        cleanupWebSocket();
        return;
    }

process_response:

    // 详细记录服务器响应结构用于诊断
    ESP_LOGI(TAG, "=== SERVER RESPONSE DIAGNOSTICS ===");
    ESP_LOGI(TAG, "Payload size: %u bytes", payload.size());

    // 记录JSON键以便理解响应结构
    String jsonKeys = "JSON keys: ";
    for (JsonPair kv : doc.as<JsonObject>()) {
        jsonKeys += String(kv.key().c_str()) + " ";
    }
    ESP_LOGI(TAG, "%s", jsonKeys.c_str());

    // 如果存在result键，检查其结构
    if (doc.containsKey("result")) {
        JsonObject resultObj = doc["result"];
        String resultKeys = "result keys: ";
        for (JsonPair kv : resultObj) {
            resultKeys += String(kv.key().c_str()) + " ";
        }
        ESP_LOGI(TAG, "%s", resultKeys.c_str());

        // 检查是否有additions键
        if (resultObj.containsKey("additions")) {
            ESP_LOGI(TAG, "result.additions exists");
            JsonObject additionsObj = resultObj["additions"];
            String additionsKeys = "additions keys: ";
            for (JsonPair kv : additionsObj) {
                additionsKeys += String(kv.key().c_str()) + " ";
            }
            ESP_LOGI(TAG, "%s", additionsKeys.c_str());
        }
    }

    // 如果存在data键，检查其结构
    if (doc.containsKey("data")) {
        if (doc["data"].is<JsonObject>()) {
            JsonObject dataObj = doc["data"];
            String dataKeys = "data keys: ";
            for (JsonPair kv : dataObj) {
                dataKeys += String(kv.key().c_str()) + " ";
            }
            ESP_LOGI(TAG, "%s", dataKeys.c_str());
        }
    }

    ESP_LOGI(TAG, "=== END DIAGNOSTICS ===");

    // 获取当前状态以决定是否处理中间响应
    AsyncRecognitionState currentState = getAsyncState();

    // 检查响应代码
    int code = doc["code"] | -1;
    String message = doc["message"] | "";
    String reqid = doc["reqid"] | "";

    if (code == 0 || (code == -1 && doc.containsKey("result")))
    {
        // 成功响应
        String text = "";

        // 尝试V3 API格式: data.text
        if (doc.containsKey("data") && doc["data"].is<JsonObject>())
        {
            text = doc["data"]["text"] | "";
        }
        // 尝试V2 API格式: result.text
        else if (doc.containsKey("result") && doc["result"].is<JsonObject>())
        {
            text = doc["result"]["text"] | "";
        }
        // 尝试直接text字段
        else if (doc.containsKey("text"))
        {
            text = doc["text"].as<String>();
        }

        if (text.isEmpty())
        {
            ESP_LOGW(TAG, "Async recognition response missing text field");
            text = "";
        }

        // 检查是否为流式识别的中间确认（只有log_id，没有text）
        bool isIntermediateConfirmation = text.isEmpty() &&
                                         doc.containsKey("result") &&
                                         doc["result"].is<JsonObject>() &&
                                         doc["result"].containsKey("additions") &&
                                         doc["result"]["additions"].is<JsonObject>() &&
                                         doc["result"]["additions"].containsKey("log_id");

        // 如果当前正在发送音频且收到中间确认，不回调不清理，等待后续响应
        if (currentState == STATE_SENDING_AUDIO && isIntermediateConfirmation)
        {
            ESP_LOGI(TAG, "Received intermediate confirmation (log_id only) during audio streaming, waiting for final response...");
            logEvent("async_intermediate_confirmation", "log_id=" + doc["result"]["additions"]["log_id"].as<String>());

            // 设置响应超时检测标志
            awaitingTextResponse = true;
            lastLogIdTime = millis();
            ESP_LOGI(TAG, "Set awaitingTextResponse=true, lastLogIdTime=%u, responseTimeout=%ums",
                     lastLogIdTime, responseTimeoutMs);

            return; // 重要：直接返回，不调用回调，不清理WebSocket
        }

        // 检查是否为流式识别的部分结果（有text但is_final为false）
        bool isFinal = false;
        // 检查可能的is_final字段位置
        if (doc.containsKey("data") && doc["data"].is<JsonObject>() && doc["data"].containsKey("is_final"))
        {
            isFinal = doc["data"]["is_final"].as<bool>();
        }
        else if (doc.containsKey("result") && doc["result"].is<JsonObject>() && doc["result"].containsKey("is_final"))
        {
            isFinal = doc["result"]["is_final"].as<bool>();
        }
        else if (doc.containsKey("is_final"))
        {
            isFinal = doc["is_final"].as<bool>();
        }
        // 检查definite字段（某些API版本使用definite）
        else if (doc.containsKey("result") && doc["result"].is<JsonObject>() &&
                 doc["result"].containsKey("utterances") && doc["result"]["utterances"].is<JsonArray>() &&
                 doc["result"]["utterances"].size() > 0)
        {
            JsonArray utterances = doc["result"]["utterances"];
            // 检查第一个utterance是否有definite字段
            if (utterances[0].containsKey("definite"))
            {
                isFinal = utterances[0]["definite"].as<bool>();
            }
        }

        // 如果是部分结果（有text但is_final为false），不回调不清理，等待最终结果
        if (!text.isEmpty() && !isFinal)
        {
            ESP_LOGI(TAG, "Received partial recognition result: text='%s', waiting for final result...", text.c_str());
            logEvent("async_partial_result", "text=" + text);
            return; // 直接返回，不调用回调，不清理WebSocket
        }

        ESP_LOGI(TAG, "Async recognition successful: text='%s', reqid=%s, is_final=%s", text.c_str(), reqid.c_str(), isFinal ? "true" : "false");
        logEvent("async_recognition_success", "text=" + text + ", reqid=" + reqid + ", is_final=" + String(isFinal ? "true" : "false"));

        // 清除响应超时检测标志
        if (awaitingTextResponse) {
            ESP_LOGI(TAG, "Clearing awaitingTextResponse flag after receiving text");
            awaitingTextResponse = false;
            lastLogIdTime = 0;
        }

        // 调用回调
        AsyncRecognitionResult result(true, text, ERROR_NONE, "Success");
        invokeAsyncCallback(result);
    }
    else
    {
        // 错误响应
        ESP_LOGE(TAG, "Async recognition failed: code=%d, message=%s, reqid=%s",
                 code, message.c_str(), reqid.c_str());
        logEvent("async_recognition_error", "code=" + String(code) + ", message=" + message + ", reqid=" + reqid);

        // 映射错误码
        int errorCode = ERROR_SERVER;
        if (code == 1001)
            errorCode = ERROR_AUTHENTICATION;
        else if (code == 1002)
            errorCode = ERROR_NETWORK;
        else if (code == 1003)
            errorCode = ERROR_PROTOCOL;

        // 清除响应超时检测标志
        if (awaitingTextResponse) {
            ESP_LOGI(TAG, "Clearing awaitingTextResponse flag due to server error");
            awaitingTextResponse = false;
            lastLogIdTime = 0;
        }

        AsyncRecognitionResult result(false, "", errorCode, "Server error: " + message);
        invokeAsyncCallback(result);
    }

    // 清理状态
    cleanupAsyncRecognitionState();
    cleanupWebSocket();
}

void VolcanoSpeechService::handleAsyncBinaryErrorMessage(const std::vector<uint8_t> &payload, uint32_t sequence)
{
    if (payload.empty())
    {
        ESP_LOGW(TAG, "Empty payload in async binary error message");
        return;
    }

    ESP_LOGI(TAG, "Processing async binary error message, payload size: %u bytes", payload.size());
    ESP_LOGI(TAG, "Sequence number: %u (0x%08X)", sequence, sequence);

    // Check if payload follows binary error message format: errorCode (4 bytes) + messageLength (4 bytes) + message
    if (payload.size() >= 8)
    {
        // Parse error code (4 bytes, big-endian)
        uint32_t binaryErrorCode = (static_cast<uint32_t>(payload[0]) << 24) |
                                   (static_cast<uint32_t>(payload[1]) << 16) |
                                   (static_cast<uint32_t>(payload[2]) << 8) |
                                   static_cast<uint32_t>(payload[3]);

        // Parse message length (4 bytes, big-endian)
        uint32_t messageLength = (static_cast<uint32_t>(payload[4]) << 24) |
                                 (static_cast<uint32_t>(payload[5]) << 16) |
                                 (static_cast<uint32_t>(payload[6]) << 8) |
                                 static_cast<uint32_t>(payload[7]);

        ESP_LOGI(TAG, "Binary error message format detected: errorCode=%u, messageLength=%u", binaryErrorCode, messageLength);

        // Validate message length
        if (messageLength > 0 && payload.size() >= 8 + messageLength)
        {
            // Extract message string starting from byte 8
            String messageString;
            messageString.reserve(messageLength + 1);
            for (size_t i = 0; i < messageLength; i++)
            {
                messageString += static_cast<char>(payload[8 + i]);
            }

            ESP_LOGV(TAG, "Binary error message content: %s", messageString.c_str());

            // Try to parse as JSON
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, messageString);

            if (!error)
            {
                // JSON error message
                int code = doc["code"] | -1;
                String message = doc["message"] | "Unknown error";
                String reqid = doc["reqid"] | "";

                ESP_LOGE(TAG, "Async binary protocol error (JSON): binaryErrorCode=%u, code=%d, message=%s, reqid=%s",
                         binaryErrorCode, code, message.c_str(), reqid.c_str());
                logEvent("async_binary_error", "binaryErrorCode=" + String(binaryErrorCode) + ", code=" + String(code) + ", message=" + message + ", reqid=" + reqid);

                // Map error code
                int errorCode = ERROR_PROTOCOL;
                if (code == 1001)
                    errorCode = ERROR_AUTHENTICATION;
                else if (code == 1002)
                    errorCode = ERROR_NETWORK;

                AsyncRecognitionResult result(false, "", errorCode, "Binary protocol error " + String(code) + ": " + message);
                invokeAsyncCallback(result);
            }
            else
            {
                // Plain text error message
                ESP_LOGE(TAG, "Async binary protocol error (text): binaryErrorCode=%u, message=%s",
                         binaryErrorCode, messageString.c_str());
                logEvent("async_binary_error_text", "binaryErrorCode=" + String(binaryErrorCode) + ", message=" + messageString);

                AsyncRecognitionResult result(false, "", ERROR_PROTOCOL, "Binary protocol error " + String(binaryErrorCode) + ": " + messageString);
                invokeAsyncCallback(result);
            }

            // Cleanup
            cleanupAsyncRecognitionState();
            cleanupWebSocket();
            return;
        }
    }

    // Fallback: try to parse entire payload as JSON (legacy format)
    String jsonString;
    jsonString.reserve(payload.size() + 1);
    for (uint8_t byte : payload)
    {
        jsonString += static_cast<char>(byte);
    }

    ESP_LOGV(TAG, "Async binary error message JSON (fallback): %s", jsonString.c_str());

    // Parse JSON
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error)
    {
        // If not JSON, treat as plain text error message
        ESP_LOGE(TAG, "Async binary protocol error (fallback): %s", jsonString.c_str());
        AsyncRecognitionResult result(false, "", ERROR_PROTOCOL, "Binary protocol error: " + jsonString);
        invokeAsyncCallback(result);
    }
    else
    {
        // JSON error message
        int code = doc["code"] | -1;
        String message = doc["message"] | "Unknown error";
        String reqid = doc["reqid"] | "";

        ESP_LOGE(TAG, "Async binary protocol error (JSON fallback): code=%d, message=%s, reqid=%s",
                 code, message.c_str(), reqid.c_str());
        logEvent("async_binary_error", "code=" + String(code) + ", message=" + message + ", reqid=" + reqid);

        // Map error code
        int errorCode = ERROR_PROTOCOL;
        if (code == 1001)
            errorCode = ERROR_AUTHENTICATION;
        else if (code == 1002)
            errorCode = ERROR_NETWORK;

        AsyncRecognitionResult result(false, "", errorCode, "Binary protocol error " + String(code) + ": " + message);
        invokeAsyncCallback(result);
    }

    // Cleanup
    cleanupAsyncRecognitionState();
    cleanupWebSocket();
}

void VolcanoSpeechService::cancelAsyncRecognition()
{
    if (!getAsyncRecognitionInProgress())
    {
        ESP_LOGW(TAG, "No async recognition in progress to cancel");
        return;
    }

    ESP_LOGI(TAG, "Canceling async recognition (request_id=%u)", lastAsyncRequestId);
    logEvent("async_cancel", "Canceling async recognition, request_id=" + String(lastAsyncRequestId));

    // 发送取消回调
    AsyncRecognitionResult result(false, "", ERROR_INVALID_STATE, "Request cancelled");
    invokeAsyncCallback(result);

    // 清理状态
    cleanupAsyncRecognitionState();
    cleanupWebSocket();

    ESP_LOGI(TAG, "Async recognition cancelled successfully");
    logEvent("async_cancel_complete", "Async recognition cancelled");
}

// ============================================================================
// Async timeout checking
// ============================================================================

void VolcanoSpeechService::checkAsyncTimeout()
{
    // 检查是否有异步识别在进行中
    if (!getAsyncRecognitionInProgress())
    {
        return; // 没有进行中的异步识别
    }

    // 首先检查响应超时（无论当前状态如何）
    if (awaitingTextResponse && lastLogIdTime > 0)
    {
        uint32_t currentTime = millis();
        uint32_t responseElapsed = currentTime - lastLogIdTime;
        if (responseElapsed >= responseTimeoutMs)
        {
            ESP_LOGW(TAG, "Response timeout after receiving log_id: elapsed=%ums, timeout=%ums, request_id=%u",
                     responseElapsed, responseTimeoutMs, lastAsyncRequestId);
            logEvent("response_timeout", "elapsed=" + String(responseElapsed) + "ms, timeout=" + String(responseTimeoutMs) + "ms");

            // 尝试发送心跳包或重试（TODO：实现心跳机制）
            // 暂时标记为错误，触发清理
            awaitingTextResponse = false;
            lastLogIdTime = 0;

            // 发送服务器错误回调
            AsyncRecognitionResult result(false, "", ERROR_SERVER,
                                          "Server response timeout after " + String(responseElapsed) + "ms");
            invokeAsyncCallback(result);

            // 清理状态
            cleanupAsyncRecognitionState();
            cleanupWebSocket();
            return;
        }
    }

    // 获取当前状态
    AsyncRecognitionState currentState = getAsyncState();

    // 只有在等待响应时才检查超时
    if (currentState != STATE_WAITING_RESPONSE)
    {
        return;
    }

    // 计算经过的时间
    uint32_t currentTime = millis();
    uint32_t elapsedTime = currentTime - asyncRequestStartTime;
    uint32_t timeoutMs = getAsyncRequestTimeout();

    // 检查是否超时
    if (elapsedTime >= timeoutMs)
    {
        ESP_LOGW(TAG, "Async recognition timeout: elapsed=%ums, timeout=%ums, request_id=%u",
                 elapsedTime, timeoutMs, lastAsyncRequestId);
        logEvent("async_timeout",
                 "elapsed=" + String(elapsedTime) + "ms, timeout=" + String(timeoutMs) + "ms, request_id=" + String(lastAsyncRequestId));

        // 更新状态为超时
        setAsyncState(STATE_TIMEOUT);

        // 发送超时错误回调
        AsyncRecognitionResult result(false, "", ERROR_TIMEOUT,
                                      "Request timeout after " + String(elapsedTime) + "ms");
        invokeAsyncCallback(result);

        // 清理状态和WebSocket
        cleanupAsyncRecognitionState();
        cleanupWebSocket();

        ESP_LOGI(TAG, "Async recognition timeout handled successfully");
        logEvent("async_timeout_handled", "Timeout handled, state cleaned up");
    }
}

// ============================================================================
// Update method for periodic timeout checking
// ============================================================================

void VolcanoSpeechService::update()
{
    // 检查异步超时
    checkAsyncTimeout();

    // 检查并处理重试
    handleAsyncRetry();

    // 同时更新WebSocket如果活动
    if (webSocketClient != nullptr)
    {
        webSocketClient->loop();
    }
}
// ============================================================================
// Async retry policy implementation
// ============================================================================

void VolcanoSpeechService::setAsyncRetryPolicy(const RetryPolicy &policy)
{
    if (lockAsyncState())
    {
        asyncRetryPolicy = policy;
        unlockAsyncState();
        ESP_LOGI(TAG, "Async retry policy updated: maxRetries=%u, initialBackoffMs=%u, backoffMultiplier=%.1f, maxBackoffMs=%u",
                 policy.maxRetries, policy.initialBackoffMs, policy.backoffMultiplier, policy.maxBackoffMs);
        logEvent("async_retry_policy_updated",
                 String("maxRetries=") + String(policy.maxRetries) +
                     ", initialBackoffMs=" + String(policy.initialBackoffMs) +
                     ", backoffMultiplier=" + String(policy.backoffMultiplier) +
                     ", maxBackoffMs=" + String(policy.maxBackoffMs));
    }
    else
    {
        ESP_LOGW(TAG, "Failed to lock async state for setting retry policy");
    }
}

void VolcanoSpeechService::resetAsyncRetryState()
{
    if (lockAsyncState())
    {
        currentRetryCount = 0;
        nextRetryTimeMs = 0;
        lastRetryableErrorCode = ERROR_NONE;
        unlockAsyncState();
        ESP_LOGD(TAG, "Async retry state reset");
    }
    else
    {
        ESP_LOGW(TAG, "Failed to lock async state for resetting retry state");
    }
}

bool VolcanoSpeechService::shouldRetryAsyncRequest(int errorCode) const
{
    // 检查是否为可重试错误
    if (!isRetryableError(errorCode))
    {
        return false;
    }

    // 检查是否达到最大重试次数
    if (currentRetryCount >= asyncRetryPolicy.maxRetries)
    {
        return false;
    }

    return true;
}

bool VolcanoSpeechService::isRetryableError(int errorCode) const
{
    // 定义可重试的错误码
    switch (errorCode)
    {
    case ERROR_NETWORK:        // 网络连接失败
    case ERROR_AUTHENTICATION: // 认证失败（可能token过期）
    case ERROR_WEBSOCKET:      // WebSocket连接错误
    case ERROR_TIMEOUT:        // 超时（可能是临时网络问题）
        return true;

    case ERROR_NONE:           // 无错误
    case ERROR_PROTOCOL:       // 协议错误（通常不可重试）
    case ERROR_SERVER:         // 服务器错误（取决于具体错误码）
    case ERROR_INVALID_STATE:  // 无效状态（编程错误）
    case ERROR_AUDIO_ENCODING: // 音频编码失败（数据问题）
    default:
        return false;
    }
}

uint32_t VolcanoSpeechService::calculateNextRetryDelay() const
{
    if (currentRetryCount == 0)
    {
        return asyncRetryPolicy.initialBackoffMs;
    }

    // 指数退避计算: delay = initial * (multiplier ^ (retryCount - 1))
    float delay = asyncRetryPolicy.initialBackoffMs *
                  pow(asyncRetryPolicy.backoffMultiplier, currentRetryCount - 1);

    // 限制最大退避时间
    if (delay > asyncRetryPolicy.maxBackoffMs)
    {
        delay = asyncRetryPolicy.maxBackoffMs;
    }

    return static_cast<uint32_t>(delay);
}

void VolcanoSpeechService::initializeAsyncRetryState()
{
    if (lockAsyncState())
    {
        currentRetryCount = 0;
        nextRetryTimeMs = 0;
        lastRetryableErrorCode = ERROR_NONE;
        unlockAsyncState();
    }
    else
    {
        ESP_LOGW(TAG, "Failed to lock async state for initializing retry state");
    }
}

bool VolcanoSpeechService::attemptAsyncRetry()
{
    if (!lockAsyncState())
    {
        ESP_LOGW(TAG, "Failed to lock async state for retry attempt");
        return false;
    }

    // 检查是否可以重试
    if (!shouldRetryAsyncRequest(lastRetryableErrorCode))
    {
        unlockAsyncState();
        return false;
    }

    // 检查重试时间是否已到
    uint32_t currentTime = millis();
    if (currentTime < nextRetryTimeMs)
    {
        unlockAsyncState();
        return false; // 还没到重试时间
    }

    // 增加重试计数
    currentRetryCount++;

    // 计算下一次重试延迟（用于日志）
    uint32_t nextDelay = calculateNextRetryDelay();

    ESP_LOGI(TAG, "Attempting async retry %u/%u for error %d (next delay: %ums)",
             currentRetryCount, asyncRetryPolicy.maxRetries, lastRetryableErrorCode, nextDelay);
    logEvent("async_retry_attempt",
             String("retry=") + String(currentRetryCount) +
                 "/" + String(asyncRetryPolicy.maxRetries) +
                 ", error=" + String(lastRetryableErrorCode) +
                 ", nextDelay=" + String(nextDelay) + "ms");

    unlockAsyncState();
    return true;
}

void VolcanoSpeechService::handleAsyncRetry()
{
    // 检查是否需要处理重试
    if (!getAsyncRecognitionInProgress())
    {
        return; // 没有进行中的异步请求
    }

    // 尝试重试
    if (attemptAsyncRetry())
    {
        // 执行重试逻辑
        // 注意：实际的重试执行需要在recognizeAsync失败时触发
        // 这里只检查时间并标记可以重试
        // 实际重试由错误处理代码调用
        ESP_LOGD(TAG, "Async retry ready to execute, should be handled by error recovery code");
    }
}

void VolcanoSpeechService::scheduleAsyncRetry()
{
    if (!lockAsyncState())
    {
        ESP_LOGW(TAG, "Failed to lock async state for scheduling retry");
        return;
    }

    // 计算下一次重试时间
    uint32_t delay = calculateNextRetryDelay();
    nextRetryTimeMs = millis() + delay;

    ESP_LOGI(TAG, "Async retry scheduled in %ums (retry %u/%u for error %d)",
             delay, currentRetryCount + 1, asyncRetryPolicy.maxRetries, lastRetryableErrorCode);
    logEvent("async_retry_scheduled",
             String("delay=") + String(delay) + "ms" +
                 ", retry=" + String(currentRetryCount + 1) +
                 "/" + String(asyncRetryPolicy.maxRetries) +
                 ", error=" + String(lastRetryableErrorCode));

    unlockAsyncState();
}

void VolcanoSpeechService::cancelAsyncRetry()
{
    if (lockAsyncState())
    {
        currentRetryCount = 0;
        nextRetryTimeMs = 0;
        lastRetryableErrorCode = ERROR_NONE;
        unlockAsyncState();
        ESP_LOGI(TAG, "Async retry cancelled");
        logEvent("async_retry_cancelled", "All pending retries cancelled");
    }
    else
    {
        ESP_LOGW(TAG, "Failed to lock async state for cancelling retry");
    }
}

// ============================================================================
// Async error handling with retry support
// ============================================================================

void VolcanoSpeechService::handleAsyncError(int errorCode, const String &errorMessage, bool immediateCleanup)
{
    if (!lockAsyncState())
    {
        ESP_LOGW(TAG, "Failed to lock async state for error handling");
        // 仍然尝试调用回调
        AsyncRecognitionResult result(false, "", errorCode, errorMessage);
        invokeAsyncCallback(result);
        return;
    }

    // 保存错误码用于重试决策
    lastRetryableErrorCode = errorCode;

    // 检查是否可以重试
    bool shouldRetry = shouldRetryAsyncRequest(errorCode);

    if (shouldRetry)
    {
        // 调度重试
        scheduleAsyncRetry();

        ESP_LOGI(TAG, "Async error handled, retry scheduled: error=%d, message=%s",
                 errorCode, errorMessage.c_str());
        logEvent("async_error_retry_scheduled",
                 String("error=") + String(errorCode) +
                     ", message=" + errorMessage +
                     ", retry=" + String(currentRetryCount + 1) +
                     "/" + String(asyncRetryPolicy.maxRetries));

        // 不立即清理状态，等待重试
        unlockAsyncState();
        return;
    }

    // 不可重试或达到最大重试次数
    ESP_LOGI(TAG, "Async error final: error=%d, message=%s, retries=%u/%u",
             errorCode, errorMessage.c_str(), currentRetryCount, asyncRetryPolicy.maxRetries);
    logEvent("async_error_final",
             String("error=") + String(errorCode) +
                 ", message=" + errorMessage +
                 ", retries=" + String(currentRetryCount) +
                 "/" + String(asyncRetryPolicy.maxRetries));

    // 发送错误回调
    AsyncRecognitionResult result(false, "", errorCode, errorMessage);

    // 解锁后再调用回调（避免死锁）
    unlockAsyncState();
    invokeAsyncCallback(result);

    // 清理状态
    if (immediateCleanup)
    {
        cleanupAsyncRecognitionState();
        cleanupWebSocket();
    }
}
