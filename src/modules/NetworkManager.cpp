#include "NetworkManager.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include "../utils/MemoryUtils.h"
#include <WiFiManager.h>
#include <nvs_flash.h>

static const char *TAG = "NetworkManager";

// 静态变量用于Wi-Fi事件回调
static NetworkManager *s_instance = nullptr;

// ============================================================================
// 构造函数/析构函数
// ============================================================================

NetworkManager::NetworkManager(ConfigManager *configMgr, Logger *log)
    : configManager(configMgr),
      logger(log),
      isInitialized(false),
      autoReconnect(true),
      reconnectInterval(30000), // 30秒重连间隔，避免过于频繁，减少内存压力
      lastReconnectAttempt(0),
      lastStatusCheck(0),
      maxHttpClients(3),
      sslClientManager(nullptr),
      wifiEventId(-1),
      wifiManager(nullptr),
      useWiFiManager(true)
{

    ESP_LOGI(TAG, "NetworkManager created");

    // 设置静态实例（用于事件回调）
    s_instance = this;
}

NetworkManager::~NetworkManager()
{
    deinitialize();
    s_instance = nullptr;
}

// ============================================================================
// 初始化/反初始化
// ============================================================================

bool NetworkManager::initialize()
{
    if (isInitialized)
    {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing NetworkManager...");

    // 加载配置
    if (!loadConfig())
    {
        ESP_LOGE(TAG, "Failed to load configuration");
        return false;
    }

    // 创建WiFiManager实例（智能配网）
    if (useWiFiManager)
    {
        wifiManager = new WiFiManager();
        // 配置WiFiManager参数
        wifiManager->setConnectTimeout(30);       // 连接超时30秒
        wifiManager->setConfigPortalTimeout(600); // 配置门户超时10分钟（原180秒）
        wifiManager->setDebugOutput(true);        // 启用调试输出

        // 设置AP参数
        wifiManager->setWiFiAPChannel(1); // 使用通道1（兼容性最好）
        wifiManager->setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));

        ESP_LOGI(TAG, "WiFiManager created and configured");
        ESP_LOGI(TAG, "AP will use channel 1, SSID: ESP32Test");
    }

    // ESP32-S3 Wi-Fi硬件初始化序列
    ESP_LOGI(TAG, "=== ESP32-S3 Wi-Fi Hardware Initialization Sequence ===");

    // 步骤1: 确保Wi-Fi完全关闭
    ESP_LOGI(TAG, "Step 1: Ensuring Wi-Fi is completely off...");
    WiFi.disconnect(true); // 完全断开并关闭Wi-Fi
    WiFi.mode(WIFI_OFF);
    delay(200);

    // 步骤2: 检查初始状态
    wl_status_t wifiStatus = WiFi.status();
    ESP_LOGI(TAG, "Initial Wi-Fi status: %d", wifiStatus);

    // 步骤3: 逐步初始化Wi-Fi
    ESP_LOGI(TAG, "Step 2: Starting Wi-Fi hardware initialization...");

    // 第一次尝试设置STA模式
    ESP_LOGI(TAG, "  Attempt 1: Setting Wi-Fi mode to STA...");
    Serial.println("[NetworkManager] Attempt 1: Setting Wi-Fi mode to STA...");
    if (!WiFi.mode(WIFI_STA))
    {
        ESP_LOGW(TAG, "  Wi-Fi mode setting failed on first attempt");

        // 等待并重试
        delay(300);
        ESP_LOGI(TAG, "  Attempt 2: Retrying Wi-Fi mode setting...");
        if (!WiFi.mode(WIFI_STA))
        {
            ESP_LOGW(TAG, "  Wi-Fi mode setting failed on second attempt");

            // 更激进的重试：完全重启Wi-Fi堆栈
            delay(200);
            ESP_LOGI(TAG, "  Attempt 3: Full Wi-Fi stack reset...");
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            delay(300);

// 使用esp_wifi_restore()如果可用
#ifdef ESP_ARDUINO_VERSION_MAJOR
            ESP_LOGI(TAG, "  Using ESP32 Arduino core version: %d", ESP_ARDUINO_VERSION_MAJOR);
#endif

            if (!WiFi.mode(WIFI_STA))
            {
                ESP_LOGE(TAG, "  Wi-Fi mode setting failed after all attempts");
                Serial.println("[NetworkManager] Wi-Fi mode setting failed after all attempts");
            }
            else
            {
                ESP_LOGI(TAG, "  Wi-Fi mode setting succeeded on third attempt");
                Serial.println("[NetworkManager] Wi-Fi mode setting succeeded on third attempt");
            }
        }
        else
        {
            ESP_LOGI(TAG, "  Wi-Fi mode setting succeeded on second attempt");
            Serial.println("[NetworkManager] Wi-Fi mode setting succeeded on second attempt");
        }
    }
    else
    {
        ESP_LOGI(TAG, "  Wi-Fi mode setting succeeded on first attempt");
        Serial.println("[NetworkManager] Wi-Fi mode setting succeeded on first attempt");
    }

    // 设置Wi-Fi功率（增加信号强度）
    ESP_LOGI(TAG, "Setting Wi-Fi TX power to maximum");
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    Serial.println("[NetworkManager] Wi-Fi TX power set to 19.5dBm");

    // 步骤4: 检查最终状态
    wifiStatus = WiFi.status();
    ESP_LOGI(TAG, "Final Wi-Fi status after initialization: %d", wifiStatus);
    Serial.printf("[NetworkManager] Final Wi-Fi status after initialization: %d\n", wifiStatus);

    // 步骤5: 等待Wi-Fi硬件稳定
    ESP_LOGI(TAG, "Step 3: Waiting for Wi-Fi hardware to stabilize...");
    for (int i = 0; i < 5; i++)
    {
        delay(200);
        wifiStatus = WiFi.status();
        ESP_LOGI(TAG, "  After %d ms: Wi-Fi status = %d", (i + 1) * 200, wifiStatus);
    }

    // 步骤6: 记录Wi-Fi硬件信息
    ESP_LOGI(TAG, "Step 4: Recording Wi-Fi hardware information...");

    // 获取MAC地址
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK)
    {
        ESP_LOGI(TAG, "  Wi-Fi MAC address: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    else
    {
        ESP_LOGW(TAG, "  Failed to read Wi-Fi MAC address");
    }

    // 检查Wi-Fi库信息
    ESP_LOGI(TAG, "  Wi-Fi library available");
    // WiFi.getVersion() may not be available in all Arduino WiFi implementations

    ESP_LOGI(TAG, "=== Wi-Fi Hardware Initialization Complete ===");
    ESP_LOGI(TAG, "Final status: %s",
             wifiStatus == WL_NO_SHIELD ? "NO_SHIELD (may still work)" : wifiStatus == WL_IDLE_STATUS ? "IDLE_STATUS (ready)"
                                                                     : wifiStatus == WL_DISCONNECTED  ? "DISCONNECTED (ready)"
                                                                                                      : "Other (check)");

    // 即使显示NO_SHIELD，也继续初始化（ESP32-S3有时会这样）

    // 确保Wi-Fi已禁用再重新启用
    WiFi.disconnect(true); // 清除之前保存的网络配置

    // 清除WiFiManager保存的网络配置（确保使用配置文件）
    WiFiManager tempWM;
    tempWM.resetSettings();
    ESP_LOGI(TAG, "Cleared WiFiManager saved settings");

    // 禁用Wi-Fi库的自动重连（使用我们自己的逻辑）
    WiFi.setAutoReconnect(false);
    ESP_LOGI(TAG, "Wi-Fi library auto-reconnect disabled, using our own logic");

    // 注册Wi-Fi事件回调并保存事件ID
    wifiEventId = WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info)
                               {
        if (s_instance) {
            s_instance->handleWiFiEvent(event, info);
        } });

    // 初始化HTTP客户端池
    for (int i = 0; i < maxHttpClients; i++)
    {
        httpClients.push_back(new HTTPClient());
    }

    // 初始化SSL客户端管理器（复用WiFiClientSecure减少内存分配）
    ESP_LOGI(TAG, "Initializing SSLClientManager for SSL client reuse...");
    sslClientManager = &SSLClientManager::getInstance();
    ESP_LOGI(TAG, "SSLClientManager instance obtained: %p", sslClientManager);
    sslClientManager->preallocateClients(1); // 预分配1个SSL客户端
    ESP_LOGI(TAG, "SSLClientManager initialized and preallocated 1 client");

    isInitialized = true;
    ESP_LOGI(TAG, "NetworkManager initialized successfully");

    // 记录初始化信息
    if (logger)
    {
        logger->log(Logger::Level::INFO, "NetworkManager initialized");
        logger->log(Logger::Level::INFO,
                    String("Wi-Fi SSID: ") + wifiConfig.ssid +
                        ", Auto-reconnect: " + (autoReconnect ? "enabled" : "disabled"));
    }

    // 如果配置为自动连接，开始连接
    if (wifiConfig.autoConnect)
    {
        connect();
    }

    return true;
}

