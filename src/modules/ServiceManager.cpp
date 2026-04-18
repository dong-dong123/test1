#include "ServiceManager.h"
#include <esp_log.h>

static const char* TAG = "ServiceManager";

// ============================================================================
// 构造函数/析构函数
// ============================================================================

ServiceManager::ServiceManager(ConfigManager* configMgr, Logger* log)
    : configManager(configMgr),
      logger(log),
      defaultSpeechService(""),
      defaultDialogueService(""),
      isInitialized(false),
      lastHealthCheckTime(0),
      healthCheckInterval(30000) { // 默认30秒检查一次

    ESP_LOGI(TAG, "ServiceManager created");
}

ServiceManager::~ServiceManager() {
    deinitialize();
}

// ============================================================================
// 初始化/反初始化
// ============================================================================

bool ServiceManager::initialize() {
    if (isInitialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing ServiceManager...");

    // 加载配置
    if (!loadConfig()) {
        ESP_LOGE(TAG, "Failed to load configuration");
        return false;
    }

    // 执行初始健康检查
    performHealthCheck();

    isInitialized = true;
    ESP_LOGI(TAG, "ServiceManager initialized successfully");

    // 记录初始化信息
    if (logger) {
        logger->log(Logger::Level::INFO, "ServiceManager initialized");
        logger->log(Logger::Level::INFO,
            String("Speech services: ") + String(getTotalSpeechServices()) +
            ", Dialogue services: " + String(getTotalDialogueServices()));
    }

    return true;
}

bool ServiceManager::deinitialize() {
    if (!isInitialized) {
        return true;
    }

    ESP_LOGI(TAG, "Deinitializing ServiceManager...");

    // 清理所有服务
    cleanupServices();

    // 清空健康状态
    speechServiceHealth.clear();
    dialogueServiceHealth.clear();

    // 重置默认服务
    defaultSpeechService = "";
    defaultDialogueService = "";

    isInitialized = false;
    ESP_LOGI(TAG, "ServiceManager deinitialized");

    if (logger) {
        logger->log(Logger::Level::INFO, "ServiceManager deinitialized");
    }

    return true;
}

void ServiceManager::cleanupServices() {
    // 清理语音服务
    for (auto& pair : speechServices) {
        delete pair.second;
    }
    speechServices.clear();

    // 清理对话服务
    for (auto& pair : dialogueServices) {
        delete pair.second;
    }
    dialogueServices.clear();
}

// ============================================================================
// 配置管理
// ============================================================================

bool ServiceManager::loadConfig() {
    if (!configManager) {
        ESP_LOGE(TAG, "No ConfigManager available");
        return false;
    }

    // 从配置管理器读取默认服务
    defaultSpeechService = configManager->getString("services.defaultSpeechService", "volcano");
    defaultDialogueService = configManager->getString("services.defaultDialogueService", "coze");

    ESP_LOGI(TAG, "Default speech service: %s", defaultSpeechService.c_str());
    ESP_LOGI(TAG, "Default dialogue service: %s", defaultDialogueService.c_str());

    // 读取可用服务列表（可选）
    // 这里可以添加从配置读取可用服务列表的逻辑

    return true;
}

bool ServiceManager::reloadConfig() {
    if (!isInitialized) {
        return initialize();
    }

    ESP_LOGI(TAG, "Reloading configuration...");

    // 保存当前服务状态
    std::map<String, SpeechService*> oldSpeechServices = speechServices;
    std::map<String, DialogueService*> oldDialogueServices = dialogueServices;

    // 清空当前服务（但不删除，因为可能被外部管理）
    speechServices.clear();
    dialogueServices.clear();

    // 重新加载配置
    if (!loadConfig()) {
        // 恢复旧服务
        speechServices = oldSpeechServices;
        dialogueServices = oldDialogueServices;
        return false;
    }

    // 重新注册旧服务
    for (auto& pair : oldSpeechServices) {
        registerSpeechService(pair.second);
    }

    for (auto& pair : oldDialogueServices) {
        registerDialogueService(pair.second);
    }

    ESP_LOGI(TAG, "Configuration reloaded successfully");

    if (logger) {
        logger->log(Logger::Level::INFO, "ServiceManager configuration reloaded");
    }

    return true;
}

void ServiceManager::setConfigManager(ConfigManager* configMgr) {
    configManager = configMgr;
    if (isInitialized && configManager) {
        reloadConfig();
    }
}

void ServiceManager::setLogger(Logger* log) {
    logger = log;
}

// ============================================================================
// 服务注册
// ============================================================================

bool ServiceManager::registerSpeechService(SpeechService* service) {
    if (!service) {
        ESP_LOGE(TAG, "Cannot register null speech service");
        return false;
    }

    String name = service->getName();
    if (name.isEmpty()) {
        ESP_LOGE(TAG, "Speech service has no name");
        return false;
    }

    // 检查是否已注册
    if (speechServices.find(name) != speechServices.end()) {
        ESP_LOGW(TAG, "Speech service '%s' already registered", name.c_str());
        return false;
    }

    // 注册服务
    speechServices[name] = service;

    // 初始化健康状态
    ServiceHealth health;
    health.status = ServiceStatus::INITIALIZING;
    speechServiceHealth[name] = health;

    ESP_LOGI(TAG, "Speech service '%s' registered", name.c_str());

    // 记录日志
    logServiceEvent(name, "registered", "speech service");

    return true;
}

bool ServiceManager::registerDialogueService(DialogueService* service) {
    if (!service) {
        ESP_LOGE(TAG, "Cannot register null dialogue service");
        return false;
    }

    String name = service->getName();
    if (name.isEmpty()) {
        ESP_LOGE(TAG, "Dialogue service has no name");
        return false;
    }

    // 检查是否已注册
    if (dialogueServices.find(name) != dialogueServices.end()) {
        ESP_LOGW(TAG, "Dialogue service '%s' already registered", name.c_str());
        return false;
    }

    // 注册服务
    dialogueServices[name] = service;

    // 初始化健康状态
    ServiceHealth health;
    health.status = ServiceStatus::INITIALIZING;
    dialogueServiceHealth[name] = health;

    ESP_LOGI(TAG, "Dialogue service '%s' registered", name.c_str());

    // 记录日志
    logServiceEvent(name, "registered", "dialogue service");

    return true;
}

bool ServiceManager::unregisterSpeechService(const String& name) {
    auto it = speechServices.find(name);
    if (it == speechServices.end()) {
        ESP_LOGW(TAG, "Speech service '%s' not found", name.c_str());
        return false;
    }

    // 删除健康状态
    speechServiceHealth.erase(name);

    // 删除服务（注意：不删除对象，因为可能由外部管理）
    speechServices.erase(it);

    ESP_LOGI(TAG, "Speech service '%s' unregistered", name.c_str());

    // 记录日志
    logServiceEvent(name, "unregistered", "speech service");

    return true;
}

bool ServiceManager::unregisterDialogueService(const String& name) {
    auto it = dialogueServices.find(name);
    if (it == dialogueServices.end()) {
        ESP_LOGW(TAG, "Dialogue service '%s' not found", name.c_str());
        return false;
    }

    // 删除健康状态
    dialogueServiceHealth.erase(name);

    // 删除服务（注意：不删除对象，因为可能由外部管理）
    dialogueServices.erase(it);

    ESP_LOGI(TAG, "Dialogue service '%s' unregistered", name.c_str());

    // 记录日志
    logServiceEvent(name, "unregistered", "dialogue service");

    return true;
}

// ============================================================================
// 服务获取
// ============================================================================

SpeechService* ServiceManager::getSpeechService(const String& name) {
    if (name.isEmpty()) {
        return getDefaultSpeechService();
    }

    auto it = speechServices.find(name);
    if (it == speechServices.end()) {
        ESP_LOGE(TAG, "Speech service '%s' not found", name.c_str());
        return nullptr;
    }

    return it->second;
}

DialogueService* ServiceManager::getDialogueService(const String& name) {
    if (name.isEmpty()) {
        return getDefaultDialogueService();
    }

    auto it = dialogueServices.find(name);
    if (it == dialogueServices.end()) {
        ESP_LOGE(TAG, "Dialogue service '%s' not found", name.c_str());
        return nullptr;
    }

    return it->second;
}

SpeechService* ServiceManager::getDefaultSpeechService() {
    if (defaultSpeechService.isEmpty()) {
        // 如果没有设置默认服务，返回第一个可用服务
        if (!speechServices.empty()) {
            return speechServices.begin()->second;
        }
        return nullptr;
    }

    SpeechService* service = getSpeechService(defaultSpeechService);
    if (!service) {
        ESP_LOGW(TAG, "Default speech service '%s' not available, trying fallback",
                defaultSpeechService.c_str());
        return getFallbackSpeechService();
    }

    return service;
}

DialogueService* ServiceManager::getDefaultDialogueService() {
    if (defaultDialogueService.isEmpty()) {
        // 如果没有设置默认服务，返回第一个可用服务
        if (!dialogueServices.empty()) {
            return dialogueServices.begin()->second;
        }
        return nullptr;
    }

    DialogueService* service = getDialogueService(defaultDialogueService);
    if (!service) {
        ESP_LOGW(TAG, "Default dialogue service '%s' not available, trying fallback",
                defaultDialogueService.c_str());
        return getFallbackDialogueService();
    }

    return service;
}

// ============================================================================
// 服务发现
// ============================================================================

std::vector<String> ServiceManager::getAvailableSpeechServices() const {
    std::vector<String> services;
    for (const auto& pair : speechServices) {
        services.push_back(pair.first);
    }
    return services;
}

std::vector<String> ServiceManager::getAvailableDialogueServices() const {
    std::vector<String> services;
    for (const auto& pair : dialogueServices) {
        services.push_back(pair.first);
    }
    return services;
}

bool ServiceManager::isSpeechServiceAvailable(const String& name) const {
    auto it = speechServices.find(name);
    if (it == speechServices.end()) {
        return false;
    }

    // 检查健康状态
    auto healthIt = speechServiceHealth.find(name);
    if (healthIt != speechServiceHealth.end()) {
        return healthIt->second.status == ServiceStatus::HEALTHY ||
               healthIt->second.status == ServiceStatus::DEGRADED;
    }

    // 如果没有健康状态，检查服务自身的可用性
    return it->second->isAvailable();
}

bool ServiceManager::isDialogueServiceAvailable(const String& name) const {
    auto it = dialogueServices.find(name);
    if (it == dialogueServices.end()) {
        return false;
    }

    // 检查健康状态
    auto healthIt = dialogueServiceHealth.find(name);
    if (healthIt != dialogueServiceHealth.end()) {
        return healthIt->second.status == ServiceStatus::HEALTHY ||
               healthIt->second.status == ServiceStatus::DEGRADED;
    }

    // 如果没有健康状态，检查服务自身的可用性
    return it->second->isAvailable();
}

// ============================================================================
// 健康管理
// ============================================================================

ServiceStatus ServiceManager::getSpeechServiceStatus(const String& name) const {
    auto it = speechServiceHealth.find(name);
    if (it == speechServiceHealth.end()) {
        return ServiceStatus::UNKNOWN;
    }
    return it->second.status;
}

ServiceStatus ServiceManager::getDialogueServiceStatus(const String& name) const {
    auto it = dialogueServiceHealth.find(name);
    if (it == dialogueServiceHealth.end()) {
        return ServiceStatus::UNKNOWN;
    }
    return it->second.status;
}

const ServiceHealth* ServiceManager::getSpeechServiceHealth(const String& name) const {
    auto it = speechServiceHealth.find(name);
    if (it == speechServiceHealth.end()) {
        return nullptr;
    }
    return &it->second;
}

const ServiceHealth* ServiceManager::getDialogueServiceHealth(const String& name) const {
    auto it = dialogueServiceHealth.find(name);
    if (it == dialogueServiceHealth.end()) {
        return nullptr;
    }
    return &it->second;
}

void ServiceManager::updateHealthCheckInterval(uint32_t interval) {
    healthCheckInterval = interval;
    ESP_LOGI(TAG, "Health check interval set to %u ms", interval);
}

uint32_t ServiceManager::getHealthySpeechServices() const {
    uint32_t count = 0;
    for (const auto& pair : speechServiceHealth) {
        if (pair.second.status == ServiceStatus::HEALTHY ||
            pair.second.status == ServiceStatus::DEGRADED) {
            count++;
        }
    }
    return count;
}

uint32_t ServiceManager::getHealthyDialogueServices() const {
    uint32_t count = 0;
    for (const auto& pair : dialogueServiceHealth) {
        if (pair.second.status == ServiceStatus::HEALTHY ||
            pair.second.status == ServiceStatus::DEGRADED) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// 故障切换
// ============================================================================

SpeechService* ServiceManager::getFallbackSpeechService() {
    // 寻找最佳备用服务
    String bestService = findBestSpeechService();
    if (bestService.isEmpty()) {
        ESP_LOGE(TAG, "No fallback speech service available");
        return nullptr;
    }

    // 确保不是当前默认服务
    if (bestService == defaultSpeechService) {
        ESP_LOGW(TAG, "Fallback service is same as default: %s", bestService.c_str());
    }

    return getSpeechService(bestService);
}

DialogueService* ServiceManager::getFallbackDialogueService() {
    // 寻找最佳备用服务
    String bestService = findBestDialogueService();
    if (bestService.isEmpty()) {
        ESP_LOGE(TAG, "No fallback dialogue service available");
        return nullptr;
    }

    // 确保不是当前默认服务
    if (bestService == defaultDialogueService) {
        ESP_LOGW(TAG, "Fallback service is same as default: %s", bestService.c_str());
    }

    return getDialogueService(bestService);
}

bool ServiceManager::switchToFallbackSpeechService() {
    SpeechService* fallback = getFallbackSpeechService();
    if (!fallback) {
        return false;
    }

    String newDefault = fallback->getName();
    if (newDefault == defaultSpeechService) {
        ESP_LOGW(TAG, "Already using service: %s", newDefault.c_str());
        return true;
    }

    String oldDefault = defaultSpeechService;
    defaultSpeechService = newDefault;

    ESP_LOGI(TAG, "Switched default speech service from '%s' to '%s'",
            oldDefault.c_str(), newDefault.c_str());

    // 记录日志
    if (logger) {
        logger->log(Logger::Level::WARN,
            String("Switched speech service: ") + oldDefault + " -> " + newDefault);
    }

    return true;
}

bool ServiceManager::switchToFallbackDialogueService() {
    DialogueService* fallback = getFallbackDialogueService();
    if (!fallback) {
        return false;
    }

    String newDefault = fallback->getName();
    if (newDefault == defaultDialogueService) {
        ESP_LOGW(TAG, "Already using service: %s", newDefault.c_str());
        return true;
    }

    String oldDefault = defaultDialogueService;
    defaultDialogueService = newDefault;

    ESP_LOGI(TAG, "Switched default dialogue service from '%s' to '%s'",
            oldDefault.c_str(), newDefault.c_str());

    // 记录日志
    if (logger) {
        logger->log(Logger::Level::WARN,
            String("Switched dialogue service: ") + oldDefault + " -> " + newDefault);
    }

    return true;
}

// ============================================================================
// 最佳服务选择
// ============================================================================

String ServiceManager::findBestSpeechService() const {
    String bestService;
    ServiceStatus bestStatus = ServiceStatus::UNHEALTHY;

    for (const auto& pair : speechServices) {
        const String& name = pair.first;

        // 获取健康状态
        ServiceStatus status = getSpeechServiceStatus(name);

        // 优先选择健康状态最好的服务
        if (static_cast<int>(status) < static_cast<int>(bestStatus)) {
            bestStatus = status;
            bestService = name;
        }
    }

    return bestService;
}

String ServiceManager::findBestDialogueService() const {
    String bestService;
    ServiceStatus bestStatus = ServiceStatus::UNHEALTHY;

    for (const auto& pair : dialogueServices) {
        const String& name = pair.first;

        // 获取健康状态
        ServiceStatus status = getDialogueServiceStatus(name);

        // 优先选择健康状态最好的服务
        if (static_cast<int>(status) < static_cast<int>(bestStatus)) {
            bestStatus = status;
            bestService = name;
        }
    }

    return bestService;
}

// ============================================================================
// 健康检查
// ============================================================================

void ServiceManager::update() {
    if (!isInitialized) {
        return;
    }

    uint32_t currentTime = millis();

    // 检查是否需要执行健康检查
    if (currentTime - lastHealthCheckTime >= healthCheckInterval) {
        performHealthCheck();
        lastHealthCheckTime = currentTime;
    }
}

void ServiceManager::performHealthCheck() {
    ESP_LOGD(TAG, "Performing health check...");

    // 检查所有语音服务
    for (auto& pair : speechServices) {
        const String& name = pair.first;
        SpeechService* service = pair.second;

        auto healthIt = speechServiceHealth.find(name);
        if (healthIt == speechServiceHealth.end()) {
            continue;
        }

        ServiceHealth& health = healthIt->second;
        checkServiceHealth(service, health);
    }

    // 检查所有对话服务
    for (auto& pair : dialogueServices) {
        const String& name = pair.first;
        DialogueService* service = pair.second;

        auto healthIt = dialogueServiceHealth.find(name);
        if (healthIt == dialogueServiceHealth.end()) {
            continue;
        }

        ServiceHealth& health = healthIt->second;
        checkServiceHealth(service, health);
    }

    ESP_LOGD(TAG, "Health check completed");
}

bool ServiceManager::checkServiceHealth(SpeechService* service, ServiceHealth& health) {
    if (!service) {
        health.status = ServiceStatus::UNHEALTHY;
        health.lastError = "Service is null";
        return false;
    }

    uint32_t startTime = millis();
    bool isAvailable = service->isAvailable();
    uint32_t endTime = millis();

    health.lastCheckTime = endTime;

    if (isAvailable) {
        health.status = ServiceStatus::HEALTHY;
        health.successCount++;
        health.avgResponseTime = (health.avgResponseTime * (health.successCount - 1) +
                                 (endTime - startTime)) / health.successCount;
        health.lastError = "";
        return true;
    } else {
        health.status = ServiceStatus::UNHEALTHY;
        health.failureCount++;
        health.lastError = "Service reported unavailable";
        return false;
    }
}

bool ServiceManager::checkServiceHealth(DialogueService* service, ServiceHealth& health) {
    if (!service) {
        health.status = ServiceStatus::UNHEALTHY;
        health.lastError = "Service is null";
        return false;
    }

    uint32_t startTime = millis();
    bool isAvailable = service->isAvailable();
    uint32_t endTime = millis();

    health.lastCheckTime = endTime;

    if (isAvailable) {
        health.status = ServiceStatus::HEALTHY;
        health.successCount++;
        health.avgResponseTime = (health.avgResponseTime * (health.successCount - 1) +
                                 (endTime - startTime)) / health.successCount;
        health.lastError = "";
        return true;
    } else {
        health.status = ServiceStatus::UNHEALTHY;
        health.failureCount++;
        health.lastError = "Service reported unavailable";
        return false;
    }
}

// ============================================================================
// 工具方法
// ============================================================================

void ServiceManager::printServiceInfo() const {
    ESP_LOGI(TAG, "=== Service Manager Info ===");
    ESP_LOGI(TAG, "Initialized: %s", isInitialized ? "Yes" : "No");
    ESP_LOGI(TAG, "Default Speech Service: %s", defaultSpeechService.c_str());
    ESP_LOGI(TAG, "Default Dialogue Service: %s", defaultDialogueService.c_str());
    ESP_LOGI(TAG, "Health Check Interval: %u ms", healthCheckInterval);

    ESP_LOGI(TAG, "--- Speech Services (%u) ---", getTotalSpeechServices());
    for (const auto& pair : speechServices) {
        const String& name = pair.first;
        ServiceStatus status = getSpeechServiceStatus(name);

        ESP_LOGI(TAG, "  %s: %s", name.c_str(),
                status == ServiceStatus::HEALTHY ? "HEALTHY" :
                status == ServiceStatus::DEGRADED ? "DEGRADED" :
                status == ServiceStatus::UNHEALTHY ? "UNHEALTHY" :
                status == ServiceStatus::INITIALIZING ? "INITIALIZING" :
                status == ServiceStatus::DISABLED_STATUS ? "DISABLED" : "UNKNOWN");
    }

    ESP_LOGI(TAG, "--- Dialogue Services (%u) ---", getTotalDialogueServices());
    for (const auto& pair : dialogueServices) {
        const String& name = pair.first;
        ServiceStatus status = getDialogueServiceStatus(name);

        ESP_LOGI(TAG, "  %s: %s", name.c_str(),
                status == ServiceStatus::HEALTHY ? "HEALTHY" :
                status == ServiceStatus::DEGRADED ? "DEGRADED" :
                status == ServiceStatus::UNHEALTHY ? "UNHEALTHY" :
                status == ServiceStatus::INITIALIZING ? "INITIALIZING" :
                status == ServiceStatus::DISABLED_STATUS ? "DISABLED" : "UNKNOWN");
    }
    ESP_LOGI(TAG, "=============================");
}

void ServiceManager::resetStatistics() {
    for (auto& pair : speechServiceHealth) {
        pair.second.successCount = 0;
        pair.second.failureCount = 0;
        pair.second.avgResponseTime = 0.0f;
    }

    for (auto& pair : dialogueServiceHealth) {
        pair.second.successCount = 0;
        pair.second.failureCount = 0;
        pair.second.avgResponseTime = 0.0f;
    }

    ESP_LOGI(TAG, "Service statistics reset");
}

void ServiceManager::logServiceEvent(const String& serviceName, const String& event, const String& details) {
    if (logger) {
        String message = "Service " + serviceName + " " + event;
        if (!details.isEmpty()) {
            message += " (" + details + ")";
        }
        logger->log(Logger::Level::INFO, message);
    }
}