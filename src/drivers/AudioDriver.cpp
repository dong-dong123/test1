#include "AudioDriver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/i2s.h>

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

    // 默认I2S配置
    i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan = I2S_BITS_PER_CHAN_16BIT
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
    i2sConfig.bits_per_sample = config.bitsPerSample;
    i2sConfig.channel_format = config.channelFormat;

    // 分配音频缓冲区
    audioBuffer = new uint8_t[config.bufferSize];
    if (!audioBuffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        return false;
    }
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

    // 安装I2S驱动
    err = i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2S driver: %s", esp_err_to_name(err));
        return false;
    }

    // 设置I2S引脚
    err = i2s_set_pin(I2S_NUM_0, &pinConfig);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set I2S pins: %s", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }

    ESP_LOGI(TAG, "I2S initialized successfully");
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
        32768,  // 堆栈大小从4096增加到32768（32KB）以解决堆栈溢出
        this,
        1,      // 优先级
        &recordTaskHandle
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
    if (!isRecording) {
        return true;
    }

    isRecording = false;

    // 等待任务自然结束（最多等待1000ms）
    uint32_t timeout = 1000; // 毫秒
    uint32_t start = millis();

    while (recordTaskHandle != nullptr && (millis() - start) < timeout) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    // 如果任务仍在运行，强制删除以避免任务泄漏
    if (recordTaskHandle != nullptr) {
        ESP_LOGW(TAG, "Record task didn't exit gracefully after %dms timeout, forcing delete", timeout);

        // 检查任务是否仍在运行
        eTaskState taskState = eTaskGetState(recordTaskHandle);
        if (taskState != eDeleted && taskState != eInvalid) {
            vTaskDelete(recordTaskHandle);
            ESP_LOGW(TAG, "Record task forcefully deleted");
        } else {
            ESP_LOGI(TAG, "Record task already deleted or invalid (state=%d)", taskState);
        }
        recordTaskHandle = nullptr;
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
        &playTaskHandle,
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

    // 等待任务自然结束（最多等待1000ms）
    uint32_t timeout = 1000; // 毫秒
    uint32_t start = millis();

    while (playTaskHandle != nullptr && (millis() - start) < timeout) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    // 如果任务仍在运行，记录警告（不强制删除以避免崩溃）
    if (playTaskHandle != nullptr) {
        ESP_LOGW(TAG, "Play task didn't exit gracefully after %dms timeout", timeout);
        // 不强制删除，避免双重删除导致崩溃
        // 任务可能在稍后自行清理
        playTaskHandle = nullptr;
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
    if (!audioBuffer || length == 0) {
        return 0;
    }

    size_t freeSpace = getFreeSpace();
    if (freeSpace == 0) {
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

    ESP_LOGI(TAG, "Testing microphone...");

    // 直接使用i2s_read测试麦克风，不依赖环形缓冲区
    uint8_t testBuffer[512];
    size_t bytesRead = 0;
    uint32_t startTime = millis();

    // 尝试直接从I2S读取数据
    while (millis() - startTime < 1000) { // 1秒超时
        esp_err_t err = i2s_read(I2S_NUM_0, testBuffer, sizeof(testBuffer), &bytesRead, 100 / portTICK_PERIOD_MS);
        if (err == ESP_OK && bytesRead > 0) {
            ESP_LOGI(TAG, "Microphone test passed: read %zu bytes", bytesRead);
            return true;
        }
        // 如果没有数据，短暂延迟后重试
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    ESP_LOGE(TAG, "Microphone test failed: no data received");
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
    uint8_t readBuffer[1024];
    uint32_t readCount = 0;
    uint32_t totalBytesRead = 0;
    uint32_t lastStackReport = 0;

    while (driver && driver->isRecording) {
        // 定期检查栈使用情况（每100次循环）
        if (readCount % 100 == 0) {
            UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(currentTask);
            if (stackHighWaterMark != lastStackReport) {
                Serial.printf("[AudioDriver] Stack high water mark: %u bytes (task handle: %p)\n",
                             stackHighWaterMark * sizeof(StackType_t), currentTask);
                lastStackReport = stackHighWaterMark;
            }
        }

        // 从I2S读取数据
        esp_err_t err = i2s_read(I2S_NUM_0, readBuffer, sizeof(readBuffer), &bytesRead, portMAX_DELAY);

        if (err == ESP_OK && bytesRead > 0) {
            totalBytesRead += bytesRead;
            readCount++;

            // 每10次读取记录一次（避免日志过多）
            if (readCount % 10 == 0) {
                ESP_LOGI(TAG, "Record task: read %zu bytes (count=%u, total=%u)",
                        bytesRead, readCount, totalBytesRead);
            }

            // 写入环形缓冲区
            size_t written = 0;
            if (driver) {
                written = driver->writeAudioData(readBuffer, bytesRead);
            }

            // 调用回调函数
            if (driver && driver->recordCallback && written > 0) {
                driver->recordCallback(readBuffer, written, driver->recordCallbackUserData);
            } else if (driver && driver->recordCallback && written == 0) {
                ESP_LOGW(TAG, "Record task: buffer full, written=0");
            }
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2S read error: %s", esp_err_to_name(err));
        } else if (bytesRead == 0) {
            // 无数据读取（不应该发生，因为portMAX_DELAY会阻塞直到有数据）
            ESP_LOGW(TAG, "Record task: i2s_read returned 0 bytes");
        }

        vTaskDelay(1 / portTICK_PERIOD_MS);
    }

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