bool NetworkManager::deinitialize()
{
    if (!isInitialized)
    {
        return true;
    }

    ESP_LOGI(TAG, "Deinitializing NetworkManager...");

    // 断开Wi-Fi连接
    disconnect();

    // 清理HTTP客户端
    cleanupHttpClients();

    // 清理SSL客户端映射和SSL客户端管理器
    sslClientMap.clear();
    sslClientManager = nullptr;
    ESP_LOGI(TAG, "SSL client mappings cleaned up");

    // 取消Wi-Fi事件回调
    if (wifiEventId >= 0)
    {
        WiFi.removeEvent(wifiEventId);
        wifiEventId = -1;
    }

    // 清理WiFiManager
    if (wifiManager)
    {
        delete wifiManager;
        wifiManager = nullptr;
        ESP_LOGI(TAG, "WiFiManager cleaned up");
    }

    isInitialized = false;
    ESP_LOGI(TAG, "NetworkManager deinitialized");

    if (logger)
    {
        logger->log(Logger::Level::INFO, "NetworkManager deinitialized");
    }

    return true;
}

// ============================================================================
// 配置管理
// ============================================================================

bool NetworkManager::loadConfig()
{
    // 修改：不读取配置文件，强制使用WiFiManager热点模式
    // 清空配置，让WiFiManager处理连接
    wifiConfig.ssid = "";
    wifiConfig.password = "";
    wifiConfig.autoConnect = false; // 不使用自动连接，让WiFiManager管理
    wifiConfig.timeout = 30000;     // 增加超时时间
    wifiConfig.maxRetries = 0;      // WiFiManager处理重试

    ESP_LOGI(TAG, "WiFi configuration mode: Pure hotspot (WiFiManager only)");
    ESP_LOGI(TAG, "Device will create hotspot 'XiaozhiAP' for mobile configuration");
    ESP_LOGI(TAG, "No configuration file reading - using WiFiManager memory");

    return true;
}

void NetworkManager::setConfigManager(ConfigManager *configMgr)
{
    configManager = configMgr;
    if (isInitialized && configManager)
    {
        loadConfig();
    }
}

void NetworkManager::setLogger(Logger *log)
{
    logger = log;
}

bool NetworkManager::updateWiFiConfig(const WiFiConfig &config)
{
    wifiConfig = config;

    // 如果当前已连接且SSID改变，需要重新连接
    if (isConnected() && status.ssid != wifiConfig.ssid)
    {
        ESP_LOGI(TAG, "Wi-Fi SSID changed, reconnecting...");
        disconnect();
        connect();
    }

    return true;
}

// ============================================================================
// Wi-Fi连接管理
// ============================================================================

bool NetworkManager::connect()
{
    if (!isInitialized)
    {
        ESP_LOGE(TAG, "NetworkManager not initialized");
        return false;
    }

    // 检查是否已有有效的Wi-Fi配置
    if (isConnected())
    {
        ESP_LOGW(TAG, "Already connected to Wi-Fi");
        return true;
    }

    // 确保Wi-Fi硬件完全就绪
    if (!ensureWiFiHardwareReady())
    {
        ESP_LOGW(TAG, "Wi-Fi hardware not fully ready, but attempting connection anyway");
    }

    // 检查是否有配置的SSID
    if (wifiConfig.ssid.isEmpty())
    {
        ESP_LOGW(TAG, "No WiFi SSID configured, falling back to WiFiManager hotspot mode");
        // 进入WiFiManager热点模式进行配置
        return startWiFiManagerHotspot();
    }

    ESP_LOGI(TAG, "Connecting to Wi-Fi: %s", wifiConfig.ssid.c_str());
    notifyEvent(NetworkEvent::WIFI_CONNECTING, wifiConfig.ssid);

    // 启用Wi-Fi库的自动重连，以便在连接断开时自动恢复
    WiFi.setAutoReconnect(true);
    ESP_LOGI(TAG, "Wi-Fi library auto-reconnect enabled");

    // 尝试使用配置的凭证连接
    WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.password.c_str());

    // 等待连接，带超时
    uint32_t startTime = millis();
    bool connected = false;

    ESP_LOGI(TAG, "Waiting for Wi-Fi connection (timeout: %u ms)...", wifiConfig.timeout);

    while (millis() - startTime < wifiConfig.timeout)
    {
        wl_status_t status = WiFi.status();
        if (status == WL_CONNECTED)
        {
            connected = true;
            break;
        }
        else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL)
        {
            ESP_LOGE(TAG, "Wi-Fi connection failed (status: %d)", status);
            break;
        }

        delay(100); // 短暂延迟，避免忙等待
    }

    if (connected)
    {
        ESP_LOGI(TAG, "Wi-Fi connected successfully!");
        ESP_LOGI(TAG, "SSID: %s", WiFi.SSID().c_str());
        ESP_LOGI(TAG, "IP Address: %s", WiFi.localIP().toString().c_str());
        ESP_LOGI(TAG, "RSSI: %d dBm", WiFi.RSSI());

        // 记录和设置DNS配置
        IPAddress dns1 = WiFi.dnsIP(0);
        IPAddress dns2 = WiFi.dnsIP(1);
        if (dns1 != INADDR_NONE) {
            ESP_LOGI(TAG, "DNS server 1: %s", dns1.toString().c_str());
        } else {
            ESP_LOGW(TAG, "DNS server 1 not set");
        }
        if (dns2 != INADDR_NONE) {
            ESP_LOGI(TAG, "DNS server 2: %s", dns2.toString().c_str());
        }

        // 设置备用DNS服务器（如果主DNS未设置或需要增强可靠性）
        ESP_LOGI(TAG, "Ensuring backup DNS servers are configured...");
        // 使用公共DNS：Google DNS (8.8.8.8) 和 114 DNS (114.114.114.114)
        IPAddress primaryDNS(8, 8, 8, 8);
        IPAddress secondaryDNS(114, 114, 114, 114);
        // 使用 WiFi.config() 设置 DNS，保持 IP 和网关为 DHCP
        WiFi.config(INADDR_NONE, INADDR_NONE, primaryDNS, secondaryDNS);
        ESP_LOGI(TAG, "Backup DNS configured: primary=%s, secondary=%s",
                primaryDNS.toString().c_str(), secondaryDNS.toString().c_str());

        // 验证DNS设置
        IPAddress verifyDNS1 = WiFi.dnsIP(0);
        IPAddress verifyDNS2 = WiFi.dnsIP(1);
        if (verifyDNS1 == primaryDNS || verifyDNS2 == primaryDNS ||
            verifyDNS1 == secondaryDNS || verifyDNS2 == secondaryDNS) {
            ESP_LOGI(TAG, "DNS configuration verified successfully");
        } else {
            ESP_LOGW(TAG, "DNS configuration may not have taken effect");
        }

        // 更新状态
        status.wifiConnected = true;
        status.hasIP = true;
        status.localIP = WiFi.localIP().toString();
        status.ssid = WiFi.SSID();
        status.rssi = WiFi.RSSI();
        status.connectionTime = millis();

        notifyEvent(NetworkEvent::WIFI_CONNECTED, WiFi.SSID());
        return true;
    }
    else
    {
        ESP_LOGW(TAG, "Wi-Fi connection failed, falling back to WiFiManager hotspot mode");
        notifyEvent(NetworkEvent::WIFI_CONNECTION_FAILED, "Connection timeout");

        // 尝试使用WiFiManager自动连接（使用保存的凭证）
        return startWiFiManagerAutoConnect();
    }
}

bool NetworkManager::disconnect()
{
    if (!isInitialized)
    {
        return false;
    }

    ESP_LOGI(TAG, "Disconnecting from Wi-Fi");
    WiFi.disconnect(true); // true = 关闭Wi-Fi

    status.wifiConnected = false;
    status.hasIP = false;
    status.localIP = "";
    status.connectionTime = 0;

    notifyEvent(NetworkEvent::WIFI_DISCONNECTED, "Manual disconnect");

    return true;
}

bool NetworkManager::reconnect()
{
    ESP_LOGI(TAG, "Attempting to reconnect to Wi-Fi");
    disconnect();
    delay(100); // 短暂延迟
    return connect();
}

// ============================================================================
// Wi-Fi事件处理
// ============================================================================

