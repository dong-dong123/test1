#include "MainApplication.h"
#include <esp_system.h>
#include <esp_log.h>

// 标签用于日志记录
static const char* TAG = "MainApplication";

// 构造函数
MainApplication::MainApplication()
    : currentState(SystemState::BOOTING),
      stateEntryTime(0),
      lastErrorMessage(""),
      audioHardwareAvailable(false),
      initState(INIT_NONE) {
    // 初始化时间戳
    stateEntryTime = millis();
}

// 析构函数
MainApplication::~MainApplication() {
    deinitialize();
}

// 初始化应用程序
bool MainApplication::initialize() {
    logEvent("app_start", "开始初始化");

    // 分阶段初始化，每个阶段失败可独立处理
    if (!initializeStage(INIT_CONFIG)) return false;
    if (!initializeStage(INIT_LOGGER)) return false;
    if (!initializeStage(INIT_DISPLAY)) return false;
    if (!initializeStage(INIT_AUDIO)) return false;
    if (!initializeStage(INIT_NETWORK)) return false;
    if (!initializeStage(INIT_SERVICES)) return false;
    if (!initializeStage(INIT_SERVICE_MANAGER)) return false;

    initState = INIT_COMPLETE;
    changeState(SystemState::IDLE);
    logEvent("app_ready", "应用程序初始化完成");

    return true;
}

// 分阶段初始化
bool MainApplication::initializeStage(InitState stage) {
    if (stage <= initState) {
        // 该阶段已经完成
        return true;
    }

    bool result = false;
    String stageName;

    switch (stage) {
        case INIT_CONFIG:
            result = initializeConfig(stageName);
            break;

        case INIT_LOGGER:
            result = initializeLogger(stageName);
            break;

        case INIT_DISPLAY:
            result = initializeDisplay(stageName);
            break;

        case INIT_AUDIO:
            result = initializeAudio(stageName);
            break;

        case INIT_NETWORK:
            result = initializeNetwork(stageName);
            break;

        case INIT_SERVICES:
            result = initializeServices(stageName);
            break;

        case INIT_SERVICE_MANAGER:
            result = initializeServiceManager(stageName);
            break;

        default:
            stageName = "未知阶段";
            result = false;
            break;
    }

    if (result) {
        initState = stage;
        logEvent("stage_complete", String("阶段完成: ") + stageName);
    } else {
        logEvent("stage_failed", String("阶段失败: ") + stageName);
    }

    return result;
}

// 初始化阶段的具体实现
bool MainApplication::initializeConfig(String& stageName) {
    stageName = "配置管理器";
    // 初始化SPIFFS文件系统
    if (SPIFFSConfigManager::initializeSPIFFS()) {
        // 加载配置
        if (configManager.load()) {
            logEvent("config_loaded", "配置加载成功");
            return true;
        } else {
            logEvent("config_load_failed", "配置加载失败，使用默认配置");
            configManager.resetToDefaults();
            configManager.save(); // 保存默认配置
            return true; // 使用默认配置继续
        }
    } else {
        handleError("SPIFFS初始化失败");
        return false;
    }
}

bool MainApplication::initializeLogger(String& stageName) {
    stageName = "日志系统";
    // 设置配置管理器
    logger.setConfigManager(&configManager);
    // 从配置更新日志设置
    logger.configureFromManager();
    logEvent("logger_initialized", "日志系统初始化完成");
    return true;
}

bool MainApplication::initializeDisplay(String& stageName) {
    stageName = "显示驱动";

    Serial.println("[DEBUG] Starting display initialization...");

    // 从配置获取显示设置
    DisplayConfig displayConfig;
    displayConfig.width = 242; // 默认值，可从配置读取
    displayConfig.height = 240;
    displayConfig.rotation = 1;
    displayConfig.brightness = configManager.getInt("display.brightness", 100);

    Serial.println("[DEBUG] Display config created, calling displayDriver.initialize()...");

    if (displayDriver.initialize(displayConfig)) {
        Serial.println("[DEBUG] displayDriver.initialize() succeeded");

        // 显示启动画面
        displayDriver.showBootScreen(SYSTEM_NAME, SYSTEM_VERSION);
        logEvent("display_initialized", "显示驱动初始化完成");
        return true;
    } else {
        Serial.println("[DEBUG] displayDriver.initialize() failed");
        handleError("显示驱动初始化失败");
        return false;
    }
}

