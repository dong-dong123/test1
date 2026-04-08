#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "PinDefinitions.h"

// 音频驱动配置结构（硬件相关）
struct AudioDriverConfig {
    uint32_t sampleRate;      // 采样率 (Hz)
    i2s_bits_per_sample_t bitsPerSample; // 位深度
    i2s_channel_fmt_t channelFormat;     // 声道格式
    size_t bufferSize;        // 缓冲区大小（字节）
    uint8_t volume;           // 音量 (0-100)

    AudioDriverConfig() :
        sampleRate(16000),
        bitsPerSample(I2S_BITS_PER_SAMPLE_16BIT),
        channelFormat(I2S_CHANNEL_FMT_ONLY_LEFT),
        bufferSize(4096),
        volume(80) {}
};

// 音频数据回调函数类型
typedef void (*AudioDataCallback)(const uint8_t* data, size_t length, void* userData);

// 音频驱动类 - 处理I2S音频输入（麦克风）和输出（扬声器）
class AudioDriver {
private:
    // I2S配置
    i2s_config_t i2sConfig;
    i2s_pin_config_t pinConfig;

    // 音频配置
    AudioDriverConfig config;

    // 状态标志
    bool isInitialized;
    bool isRecording;
    bool isPlaying;

    // 回调函数
    AudioDataCallback recordCallback;
    void* recordCallbackUserData;

    // 内部缓冲区
    uint8_t* audioBuffer;
    size_t bufferWritePos;
    size_t bufferReadPos;

    // 内部方法
    bool initI2S();
    void deinitI2S();
    bool startRecording();
    bool stopRecording();
    bool startPlaying();
    bool stopPlaying();

public:
    AudioDriver();
    virtual ~AudioDriver();

    // 初始化/反初始化
    bool initialize(const AudioDriverConfig& cfg = AudioDriverConfig());
    bool deinitialize();
    bool isReady() const { return isInitialized; }

    // 录音控制
    bool startRecord(AudioDataCallback callback = nullptr, void* userData = nullptr);
    bool stopRecord();
    bool isRecordingActive() const { return isRecording; }

    // 播放控制
    bool startPlay();
    bool stopPlay();
    bool isPlayingActive() const { return isPlaying; }

    // 音频数据操作
    size_t readAudioData(uint8_t* buffer, size_t maxLength);
    size_t writeAudioData(const uint8_t* data, size_t length);

    // 配置管理
    const AudioDriverConfig& getConfig() const { return config; }
    bool updateConfig(const AudioDriverConfig& newConfig);

    // 音量控制
    bool setVolume(uint8_t volume);
    uint8_t getVolume() const { return config.volume; }

    // 状态查询
    size_t getAvailableData() const;
    size_t getFreeSpace() const;

    // 工具方法
    static void printI2SConfig(const i2s_config_t& config);
    static void printPinConfig(const i2s_pin_config_t& config);

    // 测试方法
    bool testMic();
    bool testSpeaker();

    // 任务句柄管理
    void clearRecordTaskHandle() { recordTaskHandle = nullptr; }
    void clearPlayTaskHandle() { playTaskHandle = nullptr; }

private:
    // 任务函数（静态）
    static void recordTask(void* parameter);
    static void playTask(void* parameter);

    // 任务句柄
    TaskHandle_t recordTaskHandle;
    TaskHandle_t playTaskHandle;
};

#endif // AUDIO_DRIVER_H