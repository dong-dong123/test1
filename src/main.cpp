#include <Arduino.h>
#include <time.h>
#include <WiFi.h>
#include "MainApplication.h"
#include "drivers/PinDefinitions.h"
#include "drivers/ButtonDriver.h"

// 主应用程序实例
MainApplication app;

// 按钮驱动实例（使用GPIO0，内部上拉，150ms消抖）
ButtonDriver buttonDriver(BUTTON_PIN, ButtonDriver::PullMode::PULL_UP, 150);

// 串口事件处理函数声明
void serialEvent();

void setup()
{
  // 初始化串口（用于调试）
  Serial.begin(115200);

  // === 新增：NTP时间同步（修复SSL证书验证）===
  Serial.println("Waiting for WiFi connection before time synchronization...");

  // 等待WiFi连接（最多30秒）
  int wifiWaitCount = 0;
  while (WiFi.status() != WL_CONNECTED && wifiWaitCount < 30) {
    delay(1000);
    wifiWaitCount++;
    Serial.printf("WiFi status: %d, waiting... (%d/30)\n", WiFi.status(), wifiWaitCount);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WARNING: WiFi not connected, time synchronization may fail!");
  } else {
    Serial.println("WiFi connected, starting time synchronization...");
  }

  // 配置NTP时间同步
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // 尝试时间同步（最多5次，每次等待2秒）
  bool timeSynced = false;
  struct tm timeinfo;

  for (int attempt = 1; attempt <= 5; attempt++) {
    delay(2000); // 等待时间同步

    if (getLocalTime(&timeinfo)) {
      timeSynced = true;
      Serial.printf("Time synchronized (attempt %d): %04d-%02d-%02d %02d:%02d:%02d\n",
                    attempt, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      break;
    } else {
      Serial.printf("Time synchronization failed (attempt %d/%d)\n", attempt, 5);
    }
  }

  if (!timeSynced) {
    Serial.println("WARNING: Failed to synchronize time after multiple attempts!");
    Serial.println("SSL certificate validation may fail!");
  }
  // === 新增结束 ===

  delay(2000); // 给USB枚举足够时间

  Serial.println("\n=== Xiaozhi Voice Assistant ===");
  Serial.println("System: " + String(SYSTEM_NAME) + " v" + String(SYSTEM_VERSION));
  Serial.println("Build: " + String(__DATE__) + " " + String(__TIME__));

  // 初始化按钮驱动
  if (buttonDriver.begin())
  {
    if (BUTTON_PIN >= 0)
    {
      Serial.println("Button initialized on pin " + String(BUTTON_PIN));
    }
    else
    {
      Serial.println("No button configured, using software trigger");
    }
  }

  // 初始化应用程序
  Serial.println("Initializing application...");
  if (!app.initialize())
  {
    // 如果初始化失败，显示错误并进入深度睡眠
    Serial.println("ERROR: Application initialization failed!");
    Serial.println("Last error: " + app.getLastError());

    // 尝试显示错误信息（如果显示驱动已初始化）
    app.getDisplayDriver()->showError("Init Failed");

    // 等待5秒后进入深度睡眠
    delay(5000);
    esp_deep_sleep_start();
  }

  Serial.println("Application initialized successfully");
  Serial.println("Current state: " + stateToString(app.getCurrentState()));
  Serial.println("Ready for operation");
  Serial.println("=============================");
}

void loop()
{
  // 主循环更新
  app.update();

  // 简单的用户交互：按钮按下开始录音
  ButtonDriver::Event buttonEvent = buttonDriver.update();

  if (buttonEvent == ButtonDriver::Event::PRESSED)
  {
    Serial.println("Button pressed - starting listening");
    app.startListening();
  }
  else if (BUTTON_PIN < 0)
  {
    // 如果没有按钮，使用定时器模拟用户输入（用于测试）
    static uint32_t lastAutoTrigger = 0;
    if (millis() - lastAutoTrigger > 30000)
    { // 每30秒自动触发一次
      Serial.println("Auto trigger - starting listening");
      app.startListening();
      lastAutoTrigger = millis();
    }
  }

  // 检查状态变化并输出到串口（已由logEvent处理，此处注释以避免重复）
  // static SystemState lastState = SystemState::BOOTING;
  // SystemState currentState = app.getCurrentState();
  // if (currentState != lastState)
  // {
  //   Serial.println("State changed: " + stateToString(lastState) +
  //                  " -> " + stateToString(currentState));
  //   lastState = currentState;
  // }

  // 防止任务饥饿，但保持响应性
  serialEvent(); // 处理串口命令
  delay(1);
}

// 可选：添加串口命令处理（用于调试）
void serialEvent()
{
  static String command = "";

  while (Serial.available())
  {
    char c = Serial.read();

    if (c == '\n' || c == '\r')
    {
      // 处理完整命令
      command.trim();

      if (command.length() > 0)
      {
        Serial.println("Received command: " + command);

        // 处理命令
        if (command == "start" || command == "listen")
        {
          app.startListening();
        }
        else if (command == "stop")
        {
          app.stopListening();
        }
        else if (command == "status")
        {
          Serial.println("Current state: " + stateToString(app.getCurrentState()));
          Serial.println("Last error: " + app.getLastError());
        }
        else if (command == "setwifi")
        {
          Serial.println("Usage: setwifi <ssid> <password>");
          Serial.println("Example: setwifi 宫廷玉液酒 180onecup");
        }
        else if (command.startsWith("setwifi "))
        {
          // 解析命令：setwifi ssid password
          int firstSpace = command.indexOf(' ');
          int secondSpace = command.indexOf(' ', firstSpace + 1);

          if (secondSpace != -1)
          {
            String ssid = command.substring(firstSpace + 1, secondSpace);
            String password = command.substring(secondSpace + 1);

            // 更新Wi-Fi配置
            app.getConfigManager()->setString("wifi.ssid", ssid);
            app.getConfigManager()->setString("wifi.password", password);
            app.getConfigManager()->save();

            Serial.println("Wi-Fi配置已更新:");
            Serial.println("  SSID: " + ssid);
            Serial.println("  密码: " + password);
            Serial.println("配置已保存，重启后生效");
          }
          else
          {
            Serial.println("错误: 请提供SSID和密码");
            Serial.println("格式: setwifi <ssid> <password>");
          }
        }
        else if (command == "restart")
        {
          Serial.println("Restarting device...");
          ESP.restart();
        }
        else if (command == "startap")
        {
          Serial.println("Forcing WiFi configuration mode...");
          Serial.println("Note: If XiaozhiAP hotspot doesn't appear,");
          Serial.println("try restarting the device or use setwifi command instead.");
          // 这里可以调用网络管理器重启并进入配网模式
          // 暂时使用系统重启，设备会自动进入配网模式（如果配置了autoConnect但连接失败）
          ESP.restart();
        }
        else if (command == "testap")
        {
          Serial.println("=== WiFi AP Diagnostic Test ===");
          Serial.println("This will test if ESP32-S3 can create a visible hotspot");
          Serial.println("Testing with different settings...");

          // 这里可以调用网络管理器进行AP测试
          // 暂时使用重启并强制进入测试模式
          Serial.println("Starting hotspot test in 3 seconds...");
          Serial.println("Look for 'TestAP-1' on channel 1");
          delay(3000);

          // 临时解决方案：通过修改配置强制进入测试模式
          app.getConfigManager()->setString("wifi.ssid", "TEST_DISABLED");
          app.getConfigManager()->save();
          ESP.restart();
        }
        else if (command == "clearwifi")
        {
          Serial.println("Clearing saved WiFi credentials...");
          // 使用NetworkManager彻底清除保存的网络（包括WiFiManager配置）
          if (app.getNetworkManager())
          {
            app.getNetworkManager()->clearSavedNetworks();
          }
          else
          {
            WiFi.disconnect(true); // 备用方法
            delay(100);
          }
          Serial.println("WiFi credentials cleared. Device will restart...");
          ESP.restart();
        }
        else if (command == "error")
        {
          Serial.println("=== Last Error Information ===");
          Serial.println("Last error: " + app.getLastError());
          Serial.println("Current state: " + stateToString(app.getCurrentState()));
        }
        else if (command == "reset" || command == "idle")
        {
          Serial.println("Resetting state to IDLE...");
          // 注意：这里无法直接访问changeState，因为它是private
          // 作为临时解决方案，重启设备
          Serial.println("Note: Using restart instead (no direct state reset method)");
          Serial.println("Use 'restart' command or wait for automatic recovery (5 seconds in ERROR state)");
        }
        else if (command == "debug")
        {
          Serial.println("=== Debug Information ===");
          Serial.println("System state: " + stateToString(app.getCurrentState()));
          Serial.println("Last error: " + app.getLastError());
          // 注意：其他状态信息需要MainApplication添加相应public方法
          Serial.println("Use 'status' for basic status, or 'restart' to reboot");
        }
        else if (command == "help")
        {
          Serial.println("Available commands:");
          Serial.println("  start/listen - Start listening");
          Serial.println("  stop         - Stop listening");
          Serial.println("  status       - Show system status");
          Serial.println("  restart      - Restart device");
          Serial.println("  reset/idle   - Attempt to reset state (info only)");
          Serial.println("  error        - Show detailed error information");
          Serial.println("  debug        - Show debug system information");
          Serial.println("  startap      - Force WiFi configuration mode");
          Serial.println("  setwifi      - 设置Wi-Fi SSID和密码");
          Serial.println("  clearwifi    - 清除保存的WiFi凭据");
          Serial.println("  help         - Show this help");
        }
        else
        {
          Serial.println("Unknown command: " + command);
          Serial.println("Type 'help' for available commands");
        }

        command = "";
      }
    }
    else
    {
      command += c;
    }
  }
}