bool MainApplication::initializeAudio(String& stageName) {
    stageName = "音频驱动";
    // 从配置获取音频设置
    AudioDriverConfig audioConfig;
    audioConfig.sampleRate = configManager.getInt("audio.sampleRate", 16000);
    // 转换位深度
    int bits = configManager.getInt("audio.bitsPerSample", 16);
    if (bits == 16) {
        audioConfig.bitsPerSample = I2S_BITS_PER_SAMPLE_16BIT;
    } else if (bits == 24) {
        audioConfig.bitsPerSample = I2S_BITS_PER_SAMPLE_24BIT;
    } else if (bits == 32) {
        audioConfig.bitsPerSample = I2S_BITS_PER_SAMPLE_32BIT;
    } else {
        audioConfig.bitsPerSample = I2S_BITS_PER_SAMPLE_16BIT;
    }
    // 转换声道格式
    int channels = configManager.getInt("audio.channels", 1);
    if (channels == 1) {
        audioConfig.channelFormat = I2S_CHANNEL_FMT_ONLY_LEFT;
    } else if (channels == 2) {
        audioConfig.channelFormat = I2S_CHANNEL_FMT_RIGHT_LEFT;
    } else {
        audioConfig.channelFormat = I2S_CHANNEL_FMT_ONLY_LEFT;
    }
    audioConfig.bufferSize = AUDIO_BUFFER_SIZE;
    audioConfig.volume = configManager.getInt("audio.volume", 80);

    if (audioDriver.initialize(audioConfig)) {
        // 测试音频硬件
        if (audioDriver.testMic() && audioDriver.testSpeaker()) {
            logEvent("audio_initialized", "音频驱动初始化完成");
            audioHardwareAvailable = true;
            return true;
        } else {
            logEvent("audio_test_failed", "音频硬件测试失败，禁用音频驱动");
            audioDriver.deinitialize(); // 卸载I2S驱动以停止时钟噪音
            audioHardwareAvailable = false;
            return true; // 系统继续运行，但音频被禁用
        }
    } else {
        handleError("音频驱动初始化失败");
        audioHardwareAvailable = false;
        return false;
    }
}

bool MainApplication::initializeNetwork(String& stageName) {
    stageName = "网络管理器";

    // 在初始化网络之前，先检查Wi-Fi硬件状态
    ESP_LOGI(TAG, "Checking Wi-Fi hardware before network initialization...");

    // 设置依赖
    networkManager.setConfigManager(&configManager);
    networkManager.setLogger(&logger);

    // 检查Wi-Fi硬件状态
    if (!networkManager.checkWiFiHardware()) {
        ESP_LOGW(TAG, "Wi-Fi hardware check failed, but continuing with initialization...");
        logEvent("wifi_hardware_check_failed", "Wi-Fi硬件检查失败，但仍继续初始化");
    }

    // 给Wi-Fi硬件更多时间初始化（ESP32-S3可能需要）
    ESP_LOGI(TAG, "Waiting for Wi-Fi hardware to stabilize...");
    delay(500); // 500ms延迟

    if (networkManager.initialize()) {
        logEvent("network_initialized", "网络管理器初始化完成");

        // 开始连接（如果配置为自动连接）
        if (configManager.getBool("wifi.autoConnect", true)) {
            // 在连接前再等待一下
            delay(200);
            networkManager.connect();
            logEvent("wifi_connect_start", "开始WiFi连接");
        }

        return true;
    } else {
        logEvent("network_init_failed", "网络管理器初始化失败");
        return true; // 网络初始化失败仍继续（离线模式）
    }
}

bool MainApplication::initializeServices(String& stageName) {
    stageName = "云端服务";
    // 初始化语音服务
    speechService.setNetworkManager(&networkManager);
    speechService.setConfigManager(&configManager);
    speechService.setLogger(&logger);

    if (speechService.initialize()) {
        logEvent("speech_service_initialized", "语音服务初始化完成");
    } else {
        logEvent("speech_service_init_failed", "语音服务初始化失败");
        // 服务初始化失败仍继续，稍后重试
    }

    // 初始化对话服务
    dialogueService.setNetworkManager(&networkManager);
    dialogueService.setConfigManager(&configManager);
    dialogueService.setLogger(&logger);

    if (dialogueService.initialize()) {
        logEvent("dialogue_service_initialized", "对话服务初始化完成");
    } else {
        logEvent("dialogue_service_init_failed", "对话服务初始化失败");
        // 服务初始化失败仍继续，稍后重试
    }

    return true; // 服务初始化失败不影响整体初始化
}

