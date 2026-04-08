#ifndef SERVICE_MANAGER_H
#define SERVICE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>
#include "../interfaces/SpeechService.h"
#include "../interfaces/DialogueService.h"
#include "../interfaces/ConfigManager.h"
#include "../interfaces/Logger.h"

// 服务状态枚举
enum class ServiceStatus {
    UNKNOWN,        // 未知状态
    INITIALIZING,   // 初始化中
    HEALTHY,        // 健康（可用）
    DEGRADED,       // 降级（部分功能可用）
    UNHEALTHY,      // 不健康（不可用）
    DISABLED_STATUS        // 已禁用
};

// 服务健康信息
struct ServiceHealth {
    ServiceStatus status;
    uint32_t lastCheckTime;
    uint32_t failureCount;
    uint32_t successCount;
    float avgResponseTime;
    String lastError;

    ServiceHealth() :
        status(ServiceStatus::UNKNOWN),
        lastCheckTime(0),
        failureCount(0),
        successCount(0),
        avgResponseTime(0.0f) {}
};

// 服务管理器类 - 管理所有语音和对话服务
class ServiceManager {
private:
    // 配置管理
    ConfigManager* configManager;
    Logger* logger;

    // 服务注册表
    std::map<String, SpeechService*> speechServices;
    std::map<String, DialogueService*> dialogueServices;

    // 服务健康状态
    std::map<String, ServiceHealth> speechServiceHealth;
    std::map<String, ServiceHealth> dialogueServiceHealth;

    // 默认服务
    String defaultSpeechService;
    String defaultDialogueService;

    // 状态标志
    bool isInitialized;
    uint32_t lastHealthCheckTime;
    uint32_t healthCheckInterval; // 健康检查间隔（毫秒）

    // 内部方法
    bool loadConfig();
    void updateHealthStatus();
    bool checkServiceHealth(SpeechService* service, ServiceHealth& health);
    bool checkServiceHealth(DialogueService* service, ServiceHealth& health);
    void performHealthCheck();

public:
    ServiceManager(ConfigManager* configMgr = nullptr, Logger* log = nullptr);
    virtual ~ServiceManager();

    // 初始化/反初始化
    bool initialize();
    bool deinitialize();
    bool isReady() const { return isInitialized; }

    // 服务注册
    bool registerSpeechService(SpeechService* service);
    bool registerDialogueService(DialogueService* service);
    bool unregisterSpeechService(const String& name);
    bool unregisterDialogueService(const String& name);

    // 服务获取
    SpeechService* getSpeechService(const String& name = "");
    DialogueService* getDialogueService(const String& name = "");
    SpeechService* getDefaultSpeechService();
    DialogueService* getDefaultDialogueService();

    // 服务发现
    std::vector<String> getAvailableSpeechServices() const;
    std::vector<String> getAvailableDialogueServices() const;
    bool isSpeechServiceAvailable(const String& name) const;
    bool isDialogueServiceAvailable(const String& name) const;

    // 健康管理
    ServiceStatus getSpeechServiceStatus(const String& name) const;
    ServiceStatus getDialogueServiceStatus(const String& name) const;
    const ServiceHealth* getSpeechServiceHealth(const String& name) const;
    const ServiceHealth* getDialogueServiceHealth(const String& name) const;
    void updateHealthCheckInterval(uint32_t interval);

    // 故障切换
    SpeechService* getFallbackSpeechService();
    DialogueService* getFallbackDialogueService();
    bool switchToFallbackSpeechService();
    bool switchToFallbackDialogueService();

    // 配置管理
    void setConfigManager(ConfigManager* configMgr);
    void setLogger(Logger* log);
    bool reloadConfig();

    // 统计信息
    uint32_t getTotalSpeechServices() const { return speechServices.size(); }
    uint32_t getTotalDialogueServices() const { return dialogueServices.size(); }
    uint32_t getHealthySpeechServices() const;
    uint32_t getHealthyDialogueServices() const;

    // 工具方法
    void printServiceInfo() const;
    void resetStatistics();

    // 生命周期管理
    void update(); // 需要定期调用以执行健康检查等任务

private:
    // 内部辅助方法
    void cleanupServices();
    String findBestSpeechService() const;
    String findBestDialogueService() const;
    void logServiceEvent(const String& serviceName, const String& event, const String& details = "");
};

#endif // SERVICE_MANAGER_H