#ifndef SPEECH_SERVICE_H
#define SPEECH_SERVICE_H

#include <Arduino.h>
#include <vector>

class SpeechService {
public:
    virtual ~SpeechService() = default;

    // 语音识别
    virtual bool recognize(const uint8_t* audio_data, size_t length, String& text) = 0;

    // 流式语音识别
    virtual bool recognizeStreamStart() = 0;
    virtual bool recognizeStreamChunk(const uint8_t* audio_chunk, size_t chunk_size, String& partial_text) = 0;
    virtual bool recognizeStreamEnd(String& final_text) = 0;

    // 语音合成
    virtual bool synthesize(const String& text, std::vector<uint8_t>& audio_data) = 0;

    // 流式语音合成
    virtual bool synthesizeStreamStart(const String& text) = 0;
    virtual bool synthesizeStreamGetChunk(std::vector<uint8_t>& chunk, bool& is_last) = 0;

    // 服务信息
    virtual String getName() const = 0;
    virtual bool isAvailable() const = 0;
    virtual float getCostPerRequest() const = 0;
};

#endif