bool MainApplication::initializeServiceManager(String& stageName) {
    stageName = "服务管理器";
    // 设置依赖
    serviceManager.setConfigManager(&configManager);
    serviceManager.setLogger(&logger);

    if (serviceManager.initialize()) {
        // 注册服务
        if (speechService.isReady()) {
            serviceManager.registerSpeechService(&speechService);
        }
        if (dialogueService.isReady()) {
            serviceManager.registerDialogueService(&dialogueService);
        }

        logEvent("service_manager_initialized", "服务管理器初始化完成");
        return true;
    } else {
        handleError("服务管理器初始化失败");
        return false;
    }
}

// 反初始化
void MainApplication::deinitialize() {
    logEvent("app_stop", "开始反初始化");

    // 逆序清理
    serviceManager.deinitialize();
    speechService.deinitialize();
    dialogueService.deinitialize();
    networkManager.deinitialize();
    audioDriver.deinitialize();
    displayDriver.deinitialize();
    logger.flush();

    initState = INIT_NONE;
    changeState(SystemState::BOOTING);
}

// 更新主循环
void MainApplication::update() {
    // 更新网络管理器
    if (networkManager.isReady()) {
        networkManager.update();
    }

    // 更新服务管理器
    if (serviceManager.isReady()) {
        serviceManager.update();
    }

    // 处理当前状态
    handleState();

    // 定期刷新日志
    static uint32_t lastLogFlush = 0;
    if (millis() - lastLogFlush > 1000) {
        logger.flush();
        lastLogFlush = millis();
    }

    // 状态超时检查
    uint32_t stateDuration = millis() - stateEntryTime;
    switch (currentState) {
        case SystemState::LISTENING:
            if (stateDuration > 10000) { // 10秒超时
                stopListening();
                logEvent("listen_timeout", "录音超时");
            }
            break;

        case SystemState::SYNTHESIZING:
            if (stateDuration > 30000) { // 30秒超时
                handleError("语音合成超时");
                changeState(SystemState::IDLE);
            }
            break;

        case SystemState::PLAYING:
            if (stateDuration > 60000) { // 60秒超时
                audioDriver.stopPlay();
                handleError("播放超时");
                changeState(SystemState::IDLE);
            }
            break;

        default:
            // 其他状态无超时
            break;
    }
}

// 状态改变
void MainApplication::changeState(SystemState newState) {
    if (currentState == newState) return;

    logEvent("state_change",
             String("从 ") + stateToString(currentState) +
             " 到 " + stateToString(newState));

    currentState = newState;
    stateEntryTime = millis();
    updateDisplayForState();
}

// 处理当前状态
void MainApplication::handleState() {
    // 根据当前状态执行相应操作
    switch (currentState) {
        case SystemState::BOOTING:
            // 启动中，显示启动画面
            // 已经在初始化阶段处理
            break;

        case SystemState::IDLE:
            // 空闲状态，等待用户输入
            // 可以在这里实现低功耗模式
            break;

        case SystemState::LISTENING:
            // 录音中，显示录音状态
            // 音频数据通过回调处理
            break;

        case SystemState::RECOGNIZING:
            // 语音识别中，显示识别状态
            // 处理在 processAudioData 中完成
            break;

        case SystemState::THINKING:
            // AI处理中，显示思考状态
            break;

        case SystemState::SYNTHESIZING:
            // 语音合成中，显示合成状态
            break;

        case SystemState::PLAYING:
            // 播放中，显示播放状态
            // 检查播放是否完成
            if (!audioDriver.isPlayingActive()) {
                changeState(SystemState::IDLE);
            }
            break;

        case SystemState::ERROR:
            // 错误状态，显示错误信息
            // 等待用户操作或自动恢复
            if (millis() - stateEntryTime > 5000) {
                // 5秒后尝试恢复
                changeState(SystemState::IDLE);
            }
            break;

        case SystemState::CONFIGURING:
            // 配置中，显示配置界面
            break;

        default:
            break;
    }
}

