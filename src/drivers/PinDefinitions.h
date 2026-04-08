#ifndef PIN_DEFINITIONS_H
#define PIN_DEFINITIONS_H

// ST7789显示屏引脚 (SPI2 HSPI)
#ifndef TFT_MISO
#define TFT_MISO  -1  // MISO引脚（ST7789为只写显示器，通常不使用）
#endif
#ifndef TFT_SCLK
#define TFT_SCLK  12  // HSPI默认SCLK引脚
#endif
#ifndef TFT_MOSI
#define TFT_MOSI  11  // HSPI默认MOSI引脚
#endif
#ifndef TFT_CS
#define TFT_CS    10  // HSPI默认CS引脚
#endif
#ifndef TFT_DC
#define TFT_DC    4   // 数据/命令
#endif
#ifndef TFT_RST
#define TFT_RST   5   // 复位
#endif

// INMP441麦克风引脚 (I2S)
#ifndef I2S_MIC_SDO
#define I2S_MIC_SDO  14  // 数据输出 (SD)
#endif
#ifndef I2S_MIC_WS
#define I2S_MIC_WS   16  // 帧时钟 (WS/LRC)
#endif
#ifndef I2S_MIC_BCLK
#define I2S_MIC_BCLK 15  // 位时钟 (SCK/BCLK)
#endif

// MAX98357A扬声器引脚 (I2S)
#ifndef I2S_SPK_DIN
#define I2S_SPK_DIN   7  // 数据输入 (DIN)
#endif
#ifndef I2S_SPK_LRC
#define I2S_SPK_LRC   16 // 帧时钟 (WS/LRC) - 与麦克风共用
#endif
#ifndef I2S_SPK_BCLK
#define I2S_SPK_BCLK  15 // 位时钟 (SCK/BCLK) - 与麦克风共用
#endif

// 可选功能引脚
#ifndef BUTTON_WAKE
#define BUTTON_WAKE  -1  // 唤醒按钮引脚（预留）
#endif
#ifndef BUTTON_PIN
#define BUTTON_PIN   0   // 主按钮引脚（GPIO0，ESP32-S3 BOOT按钮）
#endif
#ifndef LED_STATUS
#define LED_STATUS   -1  // 状态LED引脚（预留）
#endif

#endif