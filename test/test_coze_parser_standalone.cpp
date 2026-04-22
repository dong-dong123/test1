#include <Arduino.h>
#include <unity.h>
#include <ArduinoJson.h>

// 复制CozeDialogueService::parseChatResponse的逻辑，但作为独立函数
bool testParseChatResponse(const String& jsonResponse, String& output) {
    // 解析Coze API响应JSON
    // 支持多种响应格式：
    // 1. 根级别messages数组格式（coze.site API）: {"messages": [{"type": "ai", "content": "..."}, ...]}
    // 2. 标准Coze API格式: {"code": 0, "data": {"messages": [...]}}
    // 3. 其他常见格式

    DynamicJsonDocument responseDoc(4096);
    DeserializationError error = deserializeJson(responseDoc, jsonResponse);

    if (error) {
        Serial.printf("Failed to parse Coze response JSON: %s\n", error.c_str());
        output = "Error parsing JSON: " + String(error.c_str());
        return false;
    }

    // 检查错误码
    if (responseDoc.containsKey("code") && responseDoc["code"].as<int>() != 0) {
        String errorMsg = responseDoc["msg"] | responseDoc["message"] | "Unknown error";
        int errorCode = responseDoc["code"] | -1;
        Serial.printf("Coze API error %d: %s\n", errorCode, errorMsg.c_str());
        output = "API Error " + String(errorCode) + ": " + errorMsg;
        return false;
    }

    // 首先检查根级别的messages数组（coze.site API格式）
    if (responseDoc.containsKey("messages") && responseDoc["messages"].is<JsonArray>()) {
        JsonArray messages = responseDoc["messages"];
        Serial.printf("Found root-level messages array with %d messages\n", messages.size());

        for (JsonObject msg : messages) {
            String type = msg["type"] | "";
            String content = msg["content"] | "";
            String role = msg["role"] | "";

            Serial.printf("Message: type=%s, role=%s, content length=%d\n",
                         type.c_str(), role.c_str(), content.length());

            // 查找type为"ai"或role为"assistant"的消息
            if ((type == "ai" || role == "assistant") && !content.isEmpty()) {
                output = content;
                Serial.printf("Extracted AI reply from root-level messages: %s\n", output.c_str());
                return true;
            }
        }

        // 如果没有找到type为"ai"的消息，尝试查找第一个非空content
        for (JsonObject msg : messages) {
            String content = msg["content"] | "";
            if (!content.isEmpty()) {
                output = content;
                Serial.printf("Extracted first non-empty content from root-level messages: %s\n", output.c_str());
                return true;
            }
        }
    }

    // 然后检查标准Coze API格式（data字段）
    if (responseDoc.containsKey("data")) {
        JsonObject data = responseDoc["data"];

        // 尝试从messages数组提取回复
        if (data.containsKey("messages") && data["messages"].is<JsonArray>()) {
            JsonArray messages = data["messages"];
            for (JsonObject msg : messages) {
                String role = msg["role"] | "";
                String type = msg["type"] | "";
                String content = msg["content"] | "";

                if ((role == "assistant" || type == "answer") && !content.isEmpty()) {
                    output = content;
                    Serial.printf("Extracted assistant reply from data.messages: %s\n", output.c_str());
                    return true;
                }
            }
        }

        // 备用：直接查找reply字段
        if (data.containsKey("reply")) {
            output = data["reply"].as<String>();
            Serial.printf("Extracted reply field: %s\n", output.c_str());
            return true;
        }

        // 备用：查找content字段
        if (data.containsKey("content")) {
            output = data["content"].as<String>();
            Serial.printf("Extracted content field: %s\n", output.c_str());
            return true;
        }
    }

    // 其他可能的响应格式
    if (responseDoc.containsKey("choices") && responseDoc["choices"].is<JsonArray>()) {
        JsonArray choices = responseDoc["choices"];
        if (choices.size() > 0) {
            JsonObject choice = choices[0];
            if (choice.containsKey("message") && choice["message"].containsKey("content")) {
                output = choice["message"]["content"].as<String>();
                Serial.printf("Extracted from choices: %s\n", output.c_str());
                return true;
            }
        }
    }

    // 如果没有找到标准格式，尝试直接提取文本字段
    if (responseDoc.containsKey("text")) {
        output = responseDoc["text"].as<String>();
        Serial.printf("Extracted text field: %s\n", output.c_str());
        return true;
    }

    // 最后手段：返回原始响应（调试用）
    output = jsonResponse.substring(0, 200); // 截断长响应
    Serial.printf("Could not extract standard response, returning raw: %s\n", output.c_str());
    return false;
}

// 测试函数
void test_parse_root_level_messages_array(void) {
    // 测试根级别messages数组格式（coze.site API格式）
    String jsonResponse = R"({
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

    String output;
    bool result = testParseChatResponse(jsonResponse, output);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(output.isEmpty());
    TEST_ASSERT_EQUAL_STRING("🌤️ 成都今日天气播报来咯：今天成都天气晴朗，气温15-25°C，适合外出活动。建议穿轻薄外套。", output.c_str());
}

