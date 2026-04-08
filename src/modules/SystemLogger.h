#ifndef SYSTEM_LOGGER_H
#define SYSTEM_LOGGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <vector>
#include "../interfaces/Logger.h"
#include "../interfaces/ConfigManager.h"
#include "../globals.h"

// 日志输出类型
enum class LogOutputType {
    SERIAL_OUTPUT,     // 串口输出
    FILE_OUTPUT,       // 文件输出 (SPIFFS)
    NETWORK_OUTPUT,    // 网络输出 (UDP/TCP)
    NONE_OUTPUT        // 无输出
};

// 日志输出器基类
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const String& message) = 0;
    virtual void flush() = 0;
    virtual LogOutputType getType() const = 0;
    virtual bool isAvailable() const = 0;
};

// 系统日志器实现
class SystemLogger : public Logger {
private:
    // 配置相关
    ConfigManager* configManager;
    Level currentLevel;
    std::vector<LogSink*> sinks;

    // 缓冲区管理
    String buffer;
    size_t maxBufferSize;
    bool useBuffer;

    // 内部方法
    String levelToString(Level level) const;
    String formatMessage(Level level, const String& message, const String& tag = "") const;
    bool shouldLog(Level level) const;
    void writeToSinks(const String& formattedMessage);
    void updateFromConfig();

public:
    // 构造函数/析构函数
    SystemLogger(ConfigManager* configMgr = nullptr);
    virtual ~SystemLogger();

    // Logger接口实现
    virtual void log(Level level, const String& message) override;
    virtual void logf(Level level, const char* format, ...) override;
    virtual void setLevel(Level level) override;
    virtual Level getLevel() const override;
    virtual void flush() override;
    virtual size_t getBufferUsage() const override;
    virtual void logWithTag(Level level, const String& tag, const String& message) override;

    // 扩展方法
    void setConfigManager(ConfigManager* configMgr);
    bool addSink(LogSink* sink);
    bool removeSink(LogOutputType type);
    void clearSinks();

    // 配置相关
    void configureFromManager();
    std::vector<String> getActiveOutputs() const;

    // 静态工具方法
    static Level stringToLevel(const String& levelStr);
    static String levelToStringStatic(Level level);
};

// 串口日志输出器
class SerialLogSink : public LogSink {
private:
    Stream* serialPort;
    uint32_t baudRate;

public:
    SerialLogSink(Stream* serial = &Serial, uint32_t baud = 115200);
    virtual ~SerialLogSink() = default;

    virtual void write(const String& message) override;
    virtual void flush() override;
    virtual LogOutputType getType() const override { return LogOutputType::SERIAL_OUTPUT; }
    virtual bool isAvailable() const override;

    void setSerialPort(Stream* serial);
    void setBaudRate(uint32_t baud);
};

// 文件日志输出器（SPIFFS）
class FileLogSink : public LogSink {
private:
    String filePath;
    size_t maxFileSize;
    size_t currentSize;
    File logFile;
    bool fileOpened;

public:
    FileLogSink(const String& path = "/logs/system.log", size_t maxSize = 1024 * 1024); // 默认1MB
    virtual ~FileLogSink();

    virtual void write(const String& message) override;
    virtual void flush() override;
    virtual LogOutputType getType() const override { return LogOutputType::FILE_OUTPUT; }
    virtual bool isAvailable() const override;

    bool openFile();
    void closeFile();
    bool rotateLogIfNeeded();
    size_t getCurrentFileSize() const { return currentSize; }
};

// 网络日志输出器（UDP）
class NetworkLogSink : public LogSink {
private:
    String serverHost;
    uint16_t serverPort;
    bool initialized;

public:
    NetworkLogSink(const String& host = "192.168.1.100", uint16_t port = 514); // 默认Syslog端口
    virtual ~NetworkLogSink() = default;

    virtual void write(const String& message) override;
    virtual void flush() override;
    virtual LogOutputType getType() const override { return LogOutputType::NETWORK_OUTPUT; }
    virtual bool isAvailable() const override;

    void setServer(const String& host, uint16_t port);
    bool initialize();
};

#endif // SYSTEM_LOGGER_H