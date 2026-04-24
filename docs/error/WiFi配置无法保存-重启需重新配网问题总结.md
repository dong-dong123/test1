# WiFi配置无法保存问题总结 — 启动时清除NVS凭据 + 入口函数不尝试自动连接（2026-04-25）

## 问题描述

通过手机连接 `XiaozhiAP` 热点配置WiFi成功后，重启ESP32-S3板子，每次都需要重新用手机配置网络。WiFi凭据未被持久化保存，无法实现"一次配置，永久使用"。

### 复现步骤

1. 板子启动，进入热点模式 `XiaozhiAP`
2. 手机连接热点，打开 `192.168.4.1` 配置页面
3. 输入正确的WiFi SSID和密码，连接成功
4. 执行 `restart` 命令或断电重启
5. 板子再次启动热点模式，需重新配置

### 实际日志

```
[NetworkManager] Wi-Fi mode setting succeeded on first attempt
[NetworkManager] Wi-Fi TX power set to 19.5dBm
[NetworkManager] Final Wi-Fi status after initialization: 6
[8089] [INFO] NetworkManager initialized
[8089] [INFO] Wi-Fi SSID: , Auto-reconnect: enabled
[NetworkManager] WiFi Configuration Portal Starting...
[NetworkManager] Hotspot: XiaozhiAP (no password)
```

注意日志中 `Wi-Fi SSID:` 为空且紧跟着 `WiFi Configuration Portal Starting...`，说明从未尝试用已保存凭据连接。

## 根因分析

### 直接原因：启动时清除WiFiManager NVS凭据

`NetworkManager::initialize()` 中有两处操作在每次启动时主动擦除了WiFi凭据：

| 代码 | 位置 | 影响 |
|------|------|------|
| `WiFi.disconnect(true)` | `NetworkManager.cpp:88` | `true` 参数清除ESP32 WiFi堆栈的NVS保存凭据 |
| `WiFi.disconnect(true)` | `NetworkManager.cpp:195` | 同上，第二次清除 |
| `tempWM.resetSettings()` | `NetworkManager.cpp:197-199` | **关键问题**：创建临时WiFiManager实例调用 `resetSettings()`，清除WiFiManager NVS中保存的全部凭据 |

这三处操作导致每次启动时，无论之前是否配网成功，保存的凭据都被清空。

### 根本原因：错误的使用WiFiManager入口函数

`connect()` 中当SSID为空时直接调用 `startWiFiManagerHotspot()` → `startConfigPortal()`。**但 `startConfigPortal()` 不会尝试用NVS中已保存的凭据连接，它直接启动AP热点**。

WiFiManager 2.0.17 源码行为对比：

| 函数 | 行为 |
|------|------|
| `startConfigPortal()` | 直接启动AP热点，不尝试连接已保存网络 |
| `autoConnect()` | 先调用 `connectWifi()` 尝试用NVS凭据连接，失败才启动AP |

正确做法应使用 `autoConnect()` 而非 `startConfigPortal()`，让WiFiManager有机会用NVS凭据自动连接。

### 关联因素

SPIFFS配置文件 `/config.json` 中虽然保存了 `wifi.ssid` 和 `wifi.password`（通过 `setwifi` 命令或热点配网后的保存），但 `loadConfig()` 被硬编码为忽略SPIFFS中的WiFi配置，完全依赖WiFiManager NVS管理凭据。因此SPIFFS中虽然有凭据，但系统不读取使用。

## 修复方案

### 修复1：停止清除已保存的WiFi凭据（NetworkManager.cpp）

```cpp
// 旧：true 表示断开并清除保存的凭据
WiFi.disconnect(true);  // :88 和 :195

// 新：false 表示断开但保留已保存的凭据，重启后可自动连接
WiFi.disconnect(false); // :88 和 :195
```

```cpp
// 旧：每次启动清除WiFiManager NVS
WiFiManager tempWM;
tempWM.resetSettings();
ESP_LOGI(TAG, "Cleared WiFiManager saved settings"); // :197-199

// 新：删除此段，不再清除NVS凭据
// （已删除）
```

### 修复2：使用autoConnect代替startConfigPortal作为入口（NetworkManager.cpp:362-367）

```cpp
// 旧：SSID为空时直接进入热点模式
if (wifiConfig.ssid.isEmpty())
{
    return startWiFiManagerHotspot(); // 直接启动AP
}

// 新：先尝试autoConnect使用NVS凭据，失败再进热点
if (wifiConfig.ssid.isEmpty())
{
    if (startWiFiManagerAutoConnect()) {
        return true; // autoConnect尝试NVS凭据连接成功
    }
    return startWiFiManagerHotspot(); // NVS无凭据或失败，启动热点
}
```

### 修复3：setwifi命令支持立即重连（main.cpp:213-221）

```cpp
// 旧：只保存到SPIFFS，提示重启后生效
app.getConfigManager()->save();
Serial.println("配置已保存，重启后生效");

// 新：保存后立即调用updateWiFiConfig触发重连，无需重启
app.getConfigManager()->save();
// 立即尝试连接新WiFi
WiFiConfig newConfig;
newConfig.ssid = ssid;
newConfig.password = password;
newConfig.autoConnect = true;
newConfig.timeout = 30000;
newConfig.maxRetries = 3;
networkMgr->updateWiFiConfig(newConfig);
```

## 涉及文件

| 文件 | 修改 | 行号 |
|------|------|------|
| `src/modules/NetworkManager.cpp` | `WiFi.disconnect(true)` → `false` | :88 |
| `src/modules/NetworkManager.cpp` | `WiFi.disconnect(true)` → `false` | :195 |
| `src/modules/NetworkManager.cpp` | 删除 `tempWM.resetSettings()` 代码块 | :197-199 |
| `src/modules/NetworkManager.cpp` | SSID为空时先 `autoConnect()` 再回退热点 | :362-367 |
| `src/main.cpp` | `setwifi` 命令增加 `updateWiFiConfig` 立即重连 | :213-221 |

## 重启后自动连接流程

```
启动 → initialize()
  → WiFi.disconnect(false)  [凭据保留]
  → loadConfig() → SSID=""   [按设计清空]
  → connect() → SSID为空
    → startWiFiManagerAutoConnect()
      → autoConnect()
        → connectWifi() 从NVS读取已保存凭据
        → WiFi.begin() 自动连接成功 ✓
        → 不启动AP热点
```

## 经验教训

1. **ESP32 WiFi凭据存储位置有三层**，容易混淆：
   - **ESP32 WiFi NVS**（`wifi` 命名空间）：由 `WiFi.begin(ssid, pass)` 通过 `WiFi.persistent(true)` 写入
   - **WiFiManager NVS**（`wifimgr` 命名空间）：由WiFiManager库内部管理，`resetSettings()` 清除
   - **SPIFFS `/config.json`**：项目自定义存储，需手动读写
   
   三者独立，需要明确每层的生命周期和清除条件

2. **WiFiManager的 `startConfigPortal()` 和 `autoConnect()` 行为不同**：`startConfigPortal()` 直接启动AP，不尝试连接已保存网络；`autoConnect()` 会先尝试连接再回退到AP。作为"默认入口"应使用 `autoConnect()`

3. **`WiFi.disconnect(bool)` 的 `true/false` 参数含义要区分**：`true` = 断开并清除NVS凭据；`false` = 仅断开连接，凭据保留

4. **每次调试WiFi问题时，应区分"首次烧录"和"重启"两种场景**：首次烧录时NVS分区是空的，需要一次配网流程；重启时应自动恢复凭据
