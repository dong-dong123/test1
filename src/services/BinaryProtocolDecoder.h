/**
 * @file BinaryProtocolDecoder.h
 * @brief WebSocket binary protocol decoder for Volcano speech recognition service
 * @details Implements the binary protocol for decoding Full Server Response and
 *          Error Message messages according to Volcano WebSocket specification.
 * @version 1.0
 * @date 2026-04-09
 */

#ifndef BINARY_PROTOCOL_DECODER_H
#define BINARY_PROTOCOL_DECODER_H

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

/**
 * @class BinaryProtocolDecoder
 * @brief Decoder for Volcano WebSocket binary protocol messages

 * This class provides static methods to decode binary messages received from
 * the Volcano speech recognition WebSocket API. It handles parsing of protocol
 * headers and extraction of payload data.
 */
class BinaryProtocolDecoder {
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
     * @struct DecodedMessage
     * @brief Structure containing decoded message components

     * This struct holds all the fields extracted from a binary protocol message,
     * including header information and payload data.

     * @note This struct provides default copy constructor and copy assignment
     *       operator. The payload vector will be deep-copied during these
     *       operations. For move semantics, use std::move() to avoid copying
     *       the payload data.
     */
    struct DecodedMessage {
        uint8_t version;                  ///< Protocol version (4 bits)
        uint8_t headerSize;               ///< Header size in bytes (4 bits, values: 4 or 8)
        MessageType messageType;          ///< Message type (4 bits)
        uint8_t flags;                    ///< Message flags (4 bits)
        SerializationMethod serialization;///< Serialization method (4 bits)
        CompressionMethod compression;    ///< Compression method (4 bits)
        uint32_t sequence;                ///< Sequence number for multi-part messages
        uint32_t payloadSize;             ///< Size of payload data in bytes
        std::vector<uint8_t> payload;     ///< Payload data (JSON or binary)

        /// @brief Default constructor
        DecodedMessage()
            : version(0), headerSize(0), messageType(MessageType::FULL_CLIENT_REQUEST), flags(0),
              serialization(SerializationMethod::NONE), compression(CompressionMethod::NONE),
              sequence(0), payloadSize(0) {}
    };

    /**
     * @brief Decode a binary protocol message
     * @param data Pointer to binary message data
     * @param length Size of binary message in bytes
     * @return DecodedMessage structure containing parsed message components
     * @throws std::invalid_argument if data is null or length is insufficient

     * This method parses the binary protocol message according to the Volcano
     * WebSocket specification. It extracts header fields, payload size, and
     * payload data. The minimum valid message length is 8 bytes (4-byte header
     * + 4-byte payload size).
     */
    static DecodedMessage decode(const uint8_t* data, size_t length);

    /**
     * @brief Read a 32-bit integer in big-endian format
     * @param data Pointer to the start of the 32-bit integer
     * @return 32-bit integer value in host byte order

     * This helper function reads a 32-bit integer stored in big-endian format
     * and converts it to the host's native byte order.
     */
    static uint32_t readUint32BigEndian(const uint8_t* data);

    /**
     * @brief Extract text from response payload
     * @param payload Binary payload data from a Full Server Response message
     * @return Extracted text as a string

     * This method extracts the recognition text from a Full Server Response
     * payload. The payload is expected to contain JSON data with the structure:
     * {
     *   "result": {
     *     "text": "recognized text",
     *     "utterances": [...]
     *   },
     *   "audio_info": {
     *     "duration": 1000
     *   }
     * }

     * @note This is currently a stub implementation that returns an empty string.
     *       Full JSON parsing will be implemented in a separate component.
     * @warning This is a temporary implementation and should be replaced with
     *          proper JSON parsing.
     */
    static BinaryString extractTextFromResponse(const std::vector<uint8_t>& payload);

private:
    /**
     * @brief Extract high 4 bits from a byte
     * @param byte Input byte
     * @return High 4 bits of the byte
     */
    static uint8_t extractHighBits(uint8_t byte);

    /**
     * @brief Extract low 4 bits from a byte
     * @param byte Input byte
     * @return Low 4 bits of the byte
     */
    static uint8_t extractLowBits(uint8_t byte);

    /**
     * @brief Minimum valid message length (header + payload size)
     */
    static const size_t MIN_MESSAGE_LENGTH = 8;

    /**
     * @brief Basic header size (without sequence field)
     */
    static const size_t BASIC_HEADER_SIZE = 4;

    /**
     * @brief Extended header size (with sequence field)
     */
    static const size_t EXTENDED_HEADER_SIZE = 8;

    /**
     * @brief Size of payload size field in bytes
     */
    static const size_t PAYLOAD_SIZE_FIELD_SIZE = 4;
};

#endif // BINARY_PROTOCOL_DECODER_H