void NetworkManager::handleWiFiEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_START: {
        ESP_LOGI(TAG, "Wi-Fi STA started");
        Serial.println("[NetworkManager] Wi-Fi STA started");
        break;
    }

    case ARDUINO_EVENT_WIFI_STA_CONNECTED: {
        status.wifiConnected = true;
        status.ssid = WiFi.SSID();
        status.localIP = WiFi.localIP().toString();
        ESP_LOGI(TAG, "Connected to AP: %s", status.ssid.c_str());
        ESP_LOGI(TAG, "BSSID: %s, Channel: %d", WiFi.BSSIDstr().c_str(), WiFi.channel());
        ESP_LOGI(TAG, "IP address: %s", status.localIP.c_str());
        notifyEvent(NetworkEvent::WIFI_CONNECTED, status.ssid);
        break;
    }

    case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
        status.hasIP = true;
        status.localIP = WiFi.localIP().toString();
        status.connectionTime = millis();
        ESP_LOGI(TAG, "Got IP address: %s", status.localIP.c_str());

        // 记录DNS配置
        IPAddress dns1 = WiFi.dnsIP(0);
        IPAddress dns2 = WiFi.dnsIP(1);
        if (dns1 != INADDR_NONE) {
            ESP_LOGI(TAG, "DNS server 1: %s", dns1.toString().c_str());
        } else {
            ESP_LOGW(TAG, "DNS server 1 not set");
        }
        if (dns2 != INADDR_NONE) {
            ESP_LOGI(TAG, "DNS server 2: %s", dns2.toString().c_str());
        }

        // 设置备用DNS服务器（如果主DNS未设置）
        if (dns1 == INADDR_NONE) {
            ESP_LOGI(TAG, "Setting backup DNS servers...");
            // 使用公共DNS：Google DNS (8.8.8.8) 和 114 DNS (114.114.114.114)
            IPAddress primaryDNS(8, 8, 8, 8);
            IPAddress secondaryDNS(114, 114, 114, 114);
            // 使用 WiFi.config() 设置 DNS，保持 IP 和网关为 DHCP
            WiFi.config(INADDR_NONE, INADDR_NONE, primaryDNS, secondaryDNS);
            ESP_LOGI(TAG, "Backup DNS set: primary=%s, secondary=%s",
                    primaryDNS.toString().c_str(), secondaryDNS.toString().c_str());

            // 验证DNS设置
            IPAddress verifyDNS1 = WiFi.dnsIP(0);
            IPAddress verifyDNS2 = WiFi.dnsIP(1);
            if (verifyDNS1 == primaryDNS || verifyDNS2 == primaryDNS ||
                verifyDNS1 == secondaryDNS || verifyDNS2 == secondaryDNS) {
                ESP_LOGI(TAG, "DNS configuration verified successfully");
            } else {
                ESP_LOGW(TAG, "DNS configuration may not have taken effect");
            }
        }

        notifyEvent(NetworkEvent::WIFI_GOT_IP, status.localIP);
        break;
    }

    case ARDUINO_EVENT_WIFI_STA_LOST_IP: {
        status.hasIP = false;
        status.localIP = "";
        ESP_LOGW(TAG, "Lost IP address");
        notifyEvent(NetworkEvent::WIFI_LOST_IP, "IP address lost");
        break;
    }

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    {
        status.wifiConnected = false;
        status.hasIP = false;
        status.lastDisconnectTime = millis();
        status.disconnectCount++;

        // 获取断开原因
        uint8_t reason = info.wifi_sta_disconnected.reason;
        String reasonStr = "Unknown";

        // 常见的Wi-Fi断开原因
        switch (reason)
        {
        case 1:
            reasonStr = "Unspecified";
            break;
        case 2:
            reasonStr = "Auth expired";
            break;
        case 3:
            reasonStr = "Deauth leaving";
            break;
        case 4:
            reasonStr = "Inactivity";
            break;
        case 5:
            reasonStr = "No beacon";
            break;
        case 6:
            reasonStr = "Too many failures";
            break;
        case 7:
            reasonStr = "Handshake timeout";
            break;
        case 8:
            reasonStr = "Connection failed";
            break;
        case 200:
            reasonStr = "Beacon timeout";
            break;
        case 201:
            reasonStr = "No AP found";
            break;
        case 202:
            reasonStr = "Auth fail";
            break;
        case 203:
            reasonStr = "Assoc fail";
            break;
        case 204:
            reasonStr = "Handshake fail";
            break;
        case 205:
            reasonStr = "Network not found";
            break;
        case 206:
            reasonStr = "Password error";
            break;
        case 207:
            reasonStr = "Four-way handshake timeout";
            break;
        case 208:
            reasonStr = "Group key update timeout";
            break;
        default:
            reasonStr = "Unknown (" + String(reason) + ")";
            break;
        }

        // 获取RSSI（信号强度）
        int32_t rssi = WiFi.RSSI();

        // 获取BSSID
        String bssid = WiFi.BSSIDstr();

        ESP_LOGW(TAG, "Disconnected from Wi-Fi - Reason: %d (%s), RSSI: %d dBm, BSSID: %s",
                 reason, reasonStr.c_str(), rssi, bssid.c_str());

        // 同时记录到自定义日志系统
        if (logger)
        {
            String detailedMsg = String("Wi-Fi disconnected - Reason: ") + String(reason) +
                                 " (" + reasonStr + "), RSSI: " + String(rssi) +
                                 " dBm, BSSID: " + bssid;
            logger->log(Logger::Level::WARN, detailedMsg);
        }

        notifyEvent(NetworkEvent::WIFI_DISCONNECTED, "Connection lost - " + reasonStr);

        // 如果启用自动重连，计划重连
        if (autoReconnect)
        {
            lastReconnectAttempt = millis();
        }
        break;
    }

    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE: {
        ESP_LOGI(TAG, "Auth mode changed");
        break;
    }

    default: {
        // 打印未知事件以便调试
        ESP_LOGI(TAG, "Unhandled Wi-Fi event: %d", event);
        break;
    }
    }

    // 更新状态
    updateStatus();
}

// ============================================================================
// 网络扫描
// ============================================================================

bool NetworkManager::scanNetworks(std::vector<String> &networks, bool async)
{
    if (!isInitialized)
    {
        return false;
    }

    ESP_LOGI(TAG, "Scanning for Wi-Fi networks...");

    int n = WiFi.scanNetworks(async);
    if (n == 0)
    {
        ESP_LOGW(TAG, "No networks found");
        return false;
    }

    networks.clear();
    for (int i = 0; i < n; i++)
    {
        String network = WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)";
        networks.push_back(network);
    }

    ESP_LOGI(TAG, "Found %d networks", n);
    notifyEvent(NetworkEvent::WIFI_SCAN_COMPLETED, String("Found ") + String(n) + " networks");

    return true;
}

int NetworkManager::getNetworkCount() const
{
    return WiFi.scanComplete();
}

String NetworkManager::getScannedNetwork(int index) const
{
    if (index < 0 || index >= getNetworkCount())
    {
        return "";
    }
    return WiFi.SSID(index) + " (" + String(WiFi.RSSI(index)) + " dBm)";
}

// ============================================================================
// HTTP请求
// ============================================================================

