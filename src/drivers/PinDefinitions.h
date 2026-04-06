#ifndef PIN_DEFINITIONS_H
#define PIN_DEFINITIONS_H

// ST7789显示屏引脚 (SPI2 HSPI)
#define TFT_SCLK  12  // HSPI默认SCLK引脚
#define TFT_MOSI  11  // HSPI默认MOSI引脚
#define TFT_CS    10  // HSPI默认CS引脚
#define TFT_DC    4   // 数据/命令
#define TFT_RST   5   // 复位

// INMP441麦克风引脚 (I2S)
#define I2S_MIC_SDO  14  // 数据输出 (SD)
#define I2S_MIC_WS   16  // 帧时钟 (WS/LRC)
#define I2S_MIC_BCLK 15  // 位时钟 (SCK/BCLK)

// MAX98357A扬声器引脚 (I2S)
#define I2S_SPK_DIN   7  // 数据输入 (DIN)
#define I2S_SPK_LRC   16 // 帧时钟 (WS/LRC) - 与麦克风共用
#define I2S_SPK_BCLK  15 // 位时钟 (SCK/BCLK) - 与麦克风共用

// 可选功能引脚
#define BUTTON_WAKE  -1  // 唤醒按钮引脚（预留）
#define LED_STATUS   -1  // 状态LED引脚（预留）

#endif