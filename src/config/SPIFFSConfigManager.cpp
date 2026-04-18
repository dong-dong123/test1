#include "SPIFFSConfigManager.h"
#include <ArduinoJson.h>

SPIFFSConfigManager::SPIFFSConfigManager(const String& filePath)
    : configFilePath(filePath.isEmpty() ? "/config.json" : filePath) {
    config.resetToDefaults();
}

bool SPIFFSConfigManager::initializeSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS初始化失败");
        return false;
    }
    Serial.println("SPIFFS初始化成功");
    return true;
}

bool SPIFFSConfigManager::load() {
    return loadFromSPIFFS();
}

bool SPIFFSConfigManager::save() {
    return saveToSPIFFS();
}

bool SPIFFSConfigManager::resetToDefaults() {
    config.resetToDefaults();
    return save();
}

bool SPIFFSConfigManager::loadFromSPIFFS() {
    if (!SPIFFS.exists(configFilePath)) {
        Serial.println("配置文件不存在，创建默认配置");
        config.resetToDefaults();
        return saveToSPIFFS();
    }

    File file = SPIFFS.open(configFilePath, "r");
    if (!file) {
        Serial.println("无法打开配置文件");
        return false;
    }

    String jsonStr = file.readString();
    file.close();

    // 调试日志
    Serial.print("[SPIFFSConfigManager] Loading config from: ");
    Serial.println(configFilePath);
    Serial.print("[SPIFFSConfigManager] File size: ");
    Serial.println(jsonStr.length());
    Serial.print("[SPIFFSConfigManager] Config content (first 500 chars): ");
    Serial.println(jsonStr.substring(0, 500));

    return parseJSONConfig(jsonStr);
}

bool SPIFFSConfigManager::saveToSPIFFS() const {
    File file = SPIFFS.open(configFilePath, "w");
    if (!file) {
        Serial.println("无法创建配置文件");
        return false;
    }

    String jsonStr = generateJSONConfig();
    file.print(jsonStr);
    file.close();

    Serial.println("配置保存成功");
    return true;
}

