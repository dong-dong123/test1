# loopTask栈溢出根本分析

## 错误现象
`Guru Meditation Error: Core 1 panic'ed (Unhandled debug exception). Debug exception reason: Stack canary watchpoint triggered (loopTask)`

发生在 TTS HTTP POST (`https://openspeech.bytedance.com/api/v1/tts`) 期间。

## 根因分析

### 根因1: CONFIG_ARDUINO_LOOP_STACK_SIZE 未生效

`sdkconfig.defaults` 中设置了 `CONFIG_ARDUINO_LOOP_STACK_SIZE=122880`，但 PlatformIO 的预编译框架不处理 `sdkconfig.defaults` 中的这个选项，导致 loopTask 实际使用默认的 **4096字节** 栈。

**证据**: addr2line 解码显示调用链只用了 ~8KB 栈帧。如果 120KB 生效不可能溢出。

### 根因2: SDK配置被预编译sdkconfig.h覆盖

platformio.ini 中的 build_flags:
```c
-DCONFIG_MBEDTLS_ECP_C=0
-DCONFIG_MBEDTLS_ECDH_C=0
-DCONFIG_MBEDTLS_SHA512_C=0
```

均被预编译的 `sdkconfig.h` 覆盖为 `=1`：
```
sdkconfig.h:610: warning: "CONFIG_MBEDTLS_ECP_C" redefined
 #define CONFIG_MBEDTLS_ECP_C 1
```

导致崩溃调用链中出现 SHA512 和 ECP/ECDH 代码路径。

### 根因3: HTTPS请求嵌套在WebSocket回调中

崩溃时的调用链深度高达 **60+帧**：
```
Arduino loop → serviceManager.update()
  → WebSocket库消息循环 (3层嵌套)
    → 识别完成回调 → handleAsyncRecognitionResult()
      → dialogue->chat()  ← HTTPS (SSL) +10KB栈
      → playResponse() → synthesize() ← HTTPS (SSL) 又 +10KB栈
```

两个连续的 HTTPS 请求不释放栈帧就叠加，加上 WebSocket 库自身的栈帧，远超 4KB 默认栈。

## 修复方案

### 修复1: build_flags 强制设置栈大小

`platformio.ini`:
```ini
-DCONFIG_ARDUINO_LOOP_STACK_SIZE=32768
```
不从 sdkconfig 走，直接编译器 -D 定义，确保生效。

### 修复2: 打破回调嵌套

异步识别回调 (`handleAsyncRecognitionResult`) 不再直接调用 dialogue 和 synthesis，只保存结果并设置标志位：

```
旧: callback → dialogue HTTP → synthesis HTTP → 返回
新: callback → 保存结果 → 立即返回（释放WebSocket栈帧）
    主循环下轮 → handleState(THINKING) → dialogue HTTP
    主循环再下轮 → handleState(SYNTHESIZING) → synthesis HTTP
```

新增方法:
- `processPendingDialogue()` — 主循环中执行对话
- `processPendingSynthesis()` — 主循环中执行合成

同步识别路径同样修复。

### 修复3: 清理无效的build_flags

移除被 sdkconfig.h 覆盖的 `-DCONFIG_MBEDTLS_*` 标志，避免混淆。

## 文件修改

| 文件 | 修改 |
|------|------|
| `platformio.ini` | 添加 `-DCONFIG_ARDUINO_LOOP_STACK_SIZE=32768` |
| `MainApplication.h` | 新增 `pendingRecognitionText`, `pendingDialogue`, `pendingSynthesisText`, `pendingSynthesis` 成员变量和 `processPendingDialogue()`, `processPendingSynthesis()` 方法 |
| `MainApplication.cpp` | 重写 `handleAsyncRecognitionResult()` 延迟处理；修改 `handleState()` THINKING/SYNTHESIZING 分支；修改同步识别路径；构造函数初始化新标志 |
