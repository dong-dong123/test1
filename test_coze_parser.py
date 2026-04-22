#!/usr/bin/env python3
"""
Coze API解析逻辑测试
验证parseChatResponse函数是否能正确解析各种Coze API响应格式
"""

import json

def parse_chat_response(json_response):
    """
    模拟CozeDialogueService::parseChatResponse的逻辑
    返回: (success, output)
    """
    try:
        data = json.loads(json_response)
    except json.JSONDecodeError as e:
        return False, f"Error parsing JSON: {e}"

    # 检查错误码
    if "code" in data and data["code"] != 0:
        error_msg = data.get("msg", data.get("message", "Unknown error"))
        error_code = data.get("code", -1)
        return False, f"API Error {error_code}: {error_msg}"

    # 首先检查根级别的messages数组（coze.site API格式）
    if "messages" in data and isinstance(data["messages"], list):
        print(f"Found root-level messages array with {len(data['messages'])} messages")

        for msg in data["messages"]:
            msg_type = msg.get("type", "")
            content = msg.get("content", "")
            role = msg.get("role", "")

            print(f"Message: type={msg_type}, role={role}, content length={len(content)}")

            # 查找type为"ai"或role为"assistant"的消息
            if (msg_type == "ai" or role == "assistant") and content:
                print(f"Extracted AI reply from root-level messages: {content[:50]}...")
                return True, content

        # 如果没有找到type为"ai"的消息，尝试查找第一个非空content
        for msg in data["messages"]:
            content = msg.get("content", "")
            if content:
                print(f"Extracted first non-empty content from root-level messages: {content[:50]}...")
                return True, content

    # 然后检查标准Coze API格式（data字段）
    if "data" in data:
        data_obj = data["data"]

        # 尝试从messages数组提取回复
        if "messages" in data_obj and isinstance(data_obj["messages"], list):
            for msg in data_obj["messages"]:
                role = msg.get("role", "")
                msg_type = msg.get("type", "")
                content = msg.get("content", "")

                if (role == "assistant" or msg_type == "answer") and content:
                    print(f"Extracted assistant reply from data.messages: {content[:50]}...")
                    return True, content

        # 备用：直接查找reply字段
        if "reply" in data_obj:
            content = data_obj["reply"]
            print(f"Extracted reply field: {content[:50]}...")
            return True, content

        # 备用：查找content字段
        if "content" in data_obj:
            content = data_obj["content"]
            print(f"Extracted content field: {content[:50]}...")
            return True, content

    # 其他可能的响应格式
    if "choices" in data and isinstance(data["choices"], list) and len(data["choices"]) > 0:
        choice = data["choices"][0]
        if "message" in choice and "content" in choice["message"]:
            content = choice["message"]["content"]
            print(f"Extracted from choices: {content[:50]}...")
            return True, content

    # 如果没有找到标准格式，尝试直接提取文本字段
    if "text" in data:
        content = data["text"]
        print(f"Extracted text field: {content[:50]}...")
        return True, content

    # 最后手段：返回原始响应（调试用）
    raw_output = json_response[:200]
    print(f"Could not extract standard response, returning raw: {raw_output}...")
    return False, raw_output

def test_case(name, json_response, expected_success, expected_content_substring=None):
    """运行单个测试用例"""
    print(f"\n{'='*60}")
    print(f"测试: {name}")
    print(f"{'='*60}")
    print(f"输入JSON: {json_response[:100]}...")

    success, output = parse_chat_response(json_response)

    if success != expected_success:
        print(f"❌ 失败: 预期success={expected_success}, 实际success={success}")
        print(f"输出: {output}")
        return False

    if expected_success and expected_content_substring and expected_content_substring not in output:
        print(f"❌ 失败: 输出中未找到预期子串 '{expected_content_substring}'")
        print(f"输出: {output}")
        return False

    print(f"✅ 通过")
    if success:
        print(f"提取的内容: {output[:100]}...")
    return True

def main():
    """运行所有测试用例"""
    print("Coze API解析逻辑测试")
    print("=" * 60)

    all_passed = True

    # 测试1: 根级别messages数组格式（coze.site API格式）
    test1_json = '''{
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
    }'''
    all_passed &= test_case(
        "根级别messages数组格式（type='ai'）",
        test1_json,
        True,
        "成都今日天气播报"
    )

    # 测试2: 根级别messages数组，使用role字段
    test2_json = '''{
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
    }'''
    all_passed &= test_case(
        "根级别messages数组（role='assistant'）",
        test2_json,
        True,
        "Hi there!"
    )

    # 测试3: 标准Coze API格式
    test3_json = '''{
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
    }'''
    all_passed &= test_case(
        "标准Coze API格式",
        test3_json,
        True,
        "标准Coze API"
    )

    # 测试4: Coze API的data.reply字段
    test4_json = '''{
        "code": 0,
        "msg": "success",
        "data": {
            "reply": "这是reply字段的响应"
        }
    }'''
    all_passed &= test_case(
        "Coze API的data.reply字段",
        test4_json,
        True,
        "reply字段"
    )

    # 测试5: Coze API的data.content字段
    test5_json = '''{
        "code": 0,
        "msg": "success",
        "data": {
            "content": "这是content字段的响应"
        }
    }'''
    all_passed &= test_case(
        "Coze API的data.content字段",
        test5_json,
        True,
        "content字段"
    )

    # 测试6: OpenAI兼容格式
    test6_json = '''{
        "choices": [
            {
                "message": {
                    "content": "这是OpenAI格式的响应"
                }
            }
        ]
    }'''
    all_passed &= test_case(
        "OpenAI兼容格式",
        test6_json,
        True,
        "OpenAI格式"
    )

    # 测试7: 简单的text字段
    test7_json = '''{
        "text": "这是text字段的响应"
    }'''
    all_passed &= test_case(
        "简单的text字段",
        test7_json,
        True,
        "text字段"
    )

    # 测试8: 错误响应
    test8_json = '''{
        "code": 1001,
        "msg": "认证失败"
    }'''
    all_passed &= test_case(
        "错误响应",
        test8_json,
        False
    )

    # 测试9: 无效JSON
    test9_json = "{这不是有效的JSON}"
    all_passed &= test_case(
        "无效JSON",
        test9_json,
        False
    )

    # 测试10: 没有找到AI消息的情况
    test10_json = '''{
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
    }'''
    all_passed &= test_case(
        "没有AI消息，返回第一个非空content",
        test10_json,
        True,
        "用户消息"
    )

    # 测试11: 真实世界的例子（来自用户日志）
    test11_json = '''{
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
    }'''
    all_passed &= test_case(
        "真实世界的例子",
        test11_json,
        True,
        "15-25°C"
    )

    # 测试12: 空响应
    test12_json = ""
    all_passed &= test_case(
        "空响应",
        test12_json,
        False
    )

    print(f"\n{'='*60}")
    print("测试总结")
    print(f"{'='*60}")
    if all_passed:
        print("✅ 所有测试通过！")
    else:
        print("❌ 有些测试失败")

    return 0 if all_passed else 1

if __name__ == "__main__":
    exit(main())