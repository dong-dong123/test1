/**
 * @file TTSRequestBuilder.h
 * @brief Builder for Volcano WebSocket Text-to-Speech (TTS) JSON requests
 * @details Creates JSON requests for Volcano speech synthesis service according to
 *          the WebSocket binary protocol specification.
 * @version 1.0
 * @date 2026-04-09
 */

#ifndef TTS_REQUEST_BUILDER_H
#define TTS_REQUEST_BUILDER_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cstdint>
#include <string>
#endif

/// @brief Platform-independent string type
#ifdef ARDUINO
using TTSRequestString = String;
#else
using TTSRequestString = std::string;
#endif

class TTSRequestBuilder {
public:
    /**
     * @brief Build Text-to-Speech synthesis request JSON for Volcano WebSocket protocol
     * @param text Text to synthesize
     * @param appid Application ID (default: placeholder)
     * @param token Access token (default: placeholder)
     * @param cluster Cluster name (default: "volcano_tts")
     * @param uid User ID (default: "esp32_user")
     * @param voiceType Voice type identifier (default: "zh-CN_female_standard")
     * @param encoding Audio encoding format (default: "pcm")
     * @param rate Audio sample rate in Hz (default: 16000)
     * @param speedRatio Speech speed ratio (default: 1.0)
     * @return JSON string containing the TTS synthesis request
     * @note Returns empty string if JSON serialization fails
     * @note The appid and token should be replaced with actual credentials from configuration
     */
    static TTSRequestString buildSynthesisRequest(
        const TTSRequestString& text,
        const TTSRequestString& appid = "2015527679", // Default placeholder appid
        const TTSRequestString& token = "YOUR_ACCESS_TOKEN", // Default placeholder token
        const TTSRequestString& cluster = "volcano_tts",
        const TTSRequestString& uid = "esp32_user",
        const TTSRequestString& voiceType = "zh-CN_female_standard",
        const TTSRequestString& encoding = "pcm",
        int rate = 16000,
        float speedRatio = 1.0f
    );

    /**
     * @brief Generate unique request ID for TTS requests
     * @return Unique request ID string
     * @note Format: "tts_[timestamp]_[random]_[counter]"
     */
    static TTSRequestString generateUniqueReqId();

private:
    // Default configuration constants
    static const char* DEFAULT_CLUSTER;
    static const char* DEFAULT_VOICE_TYPE;
    static const char* DEFAULT_ENCODING;
    static const float DEFAULT_SPEED_RATIO;
    static const int DEFAULT_SAMPLE_RATE;

    // Request ID counter (thread-safe increment on ESP32)
    static uint32_t requestCounter;
};

#endif // TTS_REQUEST_BUILDER_H