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
#include "NetworkManager.h"
#include "ConfigManager.h"
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
    ESP_LOGI(TAG, "Starting WebSocket synthesis for text: %s", text.c_str());

    // Validate input
    if (text.empty()) {
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
        if (appId.empty()) {
            appId = configManager->getString("services.volcano.appId", "");
        }
        if (accessToken.empty()) {
            accessToken = configManager->getString("services.volcano.secretKey", "");
        }
        if (cluster.empty()) {
            cluster = configManager->getString("services.volcano.cluster", "volcano_tts");
        }
        if (uid.empty()) {
            uid = configManager->getString("services.volcano.uid", "esp32_user");
        }
        if (voiceType.empty()) {
            voiceType = configManager->getString("services.volcano.voice", "zh-CN_female_standard");
        }
    }

    // Validate configuration
    if (appId.empty() || accessToken.empty()) {
        lastError = "App ID or Access Token not configured";
        ESP_LOGE(TAG, "%s", lastError.c_str());
        isSynthesisInProgress = false;
        return false;
    }

    // Connect to WebSocket endpoint
    if (!connectWithAuth(endpoint.empty() ? DEFAULT_TTS_UNIDIRECTIONAL_ENDPOINT : endpoint)) {
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

    if (jsonRequest.empty()) {
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
    float speedRatio
) {
    this->appId = appId;
    this->accessToken = accessToken;
    this->cluster = cluster;
    this->uid = uid;
    this->voiceType = voiceType;
    this->encoding = encoding;
    this->sampleRate = sampleRate;
    this->speedRatio = speedRatio;
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
        webSocketClient->setEventCallback([this](int event, const SynthesisString& message, const uint8_t* data, size_t length) {
            this->handleWebSocketEvent(event, message, data, length);
        });
    }

    // Build authentication headers
    SynthesisString headers = "";
    if (!appId.empty()) {
        headers += "X-Api-App-Key: " + appId + "\r\n";
    }
    if (!accessToken.empty()) {
        headers += "X-Api-Access-Key: " + accessToken + "\r\n";
    }

    // Generate unique connection ID
#ifdef ARDUINO
    SynthesisString uuid = "esp32_tts_" + SynthesisString(millis()) + "_" + SynthesisString(rand());
#else
    SynthesisString uuid = "desktop_tts_" + SynthesisString(time(nullptr)) + "_" + SynthesisString(rand());
#endif
    headers += "X-Api-Connect-Id: " + uuid + "\r\n";

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

    // Send JSON request as text message
    if (!webSocketClient->sendText(jsonRequest)) {
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
        webSocketClient->poll();

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
void WebSocketSynthesisHandler::handleWebSocketEvent(int event, const SynthesisString& message, const uint8_t* data, size_t length) {
    ESP_LOGD(TAG, "WebSocket event: %d", event);

    switch (event) {
    case 1: // CONNECTED
        ESP_LOGI(TAG, "WebSocket connected");
        break;

    case 2: // DISCONNECTED
        ESP_LOGI(TAG, "WebSocket disconnected");
        isSynthesisInProgress = false;
        break;

    case 3: // ERROR
        ESP_LOGE(TAG, "WebSocket error: %s", message.c_str());
        lastError = "WebSocket error: " + message;
        isSynthesisInProgress = false;
        break;

    case 4: // TEXT_MESSAGE
        parseWebSocketMessage(message);
        break;

    case 5: // BINARY_MESSAGE
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