// 更新显示状态
void MainApplication::updateDisplayForState() {
    if (!displayDriver.isReady()) return;

    // 根据状态更新显示
    switch (currentState) {
        case SystemState::BOOTING:
            displayDriver.showBootScreen(SYSTEM_NAME, SYSTEM_VERSION);
            break;

        case SystemState::IDLE:
            displayDriver.clear();
            displayDriver.drawTextCentered(100, "就绪");
            displayDriver.showStatus("空闲", 0x07E0); // 绿色
            break;

        case SystemState::LISTENING:
            displayDriver.showStatus("监听中...", 0x07E0); // 绿色
            break;

        case SystemState::RECOGNIZING:
            displayDriver.showStatus("识别中...", 0xFFE0); // 黄色
            break;

        case SystemState::THINKING:
            displayDriver.showStatus("思考中...", 0xFFE0); // 黄色
            break;

        case SystemState::SYNTHESIZING:
            displayDriver.showStatus("合成中...", 0xFD20); // 橙色
            break;

        case SystemState::PLAYING:
            displayDriver.showStatus("播放中...", 0x001F); // 蓝色
            break;

        case SystemState::ERROR:
            displayDriver.showError(lastErrorMessage);
            break;

        case SystemState::CONFIGURING:
            displayDriver.showStatus("配置中...", 0xF81F); // 粉色
            break;

        default:
            break;
    }
}

// 开始录音
void MainApplication::startListening() {
    // 如果当前状态是ERROR，直接返回（ERROR状态有自动恢复机制）
    if (currentState == SystemState::ERROR) {
        // 静默返回，不打印重复消息（ERROR状态会在5秒后自动恢复）
        return;
    }

    // 如果状态不是IDLE，只记录一次（状态变化已在changeState中记录）
    if (currentState != SystemState::IDLE) {
        // 不再打印重复调试信息，状态变化已在changeState函数中通过logEvent记录
        return;
    }

    // 如果音频驱动未就绪但硬件可用，重新初始化
    if (!audioDriver.isReady() && audioHardwareAvailable) {
        Serial.println("[DEBUG] Audio driver not ready but hardware available, reinitializing...");

        AudioDriverConfig audioConfig;
        audioConfig.sampleRate = configManager.getInt("audio.sampleRate", 16000);
        int bits = configManager.getInt("audio.bitsPerSample", 16);
        if (bits == 16) {
            audioConfig.bitsPerSample = I2S_BITS_PER_SAMPLE_16BIT;
        } else if (bits == 24) {
            audioConfig.bitsPerSample = I2S_BITS_PER_SAMPLE_24BIT;
        } else if (bits == 32) {
            audioConfig.bitsPerSample = I2S_BITS_PER_SAMPLE_32BIT;
        } else {
            audioConfig.bitsPerSample = I2S_BITS_PER_SAMPLE_16BIT;
        }
        int channels = configManager.getInt("audio.channels", 1);
        if (channels == 1) {
            audioConfig.channelFormat = I2S_CHANNEL_FMT_ONLY_LEFT;
        } else if (channels == 2) {
            audioConfig.channelFormat = I2S_CHANNEL_FMT_RIGHT_LEFT;
        } else {
            audioConfig.channelFormat = I2S_CHANNEL_FMT_ONLY_LEFT;
        }
        audioConfig.bufferSize = AUDIO_BUFFER_SIZE;
        audioConfig.volume = configManager.getInt("audio.volume", 80);

        if (!audioDriver.initialize(audioConfig)) {
            Serial.println("[DEBUG] Failed to reinitialize audio driver");
            return;
        }
        Serial.println("[DEBUG] Audio driver reinitialized successfully");
    }

    if (currentState == SystemState::IDLE && audioDriver.isReady()) {
        changeState(SystemState::LISTENING);

        // 开始录音，设置回调函数
        bool recordStarted = audioDriver.startRecord([](const uint8_t* data, size_t len, void* user) {
            // 音频数据回调
            auto app = static_cast<MainApplication*>(user);
            app->processAudioData(data, len);
        }, this);

        if (!recordStarted) {
            Serial.println("[ERROR] Audio record start failed");
            changeState(SystemState::IDLE);
            handleError("启动录音失败");
            return;
        }

        logEvent("listening_start", "开始录音");
    } else {
        // 已经在函数开头打印了状态信息，这里只打印音频驱动状态（如果需要）
        if (!audioDriver.isReady()) {
            Serial.println("[INFO] Cannot start listening: audio driver not ready");
        }
    }
}

