#include <Arduino.h>
#include <time.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "MainApplication.h"
#include "drivers/PinDefinitions.h"
#include "drivers/ButtonDriver.h"

// 主应用程序实例
MainApplication app;

// 按钮驱动实例（使用GPIO0，内部上拉，150ms消抖）
ButtonDriver buttonDriver(BUTTON_PIN, ButtonDriver::PullMode::PULL_UP, 150);

// 时间同步状态
static bool timeSynchronized = false;
static bool timeSyncAttempted = false;

// 串口事件处理函数声明
void serialEvent();

// 时间同步函数声明
bool syncTimeWithNTP();

// SSL测试函数声明
void testSSLConnection();
void testSSLToHost(const char *host, uint16_t port);

void setup()
{
  // 初始化串口（用于调试）
  Serial.begin(115200);

  // 时间同步将在网络连接后自动进行

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
  // 检查时间同步（如果WiFi已连接但时间未同步）
  static bool timeSyncChecked = false;
  if (!timeSyncChecked && WiFi.status() == WL_CONNECTED)
  {
    // 检查当前时间，如果年份是1970（默认值），则需要同步
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // 详细的时间诊断
    Serial.printf("Time diagnostic - raw: %lld, year: %d, date: %04d-%02d-%02d %02d:%02d:%02d\n",
                  (long long)now, timeinfo.tm_year + 1900, timeinfo.tm_year + 1900,
                  timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour,
                  timeinfo.tm_min, timeinfo.tm_sec);

    if (timeinfo.tm_year + 1900 < 2020)
    { // 时间未同步（年份早于2020）
      Serial.println("Time appears to be unsynchronized, attempting NTP sync...");
      if (syncTimeWithNTP())
      {
        timeSyncChecked = true;
        // 重新获取时间显示同步后的时间
        now = time(nullptr);
        localtime_r(&now, &timeinfo);
        Serial.printf("Time synchronized successfully: %04d-%02d-%02d %02d:%02d:%02d\n",
                      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      }
      else
      {
        Serial.println("NTP synchronization failed. SSL certificate validation may fail.");
      }
    }
    else if (timeinfo.tm_year + 1900 > 2030)
    {
      Serial.printf("Time appears to be in distant future (year: %d). This may cause SSL issues.\n",
                    timeinfo.tm_year + 1900);
      timeSyncChecked = true; // 标记为已检查，但时间可能有问题
    }
    else
    {
      timeSyncChecked = true;
      Serial.printf("Time already synchronized: %04d-%02d-%02d %02d:%02d:%02d\n",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
  }

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
          Serial.println("=== WiFi Configuration Portal ===");
          Serial.println("Starting AP hotspot mode for WiFi configuration...");
          Serial.println("Hotspot: XiaozhiAP (no password)");
          Serial.println("Web interface: http://192.168.4.1");
          Serial.println("Configuration portal will stay active for 10 minutes");
          Serial.println("Connect to XiaozhiAP and configure your WiFi in browser");
          Serial.println();

          // 调用NetworkManager启动AP热点模式
          if (app.isReady())
          {
            NetworkManager *networkMgr = app.getNetworkManager();
            if (networkMgr)
            {
              bool success = networkMgr->startWiFiManagerHotspot();
              if (success)
              {
                Serial.println("[SUCCESS] WiFi configuration completed!");
                Serial.println("Device will now connect to configured WiFi network.");
                Serial.println("Type 'status' to check connection status.");
              }
              else
              {
                Serial.println("[FAILED] WiFi configuration portal timed out or was cancelled.");
                Serial.println("Hotspot has been closed. Type 'startap' to try again.");
              }
            }
            else
            {
              Serial.println("[ERROR] NetworkManager not available");
              Serial.println("Trying device restart to enter configuration mode...");
              ESP.restart();
            }
          }
          else
          {
            Serial.println("[WARNING] Application not fully initialized");
            Serial.println("Restarting device to enter configuration mode...");
            ESP.restart();
          }
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
        else if (command == "synctime")
        {
          Serial.println("Manually synchronizing time with NTP...");
          if (syncTimeWithNTP())
          {
            Serial.println("Time synchronization successful");
          }
          else
          {
            Serial.println("Time synchronization failed");
          }
        }
        else if (command == "ssltest")
        {
          Serial.println("Testing SSL connection to openspeech.bytedance.com...");
          testSSLConnection();
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
          Serial.println("  synctime     - Synchronize time with NTP");
          Serial.println("  ssltest      - Test SSL connection to API server");
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

// 时间同步函数
bool syncTimeWithNTP()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected, cannot synchronize time");
    return false;
  }

  Serial.println("Starting NTP time synchronization...");
  Serial.printf("WiFi status: %d, RSSI: %d dBm\n", WiFi.status(), WiFi.RSSI());

  // 首先获取当前时间（同步前）
  time_t beforeSync = time(nullptr);
  struct tm beforeTimeinfo;
  localtime_r(&beforeSync, &beforeTimeinfo);
  Serial.printf("Time before NTP sync: raw=%lld, %04d-%02d-%02d %02d:%02d:%02d\n",
                (long long)beforeSync, beforeTimeinfo.tm_year + 1900,
                beforeTimeinfo.tm_mon + 1, beforeTimeinfo.tm_mday,
                beforeTimeinfo.tm_hour, beforeTimeinfo.tm_min, beforeTimeinfo.tm_sec);

  // 配置NTP时间同步
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("NTP configured with servers: pool.ntp.org, time.nist.gov, GMT+8");

  // 尝试时间同步（最多6次，每次等待3秒，总共最多18秒）
  bool timeSynced = false;
  struct tm timeinfo;

  for (int attempt = 1; attempt <= 6; attempt++)
  {
    delay(3000); // 等待时间同步，给NTP服务器更多时间响应

    if (getLocalTime(&timeinfo, 100))
    { // 添加100ms超时
      timeSynced = true;
      time_t currentTime = time(nullptr);
      Serial.printf("Time synchronized (attempt %d/%d): raw=%lld, %04d-%02d-%02d %02d:%02d:%02d\n",
                    attempt, 6, (long long)currentTime, timeinfo.tm_year + 1900,
                    timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour,
                    timeinfo.tm_min, timeinfo.tm_sec);

      // 验证时间是否合理（不在1970年或未来太远）
      if (timeinfo.tm_year + 1900 < 2020)
      {
        Serial.printf("WARNING: Synchronized time still appears incorrect (year: %d)\n",
                      timeinfo.tm_year + 1900);
        timeSynced = false; // 继续尝试
        continue;
      }
      break;
    }
    else
    {
      Serial.printf("Time synchronization failed (attempt %d/%d)\n", attempt, 6);
      // 检查WiFi状态
      if (WiFi.status() != WL_CONNECTED)
      {
        Serial.println("WiFi connection lost during NTP sync");
        break;
      }
    }
  }

  if (!timeSynced)
  {
    Serial.println("ERROR: Failed to synchronize time after multiple attempts!");
    Serial.println("SSL certificate validation will likely fail!");
    // 显示最终时间状态
    time_t finalTime = time(nullptr);
    struct tm finalTimeinfo;
    localtime_r(&finalTime, &finalTimeinfo);
    Serial.printf("Final time state: raw=%lld, %04d-%02d-%02d %02d:%02d:%02d\n",
                  (long long)finalTime, finalTimeinfo.tm_year + 1900,
                  finalTimeinfo.tm_mon + 1, finalTimeinfo.tm_mday,
                  finalTimeinfo.tm_hour, finalTimeinfo.tm_min, finalTimeinfo.tm_sec);
  }
  else
  {
    Serial.println("Time synchronization successful");
    // 显示时间差
    time_t afterSync = time(nullptr);
    long timeDiff = (long)(afterSync - beforeSync);
    Serial.printf("Time changed by %ld seconds (%02ld:%02ld:%02ld)\n",
                  timeDiff, timeDiff / 3600, (timeDiff % 3600) / 60, timeDiff % 60);

    // 添加时区确认日志
    struct tm timeinfo;
    localtime_r(&afterSync, &timeinfo);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.printf("Local time confirmed: %s (GMT+8)\n", timeStr);
    Serial.printf("Timezone offset: GMT+8, raw time: %lld\n", (long long)afterSync);
  }

  return timeSynced;
}

// SSL连接测试函数
void testSSLConnection()
{
  Serial.println("=== SSL Connection Test ===");

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("ERROR: WiFi not connected");
    Serial.printf("WiFi status: %d\n", WiFi.status());
    return;
  }

  // 测试DNS解析
  Serial.println("=== DNS Resolution Test ===");
  Serial.println("Testing DNS resolution for openspeech.bytedance.com...");

  IPAddress resolvedIP;
  unsigned long dnsStartTime = millis();
  int dnsResult = WiFi.hostByName("openspeech.bytedance.com", resolvedIP);
  unsigned long dnsTime = millis() - dnsStartTime;

  if (dnsResult == 1)
  {
    Serial.printf("DNS resolution SUCCESS: openspeech.bytedance.com -> %s (took %lu ms)\n",
                  resolvedIP.toString().c_str(), dnsTime);
  }
  else
  {
    Serial.printf("DNS resolution FAILED (took %lu ms)\n", dnsTime);
    Serial.println("DNS resolution failed for openspeech.bytedance.com");
  }

  // 显示当前DNS配置
  Serial.println("=== Current DNS Configuration ===");
  IPAddress dns1 = WiFi.dnsIP(0);
  IPAddress dns2 = WiFi.dnsIP(1);
  if (dns1 != INADDR_NONE)
  {
    Serial.printf("DNS server 1: %s\n", dns1.toString().c_str());
  }
  else
  {
    Serial.println("DNS server 1: not set");
  }
  if (dns2 != INADDR_NONE)
  {
    Serial.printf("DNS server 2: %s\n", dns2.toString().c_str());
  }

  // 测试其他网站以对比
  Serial.println("=== Comparative SSL Tests ===");

  // 测试1: openspeech.bytedance.com
  Serial.println("\n1. Testing openspeech.bytedance.com:443");
  testSSLToHost("openspeech.bytedance.com", 443);

  // 测试2: google.com (作为对比)
  Serial.println("\n2. Testing google.com:443 (for comparison)");
  testSSLToHost("google.com", 443);

  // 测试3: 百度 (作为对比)
  Serial.println("\n3. Testing www.baidu.com:443 (for comparison)");
  testSSLToHost("www.baidu.com", 443);

  Serial.println("=== End SSL Test ===");
}

// 辅助函数：测试到指定主机的SSL连接
void testSSLToHost(const char *host, uint16_t port)
{
  WiFiClientSecure client;
  client.setInsecure(); // 仅用于测试，禁用证书验证

  Serial.printf("Connecting to %s:%d...", host, port);

  unsigned long startTime = millis();
  bool connected = client.connect(host, port);
  unsigned long connectTime = millis() - startTime;

  if (connected)
  {
    Serial.printf("SUCCESS (took %lu ms)\n", connectTime);

    // 发送简单的HTTP请求测试数据发送
    Serial.println("  Testing data transmission...");
    client.printf("GET / HTTP/1.1\r\n");
    client.printf("Host: %s\r\n", host);
    client.printf("Connection: close\r\n\r\n");

    // 等待响应
    delay(500);

    // 读取响应（前100字节）
    Serial.println("  Response (first 100 bytes):");
    int bytesRead = 0;
    while (client.available() && bytesRead < 100)
    {
      char c = client.read();
      Serial.write(c);
      bytesRead++;
    }

    if (bytesRead == 0)
    {
      Serial.println("  (No response received)");
    }

    client.stop();
    Serial.println("  SSL connection test completed successfully");
  }
  else
  {
    Serial.printf("FAILED (took %lu ms)\n", connectTime);
    Serial.printf("  SSL connection to %s failed\n", host);

    // 尝试获取更多错误信息
    // 注意：WiFiClientSecure可能没有getLastSSLError()方法
    // 在ESP32 Arduino中，可以使用client.lastError()或其他方法
    // 暂时注释掉以避免编译错误
    // int sslError = client.getLastSSLError();
    // if (sslError != 0) {
    //   Serial.printf("  SSL error code: %d\n", sslError);
    // }
  }
}