bool SPIFFSConfigManager::parseJSONConfig(const String& jsonStr) {
    // 使用ArduinoJson解析jsonStr到config结构
    DynamicJsonDocument doc(4096); // 分配4KB内存，根据实际JSON大小调整
    DeserializationError error = deserializeJson(doc, jsonStr);

    if (error) {
        Serial.print("JSON解析失败: ");
        Serial.println(error.c_str());
        return false;
    }

    // 解析版本号
    config.version = doc["version"] | 1;

    // 解析wifi配置
    if (doc.containsKey("wifi")) {
        JsonObject wifi = doc["wifi"];
        config.wifi.ssid = wifi["ssid"] | "";
        config.wifi.password = wifi["password"] | "";
        config.wifi.autoConnect = wifi["autoConnect"] | true;
        config.wifi.timeout = wifi["timeout"] | 10000;

        // 调试日志
        Serial.print("[SPIFFSConfigManager] WiFi config parsed: SSID length=");
        Serial.print(config.wifi.ssid.length());
        Serial.print(", AutoConnect=");
        Serial.println(config.wifi.autoConnect ? "true" : "false");
        // 可选：打印SSID的十六进制转储以便调试
        Serial.print("[SPIFFSConfigManager] SSID hex: ");
        for (size_t i = 0; i < config.wifi.ssid.length(); i++) {
            if (i > 0) Serial.print(" ");
            Serial.print(config.wifi.ssid[i] & 0xFF, HEX);
        }
        Serial.println();
    } else {
        Serial.println("[SPIFFSConfigManager] No 'wifi' key found in config");
    }

    // 解析services配置
    if (doc.containsKey("services")) {
        JsonObject services = doc["services"];

        // speech服务
        if (services.containsKey("speech")) {
            JsonObject speech = services["speech"];
            config.services.defaultSpeechService = speech["default"] | "volcano";

            // 解析available数组
            if (speech.containsKey("available")) {
                config.services.availableSpeechServices.clear();
                JsonArray available = speech["available"];
                for (JsonVariant v : available) {
                    config.services.availableSpeechServices.push_back(v.as<String>());
                }
            }

            // 解析volcano配置
            if (speech.containsKey("volcano")) {
                JsonObject volcano = speech["volcano"];
                config.services.volcanoSpeech.apiKey = volcano["apiKey"] | "";
                config.services.volcanoSpeech.secretKey = volcano["secretKey"] | "";
                config.services.volcanoSpeech.endpoint = volcano["endpoint"] | "https://openspeech.bytedance.com";
                config.services.volcanoSpeech.language = volcano["language"] | "zh-CN";
                config.services.volcanoSpeech.region = volcano["region"] | "cn-north-1";
                config.services.volcanoSpeech.voice = volcano["voice"] | "zh-CN_female_standard";
                config.services.volcanoSpeech.enablePunctuation = volcano["enablePunctuation"] | true;
                config.services.volcanoSpeech.timeout = volcano["timeout"] | 10.0f;
                config.services.volcanoSpeech.resourceId = volcano["resourceId"] | "volc.bigasr.sauc.duration";
            }
        }

        // dialogue服务
        if (services.containsKey("dialogue")) {
            JsonObject dialogue = services["dialogue"];
            config.services.defaultDialogueService = dialogue["default"] | "coze";

            // 解析available数组
            if (dialogue.containsKey("available")) {
                config.services.availableDialogueServices.clear();
                JsonArray available = dialogue["available"];
                for (JsonVariant v : available) {
                    config.services.availableDialogueServices.push_back(v.as<String>());
                }
            }

            // 解析coze配置
            if (dialogue.containsKey("coze")) {
                JsonObject coze = dialogue["coze"];
                config.services.cozeDialogue.botId = coze["botId"] | "";
                config.services.cozeDialogue.apiKey = coze["apiKey"] | "";
                config.services.cozeDialogue.endpoint = coze["endpoint"] | "https://api.coze.cn";
                config.services.cozeDialogue.streamEndpoint = coze["streamEndpoint"] | "https://kfdcyyzqgx.coze.site/stream_run";
                config.services.cozeDialogue.personality = coze["personality"] | "friendly_assistant";
                config.services.cozeDialogue.model = coze["model"] | "coze-model";
                config.services.cozeDialogue.projectId = coze["projectId"] | "7625602998236004386";
                config.services.cozeDialogue.sessionId = coze["sessionId"] | "l_tnvlo49EWy6p9YUl8oC";
                config.services.cozeDialogue.temperature = coze["temperature"] | 0.7f;
                config.services.cozeDialogue.maxTokens = coze["maxTokens"] | 1000;
                config.services.cozeDialogue.timeout = coze["timeout"] | 15.0f;
            }
        }
    }

    // 解析audio配置
    if (doc.containsKey("audio")) {
        JsonObject audio = doc["audio"];
        config.audio.sampleRate = audio["sampleRate"] | 16000;
        config.audio.bitsPerSample = audio["bitsPerSample"] | 16;
        config.audio.channels = audio["channels"] | 1;

        // 向后兼容性：处理旧vadThreshold配置迁移
        if (audio.containsKey("vadThreshold") && !audio.containsKey("vadSpeechThreshold")) {
            float oldThreshold = audio["vadThreshold"] | 0.3f;
            config.audio.vadSpeechThreshold = oldThreshold + 0.2f; // 增加0.2作为语音阈值
            config.audio.vadSilenceThreshold = oldThreshold - 0.1f; // 减少0.1作为静音阈值

            // 确保阈值有效
            if (config.audio.vadSpeechThreshold > 1.0f) config.audio.vadSpeechThreshold = 1.0f;
            if (config.audio.vadSilenceThreshold < 0.0f) config.audio.vadSilenceThreshold = 0.0f;

            Serial.printf("[CONFIG] Migrated old vadThreshold (%.2f) to dual thresholds: speech=%.2f, silence=%.2f\n",
                         oldThreshold, config.audio.vadSpeechThreshold, config.audio.vadSilenceThreshold);
        } else {
            // 加载新的双阈值配置
            config.audio.vadSpeechThreshold = audio["vadSpeechThreshold"] | 0.50f;
            config.audio.vadSilenceThreshold = audio["vadSilenceThreshold"] | 0.30f;
        }

        config.audio.vadSilenceDuration = audio["vadSilenceDuration"] | 800;
        config.audio.wakeWord = audio["wakeWord"] | "小智小智";
        config.audio.wakeWordSensitivity = audio["wakeWordSensitivity"] | 0.8f;
        config.audio.volume = audio["volume"] | 80;
    }

    // 解析display配置
    if (doc.containsKey("display")) {
        JsonObject display = doc["display"];
        config.display.brightness = display["brightness"] | 100;
        config.display.timeout = display["timeout"] | 30000;
        config.display.showWaveform = display["showWaveform"] | true;
        config.display.showHistory = display["showHistory"] | true;
        config.display.maxHistoryLines = display["maxHistoryLines"] | 10;
    }

    // 解析logging配置
    if (doc.containsKey("logging")) {
        JsonObject logging = doc["logging"];
        config.logging.level = logging["level"] | "INFO";

        // 解析output数组
        if (logging.containsKey("output")) {
            config.logging.output.clear();
            JsonArray output = logging["output"];
            for (JsonVariant v : output) {
                config.logging.output.push_back(v.as<String>());
            }
        }
    }

    return true;
}

