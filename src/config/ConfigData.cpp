#include "ConfigData.h"

bool SystemConfig::validate() const {
    // 基础验证
    if (wifi.ssid.length() == 0 && wifi.autoConnect) {
        return false; // 如果启用自动连接但未设置SSID
    }

    if (audio.sampleRate < 8000 || audio.sampleRate > 48000) {
        return false;
    }

    if (audio.bitsPerSample != 8 && audio.bitsPerSample != 16 && audio.bitsPerSample != 24) {
        return false;
    }

    if (audio.channels != 1 && audio.channels != 2) {
        return false;
    }

    if (audio.vadThreshold < 0.0f || audio.vadThreshold > 1.0f) {
        return false;
    }

    if (audio.wakeWordSensitivity < 0.0f || audio.wakeWordSensitivity > 1.0f) {
        return false;
    }

    if (audio.volume > 100) {
        return false;
    }

    if (display.brightness > 100) {
        return false;
    }

    // 服务配置验证
    if (services.defaultSpeechService.length() == 0) {
        return false;
    }

    if (services.defaultDialogueService.length() == 0) {
        return false;
    }

    // 日志级别验证
    const String validLevels[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    bool validLevel = false;
    for (const auto& level : validLevels) {
        if (logging.level.equalsIgnoreCase(level)) {
            validLevel = true;
            break;
        }
    }
    if (!validLevel) {
        return false;
    }

    return true;
}

void SystemConfig::resetToDefaults() {
    version = 1; // 初始版本
    wifi = WiFiConfig();
    services = ServicesConfig();
    audio = AudioConfig();
    display = DisplayConfig();
    logging = LoggingConfig();
}