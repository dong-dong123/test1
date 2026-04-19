#ifndef SSLCLIENTMANAGER_H
#define SSLCLIENTMANAGER_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * SSL客户端管理器
 * 用于复用WiFiClientSecure对象，减少SSL内存分配和碎片化
 * 解决Arduino框架mbedTLS内存配置不可修改的问题
 */
class SSLClientManager {
public:
    static SSLClientManager& getInstance();

    /**
     * 获取一个可用的SSL客户端
     * @param host 目标主机（用于缓存连接）
     * @param port 目标端口（默认443）
     * @return 可用的WiFiClientSecure指针，失败返回nullptr
     */
    WiFiClientSecure* getClient(const String& host = "", uint16_t port = 443);

    /**
     * 释放SSL客户端
     * @param client 要释放的客户端
     * @param keepAlive 是否保持连接活跃（默认true）
     */
    void releaseClient(WiFiClientSecure* client, bool keepAlive = true);

    /**
     * 清理所有SSL客户端
     * @param force 是否强制清理（包括活跃连接）
     */
    void cleanupAll(bool force = false);

    /**
     * 获取当前内存状态
     */
    void logMemoryStatus(const char* context = "");

    /**
     * 预分配SSL客户端（减少首次连接延迟）
     * @param count 预分配数量（默认1）
     */
    void preallocateClients(int count = 1);

    /**
     * 获取统计信息
     */
    struct Stats {
        int totalClients;
        int activeClients;
        int idleClients;
        size_t peakInternalMemory;
        size_t currentInternalMemory;
        int connectionFailures;
        int reuseCount;
    };

    Stats getStats();

    /**
     * 检查内部堆是否充足
     * @param required 所需最小内存（字节）
     * @return 是否充足
     */
    bool checkInternalHeap(size_t required = 50000);

private:
    SSLClientManager();
    ~SSLClientManager();

    // 禁止拷贝
    SSLClientManager(const SSLClientManager&) = delete;
    SSLClientManager& operator=(const SSLClientManager&) = delete;

    struct SSLClientEntry {
        WiFiClientSecure* client;
        String host;
        uint16_t port;
        uint32_t lastUsedTime;
        bool inUse;
        bool connected;
        int useCount;
    };

    std::vector<SSLClientEntry> clients;
    SemaphoreHandle_t mutex;
    size_t maxClients;

    void internalCleanup(bool force = false);
    WiFiClientSecure* findAvailableClient(const String& host, uint16_t port);
    void destroyClient(WiFiClientSecure* client);

    // 统计
    size_t peakInternalMemory;
    int connectionFailures;
    int reuseCount;
};

#endif // SSLCLIENTMANAGER_H