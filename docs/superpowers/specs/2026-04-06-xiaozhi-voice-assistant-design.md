# 小智对话机器人设计规范

**版本**：1.0  
**日期**：2026-04-06  
**状态**：草案  
**作者**：Claude Code  

## 1. 项目概述

### 1.1 项目目标
构建一个基于ESP32-S3的智能语音对话机器人，具备自然语音交互、云端智能处理和本地唤醒词检测功能。

### 1.2 核心功能
- 语音唤醒功能（可选唤醒词"小智小智"）
- 本地语音活动检测（VAD）
- 云端语音识别（火山引擎豆包流式语音识别）
- 智能对话（Coze智能体）
- 语音合成（火山引擎豆包语音合成）
- 显示屏交互反馈（ST7789）
- 模块化服务架构，支持多服务切换
- 完整的日志记录和错误处理

### 1.3 技术栈
- **硬件平台**：ESP32-S3-DevKitC-1
- **音频硬件**：INMP441麦克风 + MAX98357A扬声器
- **显示硬件**：ST7789 SPI显示屏
- **云端服务**：
  - 语音识别：火山引擎豆包流式语音识别大模型
  - 语音合成：火山引擎豆包语音合成大模型  
  - 对话引擎：Coze智能体
- **开发框架**：Arduino Framework (PlatformIO)
- **通信协议**：Wi-Fi, HTTP/REST, WebSocket, I2S音频

## 2. 硬件配置

### 2.1 引脚分配表

| 设备 | 模块引脚 | 功能 | ESP32-S3引脚 | 备注 |
|------|----------|------|--------------|------|
| **ST7789屏幕** | VCC | 电源正 | 3V3 | |
| | GND | 电源地 | GND | |
| | SCL | SPI时钟 | GPIO 12 | 硬件SPI2(HSPI)默认SCLK引脚 |
| | SDA | SPI数据 | GPIO 11 | 硬件SPI2(HSPI)默认MOSI引脚 |
| | RES | 复位 | GPIO 5 | 普通GPIO控制 |
| | DC | 数据/命令 | GPIO 4 | 普通GPIO控制 |
| | CS | 片选 | GPIO 10 | 硬件SPI2(HSPI)默认CS引脚 |
| | BLK | 背光 | 3V3 | 常亮，或接GPIO做PWM调光 |
| **INMP441麦克风** | VDD | 电源正 | 3V3 | |
| | GND | 电源地 | GND | |
| | SD | 数据输出 | GPIO 14 | I2S数据输入 |
| | WS | 帧时钟 | GPIO 16 | 与扬声器共用 |
| | SCK | 位时钟 | GPIO 15 | 与扬声器共用 |
| | L/R | 左右声道 | GND | 接GND选择左声道 |
| **MAX98357A扬声器** | VIN | 电源正 | 5V | 建议5V获得更大音量 |
| | GND | 电源地 | GND | |
| | DIN | 数据输入 | GPIO 7 | I2S数据输出 |
| | BCLK | 位时钟 | GPIO 15 | 与麦克风共用 |
| | LRC | 帧时钟 | GPIO 16 | 与麦克风共用 |
| | SD_MODE | 关断控制 | 3V3 | 接高电平使能 |
| | GAIN | 增益设置 | GND | 接GND设置12dB增益 |
| | OUT+ | 喇叭正极 | 喇叭红线 | |
| | OUT- | 喇叭负极 | 喇叭黑线 | |

### 2.2 硬件初始化顺序
1. GPIO引脚初始化
2. I2S音频系统初始化（麦克风+扬声器）
3. SPI显示屏初始化
4. Wi-Fi模块初始化

## 3. 系统架构

### 3.1 模块化架构

