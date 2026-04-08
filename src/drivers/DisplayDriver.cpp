#include "DisplayDriver.h"
#include <SPI.h>
#include <esp_log.h>
#include "PinDefinitions.h"  // 确保使用正确的引脚定义

static const char* TAG = "DisplayDriver";

// 如果TFT_eSPI库未定义TFT_GRAY，则定义它
#ifndef TFT_GRAY
#define TFT_GRAY 0x7BEF  // RGB565灰色
#endif

// ============================================================================
// 构造函数/析构函数
// ============================================================================

DisplayDriver::DisplayDriver() :
    isInitialized(false),
    currentTextColor(TFT_WHITE),
    currentTextSize(2),
    currentBgColor(TFT_BLACK) {
    // TFT_eSPI对象在头文件中构造
    ESP_LOGD(TAG, "DisplayDriver constructor called");
}

DisplayDriver::~DisplayDriver() {
    deinitialize();
}

// ============================================================================
// 初始化/反初始化
// ============================================================================

bool DisplayDriver::initialize(const DisplayConfig& cfg) {
    if (isInitialized) {
        deinitialize();
    }

    config = cfg;

    // 初始化GPIO引脚
    initGPIO();

    // 初始化SPI
    if (!initSPI()) {
        return false;
    }

    // 执行显示屏硬件复位（如果复位引脚有效）
    if (TFT_RST >= 0) {
        ESP_LOGI(TAG, "Performing display hardware reset");
        digitalWrite(TFT_RST, LOW);
        delay(50);  // 保持复位低电平至少10ms
        digitalWrite(TFT_RST, HIGH);
        delay(150); // 给显示屏足够时间初始化
        ESP_LOGI(TAG, "Display reset complete");
    }

    // 初始化TFT
    ESP_LOGI(TAG, "Calling tft.init()...");
    tft.init();
    ESP_LOGI(TAG, "tft.init() completed successfully");
    applyConfig();

    isInitialized = true;
    ESP_LOGI(TAG, "DisplayDriver initialized successfully");
    ESP_LOGI(TAG, "Resolution: %dx%d, Rotation: %d", config.width, config.height, config.rotation);

    return true;
}

bool DisplayDriver::deinitialize() {
    if (!isInitialized) {
        return true;
    }

    // 关闭背光
    setBacklight(false);

    // TFT_eSPI没有显式的反初始化方法
    // 可以重置引脚等

    isInitialized = false;
    ESP_LOGI(TAG, "DisplayDriver deinitialized");

    return true;
}

void DisplayDriver::initGPIO() {
    ESP_LOGI(TAG, "Initializing GPIO pins");

    // 配置显示屏控制引脚（DC和RST）
    // TFT_eSPI库可能需要在init()之前正确配置这些引脚

    if (TFT_DC >= 0) {
        pinMode(TFT_DC, OUTPUT);
        digitalWrite(TFT_DC, LOW); // 初始为命令模式
        ESP_LOGD(TAG, "TFT_DC pin %d configured as OUTPUT", TFT_DC);
    }

    if (TFT_RST >= 0) {
        pinMode(TFT_RST, OUTPUT);
        digitalWrite(TFT_RST, HIGH); // 初始为高电平（不复位）
        ESP_LOGD(TAG, "TFT_RST pin %d configured as OUTPUT", TFT_RST);
    }

    // CS引脚由SPI库控制，不需要手动配置

    // 如果背光引脚可控制，这里可以初始化
    // 对于ST7789，背光通常直接接3.3V或通过GPIO控制
    // 这里假设背光引脚可控制
    if (config.backlightEnabled) {
        // 初始化背光引脚为输出
        // 具体引脚取决于硬件连接
        #ifdef TFT_BL
        if (TFT_BL >= 0) {
            pinMode(TFT_BL, OUTPUT);
            digitalWrite(TFT_BL, HIGH); // 默认开启背光
            ESP_LOGD(TAG, "TFT_BL pin %d configured as OUTPUT, backlight ON", TFT_BL);
        }
        #endif
    }

    ESP_LOGI(TAG, "GPIO initialization complete");
}

