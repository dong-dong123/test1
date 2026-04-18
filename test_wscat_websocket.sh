#!/bin/bash

# 使用wscat测试火山引擎WebSocket连接（用户提供的完整示例）

APP_ID="2015527679"
TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
RESOURCE_ID="volc.bigasr.sauc.duration"  # ASR 1.0小时版（已验证正确）
WS_URL="wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async"

echo "=== 火山引擎WebSocket连接测试（使用wscat）==="
echo "API端点: $WS_URL"
echo "App ID: $APP_ID"
echo "Resource ID: $RESOURCE_ID (ASR 1.0小时版)"
echo ""

# 生成UUID
if command -v uuidgen &> /dev/null; then
    CONNECT_ID=$(uuidgen)
else
    CONNECT_ID="test-connect-$(date +%s)-$RANDOM"
fi

echo "Connect ID: $CONNECT_ID"
echo ""

# 检查wscat是否安装
if ! command -v wscat &> /dev/null; then
    echo "❌ wscat未安装"
    echo "安装命令: npm install -g wscat"
    echo ""
    echo "备用测试方法：使用curl测试HTTP升级"

    # 使用curl测试
    WS_KEY="dGhlIHNhbXBsZSBub25jZQ=="

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
    echo "$response" | head -15

    if echo "$response" | grep -q "101 Switching Protocols"; then
        echo ""
        echo "✅ WebSocket握手成功！"
        echo "认证和实例访问正常"
    elif echo "$response" | grep -q "401\|403"; then
        echo ""
        echo "❌ 认证失败或权限不足"
        echo "检查认证头部和Resource ID"
    else
        echo ""
        echo "📊 其他响应，可能需要完整WebSocket客户端测试"
    fi

    exit 0
fi

echo "🔍 使用wscat建立WebSocket连接"
echo "命令: wscat -H \"X-Api-App-Key: $APP_ID\" \\"
echo "             -H \"X-Api-Access-Key: $TOKEN\" \\"
echo "             -H \"X-Api-Resource-Id: $RESOURCE_ID\" \\"
echo "             -H \"X-Api-Connect-Id: $CONNECT_ID\" \\"
echo "             -c $WS_URL"
echo ""

echo "连接将在5秒后自动关闭..."
echo "按Ctrl+C中断测试"
echo ""

# 运行wscat（后台运行，5秒后关闭）
timeout 5s wscat \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Connect-Id: $CONNECT_ID" \
  -c "$WS_URL" 2>&1

echo ""
echo "=== 测试结果分析 ==="
echo ""

# 测试不同的Resource ID
echo "🔍 测试不同Resource ID的握手结果"
RESOURCE_IDS=(
    "volc.seedasr.sauc.duration"  # 小时版
    "volc.seedasr.sauc.concurrent"  # 并发版
    "volc.seedasr.auc"  # 异步识别
    "volc.bigasr.sauc.duration"  # ASR 1.0小时版
)

for rid in "${RESOURCE_IDS[@]}"; do
    echo -n "测试 $rid ... "

    response=$(curl -i -X GET "$WS_URL" \
      -H "Host: openspeech.bytedance.com" \
      -H "Upgrade: websocket" \
      -H "Connection: Upgrade" \
      -H "Sec-WebSocket-Key: $WS_KEY" \
      -H "Sec-WebSocket-Version: 13" \
      -H "X-Api-App-Key: $APP_ID" \
      -H "X-Api-Access-Key: $TOKEN" \
      -H "X-Api-Resource-Id: $rid" \
      -H "X-Api-Connect-Id: test-$(date +%s)" \
      --http1.1 \
      -s 2>/dev/null | head -5)

    if echo "$response" | grep -q "101"; then
        echo "✅ 握手成功"
    elif echo "$response" | grep -q "401"; then
        echo "❌ 认证失败"
    elif echo "$response" | grep -q "403"; then
        echo "❌ 权限不足（资源未授权）"
    else
        echo "📊 其他响应"
    fi
done

echo ""
echo "=== 总结与建议 ==="
echo "1. 如果握手成功，说明认证正确且实例可访问"
echo "2. 如果返回401，检查Access Token是否正确"
echo "3. 如果返回403，检查Resource ID是否正确（小时版用duration，并发版用concurrent）"
echo "4. 如果WebSocket连接成功但SSL错误-80，是ESP32 WebSocket库的问题"
echo ""
echo "下一步：根据握手结果调整Resource ID，然后测试ESP32上的SSL问题"