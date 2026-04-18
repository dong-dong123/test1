#!/bin/bash

# 最终API测试 - 包含所有必需字段

TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"
RESOURCE_ID="volc.seedasr.sauc.duration"
API_URL="https://openspeech.bytedance.com/api/v1/asr"

echo "=== 火山引擎API最终测试 ==="
echo "包含所有必需字段"
echo ""

# 测试1: 完整的请求格式
echo "测试1: 完整请求格式（使用X-Api-*头部）"
response=$(curl -X POST "$API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Sequence: 1" \
  -H "Content-Type: application/json" \
  -d '{
    "app": {
      "appid": "'"$APP_ID"'",
      "token": "'"$TOKEN"'"
    },
    "user": {
      "uid": "test_user"
    },
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
      "sequence": 1,
      "model_name": "bigmodel",
      "enable_itn": true,
      "enable_punc": true
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
echo "状态码: $status"
echo "响应: $resp_body"
echo ""

# 测试2: 简化格式，只包含必需字段
echo "测试2: 简化格式"
response=$(curl -X POST "$API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Sequence: 1" \
  -H "Content-Type: application/json" \
  -d '{
    "app": {
      "appid": "'"$APP_ID"'",
      "token": "'"$TOKEN"'"
    },
    "audio": {
      "format": "pcm"
    },
    "request": {
      "reqid": "test_001",
      "sequence": 1,
      "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
echo "状态码: $status"
echo "响应: $resp_body"
echo ""

# 测试3: 测试不带app.token的格式
echo "测试3: 不带app.token（依赖头部认证）"
response=$(curl -X POST "$API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Sequence: 1" \
  -H "Content-Type: application/json" \
  -d '{
    "audio": {
      "format": "pcm"
    },
    "request": {
      "reqid": "test_002",
      "sequence": 1,
      "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
echo "状态码: $status"
echo "响应: $resp_body"
echo ""

# 测试4: 测试不同的resource_id
echo "测试4: 测试不同的resource_id"
RESOURCE_ID="volc.bigasr.sauc.duration"  # ASR 1.0小时版
response=$(curl -X POST "$API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Sequence: 1" \
  -H "Content-Type: application/json" \
  -d '{
    "app": {
      "appid": "'"$APP_ID"'",
      "token": "'"$TOKEN"'"
    },
    "audio": {
      "format": "pcm"
    },
    "request": {
      "reqid": "test_003",
      "sequence": 1,
      "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
echo "状态码: $status"
echo "响应: $resp_body"
echo ""

# 测试5: 测试空音频数据
echo "测试5: 包含空音频数据"
response=$(curl -X POST "$API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: volc.seedasr.sauc.duration" \
  -H "X-Api-Sequence: 1" \
  -H "Content-Type: application/json" \
  -d '{
    "app": {
      "appid": "'"$APP_ID"'",
      "token": "'"$TOKEN"'"
    },
    "audio": {
      "format": "pcm"
    },
    "request": {
      "reqid": "test_004",
      "sequence": 1,
      "model_name": "bigmodel"
    },
    "audio_data": ""
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
echo "状态码: $status"
echo "响应: $resp_body"
echo ""

echo "=== 测试完成 ==="