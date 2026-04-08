#include "SystemLogger.h"
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// ============================================================================
// SystemLogger 实现
// ============================================================================

SystemLogger::SystemLogger(ConfigManager* configMgr)
    : configManager(configMgr), currentLevel(Level::INFO),
      maxBufferSize(LOG_BUFFER_SIZE), useBuffer(false) {

    // 默认添加串口输出
    addSink(new SerialLogSink());

    // 如果提供了配置管理器，从配置更新设置
    if (configManager) {
        updateFromConfig();
    }
}

SystemLogger::~SystemLogger() {
    // 清理所有输出器
    for (auto sink : sinks) {
        delete sink;
    }
    sinks.clear();
}

String SystemLogger::levelToString(Level level) const {
    switch (level) {
        case Level::DEBUG: return "DEBUG";
        case Level::INFO:  return "INFO";
        case Level::WARN:  return "WARN";
        case Level::ERROR: return "ERROR";
        case Level::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

String SystemLogger::formatMessage(Level level, const String& message, const String& tag) const {
    // 格式: [时间] [级别] [标签] 消息
    // 对于ESP32，我们可以使用millis()作为时间戳
    unsigned long timestamp = millis();
    String formatted;

    formatted += "[";
    formatted += String(timestamp);
    formatted += "] ";

    formatted += "[";
    formatted += levelToString(level);
    formatted += "] ";

    if (!tag.isEmpty()) {
        formatted += "[";
        formatted += tag;
        formatted += "] ";
    }

    formatted += message;

    return formatted;
}

bool SystemLogger::shouldLog(Level level) const {
    // 日志级别过滤：只记录级别高于等于当前设置级别的日志
    // 级别顺序: DEBUG < INFO < WARN < ERROR < FATAL
    return static_cast<int>(level) >= static_cast<int>(currentLevel);
}

void SystemLogger::writeToSinks(const String& formattedMessage) {
    // 如果使用缓冲区，先缓存
    if (useBuffer && buffer.length() + formattedMessage.length() + 1 <= maxBufferSize) {
        buffer += formattedMessage;
        buffer += '\n';
    } else if (useBuffer) {
        // 缓冲区已满，先刷新
        flush();
        buffer += formattedMessage;
        buffer += '\n';
    }

    // 写入所有可用的输出器
    for (auto sink : sinks) {
        if (sink && sink->isAvailable()) {
            sink->write(formattedMessage);
        }
    }
}

void SystemLogger::updateFromConfig() {
    if (!configManager) return;

    // 从配置管理器读取日志级别
    String levelStr = configManager->getString("logging.level", "INFO");
    currentLevel = stringToLevel(levelStr);

    // 读取输出配置
    std::vector<String> outputs = configManager->getStringArray("logging.output");

    // 清空现有输出器（除了串口，因为可能正在使用）
    // 这里简化处理：我们只管理输出器列表，由调用者负责配置
    // 实际应用中可能需要更智能的输出器管理
}

void SystemLogger::log(Level level, const String& message) {
    if (!shouldLog(level)) return;

    String formatted = formatMessage(level, message);
    writeToSinks(formatted);
}

void SystemLogger::logf(Level level, const char* format, ...) {
    if (!shouldLog(level)) return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    log(level, String(buffer));
}

void SystemLogger::setLevel(Level level) {
    currentLevel = level;
}

Logger::Level SystemLogger::getLevel() const {
    return currentLevel;
}

void SystemLogger::flush() {
    // 刷新所有输出器
    for (auto sink : sinks) {
        if (sink && sink->isAvailable()) {
            sink->flush();
        }
    }

    // 清空缓冲区
    buffer = "";
}

size_t SystemLogger::getBufferUsage() const {
    return buffer.length();
}

void SystemLogger::logWithTag(Level level, const String& tag, const String& message) {
    if (!shouldLog(level)) return;

    String formatted = formatMessage(level, message, tag);
    writeToSinks(formatted);
}

void SystemLogger::setConfigManager(ConfigManager* configMgr) {
    configManager = configMgr;
    if (configManager) {
        updateFromConfig();
    }
}

bool SystemLogger::addSink(LogSink* sink) {
    if (!sink) return false;

    // 检查是否已存在相同类型的输出器
    for (auto existing : sinks) {
        if (existing->getType() == sink->getType()) {
            // 已存在，不重复添加
            delete sink;
            return false;
        }
    }

    sinks.push_back(sink);
    return true;
}

bool SystemLogger::removeSink(LogOutputType type) {
    for (auto it = sinks.begin(); it != sinks.end(); ++it) {
        if ((*it)->getType() == type) {
            delete *it;
            sinks.erase(it);
            return true;
        }
    }
    return false;
}

void SystemLogger::clearSinks() {
    for (auto sink : sinks) {
        delete sink;
    }
    sinks.clear();
}

void SystemLogger::configureFromManager() {
    if (!configManager) return;

    // 清空现有输出器（除了串口？这里简化处理：全部清空）
    clearSinks();

    // 读取输出配置
    std::vector<String> outputs = configManager->getStringArray("logging.output");

    // 根据配置创建输出器
    for (const String& output : outputs) {
        if (output == "serial") {
            addSink(new SerialLogSink());
        } else if (output == "file") {
            addSink(new FileLogSink());
        } else if (output == "network") {
            addSink(new NetworkLogSink());
        }
    }

    // 更新日志级别
    updateFromConfig();
}

std::vector<String> SystemLogger::getActiveOutputs() const {
    std::vector<String> result;
    for (auto sink : sinks) {
        switch (sink->getType()) {
            case LogOutputType::SERIAL_OUTPUT: result.push_back("serial"); break;
            case LogOutputType::FILE_OUTPUT: result.push_back("file"); break;
            case LogOutputType::NETWORK_OUTPUT: result.push_back("network"); break;
            default: break;
        }
    }
    return result;
}

Logger::Level SystemLogger::stringToLevel(const String& levelStr) {
    String upper = levelStr;
    upper.toUpperCase();

    if (upper == "DEBUG") return Level::DEBUG;
    if (upper == "INFO") return Level::INFO;
    if (upper == "WARN" || upper == "WARNING") return Level::WARN;
    if (upper == "ERROR") return Level::ERROR;
    if (upper == "FATAL") return Level::FATAL;

    // 默认返回INFO
    return Level::INFO;
}

String SystemLogger::levelToStringStatic(Level level) {
    switch (level) {
        case Level::DEBUG: return "DEBUG";
        case Level::INFO:  return "INFO";
        case Level::WARN:  return "WARN";
        case Level::ERROR: return "ERROR";
        case Level::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// SerialLogSink 实现
// ============================================================================

SerialLogSink::SerialLogSink(Stream* serial, uint32_t baud)
    : serialPort(serial), baudRate(baud) {

    if (serialPort) {
        // 串口应在外部初始化，例如在 setup() 中调用 Serial.begin()
        // serialPort->begin(baudRate);
        // delay(100);
    }
}

void SerialLogSink::write(const String& message) {
    if (serialPort && serialPort->availableForWrite() > 0) {
        serialPort->println(message);
    }
}

void SerialLogSink::flush() {
    if (serialPort) {
        serialPort->flush();
    }
}

bool SerialLogSink::isAvailable() const {
    return serialPort != nullptr;
}

void SerialLogSink::setSerialPort(Stream* serial) {
    serialPort = serial;
    if (serialPort) {
        // 串口应在外部初始化
        // serialPort->begin(baudRate);
    }
}

void SerialLogSink::setBaudRate(uint32_t baud) {
    baudRate = baud;
    if (serialPort) {
        // 串口应在外部初始化，波特率更改需要外部处理
        // serialPort->end();
        // serialPort->begin(baudRate);
    }
}

// ============================================================================
// FileLogSink 实现
// ============================================================================

FileLogSink::FileLogSink(const String& path, size_t maxSize)
    : filePath(path), maxFileSize(maxSize), currentSize(0), fileOpened(false) {

    // 确保SPIFFS已初始化
    if (!SPIFFS.begin(true)) {
        return;
    }

    openFile();
}

FileLogSink::~FileLogSink() {
    closeFile();
}

bool FileLogSink::openFile() {
    if (fileOpened) {
        closeFile();
    }

    logFile = SPIFFS.open(filePath, FILE_APPEND);
    if (!logFile) {
        fileOpened = false;
        return false;
    }

    fileOpened = true;
    currentSize = logFile.size();
    return true;
}

void FileLogSink::closeFile() {
    if (logFile) {
        logFile.close();
    }
    fileOpened = false;
}

bool FileLogSink::rotateLogIfNeeded() {
    if (currentSize >= maxFileSize) {
        closeFile();

        // 生成新的文件名（带时间戳）
        String newPath = filePath + "." + String(millis());
        SPIFFS.rename(filePath, newPath);

        // 重新打开文件
        return openFile();
    }
    return true;
}

void FileLogSink::write(const String& message) {
    if (!fileOpened && !openFile()) {
        return;
    }

    // 检查是否需要日志轮转
    rotateLogIfNeeded();

    if (logFile) {
        size_t written = logFile.println(message);
        if (written > 0) {
            currentSize += written;
            logFile.flush();
        }
    }
}

void FileLogSink::flush() {
    if (logFile) {
        logFile.flush();
    }
}

bool FileLogSink::isAvailable() const {
    return fileOpened;
}

// ============================================================================
// NetworkLogSink 实现
// ============================================================================

NetworkLogSink::NetworkLogSink(const String& host, uint16_t port)
    : serverHost(host), serverPort(port), initialized(false) {

    // 网络初始化在首次写入时进行
}

void NetworkLogSink::write(const String& message) {
    if (!isAvailable()) {
        return;
    }

    // 这里简化实现：实际应用中可能需要使用WiFiClient或WiFiUDP
    // 由于网络日志不是核心功能，这里只提供框架
    // 实际使用时需要根据网络库实现
}

void NetworkLogSink::flush() {
    // 网络输出通常无flush操作
}

bool NetworkLogSink::isAvailable() const {
    // 检查WiFi连接状态
    return WiFi.status() == WL_CONNECTED && initialized;
}

void NetworkLogSink::setServer(const String& host, uint16_t port) {
    serverHost = host;
    serverPort = port;
    initialized = false;
}

bool NetworkLogSink::initialize() {
    // 这里应初始化网络连接
    // 简化处理：假设WiFi已连接
    if (WiFi.status() == WL_CONNECTED) {
        initialized = true;
    }
    return initialized;
}