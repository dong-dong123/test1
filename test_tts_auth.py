#!/usr/bin/env python3
"""
Test different authentication methods for Volcano TTS V3 API
"""

import asyncio
import websockets
import json
import uuid
import sys

APP_ID = "2015527679"
ACCESS_TOKEN = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
RESOURCE_ID = "seed-tts-2.0"
ENDPOINT = "wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream"
TEST_TEXT = "你好"
VOICE_TYPE = "zh_female_sajiaonvyou_moon_bigtts"

# Different authentication schemes to test
AUTH_SCHEMES = {
    "v3_standard": [
        ("X-Api-App-Id", APP_ID),
        ("X-Api-Access-Key", ACCESS_TOKEN),
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Sequence", "1"),
        ("X-Api-Connect-Id", str(uuid.uuid4()))
    ],
    "v3_no_sequence": [
        ("X-Api-App-Id", APP_ID),
        ("X-Api-Access-Key", ACCESS_TOKEN),
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Connect-Id", str(uuid.uuid4()))
    ],
    "v3_sequence_neg1": [
        ("X-Api-App-Id", APP_ID),
        ("X-Api-Access-Key", ACCESS_TOKEN),
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Sequence", "-1"),
        ("X-Api-Connect-Id", str(uuid.uuid4()))
    ],
    "v3_with_request_id": [
        ("X-Api-App-Id", APP_ID),
        ("X-Api-Access-Key", ACCESS_TOKEN),
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Request-Id", str(uuid.uuid4())),
        ("X-Api-Sequence", "-1"),
        ("X-Api-Connect-Id", str(uuid.uuid4()))
    ],
    "v3_api_key": [
        ("X-Api-Key", ACCESS_TOKEN),
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Sequence", "-1"),
        ("X-Api-Connect-Id", str(uuid.uuid4()))
    ],
    "v1_style": [
        ("X-Api-App-Key", APP_ID),
        ("X-Api-Access-Key", ACCESS_TOKEN),
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Sequence", "1"),
        ("X-Api-Connect-Id", str(uuid.uuid4()))
    ],
    "minimal": [
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Connect-Id", str(uuid.uuid4()))
    ]
}

async def test_auth_scheme(scheme_name, headers):
    print(f"\n=== Testing auth scheme: {scheme_name} ===")
    print("Headers:")
    for key, value in headers:
        print(f"  {key}: {value}")

    try:
        async with websockets.connect(
            ENDPOINT,
            additional_headers=headers,
            open_timeout=10
        ) as ws:
            print("SUCCESS: WebSocket connected")

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

            print(f"Sending request...")
            await ws.send(json.dumps(request, ensure_ascii=False))
            print("Request sent")

            # Try to receive response
            try:
                response = await asyncio.wait_for(ws.recv(), timeout=10)
                print(f"Response received: {response[:200]}...")
                return True, "Connected and got response"
            except asyncio.TimeoutError:
                print("Timeout waiting for response")
                return True, "Connected but timeout on response"
            except Exception as e:
                print(f"Error receiving response: {e}")
                return True, f"Connected but response error: {e}"

    except websockets.exceptions.InvalidStatus as e:
        print(f"FAILED: HTTP {e.status_code}")
        return False, f"HTTP {e.status_code}"
    except Exception as e:
        print(f"FAILED: {e}")
        return False, str(e)

async def main():
    print("Testing different authentication schemes for Volcano TTS V3 API")
    print(f"Endpoint: {ENDPOINT}")
    print(f"App ID: {APP_ID}")
    print(f"Access Token: {ACCESS_TOKEN[:10]}...")
    print(f"Resource ID: {RESOURCE_ID}")

    results = {}
    for scheme_name, headers in AUTH_SCHEMES.items():
        success, message = await test_auth_scheme(scheme_name, headers)
        results[scheme_name] = (success, message)
        # Brief pause between tests
        await asyncio.sleep(1)

    print("\n=== Results Summary ===")
    for scheme_name, (success, message) in results.items():
        status = "PASS" if success else "FAIL"
        print(f"{scheme_name:20} {status:6} - {message}")

    # Check if any succeeded
    successful_schemes = [name for name, (success, _) in results.items() if success]
    if successful_schemes:
        print(f"\nSUCCESSFUL schemes: {', '.join(successful_schemes)}")
        return True
    else:
        print("\nALL authentication schemes failed")
        return False

if __name__ == "__main__":
    try:
        success = asyncio.run(main())
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Error running test: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)