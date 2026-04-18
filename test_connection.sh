#!/bin/bash
APP_ID="2015527679"
TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
RESOURCE_ID="volc.bigasr.sauc.duration"
CONNECT_ID="test-connect-$(date +%s)"
WS_KEY="dGhlIHNhbXBsZSBub25jZQ=="

# 测试V2端点 (api/v2/asr)
echo "=== 测试V2 WebSocket握手 (Bearer认证) ==="
WS_URL_V2="wss://openspeech.bytedance.com/api/v2/asr"

# 测试Bearer;token格式 (当前代码使用)
response1=$(curl -i -X GET "$WS_URL_V2" \
  -H "Host: openspeech.bytedance.com" \
  -H "Upgrade: websocket" \
  -H "Connection: Upgrade" \
  -H "Sec-WebSocket-Key: $WS_KEY" \
  -H "Sec-WebSocket-Version: 13" \
  -H "Authorization: Bearer;$TOKEN" \
  --max-time 10 \
  --http1.1 \
  -s 2>/dev/null)

echo "Bearer;token 响应:"
echo "$response1" | head -10
if echo "$response1" | grep -q "101 Switching Protocols"; then
    echo "✅ V2 Bearer;token 握手成功！"
else
    echo "❌ V2 Bearer;token 握手失败"
fi

echo ""
# 测试Bearer token格式 (标准格式)
response2=$(curl -i -X GET "$WS_URL_V2" \
  -H "Host: openspeech.bytedance.com" \
  -H "Upgrade: websocket" \
  -H "Connection: Upgrade" \
  -H "Sec-WebSocket-Key: $WS_KEY" \
  -H "Sec-WebSocket-Version: 13" \
  -H "Authorization: Bearer $TOKEN" \
  --max-time 10 \
  --http1.1 \
  -s 2>/dev/null)

echo "Bearer token 响应:"
echo "$response2" | head -10
if echo "$response2" | grep -q "101 Switching Protocols"; then
    echo "✅ V2 Bearer token 握手成功！"
else
    echo "❌ V2 Bearer token 握手失败"
fi

echo ""
echo "=== 测试V3 WebSocket握手 (X-Api-*头部) ==="
WS_URL_V3="wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async"

response3=$(curl -i -X GET "$WS_URL_V3" \
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
  --http1.1 \
  -s 2>/dev/null)

echo "V3 X-Api-* 响应:"
echo "$response3" | head -10
if echo "$response3" | grep -q "101 Switching Protocols"; then
    echo "✅ V3 WebSocket握手成功！"
else
    echo "❌ V3 WebSocket握手失败"
fi
