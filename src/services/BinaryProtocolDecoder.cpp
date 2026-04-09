/**
 * @file BinaryProtocolDecoder.cpp
 * @brief WebSocket binary protocol decoder implementation for Volcano speech recognition service
 * @details Implements the binary protocol for decoding Full Server Response and
 *          Error Message messages according to Volcano WebSocket specification.
 * @version 1.0
 * @date 2026-04-09
 */

#include "BinaryProtocolDecoder.h"
#include <algorithm>
#include <stdexcept>
#ifdef ARDUINO
#include <ArduinoJson.h>
#endif

// Decode binary protocol message
BinaryProtocolDecoder::DecodedMessage BinaryProtocolDecoder::decode(const uint8_t* data, size_t length) {
    // Validate input parameters
    if (data == nullptr) {
        throw std::invalid_argument("BinaryProtocolDecoder::decode: data pointer is null");
    }

    if (length < MIN_MESSAGE_LENGTH) {
        throw std::invalid_argument("BinaryProtocolDecoder::decode: message length too short");
    }

    DecodedMessage decoded;

    // Parse header byte 0: version (high 4 bits) and header size (low 4 bits)
    decoded.version = extractHighBits(data[0]);
    decoded.headerSize = extractLowBits(data[0]);

    // Parse header byte 1: message type (high 4 bits) and flags (low 4 bits)
    decoded.messageType = static_cast<MessageType>(extractHighBits(data[1]));
    decoded.flags = extractLowBits(data[1]);

    // Parse header byte 2: serialization (high 4 bits) and compression (low 4 bits)
    decoded.serialization = static_cast<SerializationMethod>(extractHighBits(data[2]));
    decoded.compression = static_cast<CompressionMethod>(extractLowBits(data[2]));

    // Byte 3 is reserved (currently unused) - must be 0x00
    if (data[3] != 0x00) {
        throw std::invalid_argument("BinaryProtocolDecoder::decode: reserved byte must be 0x00");
    }

    // Calculate actual header size including optional sequence field
    size_t currentPos = BASIC_HEADER_SIZE; // Base header size is 4 bytes

    // Validate header size (should be 4 or 8 bytes)
    if (decoded.headerSize != BASIC_HEADER_SIZE && decoded.headerSize != EXTENDED_HEADER_SIZE) {
        throw std::invalid_argument("BinaryProtocolDecoder::decode: invalid header size");
    }

    // Check if sequence field is present (header size 8 indicates extended header with sequence)
    if (decoded.headerSize == EXTENDED_HEADER_SIZE) {
        // Extended header with sequence field
        if (length < currentPos + PAYLOAD_SIZE_FIELD_SIZE) {
            throw std::invalid_argument("BinaryProtocolDecoder::decode: message too short for sequence field");
        }
        decoded.sequence = readUint32BigEndian(&data[currentPos]);
        currentPos += PAYLOAD_SIZE_FIELD_SIZE;
    } else {
        // Basic header without sequence
        decoded.sequence = 0;
    }

    // Read payload size (4 bytes, big-endian)
    if (length < currentPos + PAYLOAD_SIZE_FIELD_SIZE) {
        throw std::invalid_argument("BinaryProtocolDecoder::decode: message too short for payload size");
    }
    decoded.payloadSize = readUint32BigEndian(&data[currentPos]);
    currentPos += PAYLOAD_SIZE_FIELD_SIZE;

    // Validate payload size
    if (decoded.payloadSize > 0) {
        if (length < currentPos + decoded.payloadSize) {
            throw std::invalid_argument("BinaryProtocolDecoder::decode: message too short for payload");
        }

        // Copy payload data
        decoded.payload.resize(decoded.payloadSize);
        std::copy(data + currentPos, data + currentPos + decoded.payloadSize, decoded.payload.begin());
    }

    return decoded;
}

// Read 32-bit integer in big-endian format
uint32_t BinaryProtocolDecoder::readUint32BigEndian(const uint8_t* data) {
    if (data == nullptr) {
        throw std::invalid_argument("BinaryProtocolDecoder::readUint32BigEndian: data pointer is null");
    }

    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

// Extract text from response payload
BinaryString BinaryProtocolDecoder::extractTextFromResponse(const std::vector<uint8_t>& payload) {
    if (payload.empty()) {
        return BinaryString();
    }

    // Convert payload to string
    BinaryString jsonString;
    jsonString.reserve(payload.size() + 1);
    for (uint8_t byte : payload) {
        jsonString += static_cast<char>(byte);
    }

    // Parse JSON
    #ifdef ARDUINO
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error) {
        // Log error if possible
        #ifdef ESP_LOGE
        ESP_LOGE("BinaryProtocolDecoder", "Failed to parse JSON: %s", error.c_str());
        #endif
        return BinaryString();
    }

    // Extract recognition text (based on Volcano API response format)
    // Possible formats:
    // 1. {"result": {"text": "...", "utterances": [...]}, "audio_info": {...}}
    // 2. {"text": "..."}
    // 3. {"data": "..."}
    BinaryString recognizedText;

    if (doc.containsKey("result")) {
        JsonObject result = doc["result"];
        if (result.containsKey("text")) {
            recognizedText = result["text"].as<BinaryString>();
        }
    } else if (doc.containsKey("text")) {
        recognizedText = doc["text"].as<BinaryString>();
    } else if (doc.containsKey("data") && doc["data"].is<BinaryString>()) {
        recognizedText = doc["data"].as<BinaryString>();
    }

    return recognizedText;
    #else
    // For non-Arduino environments, use a different JSON parser
    // This is a simplified implementation
    // In production, use a proper JSON library like nlohmann/json
    // For now, return empty string
    (void)jsonString;
    return BinaryString();
    #endif
}

// Extract high 4 bits from a byte
uint8_t BinaryProtocolDecoder::extractHighBits(uint8_t byte) {
    return (byte >> 4) & 0x0F;
}

// Extract low 4 bits from a byte
uint8_t BinaryProtocolDecoder::extractLowBits(uint8_t byte) {
    return byte & 0x0F;
}