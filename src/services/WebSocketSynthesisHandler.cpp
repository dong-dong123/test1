/**
 * @file WebSocketSynthesisHandler.cpp
 * @brief Implementation of WebSocketSynthesisHandler for Volcano TTS synthesis
 * @version 1.0
 * @date 2026-04-09
 */

#include "WebSocketSynthesisHandler.h"
#include "TTSRequestBuilder.h"
#include "TTSResponseParser.h"
#include "WebSocketClient.h"
#include "../modules/NetworkManager.h"
#include "../interfaces/ConfigManager.h"
#include "../utils/MemoryUtils.h"
#include <ArduinoJson.h>

#ifdef ARDUINO
#include <esp_log.h>
static const char* TAG = "WebSocketSynthesisHandler";
#else
#include <iostream>
#define ESP_LOGI(tag, format, ...) printf("[%s] INFO: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) printf("[%s] ERROR: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("[%s] WARN: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) printf("[%s] DEBUG: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) printf("[%s] VERBOSE: " format "\n", tag, ##__VA_ARGS__)
#endif

// Default endpoint for unidirectional TTS streaming
const char* DEFAULT_TTS_UNIDIRECTIONAL_ENDPOINT = "wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream";

// Generate Connect ID (UUID format)
static SynthesisString generateConnectId() {
    // Generate UUID version 4 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    SynthesisString uuid;

#ifdef ARDUINO
    // Arduino/ESP32 implementation using esp_random()
    uuid.reserve(36);

    // Generate 32 random hexadecimal characters
    const char hexChars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        uuid += hexChars[esp_random() & 0xF];
    }

    // Insert hyphens
    uuid = uuid.substring(0, 8) + '-' + uuid.substring(8, 12) + '-' + uuid.substring(12, 16) + '-' + uuid.substring(16, 20) + '-' + uuid.substring(20);

    // Set UUID version 4 (13th character as '4')
    uuid[14] = '4';

    // Set variant 1 (17th character as '8', '9', 'a', or 'b')
    const char variants[] = {'8', '9', 'a', 'b'};
    uuid[19] = variants[esp_random() & 0x3];
#else
    // Desktop implementation using rand()
    uuid.reserve(36);

    // Generate 32 random hexadecimal characters
    const char hexChars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        uuid += hexChars[rand() & 0xF];
    }

    // Insert hyphens
    uuid = uuid.substr(0, 8) + '-' + uuid.substr(8, 4) + '-' + uuid.substr(12, 4) + '-' + uuid.substr(16, 4) + '-' + uuid.substr(20);

    // Set UUID version 4 (13th character as '4')
    uuid[14] = '4';

    // Set variant 1 (17th character as '8', '9', 'a', or 'b')
    const char variants[] = {'8', '9', 'a', 'b'};
    uuid[19] = variants[rand() & 0x3];
#endif

    return uuid;
}

// Constructor
WebSocketSynthesisHandler::WebSocketSynthesisHandler(
    NetworkManager* networkManager,
    ConfigManager* configManager
) : networkManager(networkManager),
    configManager(configManager),
    webSocketClient(nullptr),
    lastError(""),
    appId(""),
    accessToken(""),
    cluster("volcano_tts"),
    uid("esp32_user"),
    voiceType("zh-CN_female_standard"),
    encoding("pcm"),
    sampleRate(16000),
    speedRatio(1.0f),
    connectTimeoutMs(10000),
    responseTimeoutMs(30000),
    chunkTimeoutMs(5000),
    isSynthesisInProgress(false),
    receivedFinalResponse(false),
    lastReqId(""),
    startTimeMs(0)
{
    ESP_LOGI(TAG, "WebSocketSynthesisHandler initialized");
}

// Destructor
WebSocketSynthesisHandler::~WebSocketSynthesisHandler()
{
    cleanup();
    ESP_LOGI(TAG, "WebSocketSynthesisHandler destroyed");
}

