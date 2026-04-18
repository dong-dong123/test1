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

    // 双阈值VAD验证
    if (audio.vadSpeechThreshold < 0.0f || audio.vadSpeechThreshold > 1.0f) {
        return false;
    }
    if (audio.vadSilenceThreshold < 0.0f || audio.vadSilenceThreshold > 1.0f) {
        return false;
    }
    if (audio.vadSpeechThreshold <= audio.vadSilenceThreshold) {
        return false; // speech_threshold必须大于silence_threshold以实现迟滞效应
    }
    if (audio.vadSilenceDuration == 0 || audio.vadSilenceDuration > 30000) {
        return false; // 静音持续时间必须在合理范围内（1ms-30s）
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