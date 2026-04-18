#!/usr/bin/env python3
"""
Debug WebSocket connection to Volcano V3 API
"""

import sys
import asyncio
import websockets
import json
import uuid

APP_ID = "2015527679"
ACCESS_TOKEN = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
RESOURCE_ID = "seed-tts-2.0"
ENDPOINT = "wss://openspeech.bytedance.com/api/v3/tts/bidirection"

async def test_connection():
    print("Testing WebSocket connection to:", ENDPOINT)

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
        # Try to connect with headers
        async with websockets.connect(
            ENDPOINT,
            additional_headers=headers,
            open_timeout=5
        ) as ws:
            print("SUCCESS: Connected successfully!")

            # Prepare test request
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
                    "reqid": "test_" + str(uuid.uuid4())[:8],
                    "text": "你好",
                    "operation": "submit"
                }
            }

            print("Sending request...")
            await ws.send(json.dumps(request, ensure_ascii=False))
            print("Request sent")

            # Try to receive response
            try:
                response = await asyncio.wait_for(ws.recv(), timeout=5)
                print(f"SUCCESS: Received response: {response[:200]}")

                # Try to parse as JSON
                try:
                    resp_json = json.loads(response)
                    print("Response JSON:", json.dumps(resp_json, indent=2, ensure_ascii=False))
                except:
                    print("Response is not JSON (may be binary audio data)")
                    print(f"Response length: {len(response)} bytes")

            except asyncio.TimeoutError:
                print("WARNING: No response received within timeout")

            return True

    except websockets.exceptions.InvalidStatusCode as e:
        print(f"FAILED: Connection rejected with HTTP {e.status_code}")
        if hasattr(e, 'headers'):
            print("Response headers:", e.headers)
        return False
    except Exception as e:
        print(f"FAILED: Connection error: {e}")
        return False

if __name__ == "__main__":
    try:
        success = asyncio.run(test_connection())
        print(f"\nTest {'PASSED' if success else 'FAILED'}")
    except Exception as e:
        print(f"Error running test: {e}")