#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Quick WebSocket connection test
"""

import asyncio
import websockets
import uuid

APP_ID = "2015527679"
ACCESS_TOKEN = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
ENDPOINT = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async"

async def test_connection(resource_id, headers, description):
    """Test WebSocket connection with given headers"""
    print(f"\n=== {description} ===")
    print(f"Resource ID: {resource_id}")
    print(f"Headers: {headers}")

    try:
        async with websockets.connect(
            ENDPOINT,
            additional_headers=headers,
            origin="v3",
            ssl=True,
            open_timeout=10
        ) as ws:
            print(f"[SUCCESS] Connected!")

            # Try to send a test message
            await ws.send("test")
            try:
                response = await asyncio.wait_for(ws.recv(), timeout=3)
                print(f"  Received: {response[:100]}")
            except asyncio.TimeoutError:
                print(f"  No response received (timeout)")

            await ws.close()
            return True

    except websockets.exceptions.InvalidStatusCode as e:
        print(f"[FAILED] HTTP {e.status_code}")
        if hasattr(e, 'headers'):
            print(f"  Response headers: {e.headers}")
        return False
    except Exception as e:
        print(f"[FAILED] {type(e).__name__}: {str(e)[:100]}")
        return False

async def main():
    print("Quick WebSocket Connection Test")
    print("="*50)

    # Test 1: volc.bigasr.sauc.duration with original headers
    connect_id = str(uuid.uuid4())
    headers1 = {
        "X-Api-App-Key": APP_ID,
        "X-Api-Access-Key": ACCESS_TOKEN,
        "X-Api-Resource-Id": "volc.bigasr.sauc.duration",
        "Host": "openspeech.bytedance.com",
        "X-Api-Connect-Id": connect_id,
    }

    success1 = await test_connection("volc.bigasr.sauc.duration", headers1,
                                    "Test 1: volc.bigasr.sauc.duration (original headers)")

    # Test 2: volc.seedasr.sauc.concurrent with original headers
    connect_id2 = str(uuid.uuid4())
    headers2 = {
        "X-Api-App-Key": APP_ID,
        "X-Api-Access-Key": ACCESS_TOKEN,
        "X-Api-Resource-Id": "volc.seedasr.sauc.concurrent",
        "Host": "openspeech.bytedance.com",
        "X-Api-Connect-Id": connect_id2,
    }

    success2 = await test_connection("volc.seedasr.sauc.concurrent", headers2,
                                    "Test 2: volc.seedasr.sauc.concurrent (original headers)")

    # Test 3: volc.bigasr.sauc.duration with server-allowed headers
    connect_id3 = str(uuid.uuid4())
    headers3 = {
        "X-Api-App-Key": APP_ID,
        "X-Api-Access-Key": ACCESS_TOKEN,
        "X-Api-Resource-Id": "volc.bigasr.sauc.duration",
        "X-Api-Sequence": "-1",
        "X-Api-Request-Id": connect_id3,
    }

    success3 = await test_connection("volc.bigasr.sauc.duration", headers3,
                                    "Test 3: volc.bigasr.sauc.duration (server-allowed headers)")

    # Test 4: volc.seedasr.sauc.concurrent with server-allowed headers
    connect_id4 = str(uuid.uuid4())
    headers4 = {
        "X-Api-App-Key": APP_ID,
        "X-Api-Access-Key": ACCESS_TOKEN,
        "X-Api-Resource-Id": "volc.seedasr.sauc.concurrent",
        "X-Api-Sequence": "-1",
        "X-Api-Request-Id": connect_id4,
    }

    success4 = await test_connection("volc.seedasr.sauc.concurrent", headers4,
                                    "Test 4: volc.seedasr.sauc.concurrent (server-allowed headers)")

    print("\n" + "="*50)
    print("SUMMARY:")
    print(f"volc.bigasr.sauc.duration (original): {'[PASS]' if success1 else '[FAIL]'}")
    print(f"volc.seedasr.sauc.concurrent (original): {'[PASS]' if success2 else '[FAIL]'}")
    print(f"volc.bigasr.sauc.duration (server-allowed): {'[PASS]' if success3 else '[FAIL]'}")
    print(f"volc.seedasr.sauc.concurrent (server-allowed): {'[PASS]' if success4 else '[FAIL]'}")

    # Recommendation
    if success1 or success3:
        print("\n[RECOMMENDATION] Use volc.bigasr.sauc.duration")
        return "volc.bigasr.sauc.duration"
    elif success2 or success4:
        print("\n[RECOMMENDATION] Use volc.seedasr.sauc.concurrent")
        return "volc.seedasr.sauc.concurrent"
    else:
        print("\n[FAILED] No working configuration found")
        return None

if __name__ == "__main__":
    try:
        result = asyncio.run(main())
        if result:
            print(f"\nUse this Resource ID in config.json: {result}")
        else:
            print("\nAll tests failed")
    except KeyboardInterrupt:
        print("\nTest interrupted")
    except Exception as e:
        print(f"Test failed: {e}")
        import traceback
        traceback.print_exc()