HttpResponse NetworkManager::sendRequest(const HttpRequestConfig &config)
{
    HttpResponse finalResponse;
    // PSRAM内存监控
    if (MemoryUtils::isPSRAMAvailable()) {
        size_t freePSRAM = MemoryUtils::getFreePSRAM();
        size_t largestPSRAM = MemoryUtils::getLargestFreePSRAMBlock();
        ESP_LOGI(TAG, "PSRAM available: free=%u bytes, largest block=%u bytes", freePSRAM, largestPSRAM);

    }
    int maxAttempts = config.maxRetries + 1; // 总尝试次数 = 重试次数 + 1

    if (!isConnected())
    {
        finalResponse.errorMessage = "Not connected to Wi-Fi";
        notifyEvent(NetworkEvent::HTTP_REQUEST_FAILED, "No Wi-Fi connection");
        return finalResponse;
    }

    // HTTPS连接前的内存应急检查
    if (config.url.startsWith("https://") && sslClientManager) {
        size_t freeInternal = esp_get_free_internal_heap_size();
        ESP_LOGI(TAG, "HTTPS connection memory check: internal heap = %u bytes", freeInternal);

        // 早期检查：如果内存严重不足，直接拒绝请求，避免浪费资源
        if (freeInternal < 35000) {  // 内部堆低于35KB时直接拒绝
            ESP_LOGE(TAG, "ERROR: Internal heap critically low (%u bytes < 35KB), aborting HTTPS request immediately", freeInternal);
            HttpResponse errorResponse;
            errorResponse.statusCode = -1001;  // 特殊错误码，表示内存不足
            errorResponse.errorMessage = "Insufficient memory for SSL connection";
            return errorResponse;
        }

        if (freeInternal < 45000) {  // 内部堆低于45KB时触发应急清理
            ESP_LOGW(TAG, "Low internal heap detected (%u bytes < 45KB), cleaning SSL clients", freeInternal);

            // 强制清理所有SSL客户端（包括活跃的），这是最后手段
            if (freeInternal < 40000) {
                ESP_LOGE(TAG, "CRITICAL: Internal heap very low (%u bytes), forcing cleanup of ALL SSL clients", freeInternal);
                sslClientManager->cleanupAll(true);  // 强制清理所有，包括活跃连接
            } else {
                // 只清理空闲客户端
                sslClientManager->cleanupAll(false);
            }

            freeInternal = esp_get_free_internal_heap_size();
            ESP_LOGI(TAG, "After cleanup, internal heap: %u bytes", freeInternal);

            // 清理后再次检查
            if (freeInternal < 35000) {
                ESP_LOGE(TAG, "ERROR: Internal heap still critically low after cleanup (%u bytes < 35KB), aborting HTTPS request", freeInternal);
                HttpResponse errorResponse;
                errorResponse.statusCode = -1001;
                errorResponse.errorMessage = "Insufficient memory for SSL connection after cleanup";
                return errorResponse;
            }
        }
    }

    // 定义可重试的错误码
    // 网络错误：负错误码（HTTPClient错误）
    // 服务器错误：5xx
    // 限流错误：429
    // 临时错误：408（请求超时）
    auto shouldRetry = [](int httpCode, const String &errorMessage) -> bool
    {
        // 网络错误（负错误码）
        if (httpCode < 0)
        {
            // 检查是否为临时网络错误
            if (httpCode == HTTPC_ERROR_CONNECTION_REFUSED ||
                httpCode == HTTPC_ERROR_SEND_HEADER_FAILED ||
                httpCode == HTTPC_ERROR_SEND_PAYLOAD_FAILED ||
                httpCode == HTTPC_ERROR_NOT_CONNECTED ||
                httpCode == HTTPC_ERROR_READ_TIMEOUT)
            {
                return true;
            }
            return false;
        }

        // HTTP状态码错误
        if (httpCode == 408 || // 请求超时
            httpCode == 429 || // 太多请求
            httpCode == 500 || // 内部服务器错误
            httpCode == 502 || // 错误网关
            httpCode == 503 || // 服务不可用
            httpCode == 504)
        { // 网关超时
            return true;
        }

        return false;
    };

    // SSL客户端（用于HTTPS连接的复用）
    WiFiClientSecure* sslClient = nullptr;

    // 重试循环
    for (int attempt = 0; attempt < maxAttempts; attempt++)
    {
        HttpResponse response;

        HTTPClient *http = getHttpClient();
        if (!http)
        {
            response.errorMessage = "No HTTP client available";
            finalResponse = response;
            break;
        }

        ESP_LOGI(TAG, "Sending HTTP %s request to: %s (attempt %d/%d)",
                 methodToString(config.method).c_str(), config.url.c_str(),
                 attempt + 1, maxAttempts);

        if (attempt == 0)
        {
            notifyEvent(NetworkEvent::HTTP_REQUEST_START, config.url);
        }
        else
        {
            notifyEvent(NetworkEvent::HTTP_REQUEST_START,
                        config.url + " (retry " + String(attempt) + ")");
        }

        uint32_t startTime = millis();

        // 配置HTTP客户端
        http->setTimeout(config.timeout);
        http->setReuse(config.followRedirects);
        http->setFollowRedirects(config.followRedirects ? HTTPC_FORCE_FOLLOW_REDIRECTS : HTTPC_DISABLE_FOLLOW_REDIRECTS);

        // SSL内存监控：记录连接前的内存状态（包括SPIRAM）
        ESP_LOGI(TAG, "SSL memory before connection - Total heap: %u, Internal heap: %u, SPIRAM: %u, Min free: %u",
                 esp_get_free_heap_size(),
                 esp_get_free_internal_heap_size(),
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                 esp_get_minimum_free_heap_size());

        // 开始连接（使用SSL客户端复用减少内存分配）
        bool beginResult = false;
        WiFiClientSecure* sslClient = nullptr;

        if (config.url.startsWith("https://")) {
            ESP_LOGI(TAG, "HTTPS URL detected, attempting SSL client reuse for: %s", config.url.c_str());
            // 使用SSL客户端复用
            sslClient = getSSLClientForUrl(config.url);
            if (sslClient) {
                ESP_LOGI(TAG, "SSL client reuse SUCCESS, using client: %p", sslClient);
                beginResult = beginHttpWithSSLClient(http, config.url, sslClient);
            } else {
                ESP_LOGE(TAG, "ERROR: Failed to get SSL client for %s, falling back to standard begin", config.url.c_str());
                beginResult = http->begin(config.url);
            }
        } else {
            // HTTP连接，使用标准begin
            beginResult = http->begin(config.url);
        }

        if (!beginResult)
        {
            response.errorMessage = "Failed to begin HTTP connection";
            response.statusCode = -1;

            ESP_LOGW(TAG, "HTTP begin failed on attempt %d: %s",
                     attempt + 1, response.errorMessage.c_str());

            // 释放SSL客户端（如果使用了）
            if (sslClient) {
                sslClientManager->releaseClient(sslClient, false);
                sslClientMap.erase(http);
            }

            releaseHttpClient(http);

            // 检查是否应该重试
            if (attempt < maxAttempts - 1 && shouldRetry(-1, response.errorMessage))
            {
                // 指数退避延迟
                uint32_t delayMs = (1 << attempt) * 100; // 100, 200, 400, 800ms...
                ESP_LOGI(TAG, "Retrying after %u ms...", delayMs);
                delay(delayMs);
                continue;
            }

            finalResponse = response;
            notifyEvent(NetworkEvent::HTTP_REQUEST_FAILED, "Begin failed: " + config.url);
            break;
        }

        // 添加请求头
        for (const auto &header : config.headers)
        {
            http->addHeader(header.first, header.second);
        }

        // 发送请求
        int httpCode = -1;
        switch (config.method)
        {
        case HttpMethod::GET:
            httpCode = http->GET();
            break;
        case HttpMethod::POST:
            httpCode = http->POST(config.body);
            break;
        case HttpMethod::PUT:
            httpCode = http->PUT(config.body);
            break;
        case HttpMethod::DELETE:
            // http->DELETE() may not be available in some HTTPClient versions
            // Use sendRequest with "DELETE" method instead
            httpCode = http->sendRequest("DELETE");
            break;
        case HttpMethod::PATCH:
            httpCode = http->PATCH(config.body);
            break;
        }

        uint32_t endTime = millis();
        response.responseTime = endTime - startTime;
        response.statusCode = httpCode;

        // 处理响应
        if (httpCode > 0)
        {
            response.body = http->getString();

            // 获取响应头（简化处理）
            // String headers = http->getHeaders();

            ESP_LOGI(TAG, "HTTP request completed: %d, Response time: %u ms (attempt %d)",
                     httpCode, response.responseTime, attempt + 1);

            if (httpCode >= 200 && httpCode < 300)
            {
                notifyEvent(NetworkEvent::HTTP_REQUEST_SUCCESS,
                            String("Status: ") + String(httpCode) + ", Time: " + String(response.responseTime) + "ms");
                finalResponse = response;
                http->end();
                // 释放SSL客户端（如果使用了）
                releaseSSLClientForHttpClient(http);
                releaseHttpClient(http);
                break; // 成功，退出重试循环
            }
            else
            {
                // HTTP错误但非2xx
                response.errorMessage = "HTTP error " + String(httpCode);
                notifyEvent(NetworkEvent::HTTP_REQUEST_FAILED,
                            String("Status: ") + String(httpCode));

                ESP_LOGW(TAG, "HTTP error %d on attempt %d", httpCode, attempt + 1);

                // 检查是否应该重试
                if (attempt < maxAttempts - 1 && shouldRetry(httpCode, response.errorMessage))
                {
                    http->end();
                    releaseSSLClientForHttpClient(http);
                    releaseHttpClient(http);

                    // 指数退避延迟
                    uint32_t delayMs = (1 << attempt) * 100;
                    ESP_LOGI(TAG, "Retrying after %u ms...", delayMs);
                    delay(delayMs);
                    continue;
                }

                finalResponse = response;
                http->end();
                releaseSSLClientForHttpClient(http);
                releaseHttpClient(http);
                break;
            }
        }
        else
        {
            // 网络错误（负错误码）
            response.errorMessage = http->errorToString(httpCode);
            ESP_LOGE(TAG, "HTTP request failed on attempt %d: %s",
                     attempt + 1, response.errorMessage.c_str());
            // SSL失败后的内存状态记录（包括SPIRAM）
            ESP_LOGI(TAG, "SSL memory after connection failure - Total heap: %u, Internal heap: %u, SPIRAM: %u, Min free: %u",
                     esp_get_free_heap_size(),
                     esp_get_free_internal_heap_size(),
                     heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                     esp_get_minimum_free_heap_size());
            notifyEvent(NetworkEvent::HTTP_REQUEST_FAILED, response.errorMessage);

            // 检查是否应该重试
            if (attempt < maxAttempts - 1 && shouldRetry(httpCode, response.errorMessage))
            {
                http->end();
                releaseSSLClientForHttpClient(http);
                releaseHttpClient(http);

                // 指数退避延迟
                uint32_t delayMs = (1 << attempt) * 100;
                ESP_LOGI(TAG, "Retrying after %u ms...", delayMs);
                delay(delayMs);
                continue;
            }

            finalResponse = response;
            http->end();
            releaseSSLClientForHttpClient(http);
            releaseHttpClient(http);
            break;
        }
    }

    // 记录最终结果
    if (finalResponse.statusCode >= 200 && finalResponse.statusCode < 300)
    {
        ESP_LOGI(TAG, "Request succeeded after retries, final status: %d", finalResponse.statusCode);
    }
    else
    {
        ESP_LOGE(TAG, "Request failed after all retries, final error: %s", finalResponse.errorMessage.c_str());
    }

    return finalResponse;
}

