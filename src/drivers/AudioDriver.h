#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "freertos/portmacro.h"
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

    // 状态标志（volatile确保多任务环境下的可见性）
    bool isInitialized;
    volatile bool isRecording;
    volatile bool isPlaying;

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

    // 音频质量监控
    struct AudioQualityMetrics {
        double rms;              // RMS值（0-32767）
        double dbFS;             // dBFS值（负值，0为满量程）
        double snrEstimate;      // 信噪比估计（dB）
        int zeroCrossingRate;    // 零交叉率（Hz）
        int16_t peakLevel;       // 峰值电平
        int16_t noiseFloor;      // 噪声基底
        float qualityScore;      // 质量评分（0.0-1.0）
    };

    AudioQualityMetrics getAudioQualityMetrics(const uint8_t* audioData, size_t length) const;
    float calculateAudioQualityScore(const uint8_t* audioData, size_t length) const;
    bool isAudioQualityAcceptable(const uint8_t* audioData, size_t length, float minScore = 0.5f) const;
    void logAudioQualityReport(const uint8_t* audioData, size_t length, const char* context = nullptr) const;

    // 硬件诊断工具
    struct HardwareDiagnostics {
        bool i2sDriverInstalled;
        int dmaBufferCount;
        int dmaBufferLength;
        uint32_t sampleRate;
        int bitsPerSample;
        int zeroReadCount;          // 零读取计数
        int bufferOverrunCount;     // 缓冲区溢出计数
        int bufferUnderrunCount;    // 缓冲区欠载计数
        float cpuUsagePercent;      // CPU使用率估计
    };

    HardwareDiagnostics getHardwareDiagnostics() const;
    bool checkHardwareHealth() const;
    void runHardwareDiagnostics() const;
    bool diagnoseI2SConnection() const;

    // 任务句柄管理（带内存屏障确保多核心可见性）
    void clearRecordTaskHandle() {
        portMEMORY_BARRIER();
        recordTaskHandle = nullptr;
        portMEMORY_BARRIER();
    }
    void clearPlayTaskHandle() {
        portMEMORY_BARRIER();
        playTaskHandle = nullptr;
        portMEMORY_BARRIER();
    }

private:
    // 任务函数（静态）
    static void recordTask(void* parameter);
    static void playTask(void* parameter);

    // 任务句柄（volatile确保多核心可见性）
    volatile TaskHandle_t recordTaskHandle;
    volatile TaskHandle_t playTaskHandle;
};

#endif // AUDIO_DRIVER_H