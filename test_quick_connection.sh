#!/bin/bash
echo "=== 快速连接测试 $(date) ==="

TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"
RESOURCE_ID="volc.bigasr.sauc.duration"
CONNECT_ID="test-$(date +%s)-$(shuf -i 1000-9999 -n 1)"
WS_KEY="dGhlIHNhbXBsZSBub25jZQ=="

echo "参数:"
echo "  APP_ID: $APP_ID"
echo "  TOKEN: $TOKEN (长度: ${#TOKEN})"
echo "  RESOURCE_ID: $RESOURCE_ID"
echo "  CONNECT_ID: $CONNECT_ID"
echo ""

# 测试V2端点
echo "测试1: V2端点 (wss://openspeech.bytedance.com/api/v2/asr)"
WS_URL="wss://openspeech.bytedance.com/api/v2/asr"

# 使用X-Api-*头部（根据火山引擎文档）
echo "使用X-Api-*头部..."
response=$(timeout 15 curl -i -X GET "$WS_URL" \
  -H "Host: openspeech.bytedance.com" \
  -H "Upgrade: websocket" \
  -H "Connection: Upgrade" \
  -H "Sec-WebSocket-Key: $WS_KEY" \
  -H "Sec-WebSocket-Version: 13" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Connect-Id: $CONNECT_ID" \
  --max-time 10 \
  --connect-timeout 10 \
  --http1.1 \
  -s 2>&1)

echo "响应:"
echo "$response" | head -15
if echo "$response" | grep -q "101 Switching Protocols"; then
    echo "✅ V2 WebSocket握手成功！"
    exit 0
else
    echo "❌ V2 WebSocket握手失败"
fi

echo ""
echo "测试2: V3端点 (wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async)"
WS_URL_V3="wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async"

response_v3=$(timeout 15 curl -i -X GET "$WS_URL_V3" \
  -H "Host: openspeech.bytedance.com" \
  -H "Upgrade: websocket" \
  -H "Connection: Upgrade" \
  -H "Sec-WebSocket-Key: $WS_KEY" \
  -H "Sec-WebSocket-Version: 13" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Connect-Id: $CONNECT_ID" \
  --max-time 10 \
  --connect-timeout 10 \
  --http1.1 \
  -s 2>&1)

echo "响应:"
echo "$response_v3" | head -15
if echo "$response_v3" | grep -q "101 Switching Protocols"; then
    echo "✅ V3 WebSocket握手成功！"
    exit 0
else
    echo "❌ V3 WebSocket握手失败"
fi

echo ""
echo "=== 诊断信息 ==="
echo "1. 检查网络连接..."
ping -c 2 openspeech.bytedance.com 2>&1 | head -5
echo ""
echo "2. 检查SSL证书..."
timeout 5 openssl s_client -connect openspeech.bytedance.com:443 -servername openspeech.bytedance.com 2>&1 | grep -A5 "Certificate chain"
