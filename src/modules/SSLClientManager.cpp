#include "SSLClientManager.h"
#include <esp_log.h>
#include <esp_heap_caps.h>

static const char* TAG = "SSLClientManager";

SSLClientManager::SSLClientManager()
    : maxClients(2)  // 最大2个客户端，避免内存占用过多
    , peakInternalMemory(0)
    , connectionFailures(0)
    , reuseCount(0) {
    mutex = xSemaphoreCreateMutex();
    if (!mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
    }

    // 预分配1个客户端
    preallocateClients(1);

    ESP_LOGI(TAG, "SSLClientManager initialized, max clients: %d", maxClients);
}

SSLClientManager::~SSLClientManager() {
    cleanupAll(true);
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

SSLClientManager& SSLClientManager::getInstance() {
    static SSLClientManager instance;
    return instance;
}

WiFiClientSecure* SSLClientManager::getClient(const String& host, uint16_t port) {
    ESP_LOGI(TAG, "SSLClientManager::getClient called for host=%s, port=%d", host.c_str(), port);

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "ERROR: Failed to acquire mutex");
        return nullptr;
    }

    // 记录内存状态
    logMemoryStatus("before getClient");

    // 首先尝试查找可复用的客户端
    WiFiClientSecure* client = findAvailableClient(host, port);
    if (client) {
        reuseCount++;
        ESP_LOGI(TAG, "Reusing SSL client for %s:%d (total reuse: %d)",
                host.c_str(), port, reuseCount);
    } else if (clients.size() < maxClients) {
        // 创建新客户端
        ESP_LOGI(TAG, "Creating new SSL client for %s:%d", host.c_str(), port);
        client = new WiFiClientSecure();

        // 优化SSL配置，减少内存占用
        client->setInsecure(); // 跳过证书验证（测试环境）
        client->setTimeout(10); // 减少超时时间

        SSLClientEntry entry;
        entry.client = client;
        entry.host = host;
        entry.port = port;
        entry.lastUsedTime = millis();
        entry.inUse = true;
        entry.connected = false;
        entry.useCount = 1;

        clients.push_back(entry);
    } else {
        // 超出限制，清理最旧的空闲客户端
        ESP_LOGW(TAG, "Max clients reached (%d), cleaning oldest idle client", maxClients);
        internalCleanup(false);

        // 再次尝试查找
        client = findAvailableClient(host, port);
        if (!client) {
            ESP_LOGW(TAG, "No available SSL client after cleanup");
            connectionFailures++;
        }
    }

    if (client) {
        // 更新客户端状态
        for (auto& entry : clients) {
            if (entry.client == client) {
                entry.inUse = true;
                entry.lastUsedTime = millis();
                entry.host = host;
                entry.port = port;
                entry.useCount++;
                break;
            }
        }
    }

    xSemaphoreGive(mutex);

    if (client) {
        ESP_LOGI(TAG, "SSL client %p allocated for %s:%d", client, host.c_str(), port);
        logMemoryStatus("after getClient");
    }

    return client;
}

void SSLClientManager::releaseClient(WiFiClientSecure* client, bool keepAlive) {
    if (!client) return;

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for release");
        return;
    }

    // 查找客户端
    bool found = false;
    for (auto& entry : clients) {
        if (entry.client == client) {
            found = true;
            entry.inUse = false;
            entry.lastUsedTime = millis();

            if (!keepAlive && entry.connected) {
                // 断开连接但不删除客户端
                client->stop();
                entry.connected = false;
                ESP_LOGI(TAG, "SSL client %p disconnected", client);
            }

            break;
        }
    }

    xSemaphoreGive(mutex);

    if (found) {
        ESP_LOGI(TAG, "SSL client %p released (keepAlive: %s)", client, keepAlive ? "true" : "false");
    } else {
        ESP_LOGW(TAG, "Released unknown SSL client %p", client);
        delete client; // 未知客户端，直接删除
    }

    logMemoryStatus("after releaseClient");
}

void SSLClientManager::cleanupAll(bool force) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for cleanup");
        return;
    }

    internalCleanup(force);

    xSemaphoreGive(mutex);

    ESP_LOGI(TAG, "SSL clients cleaned up (force: %s)", force ? "true" : "false");
    logMemoryStatus("after cleanupAll");
}