String SPIFFSConfigManager::generateJSONConfig() const {
    // 将config结构转换为JSON字符串
    DynamicJsonDocument doc(4096); // 分配4KB内存，根据实际JSON大小调整

    // 添加版本号
    doc["version"] = config.version;

    // 构建wifi配置
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["ssid"] = config.wifi.ssid;
    wifi["password"] = config.wifi.password;
    wifi["autoConnect"] = config.wifi.autoConnect;
    wifi["timeout"] = config.wifi.timeout;

    // 构建services配置
    JsonObject services = doc.createNestedObject("services");

    // speech服务
    JsonObject speech = services.createNestedObject("speech");
    speech["default"] = config.services.defaultSpeechService;

    JsonArray availableSpeech = speech.createNestedArray("available");
    for (const String& service : config.services.availableSpeechServices) {
        availableSpeech.add(service);
    }

    JsonObject volcano = speech.createNestedObject("volcano");
    volcano["apiKey"] = config.services.volcanoSpeech.apiKey;
    volcano["secretKey"] = config.services.volcanoSpeech.secretKey;
    volcano["endpoint"] = config.services.volcanoSpeech.endpoint;
    volcano["language"] = config.services.volcanoSpeech.language;
    volcano["region"] = config.services.volcanoSpeech.region;
    volcano["voice"] = config.services.volcanoSpeech.voice;
    volcano["enablePunctuation"] = config.services.volcanoSpeech.enablePunctuation;
    volcano["timeout"] = config.services.volcanoSpeech.timeout;
    volcano["resourceId"] = config.services.volcanoSpeech.resourceId;

    // dialogue服务
    JsonObject dialogue = services.createNestedObject("dialogue");
    dialogue["default"] = config.services.defaultDialogueService;

    JsonArray availableDialogue = dialogue.createNestedArray("available");
    for (const String& service : config.services.availableDialogueServices) {
        availableDialogue.add(service);
    }

    JsonObject coze = dialogue.createNestedObject("coze");
    coze["botId"] = config.services.cozeDialogue.botId;
    coze["apiKey"] = config.services.cozeDialogue.apiKey;
    coze["endpoint"] = config.services.cozeDialogue.endpoint;
    coze["streamEndpoint"] = config.services.cozeDialogue.streamEndpoint;
    coze["personality"] = config.services.cozeDialogue.personality;
    coze["model"] = config.services.cozeDialogue.model;
    coze["projectId"] = config.services.cozeDialogue.projectId;
    coze["sessionId"] = config.services.cozeDialogue.sessionId;
    coze["temperature"] = config.services.cozeDialogue.temperature;
    coze["maxTokens"] = config.services.cozeDialogue.maxTokens;
    coze["timeout"] = config.services.cozeDialogue.timeout;

    // 构建audio配置
    JsonObject audio = doc.createNestedObject("audio");
    audio["sampleRate"] = config.audio.sampleRate;
    audio["bitsPerSample"] = config.audio.bitsPerSample;
    audio["channels"] = config.audio.channels;
    audio["vadSpeechThreshold"] = config.audio.vadSpeechThreshold;
    audio["vadSilenceThreshold"] = config.audio.vadSilenceThreshold;
    audio["vadSilenceDuration"] = config.audio.vadSilenceDuration;
    audio["wakeWord"] = config.audio.wakeWord;
    audio["wakeWordSensitivity"] = config.audio.wakeWordSensitivity;
    audio["volume"] = config.audio.volume;

    // 构建display配置
    JsonObject display = doc.createNestedObject("display");
    display["brightness"] = config.display.brightness;
    display["timeout"] = config.display.timeout;
    display["showWaveform"] = config.display.showWaveform;
    display["showHistory"] = config.display.showHistory;
    display["maxHistoryLines"] = config.display.maxHistoryLines;

    // 构建logging配置
    JsonObject logging = doc.createNestedObject("logging");
    logging["level"] = config.logging.level;

    JsonArray output = logging.createNestedArray("output");
    for (const String& out : config.logging.output) {
        output.add(out);
    }

    // 序列化为字符串
    String jsonStr;
    serializeJson(doc, jsonStr);
    return jsonStr;
}

