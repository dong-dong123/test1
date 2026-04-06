/**
 * 引脚定义验证测试程序
 * 功能：验证PinDefinitions.h中定义的引脚是否正确
 */

#include <Arduino.h>
#include "drivers/PinDefinitions.h"

void setup() {
  Serial.begin(115200);
  delay(2000); // 给USB枚举足够时间

  Serial.println("\n=== Pin Definitions Verification Test ===");

  // 显示所有引脚定义
  Serial.println("\n--- Display Pins (ST7789 SPI2 HSPI) ---");
  Serial.print("TFT_SCLK:  "); Serial.println(TFT_SCLK);
  Serial.print("TFT_MOSI:  "); Serial.println(TFT_MOSI);
  Serial.print("TFT_CS:    "); Serial.println(TFT_CS);
  Serial.print("TFT_DC:    "); Serial.println(TFT_DC);
  Serial.print("TFT_RST:   "); Serial.println(TFT_RST);

  Serial.println("\n--- Microphone Pins (INMP441 I2S) ---");
  Serial.print("I2S_MIC_SDO:  "); Serial.println(I2S_MIC_SDO);
  Serial.print("I2S_MIC_WS:   "); Serial.println(I2S_MIC_WS);
  Serial.print("I2S_MIC_BCLK: "); Serial.println(I2S_MIC_BCLK);

  Serial.println("\n--- Speaker Pins (MAX98357A I2S) ---");
  Serial.print("I2S_SPK_DIN:  "); Serial.println(I2S_SPK_DIN);
  Serial.print("I2S_SPK_LRC:  "); Serial.println(I2S_SPK_LRC);
  Serial.print("I2S_SPK_BCLK: "); Serial.println(I2S_SPK_BCLK);

  Serial.println("\n--- Optional Function Pins ---");
  Serial.print("BUTTON_WAKE: "); Serial.println(BUTTON_WAKE);
  Serial.print("LED_STATUS:  "); Serial.println(LED_STATUS);

  // 验证引脚冲突
  Serial.println("\n--- Pin Conflict Check ---");

  // 检查I2S引脚是否冲突
  if (I2S_MIC_WS == I2S_SPK_LRC && I2S_MIC_BCLK == I2S_SPK_BCLK) {
    Serial.println("✓ I2S clock pins shared between mic and speaker (expected)");
  } else {
    Serial.println("⚠ I2S clock pins not shared");
  }

  // 检查引脚范围
  bool all_pins_valid = true;
  int pins[] = {TFT_SCLK, TFT_MOSI, TFT_CS, TFT_DC, TFT_RST,
                I2S_MIC_SDO, I2S_MIC_WS, I2S_MIC_BCLK,
                I2S_SPK_DIN, I2S_SPK_LRC, I2S_SPK_BCLK};
  int pin_count = sizeof(pins) / sizeof(pins[0]);

  for (int i = 0; i < pin_count; i++) {
    if (pins[i] < 0 || pins[i] > 48) { // ESP32-S3 GPIO范围
      Serial.print("⚠ Pin ");
      Serial.print(pins[i]);
      Serial.println(" is outside valid GPIO range (0-48)");
      all_pins_valid = false;
    }
  }

  if (all_pins_valid) {
    Serial.println("✓ All pins are within valid GPIO range");
  }

  Serial.println("\n=== Test Complete ===");
  Serial.println("Note: This test only verifies pin definitions, not hardware connections.");
}

void loop() {
  // 空循环，程序只运行一次setup()
}