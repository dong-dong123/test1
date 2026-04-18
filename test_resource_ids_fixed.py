#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test different resource IDs for WebSocket connection with fixed headers
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

async def test_resource_id(resource_id, test_name, headers):
    """Test connection with specific resource ID and headers"""
    print(f"\n{test_name}: {resource_id}")
    print(f"  Headers: {headers}")

    try:
        # Try different connection methods
        if hasattr(websockets, 'connect'):
            # Newer websockets library
            async with websockets.connect(
                ENDPOINT,
                additional_headers=headers,
                origin="v3",  # From reference code
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
        else:
            print(f"  ERROR: websockets.connect not available")
            return False, "websockets.connect not available"

    except websockets.exceptions.InvalidStatusCode as e:
        print(f"  FAILED: HTTP {e.status_code} - Resource ID: {resource_id}")
        if hasattr(e, 'headers'):
            print(f"  Response headers: {e.headers}")
        return False, e.status_code
    except Exception as e:
        print(f"  FAILED: {type(e).__name__}: {str(e)[:100]}")
        return False, str(e)

async def main():
    print("=== Testing WebSocket connection with different Resource IDs ===")
    print(f"APP ID: {APP_ID}")
    print(f"Access Token: {ACCESS_TOKEN[:10]}...")
    print(f"Endpoint: {ENDPOINT}")

    results = []

    for resource_id in RESOURCE_IDS:
        # Test 1: Original headers (with Host and X-Api-Connect-Id)
        connect_id = str(uuid.uuid4())
        headers1 = {
            "X-Api-App-Key": APP_ID,
            "X-Api-Access-Key": ACCESS_TOKEN,
            "X-Api-Resource-Id": resource_id,
            "Host": "openspeech.bytedance.com",
            "X-Api-Connect-Id": connect_id,
        }

        success1, result1 = await test_resource_id(resource_id, "Test 1: Original headers", headers1)

        # Test 2: Server-allowed headers only
        connect_id2 = str(uuid.uuid4())
        headers2 = {
            "X-Api-App-Key": APP_ID,
            "X-Api-Access-Key": ACCESS_TOKEN,
            "X-Api-Resource-Id": resource_id,
            "X-Api-Sequence": "-1",
            "X-Api-Request-Id": connect_id2,
        }

        success2, result2 = await test_resource_id(resource_id, "Test 2: Server-allowed headers", headers2)

        # Test 3: Minimal headers
        headers3 = {
            "X-Api-App-Key": APP_ID,
            "X-Api-Access-Key": ACCESS_TOKEN,
            "X-Api-Resource-Id": resource_id,
        }

        success3, result3 = await test_resource_id(resource_id, "Test 3: Minimal headers", headers3)

        results.append((resource_id, success1 or success2 or success3, [success1, success2, success3]))

    print("\n" + "="*60)
    print("Results Summary:")
    print("="*60)

    for resource_id, success, test_results in results:
        if success:
            print(f"[SUCCESS] {resource_id}")
            print(f"  Test1 (Original): {'PASS' if test_results[0] else 'FAIL'}")
            print(f"  Test2 (Server-allowed): {'PASS' if test_results[1] else 'FAIL'}")
            print(f"  Test3 (Minimal): {'PASS' if test_results[2] else 'FAIL'}")
        else:
            print(f"[FAILED] {resource_id}")

    # Check if any succeeded
    successful_ids = [r[0] for r in results if r[1]]
    if successful_ids:
        print(f"\nWorking Resource IDs: {', '.join(successful_ids)}")
        return successful_ids[0]
    else:
        print("\nAll Resource IDs failed")
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