/**
 * @file BinaryProtocolEncoder.cpp
 * @brief WebSocket binary protocol encoder implementation for Volcano speech recognition service
 * @details Implements the binary protocol for encoding Full Client Request and
 *          Audio Only Request messages according to Volcano WebSocket specification.
 * @version 1.1
 * @date 2026-04-08
 * @note Fixed critical issues: error handling, memory safety, and code quality improvements
 */

#include "BinaryProtocolEncoder.h"
#include <algorithm>
#include <cstring>

// Debug logging
#ifdef ARDUINO
#include "esp_log.h"
#define PROTOCOL_DEBUG 1
#else
#include <cstdio>
#define PROTOCOL_DEBUG 1
#endif

// Protocol constants
namespace {
    constexpr uint8_t PROTOCOL_VERSION = 0b0001;
    constexpr uint8_t HEADER_SIZE_BYTES = 4;
    constexpr uint8_t EXTENDED_HEADER_SIZE_BYTES = 8; // Header with sequence number
    constexpr uint32_t MAX_PAYLOAD_SIZE = 0xFFFFFFFF; // Maximum 32-bit payload size

    // Flag bit definitions
    constexpr uint8_t FLAG_SEQUENCE_PRESENT = 0b0001;
    constexpr uint8_t FLAG_LAST_CHUNK = 0b0010;
    constexpr uint8_t FLAG_SEQUENCE_AND_LAST_CHUNK = 0b0011;
}

// 编码Full Client Request消息
std::vector<uint8_t> BinaryProtocolEncoder::encodeFullClientRequest(
    const BinaryString& jsonRequest,
    bool useCompression,
    uint32_t sequence
) {
    // 输入验证：检查JSON字符串是否为空
    if (jsonRequest.isEmpty()) {
        // 返回空向量表示错误（避免在嵌入式系统中使用异常）
        return std::vector<uint8_t>();
    }

    // 检查payload大小是否超过最大值
    if (jsonRequest.length() > MAX_PAYLOAD_SIZE) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> result;

    // 预分配内存以提高效率：header + payload size + payload
    size_t estimatedSize = EXTENDED_HEADER_SIZE_BYTES + 4 + jsonRequest.length();
    result.reserve(estimatedSize);

    // 构建header - 根据火山客服指导，客户端请求不包含sequence字段
    uint8_t flags = 0b0000;  // 无序列号字段
    auto header = buildHeader(
        static_cast<uint8_t>(MessageType::FULL_CLIENT_REQUEST),
        flags,
        static_cast<uint8_t>(SerializationMethod::JSON),
        useCompression ? static_cast<uint8_t>(CompressionMethod::GZIP) : static_cast<uint8_t>(CompressionMethod::NONE),
        0  // sequence字段省略，服务器自动分配
    );

    result.insert(result.end(), header.begin(), header.end());

    // 添加payload大小（大端）
    uint32_t payloadSize = static_cast<uint32_t>(jsonRequest.length());
    writeUint32BigEndian(result, payloadSize);

    // 添加payload（JSON字符串）- 使用批量复制提高效率
    result.insert(result.end(),
                  reinterpret_cast<const uint8_t*>(jsonRequest.c_str()),
                  reinterpret_cast<const uint8_t*>(jsonRequest.c_str()) + jsonRequest.length());

    return result;
}

// 编码Audio Only Request消息
std::vector<uint8_t> BinaryProtocolEncoder::encodeAudioOnlyRequest(
    const uint8_t* audioData,
    size_t length,
    bool isLastChunk,
    bool useCompression,
    uint32_t sequence
) {
    // 输入验证：检查音频数据指针和长度
    if (audioData == nullptr || length == 0) {
        return std::vector<uint8_t>();
    }

    // 检查payload大小是否超过最大值
    if (length > MAX_PAYLOAD_SIZE) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> result;

    // 协议调试日志
#if PROTOCOL_DEBUG
#ifdef ARDUINO
    ESP_LOGI("BinaryProtocol", "encodeAudioOnlyRequest - length=%u, isLastChunk=%d, seq=%d, useCompression=%d",
             length, isLastChunk, (int32_t)sequence, useCompression);
#else
    printf("[BinaryProtocol] encodeAudioOnlyRequest - length=%u, isLastChunk=%d, seq=%d, useCompression=%d\n",
           length, isLastChunk, (int32_t)sequence, useCompression);
#endif
#endif

    // 预分配内存以提高效率：header + payload size + payload
    size_t estimatedSize = EXTENDED_HEADER_SIZE_BYTES + 4 + length;
    result.reserve(estimatedSize);

    // 设置flags：根据火山客服指导，audio only request flags只包含LAST_CHUNK标志
    uint8_t flags = 0b0000;
    if (isLastChunk) {
        flags |= FLAG_LAST_CHUNK; // 最后一包设置LAST_CHUNK标志（0b0010）
    }

    // 构建header
    auto header = buildHeader(
        static_cast<uint8_t>(MessageType::AUDIO_ONLY_REQUEST),
        flags,
        static_cast<uint8_t>(SerializationMethod::NONE), // 音频数据无需序列化
        useCompression ? static_cast<uint8_t>(CompressionMethod::GZIP) : static_cast<uint8_t>(CompressionMethod::NONE),
        sequence  // 传递序列号（从1递增，最后一包为负数）
    );

    result.insert(result.end(), header.begin(), header.end());

    // 添加payload大小（大端）
    writeUint32BigEndian(result, static_cast<uint32_t>(length));

    // 添加payload（音频数据）- 使用安全的缓冲区复制
    result.insert(result.end(), audioData, audioData + length);

    // 协议调试日志
#if PROTOCOL_DEBUG
#ifdef ARDUINO
    ESP_LOGI("BinaryProtocol", "encodeAudioOnlyRequest - encoded size=%u bytes (header=%u, payload=%u)",
             result.size(), header.size() + 4, length);
#else
    printf("[BinaryProtocol] encodeAudioOnlyRequest - encoded size=%u bytes (header=%u, payload=%u)\n",
           result.size(), header.size() + 4, length);
#endif
#endif

    return result;
}

