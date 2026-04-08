#ifndef SPIFFS_CONFIG_MANAGER_H
#define SPIFFS_CONFIG_MANAGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include "ConfigData.h"
#include "../interfaces/ConfigManager.h"

class SPIFFSConfigManager : public ConfigManager {
private:
    SystemConfig config;
    String configFilePath;

    // 内部辅助方法
    bool loadFromSPIFFS();
    bool saveToSPIFFS() const;
    bool parseJSONConfig(const String& jsonStr);
    String generateJSONConfig() const;

    // Key路径解析辅助方法
    struct KeyPath {
        String section;
        String subsection;
        String field;
        bool hasSubsection;
    };
    static KeyPath parseKey(const String& key);
    String getValueByPath(const KeyPath& path) const;
    bool setValueByPath(const KeyPath& path, const String& value);

public:
    SPIFFSConfigManager(const String& filePath = "/config.json");
    virtual ~SPIFFSConfigManager() = default;

    // ConfigManager接口实现
    virtual bool load() override;
    virtual bool save() override;
    virtual bool resetToDefaults() override;

    // 字符串配置
    virtual String getString(const String& key, const String& defaultValue = "") override;
    virtual bool setString(const String& key, const String& value) override;

    // 整数配置
    virtual int getInt(const String& key, int defaultValue = 0) override;
    virtual bool setInt(const String& key, int value) override;

    // 浮点数配置
    virtual float getFloat(const String& key, float defaultValue = 0.0f) override;
    virtual bool setFloat(const String& key, float value) override;

    // 布尔配置
    virtual bool getBool(const String& key, bool defaultValue = false) override;
    virtual bool setBool(const String& key, bool value) override;

    // 数组配置
    virtual std::vector<String> getStringArray(const String& key) override;
    virtual bool setStringArray(const String& key, const std::vector<String>& values) override;

    // 配置验证
    virtual bool validate() const override;
    virtual std::vector<String> getValidationErrors() const override;

    // 扩展方法
    const SystemConfig& getSystemConfig() const { return config; }
    bool updateSystemConfig(const SystemConfig& newConfig);

    // 工具方法
    static bool initializeSPIFFS();
    static String getDefaultConfigJSON();
};

#endif // SPIFFS_CONFIG_MANAGER_H