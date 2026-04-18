#!/bin/bash

# 快速WebSocket连接测试脚本
# 测试两种Resource ID的连接能力

APP_ID="2015527679"
TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
WS_URL="wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async"

echo "=== 火山引擎V3 WebSocket快速连接测试 ==="
echo "API端点: $WS_URL"
echo "APP ID: $APP_ID"
echo "Token长度: ${#TOKEN} 字符"
echo ""

# 生成简单的Connect ID
CONNECT_ID="test_$(date +%s)_$(head -c 8 /dev/urandom | base64 | tr -dc 'a-zA-Z0-9' | cut -c1-8)"

# 测试两种Resource ID
RESOURCE_IDS=("volc.seedasr.sauc.concurrent" "volc.seedasr.sauc.duration")

for RESOURCE_ID in "${RESOURCE_IDS[@]}"; do
  echo ""
  echo "🔍 测试 Resource ID: $RESOURCE_ID"
  echo "========================================"
  echo "认证头部:"
  echo "  X-Api-App-Key: $APP_ID"
  echo "  X-Api-Access-Key: $(echo $TOKEN | cut -c1-10)..."
  echo "  X-Api-Resource-Id: $RESOURCE_ID"
  echo "  X-Api-Connect-Id: $CONNECT_ID"
  echo ""

  # 记录开始时间
  START_TIME=$(date +%s)

  echo "尝试连接（5秒超时）..."

  # 使用wscat测试连接（5秒超时）
  timeout 5 wscat \
    -H "X-Api-App-Key: $APP_ID" \
    -H "X-Api-Access-Key: $TOKEN" \
    -H "X-Api-Resource-Id: $RESOURCE_ID" \
    -H "X-Api-Connect-Id: $CONNECT_ID" \
    -c "$WS_URL" 2>&1 | tee /tmp/ws_test_output.txt

  EXIT_CODE=$?
  END_TIME=$(date +%s)
  DURATION=$((END_TIME - START_TIME))

  echo ""
  echo "连接持续时间: $DURATION 秒"
  echo "退出代码: $EXIT_CODE"

  # 分析结果
  if [ $EXIT_CODE -eq 124 ]; then
    echo "✅ 连接成功（5秒超时未断开）"
    echo "   - WebSocket握手成功"
    echo "   - 保持连接状态"
  elif [ $EXIT_CODE -eq 0 ]; then
    OUTPUT=$(cat /tmp/ws_test_output.txt)
    if echo "$OUTPUT" | grep -i "connected\|success\|101" > /dev/null; then
      echo "✅ 连接成功（正常断开）"
    else
      echo "⚠️  正常退出但未确认连接状态"
      echo "   输出: $OUTPUT"
    fi
  else
    OUTPUT=$(cat /tmp/ws_test_output.txt)
    echo "❌ 连接失败（退出代码: $EXIT_CODE）"
    echo "   错误输出:"
    echo "$OUTPUT" | head -10

    # 检查常见错误模式
    if echo "$OUTPUT" | grep -i "401\|unauthorized" > /dev/null; then
      echo "   可能原因: 认证失败（Token无效或过期）"
    elif echo "$OUTPUT" | grep -i "403\|forbidden" > /dev/null; then
      echo "   可能原因: 权限不足（Resource ID无效）"
    elif echo "$OUTPUT" | grep -i "ssl\|certificate" > /dev/null; then
      echo "   可能原因: SSL证书验证失败"
    elif echo "$OUTPUT" | grep -i "timeout\|connection refused" > /dev/null; then
      echo "   可能原因: 网络连接问题"
    fi
  fi

  echo "========================================"
done

echo ""
echo "=== 测试总结 ==="
echo "1. 如果两种Resource ID都失败：检查认证Token和网络连接"
echo "2. 如果只有一种成功：使用成功的Resource ID更新代码"
echo "3. 如果都成功：选择适合您计费模式的Resource ID"
echo ""
echo "下一步：根据测试结果更新代码配置"