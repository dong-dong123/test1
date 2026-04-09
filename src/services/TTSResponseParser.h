/**
 * @file TTSResponseParser.h
 * @brief Parser for Volcano WebSocket Text-to-Speech (TTS) JSON responses
 * @details Parses JSON responses from Volcano speech synthesis service and extracts
 *          audio data and metadata according to the WebSocket binary protocol specification.
 * @version 1.0
 * @date 2026-04-09
 */

#ifndef TTS_RESPONSE_PARSER_H
#define TTS_RESPONSE_PARSER_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cstdint>
#include <string>
#endif

#include <vector>

/// @brief Platform-independent string type
#ifdef ARDUINO
using TTSResponseString = String;
#else
using TTSResponseString = std::string;
#endif

class TTSResponseParser {
public:
    /**
     * @brief Structure containing parsed TTS synthesis result
     */
    struct SynthesisResult {
        TTSResponseString reqid;           ///< Request ID matching the original request
        int code;                          ///< Response status code (3000 = success)
        TTSResponseString message;         ///< Human-readable status message
        int sequence;                      ///< Sequence number for multi-part responses
        std::vector<uint8_t> audioData;    ///< Decoded audio binary data
        int durationMs;                    ///< Audio duration in milliseconds
        bool success;                      ///< Whether synthesis was successful (code == 3000)

        /// @brief Default constructor
        SynthesisResult()
            : code(0), sequence(0), durationMs(0), success(false) {}
    };

    /**
     * @brief Parse TTS synthesis response JSON
     * @param jsonResponse JSON string received from Volcano TTS API
     * @return SynthesisResult structure containing parsed data
     * @note Returns success=false if parsing fails or response indicates error
     */
    static SynthesisResult parseResponse(const TTSResponseString& jsonResponse);

    /**
     * @brief Decode base64-encoded audio data from TTS response
     * @param base64Data Base64-encoded audio data string
     * @return Vector containing decoded binary audio data
     * @note Returns empty vector if decoding fails
     */
    static std::vector<uint8_t> decodeBase64Audio(const TTSResponseString& base64Data);

private:
    /**
     * @brief Parse addition object from JSON response
     * @param additionObj JSON object containing addition fields
     * @param result Reference to SynthesisResult to update
     * @return true if parsing succeeded, false otherwise
     */
    static bool parseAdditionObject(const void* additionObj, SynthesisResult& result);

    // Success code constant
    static const int SUCCESS_CODE = 3000;
};

#endif // TTS_RESPONSE_PARSER_H