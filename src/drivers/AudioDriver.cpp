#include "AudioDriver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/i2s.h>
#include <math.h>

static const char* TAG = "AudioDriver";

// ============================================================================
// 构造函数/析构函数
// ============================================================================

AudioDriver::AudioDriver() :
    isInitialized(false),
    isRecording(false),
    isPlaying(false),
    recordCallback(nullptr),
    recordCallbackUserData(nullptr),
    audioBuffer(nullptr),
    bufferWritePos(0),
    bufferReadPos(0),
    recordTaskHandle(nullptr),
    playTaskHandle(nullptr) {

    // 默认I2S配置 - 针对INMP441麦克风优化
    // INMP441输出24位数据，使用32位I2S帧（24位数据 + 8位填充）
    // 优化配置：增加DMA缓冲区，启用APLL，提高稳定性
    i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,  // 32位帧以容纳24位数据
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,  // INMP441通常在右声道输出数据
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,   // 标准飞利浦I2S格式
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,      // 从4增加到8，减少缓冲区溢出
        .dma_buf_len = 1024,     // 从512增加到1024，容纳更多数据
        .use_apll = true,        // 启用APLL提高时钟稳定性
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan = I2S_BITS_PER_CHAN_32BIT  // 每个通道32位
    };

    // 默认引脚配置
    pinConfig = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = I2S_MIC_BCLK,  // 与扬声器共用
        .ws_io_num = I2S_MIC_WS,     // 与扬声器共用
        .data_out_num = I2S_SPK_DIN,
        .data_in_num = I2S_MIC_SDO
    };
}

AudioDriver::~AudioDriver() {
    deinitialize();
}

// ============================================================================
// 初始化/反初始化
// ============================================================================

bool AudioDriver::initialize(const AudioDriverConfig& cfg) {
    if (isInitialized) {
        deinitialize();
    }

    config = cfg;

    // 更新I2S配置
    i2sConfig.sample_rate = config.sampleRate;
    i2sConfig.bits_per_sample = config.bitsPerSample;  // 使用配置的位深度
    i2sConfig.channel_format = config.channelFormat;
    // 根据bits_per_sample设置bits_per_chan
    if (config.bitsPerSample == I2S_BITS_PER_SAMPLE_16BIT) {
        i2sConfig.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;
    } else if (config.bitsPerSample == I2S_BITS_PER_SAMPLE_24BIT) {
        i2sConfig.bits_per_chan = I2S_BITS_PER_CHAN_24BIT;
    } else if (config.bitsPerSample == I2S_BITS_PER_SAMPLE_32BIT) {
        i2sConfig.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;
    } else {
        i2sConfig.bits_per_chan = I2S_BITS_PER_CHAN_16BIT; // 默认
    }

    // 分配音频缓冲区
    ESP_LOGI(TAG, "Allocating audio buffer: %zu bytes (%.1f seconds)",
             config.bufferSize, (float)config.bufferSize / (config.sampleRate * 2));
    Serial.printf("[AudioDriver] Allocating audio buffer: %zu bytes (%.1f seconds)\n",
                  config.bufferSize, (float)config.bufferSize / (config.sampleRate * 2));
    audioBuffer = new uint8_t[config.bufferSize];
    if (!audioBuffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        Serial.printf("[AudioDriver] ERROR: Failed to allocate audio buffer\n");
        return false;
    }
    Serial.printf("[AudioDriver] Audio buffer allocated at %p\n", audioBuffer);
    bufferWritePos = 0;
    bufferReadPos = 0;

    // 初始化I2S
    if (!initI2S()) {
        delete[] audioBuffer;
        audioBuffer = nullptr;
        return false;
    }

    isInitialized = true;
    ESP_LOGI(TAG, "AudioDriver initialized successfully");
    ESP_LOGI(TAG, "Sample rate: %lu, Buffer size: %zu", config.sampleRate, config.bufferSize);

    return true;
}

bool AudioDriver::deinitialize() {
    // 停止所有活动
    if (isRecording) stopRecord();
    if (isPlaying) stopPlay();

    // 释放缓冲区
    if (audioBuffer) {
        delete[] audioBuffer;
        audioBuffer = nullptr;
    }

    // 反初始化I2S
    deinitI2S();

    isInitialized = false;
    ESP_LOGI(TAG, "AudioDriver deinitialized");

    return true;
}

bool AudioDriver::initI2S() {
    esp_err_t err;

    // 记录I2S配置详情
    ESP_LOGI(TAG, "I2S Configuration for INMP441:");
    ESP_LOGI(TAG, "  Sample rate: %lu Hz", i2sConfig.sample_rate);
    ESP_LOGI(TAG, "  Bits per sample: %d", i2sConfig.bits_per_sample);
    ESP_LOGI(TAG, "  Channel format: %s",
             i2sConfig.channel_format == I2S_CHANNEL_FMT_ONLY_RIGHT ? "ONLY_RIGHT" :
             i2sConfig.channel_format == I2S_CHANNEL_FMT_ONLY_LEFT ? "ONLY_LEFT" :
             i2sConfig.channel_format == I2S_CHANNEL_FMT_RIGHT_LEFT ? "RIGHT_LEFT" : "UNKNOWN");
    ESP_LOGI(TAG, "  Communication format: 0x%X", i2sConfig.communication_format);
    ESP_LOGI(TAG, "  DMA buffers: %d x %d", i2sConfig.dma_buf_count, i2sConfig.dma_buf_len);

    // 记录引脚配置
    ESP_LOGI(TAG, "I2S Pin Configuration:");
    ESP_LOGI(TAG, "  BCLK: GPIO%d", pinConfig.bck_io_num);
    ESP_LOGI(TAG, "  WS/LRC: GPIO%d", pinConfig.ws_io_num);
    ESP_LOGI(TAG, "  Data IN (SDO): GPIO%d", pinConfig.data_in_num);
    ESP_LOGI(TAG, "  Data OUT (DIN): GPIO%d", pinConfig.data_out_num);

    // 安装I2S驱动
    ESP_LOGI(TAG, "Installing I2S driver...");
    err = i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2S driver: %s", esp_err_to_name(err));
        return false;
    }

    // 设置I2S引脚
    ESP_LOGI(TAG, "Setting I2S pins...");
    err = i2s_set_pin(I2S_NUM_0, &pinConfig);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set I2S pins: %s", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }

    ESP_LOGI(TAG, "I2S initialized successfully for INMP441 microphone");
    return true;
}

void AudioDriver::deinitI2S() {
    i2s_driver_uninstall(I2S_NUM_0);
    ESP_LOGI(TAG, "I2S deinitialized");
}

// ============================================================================
// 录音控制
// ============================================================================

