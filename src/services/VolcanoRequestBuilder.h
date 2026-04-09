/**
 * @file VolcanoRequestBuilder.h
 * @brief Builder for Volcano WebSocket Full Client Request JSON
 * @details Creates JSON requests for Volcano speech recognition service according to
 *          the WebSocket binary protocol specification.
 * @version 1.0
 * @date 2026-04-09
 */

#ifndef VOLCANO_REQUEST_BUILDER_H
#define VOLCANO_REQUEST_BUILDER_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cstdint>
#include <string>
#endif

/// @brief Platform-independent string type
#ifdef ARDUINO
using RequestString = String;
#else
using RequestString = std::string;
#endif

class VolcanoRequestBuilder {
public:
    /**
     * @brief Build Full Client Request JSON for Volcano WebSocket protocol
     * @param uid User ID (default: "esp32_user")
     * @param language Audio language code (default: "zh-CN")
     * @param enablePunctuation Enable punctuation restoration (default: true)
     * @param enableITN Enable inverse text normalization (default: false)
     * @param enableDDC Enable digital digit conversion (default: false)
     * @param format Audio format (default: "pcm")
     * @param rate Audio sample rate in Hz (default: 16000)
     * @param bits Audio bit depth (default: 16)
     * @param channel Number of audio channels (default: 1)
     * @return JSON string containing the full client request
     * @note Returns empty string if JSON serialization fails
     */
    static RequestString buildFullClientRequest(
        const RequestString& uid = "esp32_user",
        const RequestString& language = "zh-CN",
        bool enablePunctuation = true,
        bool enableITN = false,
        bool enableDDC = false,
        const RequestString& format = "pcm",
        int rate = 16000,
        int bits = 16,
        int channel = 1
    );

    /**
     * @brief Generate unique request ID
     * @return Unique request ID string
     * @note Format: "esp32_[timestamp]_[random]_[counter]"
     */
    static RequestString generateUniqueReqId();

private:
    // Default configuration constants
    static const char* DEFAULT_PLATFORM;
    static const char* DEFAULT_SDK_VERSION;
    static const char* DEFAULT_MODEL_NAME;

    // Request ID counter (thread-safe increment on ESP32)
    static uint32_t requestCounter;
};

#endif // VOLCANO_REQUEST_BUILDER_H