HttpResponse NetworkManager::get(const String &url, const std::map<String, String> &headers)
{
    HttpRequestConfig config;
    config.url = url;
    config.method = HttpMethod::GET;
    config.headers = headers;
    return sendRequest(config);
}

HttpResponse NetworkManager::post(const String &url, const String &body, const std::map<String, String> &headers)
{
    HttpRequestConfig config;
    config.url = url;
    config.method = HttpMethod::POST;
    config.body = body;
    config.headers = headers;
    return sendRequest(config);
}

HttpResponse NetworkManager::postJson(const String &url, const String &json, const std::map<String, String> &headers)
{
    std::map<String, String> jsonHeaders = headers;
    jsonHeaders["Content-Type"] = "application/json";

    return post(url, json, jsonHeaders);
}

// ============================================================================
// 文件上传（简化实现）
// ============================================================================

bool NetworkManager::uploadFile(const String &url, const String &filePath, const String &fieldName,
                                const std::map<String, String> &formFields)
{
    // 注意：ESP32的HTTPClient对多部分表单上传支持有限
    // 这里提供简化实现，实际项目中可能需要使用更高级的HTTP库

    ESP_LOGW(TAG, "File upload not fully implemented (simplified)");

    // 读取文件内容（假设文件在SPIFFS中）
    // 这里简化处理，实际需要实现文件读取和多部分编码

    notifyEvent(NetworkEvent::HTTP_REQUEST_START, "File upload: " + url);

    // 实际实现留空，需要根据具体需求实现
    return false;
}

// ============================================================================
// 流式传输（框架实现）
// ============================================================================

bool NetworkManager::startStream(const String &url, const std::map<String, String> &headers)
{
    // 流式传输需要保持长连接
    // 这里提供框架，实际实现需要根据具体协议（如WebSocket、HTTP流）实现
    ESP_LOGW(TAG, "Streaming not fully implemented");
    return false;
}

bool NetworkManager::writeStreamChunk(const uint8_t *data, size_t length)
{
    return false;
}

bool NetworkManager::readStreamChunk(uint8_t *buffer, size_t maxLength, size_t &bytesRead)
{
    bytesRead = 0;
    return false;
}

bool NetworkManager::endStream()
{
    return true;
}

// ============================================================================
// 事件管理
// ============================================================================

void NetworkManager::addEventListener(NetworkEventCallback callback, void *userData)
{
    if (!callback)
        return;

    eventCallbacks.push_back(callback);
    callbackUserData.push_back(userData);
}

void NetworkManager::removeEventListener(NetworkEventCallback callback)
{
    for (size_t i = 0; i < eventCallbacks.size(); i++)
    {
        if (eventCallbacks[i] == callback)
        {
            eventCallbacks.erase(eventCallbacks.begin() + i);
            callbackUserData.erase(callbackUserData.begin() + i);
            break;
        }
    }
}

void NetworkManager::clearEventListeners()
{
    eventCallbacks.clear();
    callbackUserData.clear();
}

void NetworkManager::notifyEvent(NetworkEvent event, const String &details)
{
    // 记录日志
    if (logger)
    {
        String message = "Network event: " + eventToString(event);
        if (!details.isEmpty())
        {
            message += " - " + details;
        }

        Logger::Level level = Logger::Level::INFO;
        if (event == NetworkEvent::WIFI_CONNECTION_FAILED ||
            event == NetworkEvent::HTTP_REQUEST_FAILED ||
            event == NetworkEvent::NETWORK_ERROR)
        {
            level = Logger::Level::ERROR;
        }
        else if (event == NetworkEvent::WIFI_DISCONNECTED ||
                 event == NetworkEvent::WIFI_LOST_IP)
        {
            level = Logger::Level::WARN;
        }

        logger->log(level, message);
    }

    // 调用回调函数
    for (size_t i = 0; i < eventCallbacks.size(); i++)
    {
        eventCallbacks[i](event, details, callbackUserData[i]);
    }
}

// ============================================================================
// 状态监控和更新
// ============================================================================

void NetworkManager::update()
{
    if (!isInitialized)
    {
        return;
    }

    uint32_t currentTime = millis();

    // 定期更新状态
    if (currentTime - lastStatusCheck >= 1000)
    { // 每秒更新一次
        updateStatus();
        lastStatusCheck = currentTime;
    }

    // 处理自动重连
    if (autoReconnect && !isConnected() &&
        currentTime - lastReconnectAttempt >= reconnectInterval)
    {

        ESP_LOGI(TAG, "Attempting auto-reconnect...");
        connect();
        lastReconnectAttempt = currentTime;
    }
}

void NetworkManager::updateStatus()
{
    // 更新RSSI
    if (status.wifiConnected)
    {
        status.rssi = WiFi.RSSI();
    }

    // 更新其他状态信息
    status.wifiConnected = (WiFi.status() == WL_CONNECTED);
    status.hasIP = (WiFi.localIP().toString() != "0.0.0.0");
    if (status.hasIP)
    {
        status.localIP = WiFi.localIP().toString();
    }
    status.ssid = WiFi.SSID();
}

void NetworkManager::printStatus() const
{
    ESP_LOGI(TAG, "=== Network Status ===");
    ESP_LOGI(TAG, "Wi-Fi Connected: %s", status.wifiConnected ? "Yes" : "No");
    ESP_LOGI(TAG, "Has IP: %s", status.hasIP ? "Yes" : "No");
    if (status.hasIP)
    {
        ESP_LOGI(TAG, "Local IP: %s", status.localIP.c_str());
    }
    ESP_LOGI(TAG, "SSID: %s", status.ssid.c_str());
    ESP_LOGI(TAG, "RSSI: %d dBm", status.rssi);
    ESP_LOGI(TAG, "Connection Time: %u ms", status.connectionTime);
    ESP_LOGI(TAG, "Disconnect Count: %u", status.disconnectCount);
    ESP_LOGI(TAG, "Auto-reconnect: %s", autoReconnect ? "Enabled" : "Disabled");
    ESP_LOGI(TAG, "=========================");
}

// ============================================================================
// HTTP客户端池管理
// ============================================================================