String SPIFFSConfigManager::getDefaultConfigJSON() {
    // 基于设计文档第4.2节的默认配置
    return R"({
  "version": 1,
  "wifi": {
    "ssid": "YourWiFiSSID",
    "password": "YourWiFiPassword",
    "autoConnect": true,
    "timeout": 10000
  },

  "services": {
    "speech": {
      "default": "volcano",
      "available": ["volcano", "baidu", "tencent"],
      "volcano": {
        "apiKey": "2015527679",
        "secretKey": "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB",
        "endpoint": "https://openspeech.bytedance.com",
        "language": "zh-CN",
        "region": "cn-north-1",
        "voice": "zh-CN_female_standard",
        "enablePunctuation": true,
        "timeout": 10.0
      }
    },

    "dialogue": {
      "default": "coze",
      "available": ["coze", "openai"],
      "coze": {
        "botId": "your_coze_bot_id",
        "apiKey": "pat_bb81GCikaLf65LwNoWegodxDPKKnIF4byFusUdSRojzF9Q2MvYjYEZWqn0E2xNVd",
        "endpoint": "https://api.coze.cn",
        "streamEndpoint": "https://kfdcyyzqgx.coze.site/stream_run",
        "personality": "friendly_assistant",
        "model": "coze-model",
        "projectId": "7625602998236004386",
        "sessionId": "l_tnvlo49EWy6p9YUl8oC",
        "temperature": 0.7,
        "maxTokens": 1000,
        "timeout": 15.0
      }
    }
  },

  "audio": {
    "sampleRate": 16000,
    "bitsPerSample": 16,
    "channels": 1,
    "vadSpeechThreshold": 0.50,
    "vadSilenceThreshold": 0.30,
    "vadSilenceDuration": 800,
    "wakeWord": "小智小智",
    "wakeWordSensitivity": 0.8,
    "volume": 80
  },

  "display": {
    "brightness": 100,
    "timeout": 30000,
    "showWaveform": true,
    "showHistory": true,
    "maxHistoryLines": 10
  },

  "logging": {
    "level": "INFO",
    "output": ["serial"]
  }
})";
}

// 配置验证
bool SPIFFSConfigManager::validate() const {
    return getValidationErrors().empty();
}

