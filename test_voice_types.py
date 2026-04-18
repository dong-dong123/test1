#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试火山引擎TTS不同音色
验证用户提供的音色参数是否有效
"""

import sys
import json
import requests
import base64
import uuid
import time

# 火山引擎API配置
APP_ID = "2015527679"
ACCESS_TOKEN = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
# TTS 2.0版本专属资源ID
RESOURCE_ID = "seed-tts-2.0"  # 2.0字符版

# API端点
TTS_V1_API = "https://openspeech.bytedance.com/api/v1/tts"

# 测试文本
TEST_TEXT = "你好呀，欢迎使用火山引擎语音合成服务"

# 需要测试的音色列表
VOICE_TYPES = [
    # 默认音色
    "zh-CN_female_standard",

    # 用户示例中的音色
    "zh_female_vv_uranus_bigtts",

    # 用户新提供的音色 - 柔美女友
    "zh_female_sajiaonvyou_moon_bigtts",

    # 其他可能的音色
    "zh-CN_male_standard",
    "zh_female_kailangjiejie_moon_bigtts",  # 开朗姐姐
]

def test_voice_type(voice_type):
    """测试特定音色"""
    print(f"\n=== 测试音色: {voice_type} ===")

    headers = {
        "X-Api-App-Id": APP_ID,  # 注意：TTS API使用X-Api-App-Id而不是X-Api-App-Key
        "X-Api-Access-Key": ACCESS_TOKEN,
        "X-Api-Resource-Id": RESOURCE_ID,
        "Content-Type": "application/json"
    }

    # 构建请求体
    request_body = {
        "app": {
            "appid": APP_ID,
            "token": ACCESS_TOKEN,
            "cluster": "volcano_tts"
        },
        "user": {
            "uid": "test_user_" + str(uuid.uuid4())[:8]
        },
        "audio": {
            "voice_type": voice_type,
            "encoding": "pcm",
            "rate": 16000,
            "speed_ratio": 1.0
        },
        "request": {
            "reqid": "test_req_" + str(int(time.time())),
            "text": TEST_TEXT,
            "operation": "query"
        }
    }

    try:
        response = requests.post(TTS_V1_API, headers=headers, json=request_body, timeout=30)

        print(f"状态码: {response.status_code}")

        if response.status_code == 200:
            try:
                resp_json = response.json()
                code = resp_json.get("code", -1)

                if code == 3000:  # 3000表示成功
                    print(f"✅ 音色 {voice_type} 可用")

                    # 检查是否有音频数据
                    if "data" in resp_json and resp_json["data"]:
                        audio_data = resp_json["data"]
                        audio_bytes = base64.b64decode(audio_data)
                        print(f"   收到音频数据: {len(audio_bytes)} 字节")

                        # 保存音频文件用于验证
                        filename = f"tts_{voice_type.replace('/', '_').replace('-', '_')}.wav"
                        with open(filename, "wb") as f:
                            f.write(audio_bytes)
                        print(f"   音频已保存到 {filename}")

                        return True, len(audio_bytes), filename
                    else:
                        print(f"⚠️  响应中没有音频数据字段")
                        return True, 0, ""  # 音色可用但没有数据
                else:
                    message = resp_json.get("message", "未知错误")
                    print(f"❌ 音色 {voice_type} 不可用: code={code}, message={message}")
                    return False, 0, ""

            except json.JSONDecodeError as e:
                print(f"❌ 响应不是有效的JSON: {e}")
                print(f"原始响应: {response.text[:200]}...")
                return False, 0, ""
        else:
            print(f"❌ HTTP错误: {response.status_code}")
            print(f"响应内容: {response.text[:200]}...")
            return False, 0, ""

    except requests.exceptions.RequestException as e:
        print(f"❌ 请求异常: {e}")
        return False, 0, ""

def main():
    """主函数"""
    print("=" * 60)
    print("火山引擎TTS音色测试")
    print("=" * 60)
    print(f"APP ID: {APP_ID}")
    print(f"Access Token: {ACCESS_TOKEN[:10]}...")
    print(f"资源ID: {RESOURCE_ID}")
    print(f"测试文本: {TEST_TEXT}")
    print(f"测试音色数量: {len(VOICE_TYPES)}")
    print()

    results = []

    for voice_type in VOICE_TYPES:
        success, audio_size, filename = test_voice_type(voice_type)
        results.append({
            "voice_type": voice_type,
            "success": success,
            "audio_size": audio_size,
            "filename": filename
        })

    print("\n" + "=" * 60)
    print("音色测试总结:")
    print("-" * 60)

    available_voices = []
    for result in results:
        status = "✅ 可用" if result["success"] else "❌ 不可用"
        size_info = f", 音频大小: {result['audio_size']} 字节" if result["audio_size"] > 0 else ""
        print(f"{status} {result['voice_type']}{size_info}")

        if result["success"]:
            available_voices.append(result["voice_type"])

    print("\n" + "=" * 60)
    print("建议:")

    if available_voices:
        print(f"✅ 找到 {len(available_voices)} 个可用的音色")
        print("推荐使用以下音色:")
        for voice in available_voices:
            print(f"  - {voice}")

        # 特别检查用户提供的音色
        user_voice = "zh_female_sajiaonvyou_moon_bigtts"
        if user_voice in available_voices:
            print(f"\n✅ 用户提供的音色 '{user_voice}' 可用，可以集成到ESP32代码中")
        else:
            print(f"\n❌ 用户提供的音色 '{user_voice}' 不可用，请检查音色名称是否正确")

        # 检查示例音色
        example_voice = "zh_female_vv_uranus_bigtts"
        if example_voice in available_voices:
            print(f"✅ 示例音色 '{example_voice}' 可用")
        else:
            print(f"⚠️  示例音色 '{example_voice}' 不可用")
    else:
        print("❌ 没有可用的音色，请检查API配置")

    print("\n下一步:")
    print("1. 将可用的音色添加到 config.json 中")
    print("2. 更新 ESP32 代码以使用选择的音色")
    print("3. 测试 TTS 功能集成")

if __name__ == "__main__":
    main()