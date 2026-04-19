#include "MainApplication.h"
#include <esp_system.h>
#include <esp_log.h>
#include <math.h>
#include <WiFi.h>
#include "services/VolcanoSpeechService.h"
#include "modules/SSLClientManager.h"

// 标签用于日志记录
static const char* TAG = "MainApplication";

// 构造函数
MainApplication::MainApplication()
    : currentState(SystemState::BOOTING),
      stateEntryTime(0),
      lastErrorMessage(""),
      audioHardwareAvailable(false),
      initState(INIT_NONE),
      audioBufferPos(0),
      audioCollectionStartTime(0),
      vadSpeechThreshold(0.50f),  // 默认语音检测阈值0.50
      vadSilenceThreshold(0.30f), // 默认静音确认阈值0.30
      vadSilenceDuration(800),    // 默认静音持续时间800ms
      vadInSpeechState(false),    // 初始状态：静音
      vadSilenceDetected(false),
      vadSilenceStartTime(0),
      vadLastAudioTime(0),
      recognitionPending(false),
      recognitionTriggerTime(0),
      recognitionActive(false) {
    // 初始化时间戳
    stateEntryTime = millis();
    // 清零音频缓冲区
    memset(audioBuffer, 0, MAIN_AUDIO_BUFFER_SIZE);
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
    // 转换位深度 - INMP441需要32位I2S帧（24位数据+8位填充）
    int bits = configManager.getInt("audio.bitsPerSample", 32);
    if (bits == 16) {
        audioConfig.bitsPerSample = I2S_BITS_PER_SAMPLE_16BIT;
    } else if (bits == 24) {
        audioConfig.bitsPerSample = I2S_BITS_PER_SAMPLE_24BIT;
    } else if (bits == 32) {
        audioConfig.bitsPerSample = I2S_BITS_PER_SAMPLE_32BIT;
    } else {
        audioConfig.bitsPerSample = I2S_BITS_PER_SAMPLE_32BIT; // 默认32位
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
    audioConfig.bufferSize = MAIN_AUDIO_BUFFER_SIZE;
    audioConfig.volume = configManager.getInt("audio.volume", 80);

    // 加载VAD参数（配置管理器已处理向后兼容性）
    vadSpeechThreshold = configManager.getFloat("audio.vadSpeechThreshold", 0.50f);
    vadSilenceThreshold = configManager.getFloat("audio.vadSilenceThreshold", 0.30f);
    vadSilenceDuration = configManager.getInt("audio.vadSilenceDuration", 800);
    vadInSpeechState = false; // 初始状态：静音

    Serial.printf("[VAD] 配置加载: speech_threshold=%.2f, silence_threshold=%.2f, silence_duration=%u ms\n",
                 vadSpeechThreshold, vadSilenceThreshold, vadSilenceDuration);

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

    // 检查是否有待处理的识别请求（异步处理，避免在回调中执行状态转换）
    if (recognitionPending && currentState == SystemState::LISTENING) {
        Serial.printf("[VAD] Processing pending recognition: pending=%d, state=%s\n",
                     recognitionPending, stateToString(currentState).c_str());

        // 清除标志，防止重复处理
        recognitionPending = false;

        // 触发语音识别
        changeState(SystemState::RECOGNIZING);

        // 这里将添加服务检查和识别逻辑
        Serial.printf("[VAD] State changed to RECOGNIZING, will process recognition in handleState\n");
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

        case SystemState::RECOGNIZING:
            if (stateDuration > 120000) { // 120秒超时（覆盖流式识别服务器处理时间）
                // 检查网络连接
                if (WiFi.status() != WL_CONNECTED) {
                    handleError("网络连接中断，请检查WiFi");
                } else {
                    handleError("语音识别超时，服务器可能不可用");
                }
                changeState(SystemState::IDLE);
            }
            break;

        case SystemState::THINKING:
            if (stateDuration > 20000) { // 20秒超时（Coze对话服务可能需要更长时间）
                handleError("对话处理超时");
                changeState(SystemState::IDLE);
            }
            break;

        case SystemState::SYNTHESIZING:
            if (stateDuration > 60000) { // 60秒超时（语音合成和流式接收可能需要更长时间）
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

    // 保存旧状态
    SystemState oldState = currentState;

    // 先更新状态和时间，防止超时检查在清理过程中触发
    currentState = newState;
    stateEntryTime = millis();

    logEvent("state_change",
             String("从 ") + stateToString(oldState) +
             " 到 " + stateToString(newState));

    // 基于旧状态的清理逻辑
    if (oldState == SystemState::LISTENING) {
        // 从录音状态转换出去时，停止录音
        audioDriver.stopRecord();
    }

    // 当离开RECOGNIZING状态时，重置识别活动标志
    if (oldState == SystemState::RECOGNIZING) {
        recognitionActive = false;
        Serial.printf("[RECOGNITION] Left RECOGNIZING state, resetting recognitionActive flag\n");
    }

    // 当进入RECOGNIZING状态时，设置识别活动标志
    if (newState == SystemState::RECOGNIZING) {
        recognitionActive = false; // 将在handleState中设置为true
        Serial.printf("[RECOGNITION] Entering RECOGNIZING state, recognitionActive=%d\n", recognitionActive);
    }

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
            // 执行语音识别（确保只执行一次）
            static uint32_t recognitionStartTime = 0; // 仍然使用静态变量跟踪开始时间

            if (!recognitionActive) {
                recognitionActive = true;
                recognitionStartTime = millis();
                Serial.printf("[RECOGNITION] Starting recognition process, audio buffer size: %zu bytes\n", audioBufferPos);

                // 录音可能在进入RECOGNIZING状态时已被stopRecord()停止，这是正常的
                // 我们使用已经累积的音频数据进行识别
                Serial.printf("[RECOGNITION] Audio buffer size: %zu bytes, recording active: %s\n",
                             audioBufferPos, audioDriver.isRecordingActive() ? "yes" : "no");

                // 检查是否有可用的语音服务
                Serial.printf("[RECOGNITION] Checking service manager readiness...\n");
                if (!serviceManager.isReady()) {
                    Serial.println("[RECOGNITION] ServiceManager not ready");
                    handleError("服务管理器未就绪");
                    audioBufferPos = 0; // 重置缓冲区
                    recognitionActive = false;
                    break;
                }
                Serial.printf("[RECOGNITION] ServiceManager is ready\n");

                Serial.printf("[RECOGNITION] Getting default speech service...\n");
                SpeechService* speech = serviceManager.getDefaultSpeechService();
                if (!speech) {
                    Serial.println("[RECOGNITION] No default speech service");
                    handleError("语音服务不可用");
                    audioBufferPos = 0; // 重置缓冲区
                    recognitionActive = false;
                    break;
                }
                Serial.printf("[RECOGNITION] Got speech service: %p\n", speech);

                Serial.printf("[RECOGNITION] Checking speech service availability...\n");
                bool isAvailable = speech->isAvailable();
                String serviceName = speech->getName();
                Serial.printf("[RECOGNITION] Service name='%s', isAvailable=%s\n", serviceName.c_str(), isAvailable ? "true" : "false");

                if (!isAvailable) {
                    Serial.println("[RECOGNITION] Speech service not available");
                    handleError("语音服务不可用");
                    audioBufferPos = 0; // 重置缓冲区
                    recognitionActive = false;
                    break;
                }

                // 录音可能在进入RECOGNIZING状态时已被stopRecord()停止，这是正常的
                // 我们使用已经累积的音频数据进行识别，录音停止不影响识别
                if (!audioDriver.isRecordingActive()) {
                    Serial.printf("[RECOGNITION] Recording stopped, but continuing with %zu bytes of accumulated audio\n", audioBufferPos);
                }

                // 尝试使用异步语音识别（如果支持）
                Serial.printf("[RECOGNITION] Checking if service is volcano...\n");
                if (serviceName == "volcano") {
                    Serial.printf("[RECOGNITION] Service is volcano, attempting async recognition...\n");
                    // 动态转换为VolcanoSpeechService以使用异步API
                    VolcanoSpeechService* volcanoSpeech = static_cast<VolcanoSpeechService*>(speech);

                    // 创建回调函数绑定到当前对象
                    auto callback = [this](const AsyncRecognitionResult& result) {
                        this->handleAsyncRecognitionResult(result);
                    };

                    // 调用异步识别（使用累积的音频数据）
                    Serial.printf("[RECOGNITION] Calling recognizeAsync with %zu bytes...\n", audioBufferPos);
                    bool asyncStarted = volcanoSpeech->recognizeAsync(audioBuffer, audioBufferPos, callback);
                    Serial.printf("[RECOGNITION] recognizeAsync returned %s\n", asyncStarted ? "true" : "false");

                    if (asyncStarted) {
                        logEvent("async_recognition_started", String("异步识别已启动，长度: ") + String(audioBufferPos) + " 字节（累积）");
                        Serial.printf("[RECOGNITION] Async recognition started successfully\n");
                        // 状态保持在RECOGNIZING，直到回调被调用
                        // 重置状态进入时间，避免WebSocket连接期间的超时
                        stateEntryTime = millis();
                        Serial.printf("[RECOGNITION] Reset state entry time to avoid timeout during WebSocket connection\n");
                    } else {
                        String error = volcanoSpeech->getLastError();
                        Serial.printf("[RECOGNITION] Async recognition failed: %s\n", error.c_str());
                        handleError("异步语音识别启动失败: " + error);
                        recognitionActive = false;
                        audioBufferPos = 0;
                    }
                } else {
                    // 回退到同步识别
                    Serial.printf("[RECOGNITION] Falling back to sync recognition for service '%s'\n", serviceName.c_str());
                    String recognizedText;
                    bool syncResult = speech->recognize(audioBuffer, audioBufferPos, recognizedText);
                    Serial.printf("[RECOGNITION] sync recognize returned %s\n", syncResult ? "true" : "false");

                    if (syncResult) {
                        logEvent("recognition_success", String("识别结果: ") + recognizedText);
                        Serial.printf("[RECOGNITION] Sync recognition successful: %s\n", recognizedText.c_str());

                        // 重置状态进入时间，避免后续状态超时
                        stateEntryTime = millis();
                        Serial.printf("[RECOGNITION] Reset state entry time after successful sync recognition\n");

                        // 优化内存使用顺序：语音识别完成后等待500ms再启动对话服务
                        // 避免连续创建多个SSL连接导致内存分配失败
                        ESP_LOGI(TAG, "Waiting 500ms before starting dialogue service...");
                        delay(500);

                        // 对话处理
                        changeState(SystemState::THINKING);

                        DialogueService* dialogue = serviceManager.getDefaultDialogueService();
                        if (dialogue && dialogue->isAvailable()) {
                            String response = dialogue->chat(recognizedText);
                            logEvent("dialogue_response", String("对话响应: ") + response);
                            playResponse(response);
                        } else {
                            // 降级策略：对话服务不可用时使用本地预设响应
                            ESP_LOGW(TAG, "Dialogue service unavailable, using fallback response");
                            String fallbackResponse = "抱歉，网络连接有问题，请稍后再试";
                            logEvent("dialogue_fallback", "使用降级响应: " + fallbackResponse);
                            playResponse(fallbackResponse);
                        }
                    } else {
                        String error = speech->getLastError();
                        Serial.printf("[RECOGNITION] Sync recognition failed: %s\n", error.c_str());
                        handleError("语音识别失败: " + error);
                        recognitionActive = false;
                        audioBufferPos = 0;
                    }
                }

                // 重置缓冲区（如果识别已启动，缓冲区将在识别完成后清理）
                if (!recognitionActive) {
                    audioBufferPos = 0;
                }
            } else {
                // 识别活动中，检查是否超时（由update中的超时检查处理）
                uint32_t elapsed = millis() - recognitionStartTime;
                if (elapsed % 5000 == 0) {
                    Serial.printf("[RECOGNITION] Recognition in progress for %u ms, audio buffer: %zu bytes\n",
                                 elapsed, audioBufferPos);
                }
            }
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
        audioConfig.bufferSize = MAIN_AUDIO_BUFFER_SIZE;
        audioConfig.volume = configManager.getInt("audio.volume", 80);

        if (!audioDriver.initialize(audioConfig)) {
            Serial.println("[DEBUG] Failed to reinitialize audio driver");
            return;
        }
        Serial.println("[DEBUG] Audio driver reinitialized successfully");
    }

    if (currentState == SystemState::IDLE && audioDriver.isReady()) {
        changeState(SystemState::LISTENING);

        // 重置音频累积缓冲区
        audioBufferPos = 0;
        memset(audioBuffer, 0, MAIN_AUDIO_BUFFER_SIZE);

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
        resetVadState(); // 重置VAD状态
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

    // 快速检查录音是否仍在进行（防止在stopRecord期间执行长时间操作）
    if (!audioDriver.isRecordingActive()) {
        Serial.printf("[DEBUG] processAudioData: Recording not active, skipping (state=%d)\n", currentState);
        return;
    }

    // 音频累积逻辑
    static const size_t MIN_AUDIO_DURATION = 120000;  // 3.75秒音频（16000Hz * 2字节 * 3.75秒），增加以确保完整句子
    static const uint32_t MAX_COLLECTION_TIME = 10000; // 最多收集10秒（与录音超时一致）

    // 初始化音频收集开始时间（第一次收到数据时）
    if (audioBufferPos == 0) {
        audioCollectionStartTime = millis();
        Serial.printf("[DEBUG] processAudioData: Audio collection started, buffer pos=0\n");
    }

    bool shouldTrigger = false;
    uint32_t currentTime = millis();

    // 检查缓冲区是否已满
    if (audioBufferPos + length > MAIN_AUDIO_BUFFER_SIZE) {
        Serial.printf("[DEBUG] processAudioData: Audio buffer full (%zu + %zu > %zu), triggering recognition\n",
                     audioBufferPos, length, MAIN_AUDIO_BUFFER_SIZE);
        // 缓冲区满，立即触发识别
        shouldTrigger = true;
    } else {
        // 有足够空间，存储数据
        // 音频能量检测 - 计算RMS值（针对16位PCM）
        int16_t* audioSamples = (int16_t*)audioData;
        size_t sampleCount = length / 2; // 16位 = 2字节
        int64_t sumSquares = 0;
        int16_t maxSample = 0;
        int16_t minSample = 0;

        for (size_t i = 0; i < sampleCount; i++) {
            int16_t sample = audioSamples[i];
            sumSquares += (int64_t)sample * sample;
            if (sample > maxSample) maxSample = sample;
            if (sample < minSample) minSample = sample;
        }

        double rms = (sampleCount > 0) ? sqrt((double)sumSquares / sampleCount) : 0.0;
        double db = (rms > 0) ? 20 * log10(rms / 32768.0) : -100.0; // 相对于满量程

        Serial.printf("[AUDIO] Energy: RMS=%.1f, dB=%.1f, samples=%zu, max=%d, min=%d\n",
                     rms, db, sampleCount, maxSample, minSample);

        // 双阈值迟滞VAD算法
        double normalizedRMS = rms / 32768.0;
        currentTime = millis();

        // 硬件问题检测：如果所有样本都为零，很可能是I2S中断，跳过VAD更新
        if (maxSample == 0 && minSample == 0 && sampleCount > 0) {
            Serial.printf("[VAD] WARNING: All samples zero (likely I2S interruption), skipping VAD update\n");
            // 不更新VAD状态，保持当前状态
            vadLastAudioTime = currentTime; // 更新最后音频时间
            memcpy(audioBuffer + audioBufferPos, audioData, length);
            audioBufferPos += length;
            Serial.printf("[DEBUG] processAudioData: Audio accumulated, total=%zu bytes\n", audioBufferPos);

            // 跳过VAD状态更新
            goto skip_vad_update;
        }

        // 双阈值迟滞状态机（按照设计文档实现）
        if (normalizedRMS > vadSpeechThreshold) {
            // 进入或保持语音状态
            vadInSpeechState = true;
            if (vadSilenceDetected) {
                vadSilenceDetected = false;
                Serial.printf("[VAD] Speech detected (rms_rel=%.4f > %.2f), clearing silence flag\n",
                             normalizedRMS, vadSpeechThreshold);
            }
        } else if (normalizedRMS < vadSilenceThreshold) {
            // 进入或保持静音状态
            vadInSpeechState = false;

            if (!vadSilenceDetected) {
                // 第一次检测到静音
                vadSilenceStartTime = currentTime;
                vadSilenceDetected = true;
                Serial.printf("[VAD] Silence start detected at %u ms (speech_thresh=%.2f, silence_thresh=%.2f, actual=%.4f)\n",
                             vadSilenceStartTime, vadSpeechThreshold, vadSilenceThreshold, normalizedRMS);
            } else {
                // 持续静音
                uint32_t silenceDuration = currentTime - vadSilenceStartTime;
                if (silenceDuration >= vadSilenceDuration) {
                    Serial.printf("[VAD] Silence duration reached: %u ms >= %u ms, triggering recognition\n",
                                 silenceDuration, vadSilenceDuration);
                    shouldTrigger = true;
                }
            }
        } else {
            // 中间区域：保持当前状态
            // 不改变vadInSpeechState和vadSilenceDetected
        }

        // 记录最后音频时间（无论状态）
        vadLastAudioTime = currentTime;

        memcpy(audioBuffer + audioBufferPos, audioData, length);
        audioBufferPos += length;
        Serial.printf("[DEBUG] processAudioData: Audio accumulated, total=%zu bytes\n", audioBufferPos);

    skip_vad_update:
        // 检查是否达到触发条件（只有在存储数据后才检查）
        if (audioBufferPos >= MIN_AUDIO_DURATION) {
            Serial.printf("[DEBUG] processAudioData: Minimum audio duration reached (%zu >= %zu)\n",
                         audioBufferPos, MIN_AUDIO_DURATION);
            shouldTrigger = true;
        } else if (currentTime - audioCollectionStartTime >= MAX_COLLECTION_TIME) {
            Serial.printf("[DEBUG] processAudioData: Maximum collection time reached (%u ms)\n",
                         currentTime - audioCollectionStartTime);
            shouldTrigger = true;
        }
    }

    if (!shouldTrigger) {
        // 继续收集音频数据
        return;
    }

    // 记录VAD状态
    currentTime = millis();
    uint32_t silenceDuration = vadSilenceDetected ? (currentTime - vadSilenceStartTime) : 0;
    bool hasLongSilence = vadSilenceDetected && (silenceDuration >= vadSilenceDuration);

    Serial.printf("[VAD] Triggering recognition: audio=%zu bytes, silence_detected=%s, silence_duration=%u ms, has_long_silence=%s\n",
                 audioBufferPos, vadSilenceDetected ? "yes" : "no", silenceDuration, hasLongSilence ? "yes" : "no");

    // 设置异步识别触发标志，不直接调用changeState
    recognitionPending = true;
    recognitionTriggerTime = millis();

    // 重置VAD状态，准备下一次检测（但保留音频数据）
    resetVadState();

    logEvent("recognition_pending",
             String("识别待处理: ") + String(audioBufferPos) + " 字节（累积）, silence_duration=" +
             String(silenceDuration) + " ms, has_long_silence=" + (hasLongSilence ? "yes" : "no"));

    Serial.printf("[VAD] Recognition pending flag set: audio=%zu bytes\n", audioBufferPos);

    // 不进行状态转换或服务检查，这些将在update()方法中异步处理
    return;

    Serial.printf("[DEBUG] processAudioData: Getting default speech service...\n");
    SpeechService* speech = serviceManager.getDefaultSpeechService();
    if (!speech) {
        Serial.println("[DEBUG] processAudioData: No default speech service");
        handleError("语音服务不可用");
        audioBufferPos = 0; // 重置缓冲区
        return;
    }
    Serial.printf("[DEBUG] processAudioData: Got speech service: %p\n", speech);

    Serial.printf("[DEBUG] processAudioData: Checking speech service availability...\n");
    bool isAvailable = speech->isAvailable();
    String serviceName = speech->getName();
    Serial.printf("[DEBUG] processAudioData: Service name='%s', isAvailable=%s\n", serviceName.c_str(), isAvailable ? "true" : "false");

    if (!isAvailable) {
        Serial.println("[DEBUG] processAudioData: Speech service not available");
        handleError("语音服务不可用");
        audioBufferPos = 0; // 重置缓冲区
        return;
    }

    // 再次检查录音状态（异步识别启动可能耗时）
    if (!audioDriver.isRecordingActive()) {
        Serial.printf("[DEBUG] processAudioData: Recording stopped before async recognition, aborting\n");
        audioBufferPos = 0; // 重置缓冲区
        return;
    }

    // 尝试使用异步语音识别（如果支持）
    Serial.printf("[DEBUG] processAudioData: Checking if service is volcano...\n");
    if (serviceName == "volcano") {
        Serial.printf("[DEBUG] processAudioData: Service is volcano, attempting async recognition...\n");
        // 动态转换为VolcanoSpeechService以使用异步API
        VolcanoSpeechService* volcanoSpeech = static_cast<VolcanoSpeechService*>(speech);

        // 创建回调函数绑定到当前对象
        auto callback = [this](const AsyncRecognitionResult& result) {
            this->handleAsyncRecognitionResult(result);
        };

        // 调用异步识别（使用累积的音频数据）
        Serial.printf("[DEBUG] processAudioData: Calling recognizeAsync with %zu bytes...\n", audioBufferPos);
        bool asyncStarted = volcanoSpeech->recognizeAsync(audioBuffer, audioBufferPos, callback);
        Serial.printf("[DEBUG] processAudioData: recognizeAsync returned %s\n", asyncStarted ? "true" : "false");

        if (asyncStarted) {
            logEvent("async_recognition_started", String("异步识别已启动，长度: ") + String(audioBufferPos) + " 字节（累积）");
            Serial.printf("[DEBUG] processAudioData: Async recognition started successfully\n");
            // 状态保持在RECOGNIZING，直到回调被调用
        } else {
            String error = volcanoSpeech->getLastError();
            Serial.printf("[DEBUG] processAudioData: Async recognition failed: %s\n", error.c_str());
            handleError("异步语音识别启动失败: " + error);
        }
    } else {
        // 回退到同步识别
        Serial.printf("[DEBUG] processAudioData: Falling back to sync recognition for service '%s'\n", serviceName.c_str());
        String recognizedText;
        bool syncResult = speech->recognize(audioBuffer, audioBufferPos, recognizedText);
        Serial.printf("[DEBUG] processAudioData: sync recognize returned %s\n", syncResult ? "true" : "false");

        if (syncResult) {
            logEvent("recognition_success", String("识别结果: ") + recognizedText);
            Serial.printf("[DEBUG] processAudioData: Sync recognition successful: %s\n", recognizedText.c_str());

            // 优化内存使用顺序：语音识别完成后等待500ms再启动对话服务
            // 避免连续创建多个SSL连接导致内存分配失败
            ESP_LOGI(TAG, "Waiting 500ms before starting dialogue service...");
            delay(500);

            // 对话处理
            changeState(SystemState::THINKING);

            DialogueService* dialogue = serviceManager.getDefaultDialogueService();
            if (dialogue && dialogue->isAvailable()) {
                String response = dialogue->chat(recognizedText);
                logEvent("dialogue_response", String("对话响应: ") + response);
                playResponse(response);
            } else {
                // 降级策略：对话服务不可用时使用本地预设响应
                ESP_LOGW(TAG, "Dialogue service unavailable, using fallback response");
                String fallbackResponse = "抱歉，网络连接有问题，请稍后再试";
                logEvent("dialogue_fallback", "使用降级响应: " + fallbackResponse);
                playResponse(fallbackResponse);
            }
        } else {
            String error = speech->getLastError();
            Serial.printf("[DEBUG] processAudioData: Sync recognition failed: %s\n", error.c_str());
            handleError("语音识别失败: " + error);
        }
    }

    // 重置音频缓冲区
    audioBufferPos = 0;
}

// 播放响应
void MainApplication::playResponse(const String& text) {
    if (text.isEmpty() || !audioDriver.isReady()) return;

    // 优化内存使用顺序：对话服务完成后等待500ms再启动语音合成
    // 避免连续创建多个SSL连接导致内存分配失败
    ESP_LOGI(TAG, "Waiting 500ms before starting speech synthesis...");
    delay(500);

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

// 处理异步语音识别结果
void MainApplication::handleAsyncRecognitionResult(const AsyncRecognitionResult& result) {
    // 重置音频缓冲区（无论成功与否）
    audioBufferPos = 0;

    if (result.success) {
        logEvent("async_recognition_success", String("异步识别结果: ") + result.text);

        // 语音识别完成后，立即清理SSL资源以释放内存
        ESP_LOGI(TAG, "Cleaning SSL resources after recognition completion...");

        // 记录当前内存状态
        size_t beforeInternal = esp_get_free_internal_heap_size();
        ESP_LOGI(TAG, "Internal heap before SSL cleanup: %u bytes", beforeInternal);

        // 1. 先清理NetworkManager中的SSL客户端映射，避免悬垂指针
        ESP_LOGI(TAG, "Cleaning SSL client mappings in NetworkManager...");
        networkManager.cleanupSSLClientMappings();

        // 2. 强制清理所有SSL客户端（包括活跃的）
        SSLClientManager& sslManager = SSLClientManager::getInstance();
        sslManager.cleanupAll(true);  // 强制清理所有SSL客户端

        // 3. 清理HTTP客户端池，释放更多内存
        ESP_LOGI(TAG, "Cleaning HTTP client pool...");
        networkManager.cleanupHttpClients();

        size_t afterInternal = esp_get_free_internal_heap_size();
        ESP_LOGI(TAG, "Internal heap after cleanup: %u bytes (freed: %d bytes)",
                afterInternal, afterInternal - beforeInternal);

        // 优化内存使用顺序：语音识别完成后等待更长时间让系统稳定
        // 避免连续创建多个SSL连接导致内存分配失败
        if (afterInternal < 50000) {
            ESP_LOGW(TAG, "Low memory after cleanup (%u bytes < 50KB), waiting 2 seconds...", afterInternal);
            delay(2000);  // 内存很低时等待更久
        } else {
            ESP_LOGI(TAG, "Waiting 1 second before starting dialogue service...");
            delay(1000);
        }

        // 对话处理
        changeState(SystemState::THINKING);

        DialogueService* dialogue = serviceManager.getDefaultDialogueService();
        if (dialogue && dialogue->isAvailable()) {
            String response = dialogue->chat(result.text);
            logEvent("async_dialogue_response", String("对话响应: ") + response);
            playResponse(response);
        } else {
            // 降级策略：对话服务不可用时使用本地预设响应
            ESP_LOGW(TAG, "Dialogue service unavailable, using fallback response");
            String fallbackResponse = "抱歉，网络连接有问题，请稍后再试";
            logEvent("dialogue_fallback", "使用降级响应: " + fallbackResponse);
            playResponse(fallbackResponse);
        }
    } else {
        String error = "异步语音识别失败: " + result.errorMessage + " (code: " + String(result.errorCode) + ")";
        // 记录错误但不进入ERROR状态，提供降级响应
        logEvent("async_recognition_failed", error);
        // 播放降级响应
        String fallbackResponse = "抱歉，暂时无法连接到语音服务，请稍后再试";
        ESP_LOGW(TAG, "语音识别失败，使用降级响应: %s", fallbackResponse.c_str());
        playResponse(fallbackResponse);
        // 返回空闲状态
        changeState(SystemState::IDLE);
    }
}