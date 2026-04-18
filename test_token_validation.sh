#!/bin/bash

# 火山引擎语音识别API Token验证脚本
# 测试Access Token: R23gVDqaVB_j-TaRfNywkJnerpGGJtcB

API_URL="https://openspeech.bytedance.com/api/v1/asr"
TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"

echo "=== 火山引擎语音识别API Token验证 ==="
echo "Token: $TOKEN"
echo "App ID: $APP_ID"
echo "API URL: $API_URL"
echo ""

# 测试1: 简单JSON请求（无音频数据）
echo "测试1: 基本请求（无音频）"
curl -X POST "$API_URL" \
  -H "Authorization: Bearer;$TOKEN" \
  -H "X-App-Id: $APP_ID" \
  -H "Content-Type: application/json" \
  -d '{
    "user": {"uid": "test_user"},
    "audio": {
      "format": "pcm",
      "codec": "raw",
      "rate": 16000,
      "bits": 16,
      "channel": 1,
      "language": "zh-CN"
    },
    "request": {
      "reqid": "test_req_123",
      "model_name": "bigmodel",
      "enable_itn": true,
      "enable_punc": true,
      "enable_ddc": false
    },
    "audio_data": "UklGRlQAAABXQVZFZm10IBAAAAABAAEARKwAAIhYAQACABAAZGF0YQ=="
  }' \
  -w "\n状态码: %{http_code}\n" \
  -s

echo ""
echo "----------------------------------------"
echo ""

# 测试2: 更简单的请求（测试认证）
echo "测试2: 最小化请求"
curl -X POST "$API_URL" \
  -H "Authorization: Bearer;$TOKEN" \
  -H "X-App-Id: $APP_ID" \
  -H "Content-Type: application/json" \
  -d '{
    "user": {"uid": "test"},
    "audio": {"format": "pcm"},
    "request": {"reqid": "test"}
  }' \
  -w "\n状态码: %{http_code}\n" \
  -s

echo ""
echo "----------------------------------------"
echo ""

# 测试3: 仅头部验证（GET请求）
echo "测试3: 头部验证测试"
curl -X GET "$API_URL" \
  -H "Authorization: Bearer;$TOKEN" \
  -H "X-App-Id: $APP_ID" \
  -I \
  -w "\n状态码: %{http_code}\n" \
  -s

echo ""
echo "=== 测试完成 ==="