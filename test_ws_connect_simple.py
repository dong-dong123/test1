#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
简单测试V3 WebSocket TTS API连接
"""

import asyncio
import websockets
import uuid
import json
import sys

APP_ID = "2015527679"
ACCESS_TOKEN = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
RESOURCE_ID = "seed-tts-2.0"
ENDPOINT_UNI = "wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream"
ENDPOINT_BI = "wss://openspeech.bytedance.com/api/v3/tts/bidirection"

async def test_connection(endpoint, headers, test_name):
    print(f"\n=== {test_name} ===")
    print(f"端点: {endpoint}")
    print(f"头部: {headers}")

    try:
        async with websockets.connect(
            endpoint,
            additional_headers=headers,
            open_timeout=5
        ) as ws:
            print("✅ WebSocket连接成功")

            # 构建简单的请求
            request = {
                "app": {
                    "appid": APP_ID,
                    "token": ACCESS_TOKEN,
                    "cluster": "volcano_tts"
                },
                "user": {
                    "uid": "test_user"
                },
                "audio": {
                    "voice_type": "zh_female_sajiaonvyou_moon_bigtts",
                    "encoding": "pcm",
                    "rate": 16000,
                    "speed_ratio": 1.0
                },
                "request": {
                    "reqid": "test_req_123",
                    "text": "你好",
                    "operation": "submit"
                }
            }

            await ws.send(json.dumps(request))
            print("✅ 请求已发送")

            # 尝试接收响应（带超时）
            try:
                response = await asyncio.wait_for(ws.recv(), timeout=5)
                print(f"✅ 收到响应: {response[:200]}...")

                # 检查响应是否是JSON
                try:
                    resp_json = json.loads(response)
                    print(f"响应JSON: {json.dumps(resp_json, indent=2, ensure_ascii=False)[:300]}...")

                    code = resp_json.get("code", -1)
                    if code == 3000:
                        print("✅ TTS请求成功")
                        return True
                    else:
                        message = resp_json.get("message", "未知错误")
                        print(f"❌ 错误: code={code}, message={message}")
                        return False
                except json.JSONDecodeError:
                    # 可能是二进制音频
                    print(f"✅ 收到二进制响应，长度: {len(response)} 字节")
                    return True

            except asyncio.TimeoutError:
                print("⚠️  接收响应超时（可能服务器不返回响应？）")
                return True  # 超时不一定是错误

    except websockets.exceptions.InvalidStatusCode as e:
        print(f"❌ 连接失败，状态码: {e.status_code}")
        if e.status_code == 403:
            print("   403 Forbidden: 认证失败或资源无权访问")
        elif e.status_code == 401:
            print("   401 Unauthorized: 认证无效")
        return False
    except Exception as e:
        print(f"❌ 连接异常: {e}")
        return False

async def main():
    print("测试V3 WebSocket TTS API连接")
    print(f"APP ID: {APP_ID}")
    print(f"Access Token: {ACCESS_TOKEN[:10]}...")
    print(f"Resource ID: {RESOURCE_ID}")

    # 测试不同的头部格式
    connect_id = str(uuid.uuid4())

    # 测试1: V1 API格式 (X-Api-App-Id)
    headers1 = [
        ("X-Api-App-Id", APP_ID),
        ("X-Api-Access-Key", ACCESS_TOKEN),
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Connect-Id", connect_id),
        ("X-Api-Sequence", "1")
    ]

    # 测试2: Bearer Token格式
    headers2 = [
        ("Authorization", f"Bearer;{ACCESS_TOKEN}"),
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Connect-Id", connect_id),
        ("X-Api-Sequence", "1")
    ]

    # 测试3: X-Api-App-Key格式 (ASR API)
    headers3 = [
        ("X-Api-App-Key", APP_ID),
        ("X-Api-Access-Key", ACCESS_TOKEN),
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Connect-Id", connect_id),
        ("X-Api-Sequence", "1")
    ]

    # 测试4: 无Sequence头部
    headers4 = [
        ("X-Api-App-Id", APP_ID),
        ("X-Api-Access-Key", ACCESS_TOKEN),
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Connect-Id", connect_id)
    ]

    tests = [
        (ENDPOINT_UNI, headers1, "单向流式端点 - V1 API头部"),
        (ENDPOINT_UNI, headers2, "单向流式端点 - Bearer Token"),
        (ENDPOINT_UNI, headers3, "单向流式端点 - X-Api-App-Key"),
        (ENDPOINT_UNI, headers4, "单向流式端点 - 无Sequence头部"),
        (ENDPOINT_BI, headers1, "双向流式端点 - V1 API头部"),
    ]

    results = []
    for endpoint, headers, test_name in tests:
        success = await test_connection(endpoint, headers, test_name)
        results.append((test_name, success))

    print("\n=== 测试总结 ===")
    for test_name, success in results:
        status = "✅ 成功" if success else "❌ 失败"
        print(f"{test_name}: {status}")

if __name__ == "__main__":
    asyncio.run(main())