HTTPClient *NetworkManager::getHttpClient()
{
    // 在分配HTTP客户端前检查内存状态
    size_t freeInternal = esp_get_free_internal_heap_size();
    ESP_LOGI(TAG, "getHttpClient: free internal heap = %u bytes", freeInternal);

    if (freeInternal < 30000) {  // 内部堆低于30KB时触发紧急清理
        ESP_LOGW(TAG, "CRITICAL: Very low internal heap (%u bytes < 30KB) before HTTP client allocation", freeInternal);

        // 尝试紧急清理SSL客户端（如果SSL客户端管理器已初始化）
        if (sslClientManager) {
            ESP_LOGI(TAG, "Attempting emergency SSL client cleanup...");
            size_t before = esp_get_free_internal_heap_size();
            sslClientManager->cleanupAll(true);  // 强制清理所有SSL客户端
            size_t after = esp_get_free_internal_heap_size();
            ESP_LOGI(TAG, "Emergency cleanup: before=%u, after=%u, freed=%d bytes",
                    before, after, after - before);
        }

        // 清理HTTP客户端池
        if (!httpClients.empty()) {
            ESP_LOGI(TAG, "Cleaning HTTP client pool (%d clients)", httpClients.size());
            cleanupHttpClients();
        }

        freeInternal = esp_get_free_internal_heap_size();
        ESP_LOGI(TAG, "After emergency cleanup: free internal heap = %u bytes", freeInternal);

        if (freeInternal < 20000) {
            ESP_LOGE(TAG, "ERROR: Insufficient memory for HTTP client allocation (%u bytes < 20KB)", freeInternal);
            return nullptr;
        }
    }

    if (httpClients.empty())
    {
        // 如果池为空，创建新的客户端（不超过最大限制）
        if (httpClients.size() < maxHttpClients)
        {
            HTTPClient *client = new (std::nothrow) HTTPClient();
            if (!client) {
                ESP_LOGE(TAG, "ERROR: Failed to allocate HTTPClient (out of memory)");
                return nullptr;
            }
            httpClients.push_back(client);
            ESP_LOGI(TAG, "Allocated new HTTPClient %p (pool size: %d)", client, httpClients.size());
            return client;
        }
        return nullptr;
    }

    HTTPClient *client = httpClients.back();
    httpClients.pop_back();

    // 简单验证客户端指针（虽然有限，但可以检查是否为null）
    if (!client) {
        ESP_LOGE(TAG, "ERROR: Retrieved null HTTPClient from pool");
        // 从池中移除损坏的指针并尝试获取新的
        httpClients.clear();  // 清空池，避免更多损坏
        return getHttpClient();  // 递归调用（但有限制）
    }

    ESP_LOGI(TAG, "Reusing HTTPClient %p from pool (remaining: %d)", client, httpClients.size());
    return client;
}

void NetworkManager::releaseHttpClient(HTTPClient *client)
{
    if (client && httpClients.size() < maxHttpClients)
    {
        httpClients.push_back(client);
    }
    else if (client)
    {
        delete client; // 超过最大限制，删除客户端
    }
}

void NetworkManager::cleanupHttpClients()
{
    for (HTTPClient *client : httpClients)
    {
        delete client;
    }
    httpClients.clear();
}

void NetworkManager::cleanupSSLClientMappings()
{
    ESP_LOGI(TAG, "Cleaning up SSL client mappings (%d mappings)", sslClientMap.size());

    if (!sslClientManager) {
        ESP_LOGW(TAG, "SSLClientManager not initialized, cannot release SSL clients");
        sslClientMap.clear();
        return;
    }

    // 释放所有映射的SSL客户端
    for (auto& pair : sslClientMap) {
        WiFiClientSecure* sslClient = pair.second;
        ESP_LOGI(TAG, "Releasing SSL client %p for HTTP client %p", sslClient, pair.first);
        sslClientManager->releaseClient(sslClient, false);  // 不保持连接，完全释放
    }

    sslClientMap.clear();
    ESP_LOGI(TAG, "SSL client mappings cleaned up");
}

// ============================================================================
// 连接参数设置
// ============================================================================

void NetworkManager::setAutoReconnect(bool enable)
{
    autoReconnect = enable;
    WiFi.setAutoReconnect(enable);
    ESP_LOGI(TAG, "Auto-reconnect %s", enable ? "enabled" : "disabled");
}

void NetworkManager::setReconnectInterval(uint32_t interval)
{
    reconnectInterval = interval;
    ESP_LOGI(TAG, "Reconnect interval set to %u ms", interval);
}

// ============================================================================
// 工具方法
// ============================================================================

String NetworkManager::methodToString(HttpMethod method)
{
    switch (method)
    {
    case HttpMethod::GET:
        return "GET";
    case HttpMethod::POST:
        return "POST";
    case HttpMethod::PUT:
        return "PUT";
    case HttpMethod::DELETE:
        return "DELETE";
    case HttpMethod::PATCH:
        return "PATCH";
    default:
        return "UNKNOWN";
    }
}

HttpMethod NetworkManager::stringToMethod(const String &methodStr)
{
    String upper = methodStr;
    upper.toUpperCase();

    if (upper == "GET")
        return HttpMethod::GET;
    if (upper == "POST")
        return HttpMethod::POST;
    if (upper == "PUT")
        return HttpMethod::PUT;
    if (upper == "DELETE")
        return HttpMethod::DELETE;
    if (upper == "PATCH")
        return HttpMethod::PATCH;

    return HttpMethod::GET; // 默认
}

String NetworkManager::eventToString(NetworkEvent event)
{
    switch (event)
    {
    case NetworkEvent::WIFI_CONNECTING:
        return "WIFI_CONNECTING";
    case NetworkEvent::WIFI_CONNECTED:
        return "WIFI_CONNECTED";
    case NetworkEvent::WIFI_DISCONNECTED:
        return "WIFI_DISCONNECTED";
    case NetworkEvent::WIFI_GOT_IP:
        return "WIFI_GOT_IP";
    case NetworkEvent::WIFI_LOST_IP:
        return "WIFI_LOST_IP";
    case NetworkEvent::WIFI_CONNECTION_FAILED:
        return "WIFI_CONNECTION_FAILED";
    case NetworkEvent::WIFI_SCAN_COMPLETED:
        return "WIFI_SCAN_COMPLETED";
    case NetworkEvent::HTTP_REQUEST_START:
        return "HTTP_REQUEST_START";
    case NetworkEvent::HTTP_REQUEST_SUCCESS:
        return "HTTP_REQUEST_SUCCESS";
    case NetworkEvent::HTTP_REQUEST_FAILED:
        return "HTTP_REQUEST_FAILED";
    case NetworkEvent::NETWORK_ERROR:
        return "NETWORK_ERROR";
    default:
        return "UNKNOWN_EVENT";
    }
}

// ============================================================================
// 诊断工具
// ============================================================================

bool NetworkManager::testConnection(const String &testUrl)
{
    ESP_LOGI(TAG, "Testing network connection...");

    if (!isConnected())
    {
        ESP_LOGE(TAG, "Not connected to Wi-Fi");
        return false;
    }

    HttpResponse response = get(testUrl);
    bool success = (response.statusCode >= 200 && response.statusCode < 300);

    if (success)
    {
        ESP_LOGI(TAG, "Connection test passed");
    }
    else
    {
        ESP_LOGE(TAG, "Connection test failed: %d", response.statusCode);
    }

    return success;
}

bool NetworkManager::ping(const String &host, uint32_t timeout)
{
    // 简化实现：使用HTTP请求模拟ping
    // 实际可以使用ICMP ping，但需要额外的库
    ESP_LOGW(TAG, "Ping not fully implemented (using HTTP test)");
    return testConnection("http://" + host);
}

