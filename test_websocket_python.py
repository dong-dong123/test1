#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
火山引擎WebSocket连接测试
使用Python的websockets库测试连接
"""

import asyncio
import websockets
import uuid
import ssl
import sys

async def test_websocket_connection():
    """测试WebSocket连接"""
    app_id = "2015527679"
    access_token = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"

    # 测试不同的Resource ID
    resource_ids = [
        "volc.bigasr.sauc.duration",       # ASR 1.0小时版（已验证正确）
        "volc.seedasr.sauc.duration",      # 种子模型小时版
        "volc.seedasr.sauc.concurrent",    # 种子模型并发版
        "volc.bigasr.sauc.concurrent",     # ASR 1.0并发版
    ]

    # WebSocket端点
    endpoints = [
        "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async",
        "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_nostream",
        "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel",
    ]

    print("=== 火山引擎WebSocket连接测试 ===")
    print(f"App ID: {app_id}")
    print(f"Access Token: {access_token[:10]}...")
    print()

    # 测试第一个端点
    endpoint = endpoints[0]  # bigmodel_async
    print(f"测试端点: {endpoint}")
    print()

    for resource_id in resource_ids:
        connect_id = str(uuid.uuid4())

        print(f"🔍 测试 Resource ID: {resource_id}")
        print(f"   Connect ID: {connect_id}")

        # 设置自定义头部
        headers = {
            "X-Api-App-Key": app_id,
            "X-Api-Access-Key": access_token,
            "X-Api-Resource-Id": resource_id,
            "X-Api-Connect-Id": connect_id,
        }

        try:
            # 创建SSL上下文（禁用验证用于测试）
            ssl_context = ssl.create_default_context()
            ssl_context.check_hostname = False
            ssl_context.verify_mode = ssl.CERT_NONE

            # 尝试连接（设置超时）
            async with websockets.connect(
                endpoint,
                extra_headers=headers,
                ssl=ssl_context,
                open_timeout=10
            ) as websocket:
                print(f"   ✅ 连接成功！")

                # 发送测试消息（根据协议）
                print(f"   发送认证消息...")

                # 这里应该发送二进制协议消息，但先测试连接
                await websocket.send("test")
                response = await asyncio.wait_for(websocket.recv(), timeout=5)
                print(f"   收到响应: {response[:100]}...")

                await websocket.close()
                print(f"   连接关闭")

        except websockets.exceptions.InvalidStatusCode as e:
            status_code = e.status_code
            if status_code == 401:
                print(f"   ❌ 认证失败 (401)")
            elif status_code == 403:
                print(f"   ❌ 权限不足 (403)")
            elif status_code == 404:
                print(f"   ❌ 端点不存在 (404)")
            else:
                print(f"   ❌ HTTP错误: {status_code}")
        except asyncio.TimeoutError:
            print(f"   ⏱️  连接超时")
        except websockets.exceptions.WebSocketException as e:
            print(f"   ❌ WebSocket错误: {e}")
        except Exception as e:
            print(f"   ❌ 其他错误: {e}")

        print()

if __name__ == "__main__":
    # 检查websockets库是否安装
    try:
        import websockets
    except ImportError:
        print("错误: websockets库未安装")
        print("安装命令: pip install websockets")
        sys.exit(1)

    # 运行测试
    try:
        asyncio.run(test_websocket_connection())
    except KeyboardInterrupt:
        print("\n测试中断")
    except Exception as e:
        print(f"测试失败: {e}")