#!/usr/bin/env python3
"""
Simple test for Volcano TTS V3 WebSocket API
"""

import asyncio
import websockets
import json
import uuid
import sys

APP_ID = "2015527679"
ACCESS_TOKEN = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
RESOURCE_ID = "seed-tts-2.0"

# Test both endpoints
ENDPOINTS = {
    "unidirectional": "wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream",
    "bidirectional": "wss://openspeech.bytedance.com/api/v3/tts/bidirection"
}

TEST_TEXT = "你好"
VOICE_TYPE = "zh_female_sajiaonvyou_moon_bigtts"

async def test_endpoint(endpoint_name, endpoint_url):
    print(f"\n=== Testing {endpoint_name} endpoint: {endpoint_url} ===")

    # Headers as per documentation
    headers = [
        ("X-Api-App-Id", APP_ID),
        ("X-Api-Access-Key", ACCESS_TOKEN),
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Sequence", "1"),
        ("X-Api-Connect-Id", str(uuid.uuid4()))
    ]

    print("Headers:")
    for key, value in headers:
        print(f"  {key}: {value}")

    try:
        async with websockets.connect(
            endpoint_url,
            additional_headers=headers,
            open_timeout=10
        ) as ws:
            print("✅ WebSocket connected")

            # Build request
            request = {
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
                    "reqid": "test_req_" + str(uuid.uuid4())[:8],
                    "text": TEST_TEXT,
                    "operation": "submit"
                }
            }

            print(f"Sending request: {json.dumps(request, ensure_ascii=False)}")
            await ws.send(json.dumps(request, ensure_ascii=False))
            print("✅ Request sent")

            # Wait for response
            try:
                response = await asyncio.wait_for(ws.recv(), timeout=10)
                print(f"✅ Response received: {response[:200]}...")

                # Try to parse as JSON
                try:
                    resp_json = json.loads(response)
                    print(f"Response JSON: {json.dumps(resp_json, indent=2, ensure_ascii=False)}")

                    # Check for error code
                    if "code" in resp_json:
                        code = resp_json["code"]
                        if code == 3000 or code == 20000000:
                            print(f"✅ Success! Code: {code}")
                            return True
                        else:
                            print(f"❌ Error code: {code}, message: {resp_json.get('message', 'No message')}")
                            return False
                    else:
                        print("⚠️  No code field in response")
                        return True

                except json.JSONDecodeError:
                    print("⚠️  Response is not JSON (may be binary audio)")
                    print(f"Response length: {len(response)} bytes")
                    return True

            except asyncio.TimeoutError:
                print("❌ Timeout waiting for response")
                return False

    except websockets.exceptions.InvalidStatusCode as e:
        print(f"❌ Connection failed with HTTP {e.status_code}")
        if hasattr(e, 'headers'):
            print(f"Response headers: {e.headers}")
        return False
    except Exception as e:
        print(f"❌ Connection error: {e}")
        return False

async def main():
    print("Testing Volcano TTS V3 WebSocket API")
    print(f"App ID: {APP_ID}")
    print(f"Access Token: {ACCESS_TOKEN[:10]}...")
    print(f"Resource ID: {RESOURCE_ID}")
    print(f"Voice: {VOICE_TYPE}")
    print(f"Text: {TEST_TEXT}")

    results = {}
    for name, url in ENDPOINTS.items():
        success = await test_endpoint(name, url)
        results[name] = success

    print("\n=== Results ===")
    for name, success in results.items():
        print(f"{name}: {'✅ PASS' if success else '❌ FAIL'}")

    # Overall result
    if all(results.values()):
        print("\n✅ All tests passed!")
        return True
    else:
        print("\n❌ Some tests failed")
        return False

if __name__ == "__main__":
    try:
        success = asyncio.run(main())
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)