bool NetworkManager::checkWiFiHardware()
{
    ESP_LOGI(TAG, "=== Wi-Fi Hardware Diagnostic ===");

    // 检查Wi-Fi状态
    wl_status_t status = WiFi.status();
    ESP_LOGI(TAG, "Wi-Fi status code: %d", status);

    String statusStr;
    switch (status)
    {
    case WL_NO_SHIELD:
        statusStr = "NO_SHIELD - Wi-Fi hardware not found";
        break;
    case WL_IDLE_STATUS:
        statusStr = "IDLE_STATUS - Wi-Fi idle";
        break;
    case WL_NO_SSID_AVAIL:
        statusStr = "NO_SSID_AVAIL - No SSID available";
        break;
    case WL_SCAN_COMPLETED:
        statusStr = "SCAN_COMPLETED - Scan completed";
        break;
    case WL_CONNECTED:
        statusStr = "CONNECTED - Wi-Fi connected";
        break;
    case WL_CONNECT_FAILED:
        statusStr = "CONNECT_FAILED - Connection failed";
        break;
    case WL_CONNECTION_LOST:
        statusStr = "CONNECTION_LOST - Connection lost";
        break;
    case WL_DISCONNECTED:
        statusStr = "DISCONNECTED - Wi-Fi disconnected";
        break;
    default:
        statusStr = "UNKNOWN_STATUS";
        break;
    }
    ESP_LOGI(TAG, "Wi-Fi status: %s", statusStr.c_str());

    // 检查MAC地址
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "Wi-Fi MAC address: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // 检查ESP芯片信息
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "Chip model: %s",
             (chip_info.model == CHIP_ESP32S3) ? "ESP32-S3" : (chip_info.model == CHIP_ESP32) ? "ESP32"
                                                          : (chip_info.model == CHIP_ESP32C3) ? "ESP32-C3"
                                                          : (chip_info.model == CHIP_ESP32S2) ? "ESP32-S2"
                                                                                              : "Unknown");

    ESP_LOGI(TAG, "Cores: %d, Revision: %d", chip_info.cores, chip_info.revision);

    // 检查可用堆内存
    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());

    // 尝试简单的Wi-Fi操作
    ESP_LOGI(TAG, "Testing basic Wi-Fi operations...");

    // 尝试设置STA模式
    bool modeResult = WiFi.mode(WIFI_STA);
    ESP_LOGI(TAG, "WiFi.mode(WIFI_STA) result: %s", modeResult ? "Success" : "Failed");

    // 检查Wi-Fi通道和功率设置是否可用
    int8_t power = WiFi.getTxPower();
    ESP_LOGI(TAG, "Wi-Fi TX power: %d dBm", power);

    // 额外的ESP32-S3特定检查
    ESP_LOGI(TAG, "=== ESP32-S3 Specific Checks ===");

    // 检查Wi-Fi初始化状态
    if (status == WL_NO_SHIELD)
    {
        ESP_LOGW(TAG, "Wi-Fi hardware reports NO_SHIELD, attempting recovery...");

        // 尝试强制Wi-Fi重新初始化
        WiFi.disconnect(true); // 完全断开
        delay(100);

        // 尝试重新设置Wi-Fi模式
        WiFi.mode(WIFI_OFF);
        delay(100);

        bool modeSet = WiFi.mode(WIFI_STA);
        ESP_LOGI(TAG, "Wi-Fi re-initialization: mode(WIFI_STA) = %s", modeSet ? "Success" : "Failed");

        // 再次检查状态
        delay(200);
        status = WiFi.status();
        ESP_LOGI(TAG, "Wi-Fi status after re-initialization: %d", status);

        if (status != WL_NO_SHIELD)
        {
            ESP_LOGI(TAG, "Wi-Fi hardware recovery successful!");
        }
    }

    // 检查Wi-Fi扫描功能（基本功能测试）
    ESP_LOGI(TAG, "Testing Wi-Fi scan capability...");
    int scanResult = WiFi.scanNetworks(true); // 异步扫描
    ESP_LOGI(TAG, "WiFi.scanNetworks() result: %d", scanResult);

    // 总结
    bool hardwareOk = (status != WL_NO_SHIELD);
    if (hardwareOk)
    {
        ESP_LOGI(TAG, "=== Wi-Fi Hardware: OK ===");
        ESP_LOGI(TAG, "Note: Even if status shows NO_SHIELD initially,");
        ESP_LOGI(TAG, "Wi-Fi hardware may become available after proper initialization.");
    }
    else
    {
        ESP_LOGW(TAG, "=== Wi-Fi Hardware: INITIALIZATION NEEDED ===");
        ESP_LOGW(TAG, "Current status: NO_SHIELD (255)");
        ESP_LOGW(TAG, "This may be normal during early boot phase.");
        ESP_LOGW(TAG, "Wi-Fi will attempt initialization during network setup.");

        // 不返回false，让初始化继续尝试
    }

    return true; // 总是返回true，让初始化继续
}