bool DisplayDriver::initSPI() {
    ESP_LOGI(TAG, "SPI pins configured in User_Setup.h: SCLK=%d, MOSI=%d, MISO=%d, CS=%d",
             TFT_SCLK, TFT_MOSI, TFT_MISO, TFT_CS);

    // TFT_eSPI库会在tft.init()中自行初始化SPI总线
    // 我们不需要手动调用SPI.begin()，否则可能造成冲突

    // TFT_eSPI库会在tft.init()中自行设置SPI引脚模式
    // 我们不应该在这里设置引脚模式，以免与库冲突
    // 但需要确保CS引脚初始为高电平（不选中）
    if (TFT_CS >= 0) pinMode(TFT_CS, OUTPUT);

    // 确保CS引脚初始为高电平（不选中）
    if (TFT_CS >= 0) digitalWrite(TFT_CS, HIGH);

    ESP_LOGI(TAG, "SPI pin modes configured");
    return true;
}

void DisplayDriver::applyConfig() {
    // 设置旋转
    tft.setRotation(config.rotation);

    // 设置颜色
    currentTextColor = config.textColor;
    currentBgColor = config.backgroundColor;
    tft.setTextColor(currentTextColor, currentBgColor);

    // 设置文本大小
    currentTextSize = config.textSize;
    tft.setTextSize(currentTextSize);

    // 填充背景
    fillScreen(config.backgroundColor);

    // 更新背光
    updateBacklight();
}

void DisplayDriver::updateBacklight() {
    // 控制背光
    // 注意：ST7789背光通常直接接3.3V，或通过PWM控制
    // 这里简化处理：如果背光引脚可控制，则设置电平
    #ifdef TFT_BL
    if (TFT_BL >= 0) {
        if (config.backlightEnabled) {
            digitalWrite(TFT_BL, HIGH); // 开启背光
            ESP_LOGD(TAG, "Backlight turned ON");
        } else {
            digitalWrite(TFT_BL, LOW); // 关闭背光
            ESP_LOGD(TAG, "Backlight turned OFF");
        }
    }
    #endif
}

// ============================================================================
// 屏幕控制
// ============================================================================

void DisplayDriver::clear() {
    if (!isInitialized) return;
    fillScreen(config.backgroundColor);
}

void DisplayDriver::fillScreen(uint16_t color) {
    if (!isInitialized) return;
    tft.fillScreen(color);
}

void DisplayDriver::setBrightness(uint8_t brightness) {
    if (brightness > 100) brightness = 100;
    config.brightness = brightness;

    // 如果背光支持PWM，这里可以设置PWM占空比
    // 简化处理：只记录亮度值
    ESP_LOGI(TAG, "Brightness set to %u%%", brightness);
}

void DisplayDriver::setBacklight(bool enabled) {
    config.backlightEnabled = enabled;
    updateBacklight();
}

// ============================================================================
// 文本绘制
// ============================================================================

void DisplayDriver::setTextColor(uint16_t color, uint16_t bgColor) {
    if (!isInitialized) return;
    currentTextColor = color;
    currentBgColor = bgColor;
    tft.setTextColor(color, bgColor);
}

void DisplayDriver::setTextSize(uint8_t size) {
    if (!isInitialized) return;
    currentTextSize = size;
    tft.setTextSize(size);
}

void DisplayDriver::setTextFont(uint8_t font) {
    if (!isInitialized) return;
    tft.setTextFont(font);
}

void DisplayDriver::setTextWrap(bool wrap) {
    if (!isInitialized) return;
    tft.setTextWrap(wrap);
}

void DisplayDriver::drawText(int16_t x, int16_t y, const String& text, TextAlign align) {
    if (!isInitialized || text.isEmpty()) return;

    int16_t textX = x;
    int16_t textY = y;

    // 计算对齐
    if (align == TextAlign::CENTER) {
        int16_t textWidth = tft.textWidth(text.c_str());
        textX = x - textWidth / 2;
    } else if (align == TextAlign::RIGHT) {
        int16_t textWidth = tft.textWidth(text.c_str());
        textX = x - textWidth;
    }

    tft.setCursor(textX, textY);
    tft.print(text);
}

void DisplayDriver::drawTextCentered(int16_t y, const String& text) {
    if (!isInitialized) return;
    drawText(config.width / 2, y, text, TextAlign::CENTER);
}

void DisplayDriver::printf(int16_t x, int16_t y, const char* format, ...) {
    if (!isInitialized) return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    tft.setCursor(x, y);
    tft.print(buffer);
}

// ============================================================================
// 图形绘制
// ============================================================================

void DisplayDriver::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (!isInitialized) return;
    tft.drawPixel(x, y, color);
}

