#ifndef DIALOGUE_SERVICE_H
#define DIALOGUE_SERVICE_H

#include <Arduino.h>
#include <vector>

class DialogueService {
public:
    virtual ~DialogueService() = default;

    // 单轮对话
    virtual String chat(const String& input) = 0;

    // 多轮对话（带上下文）
    virtual String chatWithContext(const String& input, const std::vector<String>& context) = 0;

    // 流式对话
    virtual bool chatStreamStart(const String& input) = 0;
    virtual bool chatStreamGetChunk(String& chunk, bool& is_last) = 0;

    // 服务信息
    virtual String getName() const = 0;
    virtual bool isAvailable() const = 0;
    virtual float getCostPerRequest() const = 0;

    // 上下文管理
    virtual void clearContext() = 0;
    virtual size_t getContextSize() const = 0;
};

#endif