void SSLClientManager::logMemoryStatus(const char* context) {
    size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t freeSPIRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t minFree = esp_get_minimum_free_heap_size();

    if (freeInternal < 40000) {
        ESP_LOGW(TAG, "[%s] Low internal heap: %u bytes, SPIRAM: %u bytes, Min free: %u",
                context, freeInternal, freeSPIRAM, minFree);
    } else {
        ESP_LOGI(TAG, "[%s] Internal heap: %u bytes, SPIRAM: %u bytes, Min free: %u",
                context, freeInternal, freeSPIRAM, minFree);
    }

    // 更新峰值内存
    size_t usedInternal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL) - freeInternal;
    if (usedInternal > peakInternalMemory) {
        peakInternalMemory = usedInternal;
    }
}

void SSLClientManager::preallocateClients(int count) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for preallocation");
        return;
    }

    int toCreate = count;
    if (clients.size() + toCreate > maxClients) {
        toCreate = maxClients - clients.size();
    }

    for (int i = 0; i < toCreate; i++) {
        WiFiClientSecure* client = new WiFiClientSecure();
        client->setInsecure();
        client->setTimeout(10);

        SSLClientEntry entry;
        entry.client = client;
        entry.host = "";
        entry.port = 0;
        entry.lastUsedTime = millis();
        entry.inUse = false;
        entry.connected = false;
        entry.useCount = 0;

        clients.push_back(entry);
        ESP_LOGI(TAG, "Preallocated SSL client %p", client);
    }

    xSemaphoreGive(mutex);

    if (toCreate > 0) {
        ESP_LOGI(TAG, "Preallocated %d SSL client(s)", toCreate);
        logMemoryStatus("after preallocation");
    }
}

SSLClientManager::Stats SSLClientManager::getStats() {
    Stats stats = {0};

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for stats");
        return stats;
    }

    stats.totalClients = clients.size();
    for (const auto& entry : clients) {
        if (entry.inUse) {
            stats.activeClients++;
        } else {
            stats.idleClients++;
        }
    }

    stats.peakInternalMemory = peakInternalMemory;
    stats.currentInternalMemory = heap_caps_get_total_size(MALLOC_CAP_INTERNAL) -
                                  heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    stats.connectionFailures = connectionFailures;
    stats.reuseCount = reuseCount;

    xSemaphoreGive(mutex);

    return stats;
}

bool SSLClientManager::checkInternalHeap(size_t required) {
    size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    bool sufficient = freeInternal >= required;

    if (!sufficient) {
        ESP_LOGE(TAG, "ERROR: Internal heap insufficient: %u available, %u required",
                freeInternal, required);
    } else {
        ESP_LOGI(TAG, "Internal heap sufficient: %u available, %u required",
                freeInternal, required);
    }

    return sufficient;
}

// ===== 私有方法 =====

void SSLClientManager::internalCleanup(bool force) {
    size_t before = clients.size();

    // 清理策略：
    // 1. force=true: 清理所有客户端
    // 2. force=false: 只清理空闲超过30秒的客户端
    auto it = clients.begin();
    while (it != clients.end()) {
        bool shouldRemove = false;

        if (force) {
            shouldRemove = true;
        } else {
            // 清理空闲超过30秒的客户端
            if (!it->inUse && (millis() - it->lastUsedTime > 30000)) {
                shouldRemove = true;
            }
        }

        if (shouldRemove) {
            destroyClient(it->client);
            it = clients.erase(it);
        } else {
            ++it;
        }
    }

    size_t removed = before - clients.size();
    if (removed > 0) {
        ESP_LOGI(TAG, "Cleaned up %d SSL client(s)", removed);
    }
}

WiFiClientSecure* SSLClientManager::findAvailableClient(const String& host, uint16_t port) {
    // 优先查找匹配主机端口的空闲客户端
    for (auto& entry : clients) {
        if (!entry.inUse && entry.host == host && entry.port == port) {
            return entry.client;
        }
    }

    // 其次查找任何空闲客户端
    for (auto& entry : clients) {
        if (!entry.inUse) {
            return entry.client;
        }
    }

    return nullptr;
}

void SSLClientManager::destroyClient(WiFiClientSecure* client) {
    if (!client) return;

    ESP_LOGI(TAG, "Destroying SSL client %p", client);
    client->stop();
    delete client;
}