void DisplayDriver::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (!isInitialized) return;
    tft.drawLine(x0, y0, x1, y1, color);
}

void DisplayDriver::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!isInitialized) return;
    tft.drawRect(x, y, w, h, color);
}

void DisplayDriver::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!isInitialized) return;
    tft.fillRect(x, y, w, h, color);
}

void DisplayDriver::drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (!isInitialized) return;
    tft.drawCircle(x, y, r, color);
}

void DisplayDriver::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (!isInitialized) return;
    tft.fillCircle(x, y, r, color);
}

void DisplayDriver::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    if (!isInitialized) return;
    tft.drawTriangle(x0, y0, x1, y1, x2, y2, color);
}

void DisplayDriver::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    if (!isInitialized) return;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
}

// ============================================================================
// 图像绘制
// ============================================================================

void DisplayDriver::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) {
    if (!isInitialized) return;
    tft.drawBitmap(x, y, bitmap, w, h, color);
}

void DisplayDriver::drawXBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) {
    if (!isInitialized) return;
    tft.drawXBitmap(x, y, bitmap, w, h, color);
}

void DisplayDriver::drawJpeg(int16_t x, int16_t y, const uint8_t* jpegData, size_t jpegLen) {
    if (!isInitialized) return;
    // TFT_eSPI支持JPEG解码
    // tft.drawJpeg(jpegData, jpegLen, x, y);
    // 注意：需要包含TJpg_Decoder库
    ESP_LOGW(TAG, "JPEG drawing not implemented");
}

void DisplayDriver::drawPng(int16_t x, int16_t y, const uint8_t* pngData, size_t pngLen) {
    if (!isInitialized) return;
    ESP_LOGW(TAG, "PNG drawing not implemented");
}

// ============================================================================
// 进度条和图表
// ============================================================================

void DisplayDriver::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint16_t color, uint16_t bgColor) {
    if (!isInitialized) return;

    if (percent > 100) percent = 100;

    // 绘制背景
    fillRect(x, y, w, h, bgColor);

    // 计算填充宽度
    int16_t fillWidth = (w - 2) * percent / 100;

    // 绘制填充部分
    if (fillWidth > 0) {
        fillRect(x + 1, y + 1, fillWidth, h - 2, color);
    }

    // 绘制边框
    drawRect(x, y, w, h, TFT_WHITE);
}

void DisplayDriver::drawWaveform(int16_t x, int16_t y, int16_t w, int16_t h, const int16_t* data, size_t dataLength, uint16_t color) {
    if (!isInitialized || dataLength < 2) return;

    // 找到数据的最大值和最小值
    int16_t minVal = data[0];
    int16_t maxVal = data[0];
    for (size_t i = 1; i < dataLength; i++) {
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
    }

    // 计算缩放因子
    float yScale = (float)(h - 2) / (maxVal - minVal);
    float xScale = (float)(w - 2) / (dataLength - 1);

    // 绘制波形
    for (size_t i = 0; i < dataLength - 1; i++) {
        int16_t x0 = x + 1 + (int16_t)(i * xScale);
        int16_t y0 = y + 1 + (int16_t)((data[i] - minVal) * yScale);
        int16_t x1 = x + 1 + (int16_t)((i + 1) * xScale);
        int16_t y1 = y + 1 + (int16_t)((data[i + 1] - minVal) * yScale);

        drawLine(x0, y0, x1, y1, color);
    }

    // 绘制边框
    drawRect(x, y, w, h, TFT_WHITE);
}

// ============================================================================
// 屏幕信息
// ============================================================================

void DisplayDriver::setRotation(uint8_t rotation) {
    if (!isInitialized) return;

    config.rotation = rotation % 4;
    tft.setRotation(config.rotation);
}

// ============================================================================
// 配置管理
// ============================================================================

bool DisplayDriver::updateConfig(const DisplayConfig& newConfig) {
    if (!isInitialized) {
        return initialize(newConfig);
    }

    config = newConfig;
    applyConfig();

    return true;
}

// ============================================================================
// 工具方法
// ============================================================================

uint16_t DisplayDriver::colorRGB(uint8_t r, uint8_t g, uint8_t b) {
    return tft.color565(r, g, b);
}

uint16_t DisplayDriver::colorHSV(uint16_t hue, uint8_t sat, uint8_t val) {
    // 简化HSV到RGB转换
    // 实际实现需要完整转换算法
    uint8_t r, g, b;
    // 这里使用简化转换
    r = val;
    g = val;
    b = val;
    return colorRGB(r, g, b);
}

