#!/bin/bash

# 火山引擎API系统测试
# 探索正确的请求格式

TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"
RESOURCE_ID="volc.seedasr.sauc.duration"
API_URL="https://openspeech.bytedance.com/api/v1/asr"

echo "=== 火山引擎API系统测试 ==="
echo "测试不同的请求格式和参数"
echo ""

# 辅助函数：显示响应
show_response() {
    local test_name="$1"
    local response="$2"
    local status="$3"

    echo "=== $test_name ==="
    echo "状态码: $status"
    echo "响应: $response"
    echo ""
}

# 测试1: 基础格式，包含app.token
echo "测试1: 基础格式，包含app.token"
response=$(curl -X POST "$API_URL" \
  -H "Authorization: Bearer;$TOKEN" \
  -H "X-App-Id: $APP_ID" \
  -H "Content-Type: application/json" \
  -d '{
    "app": {
      "appid": "'"$APP_ID"'",
      "token": "'"$TOKEN"'"
    },
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
      "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
show_response "测试1" "$resp_body" "$status"

# 测试2: 添加cluster字段
echo "测试2: 添加cluster字段"
response=$(curl -X POST "$API_URL" \
  -H "Authorization: Bearer;$TOKEN" \
  -H "X-App-Id: $APP_ID" \
  -H "Content-Type: application/json" \
  -d '{
    "app": {
      "appid": "'"$APP_ID"'",
      "token": "'"$TOKEN"'",
      "cluster": "'"$RESOURCE_ID"'"
    },
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
      "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
show_response "测试2" "$resp_body" "$status"

# 测试3: 添加resource_id字段
echo "测试3: 添加resource_id字段"
response=$(curl -X POST "$API_URL" \
  -H "Authorization: Bearer;$TOKEN" \
  -H "X-App-Id: $APP_ID" \
  -H "Content-Type: application/json" \
  -d '{
    "app": {
      "appid": "'"$APP_ID"'",
      "token": "'"TOKEN"'",
      "resource_id": "'"$RESOURCE_ID"'"
    },
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
      "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
show_response "测试3" "$resp_body" "$status"

# 测试4: 使用X-Api-*头部，包含app.token
echo "测试4: 使用X-Api-*头部，包含app.token"
response=$(curl -X POST "$API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Connect-Id: test-connect-123" \
  -H "Content-Type: application/json" \
  -d '{
    "app": {
      "appid": "'"$APP_ID"'",
      "token": "'"$TOKEN"'"
    },
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
      "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
show_response "测试4" "$resp_body" "$status"

# 测试5: 最小化请求，只包含必需字段
echo "测试5: 最小化请求"
response=$(curl -X POST "$API_URL" \
  -H "Authorization: Bearer;$TOKEN" \
  -H "X-App-Id: $APP_ID" \
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
      "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
show_response "测试5" "$resp_body" "$status"

# 测试6: 测试不同的API端点
echo "测试6: 测试WebSocket风格的API端点"
WS_API_URL="https://openspeech.bytedance.com/api/v3/sauc/bigmodel_nostream"
response=$(curl -X GET "$WS_API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Connect-Id: test-connect-456" \
  -I \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_headers=$(echo "$response" | sed '$d')
show_response "测试6 WebSocket端点" "$resp_headers" "$status"

echo "=== 测试完成 ==="