```
┌─────────────────────────────────────────────────────────┐
│                    MainController                       │
│   (状态机管理、模块协调、错误恢复)                       │
└────────────┬──────────────────────┬────────────────────┘
             │                      │
    ┌────────▼────────┐   ┌─────────▼─────────┐
    │ AudioProcessor  │   │   NetworkManager  │
    │ - 音频采集      │   │ - Wi-Fi连接       │
    │ - VAD检测       │   │ - API客户端       │
    │ - 唤醒词识别    │   │ - 音频流传输      │
    │ - 音频播放      │   │ - 服务健康检查    │
    └─────────────────┘   └───────────────────┘
             │                      │
    ┌────────▼────────┐   ┌─────────▼─────────┐
    │   UIManager     │   │ ServiceManager    │
    │ - 显示屏驱动    │   │ - 多服务配置      │
    │ - 状态显示      │   │ - 服务切换        │
    │ - 对话历史      │   │ - API密钥管理     │
    │ - 波形可视化    │   └───────────────────┘
    └─────────────────┘             │
             │             ┌────────▼─────────┐
             │             │     Logger       │
             └─────────────► - 日志记录       │
                           │ - 内存缓冲       │
                           │ - 串口调试       │
                           └───────────────────┘
```

### 3.2 核心接口定义

```cpp
// 语音服务接口
class SpeechService {
public:
    virtual bool recognize(const uint8_t* audio_data, size_t length, String& text) = 0;
    virtual bool synthesize(const String& text, uint8_t*& audio_data, size_t& length) = 0;
    virtual String getName() const = 0;
    virtual bool isAvailable() const = 0;
};

// 对话服务接口
class DialogueService {
public:
    virtual String chat(const String& input, const String& context = "") = 0;
    virtual String getName() const = 0;
    virtual bool isAvailable() const = 0;
};

// 日志接口
class Logger {
public:
    enum Level { DEBUG, INFO, WARN, ERROR };
    virtual void log(Level level, const String& message) = 0;
    virtual void flush() = 0;
};

// 配置管理接口
class ConfigManager {
public:
    virtual bool loadFromFile(const String& filename) = 0;
    virtual bool saveToFile(const String& filename) = 0;
    virtual String getString(const String& key, const String& defaultValue = "") = 0;
    virtual int getInt(const String& key, int defaultValue = 0) = 0;
    virtual bool setString(const String& key, const String& value) = 0;
    virtual bool setInt(const String& key, int value) = 0;
};
```

## 4. 详细设计

### 4.1 状态机设计

```cpp
enum SystemState {
    STATE_BOOTING,      // 系统启动中
    STATE_IDLE,         // 空闲状态，等待唤醒
    STATE_LISTENING,    // 监听麦克风
    STATE_PROCESSING,   // 音频处理中（VAD/唤醒词）
    STATE_RECOGNIZING,  // 语音识别中
    STATE_THINKING,     // AI思考中
    STATE_SYNTHESIZING, // 语音合成中
    STATE_PLAYING,      // 播放音频
    STATE_ERROR,        // 错误状态
    STATE_CONFIGURING   // 配置模式（通过Web）
};

enum StateTransition {
    TRANSITION_BOOT_COMPLETE,
    TRANSITION_WAKE_DETECTED,
    TRANSITION_VAD_DETECTED,
    TRANSITION_RECOGNITION_START,
    TRANSITION_RECOGNITION_COMPLETE,
    TRANSITION_DIALOGUE_COMPLETE,
    TRANSITION_SYNTHESIS_COMPLETE,
    TRANSITION_PLAYBACK_COMPLETE,
    TRANSITION_ERROR_OCCURRED,
    TRANSITION_RETRY
};
```

### 4.2 配置数据结构

