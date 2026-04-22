#!/usr/bin/env python3
"""
Coze API解析逻辑测试 - 简化版
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
                print(f"Extracted AI reply from root-level messages")
                return True, content

        # 如果没有找到type为"ai"的消息，尝试查找第一个非空content
        for msg in data["messages"]:
            content = msg.get("content", "")
            if content:
                print(f"Extracted first non-empty content from root-level messages")
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
                    print(f"Extracted assistant reply from data.messages")
                    return True, content

        # 备用：直接查找reply字段
        if "reply" in data_obj:
            content = data_obj["reply"]
            print(f"Extracted reply field")
            return True, content

        # 备用：查找content字段
        if "content" in data_obj:
            content = data_obj["content"]
            print(f"Extracted content field")
            return True, content

    # 其他可能的响应格式
    if "choices" in data and isinstance(data["choices"], list) and len(data["choices"]) > 0:
        choice = data["choices"][0]
        if "message" in choice and "content" in choice["message"]:
            content = choice["message"]["content"]
            print(f"Extracted from choices")
            return True, content

    # 如果没有找到标准格式，尝试直接提取文本字段
    if "text" in data:
        content = data["text"]
        print(f"Extracted text field")
        return True, content

    # 最后手段：返回原始响应（调试用）
    raw_output = json_response[:200]
    print(f"Could not extract standard response, returning raw")
    return False, raw_output

def test_case(name, json_response, expected_success, expected_content_substring=None):
    """运行单个测试用例"""
    print(f"\n{'='*60}")
    print(f"Test: {name}")
    print(f"{'='*60}")
    print(f"Input JSON (truncated): {json_response[:80]}...")

    success, output = parse_chat_response(json_response)

    if success != expected_success:
        print(f"FAIL: Expected success={expected_success}, got success={success}")
        print(f"Output: {output}")
        return False

    if expected_success and expected_content_substring and expected_content_substring not in output:
        print(f"FAIL: Expected substring '{expected_content_substring}' not found in output")
        print(f"Output: {output}")
        return False

    print(f"PASS")
    if success:
        print(f"Extracted content (truncated): {output[:80]}...")
    return True

def main():
    """运行所有测试用例"""
    print("Coze API Parser Test")
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
                "content": "Weather report for Chengdu: sunny, 15-25C.",
                "type": "tool"
            },
            {
                "content": "Chengdu weather: sunny today, temperature 15-25C. Good for outdoor activities.",
                "type": "ai"
            }
        ]
    }'''
    all_passed &= test_case(
        "Root-level messages array (type='ai')",
        test1_json,
        True,
        "Chengdu weather"
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
        "Root-level messages array (role='assistant')",
        test2_json,
        True,
        "Hi there"
    )

    # 测试3: 标准Coze API格式
    test3_json = '''{
        "code": 0,
        "msg": "success",
        "data": {
            "messages": [
                {
                    "role": "assistant",
                    "content": "Standard Coze API response"
                }
            ]
        }
    }'''
    all_passed &= test_case(
        "Standard Coze API format",
        test3_json,
        True,
        "Standard Coze"
    )

    # 测试4: 错误响应
    test4_json = '''{
        "code": 1001,
        "msg": "Authentication failed"
    }'''
    all_passed &= test_case(
        "Error response",
        test4_json,
        False
    )

    # 测试5: 无效JSON
    test5_json = "{This is not valid JSON}"
    all_passed &= test_case(
        "Invalid JSON",
        test5_json,
        False
    )

    # 测试6: 没有找到AI消息的情况
    test6_json = '''{
        "messages": [
            {
                "content": "User message",
                "type": "user"
            },
            {
                "content": "Tool message",
                "type": "tool"
            }
        ]
    }'''
    all_passed &= test_case(
        "No AI message, return first non-empty content",
        test6_json,
        True,
        "User message"
    )

    # 测试7: 真实世界的例子（简化版）
    test7_json = '''{
        "messages": [
            {
                "content": "",
                "type": "user"
            },
            {
                "content": "Weather info for Chengdu: sunny, 15-25C.",
                "type": "tool"
            },
            {
                "content": "Chengdu weather broadcast: sunny today, 15-25C. Good for outdoor activities.",
                "type": "ai"
            }
        ]
    }'''
    all_passed &= test_case(
        "Real-world example (simplified)",
        test7_json,
        True,
        "15-25C"
    )

    print(f"\n{'='*60}")
    print("Test Summary")
    print(f"{'='*60}")
    if all_passed:
        print("✅ All tests passed!")
    else:
        print("❌ Some tests failed")

    return 0 if all_passed else 1

if __name__ == "__main__":
    exit(main())