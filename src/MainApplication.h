#ifndef MAIN_APPLICATION_H
#define MAIN_APPLICATION_H

#include <Arduino.h>
#include "globals.h"
#include "config/SPIFFSConfigManager.h"
#include "modules/SystemLogger.h"
#include "modules/NetworkManager.h"
#include "modules/ServiceManager.h"
#include "drivers/AudioDriver.h"
#include "drivers/DisplayDriver.h"
#include "services/VolcanoSpeechService.h"
#include "services/CozeDialogueService.h"

// 系统状态字符串转换辅助函数
inline String stateToString(SystemState state) {
    switch (state) {
        case SystemState::BOOTING:      return "BOOTING";
        case SystemState::IDLE:         return "IDLE";
        case SystemState::LISTENING:    return "LISTENING";
        case SystemState::PROCESSING:   return "PROCESSING";
        case SystemState::RECOGNIZING:  return "RECOGNIZING";
        case SystemState::THINKING:     return "THINKING";
        case SystemState::SYNTHESIZING: return "SYNTHESIZING";
        case SystemState::PLAYING:      return "PLAYING";
        case SystemState::ERROR:        return "ERROR";
        case SystemState::CONFIGURING:  return "CONFIGURING";
        default:                        return "UNKNOWN";
    }
}

// 主应用程序类
class MainApplication {
private:
    // 模块实例
    SPIFFSConfigManager configManager;
    SystemLogger logger;
    NetworkManager networkManager;
    AudioDriver audioDriver;
    DisplayDriver displayDriver;
    VolcanoSpeechService speechService;
    CozeDialogueService dialogueService;
    ServiceManager serviceManager;

    // 状态管理
    SystemState currentState;
    uint32_t stateEntryTime;
    String lastErrorMessage;
    bool audioHardwareAvailable;

    // 音频累积
    static const size_t MAIN_AUDIO_BUFFER_SIZE = 160000;  // 5秒音频（16000Hz * 2字节 * 5秒）
    uint8_t audioBuffer[MAIN_AUDIO_BUFFER_SIZE];
    size_t audioBufferPos;
    uint32_t audioCollectionStartTime;

    // 静音检测（双阈值迟滞）
    float vadSpeechThreshold;     // VAD语音检测阈值 (0.0-1.0, 默认0.50)
    float vadSilenceThreshold;    // VAD静音确认阈值 (0.0-1.0, 默认0.30)
    uint32_t vadSilenceDuration;  // 静音持续时间阈值 (ms, 默认800)
    bool vadInSpeechState;        // 当前是否在语音状态
    bool vadSilenceDetected;      // 是否检测到静音
    uint32_t vadSilenceStartTime; // 静音开始时间
    uint32_t vadLastAudioTime;    // 最后有音频的时间

    // 识别触发标志（解决回调执行期间无法响应停止信号的问题）
    bool recognitionPending;      // 是否有待处理的识别请求
    uint32_t recognitionTriggerTime; // 识别触发时间
    bool recognitionActive;       // 识别是否正在进行中

    // 初始化状态
    enum InitState {
        INIT_NONE,
        INIT_CONFIG,
        INIT_LOGGER,
        INIT_DISPLAY,
        INIT_AUDIO,
        INIT_NETWORK,
        INIT_SERVICES,
        INIT_SERVICE_MANAGER,
        INIT_COMPLETE
    };
    InitState initState;

    // 内部方法
    bool initializeStage(InitState stage);
    bool initializeConfig(String& stageName);
    bool initializeLogger(String& stageName);
    bool initializeDisplay(String& stageName);
    bool initializeAudio(String& stageName);
    bool initializeNetwork(String& stageName);
    bool initializeServices(String& stageName);
    bool initializeServiceManager(String& stageName);
    void changeState(SystemState newState);
    void handleState();
    void updateDisplayForState();
    void handleError(const String& error);
    void logEvent(const String& event, const String& details = "");
    void handleAsyncRecognitionResult(const AsyncRecognitionResult& result);

public:
    MainApplication();
    virtual ~MainApplication();

    bool initialize();
    void update();
    void deinitialize();

    // 用户交互方法
    void startListening();
    void stopListening();
    void processAudioData(const uint8_t* audioData, size_t length);
    void playResponse(const String& text);

    // 状态查询
    SystemState getCurrentState() const { return currentState; }
    bool isReady() const { return initState == INIT_COMPLETE; }
    String getLastError() const { return lastErrorMessage; }

    // 静音检测状态
    bool hasDetectedSilence() const { return vadSilenceDetected && (millis() - vadSilenceStartTime >= vadSilenceDuration); }
    uint32_t getSilenceDuration() const { return vadSilenceDetected ? (millis() - vadSilenceStartTime) : 0; }
    void resetVadState() { vadInSpeechState = false; vadSilenceDetected = false; vadSilenceStartTime = 0; vadLastAudioTime = millis(); }

    // 模块访问器（用于测试和调试）
    ConfigManager* getConfigManager() { return &configManager; }
    Logger* getLogger() { return &logger; }
    NetworkManager* getNetworkManager() { return &networkManager; }
    AudioDriver* getAudioDriver() { return &audioDriver; }
    DisplayDriver* getDisplayDriver() { return &displayDriver; }
    ServiceManager* getServiceManager() { return &serviceManager; }
};

#endif // MAIN_APPLICATION_H