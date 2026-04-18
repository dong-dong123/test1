#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试V3 WebSocket TTS API认证
"""

import asyncio
import websockets
import uuid
import json
import sys

APP_ID = "2015527679"
ACCESS_TOKEN = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
RESOURCE_ID = "seed-tts-2.0"
ENDPOINT = "wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream"

def test_headers(name, headers):
    """测试头部连接"""
    print(f"\n--- {name} ---")
    print(f"Headers: {headers}")

    async def connect():
        try:
            async with websockets.connect(
                ENDPOINT,
                additional_headers=headers,
                open_timeout=5
            ) as ws:
                print("SUCCESS: Connected!")
                return True
        except websockets.exceptions.InvalidStatus as e:
            print(f"FAILED: HTTP {e.status_code}")
            return False
        except Exception as e:
            print(f"ERROR: {e}")
            return False

    return asyncio.run(connect())

def main():
    print("Testing V3 WebSocket TTS API authentication")
    print(f"APP ID: {APP_ID}")
    print(f"Access Token: {ACCESS_TOKEN[:10]}...")
    print(f"Resource ID: {RESOURCE_ID}")
    print(f"Endpoint: {ENDPOINT}")

    connect_id = str(uuid.uuid4())

    # 测试不同的头部格式
    tests = []

    # 测试1: V1 API格式 (X-Api-App-Id) - 我们当前使用的
    tests.append((
        "V1 API headers (X-Api-App-Id)",
        [
            ("X-Api-App-Id", APP_ID),
            ("X-Api-Access-Key", ACCESS_TOKEN),
            ("X-Api-Resource-Id", RESOURCE_ID),
            ("X-Api-Connect-Id", connect_id),
            ("X-Api-Sequence", "1")
        ]
    ))

    # 测试2: Bearer Token格式 (文档中V1 API的认证方式)
    tests.append((
        "Bearer Token (Authorization: Bearer;token)",
        [
            ("Authorization", f"Bearer;{ACCESS_TOKEN}"),
            ("X-Api-Resource-Id", RESOURCE_ID),
            ("X-Api-Connect-Id", connect_id),
            ("X-Api-Sequence", "1")
        ]
    ))

    # 测试3: X-Api-App-Key格式 (ASR API使用)
    tests.append((
        "ASR API headers (X-Api-App-Key)",
        [
            ("X-Api-App-Key", APP_ID),
            ("X-Api-Access-Key", ACCESS_TOKEN),
            ("X-Api-Resource-Id", RESOURCE_ID),
            ("X-Api-Connect-Id", connect_id),
            ("X-Api-Sequence", "1")
        ]
    ))

    # 测试4: 无Resource ID
    tests.append((
        "No Resource ID",
        [
            ("X-Api-App-Id", APP_ID),
            ("X-Api-Access-Key", ACCESS_TOKEN),
            ("X-Api-Connect-Id", connect_id),
            ("X-Api-Sequence", "1")
        ]
    ))

    # 测试5: 无Sequence头部
    tests.append((
        "No Sequence header",
        [
            ("X-Api-App-Id", APP_ID),
            ("X-Api-Access-Key", ACCESS_TOKEN),
            ("X-Api-Resource-Id", RESOURCE_ID),
            ("X-Api-Connect-Id", connect_id)
        ]
    ))

    # 测试6: 仅Bearer Token，无其他头部
    tests.append((
        "Only Bearer Token",
        [
            ("Authorization", f"Bearer;{ACCESS_TOKEN}"),
            ("X-Api-Connect-Id", connect_id)
        ]
    ))

    # 运行测试
    results = []
    for name, headers in tests:
        success = test_headers(name, headers)
        results.append((name, success))

    # 打印总结
    print("\n=== TEST SUMMARY ===")
    for name, success in results:
        status = "PASS" if success else "FAIL"
        print(f"{status}: {name}")

if __name__ == "__main__":
    main()