bool NetworkManager::ensureWiFiHardwareReady()
{
    ESP_LOGI(TAG, "=== Ensuring Wi-Fi Hardware Readiness ===");

    // 检查当前Wi-Fi状态
    wl_status_t status = WiFi.status();
    ESP_LOGI(TAG, "Current Wi-Fi status: %d", status);

    // 如果状态是NO_SHIELD，尝试修复
    if (status == WL_NO_SHIELD)
    {
        ESP_LOGW(TAG, "Wi-Fi reports NO_SHIELD, attempting hardware recovery...");

        // 尝试完整的Wi-Fi重启序列
        ESP_LOGI(TAG, "Step 1: Full Wi-Fi stack reset...");
        WiFi.disconnect(true); // 完全断开
        delay(100);
        WiFi.mode(WIFI_OFF);
        delay(200);

        // 尝试重新初始化
        ESP_LOGI(TAG, "Step 2: Re-initializing Wi-Fi...");
        for (int attempt = 1; attempt <= 3; attempt++)
        {
            ESP_LOGI(TAG, "  Attempt %d: Setting mode to WIFI_STA...", attempt);
            if (WiFi.mode(WIFI_STA))
            {
                ESP_LOGI(TAG, "  Success on attempt %d", attempt);
                break;
            }
            else
            {
                ESP_LOGW(TAG, "  Failed on attempt %d", attempt);
                delay(300);
            }
        }

        // 检查结果
        delay(500);
        status = WiFi.status();
        ESP_LOGI(TAG, "Wi-Fi status after recovery attempt: %d", status);
    }

    // 检查Wi-Fi基本功能
    bool basicFunctionsOk = true;

    // 1. 检查模式设置
    if (!WiFi.getMode())
    {
        ESP_LOGW(TAG, "Wi-Fi mode not set, attempting to set...");
        if (!WiFi.mode(WIFI_STA))
        {
            ESP_LOGE(TAG, "Failed to set Wi-Fi mode");
            basicFunctionsOk = false;
        }
    }

    // 2. 检查MAC地址（基本硬件存在性）
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK)
    {
        ESP_LOGW(TAG, "Cannot read Wi-Fi MAC address");
        basicFunctionsOk = false;
    }
    else
    {
        ESP_LOGI(TAG, "Wi-Fi MAC address readable: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    // 3. 检查Wi-Fi库功能
    ESP_LOGI(TAG, "Wi-Fi library functional");
    // Note: WiFi.getVersion() may not be available in all implementations

    // 4. 测试简单的Wi-Fi操作
    int8_t power = WiFi.getTxPower();
    ESP_LOGI(TAG, "Wi-Fi TX power: %d dBm", power);

    // 总结
    if (basicFunctionsOk && status != WL_NO_SHIELD)
    {
        ESP_LOGI(TAG, "=== Wi-Fi Hardware: READY ===");
        return true;
    }
    else
    {
        ESP_LOGW(TAG, "=== Wi-Fi Hardware: LIMITED FUNCTIONALITY ===");
        ESP_LOGW(TAG, "Status: %d, Basic functions: %s", status, basicFunctionsOk ? "OK" : "FAILED");
        ESP_LOGW(TAG, "Connection may still work, but hardware is not fully ready.");

        // 即使不理想，也允许连接尝试
        return false;
    }
}

// ============================================================================
// 清除保存的网络配置
// ============================================================================

void NetworkManager::clearSavedNetworks()
{
    ESP_LOGI(TAG, "Clearing all saved Wi-Fi networks...");

    // 方法1: 使用WiFi.disconnect(true)清除保存的网络
    WiFi.disconnect(true);  // true参数表示清除保存的网络

    // 方法2: 重置WiFiManager保存的配置
    if (wifiManager)
    {
        ESP_LOGI(TAG, "Clearing WiFiManager configuration...");
        wifiManager->resetSettings(); // 重置WiFiManager设置
    }

    // 方法3: 删除NVS中的WiFi配置（ESP32-specific）
    // 注意：这可能会删除其他NVS数据，谨慎使用
    ESP_LOGI(TAG, "Deleting Wi-Fi NVS configuration...");
    nvs_flash_erase(); // 擦除整个NVS分区
    nvs_flash_init();  // 重新初始化NVS

    // 重新初始化WiFi
    WiFi.begin("", ""); // 使用空SSID和密码初始化

    ESP_LOGI(TAG, "All saved Wi-Fi networks cleared");
    ESP_LOGI(TAG, "Device will need to reconnect to Wi-Fi network");
}

// ============================================================================
// WiFiManager辅助函数
// ============================================================================

bool NetworkManager::startWiFiManagerHotspot()
{
    ESP_LOGI(TAG, "Starting WiFiManager hotspot mode for configuration");

    // 确保WiFiManager实例存在
    if (!wifiManager)
    {
        ESP_LOGW(TAG, "WiFiManager not created, creating instance...");
        wifiManager = new WiFiManager();
        // 配置WiFiManager参数
        wifiManager->setConnectTimeout(30);       // 连接超时30秒
        wifiManager->setConfigPortalTimeout(600); // 配置门户超时10分钟
        wifiManager->setDebugOutput(true);        // 启用调试输出
        wifiManager->setWiFiAPChannel(1);         // 使用通道1（兼容性最好）
        wifiManager->setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        ESP_LOGI(TAG, "WiFiManager instance created for hotspot mode");
    }

    String apName = "XiaozhiAP";

    // 配置WiFiManager AP参数
    wifiManager->setAPCallback([](WiFiManager *wm)
                               {
            ESP_LOGI("NetworkManager", "WiFiManager AP started");
            Serial.println("[NetworkManager] WiFiManager AP started - SSID: XiaozhiAP, IP: 192.168.4.1");

            // 设置AP参数
            WiFi.softAPsetHostname("xiaozhi.local");

            // 记录AP信息
            String apIP = WiFi.softAPIP().toString();
            ESP_LOGI("NetworkManager", "AP IP address: %s", apIP.c_str());
            ESP_LOGI("NetworkManager", "AP SSID: XiaozhiAP");
            ESP_LOGI("NetworkManager", "AP is visible and broadcasting"); });

    // 记录热点启动信息
    ESP_LOGI(TAG, "Starting WiFi configuration portal...");
    ESP_LOGI(TAG, "Hotspot: %s (no password)", apName.c_str());
    ESP_LOGI(TAG, "Configuration page: http://192.168.4.1");
    ESP_LOGI(TAG, "Hotspot will remain active for 10 minutes or until configured");
    Serial.println("[NetworkManager] WiFi Configuration Portal Starting...");
    Serial.println("[NetworkManager] Hotspot: XiaozhiAP (no password)");
    Serial.println("[NetworkManager] Web interface: http://192.168.4.1");
    Serial.println("[NetworkManager] Hotspot will stay active for 10 minutes");

    // 启动配置门户（热点会一直存在，直到用户配置或超时）
    bool configured = wifiManager->startConfigPortal(apName.c_str());

    if (configured)
    {
        ESP_LOGI(TAG, "WiFiManager configuration successful");
        Serial.println("[NetworkManager] WiFi configuration successful via web portal");

        // 获取实际连接的SSID并更新配置
        String connectedSSID = WiFi.SSID();
        if (connectedSSID != wifiConfig.ssid && configManager)
        {
            // 更新配置文件中的SSID（密码WiFiManager已保存）
            configManager->setString("wifi.ssid", connectedSSID);
            ESP_LOGI(TAG, "Updated config SSID to: %s", connectedSSID.c_str());
            Serial.printf("[NetworkManager] Updated WiFi config: %s\n", connectedSSID.c_str());
        }
        return true;
    }
    else
    {
        ESP_LOGW(TAG, "WiFiManager configuration portal timed out or was cancelled");
        Serial.println("[NetworkManager] Configuration portal timed out (10 minutes)");
        Serial.println("[NetworkManager] Hotspot has been closed");

        // 注意：这里返回false，但热点已经关闭
        // 用户可以重新调用connect()再次启动热点
        notifyEvent(NetworkEvent::WIFI_CONNECTION_FAILED, "Configuration timeout");
        return false;
    }
}

bool NetworkManager::startWiFiManagerAutoConnect()
{
    ESP_LOGI(TAG, "Starting WiFiManager auto-connect (using saved credentials)");

    // 确保WiFiManager实例存在
    if (!wifiManager)
    {
        ESP_LOGW(TAG, "WiFiManager not created, creating instance...");
        wifiManager = new WiFiManager();
        // 配置WiFiManager参数
        wifiManager->setConnectTimeout(30);       // 连接超时30秒
        wifiManager->setConfigPortalTimeout(600); // 配置门户超时10分钟
        wifiManager->setDebugOutput(true);        // 启用调试输出
        wifiManager->setWiFiAPChannel(1);         // 使用通道1（兼容性最好）
        wifiManager->setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        ESP_LOGI(TAG, "WiFiManager instance created for auto-connect");
    }

    // 使用autoConnect尝试连接，如果失败则启动热点
    String apName = "XiaozhiAP";
    bool connected = wifiManager->autoConnect(apName.c_str());

    if (connected)
    {
        ESP_LOGI(TAG, "WiFiManager auto-connect successful");
        ESP_LOGI(TAG, "SSID: %s", WiFi.SSID().c_str());
        ESP_LOGI(TAG, "IP Address: %s", WiFi.localIP().toString().c_str());

        // 更新状态
        status.wifiConnected = true;
        status.hasIP = true;
        status.localIP = WiFi.localIP().toString();
        status.ssid = WiFi.SSID();
        status.rssi = WiFi.RSSI();
        status.connectionTime = millis();

        notifyEvent(NetworkEvent::WIFI_CONNECTED, WiFi.SSID());
        return true;
    }
    else
    {
        ESP_LOGW(TAG, "WiFiManager auto-connect failed (user cancelled or timeout)");
        notifyEvent(NetworkEvent::WIFI_CONNECTION_FAILED, "Auto-connect failed");
        return false;
    }
}

// ============================================================================
// SSL客户端管理方法
// ============================================================================

WiFiClientSecure* NetworkManager::getSSLClientForUrl(const String& url) {
    ESP_LOGI(TAG, "getSSLClientForUrl called for: %s", url.c_str());

    if (!sslClientManager) {
        ESP_LOGE(TAG, "ERROR: SSLClientManager not initialized");
        return nullptr;
    }

    // 从URL中提取主机名和端口
    String host;
    uint16_t port = 443; // HTTPS默认端口

    // 简单解析URL，提取主机名
    if (url.startsWith("https://")) {
        int hostStart = 8; // "https://".length()
        int hostEnd = url.indexOf('/', hostStart);
        if (hostEnd == -1) {
            hostEnd = url.length();
        }

        String hostPort = url.substring(hostStart, hostEnd);
        int colonPos = hostPort.indexOf(':');
        if (colonPos != -1) {
            host = hostPort.substring(0, colonPos);
            port = hostPort.substring(colonPos + 1).toInt();
        } else {
            host = hostPort;
        }
    } else {
        // 非HTTPS URL，不需要SSL客户端
        return nullptr;
    }

    // 检查内部堆是否充足 - 提高到30000字节，更保守以避免分配失败
    if (!sslClientManager->checkInternalHeap(30000)) {
        ESP_LOGW(TAG, "Internal heap insufficient for SSL client to %s:%d (available < 30KB), attempting emergency cleanup", host.c_str(), port);

        // 尝试紧急清理SSL客户端
        size_t beforeInternal = esp_get_free_internal_heap_size();
        sslClientManager->cleanupAll(true);  // 强制清理所有SSL客户端
        size_t afterInternal = esp_get_free_internal_heap_size();

        ESP_LOGI(TAG, "Emergency SSL cleanup: before=%u bytes, after=%u bytes, freed=%d bytes",
                beforeInternal, afterInternal, afterInternal - beforeInternal);

        // 再次检查内存
        if (!sslClientManager->checkInternalHeap(30000)) {
            ESP_LOGE(TAG, "Internal heap STILL insufficient after emergency cleanup (%u bytes < 30KB)", afterInternal);
            return nullptr;
        }

        ESP_LOGI(TAG, "Emergency cleanup successful, memory now sufficient for SSL client");
    }

    WiFiClientSecure* sslClient = sslClientManager->getClient(host, port);
    if (sslClient) {
        ESP_LOGI(TAG, "Got SSL client %p for %s:%d", sslClient, host.c_str(), port);
    } else {
        ESP_LOGW(TAG, "Failed to get SSL client for %s:%d", host.c_str(), port);
    }

    return sslClient;
}

void NetworkManager::releaseSSLClientForHttpClient(HTTPClient* httpClient) {
    if (!httpClient || !sslClientManager) return;

    auto it = sslClientMap.find(httpClient);
    if (it != sslClientMap.end()) {
        WiFiClientSecure* sslClient = it->second;
        ESP_LOGI(TAG, "Releasing SSL client %p for HTTP client %p", sslClient, httpClient);

        // 释放SSL客户端（保持连接活跃以减少重连开销）
        sslClientManager->releaseClient(sslClient, true);

        // 从映射中移除
        sslClientMap.erase(it);
    }
}

bool NetworkManager::beginHttpWithSSLClient(HTTPClient* http, const String& url, WiFiClientSecure* sslClient) {
    ESP_LOGI(TAG, "beginHttpWithSSLClient called: http=%p, sslClient=%p, url=%s", http, sslClient, url.c_str());

    if (!http || !sslClient) {
        ESP_LOGE(TAG, "ERROR: Invalid parameters for beginHttpWithSSLClient");
        return false;
    }

    // 使用重载的begin方法，传入自定义的WiFiClientSecure
    ESP_LOGI(TAG, "Calling http->begin(*sslClient, url) for SSL client reuse");
    bool result = http->begin(*sslClient, url);

    if (result) {
        ESP_LOGI(TAG, "SUCCESS: HTTP client %p began with SSL client %p for %s", http, sslClient, url.c_str());

        // 保存映射关系
        sslClientMap[http] = sslClient;
    } else {
        ESP_LOGE(TAG, "ERROR: HTTP client failed to begin with SSL client for %s", url.c_str());

        // 立即释放SSL客户端
        sslClientManager->releaseClient(sslClient, false);
    }

    return result;
}