```json
{
  "wifi": {
    "ssid": "YourWiFiSSID",
    "password": "YourWiFiPassword",
    "autoConnect": true,
    "timeout": 10000
  },
  
  "services": {
    "speech": {
      "default": "volcano",
      "available": ["volcano", "baidu", "tencent"],
      "volcano": {
        "apiKey": "your_volcano_api_key",
        "secretKey": "your_volcano_secret",
        "endpoint": "https://speech.volcengineapi.com",
        "language": "zh-CN"
      },
      "baidu": {
        "appId": "your_baidu_app_id",
        "apiKey": "your_baidu_api_key",
        "secretKey": "your_baidu_secret",
        "endpoint": "https://vop.baidu.com"
      }
    },
    
    "dialogue": {
      "default": "coze",
      "available": ["coze", "openai"],
      "coze": {
        "botId": "your_coze_bot_id",
        "apiKey": "your_coze_api_key",
        "endpoint": "https://api.coze.cn",
        "personality": "friendly_assistant"
      }
    }
  },
  
  "audio": {
    "sampleRate": 16000,
    "bitsPerSample": 16,
    "channels": 1,
    "vadThreshold": 0.3,
    "wakeWord": "小智小智",
    "wakeWordSensitivity": 0.8,
    "volume": 80
  },
  
  "display": {
    "brightness": 100,
    "timeout": 30000,
    "showWaveform": true,
    "showHistory": true,
    "maxHistoryLines": 10
  },
  
  "logging": {
    "level": "INFO",
    "output": ["serial"]
  }
}
```

### 4.3 数据流处理

```
音频采集 (16kHz, 16bit, 单声道)
    ↓
[AudioProcessor] 实时VAD检测 + 唤醒词识别
    ↓（检测到有效语音）
分段音频缓存（最大10秒）
    ↓  
[NetworkManager] 编码为PCM/WAV格式，WebSocket流式上传
    ↓
火山引擎豆包流式语音识别API
    ↓（返回识别文本）
[ServiceManager] 转发文本到Coze智能体
    ↓（返回回复文本）  
[NetworkManager] 通过WebSocket请求火山引擎豆包语音合成API
    ↓（返回PCM音频流）
[AudioProcessor] I2S流式播放
    ↓
[UIManager] 实时更新屏幕状态
```

**关键优化**：
- 音频流式处理，避免大内存占用
- 识别结果分段返回，降低延迟
- 合成音频流式播放，边下边播
- UI状态实时反馈

## 4.4 网络连接管理

### WiFiManager智能配网

系统集成了**WiFiManager**库，提供智能Wi-Fi连接和配置功能：

1. **智能连接流程**：
   - 首先尝试连接之前保存的Wi-Fi网络
   - 如果连接失败，自动启动AP模式（SSID: `Xiaozhi-Voice-Assistant`）
   - 用户通过手机/电脑连接AP并访问配置页面（通常为`192.168.4.1`）
   - 选择可用Wi-Fi网络并输入密码
   - 设备自动重启并连接配置的网络

2. **配置管理**：
   - WiFiManager将凭据保存在ESP32的NVS（非易失性存储）中
   - 系统配置文件（`config.json`）中的Wi-Fi设置作为初始默认值
   - 成功连接后，SSID会自动更新到配置文件

3. **网络事件处理**：
   - 完整的Wi-Fi事件回调系统（连接、断开、获取IP等）
   - 自动重连机制（默认启用）
   - 详细的断开原因诊断日志

4. **与传统模式的兼容**：
   - 支持通过`useWiFiManager`配置开关控制
   - 传统模式：直接使用配置文件中的SSID/密码连接
   - 智能模式：使用WiFiManager进行智能配网

### 网络管理器（NetworkManager）架构

```cpp
class NetworkManager {
    // 核心功能：
    // 1. Wi-Fi连接管理（支持WiFiManager和传统模式）
    // 2. HTTP客户端池（连接复用，性能优化）
    // 3. 网络事件系统（状态变更通知）
    // 4. 自动重连机制（可配置重试策略）
    // 5. 连接诊断工具（ping、网络测试）
};
```

## 5. 错误处理策略

### 5.1 错误分级

```cpp
enum ErrorLevel {
    ERROR_WARNING,     // 可恢复警告（如网络波动）
    ERROR_RECOVERABLE, // 可恢复错误（如API限流）
    ERROR_FATAL        // 致命错误（如硬件故障）
};
```

### 5.2 错误处理流程

```
错误发生 → 错误分类 → 用户反馈 → 恢复尝试
    ↓                      ↓
[Logger记录]        [UI显示错误信息]
    ↓                      ↓
决定恢复策略          用户知晓状态
    ↓
尝试恢复（最多3次）
    ↓
成功则继续，失败则进入安全模式
```

### 5.3 具体错误场景处理

