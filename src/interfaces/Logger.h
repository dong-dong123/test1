#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

class Logger {
public:
    enum class Level {
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL
    };

    virtual ~Logger() = default;

    // 日志记录
    virtual void log(Level level, const String& message) = 0;
    virtual void logf(Level level, const char* format, ...) = 0;

    // 日志级别控制
    virtual void setLevel(Level level) = 0;
    virtual Level getLevel() const = 0;

    // 缓冲区管理
    virtual void flush() = 0;
    virtual size_t getBufferUsage() const = 0;

    // 标签支持
    virtual void logWithTag(Level level, const String& tag, const String& message) = 0;
};

#endif