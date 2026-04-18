#!/bin/bash

# 测试火山引擎API访问性

TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"
RESOURCE_ID="volc.seedasr.sauc.duration"

echo "=== 测试火山引擎API访问性 ==="
echo ""

# 测试1: 检查API端点是否可达
echo "测试1: 检查API端点是否可达"
echo "URL: https://openspeech.bytedance.com"
curl -I "https://openspeech.bytedance.com" \
  -w "HTTP状态: %{http_code}\n" \
  -s

echo ""
echo "----------------------------------------"
echo ""

# 测试2: 检查API路径是否可达
echo "测试2: 检查API路径"
echo "URL: https://openspeech.bytedance.com/api/v1/asr"
curl -I "https://openspeech.bytedance.com/api/v1/asr" \
  -w "HTTP状态: %{http_code}\n" \
  -s

echo ""
echo "----------------------------------------"
echo ""

# 测试3: 检查WebSocket端点
echo "测试3: 检查WebSocket端点"
echo "URL: wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_nostream"
curl -I "https://openspeech.bytedance.com/api/v3/sauc/bigmodel_nostream" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -w "HTTP状态: %{http_code}\n" \
  -s

echo ""
echo "----------------------------------------"
echo ""

# 测试4: 简单的OPTIONS请求（CORS预检）
echo "测试4: OPTIONS请求"
curl -X OPTIONS "https://openspeech.bytedance.com/api/v1/asr" \
  -H "Origin: https://example.com" \
  -H "Access-Control-Request-Method: POST" \
  -H "Access-Control-Request-Headers: X-Api-App-Key,X-Api-Access-Key,X-Api-Resource-Id" \
  -I \
  -w "\nHTTP状态: %{http_code}\n" \
  -s

echo ""
echo "=== 测试完成 ==="