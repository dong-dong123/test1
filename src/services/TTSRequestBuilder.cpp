/**
 * @file TTSRequestBuilder.cpp
 * @brief Implementation of TTSRequestBuilder for creating WebSocket TTS JSON requests
 * @version 1.0
 * @date 2026-04-09
 */

#include "TTSRequestBuilder.h"
#include <ArduinoJson.h>

#ifdef ARDUINO
#include <time.h>
#include <esp_random.h>
#else
#include <ctime>
#include <cstdlib>
#endif

// Static member initialization
const char* TTSRequestBuilder::DEFAULT_CLUSTER = "volcano_tts";
const char* TTSRequestBuilder::DEFAULT_VOICE_TYPE = "zh-CN_female_standard";
const char* TTSRequestBuilder::DEFAULT_ENCODING = "pcm";
const float TTSRequestBuilder::DEFAULT_SPEED_RATIO = 1.0f;
const int TTSRequestBuilder::DEFAULT_SAMPLE_RATE = 16000;
uint32_t TTSRequestBuilder::requestCounter = 0;

TTSRequestString TTSRequestBuilder::buildSynthesisRequest(
    const TTSRequestString& text,
    const TTSRequestString& appid,
    const TTSRequestString& token,
    const TTSRequestString& cluster,
    const TTSRequestString& uid,
    const TTSRequestString& voiceType,
    const TTSRequestString& encoding,
    int rate,
    float speedRatio
) {
    // Create JSON document with appropriate size for the request
    // 1536 bytes should be sufficient for typical TTS request with text
    DynamicJsonDocument doc(1536);

    // Build app object
    JsonObject app = doc.createNestedObject("app");
    app["appid"] = appid;
    app["token"] = token;
    app["cluster"] = cluster;

    // Build user object
    JsonObject user = doc.createNestedObject("user");
    user["uid"] = uid;

    // Build audio object
    JsonObject audio = doc.createNestedObject("audio");
    audio["voice_type"] = voiceType;
    audio["encoding"] = encoding;
    audio["rate"] = rate;
    audio["speed_ratio"] = speedRatio;

    // Build request object
    JsonObject request = doc.createNestedObject("request");
    request["reqid"] = generateUniqueReqId();
    request["text"] = text;
    request["operation"] = "submit"; // Submit operation for synthesis

    // Serialize JSON to string
    TTSRequestString jsonString;
    size_t serializedSize = serializeJson(doc, jsonString);

    // Check if serialization was successful
    if (serializedSize == 0) {
        // Return empty string on failure
#ifdef ARDUINO
        return "";
#else
        return std::string();
#endif
    }

    return jsonString;
}

TTSRequestString TTSRequestBuilder::generateUniqueReqId() {
    // Get current timestamp
    uint32_t timestamp = 0;
#ifdef ARDUINO
    timestamp = (uint32_t)time(nullptr);
#else
    timestamp = (uint32_t)std::time(nullptr);
#endif

    // Generate random value
    uint32_t randomVal = 0;
#ifdef ARDUINO
    randomVal = esp_random();
#else
    randomVal = (uint32_t)std::rand();
#endif

    // Increment counter (thread-safe on ESP32 with atomic increment)
    uint32_t counter = 0;
#ifdef ARDUINO
    // On ESP32, use atomic increment for thread safety
    counter = __atomic_fetch_add(&requestCounter, 1, __ATOMIC_SEQ_CST);
#else
    counter = requestCounter++;
#endif

    // Build request ID string
    TTSRequestString reqId;

    // Format: "tts_[timestamp]_[random]_[counter]"
#ifdef ARDUINO
    reqId = "tts_" + String(timestamp) + "_" + String(randomVal) + "_" + String(counter);
#else
    reqId = "tts_" + std::to_string(timestamp) + "_" +
            std::to_string(randomVal) + "_" + std::to_string(counter);
#endif

    return reqId;
}