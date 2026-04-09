/**
 * @file BinaryProtocolEncoder.h
 * @brief WebSocket binary protocol encoder for Volcano speech recognition service
 * @details Implements the binary protocol for encoding Full Client Request and
 *          Audio Only Request messages according to Volcano WebSocket specification.
 * @version 1.0
 * @date 2026-04-08
 */

#ifndef BINARY_PROTOCOL_ENCODER_H
#define BINARY_PROTOCOL_ENCODER_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cstdint>
#include <cstddef>
#include <string>
#endif

#include <vector>

/// @brief Platform-independent string type for binary protocol
#ifdef ARDUINO
using BinaryString = String;
#else
using BinaryString = std::string;
#endif

class BinaryProtocolEncoder {
public:
    /// @brief Message type definitions for WebSocket binary protocol
    enum class MessageType : uint8_t {
        FULL_CLIENT_REQUEST = 0b0001,    ///< Full client request with JSON metadata
        AUDIO_ONLY_REQUEST = 0b0010,     ///< Audio data only request
        FULL_SERVER_RESPONSE = 0b1001,   ///< Server response with recognition results
        ERROR_MESSAGE = 0b1111           ///< Error message from server
    };

    /// @brief Serialization method definitions
    enum class SerializationMethod : uint8_t {
        NONE = 0b0000,    ///< No serialization (raw binary data)
        JSON = 0b0001     ///< JSON serialization
    };

    /// @brief Compression method definitions
    enum class CompressionMethod : uint8_t {
        NONE = 0b0000,    ///< No compression
        GZIP = 0b0001     ///< GZIP compression
    };

    /**
     * @brief Encode a Full Client Request message
     * @param jsonRequest JSON string containing request metadata
     * @param useCompression Whether to use GZIP compression (default: false)
     * @param sequence Sequence number for multi-part messages (default: 0)
     * @return Vector containing encoded binary message
     * @note The JSON request should follow Volcano API specification
     */
    static std::vector<uint8_t> encodeFullClientRequest(
        const BinaryString& jsonRequest,
        bool useCompression = false,
        uint32_t sequence = 0
    );

    /**
     * @brief Encode an Audio Only Request message
     * @param audioData Pointer to audio data buffer
     * @param length Size of audio data in bytes
     * @param isLastChunk Whether this is the last audio chunk (default: false)
     * @param useCompression Whether to use GZIP compression (default: false)
     * @param sequence Sequence number for multi-part messages (default: 0)
     * @return Vector containing encoded binary message
     */
    static std::vector<uint8_t> encodeAudioOnlyRequest(
        const uint8_t* audioData,
        size_t length,
        bool isLastChunk = false,
        bool useCompression = false,
        uint32_t sequence = 0
    );

private:
    /**
     * @brief Build protocol header
     * @param messageType Message type (4 bits)
     * @param flags Message flags (4 bits)
     * @param serialization Serialization method (4 bits)
     * @param compression Compression method (4 bits)
     * @param sequence Optional sequence number (default: 0)
     * @return Header bytes
     * @note Header format: [Version:4][HeaderSize:4][MessageType:4][Flags:4][Serialization:4][Compression:4][Reserved:8]
     */
    static std::vector<uint8_t> buildHeader(
        uint8_t messageType,
        uint8_t flags,
        uint8_t serialization,
        uint8_t compression,
        uint32_t sequence = 0
    );

    /// @brief Write 32-bit integer in big-endian format
    static void writeUint32BigEndian(std::vector<uint8_t>& buffer, uint32_t value);

    /// @brief Build byte from high and low 4-bit values
    static uint8_t buildByte(uint8_t highBits, uint8_t lowBits);
};

#endif // BINARY_PROTOCOL_ENCODER_H