bool AudioDriver::startRecord(AudioDataCallback callback, void* userData) {
    if (!isInitialized) {
        ESP_LOGE(TAG, "AudioDriver not initialized");
        return false;
    }

    if (isRecording) {
        ESP_LOGW(TAG, "Already recording");
        return true;
    }

    // 安全检查：如果任务句柄不为空，说明之前没有正确清理
    if (recordTaskHandle != nullptr) {
        ESP_LOGW(TAG, "Record task handle not null, attempting cleanup...");
        eTaskState taskState = eTaskGetState(recordTaskHandle);
        if (taskState != eDeleted && taskState != eInvalid) {
            ESP_LOGW(TAG, "Deleting stale record task (state=%d)", taskState);
            vTaskDelete(recordTaskHandle);
        }
        recordTaskHandle = nullptr;
        ESP_LOGI(TAG, "Cleaned up stale record task handle");
    }

    recordCallback = callback;
    recordCallbackUserData = userData;

    // 检查可用堆内存
    size_t freeHeap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Free heap before creating record task: %zu bytes", freeHeap);
    Serial.printf("[AudioDriver] Free heap: %zu bytes\n", freeHeap);

    // 先设置录音标志，确保任务启动时能看到正确的状态
    isRecording = true;

    // 启动录音任务 - 增加堆栈大小以解决溢出问题
    BaseType_t taskCreateResult = xTaskCreate(
        recordTask,
        "AudioRecord",
        16384,  // 堆栈大小减小到16384（16KB）以解决内存不足问题
        this,
        1,      // 优先级
        const_cast<TaskHandle_t*>(&recordTaskHandle)
    );

    if (taskCreateResult != pdPASS || recordTaskHandle == nullptr) {
        ESP_LOGE(TAG, "Failed to create record task (result=%d, handle=%p)", taskCreateResult, recordTaskHandle);
        Serial.printf("[AudioDriver] Task creation failed: result=%d, handle=%p\n", taskCreateResult, recordTaskHandle);

        // 检查具体错误
        if (taskCreateResult == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {
            ESP_LOGE(TAG, "Insufficient memory for task creation");
            Serial.println("[AudioDriver] Error: Insufficient memory");
        }

        // 任务创建失败，重置录音标志
        isRecording = false;
        return false;
    }

    ESP_LOGI(TAG, "Recording started (task created successfully)");
    Serial.printf("[AudioDriver] Task created successfully: handle=%p\n", recordTaskHandle);
    return true;
}

bool AudioDriver::stopRecord() {
    ESP_LOGI(TAG, "stopRecord: entering, isRecording=%d, recordTaskHandle=%p", isRecording, recordTaskHandle);
    if (!isRecording) {
        ESP_LOGI(TAG, "stopRecord: already not recording, returning");
        return true;
    }

    ESP_LOGI(TAG, "stopRecord: setting isRecording=false (with memory barrier)");
    portMEMORY_BARRIER();
    isRecording = false;
    portMEMORY_BARRIER(); // 确保isRecording修改对其他核心可见

    // 发送任务通知，确保录音任务能立即收到停止信号
    if (recordTaskHandle != nullptr) {
        xTaskNotifyGive(recordTaskHandle);
        ESP_LOGI(TAG, "stopRecord: task notification sent to record task");
    }

    ESP_LOGI(TAG, "stopRecord: memory barrier executed");

    // 立即让出CPU，使录音任务有机会检测到停止标志
    vTaskDelay(2 / portTICK_PERIOD_MS);

    // 等待任务自然结束（最多等待200ms，使用任务通知确保快速响应）
    uint32_t timeout = 200; // 减少到200ms，任务通知应该能立即响应
    uint32_t start = millis();

    // 获取当前任务句柄的本地副本（带内存屏障确保读取最新值）
    portMEMORY_BARRIER();
    TaskHandle_t handle = recordTaskHandle;

    ESP_LOGI(TAG, "stopRecord: waiting for task to exit, handle=%p", handle);

    while (handle != nullptr && (millis() - start) < timeout) {
        // 检查任务状态
        eTaskState taskState = eTaskGetState(handle);
        ESP_LOGI(TAG, "stopRecord: task state=%d, elapsed=%u ms", taskState, millis() - start);

        if (taskState == eDeleted || taskState == eInvalid) {
            // 任务已经结束
            ESP_LOGI(TAG, "stopRecord: task already deleted/invalid");
            portMEMORY_BARRIER();
            recordTaskHandle = nullptr;
            portMEMORY_BARRIER();
            break;
        } else if (taskState == eSuspended || taskState == eBlocked) {
            // 任务可能阻塞在i2s_read中，发送额外通知
            ESP_LOGI(TAG, "stopRecord: task is blocked/suspended, sending additional notification");
            xTaskNotifyGive(handle);
        }

        // 短暂延迟后重试
        vTaskDelay(10 / portTICK_PERIOD_MS);

        // 更新句柄副本（任务可能已退出并清除了句柄，带内存屏障）
        portMEMORY_BARRIER();
        handle = recordTaskHandle;
    }

    // 如果任务仍在运行，强制删除以避免任务泄漏
    if (handle != nullptr) {
        ESP_LOGW(TAG, "Record task didn't exit gracefully after %dms timeout, forcing delete", timeout);

        // 检查任务是否仍在运行
        eTaskState taskState = eTaskGetState(handle);
        if (taskState != eDeleted && taskState != eInvalid) {
            vTaskDelete(handle);
            ESP_LOGW(TAG, "Record task forcefully deleted");
        } else {
            ESP_LOGI(TAG, "Record task already deleted or invalid (state=%d)", taskState);
        }
        portMEMORY_BARRIER();
        recordTaskHandle = nullptr;
        portMEMORY_BARRIER();
    }

    ESP_LOGI(TAG, "Recording stopped");
    return true;
}

// ============================================================================
// 播放控制
// ============================================================================

bool AudioDriver::startPlay() {
    if (!isInitialized) {
        ESP_LOGE(TAG, "AudioDriver not initialized");
        return false;
    }

    if (isPlaying) {
        ESP_LOGW(TAG, "Already playing");
        return true;
    }

    // 启动播放任务
    xTaskCreatePinnedToCore(
        playTask,
        "AudioPlay",
        4096,
        this,
        2,
        const_cast<TaskHandle_t*>(&playTaskHandle),
        1  // 在核心1上运行
    );

    if (playTaskHandle == nullptr) {
        ESP_LOGE(TAG, "Failed to create play task");
        return false;
    }

    isPlaying = true;
    ESP_LOGI(TAG, "Playback started");
    return true;
}

bool AudioDriver::stopPlay() {
    if (!isPlaying) {
        return true;
    }

    isPlaying = false;
    portMEMORY_BARRIER(); // 确保isPlaying修改对其他核心可见

    // 等待任务自然结束（最多等待1000ms）
    uint32_t timeout = 1000; // 毫秒
    uint32_t start = millis();

    // 获取当前任务句柄的本地副本（带内存屏障确保读取最新值）
    portMEMORY_BARRIER();
    TaskHandle_t handle = playTaskHandle;

    while (handle != nullptr && (millis() - start) < timeout) {
        // 检查任务状态
        eTaskState taskState = eTaskGetState(handle);
        if (taskState == eDeleted || taskState == eInvalid) {
            // 任务已经结束
            portMEMORY_BARRIER();
            playTaskHandle = nullptr;
            portMEMORY_BARRIER();
            break;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);

        // 更新句柄副本（任务可能已退出并清除了句柄，带内存屏障）
        portMEMORY_BARRIER();
        handle = playTaskHandle;
    }

    // 如果任务仍在运行，记录警告（不强制删除以避免崩溃）
    if (handle != nullptr) {
        ESP_LOGW(TAG, "Play task didn't exit gracefully after %dms timeout", timeout);
        // 不强制删除，避免双重删除导致崩溃
        // 任务可能在稍后自行清理
        portMEMORY_BARRIER();
        playTaskHandle = nullptr;
        portMEMORY_BARRIER();
    }

    ESP_LOGI(TAG, "Playback stopped");
    return true;
}

// ============================================================================
// 音频数据操作
// ============================================================================

size_t AudioDriver::readAudioData(uint8_t* buffer, size_t maxLength) {
    if (!audioBuffer || maxLength == 0) {
        return 0;
    }

    size_t available = getAvailableData();
    if (available == 0) {
        return 0;
    }

    size_t toRead = (available < maxLength) ? available : maxLength;

    // 从环形缓冲区读取
    if (bufferReadPos + toRead <= config.bufferSize) {
        memcpy(buffer, audioBuffer + bufferReadPos, toRead);
        bufferReadPos += toRead;
    } else {
        // 需要环绕读取
        size_t firstPart = config.bufferSize - bufferReadPos;
        memcpy(buffer, audioBuffer + bufferReadPos, firstPart);
        memcpy(buffer + firstPart, audioBuffer, toRead - firstPart);
        bufferReadPos = toRead - firstPart;
    }

    // 如果读指针追上了写指针，重置位置
    if (bufferReadPos == bufferWritePos) {
        bufferReadPos = 0;
        bufferWritePos = 0;
    }

    return toRead;
}

size_t AudioDriver::writeAudioData(const uint8_t* data, size_t length) {
    if (!audioBuffer) {
        ESP_LOGW(TAG, "writeAudioData: audioBuffer is null!");
        return 0;
    }
    if (length == 0) {
        ESP_LOGW(TAG, "writeAudioData: zero length, data=%p", data);
        return 0;
    }

    size_t freeSpace = getFreeSpace();
    if (freeSpace == 0) {
        static int warnCount = 0;
        if (warnCount++ < 5) {
            ESP_LOGW(TAG, "writeAudioData: buffer full (freeSpace=0, available=%zu, bufferSize=%zu)",
                     getAvailableData(), config.bufferSize);
        }
        return 0;
    }

    size_t toWrite = (freeSpace < length) ? freeSpace : length;

    // 写入环形缓冲区
    if (bufferWritePos + toWrite <= config.bufferSize) {
        memcpy(audioBuffer + bufferWritePos, data, toWrite);
        bufferWritePos += toWrite;
    } else {
        // 需要环绕写入
        size_t firstPart = config.bufferSize - bufferWritePos;
        memcpy(audioBuffer + bufferWritePos, data, firstPart);
        memcpy(audioBuffer, data + firstPart, toWrite - firstPart);
        bufferWritePos = toWrite - firstPart;
    }

    return toWrite;
}

// ============================================================================
// 配置管理
// ============================================================================

bool AudioDriver::updateConfig(const AudioDriverConfig& newConfig) {
    if (isRecording || isPlaying) {
        ESP_LOGE(TAG, "Cannot update config while active");
        return false;
    }

    // 保存旧状态
    bool wasInitialized = isInitialized;

    if (wasInitialized) {
        deinitialize();
    }

    config = newConfig;

    if (wasInitialized) {
        return initialize(config);
    }

    return true;
}

// ============================================================================
// 音量控制
// ============================================================================

bool AudioDriver::setVolume(uint8_t volume) {
    if (volume > 100) {
        volume = 100;
    }

    config.volume = volume;

    // 注意：MAX98357A的增益通过硬件引脚设置，软件音量控制需要PWM或数字音量调节
    // 这里可以添加软件音量控制逻辑
    ESP_LOGI(TAG, "Volume set to %u%%", volume);

    return true;
}

// ============================================================================
// 状态查询
// ============================================================================

size_t AudioDriver::getAvailableData() const {
    if (!audioBuffer) {
        return 0;
    }

    if (bufferWritePos >= bufferReadPos) {
        return bufferWritePos - bufferReadPos;
    } else {
        return (config.bufferSize - bufferReadPos) + bufferWritePos;
    }
}

size_t AudioDriver::getFreeSpace() const {
    if (!audioBuffer) {
        return 0;
    }

    size_t available = getAvailableData();
    return config.bufferSize - available - 1; // 保留一个字节防止完全满
}

// ============================================================================
// 工具方法
// ============================================================================

void AudioDriver::printI2SConfig(const i2s_config_t& config) {
    ESP_LOGI(TAG, "I2S Config:");
    ESP_LOGI(TAG, "  Mode: 0x%08X", config.mode);
    ESP_LOGI(TAG, "  Sample Rate: %lu", config.sample_rate);
    ESP_LOGI(TAG, "  Bits per Sample: %d", config.bits_per_sample);
    ESP_LOGI(TAG, "  Channel Format: %d", config.channel_format);
    ESP_LOGI(TAG, "  DMA Buffer Count: %d", config.dma_buf_count);
    ESP_LOGI(TAG, "  DMA Buffer Length: %d", config.dma_buf_len);
}

void AudioDriver::printPinConfig(const i2s_pin_config_t& config) {
    ESP_LOGI(TAG, "I2S Pin Config:");
    ESP_LOGI(TAG, "  BCK: %d", config.bck_io_num);
    ESP_LOGI(TAG, "  WS: %d", config.ws_io_num);
    ESP_LOGI(TAG, "  Data Out: %d", config.data_out_num);
    ESP_LOGI(TAG, "  Data In: %d", config.data_in_num);
}

// ============================================================================
// 测试方法
// ============================================================================

bool AudioDriver::testMic() {
    if (!isInitialized) {
        ESP_LOGE(TAG, "AudioDriver not initialized");
        return false;
    }

    ESP_LOGI(TAG, "Testing INMP441 microphone with 30-second extended test...");

    // 等待I2S稳定 - INMP441麦克风需要2.3秒启动时间（硬件特性）
    ESP_LOGI(TAG, "Waiting 2.3 seconds for INMP441 microphone startup...");
    vTaskDelay(2300 / portTICK_PERIOD_MS);

    // 测试配置：通信格式 + 声道格式 + 引脚配置
    struct TestConfig {
        const char* name;
        i2s_comm_format_t commFormat;
        i2s_channel_fmt_t channelFormat;
        int bckPin;
        int wsPin;
    };

    // 关键配置组合（基于INMP441常见问题）
    TestConfig testConfigs[] = {
        // 标准配置 - 当前设置
        {"STAND_I2S + ONLY_LEFT (标准)", I2S_COMM_FORMAT_STAND_I2S, I2S_CHANNEL_FMT_ONLY_LEFT, pinConfig.bck_io_num, pinConfig.ws_io_num},
        {"STAND_I2S + ONLY_RIGHT", I2S_COMM_FORMAT_STAND_I2S, I2S_CHANNEL_FMT_ONLY_RIGHT, pinConfig.bck_io_num, pinConfig.ws_io_num},
        {"STAND_MSB + ONLY_LEFT", I2S_COMM_FORMAT_STAND_MSB, I2S_CHANNEL_FMT_ONLY_LEFT, pinConfig.bck_io_num, pinConfig.ws_io_num},
        {"STAND_MSB + ONLY_RIGHT", I2S_COMM_FORMAT_STAND_MSB, I2S_CHANNEL_FMT_ONLY_RIGHT, pinConfig.bck_io_num, pinConfig.ws_io_num},

        // 交换BCLK/WS引脚（硬件接线可能反了）
        {"STAND_I2S + ONLY_LEFT (交换引脚)", I2S_COMM_FORMAT_STAND_I2S, I2S_CHANNEL_FMT_ONLY_LEFT, pinConfig.ws_io_num, pinConfig.bck_io_num},
        {"STAND_I2S + ONLY_RIGHT (交换引脚)", I2S_COMM_FORMAT_STAND_I2S, I2S_CHANNEL_FMT_ONLY_RIGHT, pinConfig.ws_io_num, pinConfig.bck_io_num},
        {"STAND_MSB + ONLY_LEFT (交换引脚)", I2S_COMM_FORMAT_STAND_MSB, I2S_CHANNEL_FMT_ONLY_LEFT, pinConfig.ws_io_num, pinConfig.bck_io_num},
        {"STAND_MSB + ONLY_RIGHT (交换引脚)", I2S_COMM_FORMAT_STAND_MSB, I2S_CHANNEL_FMT_ONLY_RIGHT, pinConfig.ws_io_num, pinConfig.bck_io_num},
    };

    int numConfigs = sizeof(testConfigs) / sizeof(testConfigs[0]);
    bool testPassed = false;
    TestConfig* bestConfig = nullptr;
    double bestRMS = 0.0;
    uint32_t bestConfigIndex = 0;

    ESP_LOGI(TAG, "Will test %d configurations over 30 seconds (~3.75 seconds per config)", numConfigs);

    // 测试每个配置（约30秒总测试时间）
    for (int configIdx = 0; configIdx < numConfigs && !testPassed; configIdx++) {
        TestConfig& config = testConfigs[configIdx];

        ESP_LOGI(TAG, "=== Testing config %d/%d: %s ===",
                 configIdx + 1, numConfigs, config.name);
        ESP_LOGI(TAG, "  Comm format: 0x%X, Channel: %s, BCLK: GPIO%d, WS: GPIO%d",
                 config.commFormat,
                 config.channelFormat == I2S_CHANNEL_FMT_ONLY_LEFT ? "ONLY_LEFT" :
                 config.channelFormat == I2S_CHANNEL_FMT_ONLY_RIGHT ? "ONLY_RIGHT" : "UNKNOWN",
                 config.bckPin, config.wsPin);

        // 重新配置I2S
        i2s_driver_uninstall(I2S_NUM_0);

        i2s_config_t tempConfig = i2sConfig;
        tempConfig.channel_format = config.channelFormat;
        tempConfig.communication_format = config.commFormat;

        i2s_pin_config_t tempPinConfig = pinConfig;
        tempPinConfig.bck_io_num = config.bckPin;
        tempPinConfig.ws_io_num = config.wsPin;

        esp_err_t err = i2s_driver_install(I2S_NUM_0, &tempConfig, 0, nullptr);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "  Failed to install I2S driver: %s", esp_err_to_name(err));
            continue;
        }

        err = i2s_set_pin(I2S_NUM_0, &tempPinConfig);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "  Failed to set I2S pins: %s", esp_err_to_name(err));
            i2s_driver_uninstall(I2S_NUM_0);
            continue;
        }

        // 等待重新配置稳定
        vTaskDelay(100 / portTICK_PERIOD_MS);

        // 清空缓冲区（丢弃可能存在的旧数据）
        uint8_t discardBuffer[256];
        size_t discarded = 0;
        i2s_read(I2S_NUM_0, discardBuffer, sizeof(discardBuffer), &discarded, 0);
        vTaskDelay(50 / portTICK_PERIOD_MS);

        // 对此配置进行持续测试（约3.75秒）
        double configMaxRMS = 0.0;
        bool configHasNonZero = false;
        uint32_t startTime = millis();
        int readAttempts = 0;

        while (millis() - startTime < 3750 && !testPassed) { // 每个配置测试~3.75秒
            readAttempts++;

            uint8_t testBuffer[512];
            size_t bytesRead = 0;

            // 尝试读取数据（100ms超时）
            err = i2s_read(I2S_NUM_0, testBuffer, sizeof(testBuffer), &bytesRead, 100 / portTICK_PERIOD_MS);

            if (err == ESP_OK && bytesRead > 0) {
                // 转换32位I2S数据到16位PCM
                size_t sampleCount = bytesRead / 4;
                if (sampleCount == 0) continue;

                // 检查样本是否全为零
                bool allZero = true;
                int64_t sumSquares = 0;
                int32_t* samples = (int32_t*)testBuffer;

                // 只检查前32个样本以加快速度
                size_t checkCount = sampleCount > 32 ? 32 : sampleCount;
                for (size_t i = 0; i < checkCount; i++) {
                    int32_t sample = samples[i];
                    // 转换为16位（右移8位）
                    int16_t sample16 = (int16_t)(sample >> 8);
                    sumSquares += (int64_t)sample16 * sample16;
                    if (sample16 != 0) allZero = false;
                }

                double rms = sqrt((double)sumSquares / checkCount);
                double dbFS = (rms > 0) ? 20 * log10(rms / 32768.0) : -100.0;

                // 每5次读取或当发现非零数据时记录日志
                if (readAttempts % 5 == 0 || !allZero) {
                    ESP_LOGI(TAG, "  Read %d: %zu samples, RMS=%.1f dBFS, allZero=%s",
                             readAttempts, sampleCount, dbFS, allZero ? "YES" : "NO");

                    if (allZero && readAttempts == 1) {
                        ESP_LOGI(TAG, "    Raw samples: 0x%08X 0x%08X 0x%08X 0x%08X",
                                 samples[0], samples[1], samples[2], samples[3]);
                    }
                }

                // 记录最佳结果
                if (rms > configMaxRMS) {
                    configMaxRMS = rms;
                }

                if (!allZero) {
                    configHasNonZero = true;

                    // 如果达到阈值，测试通过
                    if (rms > 10.0) {
                        testPassed = true;
                        bestConfig = &config;
                        bestRMS = rms;
                        bestConfigIndex = configIdx;
                        ESP_LOGI(TAG, "  *** FOUND VALID AUDIO! RMS=%.1f ***", rms);
                        break;
                    }
                }
            } else if (err != ESP_OK) {
                if (readAttempts % 10 == 0) {
                    ESP_LOGW(TAG, "  I2S read error: %s", esp_err_to_name(err));
                }
            }

            // 短暂延迟（50ms）
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        // 记录此配置的最佳结果
        if (configMaxRMS > bestRMS) {
            bestRMS = configMaxRMS;
            bestConfig = &config;
            bestConfigIndex = configIdx;
        }

        ESP_LOGI(TAG, "  Config result: max RMS=%.1f, hasNonZero=%s",
                 configMaxRMS, configHasNonZero ? "YES" : "NO");
        ESP_LOGI(TAG, "  Time spent: %.1f seconds", (millis() - startTime) / 1000.0);
    }

    // 所有配置测试完成，恢复最佳配置
    if (bestConfig != nullptr) {
        ESP_LOGI(TAG, "=== Best configuration: #%d %s ===",
                 bestConfigIndex + 1, bestConfig->name);
        ESP_LOGI(TAG, "  Max RMS achieved: %.1f", bestRMS);

        // 重新安装最佳配置
        i2s_driver_uninstall(I2S_NUM_0);

        i2s_config_t finalConfig = i2sConfig;
        finalConfig.channel_format = bestConfig->channelFormat;
        finalConfig.communication_format = bestConfig->commFormat;

        i2s_pin_config_t finalPinConfig = pinConfig;
        finalPinConfig.bck_io_num = bestConfig->bckPin;
        finalPinConfig.ws_io_num = bestConfig->wsPin;

        esp_err_t err = i2s_driver_install(I2S_NUM_0, &finalConfig, 0, nullptr);
        if (err == ESP_OK) {
            i2s_set_pin(I2S_NUM_0, &finalPinConfig);

            // 更新全局配置
            i2sConfig.channel_format = bestConfig->channelFormat;
            i2sConfig.communication_format = bestConfig->commFormat;
            pinConfig.bck_io_num = bestConfig->bckPin;
            pinConfig.ws_io_num = bestConfig->wsPin;

            ESP_LOGI(TAG, "  Configuration updated successfully");
        }
    }

    // 检查最终结果
    if (testPassed) {
        ESP_LOGI(TAG, "Microphone test PASSED! Configuration: %s, RMS=%.1f",
                 bestConfig->name, bestRMS);
        return true;
    } else if (bestRMS > 1.0) {
        ESP_LOGW(TAG, "Microphone test PASSED (weak signal). Best config: %s, RMS=%.1f",
                 bestConfig != nullptr ? bestConfig->name : "NONE", bestRMS);
        return true;
    } else if (bestRMS > 0) {
        ESP_LOGW(TAG, "Microphone test FAILED (too quiet). Best config: %s, RMS=%.1f",
                 bestConfig != nullptr ? bestConfig->name : "NONE", bestRMS);
    } else {
        ESP_LOGE(TAG, "Microphone test FAILED (no audio detected in any configuration)");
    }

    // 详细的硬件检查清单
    ESP_LOGE(TAG, "==========================================");
    ESP_LOGE(TAG, "HARDWARE DIAGNOSTIC CHECKLIST:");
    ESP_LOGE(TAG, "==========================================");
    ESP_LOGE(TAG, "1. POWER SUPPLY (MOST IMPORTANT):");
    ESP_LOGE(TAG, "   - Measure INMP441 VDD-GND with multimeter: MUST be 3.3V ±5%");
    ESP_LOGE(TAG, "   - ESP32 3.3V pin MUST connect to INMP441 VDD (pin 1)");
    ESP_LOGE(TAG, "   - ESP32 GND MUST connect to INMP441 GND (pin 4)");
    ESP_LOGE(TAG, "   - L/R pin (pin 3) MUST be grounded (connect to GND)");

    ESP_LOGE(TAG, "2. PIN CONNECTIONS (YOUR CONFIG):");
    ESP_LOGE(TAG, "   - SDO (pin 2) -> GPIO14 (data)");
    ESP_LOGE(TAG, "   - WS  (pin 5) -> GPIO16 (LR clock)");
    ESP_LOGE(TAG, "   - SCK (pin 6) -> GPIO15 (bit clock)");

    ESP_LOGE(TAG, "3. HARDWARE TESTS:");
    ESP_LOGE(TAG, "   - Try SWAPPING BCLK and WS wires (GPIO15 ↔ GPIO16)");
    ESP_LOGE(TAG, "   - Check ALL Dupont wire connections are SECURE");
    ESP_LOGE(TAG, "   - Try different GPIO pins for I2S (e.g., 12, 13, 14)");
    ESP_LOGE(TAG, "   - Test with INMP441 test program first (INMP441 project)");

    ESP_LOGE(TAG, "4. MODULE VERIFICATION:");
    ESP_LOGE(TAG, "   - INMP441 module may be defective or counterfeit");
    ESP_LOGE(TAG, "   - Try with a known-working INMP441 module");
    ESP_LOGE(TAG, "   - Check module orientation (look for dot/mark on chip)");

    ESP_LOGE(TAG, "5. ESP32 CONFIGURATION:");
    ESP_LOGE(TAG, "   - Ensure ESP32-S3 board is selected in PlatformIO");
    ESP_LOGE(TAG, "   - Check I2S pins are not used by other peripherals");
    ESP_LOGE(TAG, "   - Verify correct USB port is selected for upload");

    ESP_LOGE(TAG, "==========================================");
    ESP_LOGE(TAG, "QUICK TEST: Upload INMP441 test project first!");
    ESP_LOGE(TAG, "Path: Documents/PlatformIO/Projects/INMP441");
    ESP_LOGE(TAG, "==========================================");

    // 恢复到原始配置（以防后续代码依赖）
    i2s_driver_uninstall(I2S_NUM_0);
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, nullptr);
    if (err == ESP_OK) {
        i2s_set_pin(I2S_NUM_0, &pinConfig);
    }

    return false;
}

