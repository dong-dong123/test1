#include <Arduino.h>
#include <unity.h>
#include <ArduinoJson.h>

// 测试Unicode字符解析
void test_unicode_emoji_parsing(void) {
    // 测试包含表情符号的JSON
    String jsonWithEmoji = R"({
        "messages": [
            {
                "content": "",
                "type": "user"
            },
            {
                "content": "☀️ 「成都」的天气情况来啦！今天成都天气晴朗，气温15-25°C，适合外出。",
                "type": "tool"
            },
            {
                "content": "🌤️ 成都今日天气播报来咯：今天成都天气晴朗，气温15-25°C，适合外出活动。建议穿轻薄外套。",
                "type": "ai"
            }
        ]
    })";

    Serial.println("Testing Unicode emoji parsing...");
    Serial.printf("JSON length: %d bytes\n", jsonWithEmoji.length());

    // 检查字符串长度（UTF-8字节数）
    Serial.printf("Tool message length: %d bytes\n",
        strlen("☀️ 「成都」的天气情况来啦！今天成都天气晴朗，气温15-25°C，适合外出。"));
    Serial.printf("AI message length: %d bytes\n",
        strlen("🌤️ 成都今日天气播报来咯：今天成都天气晴朗，气温15-25°C，适合外出活动。建议穿轻薄外套。"));

    // 尝试解析
    DynamicJsonDocument doc(8192); // 使用更大的缓冲区
    DeserializationError error = deserializeJson(doc, jsonWithEmoji);

    if (error) {
        Serial.printf("JSON parsing failed: %s\n", error.c_str());
        TEST_FAIL_MESSAGE("Failed to parse JSON with Unicode emoji");
        return;
    }

    TEST_ASSERT_TRUE(doc.containsKey("messages"));
    TEST_ASSERT_TRUE(doc["messages"].is<JsonArray>());

    JsonArray messages = doc["messages"];
    TEST_ASSERT_EQUAL(3, messages.size());

    // 检查第二个消息（tool）
    JsonObject toolMsg = messages[1];
    TEST_ASSERT_TRUE(toolMsg.containsKey("content"));
    TEST_ASSERT_TRUE(toolMsg.containsKey("type"));

    String toolType = toolMsg["type"] | "";
    String toolContent = toolMsg["content"] | "";

    TEST_ASSERT_EQUAL_STRING("tool", toolType.c_str());
    TEST_ASSERT_FALSE(toolContent.isEmpty());

    Serial.printf("Tool message type: %s\n", toolType.c_str());
    Serial.printf("Tool message content (first 50 chars): ");
    for (int i = 0; i < 50 && i < toolContent.length(); i++) {
        Serial.printf("%02x ", (uint8_t)toolContent[i]);
    }
    Serial.println();

    // 检查第三个消息（ai）
    JsonObject aiMsg = messages[2];
    TEST_ASSERT_TRUE(aiMsg.containsKey("content"));
    TEST_ASSERT_TRUE(aiMsg.containsKey("type"));

    String aiType = aiMsg["type"] | "";
    String aiContent = aiMsg["content"] | "";

    TEST_ASSERT_EQUAL_STRING("ai", aiType.c_str());
    TEST_ASSERT_FALSE(aiContent.isEmpty());

    Serial.printf("AI message type: %s\n", aiType.c_str());
    Serial.printf("AI message length: %d characters, %d bytes\n",
                  aiContent.length(), aiContent.length());

    // 检查是否包含中文字符
    TEST_ASSERT_TRUE(aiContent.indexOf("成都") >= 0);
    TEST_ASSERT_TRUE(aiContent.indexOf("15-25") >= 0);

    Serial.println("Unicode emoji parsing test passed!");
}

void test_json_buffer_size(void) {
    // 测试不同缓冲区大小的影响
    String testJson = R"({
        "messages": [
            {
                "content": "这是一个测试消息，包含一些文本。",
                "type": "user"
            },
            {
                "content": "这是AI的回复，也包含一些文本内容。",
                "type": "ai"
            }
        ]
    })";

    Serial.println("\nTesting JSON buffer sizes...");

    // 测试太小缓冲区
    DynamicJsonDocument smallDoc(256);
    DeserializationError error1 = deserializeJson(smallDoc, testJson);
    if (error1) {
        Serial.printf("Small buffer (256) failed: %s\n", error1.c_str());
    } else {
        Serial.println("Small buffer (256) succeeded");
    }

    // 测试适当缓冲区
    DynamicJsonDocument mediumDoc(1024);
    DeserializationError error2 = deserializeJson(mediumDoc, testJson);
    if (error2) {
        Serial.printf("Medium buffer (1024) failed: %s\n", error2.c_str());
        TEST_FAIL_MESSAGE("1024 buffer should be enough");
    } else {
        Serial.println("Medium buffer (1024) succeeded");
    }

    // 测试大缓冲区
    DynamicJsonDocument largeDoc(8192);
    DeserializationError error3 = deserializeJson(largeDoc, testJson);
    if (error3) {
        Serial.printf("Large buffer (8192) failed: %s\n", error3.c_str());
        TEST_FAIL_MESSAGE("8192 buffer should definitely work");
    } else {
        Serial.println("Large buffer (8192) succeeded");
    }
}

void test_utf8_character_handling(void) {
    // 测试UTF-8字符处理
    Serial.println("\nTesting UTF-8 character handling...");

    // 各种UTF-8字符
    const char* testStrings[] = {
        "Hello World",  // ASCII
        "你好世界",      // 中文
        "🌤️☀️",         // 表情符号
        "15-25°C",      // 度符号
        "「成都」",      // 中文标点
    };

    for (int i = 0; i < 5; i++) {
        const char* str = testStrings[i];
        String jsonStr = "{\"text\":\"" + String(str) + "\"}";

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, jsonStr);

        if (error) {
            Serial.printf("UTF-8 test %d failed: %s\n", i, error.c_str());
            Serial.printf("String: %s\n", str);
            Serial.printf("JSON: %s\n", jsonStr.c_str());
            TEST_FAIL_MESSAGE("UTF-8 parsing failed");
        } else {
            String extracted = doc["text"].as<String>();
            Serial.printf("UTF-8 test %d passed: length=%d bytes\n", i, extracted.length());
        }
    }
}

void setup() {
    delay(2000);
    Serial.begin(115200);
    Serial.println("Starting Unicode parsing tests...");

    UNITY_BEGIN();

    RUN_TEST(test_unicode_emoji_parsing);
    RUN_TEST(test_json_buffer_size);
    RUN_TEST(test_utf8_character_handling);

    UNITY_END();
}

void loop() {
    // Empty
}