uint16_t DisplayDriver::getPixel(int16_t x, int16_t y) {
    if (!isInitialized) return 0;
    return tft.readPixel(x, y);
}

// ============================================================================
// 测试方法
// ============================================================================

bool DisplayDriver::testDisplay() {
    if (!isInitialized) {
        ESP_LOGE(TAG, "DisplayDriver not initialized");
        return false;
    }

    ESP_LOGI(TAG, "Testing display...");

    // 显示测试图案
    showTestPattern();

    // 短暂延迟
    delay(2000);

    // 显示颜色条
    showColorBars();

    // 短暂延迟
    delay(2000);

    // 显示文本演示
    showTextDemo();

    ESP_LOGI(TAG, "Display test completed");
    return true;
}

void DisplayDriver::showTestPattern() {
    clear();

    // 绘制网格
    for (int x = 0; x < config.width; x += 20) {
        drawLine(x, 0, x, config.height - 1, TFT_GRAY);
    }
    for (int y = 0; y < config.height; y += 20) {
        drawLine(0, y, config.width - 1, y, TFT_GRAY);
    }

    // 绘制中心十字
    drawLine(config.width / 2, 0, config.width / 2, config.height - 1, TFT_RED);
    drawLine(0, config.height / 2, config.width - 1, config.height / 2, TFT_RED);

    // 显示测试文本
    drawTextCentered(config.height / 2 - 20, "Test Pattern");
    drawTextCentered(config.height / 2 + 20, String(config.width) + "x" + String(config.height));
}

void DisplayDriver::showColorBars() {
    clear();

    const uint16_t colors[] = {
        TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW,
        TFT_CYAN, TFT_MAGENTA, TFT_WHITE, TFT_BLACK
    };

    int barWidth = config.width / 8;
    int barHeight = config.height;

    for (int i = 0; i < 8; i++) {
        fillRect(i * barWidth, 0, barWidth, barHeight, colors[i]);
    }

    // 显示文本
    setTextColor(TFT_BLACK, TFT_WHITE);
    drawTextCentered(20, "Color Bars");
    setTextColor(TFT_WHITE, TFT_BLACK);
}

void DisplayDriver::showTextDemo() {
    clear();

    int y = 20;

    // 不同大小的文本
    setTextSize(1);
    drawTextCentered(y, "Text Size 1");
    y += 20;

    setTextSize(2);
    drawTextCentered(y, "Text Size 2");
    y += 30;

    setTextSize(3);
    drawTextCentered(y, "Text Size 3");
    y += 40;

    // 不同颜色的文本
    setTextSize(2);
    setTextColor(TFT_RED);
    drawTextCentered(y, "Red Text");
    y += 30;

    setTextColor(TFT_GREEN);
    drawTextCentered(y, "Green Text");
    y += 30;

    setTextColor(TFT_BLUE);
    drawTextCentered(y, "Blue Text");
    y += 30;

    // 重置颜色
    setTextColor(TFT_WHITE);
}

// ============================================================================
// 状态显示
// ============================================================================

void DisplayDriver::showBootScreen(const String& appName, const String& version) {
    clear();

    // 绘制应用名称
    setTextSize(3);
    setTextColor(TFT_CYAN);
    drawTextCentered(config.height / 2 - 30, appName);

    // 绘制版本号
    setTextSize(2);
    setTextColor(TFT_YELLOW);
    drawTextCentered(config.height / 2 + 10, "v" + version);

    // 绘制启动提示
    setTextSize(1);
    setTextColor(TFT_GREEN);
    drawTextCentered(config.height - 20, "Initializing...");
}

void DisplayDriver::showStatus(const String& status, uint16_t color) {
    if (!isInitialized) return;

    // 在屏幕底部显示状态
    int statusY = config.height - 20;
    fillRect(0, statusY, config.width, 20, TFT_BLACK);

    setTextSize(1);
    setTextColor(color);
    drawTextCentered(statusY + 10, status);
}

void DisplayDriver::showError(const String& error) {
    showStatus("ERROR: " + error, TFT_RED);
}

void DisplayDriver::showWarning(const String& warning) {
    showStatus("WARN: " + warning, TFT_YELLOW);
}

void DisplayDriver::showInfo(const String& info) {
    showStatus("INFO: " + info, TFT_GREEN);
}