void test_parse_root_level_messages_with_role(void) {
    // 测试根级别messages数组，使用role字段而不是type
    String jsonResponse = R"({
        "messages": [
            {
                "content": "Hello",
                "role": "user"
            },
            {
                "content": "Hi there! How can I help you?",
                "role": "assistant"
            }
        ]
    })";

    String output;
    bool result = testParseChatResponse(jsonResponse, output);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(output.isEmpty());
    TEST_ASSERT_EQUAL_STRING("Hi there! How can I help you?", output.c_str());
}

void test_parse_standard_coze_api_format(void) {
    // 测试标准Coze API格式：{"code": 0, "data": {"messages": [...]}}
    String jsonResponse = R"({
        "code": 0,
        "msg": "success",
        "data": {
            "messages": [
                {
                    "role": "assistant",
                    "content": "这是标准Coze API的响应"
                }
            ]
        }
    })";

    String output;
    bool result = testParseChatResponse(jsonResponse, output);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(output.isEmpty());
    TEST_ASSERT_EQUAL_STRING("这是标准Coze API的响应", output.c_str());
}

void test_parse_coze_api_with_reply_field(void) {
    // 测试Coze API的data.reply字段
    String jsonResponse = R"({
        "code": 0,
        "msg": "success",
        "data": {
            "reply": "这是reply字段的响应"
        }
    })";

    String output;
    bool result = testParseChatResponse(jsonResponse, output);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(output.isEmpty());
    TEST_ASSERT_EQUAL_STRING("这是reply字段的响应", output.c_str());
}

void test_parse_coze_api_with_content_field(void) {
    // 测试Coze API的data.content字段
    String jsonResponse = R"({
        "code": 0,
        "msg": "success",
        "data": {
            "content": "这是content字段的响应"
        }
    })";

    String output;
    bool result = testParseChatResponse(jsonResponse, output);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(output.isEmpty());
    TEST_ASSERT_EQUAL_STRING("这是content字段的响应", output.c_str());
}

void test_parse_openai_format(void) {
    // 测试OpenAI兼容格式
    String jsonResponse = R"({
        "choices": [
            {
                "message": {
                    "content": "这是OpenAI格式的响应"
                }
            }
        ]
    })";

    String output;
    bool result = testParseChatResponse(jsonResponse, output);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(output.isEmpty());
    TEST_ASSERT_EQUAL_STRING("这是OpenAI格式的响应", output.c_str());
}

void test_parse_simple_text_field(void) {
    // 测试简单的text字段
    String jsonResponse = R"({
        "text": "这是text字段的响应"
    })";

    String output;
    bool result = testParseChatResponse(jsonResponse, output);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(output.isEmpty());
    TEST_ASSERT_EQUAL_STRING("这是text字段的响应", output.c_str());
}

void test_parse_error_response(void) {
    // 测试错误响应
    String jsonResponse = R"({
        "code": 1001,
        "msg": "认证失败"
    })";

    String output;
    bool result = testParseChatResponse(jsonResponse, output);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_TRUE(output.indexOf("API Error") >= 0);
}

void test_parse_invalid_json(void) {
    // 测试无效JSON
    String jsonResponse = "{这不是有效的JSON}";

    String output;
    bool result = testParseChatResponse(jsonResponse, output);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_TRUE(output.indexOf("Error parsing JSON") >= 0);
}

void test_parse_empty_response(void) {
    // 测试空响应
    String jsonResponse = "";

    String output;
    bool result = testParseChatResponse(jsonResponse, output);

    TEST_ASSERT_FALSE(result);
}

void test_parse_no_ai_message_found(void) {
    // 测试没有找到AI消息的情况
    String jsonResponse = R"({
        "messages": [
            {
                "content": "用户消息",
                "type": "user"
            },
            {
                "content": "工具消息",
                "type": "tool"
            }
        ]
    })";

    String output;
    bool result = testParseChatResponse(jsonResponse, output);

    // 应该返回第一个非空content
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(output.isEmpty());
    TEST_ASSERT_EQUAL_STRING("用户消息", output.c_str());
}

void test_parse_real_world_example(void) {
    // 测试真实世界的例子（来自用户日志）
    String jsonResponse = R"({
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

    String output;
    bool result = testParseChatResponse(jsonResponse, output);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(output.isEmpty());
    TEST_ASSERT_TRUE(output.indexOf("成都今日天气播报") >= 0);
    TEST_ASSERT_TRUE(output.indexOf("15-25°C") >= 0);
}

void setup() {
    // 等待串口就绪
    delay(2000);
    Serial.begin(115200);
    Serial.println("Starting Coze parser tests...");

    UNITY_BEGIN();

    // 运行解析测试
    RUN_TEST(test_parse_root_level_messages_array);
    RUN_TEST(test_parse_root_level_messages_with_role);
    RUN_TEST(test_parse_standard_coze_api_format);
    RUN_TEST(test_parse_coze_api_with_reply_field);
    RUN_TEST(test_parse_coze_api_with_content_field);
    RUN_TEST(test_parse_openai_format);
    RUN_TEST(test_parse_simple_text_field);
    RUN_TEST(test_parse_error_response);
    RUN_TEST(test_parse_invalid_json);
    RUN_TEST(test_parse_empty_response);
    RUN_TEST(test_parse_no_ai_message_found);
    RUN_TEST(test_parse_real_world_example);

    UNITY_END();
}

void loop() {
    // 空循环
}