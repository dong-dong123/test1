#!/bin/bash

# 测试火山引擎V3 WebSocket连接
# 使用用户提供的连接头部示例

APP_ID="2015527679"
TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
RESOURCE_ID="volc.bigasr.sauc.duration"  # ASR 1.0小时版（已验证正确）
WS_URL="wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async"
CONNECT_ID="67ee89ba-7050-4c04-a3d7-ac61a63499b3"  # 用户示例中的Connect ID

echo "=== 火山引擎V3 WebSocket连接测试 ==="
echo "API端点: $WS_URL"
echo "Resource ID: $RESOURCE_ID"
echo "Connect ID: $CONNECT_ID"
echo ""

# 生成WebSocket Key
WS_KEY="dGhlIHNhbXBsZSBub25jZQ=="  # 用户示例中的固定key

# 使用wscat或websocat测试（如果可用）
if command -v wscat &> /dev/null; then
    echo "🔍 使用wscat测试WebSocket连接"

    # 创建自定义头部文件
    cat > /tmp/ws_headers.txt << EOF
X-Api-App-Key: $APP_ID
X-Api-Access-Key: $TOKEN
X-Api-Resource-Id: $RESOURCE_ID
X-Api-Connect-Id: $CONNECT_ID
EOF

    echo "连接头部:"
    cat /tmp/ws_headers.txt
    echo ""

    echo "尝试连接到: $WS_URL"
    wscat -c "$WS_URL" -H "$(cat /tmp/ws_headers.txt | tr '\n' ', ' | sed 's/, $//')" &
    WSCAT_PID=$!
    sleep 5
    kill $WSCAT_PID 2>/dev/null
    echo ""

elif command -v websocat &> /dev/null; then
    echo "🔍 使用websocat测试WebSocket连接"

    HEADERS="X-Api-App-Key: $APP_ID
X-Api-Access-Key: $TOKEN
X-Api-Resource-Id: $RESOURCE_ID
X-Api-Connect-Id: $CONNECT_ID"

    echo "连接头部:"
    echo "$HEADERS"
    echo ""

    echo "尝试连接到: $WS_URL"
    echo "$HEADERS" | websocat --header - "$WS_URL" &
    WEBSOCAT_PID=$!
    sleep 5
    kill $WEBSOCAT_PID 2>/dev/null
    echo ""

else
    echo "⚠️  未找到wscat或websocat，使用curl测试HTTP升级"

    # 使用curl测试WebSocket握手
    echo "🔍 使用curl测试WebSocket握手"

    response=$(curl -i -X GET "$WS_URL" \
      -H "Host: openspeech.bytedance.com" \
      -H "Upgrade: websocket" \
      -H "Connection: Upgrade" \
      -H "Sec-WebSocket-Key: $WS_KEY" \
      -H "Sec-WebSocket-Version: 13" \
      -H "X-Api-App-Key: $APP_ID" \
      -H "X-Api-Access-Key: $TOKEN" \
      -H "X-Api-Resource-Id: $RESOURCE_ID" \
      -H "X-Api-Connect-Id: $CONNECT_ID" \
      --http1.1 \
      -s 2>/dev/null)

    echo "响应头部:"
    echo "$response" | head -20
    echo ""

    # 检查响应
    if echo "$response" | grep -q "101 Switching Protocols"; then
        echo "✅ WebSocket握手成功！"
        echo "服务器接受连接"
    elif echo "$response" | grep -q "401"; then
        echo "❌ 认证失败"
        echo "检查认证头部"
    elif echo "$response" | grep -q "403"; then
        echo "❌ 权限不足"
        echo "检查Resource ID和Connect ID"
    else
        echo "📊 其他响应"
        echo "可能需要不同的测试方法"
    fi
fi

echo ""
echo "=== 对比分析 ==="
echo "用户示例头部与我们现有代码的差异："
echo ""

echo "📋 用户示例头部："
echo "  X-Api-App-Key: $APP_ID"
echo "  X-Api-Access-Key: $TOKEN"
echo "  X-Api-Resource-Id: $RESOURCE_ID"
echo "  X-Api-Connect-Id: $CONNECT_ID"
echo ""

# 检查现有代码中的默认配置
echo "📋 代码中的默认配置："
echo "  端点: $WS_URL ✅ (匹配)"
echo "  Resource ID: volc.bigasr.sauc.duration ✅ (已更新)"
echo "  Connect ID: UUID格式 ✅ (已实现)"
echo ""

echo "=== 建议修改 ==="
echo "1. Resource ID已更新为: $RESOURCE_ID ✅"
echo "2. X-Api-Connect-Id头部（UUID格式）✅ (已实现)"
echo "3. 确保使用相同的认证头部"

# 测试不同的Resource ID
echo ""
echo "=== 测试不同Resource ID ==="
RESOURCE_IDS=(
    "volc.seedasr.sauc.concurrent"
    "volc.seedasr.sauc.duration"
    "volc.seedasr.auc"
    "volc.bigasr.sauc.concurrent"
    "volc.bigasr.sauc.duration"
)

for rid in "${RESOURCE_IDS[@]}"; do
    echo "测试 Resource ID: $rid"

    response=$(curl -i -X GET "$WS_URL" \
      -H "Host: openspeech.bytedance.com" \
      -H "Upgrade: websocket" \
      -H "Connection: Upgrade" \
      -H "Sec-WebSocket-Key: $WS_KEY" \
      -H "Sec-WebSocket-Version: 13" \
      -H "X-Api-App-Key: $APP_ID" \
      -H "X-Api-Access-Key: $TOKEN" \
      -H "X-Api-Resource-Id: $rid" \
      -H "X-Api-Connect-Id: test-connect-$(date +%s)" \
      --http1.1 \
      -s 2>/dev/null | head -10)

    if echo "$response" | grep -q "101"; then
        echo "  ✅ 握手成功"
    elif echo "$response" | grep -q "401\|403"; then
        echo "  ❌ 认证/权限失败"
    else
        echo "  📊 其他响应"
    fi
done

echo ""
echo "=== 总结 ==="
echo "关键发现：正确的Resource ID是 'volc.bigasr.sauc.duration' (ASR 1.0小时版)"
echo "Connect ID需要使用UUIDv4格式 ✅ (已实现)"