bool AudioDriver::testSpeaker() {
    if (!isInitialized) {
        ESP_LOGE(TAG, "AudioDriver not initialized");
        return false;
    }

    ESP_LOGI(TAG, "Testing speaker...");

    // 生成测试音调（正弦波）
    const size_t testLength = 1024;
    uint8_t testTone[testLength];
    const float frequency = 440.0f; // A4音
    const float amplitude = 0.5f;

    for (size_t i = 0; i < testLength / 2; i++) {
        int16_t sample = (int16_t)(amplitude * 32767.0f *
            sin(2.0f * M_PI * frequency * i / config.sampleRate));
        testTone[i * 2] = sample & 0xFF;
        testTone[i * 2 + 1] = (sample >> 8) & 0xFF;
    }

    // 直接使用i2s_write测试扬声器，不依赖环形缓冲区
    size_t bytesWritten = 0;
    esp_err_t err = i2s_write(I2S_NUM_0, testTone, testLength, &bytesWritten, portMAX_DELAY);
    if (err == ESP_OK && bytesWritten == testLength) {
        ESP_LOGI(TAG, "Speaker test passed: wrote %zu bytes", bytesWritten);
        return true;
    } else {
        ESP_LOGE(TAG, "Speaker test failed: err=%s, written %zu/%zu bytes",
                 err != ESP_OK ? esp_err_to_name(err) : "OK", bytesWritten, testLength);
        return false;
    }
}

