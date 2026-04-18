#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Simple test for V3 WebSocket TTS API
"""

import sys
import json
import asyncio
import websockets
import uuid
import time

APP_ID = "2015527679"
ACCESS_TOKEN = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
RESOURCE_ID = "seed-tts-2.0"
ENDPOINT = "wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream"
TEST_TEXT = "hello"
VOICE_TYPE = "zh_female_sajiaonvyou_moon_bigtts"

async def test():
    print("Testing WebSocket connection...")

    # Headers
    headers = [
        ("X-Api-App-Id", APP_ID),
        ("X-Api-Access-Key", ACCESS_TOKEN),
        ("X-Api-Resource-Id", RESOURCE_ID),
        ("X-Api-Connect-Id", str(uuid.uuid4()))
    ]

    try:
        # Try with headers parameter (newer websockets)
        async with websockets.connect(
            ENDPOINT,
            additional_headers=headers,
            open_timeout=5
        ) as ws:
            print("Connected!")

            # Build request
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
                    "voice_type": VOICE_TYPE,
                    "encoding": "pcm",
                    "rate": 16000,
                    "speed_ratio": 1.0
                },
                "request": {
                    "reqid": "test_123",
                    "text": TEST_TEXT,
                    "operation": "submit"
                }
            }

            await ws.send(json.dumps(request))
            print("Request sent")

            # Try to receive
            try:
                response = await asyncio.wait_for(ws.recv(), timeout=5)
                print(f"Received: {response[:100]}")
                return True
            except asyncio.TimeoutError:
                print("Timeout waiting for response")
                return False

    except Exception as e:
        print(f"Error: {e}")
        return False

if __name__ == "__main__":
    success = asyncio.run(test())
    print(f"Success: {success}")