// 停止录音
void MainApplication::stopListening() {
    if (currentState == SystemState::LISTENING) {
        audioDriver.stopRecord();
        changeState(SystemState::IDLE);
        logEvent("listening_stop", "停止录音");
    }
}

// 处理音频数据
void MainApplication::processAudioData(const uint8_t* audioData, size_t length) {
    Serial.printf("[DEBUG] processAudioData: state=%d, length=%zu\n", currentState, length);

    if (currentState != SystemState::LISTENING || length == 0) {
        Serial.printf("[DEBUG] processAudioData: Invalid state (%d) or length (%zu), skipping\n",
                      currentState, length);
        return;
    }

    changeState(SystemState::RECOGNIZING);
    logEvent("audio_received", String("收到音频数据: ") + String(length) + " 字节");

    // 检查是否有可用的语音服务
    if (!serviceManager.isReady()) {
        Serial.println("[DEBUG] processAudioData: ServiceManager not ready");
        handleError("服务管理器未就绪");
        return;
    }

    SpeechService* speech = serviceManager.getDefaultSpeechService();
    if (!speech) {
        Serial.println("[DEBUG] processAudioData: No default speech service");
        handleError("语音服务不可用");
        return;
    }

    if (!speech->isAvailable()) {
        Serial.println("[DEBUG] processAudioData: Speech service not available");
        handleError("语音服务不可用");
        return;
    }

    // 语音识别
    String recognizedText;
    if (speech->recognize(audioData, length, recognizedText)) {
        logEvent("recognition_success", String("识别结果: ") + recognizedText);

        // 对话处理
        changeState(SystemState::THINKING);

        DialogueService* dialogue = serviceManager.getDefaultDialogueService();
        if (dialogue && dialogue->isAvailable()) {
            String response = dialogue->chat(recognizedText);
            logEvent("dialogue_response", String("对话响应: ") + response);
            playResponse(response);
        } else {
            handleError("对话服务不可用");
        }
    } else {
        String error = speech->getLastError();
        handleError("语音识别失败: " + error);
    }
}

// 播放响应
void MainApplication::playResponse(const String& text) {
    if (text.isEmpty() || !audioDriver.isReady()) return;

    changeState(SystemState::SYNTHESIZING);
    logEvent("synthesis_start", String("开始合成: ") + text);

    // 检查是否有可用的语音服务
    SpeechService* speech = serviceManager.getDefaultSpeechService();
    if (!speech || !speech->isAvailable()) {
        handleError("语音服务不可用");
        return;
    }

    // 语音合成
    std::vector<uint8_t> audioData;
    if (speech->synthesize(text, audioData)) {
        logEvent("synthesis_success", String("合成成功: ") + String(audioData.size()) + " 字节");

        changeState(SystemState::PLAYING);

        // 写入音频数据到缓冲区
        size_t written = audioDriver.writeAudioData(audioData.data(), audioData.size());
        if (written > 0) {
            audioDriver.startPlay();
            logEvent("playback_start", "开始播放");
        } else {
            handleError("音频写入失败");
            changeState(SystemState::IDLE);
        }
    } else {
        String error = speech->getLastError();
        handleError("语音合成失败: " + error);
        changeState(SystemState::IDLE);
    }
}

// 错误处理
void MainApplication::handleError(const String& error) {
    lastErrorMessage = error;
    changeState(SystemState::ERROR);
    logEvent("error", error);

    // 记录错误日志
    logger.log(Logger::Level::ERROR, error);
}

// 记录事件
void MainApplication::logEvent(const String& event, const String& details) {
    String message = String("[") + event + "] " + details;
    logger.log(Logger::Level::INFO, message);

    // 同时输出到串口（如果日志系统未配置串口输出）
    Serial.println(message);
}