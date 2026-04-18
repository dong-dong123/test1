#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test different resource IDs for WebSocket connection
"""

import sys
import asyncio
import websockets
import uuid

APP_ID = "2015527679"
ACCESS_TOKEN = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
ENDPOINT = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async"

RESOURCE_IDS = [
    "volc.bigasr.sauc.duration",       # ASR 1.0小时版
    "volc.seedasr.sauc.concurrent",    # 种子模型并发版 (current config)
    "volc.seedasr.sauc.duration",      # 种子模型小时版
    "volc.bigasr.sauc.concurrent",     # ASR 1.0并发版
]

async def test_resource_id(resource_id):
    """Test connection with specific resource ID"""
    connect_id = str(uuid.uuid4())

    headers = {
        "X-Api-App-Key": APP_ID,
        "X-Api-Access-Key": ACCESS_TOKEN,
        "X-Api-Resource-Id": resource_id,
        "X-Api-Connect-Id": connect_id,
    }

    print(f"Testing Resource ID: {resource_id}")
    print(f"  Connect ID: {connect_id}")

    try:
        async with websockets.connect(
            ENDPOINT,
            extra_headers=headers,
            ssl=True,
            open_timeout=10
        ) as ws:
            print(f"  SUCCESS: Connected with resource ID: {resource_id}")

            # Try to send a test message
            await ws.send("test")
            try:
                response = await asyncio.wait_for(ws.recv(), timeout=3)
                print(f"  Received response: {response[:100]}")
            except asyncio.TimeoutError:
                print(f"  No response received (timeout)")

            await ws.close()
            return True, resource_id

    except websockets.exceptions.InvalidStatusCode as e:
        print(f"  FAILED: HTTP {e.status_code} - Resource ID: {resource_id}")
        return False, e.status_code
    except Exception as e:
        print(f"  FAILED: {type(e).__name__}: {str(e)[:100]}")
        return False, str(e)

async def main():
    print("=== Testing WebSocket connection with different Resource IDs ===")
    print(f"APP ID: {APP_ID}")
    print(f"Access Token: {ACCESS_TOKEN[:10]}...")
    print(f"Endpoint: {ENDPOINT}")
    print()

    results = []
    for resource_id in RESOURCE_IDS:
        success, result = await test_resource_id(resource_id)
        results.append((resource_id, success, result))
        print()

    print("=== Results Summary ===")
    for resource_id, success, result in results:
        if success:
            print(f"✅ {resource_id}: SUCCESS")
        else:
            print(f"❌ {resource_id}: FAILED ({result})")

    # Check if any succeeded
    successful_ids = [r[0] for r in results if r[1]]
    if successful_ids:
        print(f"\n✅ Working Resource IDs: {', '.join(successful_ids)}")
        return successful_ids[0]
    else:
        print("\n❌ All Resource IDs failed")
        return None

if __name__ == "__main__":
    try:
        result = asyncio.run(main())
        if result:
            print(f"\nRecommended Resource ID: {result}")
        else:
            print("\nNo working Resource ID found")
    except KeyboardInterrupt:
        print("\nTest interrupted")
    except Exception as e:
        print(f"Test failed: {e}")
        import traceback
        traceback.print_exc()