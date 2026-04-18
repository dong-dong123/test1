#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试火山引擎V1 TTS API
"""

import sys
import json
import requests
import base64
import uuid
import time

APP_ID = "2015527679"
ACCESS_TOKEN = "R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
RESOURCE_ID = "seed-tts-2.0"
TTS_V1_API = "https://openspeech.bytedance.com/api/v1/tts"

TEST_TEXT = "你好"
VOICE_TYPE = "zh_female_sajiaonvyou_moon_bigtts"

def test_v1_tts():
    print("=== Testing V1 TTS API ===")
    print(f"API: {TTS_V1_API}")
    print(f"App ID: {APP_ID}")
    print(f"Access Token: {ACCESS_TOKEN[:10]}...")
    print(f"Resource ID: {RESOURCE_ID}")
    print(f"Voice: {VOICE_TYPE}")
    print(f"Text: {TEST_TEXT}")

    headers = {
        "X-Api-App-Id": APP_ID,
        "X-Api-Access-Key": ACCESS_TOKEN,
        "X-Api-Resource-Id": RESOURCE_ID,
        "Content-Type": "application/json"
    }

    request_body = {
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
            "operation": "query"
        }
    }

    print(f"\nHeaders: {headers}")
    print(f"Request body: {json.dumps(request_body, indent=2, ensure_ascii=False)}")

    try:
        response = requests.post(TTS_V1_API, headers=headers, json=request_body, timeout=30)
        print(f"\nStatus code: {response.status_code}")

        if response.status_code == 200:
            try:
                resp_json = response.json()
                print(f"Response: {json.dumps(resp_json, indent=2, ensure_ascii=False)}")

                code = resp_json.get("code", -1)
                if code == 3000:
                    print("SUCCESS: TTS API call successful!")
                    if "data" in resp_json and resp_json["data"]:
                        audio_data = resp_json["data"]
                        audio_bytes = base64.b64decode(audio_data)
                        print(f"Audio data: {len(audio_bytes)} bytes")
                        with open("tts_v1_output.wav", "wb") as f:
                            f.write(audio_bytes)
                        print("Audio saved to tts_v1_output.wav")
                    return True
                else:
                    message = resp_json.get("message", "Unknown error")
                    print(f"ERROR: API returned error: code={code}, message={message}")
                    return False
            except json.JSONDecodeError as e:
                print(f"ERROR: Response is not valid JSON: {e}")
                print(f"Raw response: {response.text[:500]}...")
                return False
        else:
            print(f"ERROR: HTTP error: {response.status_code}")
            print(f"Response: {response.text[:500]}...")
            return False
    except requests.exceptions.RequestException as e:
        print(f"ERROR: Request exception: {e}")
        return False

if __name__ == "__main__":
    success = test_v1_tts()
    sys.exit(0 if success else 1)