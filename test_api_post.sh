#!/bin/bash

# 测试POST请求到API端点

TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"
RESOURCE_ID="volc.seedasr.sauc.duration"
API_URL="https://openspeech.bytedance.com/api/v1/asr"

echo "=== 测试POST请求 ==="
echo ""

# 测试1: 最简单的POST请求
echo "测试1: 最简单的POST请求"
curl -X POST "$API_URL" \
  -H "Content-Type: application/json" \
  -d '{}' \
  -w "\n状态码: %{http_code}\n" \
  -s

echo ""
echo "----------------------------------------"
echo ""

# 测试2: 带有基本头部的POST请求
echo "测试2: 带有基本头部的POST请求"
curl -X POST "$API_URL" \
  -H "Content-Type: application/json" \
  -H "X-Api-App-Key: $APP_ID" \
  -d '{}' \
  -w "\n状态码: %{http_code}\n" \
  -s

echo ""
echo "----------------------------------------"
echo ""

# 测试3: 测试不同的API端点
echo "测试3: 测试/api/v2/asr"
API_URL_V2="https://openspeech.bytedance.com/api/v2/asr"
curl -X POST "$API_URL_V2" \
  -H "Content-Type: application/json" \
  -H "X-Api-App-Key: $APP_ID" \
  -d '{}' \
  -w "\n状态码: %{http_code}\n" \
  -s

echo ""
echo "----------------------------------------"
echo ""

# 测试4: 测试/api/v3/asr
echo "测试4: 测试/api/v3/asr"
API_URL_V3="https://openspeech.bytedance.com/api/v3/asr"
curl -X POST "$API_URL_V3" \
  -H "Content-Type: application/json" \
  -H "X-Api-App-Key: $APP_ID" \
  -d '{}' \
  -w "\n状态码: %{http_code}\n" \
  -s

echo ""
echo "=== 测试完成 ==="