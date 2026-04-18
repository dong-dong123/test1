/**
 * @file TTSResponseParser.cpp
 * @brief Implementation of TTSResponseParser for parsing WebSocket TTS JSON responses
 * @version 1.0
 * @date 2026-04-09
 */

#include "TTSResponseParser.h"
#include <ArduinoJson.h>

// String functions for base64 decoding
#ifdef ARDUINO
#include <string.h>
#else
#include <cstring>
#endif

// base64 decoding implemented inline

TTSResponseParser::SynthesisResult TTSResponseParser::parseResponse(
    const TTSResponseString& jsonResponse
) {
    SynthesisResult result;

    // Check for empty response
    if (jsonResponse.isEmpty()) {
        result.message = "Empty response";
        return result;
    }

    // Parse JSON document
    // Use appropriate size for TTS response (2KB should be sufficient)
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonResponse);

    if (error) {
        result.message = "JSON parse error: " + TTSResponseString(error.c_str());
        return result;
    }

    // Extract top-level fields
    result.reqid = doc["reqid"] | "";
    result.code = doc["code"] | 0;
    result.message = doc["message"] | "";
    result.sequence = doc["sequence"] | -1;

    // Check if synthesis was successful
    result.success = (result.code == SUCCESS_CODE);

    // Extract base64 audio data
    const char* base64Data = doc["data"];
    if (base64Data && result.success) {
        result.audioData = decodeBase64Audio(TTSResponseString(base64Data));
    }

    // Parse addition object for duration
    JsonObject addition = doc["addition"];
    if (!addition.isNull()) {
        parseAdditionObject(&addition, result);
    }

    return result;
}

std::vector<uint8_t> TTSResponseParser::decodeBase64Audio(
    const TTSResponseString& base64Data
) {
    std::vector<uint8_t> decodedData;

    if (base64Data.isEmpty()) {
        return decodedData;
    }

    // Simple base64 decoding implementation
    const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int encodedLength = base64Data.length();
    int i = 0;
    int j = 0;
    uint8_t charArray4[4], charArray3[3];

    decodedData.reserve((encodedLength * 3) / 4);

    while (encodedLength-- && base64Data[i] != '=') {
        charArray4[j++] = base64Data[i++];
        if (j == 4) {
            for (j = 0; j < 4; j++) {
                const char* pos = strchr(base64Chars, charArray4[j]);
                charArray4[j] = (pos != nullptr) ? (pos - base64Chars) : 0;
            }

            charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
            charArray3[1] = ((charArray4[1] & 0x0F) << 4) + ((charArray4[2] & 0x3C) >> 2);
            charArray3[2] = ((charArray4[2] & 0x03) << 6) + charArray4[3];

            for (j = 0; j < 3; j++) {
                decodedData.push_back(charArray3[j]);
            }
            j = 0;
        }
    }

    if (j > 0) {
        for (int k = j; k < 4; k++) {
            charArray4[k] = 0;
        }

        for (int k = 0; k < 4; k++) {
            const char* pos = strchr(base64Chars, charArray4[k]);
            charArray4[k] = (pos != nullptr) ? (pos - base64Chars) : 0;
        }

        charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
        charArray3[1] = ((charArray4[1] & 0x0F) << 4) + ((charArray4[2] & 0x3C) >> 2);
        charArray3[2] = ((charArray4[2] & 0x03) << 6) + charArray4[3];

        for (int k = 0; k < j - 1; k++) {
            decodedData.push_back(charArray3[k]);
        }
    }

    return decodedData;
}

bool TTSResponseParser::parseAdditionObject(const void* additionObj, SynthesisResult& result) {
    // Cast to JsonObject (ArduinoJson)
    const JsonObject& addition = *(static_cast<const JsonObject*>(additionObj));

    // Extract duration field (may be string or integer)
    if (addition.containsKey("duration")) {
        if (addition["duration"].is<const char*>()) {
            // Duration as string
            const char* durationStr = addition["duration"];
            result.durationMs = atoi(durationStr);
        } else if (addition["duration"].is<int>()) {
            // Duration as integer
            result.durationMs = addition["duration"];
        } else {
            result.durationMs = 0;
        }
    } else {
        result.durationMs = 0;
    }

    return true;
}