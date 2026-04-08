#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "PinDefinitions.h"
#include "config/ConfigData.h"  // 使用统一的DisplayConfig定义

// 文本对齐方式
enum class TextAlign {
    LEFT,
    CENTER,
    RIGHT
};

// 显示驱动类 - 封装TFT_eSPI功能
class DisplayDriver {
private:
    TFT_eSPI tft;
    DisplayConfig config;
    bool isInitialized;

    // 当前绘图状态
    uint16_t currentTextColor;
    uint8_t currentTextSize;
    uint16_t currentBgColor;

    // 内部方法
    bool initSPI();
    void initGPIO();

public:
    DisplayDriver();
    virtual ~DisplayDriver();

    // 初始化/反初始化
    bool initialize(const DisplayConfig& cfg = DisplayConfig());
    bool deinitialize();
    bool isReady() const { return isInitialized; }

    // 屏幕控制
    void clear();
    void fillScreen(uint16_t color);
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const { return config.brightness; }
    void setBacklight(bool enabled);
    bool isBacklightEnabled() const { return config.backlightEnabled; }

    // 文本绘制
    void setTextColor(uint16_t color, uint16_t bgColor = TFT_BLACK);
    void setTextSize(uint8_t size);
    void setTextFont(uint8_t font = 1);
    void setTextWrap(bool wrap);

    void drawText(int16_t x, int16_t y, const String& text, TextAlign align = TextAlign::LEFT);
    void drawTextCentered(int16_t y, const String& text);
    void printf(int16_t x, int16_t y, const char* format, ...);

    // 图形绘制
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

    // 图像绘制
    void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color);
    void drawXBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color);
    void drawJpeg(int16_t x, int16_t y, const uint8_t* jpegData, size_t jpegLen);
    void drawPng(int16_t x, int16_t y, const uint8_t* pngData, size_t pngLen);

    // 进度条和图表
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint16_t color, uint16_t bgColor);
    void drawWaveform(int16_t x, int16_t y, int16_t w, int16_t h, const int16_t* data, size_t dataLength, uint16_t color);

    // 屏幕信息
    uint16_t getWidth() const { return config.width; }
    uint16_t getHeight() const { return config.height; }
    uint8_t getRotation() const { return config.rotation; }
    void setRotation(uint8_t rotation);

    // 配置管理
    const DisplayConfig& getConfig() const { return config; }
    bool updateConfig(const DisplayConfig& newConfig);

    // 工具方法
    uint16_t colorRGB(uint8_t r, uint8_t g, uint8_t b);
    uint16_t colorHSV(uint16_t hue, uint8_t sat, uint8_t val);
    uint16_t getPixel(int16_t x, int16_t y);

    // 测试方法
    bool testDisplay();
    void showTestPattern();
    void showColorBars();
    void showTextDemo();

    // 状态显示
    void showBootScreen(const String& appName, const String& version);
    void showStatus(const String& status, uint16_t color = TFT_WHITE);
    void showError(const String& error);
    void showWarning(const String& warning);
    void showInfo(const String& info);

private:
    // 内部辅助方法
    void applyConfig();
    void updateBacklight();
};

#endif // DISPLAY_DRIVER_H