// Synthesize text to speech via WebSocket
bool WebSocketSynthesisHandler::synthesizeViaWebSocket(
    const SynthesisString& text,
    std::vector<uint8_t>& audioData,
    const SynthesisString& endpoint
) {
    // PSRAM内存监控 - WebSocket网络缓冲区
    if (MemoryUtils::isPSRAMAvailable()) {
        size_t freePSRAM = MemoryUtils::getFreePSRAM();
        size_t largestPSRAM = MemoryUtils::getLargestFreePSRAMBlock();
        ESP_LOGI(TAG, "PSRAM available: free=%u bytes, largest block=%u bytes", freePSRAM, largestPSRAM);

        // 为WebSocket数据分配PSRAM缓冲区
        const size_t bufferSize = 8192; // 8KB WebSocket缓冲区
        void* wsBuffer = MemoryUtils::allocateNetworkBuffer(bufferSize);
        if (wsBuffer) {
            ESP_LOGI(TAG, "Allocated PSRAM WebSocket buffer %p (%u bytes)", wsBuffer, bufferSize);
            // 注意：这里仅演示，实际需要将缓冲区用于WebSocket数据接收
            heap_caps_free(wsBuffer);
        }
    }

    ESP_LOGI(TAG, "Starting WebSocket synthesis for text: %s", text.c_str());

    // Validate input
    if (text.isEmpty()) {
        lastError = "Text cannot be empty";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    // Reset state
    cleanup();
    pendingAudioData.clear();
    receivedFinalResponse = false;
    lastReqId = "";
    isSynthesisInProgress = true;
    startTimeMs = millis();

    // Load configuration from ConfigManager if available
    if (configManager) {
        if (appId.isEmpty()) {
            appId = configManager->getString("services.speech.volcano.appId", "");
        }
        if (accessToken.isEmpty()) {
            accessToken = configManager->getString("services.speech.volcano.secretKey", "");
        }
        if (cluster.isEmpty()) {
            cluster = configManager->getString("services.speech.volcano.cluster", "volcano_tts");
        }
        if (uid.isEmpty()) {
            uid = configManager->getString("services.speech.volcano.uid", "esp32_user");
        }
        if (voiceType.isEmpty()) {
            voiceType = configManager->getString("services.speech.volcano.voice", "zh-CN_female_standard");
        }
        if (ttsResourceId.isEmpty()) {
            ttsResourceId = configManager->getString("services.speech.volcano.ttsResourceId", "seed-tts-2.0");
        }
    }

    // Validate configuration
    if (appId.isEmpty() || accessToken.isEmpty()) {
        lastError = "App ID or Access Token not configured";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        isSynthesisInProgress = false;
        return false;
    }

    // Connect to WebSocket endpoint
    if (!connectWithAuth(endpoint.isEmpty() ? DEFAULT_TTS_UNIDIRECTIONAL_ENDPOINT : endpoint)) {
        ESP_LOGE(TAG, "Failed to connect to WebSocket endpoint");
        isSynthesisInProgress = false;
        return false;
    }

    // Build synthesis request using TTSRequestBuilder
    SynthesisString jsonRequest = TTSRequestBuilder::buildSynthesisRequest(
        text,
        appId,
        accessToken,
        cluster,
        uid,
        voiceType,
        encoding,
        sampleRate,
        speedRatio
    );

    if (jsonRequest.isEmpty()) {
        lastError = "Failed to build synthesis request JSON";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        cleanup();
        isSynthesisInProgress = false;
        return false;
    }

    ESP_LOGI(TAG, "Synthesis request JSON size: %u bytes", jsonRequest.length());
    ESP_LOGV(TAG, "Synthesis request JSON: %s", jsonRequest.c_str());

    // Send synthesis request
    if (!sendSynthesisRequest(jsonRequest)) {
        ESP_LOGE(TAG, "Failed to send synthesis request");
        cleanup();
        isSynthesisInProgress = false;
        return false;
    }

    ESP_LOGI(TAG, "Synthesis request sent successfully");

    // Receive audio response
    if (!receiveAudioResponse(audioData)) {
        ESP_LOGE(TAG, "Failed to receive audio response");
        cleanup();
        isSynthesisInProgress = false;
        return false;
    }

    ESP_LOGI(TAG, "Audio response received successfully, size: %u bytes", audioData.size());

    // Clean up and return success
    cleanup();
    isSynthesisInProgress = false;
    return true;
}

// Get last error message
SynthesisString WebSocketSynthesisHandler::getLastError() const {
    return lastError;
}

// Check if WebSocket is connected
bool WebSocketSynthesisHandler::isConnected() const {
    return webSocketClient && webSocketClient->isConnected();
}

// Set configuration parameters
void WebSocketSynthesisHandler::setConfiguration(
    const SynthesisString& appId,
    const SynthesisString& accessToken,
    const SynthesisString& cluster,
    const SynthesisString& uid,
    const SynthesisString& voiceType,
    const SynthesisString& encoding,
    int sampleRate,
    float speedRatio,
    const SynthesisString& ttsResourceId
) {
    this->appId = appId;
    this->accessToken = accessToken;
    this->cluster = cluster;
    this->uid = uid;
    this->voiceType = voiceType;
    this->encoding = encoding;
    this->sampleRate = sampleRate;
    this->speedRatio = speedRatio;
    this->ttsResourceId = ttsResourceId;
}

// Set timeout values
void WebSocketSynthesisHandler::setTimeouts(
    uint32_t connectTimeoutMs,
    uint32_t responseTimeoutMs,
    uint32_t chunkTimeoutMs
) {
    this->connectTimeoutMs = connectTimeoutMs;
    this->responseTimeoutMs = responseTimeoutMs;
    this->chunkTimeoutMs = chunkTimeoutMs;
}

// Connect to WebSocket endpoint with authentication
bool WebSocketSynthesisHandler::connectWithAuth(const SynthesisString& endpoint) {
    ESP_LOGI(TAG, "Connecting to WebSocket endpoint: %s", endpoint.c_str());

    // Create WebSocket client if needed
    if (!webSocketClient) {
        webSocketClient = new WebSocketClient();
        if (!webSocketClient) {
            lastError = "Failed to create WebSocket client";
            ESP_LOGE(TAG, "%s", lastError.c_str());
            return false;
        }

        // Set event callback
        webSocketClient->setEventCallback([this](WebSocketEvent event, const String& message, const uint8_t* data, size_t length) {
            this->handleWebSocketEvent(event, message, data, length);
        });
    }

    // Build authentication headers
    SynthesisString headers = "";
    if (!appId.isEmpty()) {
        headers += "X-Api-App-Id: " + appId + "\r\n";
    }
    if (!accessToken.isEmpty()) {
        headers += "X-Api-Access-Key: " + accessToken + "\r\n";
    }

    // Add TTS resource ID header
    if (!ttsResourceId.isEmpty()) {
        headers += "X-Api-Resource-Id: " + ttsResourceId + "\r\n";
    }

    // Add sequence header (required for V3 API)
    headers += "X-Api-Sequence: -1\r\n";

    // Generate unique connection ID (UUID v4 format)
    SynthesisString uuid = generateConnectId();
    headers += "X-Api-Connect-Id: " + uuid + "\r\n";
    // Add protocol negotiation headers for better compatibility
    // Temporarily commented to restore server response
    // headers += "Accept-Encoding: identity\r\n"; // Request uncompressed responses
    // headers += "Accept: application/json";      // Explicitly request JSON format

    ESP_LOGI(TAG, "WebSocket authentication headers configured");
    ESP_LOGV(TAG, "Headers:\n%s", headers.c_str());

    webSocketClient->setExtraHeaders(headers);

    // Connect to endpoint
    if (!webSocketClient->connect(endpoint)) {
        lastError = "Failed to connect to WebSocket: " + webSocketClient->getLastError();
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    // Wait for connection to establish
    uint32_t connectStart = millis();
    while (!webSocketClient->isConnected()) {
        if (millis() - connectStart > connectTimeoutMs) {
            lastError = "Connection timeout";
            ESP_LOGE(TAG, "%s", lastError.c_str());
            webSocketClient->disconnect();
            return false;
        }
#ifdef ARDUINO
        delay(10);
#endif
    }

    ESP_LOGI(TAG, "WebSocket connected successfully");
    return true;
}

// Send synthesis request JSON
bool WebSocketSynthesisHandler::sendSynthesisRequest(const SynthesisString& jsonRequest) {
    if (!webSocketClient || !webSocketClient->isConnected()) {
        lastError = "WebSocket not connected";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    // Send JSON request as text message (chunked to avoid SSL error -80)
    if (!webSocketClient->sendTextChunked(jsonRequest, 128)) {
        lastError = "Failed to send synthesis request: " + webSocketClient->getLastError();
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Synthesis request sent successfully");
    return true;
}

// Receive and process audio response
bool WebSocketSynthesisHandler::receiveAudioResponse(std::vector<uint8_t>& audioData) {
    if (!webSocketClient || !webSocketClient->isConnected()) {
        lastError = "WebSocket not connected";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Waiting for audio response (timeout: %u ms)...", responseTimeoutMs);

    // Wait for response with timeout
    uint32_t responseStart = millis();
    while (!receivedFinalResponse && isSynthesisInProgress) {
        // Check timeout
        if (millis() - responseStart > responseTimeoutMs) {
            lastError = "Response timeout";
            ESP_LOGE(TAG, "%s", lastError.c_str());
            return false;
        }

        // Process WebSocket events (non-blocking)
        webSocketClient->loop();

#ifdef ARDUINO
        delay(10); // Small delay to prevent busy waiting
#endif
    }

    // Check if we received a final response
    if (!receivedFinalResponse) {
        lastError = "No final response received";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        return false;
    }

    // Copy collected audio data to output
    audioData = pendingAudioData;
    pendingAudioData.clear();

    ESP_LOGI(TAG, "Audio response processed successfully, total size: %u bytes", audioData.size());
    return true;
}

// Handle WebSocket events
void WebSocketSynthesisHandler::handleWebSocketEvent(WebSocketEvent event, const String& message, const uint8_t* data, size_t length) {
    ESP_LOGD(TAG, "WebSocket event: %d", static_cast<int>(event));

    switch (event) {
    case WebSocketEvent::CONNECTED:
        ESP_LOGI(TAG, "WebSocket connected");
        break;

    case WebSocketEvent::DISCONNECTED:
        ESP_LOGI(TAG, "WebSocket disconnected");
        isSynthesisInProgress = false;
        break;

    case WebSocketEvent::ERROR:
        ESP_LOGE(TAG, "WebSocket error: %s", message.c_str());
        lastError = "WebSocket error: " + message;
        isSynthesisInProgress = false;
        break;

    case WebSocketEvent::TEXT_MESSAGE:
        parseWebSocketMessage(message);
        break;

    case WebSocketEvent::BINARY_MESSAGE:
        handleSynthesisAudio(data, length, false);
        break;

    default:
        // Ignore other events
        break;
    }
}

// Parse WebSocket text message
void WebSocketSynthesisHandler::parseWebSocketMessage(const SynthesisString& jsonMessage) {
    ESP_LOGD(TAG, "Parsing WebSocket text message: %s", jsonMessage.c_str());

    // Parse JSON response using TTSResponseParser
    TTSResponseParser::SynthesisResult result = TTSResponseParser::parseResponse(jsonMessage);

    // Store request ID for tracking
    lastReqId = result.reqid;

    if (result.success) {
        ESP_LOGI(TAG, "Synthesis response successful: reqid=%s, duration=%dms",
                 result.reqid.c_str(), result.durationMs);

        // If there's audio data in the response (base64 encoded), decode and add it
        if (!result.audioData.empty()) {
            pendingAudioData.insert(pendingAudioData.end(),
                                    result.audioData.begin(),
                                    result.audioData.end());
            ESP_LOGI(TAG, "Added %u bytes of audio data from base64 response",
                     result.audioData.size());
        }

        // Check if this is the final response
        if (result.sequence >= 0) {
            // Sequence indicates multi-part response
            ESP_LOGI(TAG, "Received response part %d", result.sequence);
        } else {
            // No sequence means final response
            receivedFinalResponse = true;
            ESP_LOGI(TAG, "Received final synthesis response");
        }
    } else {
        ESP_LOGE(TAG, "Synthesis response failed: code=%d, message=%s",
                 result.code, result.message.c_str());
        lastError = "Synthesis failed: " + result.message;
        receivedFinalResponse = true; // Still final, but error
    }
}

// Handle synthesis audio chunk
void WebSocketSynthesisHandler::handleSynthesisAudio(const uint8_t* data, size_t length, bool isLastChunk) {
    if (!data || length == 0) {
        ESP_LOGW(TAG, "Empty audio chunk received");
        return;
    }

    ESP_LOGD(TAG, "Received audio chunk: %u bytes%s", length, isLastChunk ? " (last)" : "");

    // Add chunk to pending audio data
    pendingAudioData.insert(pendingAudioData.end(), data, data + length);

    // If this is the last chunk, mark response as complete
    if (isLastChunk) {
        receivedFinalResponse = true;
        ESP_LOGI(TAG, "Received last audio chunk, total audio size: %u bytes",
                 pendingAudioData.size());
    }
}

// Clean up WebSocket connection
void WebSocketSynthesisHandler::cleanup() {
    if (webSocketClient) {
        if (webSocketClient->isConnected()) {
            webSocketClient->disconnect();
        }
        delete webSocketClient;
        webSocketClient = nullptr;
    }

    isSynthesisInProgress = false;
    receivedFinalResponse = false;
    pendingAudioData.clear();
    lastReqId = "";

    ESP_LOGI(TAG, "WebSocket connection cleaned up");
}