#include <Arduino.h>
#include "HardwareTestSuite.h"

HardwareTestSuite testSuite;

void setup() {
    // 初始化串口
    Serial.begin(115200);
    delay(2000); // 等待USB枚举

    Serial.println("\n");
    Serial.println("========================================");
    Serial.println("=== HARDWARE INTEGRATION TEST SUITE ===");
    Serial.println("========================================");
    Serial.println("System: Xiaozhi Voice Assistant Hardware Test");
    Serial.println("Board: ESP32-S3 Development Kit");
    Serial.println("Date: " + String(__DATE__) + " " + String(__TIME__));
    Serial.println("========================================\n");

    // 打印欢迎信息
    Serial.println("This test suite will verify all hardware components:");
    Serial.println("1. Audio System (INMP441 Microphone + MAX98357A Speaker)");
    Serial.println("2. Display System (ST7789 TFT LCD 240x320)");
    Serial.println("3. Button Input (GPIO0 BOOT button)");
    Serial.println("4. System Integration and Performance");
    Serial.println();

    // 等待用户确认
    Serial.println("Press the BOOT button (GPIO0) to begin testing...");
    Serial.println("Or send 'start' via serial to begin immediately.");
    Serial.println();

    // 等待按钮按下或串口命令
    bool startTest = false;
    uint32_t startTime = millis();

    while (!startTest && millis() - startTime < 30000) {
        // 检查按钮
        if (digitalRead(0) == LOW) {
            startTest = true;
            Serial.println("Button pressed, starting tests...");
            delay(500); // 去抖动
        }

        // 检查串口命令
        if (Serial.available()) {
            String command = Serial.readStringUntil('\n');
            command.trim();
            if (command == "start" || command == "begin") {
                startTest = true;
                Serial.println("Serial command received, starting tests...");
            }
        }

        delay(10);
    }

    if (!startTest) {
        Serial.println("Timeout, starting tests automatically...");
    }

    Serial.println("\n" + String('=', 50));

    // 运行完整测试套件
    testSuite.runAllTests();

    // 测试完成
    Serial.println("\n" + String('=', 50));
    Serial.println("=== TESTING COMPLETE ===");

    // 显示最终结果
    uint32_t total = testSuite.getTotalTests();
    uint32_t passed = testSuite.getPassedTests();
    uint32_t failed = testSuite.getFailedTests();
    float passRate = testSuite.getPassRate();

    Serial.println("Final Results:");
    Serial.println("  Total Tests: " + String(total));
    Serial.println("  Passed: " + String(passed));
    Serial.println("  Failed: " + String(failed));
    Serial.println("  Pass Rate: " + String(passRate, 1) + "%");

    if (failed == 0) {
        Serial.println("\n✅ ALL TESTS PASSED!");
        Serial.println("Hardware is fully functional and ready for use.");
    } else {
        Serial.println("\n⚠️ SOME TESTS FAILED");
        Serial.println("Please check the failed tests above.");
        Serial.println("Some hardware features may not work correctly.");
    }

    Serial.println("\nCommands available after testing:");
    Serial.println("  'results' - Show detailed test results");
    Serial.println("  'audio'   - Run audio tests only");
    Serial.println("  'display' - Run display tests only");
    Serial.println("  'restart' - Restart the test suite");
    Serial.println("  'info'    - Show system information");
    Serial.println("  'help'    - Show this help");

    Serial.println("\nSystem will continue running for command input.");
    Serial.println("Press RESET button to restart the test suite.");
    Serial.println("========================================\n");
}

void loop() {
    // 处理串口命令
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        Serial.println("Command: " + command);

        if (command == "results" || command == "status") {
            // 显示详细结果
            const auto& results = testSuite.getResults();
            Serial.println("\n=== DETAILED TEST RESULTS ===");
            for (const auto& result : results) {
                Serial.print(result.passed ? "[PASS] " : "[FAIL] ");
                Serial.print(result.testName);
                Serial.print(" - ");
                Serial.print(result.message);
                Serial.print(" (");
                Serial.print(result.executionTime);
                Serial.print("ms)");

                int32_t memChange = result.getMemoryChange();
                if (memChange != 0) {
                    Serial.print(" [Memory: ");
                    Serial.print(memChange);
                    Serial.print(" bytes]");
                }
                Serial.println();
            }
            Serial.println("==============================\n");

        } else if (command == "audio") {
            Serial.println("\n=== RUNNING AUDIO TESTS ===");
            testSuite.runAudioTests();

        } else if (command == "display") {
            Serial.println("\n=== RUNNING DISPLAY TESTS ===");
            testSuite.runDisplayTests();

        } else if (command == "restart" || command == "reset") {
            Serial.println("\n=== RESTARTING TEST SUITE ===");
            ESP.restart();

        } else if (command == "info" || command == "system") {
            HardwareTestSuite::printSystemInfo();
            HardwareTestSuite::printPinConfiguration();

        } else if (command == "help") {
            Serial.println("\nAvailable commands:");
            Serial.println("  results  - Show detailed test results");
            Serial.println("  audio    - Run audio tests only");
            Serial.println("  display  - Run display tests only");
            Serial.println("  restart  - Restart the test suite");
            Serial.println("  info     - Show system information");
            Serial.println("  help     - Show this help");
            Serial.println();

        } else if (command == "mem" || command == "memory") {
            uint32_t freeHeap = esp_get_free_heap_size();
            uint32_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            uint32_t minHeap = esp_get_minimum_free_heap_size();

            Serial.println("\n=== MEMORY STATUS ===");
            Serial.println("Free Heap: " + String(freeHeap) + " bytes");
            Serial.println("Free PSRAM: " + String(freePsram) + " bytes");
            Serial.println("Min Free Heap: " + String(minHeap) + " bytes");
            Serial.println("Heap Fragmentation: " +
                String(100.0f - (freeHeap * 100.0f / minHeap), 1) + "%");
            Serial.println();

        } else if (command == "pin" || command == "pins") {
            HardwareTestSuite::printPinConfiguration();

        } else if (command == "test" || command == "testall") {
            Serial.println("\n=== RUNNING COMPLETE TEST SUITE ===");
            testSuite.runAllTests();

        } else if (!command.isEmpty()) {
            Serial.println("Unknown command: " + command);
            Serial.println("Type 'help' for available commands");
        }
    }

    // 定期显示状态（每30秒）
    static uint32_t lastStatusTime = 0;
    if (millis() - lastStatusTime > 30000) {
        uint32_t freeHeap = esp_get_free_heap_size();
        uint32_t uptime = millis() / 1000;

        Serial.println("[Status] Uptime: " + String(uptime) + "s, " +
                       "Free Heap: " + String(freeHeap) + " bytes");
        lastStatusTime = millis();
    }

    delay(100);
}

// 串口事件处理（备用）
void serialEvent() {
    // 主循环中已经处理串口命令
    // 这个函数保持为空以确保兼容性
}