#!/bin/bash

# 简单的WebSocket握手测试
# 使用curl测试HTTP升级

APP_ID="2015527679"
ACCESS_TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
WS_URL="wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async"
CONNECT_ID="test-$(date +%s)-$RANDOM"
WS_KEY="dGhlIHNhbXBsZSBub25jZQ=="  # 固定的测试key

echo "=== WebSocket握手测试 ==="
echo "URL: $WS_URL"
echo "App ID: $APP_ID"
echo "Connect ID: $CONNECT_ID"
echo ""

# 测试1: 使用volc.bigasr.sauc.duration（ASR 1.0小时版）
echo "🔍 测试1: volc.bigasr.sauc.duration（ASR 1.0小时版）"
RESOURCE_ID="volc.bigasr.sauc.duration"

response=$(curl -i -X GET "$WS_URL" \
  -H "Host: openspeech.bytedance.com" \
  -H "Upgrade: websocket" \
  -H "Connection: Upgrade" \
  -H "Sec-WebSocket-Key: $WS_KEY" \
  -H "Sec-WebSocket-Version: 13" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $ACCESS_TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Connect-Id: $CONNECT_ID" \
  --http1.1 \
  -s 2>&1)

echo "响应:"
echo "$response" | head -20

if echo "$response" | grep -q "101 Switching Protocols"; then
    echo "✅ 握手成功！Resource ID正确"
elif echo "$response" | grep -q "401"; then
    echo "❌ 认证失败"
elif echo "$response" | grep -q "403"; then
    echo "❌ 权限不足（Resource ID可能错误）"
elif echo "$response" | grep -q "400"; then
    echo "❌ 请求错误"
else
    echo "📊 其他响应"
fi

echo ""

# 测试2: 使用volc.seedasr.sauc.concurrent（并发版）
echo "🔍 测试2: volc.seedasr.sauc.concurrent（并发版）"
RESOURCE_ID="volc.seedasr.sauc.concurrent"
CONNECT_ID="test-$(date +%s)-$RANDOM-2"

response=$(curl -i -X GET "$WS_URL" \
  -H "Host: openspeech.bytedance.com" \
  -H "Upgrade: websocket" \
  -H "Connection: Upgrade" \
  -H "Sec-WebSocket-Key: $WS_KEY" \
  -H "Sec-WebSocket-Version: 13" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $ACCESS_TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Connect-Id: $CONNECT_ID" \
  --http1.1 \
  -s 2>&1)

echo "响应:"
echo "$response" | head -20

if echo "$response" | grep -q "101"; then
    echo "✅ 握手成功！可能实例是并发版"
elif echo "$response" | grep -q "403"; then
    echo "❌ 权限不足（实例可能是小时版，不是并发版）"
else
    echo "📊 其他响应"
fi

echo ""

# 测试3: 使用volc.bigasr.sauc.duration（ASR 1.0小时版）
echo "🔍 测试3: volc.bigasr.sauc.duration（ASR 1.0小时版）"
RESOURCE_ID="volc.bigasr.sauc.duration"
CONNECT_ID="test-$(date +%s)-$RANDOM-3"

response=$(curl -i -X GET "$WS_URL" \
  -H "Host: openspeech.bytedance.com" \
  -H "Upgrade: websocket" \
  -H "Connection: Upgrade" \
  -H "Sec-WebSocket-Key: $WS_KEY" \
  -H "Sec-WebSocket-Version: 13" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $ACCESS_TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Connect-Id: $CONNECT_ID" \
  --http1.1 \
  -s 2>&1)

echo "响应:"
echo "$response" | head -20

if echo "$response" | grep -q "101"; then
    echo "✅ 握手成功！实例可能是ASR 1.0小时版"
elif echo "$response" | grep -q "403"; then
    echo "❌ 权限不足（可能是错误的模型类型）"
else
    echo "📊 其他响应"
fi

echo ""
echo "=== 总结 ==="
echo "根据握手测试结果："
echo "1. 如果任何测试返回101，说明认证正确且实例可访问"
echo "2. 需要找到正确的Resource ID格式"
echo "3. 然后解决ESP32上的SSL错误-80问题"