// ============================================================================
// 任务函数
// ============================================================================

void AudioDriver::recordTask(void* parameter) {
    AudioDriver* driver = static_cast<AudioDriver*>(parameter);
    if (!driver) {
        ESP_LOGE(TAG, "Record task: invalid driver parameter");
        Serial.println("[AudioDriver] Record task: driver parameter is null!");
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "Record task started");
    Serial.printf("[AudioDriver] Record task: driver=%p, isRecording=%d\n", driver, driver->isRecording);

    // 获取当前任务句柄用于栈监控
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();

    // 短暂延迟确保状态稳定
    vTaskDelay(10 / portTICK_PERIOD_MS);

    size_t bytesRead = 0;
    uint8_t readBuffer[1024];  // 减小缓冲区大小以减少I2S读取阻塞时间
    uint32_t readCount = 0;
    uint32_t totalBytesRead = 0;
    uint32_t lastStackReport = 0;
    static uint32_t consecutiveZeroReads = 0;  // 跟踪连续0字节读取次数

    // 主录音循环 - 使用原子操作和任务通知确保快速响应
    while (driver) {
        portMEMORY_BARRIER();

        // 调试：记录循环次数（每50次记录一次，避免日志过多）
        if (readCount % 50 == 0) {
            ESP_LOGI(TAG, "Record task: loop iteration %u", readCount);
        }

        // 检查停止标志 - 使用volatile读取加内存屏障确保多核心可见性
        portMEMORY_BARRIER();
        bool shouldContinue = driver->isRecording;
        portMEMORY_BARRIER();
        // 调试：记录标志状态（每10次循环记录一次，避免日志过多）
        if (readCount % 10 == 0) {
            ESP_LOGI(TAG, "Record task: flag check, shouldContinue=%d, readCount=%u", shouldContinue, readCount);
        }
        if (!shouldContinue) {
            ESP_LOGI(TAG, "Record task: flag cleared (readCount=%u), breaking loop", readCount);
            break;
        }

        // 检查任务通知（非阻塞，立即返回）
        uint32_t notificationValue = ulTaskNotifyTake(pdTRUE, 0);
        // 调试：记录通知状态（每10次循环记录一次）
        if (readCount % 10 == 0) {
            ESP_LOGI(TAG, "Record task: notification check, value=%u, readCount=%u", notificationValue, readCount);
        }
        if (notificationValue > 0) {
            ESP_LOGI(TAG, "Record task: received stop notification (%u, readCount=%u), breaking loop", notificationValue, readCount);
            break;
        }

        // 定期检查栈使用情况（每100次循环）
        if (readCount % 100 == 0) {
            UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(currentTask);
            if (stackHighWaterMark != lastStackReport) {
                Serial.printf("[AudioDriver] Stack high water mark: %u bytes (task handle: %p)\n",
                             stackHighWaterMark * sizeof(StackType_t), currentTask);
                lastStackReport = stackHighWaterMark;
            }
        }

        // 在I2S读取前再次检查停止标志，确保快速响应
        portMEMORY_BARRIER();
        shouldContinue = driver->isRecording;
        portMEMORY_BARRIER();
        // 调试：记录标志状态
        if (readCount % 10 == 0) {
            ESP_LOGI(TAG, "Record task: pre-i2s flag check, shouldContinue=%d, readCount=%u", shouldContinue, readCount);
        }
        if (!shouldContinue) {
            ESP_LOGI(TAG, "Record task: flag cleared before i2s_read (readCount=%u), breaking loop", readCount);
            break;
        }

        // 再次检查任务通知
        notificationValue = ulTaskNotifyTake(pdTRUE, 0);
        // 调试：记录通知状态
        if (readCount % 10 == 0) {
            ESP_LOGI(TAG, "Record task: pre-i2s notification check, value=%u, readCount=%u", notificationValue, readCount);
        }
        if (notificationValue > 0) {
            ESP_LOGI(TAG, "Record task: received stop notification before i2s_read (%u, readCount=%u), breaking loop", notificationValue, readCount);
            break;
        }

        // 从I2S读取数据（使用20ms超时，平衡响应性和数据可用性）
        esp_err_t err = i2s_read(I2S_NUM_0, readBuffer, sizeof(readBuffer), &bytesRead, pdMS_TO_TICKS(20));

        // 调试日志：记录读取结果
        if (readCount < 10 || readCount % 10 == 0) {
            ESP_LOGI(TAG, "I2S read: err=%d, bytesRead=%zu, buffer=%p", err, bytesRead, readBuffer);
        }

        if (err == ESP_OK && bytesRead > 0) {
            totalBytesRead += bytesRead;
            readCount++;

            size_t originalBytesRead = bytesRead; // 保存原始字节数用于日志记录

            // 处理I2S数据（INMP441输出24位数据，使用32位I2S帧）
            // 将32位数据转换为16位（右移8位，24位->16位）
            size_t sampleCount32 = bytesRead / 4;  // 32位样本数
            size_t sampleCount16 = sampleCount32;  // 16位样本数（转换后）
            int32_t* samples32 = (int32_t*)readBuffer;
            int16_t* samples16 = (int16_t*)readBuffer; // 原地转换

            // 音频处理参数
            const int16_t MAX_SAMPLE = 32767;
            const int16_t MIN_SAMPLE = -32768;

            // 自动增益控制（AGC）参数
            static float currentGain = 3.0f;  // 初始增益
            const float TARGET_RMS = 15000.0f;  // 目标RMS值
            const float GAIN_ADJUST_RATE = 0.01f;  // 增益调整速率
            const float MIN_GAIN = 1.0f;
            const float MAX_GAIN = 10.0f;

            // 噪声门参数
            const int16_t NOISE_GATE_THRESHOLD = 500;  // 低于此值视为噪声
            static int32_t dcOffset = 0;  // 直流偏移估计
            const float DC_OFFSET_ALPHA = 0.001f;  // 直流偏移估计平滑因子

            // 统计变量（原始24位数据）
            int64_t rawSumSquares = 0;
            int32_t rawSumSamples = 0;
            int16_t rawMaxSample = 0;
            int16_t rawMinSample = 0;

            // 第一次循环：计算统计信息
            for (size_t i = 0; i < sampleCount32; i++) {
                // 32位样本：高24位是音频数据，低8位为0
                // 提取24位音频数据（右移8位）
                int32_t sample32 = samples32[i];
                int32_t sample24 = sample32 >> 8;  // 32位->24位

                // 更新直流偏移估计（指数移动平均）
                dcOffset = (int32_t)((1.0f - DC_OFFSET_ALPHA) * dcOffset + DC_OFFSET_ALPHA * sample24);

                // 移除直流偏移
                int32_t centeredSample = sample24 - dcOffset;

                // 累积统计信息
                rawSumSamples += centeredSample;
                rawSumSquares += (int64_t)centeredSample * centeredSample;

                if (centeredSample > rawMaxSample) rawMaxSample = (int16_t)(centeredSample >> 8);  // 粗略估计
                if (centeredSample < rawMinSample) rawMinSample = (int16_t)(centeredSample >> 8);

                // 每64个样本检查一次停止标志
                if ((i & 0x3F) == 0x3F) {
                    portMEMORY_BARRIER();
                    bool shouldContinue = driver->isRecording;
                    portMEMORY_BARRIER();
                    if (!shouldContinue) {
                        ESP_LOGI(TAG, "Record task: flag cleared during audio conversion (i=%zu), breaking outer loop", i);
                        goto outer_loop_exit;
                    }
                }
            }

            // 计算RMS并调整增益
            double rawRms = (sampleCount32 > 0) ? sqrt((double)rawSumSquares / sampleCount32) : 0.0;
            if (rawRms > 100.0) {  // 避免在静音时调整增益
                float desiredGain = TARGET_RMS / rawRms;
                // 限制增益范围并平滑调整
                if (desiredGain < MIN_GAIN) desiredGain = MIN_GAIN;
                if (desiredGain > MAX_GAIN) desiredGain = MAX_GAIN;
                currentGain = currentGain * (1.0f - GAIN_ADJUST_RATE) + desiredGain * GAIN_ADJUST_RATE;
            }

            // 第二次循环：应用处理
            for (size_t i = 0; i < sampleCount32; i++) {
                // 提取24位音频数据
                int32_t sample32 = samples32[i];
                int32_t sample24 = sample32 >> 8;  // 32位->24位

                // 移除直流偏移
                int32_t centeredSample = sample24 - dcOffset;

                // 应用自动增益
                int32_t amplifiedSample = (int32_t)(centeredSample * currentGain);

                // 24位->16位转换（右移8位）
                int16_t sample16 = (int16_t)(amplifiedSample >> 8);

                // 应用噪声门
                if (abs(sample16) < NOISE_GATE_THRESHOLD) {
                    sample16 = 0;
                }

                // 限制在16位范围内
                if (sample16 > MAX_SAMPLE) sample16 = MAX_SAMPLE;
                if (sample16 < MIN_SAMPLE) sample16 = MIN_SAMPLE;

                samples16[i] = sample16;

                // 每64个样本检查一次停止标志
                if ((i & 0x3F) == 0x3F) {
                    portMEMORY_BARRIER();
                    bool shouldContinue = driver->isRecording;
                    portMEMORY_BARRIER();
                    if (!shouldContinue) {
                        ESP_LOGI(TAG, "Record task: flag cleared during audio processing (i=%zu), breaking outer loop", i);
                        goto outer_loop_exit;
                    }
                }
            }

            // 更新字节数：32位->16位转换后大小减半
            bytesRead = sampleCount16 * 2;

            // 调试：记录增益处理后的样本统计
            static int debugCount = 0;
            if (debugCount++ < 5) {
                int64_t sumAbs = 0;
                int16_t maxSample = 0;
                int16_t minSample = 0;
                for (size_t i = 0; i < sampleCount16 && i < 10; i++) {
                    int16_t s = samples16[i];
                    sumAbs += abs(s);
                    if (s > maxSample) maxSample = s;
                    if (s < minSample) minSample = s;
                }
                int avgAmplitude = (sampleCount16 > 0 && sampleCount16 < 10) ?
                                   (sumAbs / sampleCount16) : (sumAbs / 10);
                ESP_LOGI(TAG, "Gain debug[%d]: first sample=0x%04X (%d), avg=%d, range=[%d,%d]",
                         debugCount, samples16[0] & 0xFFFF, samples16[0], avgAmplitude, minSample, maxSample);
            }

            // 音频能量检测（每5次读取记录一次，前10次每次都记录）
            bool shouldLog = (readCount <= 10) || (readCount % 5 == 0);

            // 总是计算RMS用于VAD，但只在需要时记录日志
            int64_t sumSquares = 0;
            int16_t maxSample = 0;
            int16_t minSample = 0;

            for (size_t i = 0; i < sampleCount16; i++) {
                int16_t sample = samples16[i];
                sumSquares += (int64_t)sample * sample;

                // 只在需要记录日志时计算最大值/最小值
                if (shouldLog) {
                    if (sample > maxSample) maxSample = sample;
                    if (sample < minSample) minSample = sample;
                }

                // 每128个样本检查一次停止标志，确保快速响应
                if ((i & 0x7F) == 0x7F) { // i % 128 == 127
                    portMEMORY_BARRIER();
                    bool shouldContinue = driver->isRecording;
                    portMEMORY_BARRIER();
                    if (!shouldContinue) {
                        ESP_LOGI(TAG, "Record task: flag cleared during energy calculation (i=%zu), breaking outer loop", i);
                        goto outer_loop_exit;
                    }
                }
            }

            double rms = (sampleCount16 > 0) ? sqrt((double)sumSquares / sampleCount16) : 0.0;

            if (shouldLog) {
                double dbFS = (rms > 0) ? 20 * log10(rms / 32768.0) : -100.0;
                ESP_LOGI(TAG, "Record[%u]: %zu bytes -> %zu bytes, RMS=%.1f (%.1f dBFS), range=[%d,%d]",
                        readCount, originalBytesRead, bytesRead, rms, dbFS, minSample, maxSample);

                // 如果前几次读取都是静音，警告可能的硬件问题
                if (readCount <= 5 && rms < 10.0) {
                    ESP_LOGW(TAG, "WARNING: Audio appears silent (RMS=%.1f). Check microphone!", rms);
                }

                // 音频质量检查
                if (rms > 0) {
                    // 计算质量评分（0.0-1.0）
                    float qualityScore = 0.0f;

                    // RMS评分：目标>15000，满分为1.0
                    float rmsScore = fminf(rms / 15000.0f, 1.0f);

                    // 动态范围评分：范围越大越好
                    int16_t dynamicRange = maxSample - minSample;
                    float dynamicScore = fminf(dynamicRange / 30000.0f, 1.0f);

                    // 零交叉率评分：目标范围1000-3000 Hz
                    // 这里简化：使用固定值0.7，实际需要计算
                    float zcrScore = 0.7f;

                    // 综合评分
                    qualityScore = rmsScore * 0.5f + dynamicScore * 0.3f + zcrScore * 0.2f;

                    // 记录质量信息
                    if (qualityScore < 0.3f) {
                        ESP_LOGE(TAG, "POOR audio quality: score=%.2f, RMS=%.1f, range=[%d,%d]",
                                qualityScore, rms, minSample, maxSample);
                    } else if (qualityScore < 0.6f) {
                        ESP_LOGW(TAG, "MEDIUM audio quality: score=%.2f, RMS=%.1f, range=[%d,%d]",
                                qualityScore, rms, minSample, maxSample);
                    } else if (readCount % 10 == 0) { // 避免日志过多
                        ESP_LOGI(TAG, "GOOD audio quality: score=%.2f, RMS=%.1f, range=[%d,%d]",
                                qualityScore, rms, minSample, maxSample);
                    }
                }
            }

            // 写入环形缓冲区（使用转换后的16位数据）
            size_t written = 0;
            if (driver) {
                written = driver->writeAudioData(readBuffer, bytesRead);
            }

            // 调用回调函数（使用转换后的16位数据）
            if (driver && driver->recordCallback && written > 0) {
                // 回调前检查停止标志
                portMEMORY_BARRIER();
                bool shouldContinue = driver->isRecording;
                portMEMORY_BARRIER();
                if (!shouldContinue) {
                    ESP_LOGI(TAG, "Record task: flag cleared before callback");
                    break;
                }

                // 记录回调执行时间
                uint32_t callbackStart = millis();
                driver->recordCallback(readBuffer, written, driver->recordCallbackUserData);
                uint32_t callbackDuration = millis() - callbackStart;
                if (callbackDuration > 50) {
                    ESP_LOGW(TAG, "Record task: callback took %u ms (may affect stop response)", callbackDuration);
                }
            } else if (driver && driver->recordCallback && written == 0) {
                ESP_LOGW(TAG, "Record task: buffer full, written=0");
            }

            // 在循环结束前再次检查停止标志，确保最快响应
            portMEMORY_BARRIER();
            bool shouldContinue = driver->isRecording;
            portMEMORY_BARRIER();
            if (!shouldContinue) {
                ESP_LOGI(TAG, "Record task: flag cleared after callback, breaking loop");
                break;
            }
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2S read error: %s", esp_err_to_name(err));
        } else if (bytesRead == 0) {
            // 无数据读取（20ms超时内无数据）
            static uint32_t consecutiveZeroReads = 0;
            consecutiveZeroReads++;

            if (consecutiveZeroReads <= 5) {
                ESP_LOGW(TAG, "Record task: i2s_read returned 0 bytes (consecutive: %u)", consecutiveZeroReads);
            } else if (consecutiveZeroReads <= 20) {
                ESP_LOGW(TAG, "Record task: WARNING - %u consecutive zero-byte reads", consecutiveZeroReads);
                // 短延迟让出CPU，避免忙等待
                vTaskDelay(pdMS_TO_TICKS(5));
            } else {
                ESP_LOGE(TAG, "Record task: CRITICAL - %u consecutive zero-byte reads, potential I2S issue", consecutiveZeroReads);
                // 重置计数器避免日志泛滥
                consecutiveZeroReads = 15;
                // 更长延迟
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            // 立即检查停止标志，确保最快响应
            portMEMORY_BARRIER();
            bool shouldContinue = driver->isRecording;
            portMEMORY_BARRIER();
            if (!shouldContinue) {
                ESP_LOGI(TAG, "Record task: flag cleared after i2s_read returned 0");
                break;
            }
            // 让出CPU，使其他任务（如stopRecord）有机会运行
            taskYIELD();
        }

        // 内存屏障确保下次读取isRecording时看到最新值
        portMEMORY_BARRIER();
        // 移除vTaskDelay以最大化响应性，i2s_read的1ms超时已足够
    }

outer_loop_exit:
    ESP_LOGI(TAG, "Record task ended");
    if (driver) {
        driver->clearRecordTaskHandle();
    }
    vTaskDelete(nullptr);
}

void AudioDriver::playTask(void* parameter) {
    AudioDriver* driver = static_cast<AudioDriver*>(parameter);
    if (!driver) {
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "Play task started");

    size_t bytesToWrite = 0;
    uint8_t writeBuffer[1024];

    while (driver->isPlaying) {
        // 从环形缓冲区读取数据
        bytesToWrite = driver->readAudioData(writeBuffer, sizeof(writeBuffer));

        if (bytesToWrite > 0) {
            // 写入I2S
            size_t bytesWritten = 0;
            esp_err_t err = i2s_write(I2S_NUM_0, writeBuffer, bytesToWrite, &bytesWritten, portMAX_DELAY);

            if (err != ESP_OK) {
                ESP_LOGE(TAG, "I2S write error: %s", esp_err_to_name(err));
            }
        } else {
            // 无数据，短暂休眠
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }

    ESP_LOGI(TAG, "Play task ended");
    if (driver) {
        driver->clearPlayTaskHandle();
    }
    vTaskDelete(nullptr);
}