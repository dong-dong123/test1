#!/bin/bash
echo "=== 测试不同认证格式 $(date) ==="

TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"
RESOURCE_ID="volc.bigasr.sauc.duration"
CONNECT_ID="test-$(date +%s)-$(shuf -i 1000-9999 -n 1)"
# 生成随机的WebSocket Key
WS_KEY=$(openssl rand -base64 16 2>/dev/null | tr -d '\n')
if [ -z "$WS_KEY" ]; then
    WS_KEY="dGhlIHNhbXBsZSBub25jZQ==" # 备用固定key
fi

WS_URL="wss://openspeech.bytedance.com/api/v2/asr"

test_auth() {
    local name="$1"
    local headers="$2"
    
    echo ""
    echo "测试: $name"
    echo "头部: $headers"
    
    response=$(timeout 10 curl -i -X GET "$WS_URL" \
      -H "Host: openspeech.bytedance.com" \
      -H "Upgrade: websocket" \
      -H "Connection: Upgrade" \
      -H "Sec-WebSocket-Key: $WS_KEY" \
      -H "Sec-WebSocket-Version: 13" \
      $headers \
      --max-time 8 \
      --connect-timeout 8 \
      --http1.1 \
      -w "\n%{http_code}" \
      -s 2>&1)
    
    echo "响应:"
    echo "$response" | head -10
    if echo "$response" | grep -q "101"; then
        echo "✅ 成功"
        return 0
    else
        echo "❌ 失败"
        return 1
    fi
}

# 测试不同的认证格式
test_auth "1. Bearer;token (分号后无空格)" "-H \"Authorization: Bearer;$TOKEN\""
test_auth "2. Bearer token (空格)" "-H \"Authorization: Bearer $TOKEN\""
test_auth "3. X-Api-*头部 (完整)" "-H \"X-Api-App-Key: $APP_ID\" -H \"X-Api-Access-Key: $TOKEN\" -H \"X-Api-Resource-Id: $RESOURCE_ID\" -H \"X-Api-Connect-Id: $CONNECT_ID\""
test_auth "4. X-Api-App-Key + Bearer" "-H \"X-Api-App-Key: $APP_ID\" -H \"Authorization: Bearer;$TOKEN\""
test_auth "5. 仅X-Api-App-Key" "-H \"X-Api-App-Key: $APP_ID\""
test_auth "6. 无认证头部" ""

echo ""
echo "=== 总结 ==="
echo "请检查哪个格式成功。如果都失败，可能是网络或凭证问题。"
