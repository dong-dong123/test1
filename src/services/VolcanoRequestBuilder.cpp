/**
 * @file VolcanoRequestBuilder.cpp
 * @brief Implementation of VolcanoRequestBuilder for creating WebSocket JSON requests
 * @version 1.0
 * @date 2026-04-09
 */

#include "VolcanoRequestBuilder.h"
#include <ArduinoJson.h>

#ifdef ARDUINO
#include <time.h>
#include <esp_random.h>
#else
#include <ctime>
#include <cstdlib>
#endif

// Static member initialization
const char* VolcanoRequestBuilder::DEFAULT_PLATFORM = "ESP32";
const char* VolcanoRequestBuilder::DEFAULT_SDK_VERSION = "1.0";
const char* VolcanoRequestBuilder::DEFAULT_MODEL_NAME = "bigmodel";
uint32_t VolcanoRequestBuilder::requestCounter = 0;

RequestString VolcanoRequestBuilder::buildFullClientRequest(
    const RequestString& uid,
    const RequestString& language,
    bool enablePunctuation,
    bool enableITN,
    bool enableDDC,
    const RequestString& format,
    int rate,
    int bits,
    int channel
) {
    // Create JSON document with appropriate size for the request
    // 1024 bytes should be sufficient for typical request
    DynamicJsonDocument doc(1024);

    // Build user object
    JsonObject user = doc.createNestedObject("user");
    user["uid"] = uid;
    user["platform"] = DEFAULT_PLATFORM;
    user["sdk_version"] = DEFAULT_SDK_VERSION;

    // Build audio object
    JsonObject audio = doc.createNestedObject("audio");
    audio["format"] = format;
    audio["rate"] = rate;
    audio["bits"] = bits;
    audio["channel"] = channel;
    audio["language"] = language;

    // Build request object
    JsonObject request = doc.createNestedObject("request");
    request["reqid"] = generateUniqueReqId();
    request["model_name"] = DEFAULT_MODEL_NAME;
    request["enable_itn"] = enableITN;
    request["enable_punc"] = enablePunctuation;
    request["enable_ddc"] = enableDDC;

    // Serialize JSON to string
    RequestString jsonString;
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

RequestString VolcanoRequestBuilder::generateUniqueReqId() {
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
    RequestString reqId;

    // Format: "esp32_[timestamp]_[random]_[counter]"
#ifdef ARDUINO
    reqId = "esp32_" + String(timestamp) + "_" + String(randomVal) + "_" + String(counter);
#else
    reqId = "esp32_" + std::to_string(timestamp) + "_" +
            std::to_string(randomVal) + "_" + std::to_string(counter);
#endif

    return reqId;
}