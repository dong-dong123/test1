#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <vector>

class ConfigManager {
public:
    virtual ~ConfigManager() = default;

    // 配置加载/保存
    virtual bool load() = 0;
    virtual bool save() = 0;
    virtual bool resetToDefaults() = 0;

    // 字符串配置
    virtual String getString(const String& key, const String& defaultValue = "") = 0;
    virtual bool setString(const String& key, const String& value) = 0;

    // 整数配置
    virtual int getInt(const String& key, int defaultValue = 0) = 0;
    virtual bool setInt(const String& key, int value) = 0;

    // 浮点数配置
    virtual float getFloat(const String& key, float defaultValue = 0.0f) = 0;
    virtual bool setFloat(const String& key, float value) = 0;

    // 布尔配置
    virtual bool getBool(const String& key, bool defaultValue = false) = 0;
    virtual bool setBool(const String& key, bool value) = 0;

    // 数组配置
    virtual std::vector<String> getStringArray(const String& key) = 0;
    virtual bool setStringArray(const String& key, const std::vector<String>& values) = 0;

    // 配置验证
    virtual bool validate() const = 0;
    virtual std::vector<String> getValidationErrors() const = 0;
};

#endif