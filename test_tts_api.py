#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
火山引擎语音合成(TTS) API测试脚本
基于用户提供的API调用示例
"""

import sys
import json
import requests
import base64
import uuid
import time
from typing import Dict, Any, Optional
import os

# 火山引擎API配置
APP_ID = "2015527679"
ACCESS_TOKEN = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
# TTS 2.0版本专属资源ID
RESOURCE_ID = "seed-tts-2.0"  # 2.0字符版

# API端点（需要根据实际文档确定）
# 参考：https://openspeech.bytedance.com/api/v3/tts/bidirection (双向流式)
# 参考：https://openspeech.bytedance.com/api/v3/tts/unidirectional/stream (单向流式)
# 参考：https://openspeech.bytedance.com/api/v1/tts (V1非流式API)
TTS_V1_API = "https://openspeech.bytedance.com/api/v1/tts"
TTS_V3_UNIDIRECTIONAL = "wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream"
TTS_V3_BIDIRECTIONAL = "wss://openspeech.bytedance.com/api/v3/tts/bidirection"

# 测试文本
TEST_TEXT = "你好呀，欢迎使用火山引擎语音合成服务"
VOICE_TYPE = "zh_female_vv_uranus_bigtts"  # 用户示例中的音色

def test_v1_tts_api():
    """测试V1非流式TTS API"""
    print("=== 测试V1非流式TTS API ===")
    print(f"API端点: {TTS_V1_API}")
    print(f"文本: {TEST_TEXT}")
    print(f"音色: {VOICE_TYPE}")

    headers = {
        "X-Api-App-Id": APP_ID,  # 注意：TTS API使用X-Api-App-Id而不是X-Api-App-Key
        "X-Api-Access-Key": ACCESS_TOKEN,
        "X-Api-Resource-Id": RESOURCE_ID,
        "Content-Type": "application/json"
    }

    # 构建请求体（根据火山TTS API文档格式）
    # 参考用户提供的bidirection.py示例格式
    request_body = {
        "app": {
            "appid": APP_ID,
            "token": ACCESS_TOKEN,
            "cluster": "volcano_tts"  # 固定集群
        },
        "user": {
            "uid": "test_user_" + str(uuid.uuid4())[:8]
        },
        "audio": {
            "voice_type": VOICE_TYPE,
            "encoding": "pcm",  # 音频编码格式
            "rate": 16000,      # 采样率
            "speed_ratio": 1.0  # 语速比例
        },
        "request": {
            "reqid": "test_req_" + str(int(time.time())),
            "text": TEST_TEXT,
            "operation": "query"  # 非流式操作
        }
    }

    print(f"请求头: {headers}")
    print(f"请求体: {json.dumps(request_body, indent=2, ensure_ascii=False)}")

    try:
        response = requests.post(TTS_V1_API, headers=headers, json=request_body, timeout=30)

        print(f"状态码: {response.status_code}")
        print(f"响应头: {dict(response.headers)}")

        if response.status_code == 200:
            try:
                resp_json = response.json()
                print(f"响应JSON: {json.dumps(resp_json, indent=2, ensure_ascii=False)}")

                # 检查响应码
                code = resp_json.get("code", -1)
                if code == 3000:  # 3000表示成功
                    print("✅ TTS API调用成功")

                    # 检查是否有音频数据
                    if "data" in resp_json and resp_json["data"]:
                        audio_data = resp_json["data"]
                        # 解码base64音频数据
                        audio_bytes = base64.b64decode(audio_data)
                        print(f"✅ 收到音频数据: {len(audio_bytes)} 字节")

                        # 保存音频文件用于验证
                        with open("test_tts_output.wav", "wb") as f:
                            f.write(audio_bytes)
                        print("✅ 音频已保存到 test_tts_output.wav")

                        return True, audio_bytes
                    else:
                        print("⚠️  响应中没有音频数据字段")
                        return False, None
                else:
                    message = resp_json.get("message", "未知错误")
                    print(f"❌ API返回错误: code={code}, message={message}")
                    return False, None

            except json.JSONDecodeError as e:
                print(f"❌ 响应不是有效的JSON: {e}")
                print(f"原始响应: {response.text[:500]}...")
                return False, None
        else:
            print(f"❌ HTTP错误: {response.status_code}")
            print(f"响应内容: {response.text[:500]}...")
            return False, None

    except requests.exceptions.RequestException as e:
        print(f"❌ 请求异常: {e}")
        return False, None

def test_v3_websocket_tts():
    """测试V3 WebSocket TTS API（需要websockets库）"""
    print("\n=== 测试V3 WebSocket TTS API ===")
    print("注意: 此测试需要websockets库 (pip install websockets)")
    print(f"WebSocket端点: {TTS_V3_UNIDIRECTIONAL}")

    try:
        import websockets
        import asyncio
    except ImportError:
        print("❌ websockets库未安装，跳过WebSocket测试")
        print("安装命令: pip install websockets")
        return False, None

    async def websocket_test():
        """异步WebSocket测试函数"""
        try:
            # 构建WebSocket连接头部
            headers = {
                "X-Api-App-Id": APP_ID,
                "X-Api-Access-Key": ACCESS_TOKEN,
                "X-Api-Resource-Id": RESOURCE_ID,
                "X-Api-Connect-Id": str(uuid.uuid4())
            }

            print(f"连接头部: {headers}")
            print("尝试连接WebSocket...")

            # 注意: WebSocket端点使用wss://
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
                            return True, response
                        else:
                            message = resp_json.get("message", "未知错误")
                            print(f"❌ WebSocket返回错误: code={code}, message={message}")
                            return False, None
                    except json.JSONDecodeError:
                        print("⚠️  响应不是JSON格式，可能是二进制音频数据")
                        print(f"响应类型: {type(response)}, 长度: {len(response)}")
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

    # 运行异步测试
    try:
        return asyncio.run(websocket_test())
    except Exception as e:
        print(f"❌ 运行WebSocket测试时出错: {e}")
        return False, None

def test_coze_tts_api():
    """测试Coze TTS API（用户提供的curl示例）"""
    print("\n=== 测试Coze TTS API ===")

    # 用户提供的Coze API示例
    COZE_API = "https://api.coze.cn/v1/audio/speech"
    # 注意：Coze使用Bearer token认证，不是火山引擎的头部
    COZE_TOKEN = "pat_OYDacMzM3WyOWV3Dtj2bHRMymzxP****"  # 需要真实token

    if COZE_TOKEN.endswith("****"):
        print("⚠️  Coze token不完整，跳过Coze API测试")
        print("注意: Coze API使用Bearer token认证，与火山引擎API不同")
        return False, None

    headers = {
        "Authorization": f"Bearer {COZE_TOKEN}",
        "Content-Type": "application/json"
    }

    request_body = {
        "input": TEST_TEXT,
        "voice_id": "742894*********",  # 需要真实voice_id
        "response_format": "wav"
    }

    print(f"API端点: {COZE_API}")
    print(f"请求头: {{'Authorization': 'Bearer ...', 'Content-Type': 'application/json'}}")
    print(f"请求体: {json.dumps(request_body, indent=2, ensure_ascii=False)}")

    try:
        response = requests.post(COZE_API, headers=headers, json=request_body, timeout=30)
        print(f"状态码: {response.status_code}")

        if response.status_code == 200:
            content_type = response.headers.get("Content-Type", "")
            if "audio" in content_type or "wav" in content_type:
                audio_data = response.content
                print(f"✅ 收到音频数据: {len(audio_data)} 字节")

                # 保存音频文件
                with open("test_coze_tts_output.wav", "wb") as f:
                    f.write(audio_data)
                print("✅ 音频已保存到 test_coze_tts_output.wav")
                return True, audio_data
            else:
                print(f"⚠️  响应Content-Type不是音频: {content_type}")
                print(f"响应内容: {response.text[:500]}...")
                return False, None
        else:
            print(f"❌ HTTP错误: {response.status_code}")
            print(f"响应内容: {response.text[:500]}...")
            return False, None

    except requests.exceptions.RequestException as e:
        print(f"❌ 请求异常: {e}")
        return False, None

def main():
    """主函数"""
    print("=" * 60)
    print("火山引擎语音合成(TTS) API测试")
    print("=" * 60)
    print(f"APP ID: {APP_ID}")
    print(f"Access Token: {ACCESS_TOKEN[:10]}...")
    print(f"资源ID: {RESOURCE_ID}")
    print(f"测试文本: {TEST_TEXT}")
    print()

    # 测试V1 API
    print("1. 测试V1非流式TTS API...")
    v1_success, v1_audio = test_v1_tts_api()

    print("\n" + "=" * 60)

    # 测试V3 WebSocket API
    print("2. 测试V3 WebSocket TTS API...")
    v3_success, v3_response = test_v3_websocket_tts()

    print("\n" + "=" * 60)

    # 测试Coze API（如果token可用）
    print("3. 测试Coze TTS API...")
    coze_success, coze_audio = test_coze_tts_api()

    print("\n" + "=" * 60)
    print("测试总结:")
    print(f"V1 TTS API: {'✅ 成功' if v1_success else '❌ 失败'}")
    print(f"V3 WebSocket TTS: {'✅ 成功' if v3_success else '❌ 失败'}")
    print(f"Coze TTS API: {'✅ 成功' if coze_success else '❌ 跳过/失败'}")

    # 提供建议
    print("\n建议:")
    if v1_success:
        print("- V1 API可用，可以在ESP32代码中使用")
    elif v3_success:
        print("- V3 WebSocket API可用，需要WebSocket客户端支持")
    else:
        print("- 需要检查API配置、认证头部和网络连接")
        print("- 确认资源ID 'seed-tts-2.0' 是否正确")
        print("- 确认使用正确的头部: X-Api-App-Id, X-Api-Access-Key, X-Api-Resource-Id")

if __name__ == "__main__":
    main()