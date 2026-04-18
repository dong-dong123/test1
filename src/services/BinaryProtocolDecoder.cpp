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
#include "esp_log.h"
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
    uint8_t headerSizeField = extractLowBits(data[0]);

    // Convert header size field to bytes
    // According to Volcano WebSocket binary protocol, header size field may represent:
    // 1 = 4 bytes (basic header), 2 = 8 bytes (extended header with sequence)
    // If value is less than 4, assume it's in 4-byte units, otherwise assume bytes
    if (headerSizeField < 4) {
        decoded.headerSize = headerSizeField * 4; // Convert to bytes
    } else {
        decoded.headerSize = headerSizeField; // Already in bytes
    }

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

    // Special handling for error messages (type 0b1111)
    if (decoded.messageType == MessageType::ERROR_MESSAGE) {
        // Error message format: header + errorCode (4 bytes) + messageLength (4 bytes) + message
        size_t currentPos = BASIC_HEADER_SIZE; // Skip 4-byte header

        // Check if we have enough bytes for error code and message length
        if (length < currentPos + 8) {
            throw std::invalid_argument("BinaryProtocolDecoder::decode: error message too short for error code and message length");
        }

        // Read error code (4 bytes)
        uint32_t errorCode = readUint32BigEndian(&data[currentPos]);
        currentPos += 4;

        // Read message length (4 bytes)
        uint32_t messageLength = readUint32BigEndian(&data[currentPos]);
        currentPos += 4;

        // Validate message length
        if (messageLength == 0) {
            // Empty error message is valid
            decoded.payloadSize = 8; // errorCode + messageLength
            decoded.payload.resize(8);
            // Store error code and message length in payload
            std::copy(data + BASIC_HEADER_SIZE, data + BASIC_HEADER_SIZE + 8, decoded.payload.begin());
            return decoded;
        }

        // Check if we have enough bytes for the message
        if (length < currentPos + messageLength) {
            throw std::invalid_argument("BinaryProtocolDecoder::decode: error message too short for message content");
        }

        // Total payload size: errorCode (4) + messageLength (4) + message
        decoded.payloadSize = 8 + messageLength;
        decoded.payload.resize(decoded.payloadSize);
        std::copy(data + BASIC_HEADER_SIZE, data + BASIC_HEADER_SIZE + decoded.payloadSize, decoded.payload.begin());

        return decoded;
    }

    // Calculate actual header size including optional sequence field
    size_t currentPos = BASIC_HEADER_SIZE; // Base header size is 4 bytes

    // Validate header size (should be 4 or 8 bytes after conversion)
    if (decoded.headerSize != BASIC_HEADER_SIZE && decoded.headerSize != EXTENDED_HEADER_SIZE) {
        ESP_LOGE("BinaryProtocolDecoder", "Invalid header size: %u (field value: %u)", decoded.headerSize, headerSizeField);
        throw std::invalid_argument("BinaryProtocolDecoder::decode: invalid header size");
    }

    // Check if sequence field is present based on flags (not header size)
    if (decoded.flags & FLAG_SEQUENCE_PRESENT) {
        // Sequence field present (extended header)
        // Note: Server may send incorrect headerSizeField=1 but still include sequence
        // We'll read sequence field regardless of headerSize
        if (length < currentPos + PAYLOAD_SIZE_FIELD_SIZE) {
            throw std::invalid_argument("BinaryProtocolDecoder::decode: message too short for sequence field");
        }
        decoded.sequence = readUint32BigEndian(&data[currentPos]);
        currentPos += PAYLOAD_SIZE_FIELD_SIZE;
        // Adjust header size to reflect actual header (8 bytes)
        decoded.headerSize = EXTENDED_HEADER_SIZE;
    } else {
        // Basic header without sequence
        decoded.sequence = 0;
    }

    // Note: Session ID field removed per Volcano customer service guidance
    // ASR 1.0 protocol does not include session_id field in server responses

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

// Simple decode method based on DoubaoASR reference implementation
bool BinaryProtocolDecoder::decodeSimple(const uint8_t* data, size_t length, uint8_t& messageType, uint32_t& sequence, std::vector<uint8_t>& payload) {
    // Minimum message length: 4-byte header + 4-byte payload size = 8 bytes
    if (data == nullptr || length < 8) {
        return false;
    }

    // Extract message type from byte 1 high 4 bits (reference: response[1] >> 4)
    messageType = extractHighBits(data[1]);

    // Extract sequence from byte 1 low 4 bits? Actually sequence is in extended header.
    // Reference code doesn't seem to extract sequence from header, but from JSON payload.
    // For now, set sequence to 0.
    sequence = 0;

    // Check if header has sequence field (byte 0 low 4 bits indicates header size)
    uint8_t headerSizeField = extractLowBits(data[0]);
    size_t headerSize;
    if (headerSizeField < 4) {
        headerSize = headerSizeField * 4; // Convert to bytes
    } else {
        headerSize = headerSizeField; // Already in bytes
    }

    // Validate header size (should be 4 or 8 bytes)
    if (headerSize != 4 && headerSize != 8) {
        return false;
    }

    // Position after header
    size_t pos = headerSize;

    // Check if we have enough bytes for payload size
    if (length < pos + 4) {
        return false;
    }

    // Read payload size (4 bytes, big-endian)
    uint32_t payloadSize = readUint32BigEndian(&data[pos]);
    pos += 4;

    // Validate payload size
    if (payloadSize == 0) {
        // Empty payload is valid
        payload.clear();
        return true;
    }

    // Check if we have enough bytes for payload
    if (length < pos + payloadSize) {
        return false;
    }

    // Copy payload data
    payload.resize(payloadSize);
    std::copy(data + pos, data + pos + payloadSize, payload.begin());

    return true;
}

// Extract high 4 bits from a byte
uint8_t BinaryProtocolDecoder::extractHighBits(uint8_t byte) {
    return (byte >> 4) & 0x0F;
}

// Extract low 4 bits from a byte
uint8_t BinaryProtocolDecoder::extractLowBits(uint8_t byte) {
    return byte & 0x0F;
}