#include <Arduino.h>
#include "globals.h"
#include "drivers/PinDefinitions.h"

/**
 * ESP32-S3内存信息测试程序
 * 功能：显示ESP32-S3的内置RAM、PSRAM和Flash信息
 */

void setup()
{
  // 初始化串口 - ESP32-S3使用USB CDC串口
  Serial.begin(115200);
  delay(2000); // 给USB枚举足够时间

  Serial.println("\n=== ESP32-S3 Memory Info Test ===");

  // 获取并显示内置RAM剩余大小
  Serial.print("Free Heap Memory: ");
  Serial.print(esp_get_free_heap_size());
  Serial.println(" bytes");

  // 获取并显示PSRAM剩余大小
  // MALLOC_CAP_SPIRAM表示SPI RAM（外部PSRAM）
  Serial.print("Free PSRAM Memory: ");
  Serial.print(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  Serial.println(" bytes");

  // 获取并显示Flash总大小
  Serial.print("Total Flash Size: ");
  Serial.print(ESP.getFlashChipSize());
  Serial.println(" bytes");

  Serial.println("=== Test Complete ===");
}

void loop()
{
  // 空循环，程序只运行一次setup()
}
