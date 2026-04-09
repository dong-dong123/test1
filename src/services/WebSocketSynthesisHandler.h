/**
 * @file WebSocketSynthesisHandler.h
 * @brief WebSocket handler for Volcano text-to-speech synthesis
 * @details Implements WebSocket-based speech synthesis using Volcano TTS API
 *          with binary protocol support for streaming audio data.
 * @version 1.0
 * @date 2026-04-09
 */

#ifndef WEB_SOCKET_SYNTHESIS_HANDLER_H
#define WEB_SOCKET_SYNTHESIS_HANDLER_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cstdint>
#include <string>
#include <vector>
#endif

// Forward declarations
class WebSocketClient;
class NetworkManager;
class ConfigManager;

/// @brief Platform-independent string type
#ifdef ARDUINO
using SynthesisString = String;
#else
using SynthesisString = std::string;
#endif

/**
 * @class WebSocketSynthesisHandler
 * @brief Handler for WebSocket-based speech synthesis using Volcano TTS API
 *
 * This class provides methods to synthesize text to speech using Volcano's
 * WebSocket TTS API with support for streaming audio data and binary protocol.
 * It integrates with existing TTSRequestBuilder and TTSResponseParser components.
 */
class WebSocketSynthesisHandler {
public:
    /**
     * @brief Construct a new WebSocketSynthesisHandler object
     * @param networkManager Network manager instance (optional)
     * @param configManager Configuration manager instance (optional)
     *
     * If networkManager is not provided, the handler will create its own.
     * If configManager is not provided, default configuration will be used.
     */
    explicit WebSocketSynthesisHandler(
        NetworkManager* networkManager = nullptr,
        ConfigManager* configManager = nullptr
    );

    /**
     * @brief Destroy the WebSocketSynthesisHandler object
     */
    ~WebSocketSynthesisHandler();

    /**
     * @brief Synthesize text to speech via WebSocket
     * @param text Text to synthesize
     * @param audioData Output buffer for synthesized audio data
     * @param endpoint WebSocket endpoint URL (defaults to unidirectional stream endpoint)
     * @return true if synthesis succeeded, false otherwise
     *
     * This method performs the complete synthesis workflow:
     * 1. Connect to WebSocket endpoint with authentication
     * 2. Send synthesis request using TTSRequestBuilder
     * 3. Receive and parse audio response using TTSResponseParser
     * 4. Disconnect and return audio data
     */
    bool synthesizeViaWebSocket(
        const SynthesisString& text,
        std::vector<uint8_t>& audioData,
        const SynthesisString& endpoint = "wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream"
    );

    /**
     * @brief Get the last error message
     * @return Last error message as string
     */
    SynthesisString getLastError() const;

    /**
     * @brief Check if WebSocket is currently connected
     * @return true if connected, false otherwise
     */
    bool isConnected() const;

    /**
     * @brief Set configuration parameters
     * @param appId Application ID (APP ID)
     * @param accessToken Access token for authentication
     * @param cluster Cluster identifier (default: "volcano_tts")
     * @param uid User ID (default: "esp32_user")
     * @param voiceType Voice type identifier (default: "zh-CN_female_standard")
     * @param encoding Audio encoding format (default: "pcm")
     * @param sampleRate Audio sample rate in Hz (default: 16000)
     * @param speedRatio Speech speed ratio (default: 1.0)
     */
    void setConfiguration(
        const SynthesisString& appId,
        const SynthesisString& accessToken,
        const SynthesisString& cluster = "volcano_tts",
        const SynthesisString& uid = "esp32_user",
        const SynthesisString& voiceType = "zh-CN_female_standard",
        const SynthesisString& encoding = "pcm",
        int sampleRate = 16000,
        float speedRatio = 1.0f
    );

    /**
     * @brief Set timeout values for WebSocket operations
     * @param connectTimeoutMs Connection timeout in milliseconds (default: 10000)
     * @param responseTimeoutMs Response timeout in milliseconds (default: 30000)
     * @param chunkTimeoutMs Chunk receive timeout in milliseconds (default: 5000)
     */
    void setTimeouts(
        uint32_t connectTimeoutMs = 10000,
        uint32_t responseTimeoutMs = 30000,
        uint32_t chunkTimeoutMs = 5000
    );

private:
    /**
     * @brief Connect to WebSocket endpoint with authentication
     * @param endpoint WebSocket endpoint URL
     * @return true if connection succeeded, false otherwise
     */
    bool connectWithAuth(const SynthesisString& endpoint);

    /**
     * @brief Send synthesis request JSON
     * @param jsonRequest JSON request string
     * @return true if send succeeded, false otherwise
     */
    bool sendSynthesisRequest(const SynthesisString& jsonRequest);

    /**
     * @brief Receive and process audio response
     * @param audioData Output buffer for audio data
     * @return true if reception succeeded, false otherwise
     */
    bool receiveAudioResponse(std::vector<uint8_t>& audioData);

    /**
     * @brief Handle WebSocket events
     * @param event WebSocket event type
     * @param message Event message (for TEXT_MESSAGE events)
     * @param data Binary data (for BINARY_MESSAGE events)
     * @param length Length of binary data
     */
    void handleWebSocketEvent(int event, const SynthesisString& message, const uint8_t* data, size_t length);

    /**
     * @brief Parse WebSocket text message
     * @param jsonMessage JSON message string
     */
    void parseWebSocketMessage(const SynthesisString& jsonMessage);

    /**
     * @brief Handle synthesis audio chunk
     * @param data Audio chunk data
     * @param length Audio chunk length
     * @param isLastChunk Whether this is the last chunk
     */
    void handleSynthesisAudio(const uint8_t* data, size_t length, bool isLastChunk);

    /**
     * @brief Clean up WebSocket connection
     */
    void cleanup();

    // Member variables
    NetworkManager* networkManager;
    ConfigManager* configManager;
    WebSocketClient* webSocketClient;
    SynthesisString lastError;

    // Configuration
    SynthesisString appId;
    SynthesisString accessToken;
    SynthesisString cluster;
    SynthesisString uid;
    SynthesisString voiceType;
    SynthesisString encoding;
    int sampleRate;
    float speedRatio;

    // Timeouts
    uint32_t connectTimeoutMs;
    uint32_t responseTimeoutMs;
    uint32_t chunkTimeoutMs;

    // State tracking
    bool isSynthesisInProgress;
    std::vector<uint8_t> pendingAudioData;
    bool receivedFinalResponse;
    SynthesisString lastReqId;
    uint32_t startTimeMs;

    // Disable copy and assignment
    WebSocketSynthesisHandler(const WebSocketSynthesisHandler&) = delete;
    WebSocketSynthesisHandler& operator=(const WebSocketSynthesisHandler&) = delete;
};

#endif // WEB_SOCKET_SYNTHESIS_HANDLER_H