// 构建协议头
std::vector<uint8_t> BinaryProtocolEncoder::buildHeader(
    uint8_t messageType,
    uint8_t flags,
    uint8_t serialization,
    uint8_t compression,
    uint32_t sequence
) {
    std::vector<uint8_t> header;

    // 确定头部大小：如果有序列号，使用扩展头部
    uint8_t headerSize = HEADER_SIZE_BYTES;
    bool hasSequence = sequence != 0; // 根据火山客服指导，序列号字段总是存在（当sequence != 0时）
    if (hasSequence) {
        headerSize = EXTENDED_HEADER_SIZE_BYTES;
    }

    // Byte 0: 版本(4 bits) + 头部大小(4 bits)
    // 协议约定：头部大小字段值 = 头部总字节数 / 4
    uint8_t headerSizeField = HEADER_SIZE_BYTES / 4; // 默认4字节头部 → 字段值1
    if (hasSequence) {
        headerSizeField = EXTENDED_HEADER_SIZE_BYTES / 4; // 8字节头部 → 字段值2
    }
    header.push_back(buildByte(PROTOCOL_VERSION, headerSizeField));

    // Byte 1: 消息类型(4 bits) + flags(4 bits)
    header.push_back(buildByte(messageType & 0x0F, flags & 0x0F));

    // Byte 2: 序列化方法(4 bits) + 压缩方法(4 bits)
    header.push_back(buildByte(serialization & 0x0F, compression & 0x0F));

    // Byte 3: 保留字节
    header.push_back(0x00);

    // 如果有序列号，添加序列号字段（4字节大端）
    if (hasSequence) {
        writeUint32BigEndian(header, sequence);
    }

    // 协议调试日志
#if PROTOCOL_DEBUG
    if (messageType == static_cast<uint8_t>(MessageType::AUDIO_ONLY_REQUEST)) {
#ifdef ARDUINO
        ESP_LOGI("BinaryProtocol", "buildHeader - type=AUDIO_ONLY, flags=0x%02X, seq=%d, headerSize=%u, hasSequence=%d",
                 flags, (int32_t)sequence, headerSize, hasSequence);
        // 输出头部字节
        char hexBuffer[100] = {0};
        char* ptr = hexBuffer;
        for (size_t i = 0; i < header.size() && i < 12; i++) {
            ptr += sprintf(ptr, "%02X ", header[i]);
        }
        ESP_LOGI("BinaryProtocol", "Header bytes: %s", hexBuffer);
#else
        printf("[BinaryProtocol] buildHeader - type=AUDIO_ONLY, flags=0x%02X, seq=%d, headerSize=%u, hasSequence=%d\n",
               flags, (int32_t)sequence, headerSize, hasSequence);
        printf("[BinaryProtocol] Header bytes: ");
        for (size_t i = 0; i < header.size() && i < 12; i++) {
            printf("%02X ", header[i]);
        }
        printf("\n");
#endif
    }
#endif

    return header;
}

// 辅助函数：大端写入32位整数
void BinaryProtocolEncoder::writeUint32BigEndian(std::vector<uint8_t>& buffer, uint32_t value) {
    // 安全地添加4个字节
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

// 辅助函数：构建字节（高4位+低4位）
uint8_t BinaryProtocolEncoder::buildByte(uint8_t highBits, uint8_t lowBits) {
    // 确保只使用低4位
    return ((highBits & 0x0F) << 4) | (lowBits & 0x0F);
}