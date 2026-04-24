#include "CozeDialogueService.h"
#include <esp_log.h>
#include <ArduinoJson.h>
#include "../utils/MemoryUtils.h"

static const char* TAG = "CozeDialogueService";

// ============================================================================
// 构造函数/析构函数
// ============================================================================

CozeDialogueService::CozeDialogueService(NetworkManager* netMgr,
                                         ConfigManager* configMgr,
                                         Logger* log)
    : networkManager(netMgr),
      configManager(configMgr),
      logger(log),
      isInitialized(false),
      isAvailableStatus(false),
      lastAvailabilityCheck(0),
      maxContextSize(10),
      isStreaming(false),
      streamStartTime(0),
      lastStreamDataTime(0) {

    ESP_LOGI(TAG, "CozeDialogueService created");
    config = createDefaultConfig();
}

CozeDialogueService::~CozeDialogueService() {
    deinitialize();
}

// ============================================================================
// 初始化/反初始化
// ============================================================================

bool CozeDialogueService::initialize() {
    if (isInitialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing CozeDialogueService...");

    // 检查必要的依赖
    if (!networkManager) {
        ESP_LOGE(TAG, "NetworkManager not set");
        lastError = "NetworkManager not set";
        return false;
    }

    // 加载配置
    if (!loadConfig()) {
        ESP_LOGW(TAG, "Failed to load configuration, using defaults");
    }

    // 初始化上下文
    clearContext();

    // 检查服务可用性
    updateAvailability();

    isInitialized = true;
    ESP_LOGI(TAG, "CozeDialogueService initialized successfully");

    logEvent("initialized", "Coze dialogue service ready");

    return true;
}

bool CozeDialogueService::deinitialize() {
    if (!isInitialized) {
        return true;
    }

    ESP_LOGI(TAG, "Deinitializing CozeDialogueService...");

    // 结束任何正在进行的流式会话
    if (isStreaming) {
        callStreamChatEnd();
        isStreaming = false;
    }

    // 清理上下文
    clearContext();

    isInitialized = false;
    isAvailableStatus = false;
    ESP_LOGI(TAG, "CozeDialogueService deinitialized");

    logEvent("deinitialized", "Coze dialogue service stopped");

    return true;
}

// ============================================================================
// 配置管理
// ============================================================================

bool CozeDialogueService::loadConfig() {
    if (!configManager) {
        ESP_LOGE(TAG, "No ConfigManager available");
        return false;
    }

    // 从配置管理器读取Coze配置
    config.apiKey = configManager->getString("services.dialogue.coze.apiKey", "");
    config.botId = configManager->getString("services.dialogue.coze.botId", "");
    config.userId = configManager->getString("services.dialogue.coze.userId", "esp32_user");
    config.endpoint = configManager->getString("services.dialogue.coze.endpoint", "https://api.coze.cn/v1/chat");
    config.streamEndpoint = configManager->getString("services.dialogue.coze.streamEndpoint", "https://kfdcyyzqgx.coze.site/stream_run");
    config.model = configManager->getString("services.dialogue.coze.model", "coze-model");
    config.temperature = configManager->getFloat("services.dialogue.coze.temperature", 0.95f);
    config.maxTokens = configManager->getInt("services.dialogue.coze.maxTokens", 8192);
    config.timeout = configManager->getFloat("services.dialogue.coze.timeout", 15.0f);
    config.projectId = configManager->getString("services.dialogue.coze.projectId", "7625602998236004386");
    config.sessionId = configManager->getString("services.dialogue.coze.sessionId", "l_tnvlo49EWy6p9YUl8oC");

    if (config.apiKey.isEmpty()) {
        ESP_LOGW(TAG, "No API key configured for Coze service");
    }

    if (config.botId.isEmpty()) {
        ESP_LOGW(TAG, "No Bot ID configured for Coze service");
    }

    ESP_LOGI(TAG, "Loaded Coze config: BotId=%s, Model=%s, Endpoint=%s, StreamEndpoint=%s",
            config.botId.c_str(), config.model.c_str(), config.endpoint.c_str(), config.streamEndpoint.c_str());

    return true;
}

bool CozeDialogueService::updateConfig(const CozeDialogueConfig& newConfig) {
    config = newConfig;

    // 如果已初始化，重新检查可用性
    if (isInitialized) {
        updateAvailability();
    }

    return true;
}

void CozeDialogueService::setNetworkManager(NetworkManager* netMgr) {
    networkManager = netMgr;
    if (isInitialized && networkManager) {
        updateAvailability();
    }
}

void CozeDialogueService::setConfigManager(ConfigManager* configMgr) {
    configManager = configMgr;
    if (isInitialized && configManager) {
        loadConfig();
        updateAvailability();
    }
}

void CozeDialogueService::setLogger(Logger* log) {
    logger = log;
}

// ============================================================================
// DialogueService接口实现
// ============================================================================

String CozeDialogueService::chat(const String& input) {
    if (!isInitialized) {
        ESP_LOGE(TAG, "Service not initialized");
        lastError = "Service not initialized";
        return "";
    }

    if (!isAvailable()) {
        ESP_LOGE(TAG, "Service not available");
        lastError = "Service not available";
        return "";
    }

    if (input.isEmpty()) {
        ESP_LOGE(TAG, "Input is empty");
        lastError = "Input is empty";
        return "";
    }

    ESP_LOGI(TAG, "Chat request: %s", input.c_str());

    String response;
    bool success = callChatAPI(input, contextHistory, response);

    if (success) {
        // 添加到上下文历史
        addToContext("User: " + input);
        addToContext("Assistant: " + response);

        ESP_LOGI(TAG, "Chat response: %s", response.c_str());
        logEvent("chat_success", "Input length: " + String(input.length()));
    } else {
        ESP_LOGE(TAG, "Chat failed: %s", lastError.c_str());
        logEvent("chat_failed", lastError);
        response = "Error: " + lastError;
    }

    return response;
}

String CozeDialogueService::chatWithContext(const String& input, const std::vector<String>& ctx) {
    if (!isInitialized) {
        lastError = "Service not initialized";
        return "";
    }

    ESP_LOGI(TAG, "Chat with context (context size: %u)", ctx.size());

    String response;
    bool success = callChatAPI(input, ctx, response);

    if (success) {
        ESP_LOGI(TAG, "Contextual chat response: %s", response.c_str());
    } else {
        ESP_LOGE(TAG, "Contextual chat failed: %s", lastError.c_str());
        response = "Error: " + lastError;
    }

    return response;
}

bool CozeDialogueService::chatStreamStart(const String& input) {
    if (!isInitialized) {
        lastError = "Service not initialized";
        return false;
    }

    if (isStreaming) {
        ESP_LOGW(TAG, "Streaming already in progress");
        return false;
    }

    ESP_LOGI(TAG, "Starting stream chat: %s", input.c_str());

    isStreaming = callStreamChatStart(input, contextHistory);

    if (isStreaming) {
        // 添加到上下文历史
        addToContext("User: " + input);
        logEvent("stream_chat_started", "Input length: " + String(input.length()));
    } else {
        ESP_LOGE(TAG, "Failed to start stream chat");
    }

    return isStreaming;
}

bool CozeDialogueService::chatStreamGetChunk(String& chunk, bool& is_last) {
    if (!isInitialized || !isStreaming) {
        lastError = "Not streaming";
        return false;
    }

    bool success = callStreamChatGetChunk(chunk, is_last);

    if (success && is_last) {
        // 流式对话结束
        addToContext("Assistant: " + chunk);
        isStreaming = false;
        currentStreamSessionId = "";
    }

    return success;
}

bool CozeDialogueService::isAvailable() const {
    // 如果最近检查过，返回缓存状态
    uint32_t currentTime = millis();
    if (currentTime - lastAvailabilityCheck < 60000) { // 1分钟缓存
        return isAvailableStatus;
    }

    // 非const版本更新状态
    const_cast<CozeDialogueService*>(this)->updateAvailability();
    return isAvailableStatus;
}

float CozeDialogueService::getCostPerRequest() const {
    // Coze成本估算（单位：元/请求）
    // 实际成本可能根据token数变化
    return 0.02f; // 假设每请求0.02元
}

void CozeDialogueService::clearContext() {
    contextHistory.clear();
    ESP_LOGI(TAG, "Context cleared");
    logEvent("context_cleared", "");
}

size_t CozeDialogueService::getContextSize() const {
    return contextHistory.size();
}

// ============================================================================
// 上下文管理
// ============================================================================

void CozeDialogueService::addToContext(const String& message) {
    if (message.isEmpty()) {
        return;
    }

    contextHistory.push_back(message);
    trimContext();

    ESP_LOGD(TAG, "Added to context: %s (total: %u)", message.c_str(), contextHistory.size());
}

void CozeDialogueService::trimContext() {
    while (contextHistory.size() > maxContextSize) {
        contextHistory.erase(contextHistory.begin());
    }
}

// ============================================================================
// 可用性检查
// ============================================================================

bool CozeDialogueService::updateAvailability() {
    if (!networkManager || !networkManager->isConnected()) {
        isAvailableStatus = false;
        lastError = "Network not connected";
        return false;
    }

    if (config.apiKey.isEmpty()) {
        isAvailableStatus = false;
        lastError = "API key not configured";
        return false;
    }

    if (config.botId.isEmpty()) {
        isAvailableStatus = false;
        lastError = "Bot ID not configured";
        return false;
    }

    // 简单的心跳检查
    isAvailableStatus = true;
    lastAvailabilityCheck = millis();
    lastError = "";

    return true;
}

// ============================================================================
// API调用实现（框架）
// ============================================================================

bool CozeDialogueService::callChatAPI(const String& input, const std::vector<String>& context, String& response) {
    // 根据用户提供的Coze示例代码实现
    // 示例使用: http://kfdcyyzqgx.coze.site/run
    // 请求格式: {"messages": [{"role": "user", "content": "..."}]}
    // 响应格式: {"messages": [{"role": "user", "content": "..."}, {"type": "ai", "content": "..."}]}

    if (!networkManager || !networkManager->isConnected()) {
        lastError = "Network not available";
        ESP_LOGE(TAG, "Network not available for Coze API call");
        return false;
    }

    ESP_LOGI(TAG, "Calling Coze chat API for input: %s (context size: %u)",
             input.c_str(), context.size());

    // 构建请求头
    std::map<String, String> headers;
    headers["Content-Type"] = "application/json";

    // 添加认证头（如果配置了API密钥）
    if (!config.apiKey.isEmpty()) {
        headers["Authorization"] = "Bearer " + config.apiKey;
        headers["Accept"] = "application/json";
    }

    // 构建请求JSON
    String requestBody = buildChatRequestJSON(input, context);
    if (requestBody.isEmpty()) {
        lastError = "Failed to build request JSON";
        ESP_LOGE(TAG, "Failed to build Coze request JSON");
        return false;
    }

    ESP_LOGV(TAG, "Coze request body: %s", requestBody.c_str());

    // 发送HTTP请求
    String endpoint = config.endpoint;

    // 增加超时时间到15秒（从5秒增加）
    HttpRequestConfig requestConfig;
    requestConfig.url = endpoint;
    requestConfig.method = HttpMethod::POST;
    requestConfig.body = requestBody;
    requestConfig.headers = headers;
    requestConfig.timeout = 15000; // 15秒超时
    requestConfig.maxRetries = 1;
    requestConfig.followRedirects = false;
    requestConfig.useSSL = endpoint.startsWith("https://");

    ESP_LOGI(TAG, "Coze API request: endpoint=%s, useSSL=%s, timeout=%dms",
             endpoint.c_str(), requestConfig.useSSL ? "true" : "false", requestConfig.timeout);

    HttpResponse httpResponse = networkManager->sendRequest(requestConfig);

    // 检查响应
    ESP_LOGI(TAG, "Coze API response status: %d, time: %dms",
             httpResponse.statusCode, httpResponse.responseTime);

    if (httpResponse.statusCode != 200) {
        lastError = "Coze API request failed with status: " + String(httpResponse.statusCode);
        if (!httpResponse.body.isEmpty()) {
            lastError += ", response: " + httpResponse.body.substring(0, 200);
            ESP_LOGE(TAG, "Coze API error response: %s", httpResponse.body.c_str());
        }
        ESP_LOGE(TAG, "Coze API failed: %s", lastError.c_str());
        return false;
    }

    // 解析响应
    bool parseSuccess = parseChatResponse(httpResponse.body, response);

    if (!parseSuccess) {
        lastError = "Failed to parse Coze API response";
        ESP_LOGE(TAG, "Failed to parse Coze response: %s", httpResponse.body.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Coze chat successful, response length: %u chars", response.length());
    logEvent("chat_success", "Input: \"" + input + "\", Response length: " + String(response.length()));

    return true;
}

bool CozeDialogueService::callStreamChatStart(const String& input, const std::vector<String>& context) {
    if (!networkManager || !networkManager->isConnected()) {
        lastError = "Network not available";
        ESP_LOGE(TAG, "Network not available for Coze stream API call");
        return false;
    }

    if (config.apiKey.isEmpty()) {
        lastError = "API key not configured";
        ESP_LOGE(TAG, "Coze API key not configured");
        return false;
    }

    if (config.streamEndpoint.isEmpty()) {
        lastError = "Stream endpoint not configured";
        ESP_LOGE(TAG, "Coze stream endpoint not configured");
        return false;
    }

    ESP_LOGI(TAG, "Starting Coze stream chat for input: %s (context size: %u)",
             input.c_str(), context.size());

    // 构建请求头
    std::map<String, String> headers;
    headers["Authorization"] = "Bearer " + config.apiKey;
    headers["Content-Type"] = "application/json";
    headers["Accept"] = "text/event-stream"; // 流式响应
    headers["Cache-Control"] = "no-cache";
    headers["Connection"] = "keep-alive";

    // 构建请求JSON
    String requestBody = buildStreamRequestJSON(input, context);
    if (requestBody.isEmpty()) {
        lastError = "Failed to build stream request JSON";
        ESP_LOGE(TAG, "Failed to build Coze stream request JSON");
        return false;
    }

    ESP_LOGI(TAG, "Starting stream connection to: %s", config.streamEndpoint.c_str());

    // 开始流式连接
    if (!networkManager->startStream(config.streamEndpoint, headers)) {
        lastError = "Failed to start stream connection";
        ESP_LOGE(TAG, "Failed to start Coze stream connection");
        return false;
    }

    // 发送请求体
    if (!networkManager->writeStreamChunk((const uint8_t*)requestBody.c_str(), requestBody.length())) {
        lastError = "Failed to send stream request body";
        ESP_LOGE(TAG, "Failed to send Coze stream request body");
        networkManager->endStream();
        return false;
    }

    // 初始化流式状态
    isStreaming = true;
    accumulatedAnswer = "";
    streamBuffer.clear();
    streamStartTime = millis();
    lastStreamDataTime = streamStartTime;
    streamEndpoint = config.streamEndpoint;

    ESP_LOGI(TAG, "Coze stream chat started successfully");
    logEvent("stream_chat_started", "Input: \"" + input + "\", Endpoint: " + config.streamEndpoint);

    return true;
}

bool CozeDialogueService::callStreamChatGetChunk(String& chunk, bool& is_last) {
    if (!isStreaming || !networkManager) {
        lastError = "Not streaming";
        ESP_LOGE(TAG, "Not in streaming state");
        return false;
    }

    // 检查流式超时（30秒无数据）
    uint32_t currentTime = millis();
    if (currentTime - lastStreamDataTime > 30000) {
        lastError = "Stream timeout (no data for 30 seconds)";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        callStreamChatEnd();
        return false;
    }

    // 检查总超时（120秒）
    if (currentTime - streamStartTime > 120000) {
        lastError = "Stream total timeout (120 seconds)";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        callStreamChatEnd();
        return false;
    }

    // 读取流式数据
    uint8_t buffer[1024];
    size_t bytesRead = 0;

    if (!networkManager->readStreamChunk(buffer, sizeof(buffer) - 1, bytesRead)) {
        // 没有数据可读，但连接可能仍然正常
        return false;
    }

    if (bytesRead == 0) {
        // 无数据
        return false;
    }

    // 更新最后数据时间
    lastStreamDataTime = currentTime;

    // 将数据添加到缓冲区
    size_t oldSize = streamBuffer.size();
    streamBuffer.resize(oldSize + bytesRead);
    memcpy(streamBuffer.data() + oldSize, buffer, bytesRead);

    // 尝试从缓冲区解析完整的事件行
    // 流式数据通常是Server-Sent Events格式：data: {...}\n\n
    bool processedEvents = false;
    size_t pos = 0;

    while (pos < streamBuffer.size()) {
        // 查找换行符
        size_t lineEnd = pos;
        while (lineEnd < streamBuffer.size() && streamBuffer[lineEnd] != '\n') {
            lineEnd++;
        }

        if (lineEnd >= streamBuffer.size()) {
            // 没有完整行
            break;
        }

        // 提取行
        String line((const char*)streamBuffer.data() + pos, lineEnd - pos);
        pos = lineEnd + 1; // 跳过换行符

        // 跳过空行
        if (line.isEmpty()) {
            continue;
        }

        // 检查是否是事件行
        if (line.startsWith("data: ")) {
            String eventJson = line.substring(6); // 去掉"data: "
            eventJson.trim();

            if (!eventJson.isEmpty()) {
                // 处理事件
                if (processStreamEvent(eventJson)) {
                    processedEvents = true;
                }
            }
        }
    }

    // 移除已处理的数据
    if (pos > 0) {
        if (pos < streamBuffer.size()) {
            // 移动剩余数据到缓冲区开头
            memmove(streamBuffer.data(), streamBuffer.data() + pos, streamBuffer.size() - pos);
        }
        streamBuffer.resize(streamBuffer.size() - pos);
    }

    // 如果有累积的答案，返回它
    if (!accumulatedAnswer.isEmpty()) {
        chunk = accumulatedAnswer;
        accumulatedAnswer = ""; // 清空以便下次获取新数据
        is_last = false; // 不是最后一块，除非收到message_end事件
        ESP_LOGI(TAG, "Returning stream chunk: %s", chunk.c_str());
        return true;
    }

    // 如果没有数据但processedEvents为true，可能处理了非answer事件
    if (processedEvents) {
        return false; // 继续等待answer数据
    }

    return false;
}

bool CozeDialogueService::callStreamChatEnd() {
    if (!isStreaming || !networkManager) {
        return true; // 已经结束
    }

    ESP_LOGI(TAG, "Ending Coze stream chat");

    // 结束流式连接
    networkManager->endStream();

    // 清理流式状态
    isStreaming = false;
    accumulatedAnswer = "";
    streamBuffer.clear();
    streamEndpoint = "";
    streamStartTime = 0;
    lastStreamDataTime = 0;

    ESP_LOGI(TAG, "Coze stream chat ended");
    logEvent("stream_chat_ended", "");

    return true;
}

// ============================================================================
// 请求/响应处理
// ============================================================================

String CozeDialogueService::buildChatRequestJSON(const String& input, const std::vector<String>& context) const {
    // 根据Coze API文档构建请求JSON
    // 参考：https://www.coze.cn/open/docs/chat

    DynamicJsonDocument requestDoc(4096); // 根据上下文大小调整

    // 必需参数
    requestDoc["bot_id"] = config.botId;
    requestDoc["user_id"] = config.userId;
    requestDoc["query"] = input;
    requestDoc["stream"] = false; // 非流式模式

    // 可选参数
    if (!config.model.isEmpty()) {
        requestDoc["model"] = config.model;
    }

    if (config.temperature > 0) {
        requestDoc["temperature"] = config.temperature;
    }

    if (config.maxTokens > 0) {
        requestDoc["max_tokens"] = config.maxTokens;
    }

    // 构建上下文消息
    if (!context.empty()) {
        JsonArray messages = requestDoc.createNestedArray("messages");

        // 添加上下文历史
        for (const String& msg : context) {
            // 简单处理：假设上下文消息是交替的用户/助手消息
            // 实际应根据对话历史格式处理
            JsonObject message = messages.createNestedObject();
            message["role"] = "user"; // 简化：全部作为用户消息
            message["content"] = msg;
        }

        // 添加当前查询
        JsonObject currentMessage = messages.createNestedObject();
        currentMessage["role"] = "user";
        currentMessage["content"] = input;
    }

    // 其他可选参数
    // requestDoc["conversation_id"] = ""; // 用于多轮对话
    // requestDoc["additional_messages"] = JsonArray(); // 附加消息

    String requestBody;
    serializeJson(requestDoc, requestBody);

    ESP_LOGV(TAG, "Built Coze request JSON: %s", requestBody.c_str());
    return requestBody;
}

String CozeDialogueService::buildStreamRequestJSON(const String& input, const std::vector<String>& context) const {
    // 根据Coze流式API文档构建请求JSON
    // 参考用户提供的API文档格式

    DynamicJsonDocument requestDoc(4096);

    // content对象
    JsonObject contentObj = requestDoc.createNestedObject("content");
    JsonObject queryObj = contentObj.createNestedObject("query");
    JsonArray promptArray = queryObj.createNestedArray("prompt");

    // 添加上下文历史
    for (const String& msg : context) {
        JsonObject msgObj = promptArray.createNestedObject();
        msgObj["type"] = "text";
        JsonObject contentData = msgObj.createNestedObject("content");
        contentData["text"] = msg;
    }

    // 添加当前输入
    JsonObject currentMsgObj = promptArray.createNestedObject();
    currentMsgObj["type"] = "text";
    JsonObject currentContentData = currentMsgObj.createNestedObject("content");
    currentContentData["text"] = input;

    // 其他必需字段
    requestDoc["type"] = "query";
    requestDoc["session_id"] = config.sessionId.isEmpty() ? "l_tnvlo49EWy6p9YUl8oC" : config.sessionId;
    requestDoc["project_id"] = config.projectId.isEmpty() ? "7625602998236004386" : config.projectId;

    String requestBody;
    serializeJson(requestDoc, requestBody);

    ESP_LOGV(TAG, "Built Coze stream request JSON: %s", requestBody.c_str());
    return requestBody;
}

bool CozeDialogueService::parseChatResponse(const String& jsonResponse, String& output) const {
    // 解析Coze API响应JSON
    // 支持多种响应格式：
    // 1. 根级别messages数组格式（coze.site API）: {"messages": [{"type": "ai", "content": "..."}, ...]}
    // 2. 标准Coze API格式: {"code": 0, "data": {"messages": [...]}}
    // 3. 其他常见格式

    // 解析前的内存监控
    ESP_LOGI(TAG, "=== Coze API响应解析前内存状态 ===");
    MemoryUtils::printDetailedMemoryStatus("Pre-Coze Parse");
    MemoryUtils::monitorTaskStacks("Pre-Coze Parse");

    DynamicJsonDocument responseDoc(4096);
    DeserializationError error = deserializeJson(responseDoc, jsonResponse);

    if (error) {
        ESP_LOGE(TAG, "Failed to parse Coze response JSON: %s", error.c_str());
        output = "Error parsing JSON: " + String(error.c_str());
        return false;
    }

    // 检查错误码
    if (responseDoc.containsKey("code") && responseDoc["code"].as<int>() != 0) {
        String errorMsg = responseDoc["msg"] | responseDoc["message"] | "Unknown error";
        int errorCode = responseDoc["code"] | -1;
        ESP_LOGE(TAG, "Coze API error %d: %s", errorCode, errorMsg.c_str());
        output = "API Error " + String(errorCode) + ": " + errorMsg;
        return false;
    }

    // 首先检查根级别的messages数组（coze.site API格式）
    if (responseDoc.containsKey("messages") && responseDoc["messages"].is<JsonArray>()) {
        JsonArray messages = responseDoc["messages"];
        ESP_LOGI(TAG, "Found root-level messages array with %d messages", messages.size());

        for (JsonObject msg : messages) {
            String type = msg["type"] | "";
            String content = msg["content"] | "";
            String role = msg["role"] | "";

            ESP_LOGD(TAG, "Message: type=%s, role=%s, content length=%d",
                     type.c_str(), role.c_str(), content.length());

            // 查找type为"ai"或role为"assistant"的消息
            if ((type == "ai" || role == "assistant") && !content.isEmpty()) {
                output = content;
                ESP_LOGI(TAG, "Extracted AI reply from root-level messages: %s", output.c_str());

                // 解析成功后的内存监控
                ESP_LOGI(TAG, "=== Coze API响应解析成功后内存状态 ===");
                MemoryUtils::printDetailedMemoryStatus("Post-Coze Parse Success");
                MemoryUtils::monitorTaskStacks("Post-Coze Parse Success");

                return true;
            }
        }

        // 如果没有找到type为"ai"的消息，尝试查找第一个非空content
        for (JsonObject msg : messages) {
            String content = msg["content"] | "";
            if (!content.isEmpty()) {
                output = content;
                ESP_LOGI(TAG, "Extracted first non-empty content from root-level messages: %s", output.c_str());

                // 解析成功后的内存监控
                ESP_LOGI(TAG, "=== Coze API响应解析成功后内存状态（第一个非空内容）===");
                MemoryUtils::printDetailedMemoryStatus("Post-Coze Parse First Non-Empty");
                MemoryUtils::monitorTaskStacks("Post-Coze Parse First Non-Empty");

                return true;
            }
        }
    }

    // 然后检查标准Coze API格式（data字段）
    if (responseDoc.containsKey("data")) {
        JsonObject data = responseDoc["data"];

        // 尝试从messages数组提取回复
        if (data.containsKey("messages") && data["messages"].is<JsonArray>()) {
            JsonArray messages = data["messages"];
            for (JsonObject msg : messages) {
                String role = msg["role"] | "";
                String type = msg["type"] | "";
                String content = msg["content"] | "";

                if ((role == "assistant" || type == "answer") && !content.isEmpty()) {
                    output = content;
                    ESP_LOGI(TAG, "Extracted assistant reply from data.messages: %s", output.c_str());

                    // 解析成功后的内存监控
                    ESP_LOGI(TAG, "=== Coze API响应解析成功后内存状态（data.messages）===");
                    MemoryUtils::printDetailedMemoryStatus("Post-Coze Parse Data Messages");
                    MemoryUtils::monitorTaskStacks("Post-Coze Parse Data Messages");

                    return true;
                }
            }
        }

        // 备用：直接查找reply字段
        if (data.containsKey("reply")) {
            output = data["reply"].as<String>();
            ESP_LOGI(TAG, "Extracted reply field: %s", output.c_str());

            // 解析成功后的内存监控
            ESP_LOGI(TAG, "=== Coze API响应解析成功后内存状态（data.reply）===");
            MemoryUtils::printDetailedMemoryStatus("Post-Coze Parse Data Reply");
            MemoryUtils::monitorTaskStacks("Post-Coze Parse Data Reply");

            return true;
        }

        // 备用：查找content字段
        if (data.containsKey("content")) {
            output = data["content"].as<String>();
            ESP_LOGI(TAG, "Extracted content field: %s", output.c_str());

            // 解析成功后的内存监控
            ESP_LOGI(TAG, "=== Coze API响应解析成功后内存状态（data.content）===");
            MemoryUtils::printDetailedMemoryStatus("Post-Coze Parse Data Content");
            MemoryUtils::monitorTaskStacks("Post-Coze Parse Data Content");

            return true;
        }
    }

    // 其他可能的响应格式
    if (responseDoc.containsKey("choices") && responseDoc["choices"].is<JsonArray>()) {
        JsonArray choices = responseDoc["choices"];
        if (choices.size() > 0) {
            JsonObject choice = choices[0];
            if (choice.containsKey("message") && choice["message"].containsKey("content")) {
                output = choice["message"]["content"].as<String>();
                ESP_LOGI(TAG, "Extracted from choices: %s", output.c_str());

                // 解析成功后的内存监控
                ESP_LOGI(TAG, "=== Coze API响应解析成功后内存状态（choices）===");
                MemoryUtils::printDetailedMemoryStatus("Post-Coze Parse Choices");
                MemoryUtils::monitorTaskStacks("Post-Coze Parse Choices");

                return true;
            }
        }
    }

    // 如果没有找到标准格式，尝试直接提取文本字段
    if (responseDoc.containsKey("text")) {
        output = responseDoc["text"].as<String>();
        ESP_LOGI(TAG, "Extracted text field: %s", output.c_str());

        // 解析成功后的内存监控
        ESP_LOGI(TAG, "=== Coze API响应解析成功后内存状态（text字段）===");
        MemoryUtils::printDetailedMemoryStatus("Post-Coze Parse Text Field");
        MemoryUtils::monitorTaskStacks("Post-Coze Parse Text Field");

        return true;
    }

    // 最后手段：返回原始响应（调试用）
    output = jsonResponse.substring(0, 200); // 截断长响应
    ESP_LOGW(TAG, "Could not extract standard response, returning raw: %s", output.c_str());

    // 解析失败后的内存监控
    ESP_LOGI(TAG, "=== Coze API响应解析失败后内存状态 ===");
    MemoryUtils::printDetailedMemoryStatus("Post-Coze Parse Failed");
    MemoryUtils::monitorTaskStacks("Post-Coze Parse Failed");

    return false;
}


bool CozeDialogueService::parseStreamChunk(const String& chunkData, String& text, bool& isLast) const {
    // TODO: 解析流式分片（保留用于兼容性）
    ESP_LOGW(TAG, "parseStreamChunk not implemented");
    return false;
}

// ============================================================================
// 流式事件处理
// ============================================================================

bool CozeDialogueService::processStreamEvent(const String& eventJson) {
    ESP_LOGD(TAG, "Processing stream event: %s", eventJson.c_str());

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, eventJson);

    if (error) {
        ESP_LOGE(TAG, "Failed to parse stream event JSON: %s", error.c_str());
        return false;
    }

    // 提取事件类型
    String eventType = doc["event"] | "";
    if (eventType.isEmpty()) {
        ESP_LOGW(TAG, "Stream event missing event type");
        return false;
    }

    // 检查是否有data对象
    if (!doc.containsKey("data") || !doc["data"].is<JsonObject>()) {
        ESP_LOGW(TAG, "Stream event missing data object");
        return false;
    }

    JsonObject data = doc["data"].as<JsonObject>();

    // 根据事件类型处理
    if (eventType == "message_start") {
        return handleStreamMessageStart(data);
    } else if (eventType == "message_end") {
        return handleStreamMessageEnd(data);
    } else if (eventType == "answer") {
        return handleStreamAnswer(data);
    } else if (eventType == "tool_request") {
        return handleStreamToolRequest(data);
    } else if (eventType == "tool_response") {
        return handleStreamToolResponse(data);
    } else if (eventType == "error") {
        return handleStreamError(data);
    } else {
        ESP_LOGW(TAG, "Unknown stream event type: %s", eventType.c_str());
        return false;
    }
}

bool CozeDialogueService::handleStreamMessageStart(const JsonObject& data) {
    // message_start事件
    String localMsgId = data["local_msg_id"] | "";
    String msgId = data["msg_id"] | "";

    ESP_LOGI(TAG, "Message started: local_msg_id=%s, msg_id=%s",
             localMsgId.c_str(), msgId.c_str());

    // 重置累积的答案
    accumulatedAnswer = "";

    logEvent("stream_message_start", "msg_id=" + msgId);
    return true;
}

bool CozeDialogueService::handleStreamMessageEnd(const JsonObject& data) {
    // message_end事件
    String code = data["code"] | "";
    String message = data["message"] | "";

    ESP_LOGI(TAG, "Message ended: code=%s, message=%s",
             code.c_str(), message.c_str());

    // 检查是否成功
    if (code == "0") {
        // 成功完成
        logEvent("stream_message_end", "Success: " + message);
    } else {
        // 失败或取消
        lastError = "Stream ended with code " + code + ": " + message;
        ESP_LOGE(TAG, "%s", lastError.c_str());
        logEvent("stream_message_end_error", lastError);
    }

    // 标记流式结束
    // 注意：这里不设置isStreaming=false，因为callStreamChatGetChunk需要处理最终数据
    return true;
}

bool CozeDialogueService::handleStreamAnswer(const JsonObject& data) {
    // answer事件
    String answer = data["answer"] | "";

    if (!answer.isEmpty()) {
        // 累积答案
        accumulatedAnswer += answer;
        ESP_LOGI(TAG, "Received answer chunk: %s (total: %u chars)",
                 answer.c_str(), accumulatedAnswer.length());
        logEvent("stream_answer_chunk", "Length: " + String(answer.length()));
        return true;
    }

    ESP_LOGW(TAG, "Empty answer in stream event");
    return false;
}

bool CozeDialogueService::handleStreamToolRequest(const JsonObject& data) {
    // tool_request事件
    String toolCallId = data["tool_call_id"] | "";
    String toolName = data["tool_name"] | "";
    bool isParallel = data["is_parallel"] | false;
    int index = data["index"] | 0;

    ESP_LOGI(TAG, "Tool request: tool_call_id=%s, tool_name=%s, is_parallel=%s, index=%d",
             toolCallId.c_str(), toolName.c_str(), isParallel ? "yes" : "no", index);

    // 这里可以处理工具调用请求
    // 实际应用中可能需要调用相应工具并发送tool_response

    logEvent("stream_tool_request", "tool_name=" + toolName);
    return true;
}

bool CozeDialogueService::handleStreamToolResponse(const JsonObject& data) {
    // tool_response事件
    String toolCallId = data["tool_call_id"] | "";
    String code = data["code"] | "";
    String message = data["message"] | "";
    String result = data["result"] | "";
    String toolName = data["tool_name"] | "";
    int timeCostMs = data["time_cost_ms"] | 0;

    ESP_LOGI(TAG, "Tool response: tool_call_id=%s, code=%s, tool_name=%s, time=%dms",
             toolCallId.c_str(), code.c_str(), toolName.c_str(), timeCostMs);

    logEvent("stream_tool_response", "tool_name=" + toolName + ", code=" + code);
    return true;
}

bool CozeDialogueService::handleStreamError(const JsonObject& data) {
    // error事件
    String localMsgId = data["local_msg_id"] | "";
    int errorCode = data["code"] | 0;
    String errorMsg = data["error_msg"] | "";

    lastError = "Stream error " + String(errorCode) + ": " + errorMsg;
    ESP_LOGE(TAG, "Stream error: local_msg_id=%s, code=%d, message=%s",
             localMsgId.c_str(), errorCode, errorMsg.c_str());

    logEvent("stream_error", lastError);
    return true;
}

// ============================================================================
// 认证头生成
// ============================================================================

String CozeDialogueService::generateAuthHeader() const {
    // Coze API通常使用Bearer Token认证
    if (config.apiKey.isEmpty()) {
        return "";
    }

    return "Bearer " + config.apiKey;
}

// ============================================================================
// 日志记录
// ============================================================================

void CozeDialogueService::logEvent(const String& event, const String& details) const {
    if (logger) {
        String message = "CozeDialogueService: " + event;
        if (!details.isEmpty()) {
            message += " - " + details;
        }
        logger->log(Logger::Level::INFO, message);
    }
}

// ============================================================================
// 静态工具方法
// ============================================================================

String CozeDialogueService::getDefaultConfigJSON() {
    // 返回默认配置的JSON字符串
    return R"({
        "services": {
            "coze": {
                "apiKey": "",
                "botId": "",
                "userId": "esp32_user",
                "endpoint": "https://api.coze.cn/v1/chat",
                "streamEndpoint": "https://kfdcyyzqgx.coze.site/stream_run",
                "model": "coze-model",
                "temperature": 0.95,
                "maxTokens": 8192,
                "timeout": 15.0,
                "projectId": "7625602998236004386",
                "sessionId": "l_tnvlo49EWy6p9YUl8oC"
            }
        }
    })";
}

CozeDialogueConfig CozeDialogueService::createDefaultConfig() {
    CozeDialogueConfig config;
    config.apiKey = "";
    config.botId = "";
    config.userId = "esp32_user";
    config.endpoint = "https://api.coze.cn/v1/chat";
    config.streamEndpoint = "https://kfdcyyzqgx.coze.site/stream_run";
    config.model = "coze-model";
    config.temperature = 0.95f;
    config.maxTokens = 8192;
    config.timeout = 15.0f;
    config.projectId = "7625602998236004386";
    config.sessionId = "l_tnvlo49EWy6p9YUl8oC";
    return config;
}