| 错误类型 | 处理策略 | 用户反馈 |
|----------|----------|----------|
| 网络断开 | 自动重连（间隔递增） | "网络连接中..." |
| API限流 | 服务降级（切换到备用服务） | "正在切换服务..." |
| 识别失败 | 重新录音或文本输入 | "没听清，请再说一次" |
| 合成失败 | 文本显示代替语音 | "无法播放语音，请看屏幕" |
| 硬件错误 | 进入安全模式 | "硬件故障，错误码: XXX" |

## 6. 测试策略

### 6.1 单元测试（开发阶段）

- **模拟硬件接口测试**：不依赖实际硬件的接口模拟
- **服务接口Mock测试**：模拟API响应，测试各种场景
- **状态机逻辑测试**：验证所有状态转移路径
- **配置管理测试**：配置文件读写和验证

### 6.2 集成测试（硬件测试）

**阶段1：硬件模块独立测试**
1. 显示屏测试 - 显示测试图案、文本渲染
2. 麦克风测试 - 音频采集质量、VAD检测准确率
3. 扬声器测试 - 音频播放质量、音量控制
4. Wi-Fi测试 - 连接稳定性、重连机制

**阶段2：系统集成测试**
1. 端到端语音流程测试 - 完整语音交互流程
2. 网络服务切换测试 - 不同服务间的无缝切换
3. 错误恢复流程测试 - 模拟各种错误场景
4. 长时间稳定性测试 - 24小时连续运行

**阶段3：用户体验测试**
1. 唤醒词识别率测试 - 不同环境下的识别准确率
2. 语音识别准确率测试 - 多种语音内容和口音
3. 响应延迟测试 - 目标整体延迟<3秒
4. 多轮对话连贯性测试 - 上下文保持能力

### 6.3 调试工具

- **串口调试控制台**：实时命令交互、状态查询
- **动态日志级别**：运行时调整日志详细程度
- **网络抓包集成**：HTTP请求/响应监控
- **性能监控**：内存使用率、CPU负载、网络延迟

## 7. 部署与维护

### 7.1 固件更新机制

- **OTA更新**：通过Wi-Fi远程更新固件
- **配置备份**：更新前自动备份用户配置
- **回滚机制**：更新失败自动回滚到前一版本

### 7.2 监控与维护

- **健康检查**：定期自检硬件和网络状态
- **使用统计**：记录交互次数、成功率等数据
- **远程诊断**：支持远程日志收集和问题诊断

## 8. 未来扩展

### 8.1 功能扩展点

1. **离线语音识别**：集成本地语音识别模型（如VOSK）
2. **多语言支持**：扩展支持英语、日语等其他语言
3. **技能扩展**：通过插件机制增加新功能（天气、新闻等）
4. **多设备协同**：支持多个设备间的协同工作

### 8.2 性能优化方向

1. **边缘计算**：部分AI模型本地化运行
2. **音频压缩**：优化音频传输带宽
3. **缓存策略**：常用对话结果的本地缓存
4. **电源管理**：低功耗模式优化

## 9. 项目时间线

### 阶段1：基础框架搭建（1-2周）
- 硬件驱动开发（I2S音频、SPI显示屏）
- 模块化架构实现
- 基础配置管理系统

### 阶段2：核心功能实现（2-3周）
- 音频处理流水线（VAD+唤醒词）
- 网络服务集成（火山引擎+Coze）
- 用户界面开发

### 阶段3：优化与测试（1-2周）
- 错误处理和恢复机制
- 性能优化和调试
- 完整测试套件

### 阶段4：部署与文档（1周）
- 用户文档编写
- 示例配置和快速入门指南
- 最终版本发布

---

**设计评审清单**：
- [ ] 硬件引脚配置完整且正确
- [ ] 模块接口定义清晰
- [ ] 数据流程合理可行
- [ ] 错误处理覆盖全面
- [ ] 测试策略充分
- [ ] 扩展性考虑周全

**下一步**：
1. 用户审核本设计规范
2. 根据反馈进行调整
3. 创建详细实施计划
4. 开始模块化开发