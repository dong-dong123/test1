#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试火山引擎V3 WebSocket TTS API
"""

import sys
import json
import asyncio
import websockets
import uuid
import time

# 火山引擎API配置
APP_ID = "2015527679"
ACCESS_TOKEN = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
# TTS 2.0版本专属资源ID
RESOURCE_ID = "seed-tts-2.0"  # 2.0字符版

# WebSocket端点
TTS_V3_UNIDIRECTIONAL = "wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream"

# 测试文本
TEST_TEXT = "你好呀，欢迎使用火山引擎语音合成服务"
# 音色
VOICE_TYPE = "zh_female_sajiaonvyou_moon_bigtts"

async def test_websocket_tts():
    """测试WebSocket TTS API"""
    print("=== 测试V3 WebSocket TTS API ===")
    print(f"WebSocket端点: {TTS_V3_UNIDIRECTIONAL}")
    print(f"文本: {TEST_TEXT}")
    print(f"音色: {VOICE_TYPE}")

    # 构建WebSocket连接头部
    headers = {
        "X-Api-App-Id": APP_ID,
        "X-Api-Access-Key": ACCESS_TOKEN,
        "X-Api-Resource-Id": RESOURCE_ID,
        "X-Api-Connect-Id": str(uuid.uuid4())
    }

    print(f"连接头部: {headers}")
    print("尝试连接WebSocket...")

    try:
        async with websockets.connect(
            TTS_V3_UNIDIRECTIONAL,
            extra_headers=headers,
            ssl=True,
            open_timeout=10
        ) as websocket:
            print("✅ WebSocket连接成功")

            # 构建TTS请求JSON
            tts_request = {
                "app": {
                    "appid": APP_ID,
                    "token": ACCESS_TOKEN,
                    "cluster": "volcano_tts"
                },
                "user": {
                    "uid": "test_user_" + str(uuid.uuid4())[:8]
                },
                "audio": {
                    "voice_type": VOICE_TYPE,
                    "encoding": "pcm",
                    "rate": 16000,
                    "speed_ratio": 1.0
                },
                "request": {
                    "reqid": "test_req_" + str(int(time.time())),
                    "text": TEST_TEXT,
                    "operation": "submit"  # 流式操作
                }
            }

            request_json = json.dumps(tts_request)
            print(f"发送TTS请求: {request_json}")

            # 发送请求
            await websocket.send(request_json)
            print("✅ TTS请求已发送")

            # 接收响应（设置超时）
            try:
                response = await asyncio.wait_for(websocket.recv(), timeout=10)
                print(f"收到响应: {response[:200]}...")

                # 尝试解析响应
                try:
                    resp_json = json.loads(response)
                    print(f"响应JSON: {json.dumps(resp_json, indent=2, ensure_ascii=False)}")

                    # 检查响应码
                    code = resp_json.get("code", -1)
                    if code == 3000:
                        print("✅ WebSocket TTS请求成功")

                        # 检查是否有音频数据
                        if "data" in resp_json and resp_json["data"]:
                            import base64
                            audio_data = resp_json["data"]
                            audio_bytes = base64.b64decode(audio_data)
                            print(f"✅ 收到音频数据: {len(audio_bytes)} 字节")

                            # 保存音频文件用于验证
                            filename = "tts_websocket_output.wav"
                            with open(filename, "wb") as f:
                                f.write(audio_bytes)
                            print(f"✅ 音频已保存到 {filename}")
                            return True, audio_bytes
                        else:
                            print("⚠️  响应中没有音频数据字段")
                            return True, None
                    else:
                        message = resp_json.get("message", "未知错误")
                        print(f"❌ WebSocket返回错误: code={code}, message={message}")
                        return False, None

                except json.JSONDecodeError:
                    print("⚠️  响应不是JSON格式，可能是二进制音频数据")
                    print(f"响应类型: {type(response)}, 长度: {len(response)}")
                    # 可能是二进制音频数据
                    if len(response) > 100:
                        filename = "tts_websocket_binary.bin"
                        with open(filename, "wb") as f:
                            f.write(response)
                        print(f"✅ 二进制数据已保存到 {filename}")
                        return True, response
                    return True, response

            except asyncio.TimeoutError:
                print("❌ 等待响应超时")
                return False, None

    except websockets.exceptions.InvalidStatusCode as e:
        print(f"❌ WebSocket连接失败，状态码: {e.status_code}")
        return False, None
    except Exception as e:
        print(f"❌ WebSocket异常: {e}")
        return False, None

def main():
    """主函数"""
    print("火山引擎V3 WebSocket TTS API测试")
    print(f"APP ID: {APP_ID}")
    print(f"Access Token: {ACCESS_TOKEN[:10]}...")
    print(f"资源ID: {RESOURCE_ID}")
    print(f"测试文本: {TEST_TEXT}")
    print()

    try:
        success, audio_data = asyncio.run(test_websocket_tts())

        print("\n" + "=" * 60)
        if success:
            print("✅ WebSocket TTS API测试成功")
            if audio_data:
                print(f"   收到音频数据: {len(audio_data)} 字节")
        else:
            print("❌ WebSocket TTS API测试失败")

    except Exception as e:
        print(f"❌ 运行测试时出错: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    main()