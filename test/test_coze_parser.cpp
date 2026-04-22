#include <Arduino.h>
#include <unity.h>
#include "../src/services/CozeDialogueService.h"

// 测试parseChatResponse函数
void test_parse_root_level_messages_array(void) {
    // 测试根级别messages数组格式（coze.site API格式）
    // 实际响应格式：{"messages":[{"content":"",...},{"content":"☀️ 「成都」的天气情况来啦！...","type":"tool"},{"content":"🌤️ 成都今日天气播报来咯：...","type":"ai"}]}

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
    bool result = CozeDialogueService::parseChatResponse(jsonResponse, output);

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
    bool result = CozeDialogueService::parseChatResponse(jsonResponse, output);

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
    bool result = CozeDialogueService::parseChatResponse(jsonResponse, output);

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
    bool result = CozeDialogueService::parseChatResponse(jsonResponse, output);

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
    bool result = CozeDialogueService::parseChatResponse(jsonResponse, output);

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
    bool result = CozeDialogueService::parseChatResponse(jsonResponse, output);

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
    bool result = CozeDialogueService::parseChatResponse(jsonResponse, output);

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
    bool result = CozeDialogueService::parseChatResponse(jsonResponse, output);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_TRUE(output.indexOf("API Error") >= 0);
}

void test_parse_invalid_json(void) {
    // 测试无效JSON
    String jsonResponse = "{这不是有效的JSON}";

    String output;
    bool result = CozeDialogueService::parseChatResponse(jsonResponse, output);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_TRUE(output.indexOf("Error parsing JSON") >= 0);
}

void test_parse_empty_response(void) {
    // 测试空响应
    String jsonResponse = "";

    String output;
    bool result = CozeDialogueService::parseChatResponse(jsonResponse, output);

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
    bool result = CozeDialogueService::parseChatResponse(jsonResponse, output);

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
    bool result = CozeDialogueService::parseChatResponse(jsonResponse, output);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(output.isEmpty());
    TEST_ASSERT_TRUE(output.indexOf("成都今日天气播报") >= 0);
    TEST_ASSERT_TRUE(output.indexOf("15-25°C") >= 0);
}

void setup() {
    // 等待串口就绪
    delay(2000);

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