std::vector<String> SPIFFSConfigManager::getValidationErrors() const {
    std::vector<String> errors;

    // 检查wifi配置
    if (config.wifi.ssid.length() == 0 && config.wifi.autoConnect) {
        errors.push_back("WiFi: 启用自动连接但未设置SSID");
    }

    // 检查audio配置
    if (config.audio.sampleRate < 8000 || config.audio.sampleRate > 48000) {
        errors.push_back("Audio: 采样率必须在8000-48000之间");
    }
    if (config.audio.bitsPerSample != 8 && config.audio.bitsPerSample != 16 && config.audio.bitsPerSample != 24) {
        errors.push_back("Audio: 采样位数必须是8、16或24");
    }
    if (config.audio.channels != 1 && config.audio.channels != 2) {
        errors.push_back("Audio: 声道数必须是1或2");
    }
    if (config.audio.vadSpeechThreshold < 0.0f || config.audio.vadSpeechThreshold > 1.0f) {
        errors.push_back("Audio: VAD语音检测阈值必须在0.0-1.0之间");
    }
    if (config.audio.vadSilenceThreshold < 0.0f || config.audio.vadSilenceThreshold > 1.0f) {
        errors.push_back("Audio: VAD静音确认阈值必须在0.0-1.0之间");
    }
    if (config.audio.vadSilenceThreshold >= config.audio.vadSpeechThreshold) {
        errors.push_back("Audio: VAD静音确认阈值必须小于语音检测阈值");
    }
    if (config.audio.wakeWordSensitivity < 0.0f || config.audio.wakeWordSensitivity > 1.0f) {
        errors.push_back("Audio: 唤醒词敏感度必须在0.0-1.0之间");
    }
    if (config.audio.volume > 100) {
        errors.push_back("Audio: 音量不能超过100");
    }

    // 检查display配置
    if (config.display.brightness > 100) {
        errors.push_back("Display: 亮度不能超过100");
    }

    // 检查services配置
    if (config.services.defaultSpeechService.length() == 0) {
        errors.push_back("Services: 未设置默认语音服务");
    }
    if (config.services.defaultDialogueService.length() == 0) {
        errors.push_back("Services: 未设置默认对话服务");
    }

    // 检查logging配置
    const String validLevels[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    bool validLevel = false;
    for (const auto& level : validLevels) {
        if (config.logging.level.equalsIgnoreCase(level)) {
            validLevel = true;
            break;
        }
    }
    if (!validLevel) {
        errors.push_back("Logging: 无效的日志级别，必须是DEBUG、INFO、WARN、ERROR或FATAL");
    }

    return errors;
}

// Key路径解析辅助方法实现
SPIFFSConfigManager::KeyPath SPIFFSConfigManager::parseKey(const String& key) {
    KeyPath path;
    int firstDot = key.indexOf('.');
    int secondDot = -1;

    if (firstDot != -1) {
        path.section = key.substring(0, firstDot);
        secondDot = key.indexOf('.', firstDot + 1);
        if (secondDot != -1) {
            path.subsection = key.substring(firstDot + 1, secondDot);
            path.field = key.substring(secondDot + 1);
            path.hasSubsection = true;
        } else {
            path.field = key.substring(firstDot + 1);
            path.hasSubsection = false;
        }
    } else {
        path.section = key;
        path.hasSubsection = false;
    }

    return path;
}

String SPIFFSConfigManager::getValueByPath(const KeyPath& path) const {
    // 版本号
    if (path.section == "version" || (path.section.isEmpty() && path.field == "version")) {
        return String(config.version);
    }

    // wifi配置
    if (path.section == "wifi") {
        if (path.field == "ssid") return config.wifi.ssid;
        if (path.field == "password") return config.wifi.password;
        if (path.field == "autoConnect") return config.wifi.autoConnect ? "true" : "false";
        if (path.field == "timeout") return String(config.wifi.timeout);
    }
    // services配置
    else if (path.section == "services") {
        if (path.hasSubsection) {
            if (path.subsection == "speech") {
                if (path.field == "default") return config.services.defaultSpeechService;
                // speech服务配置
                if (path.field == "volcano.apiKey") return config.services.volcanoSpeech.apiKey;
                if (path.field == "volcano.secretKey") return config.services.volcanoSpeech.secretKey;
                if (path.field == "volcano.endpoint") return config.services.volcanoSpeech.endpoint;
                if (path.field == "volcano.language") return config.services.volcanoSpeech.language;
                if (path.field == "volcano.region") return config.services.volcanoSpeech.region;
                if (path.field == "volcano.voice") return config.services.volcanoSpeech.voice;
                if (path.field == "volcano.enablePunctuation") return config.services.volcanoSpeech.enablePunctuation ? "true" : "false";
                if (path.field == "volcano.timeout") return String(config.services.volcanoSpeech.timeout);
                if (path.field == "volcano.resourceId") return config.services.volcanoSpeech.resourceId;
            }
            else if (path.subsection == "dialogue") {
                if (path.field == "default") return config.services.defaultDialogueService;
                // dialogue服务配置
                if (path.field == "coze.botId") return config.services.cozeDialogue.botId;
                if (path.field == "coze.apiKey") return config.services.cozeDialogue.apiKey;
                if (path.field == "coze.endpoint") return config.services.cozeDialogue.endpoint;
                if (path.field == "coze.personality") return config.services.cozeDialogue.personality;
                if (path.field == "coze.streamEndpoint") return config.services.cozeDialogue.streamEndpoint;
                if (path.field == "coze.model") return config.services.cozeDialogue.model;
                if (path.field == "coze.projectId") return config.services.cozeDialogue.projectId;
                if (path.field == "coze.sessionId") return config.services.cozeDialogue.sessionId;
                if (path.field == "coze.temperature") return String(config.services.cozeDialogue.temperature);
                if (path.field == "coze.maxTokens") return String(config.services.cozeDialogue.maxTokens);
                if (path.field == "coze.timeout") return String(config.services.cozeDialogue.timeout);
            }
        }
        else {
            // 顶级services字段
            if (path.field == "speech.default") return config.services.defaultSpeechService;
            if (path.field == "dialogue.default") return config.services.defaultDialogueService;
        }
    }
    // audio配置
    else if (path.section == "audio") {
        if (path.field == "sampleRate") return String(config.audio.sampleRate);
        if (path.field == "bitsPerSample") return String(config.audio.bitsPerSample);
        if (path.field == "channels") return String(config.audio.channels);
        if (path.field == "vadSpeechThreshold") return String(config.audio.vadSpeechThreshold, 2);
        if (path.field == "vadSilenceThreshold") return String(config.audio.vadSilenceThreshold, 2);
        if (path.field == "vadSilenceDuration") return String(config.audio.vadSilenceDuration);
        // 向后兼容：旧vadThreshold字段返回语音检测阈值
        if (path.field == "vadThreshold") return String(config.audio.vadSpeechThreshold, 2);
        if (path.field == "wakeWord") return config.audio.wakeWord;
        if (path.field == "wakeWordSensitivity") return String(config.audio.wakeWordSensitivity, 2);
        if (path.field == "volume") return String(config.audio.volume);
    }
    // display配置
    else if (path.section == "display") {
        if (path.field == "brightness") return String(config.display.brightness);
        if (path.field == "timeout") return String(config.display.timeout);
        if (path.field == "showWaveform") return config.display.showWaveform ? "true" : "false";
        if (path.field == "showHistory") return config.display.showHistory ? "true" : "false";
        if (path.field == "maxHistoryLines") return String(config.display.maxHistoryLines);
    }
    // logging配置
    else if (path.section == "logging") {
        if (path.field == "level") return config.logging.level;
        // 数组字段需要特殊处理
    }
    // 直接字段访问（不带section）
    else if (path.section.isEmpty() && !path.hasSubsection) {
        // 处理如"wifi.ssid"格式的key
        if (path.field == "wifi.ssid") return config.wifi.ssid;
        if (path.field == "wifi.password") return config.wifi.password;
        if (path.field == "wifi.autoConnect") return config.wifi.autoConnect ? "true" : "false";
        if (path.field == "wifi.timeout") return String(config.wifi.timeout);
        if (path.field == "services.speech.default") return config.services.defaultSpeechService;
        if (path.field == "services.dialogue.default") return config.services.defaultDialogueService;
    }

    return "";
}

bool SPIFFSConfigManager::setValueByPath(const KeyPath& path, const String& value) {
    // 版本号
    if (path.section == "version" || (path.section.isEmpty() && path.field == "version")) {
        config.version = value.toInt();
        return true;
    }

    // wifi配置
    if (path.section == "wifi") {
        if (path.field == "ssid") { config.wifi.ssid = value; return true; }
        if (path.field == "password") { config.wifi.password = value; return true; }
        if (path.field == "autoConnect") { config.wifi.autoConnect = (value == "true" || value == "1"); return true; }
        if (path.field == "timeout") { config.wifi.timeout = value.toInt(); return true; }
    }
    // services配置
    else if (path.section == "services") {
        if (path.hasSubsection) {
            if (path.subsection == "speech") {
                if (path.field == "default") { config.services.defaultSpeechService = value; return true; }
                // speech服务配置 - 需要支持嵌套字段
                if (path.field == "volcano.apiKey") { config.services.volcanoSpeech.apiKey = value; return true; }
                if (path.field == "volcano.secretKey") { config.services.volcanoSpeech.secretKey = value; return true; }
                if (path.field == "volcano.endpoint") { config.services.volcanoSpeech.endpoint = value; return true; }
                if (path.field == "volcano.language") { config.services.volcanoSpeech.language = value; return true; }
                if (path.field == "volcano.region") { config.services.volcanoSpeech.region = value; return true; }
                if (path.field == "volcano.voice") { config.services.volcanoSpeech.voice = value; return true; }
                if (path.field == "volcano.enablePunctuation") { config.services.volcanoSpeech.enablePunctuation = (value == "true" || value == "1"); return true; }
                if (path.field == "volcano.timeout") { config.services.volcanoSpeech.timeout = value.toFloat(); return true; }
                if (path.field == "volcano.resourceId") { config.services.volcanoSpeech.resourceId = value; return true; }
            }
            else if (path.subsection == "dialogue") {
                if (path.field == "default") { config.services.defaultDialogueService = value; return true; }
                // dialogue服务配置
                if (path.field == "coze.botId") { config.services.cozeDialogue.botId = value; return true; }
                if (path.field == "coze.apiKey") { config.services.cozeDialogue.apiKey = value; return true; }
                if (path.field == "coze.endpoint") { config.services.cozeDialogue.endpoint = value; return true; }
                if (path.field == "coze.personality") { config.services.cozeDialogue.personality = value; return true; }
                if (path.field == "coze.streamEndpoint") { config.services.cozeDialogue.streamEndpoint = value; return true; }
                if (path.field == "coze.model") { config.services.cozeDialogue.model = value; return true; }
                if (path.field == "coze.projectId") { config.services.cozeDialogue.projectId = value; return true; }
                if (path.field == "coze.sessionId") { config.services.cozeDialogue.sessionId = value; return true; }
                if (path.field == "coze.temperature") { config.services.cozeDialogue.temperature = value.toFloat(); return true; }
                if (path.field == "coze.maxTokens") { config.services.cozeDialogue.maxTokens = value.toInt(); return true; }
                if (path.field == "coze.timeout") { config.services.cozeDialogue.timeout = value.toFloat(); return true; }
            }
        }
        else {
            // 顶级services字段（如"services.speech.default"格式）
            if (path.field == "speech.default") { config.services.defaultSpeechService = value; return true; }
            if (path.field == "dialogue.default") { config.services.defaultDialogueService = value; return true; }
        }
    }
    // audio配置
    else if (path.section == "audio") {
        if (path.field == "sampleRate") { config.audio.sampleRate = value.toInt(); return true; }
        if (path.field == "bitsPerSample") { config.audio.bitsPerSample = value.toInt(); return true; }
        if (path.field == "channels") { config.audio.channels = value.toInt(); return true; }
        if (path.field == "vadSpeechThreshold") { config.audio.vadSpeechThreshold = value.toFloat(); return true; }
        if (path.field == "vadSilenceThreshold") { config.audio.vadSilenceThreshold = value.toFloat(); return true; }
        if (path.field == "vadSilenceDuration") { config.audio.vadSilenceDuration = value.toInt(); return true; }
        // 向后兼容：设置旧vadThreshold时同时更新两个新阈值
        if (path.field == "vadThreshold") {
            float oldThreshold = value.toFloat();
            config.audio.vadSpeechThreshold = oldThreshold + 0.2f;
            config.audio.vadSilenceThreshold = oldThreshold - 0.1f;
            // 确保阈值有效
            if (config.audio.vadSpeechThreshold > 1.0f) config.audio.vadSpeechThreshold = 1.0f;
            if (config.audio.vadSilenceThreshold < 0.0f) config.audio.vadSilenceThreshold = 0.0f;
            return true;
        }
        if (path.field == "wakeWord") { config.audio.wakeWord = value; return true; }
        if (path.field == "wakeWordSensitivity") { config.audio.wakeWordSensitivity = value.toFloat(); return true; }
        if (path.field == "volume") { config.audio.volume = value.toInt(); return true; }
    }
    // display配置
    else if (path.section == "display") {
        if (path.field == "brightness") { config.display.brightness = value.toInt(); return true; }
        if (path.field == "timeout") { config.display.timeout = value.toInt(); return true; }
        if (path.field == "showWaveform") { config.display.showWaveform = (value == "true" || value == "1"); return true; }
        if (path.field == "showHistory") { config.display.showHistory = (value == "true" || value == "1"); return true; }
        if (path.field == "maxHistoryLines") { config.display.maxHistoryLines = value.toInt(); return true; }
    }
    // logging配置
    else if (path.section == "logging") {
        if (path.field == "level") { config.logging.level = value; return true; }
    }

    return false;
}

// ConfigManager接口方法实现
String SPIFFSConfigManager::getString(const String& key, const String& defaultValue) {
    KeyPath path = parseKey(key);
    String result = getValueByPath(path);
    return result.isEmpty() ? defaultValue : result;
}

bool SPIFFSConfigManager::setString(const String& key, const String& value) {
    KeyPath path = parseKey(key);
    bool success = setValueByPath(path, value);
    if (success) {
        // 自动保存配置更改
        return save();
    }
    return false;
}

int SPIFFSConfigManager::getInt(const String& key, int defaultValue) {
    String strValue = getString(key);
    if (strValue.isEmpty()) {
        return defaultValue;
    }
    return strValue.toInt();
}

bool SPIFFSConfigManager::setInt(const String& key, int value) {
    return setString(key, String(value));
}

float SPIFFSConfigManager::getFloat(const String& key, float defaultValue) {
    String strValue = getString(key);
    if (strValue.isEmpty()) {
        return defaultValue;
    }
    return strValue.toFloat();
}

bool SPIFFSConfigManager::setFloat(const String& key, float value) {
    // 保留2位小数
    char buffer[32];
    dtostrf(value, 0, 2, buffer);
    return setString(key, String(buffer));
}

bool SPIFFSConfigManager::getBool(const String& key, bool defaultValue) {
    String strValue = getString(key);
    if (strValue.isEmpty()) {
        return defaultValue;
    }
    strValue.toLowerCase();
    return (strValue == "true" || strValue == "1" || strValue == "yes" || strValue == "on");
}

bool SPIFFSConfigManager::setBool(const String& key, bool value) {
    return setString(key, value ? "true" : "false");
}

std::vector<String> SPIFFSConfigManager::getStringArray(const String& key) {
    KeyPath path = parseKey(key);

    if (path.section == "services" && path.hasSubsection) {
        if (path.subsection == "speech" && path.field == "available") {
            return config.services.availableSpeechServices;
        }
        else if (path.subsection == "dialogue" && path.field == "available") {
            return config.services.availableDialogueServices;
        }
    }
    else if (path.section == "logging" && path.field == "output") {
        return config.logging.output;
    }

    return std::vector<String>();
}

bool SPIFFSConfigManager::setStringArray(const String& key, const std::vector<String>& values) {
    KeyPath path = parseKey(key);

    if (path.section == "services" && path.hasSubsection) {
        if (path.subsection == "speech" && path.field == "available") {
            config.services.availableSpeechServices = values;
            return save();
        }
        else if (path.subsection == "dialogue" && path.field == "available") {
            config.services.availableDialogueServices = values;
            return save();
        }
    }
    else if (path.section == "logging" && path.field == "output") {
        config.logging.output = values;
        return save();
    }

    return false;
}

bool SPIFFSConfigManager::updateSystemConfig(const SystemConfig& newConfig) {
    config = newConfig;
    return save();
}