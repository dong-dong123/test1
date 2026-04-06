#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

// 系统版本
#define SYSTEM_VERSION "1.0.0"
#define SYSTEM_NAME "Xiaozhi Voice Assistant"

// 内存配置
#define AUDIO_BUFFER_SIZE 4096
#define MAX_CONFIG_SIZE 2048
#define LOG_BUFFER_SIZE 512

// 状态枚举
enum class SystemState {
    BOOTING,
    IDLE,
    LISTENING,
    PROCESSING,
    RECOGNIZING,
    THINKING,
    SYNTHESIZING,
    PLAYING,
    ERROR,
    CONFIGURING
};

#endif