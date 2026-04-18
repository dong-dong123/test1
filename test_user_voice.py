#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试用户提供的音色参数
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
    # 用户新提供的音色 - 柔美女友
    "zh_female_sajiaonvyou_moon_bigtts",

    # 默认音色
    "zh-CN_female_standard",

    # 用户示例中的音色
    "zh_female_vv_uranus_bigtts",
]

def test_voice_type(voice_type):
    """测试特定音色"""
    print(f"\n=== Testing voice type: {voice_type} ===")

    headers = {
        "X-Api-App-Id": APP_ID,  # TTS API uses X-Api-App-Id not X-Api-App-Key
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

        print(f"Status code: {response.status_code}")

        if response.status_code == 200:
            try:
                resp_json = response.json()
                code = resp_json.get("code", -1)

                if code == 3000:  # 3000 means success
                    print(f"SUCCESS: Voice type {voice_type} is available")

                    # Check for audio data
                    if "data" in resp_json and resp_json["data"]:
                        audio_data = resp_json["data"]
                        audio_bytes = base64.b64decode(audio_data)
                        print(f"   Received audio data: {len(audio_bytes)} bytes")

                        # Save audio file for verification
                        filename = f"tts_{voice_type.replace('/', '_').replace('-', '_')}.wav"
                        with open(filename, "wb") as f:
                            f.write(audio_bytes)
                        print(f"   Audio saved to {filename}")

                        return True, len(audio_bytes), filename
                    else:
                        print(f"WARNING: No audio data field in response")
                        return True, 0, ""  # Voice available but no data
                else:
                    message = resp_json.get("message", "Unknown error")
                    print(f"FAILED: Voice type {voice_type} not available: code={code}, message={message}")
                    return False, 0, ""

            except json.JSONDecodeError as e:
                print(f"ERROR: Response is not valid JSON: {e}")
                print(f"Raw response: {response.text[:200]}...")
                return False, 0, ""
        else:
            print(f"ERROR: HTTP error: {response.status_code}")
            print(f"Response: {response.text[:200]}...")
            return False, 0, ""

    except requests.exceptions.RequestException as e:
        print(f"ERROR: Request exception: {e}")
        return False, 0, ""

def main():
    """主函数"""
    print("=" * 60)
    print("Volcano TTS Voice Type Test")
    print("=" * 60)
    print(f"APP ID: {APP_ID}")
    print(f"Access Token: {ACCESS_TOKEN[:10]}...")
    print(f"Resource ID: {RESOURCE_ID}")
    print(f"Test text: {TEST_TEXT}")
    print(f"Number of voice types to test: {len(VOICE_TYPES)}")
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
    print("Test Summary:")
    print("-" * 60)

    available_voices = []
    for result in results:
        status = "AVAILABLE" if result["success"] else "NOT AVAILABLE"
        size_info = f", audio size: {result['audio_size']} bytes" if result["audio_size"] > 0 else ""
        print(f"{status}: {result['voice_type']}{size_info}")

        if result["success"]:
            available_voices.append(result["voice_type"])

    print("\n" + "=" * 60)
    print("Recommendations:")

    if available_voices:
        print(f"SUCCESS: Found {len(available_voices)} available voice types")
        print("Available voice types:")
        for voice in available_voices:
            print(f"  - {voice}")

        # Check user's voice
        user_voice = "zh_female_sajiaonvyou_moon_bigtts"
        if user_voice in available_voices:
            print(f"\nSUCCESS: User's voice type '{user_voice}' is available, can be integrated into ESP32 code")
        else:
            print(f"\nFAILED: User's voice type '{user_voice}' is not available, please check the voice name")
    else:
        print("FAILED: No available voice types, please check API configuration")

    print("\nNext steps:")
    print("1. Add available voice types to config.json")
    print("2. Update ESP32 code to use selected voice type")
    print("3. Test TTS functionality integration")

if __name__ == "__main__":
    main()