#!/bin/bash

# 根据API文档测试正确的头部格式
# API文档: docs/API/流水语音识别api.md

API_URL="https://openspeech.bytedance.com/api/v1/asr"
TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"
RESOURCE_ID="volc.seedasr.sauc.duration"  # 根据API文档，ASR 2.0小时版

echo "=== 火山引擎语音识别API测试（正确头部格式） ==="
echo "Token: $TOKEN"
echo "App ID: $APP_ID"
echo "Resource ID: $RESOURCE_ID"
echo "API URL: $API_URL"
echo ""

# 测试1: 使用API文档中的头部格式
echo "测试1: 使用X-Api-*头部（根据API文档）"
curl -X POST "$API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Connect-Id: $(uuidgen 2>/dev/null || echo 'test-connect-id')" \
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

# 测试2: 测试Authorization头部格式（Bearer;TOKEN）
echo "测试2: Authorization头部格式（Bearer;TOKEN）"
curl -X POST "$API_URL" \
  -H "Authorization: Bearer;$TOKEN" \
  -H "X-App-Id: $APP_ID" \
  -H "X-Resource-Id: $RESOURCE_ID" \
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

# 测试3: 测试cluster字段（根据用户之前的测试）
echo "测试3: 在请求体中包含cluster字段"
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
    "app": {
      "appid": "'"$APP_ID"'",
      "cluster": "'"$RESOURCE_ID"'"
    },
    "audio_data": "UklGRlQAAABXQVZFZm10IBAAAAABAAEARKwAAIhYAQACABAAZGF0YQ=="
  }' \
  -w "\n状态码: %{http_code}\n" \
  -s

echo ""
echo "----------------------------------------"
echo ""

# 测试4: 测试resource_id字段在请求体中
echo "测试4: 在请求体中包含resource_id字段"
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
    "app": {
      "appid": "'"$APP_ID"'",
      "resource_id": "'"$RESOURCE_ID"'"
    },
    "audio_data": "UklGRlQAAABXQVZFZm10IBAAAAABAAEARKwAAIhYAQACABAAZGF0YQ=="
  }' \
  -w "\n状态码: %{http_code}\n" \
  -s

echo ""
echo "=== 测试完成 ==="