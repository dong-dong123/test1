#!/bin/bash
# 测试V3 WebSocket TTS API连接

TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"
RESOURCE_ID="seed-tts-2.0"
UUID=$(uuidgen 2>/dev/null || python -c "import uuid; print(uuid.uuid4())")
ENDPOINT="wss://openspeech.bytedance.com/api/v3/tts/unidirectional/stream"

echo "=== 测试V3 WebSocket TTS API连接 ==="
echo "端点: $ENDPOINT"
echo "App ID: $APP_ID"
echo "Resource ID: $RESOURCE_ID"
echo "Connect ID: $UUID"
echo ""

# 测试1: 使用V1 HTTP API相同的头部 (X-Api-App-Id, X-Api-Access-Key, X-Api-Resource-Id)
echo "测试1: V1 API头部格式 (X-Api-App-Id)"
wscat -H "X-Api-App-Id: $APP_ID" \
      -H "X-Api-Access-Key: $TOKEN" \
      -H "X-Api-Resource-Id: $RESOURCE_ID" \
      -H "X-Api-Connect-Id: $UUID" \
      -c "$ENDPOINT" 2>&1 || echo "连接失败"

echo ""
echo ""

# 测试2: 使用Bearer Token格式 (Authorization: Bearer;token)
echo "测试2: Bearer Token格式 (Authorization: Bearer;token)"
wscat -H "Authorization: Bearer;$TOKEN" \
      -H "X-Api-Resource-Id: $RESOURCE_ID" \
      -H "X-Api-Connect-Id: $UUID" \
      -c "$ENDPOINT" 2>&1 || echo "连接失败"

echo ""
echo ""

# 测试3: 使用X-Api-App-Key (ASR API格式)
echo "测试3: X-Api-App-Key格式 (ASR API)"
wscat -H "X-Api-App-Key: $APP_ID" \
      -H "X-Api-Access-Key: $TOKEN" \
      -H "X-Api-Resource-Id: $RESOURCE_ID" \
      -H "X-Api-Connect-Id: $UUID" \
      -c "$ENDPOINT" 2>&1 || echo "连接失败"

echo ""
echo "测试完成"