#!/bin/bash

# 火山引擎语音识别API Resource ID测试脚本
# 包含购买时长包后的验证功能

TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"
RESOURCE_ID="Speech_Recognition_Seed_streaming2000000693011331714"
API_URL="https://openspeech.bytedance.com/api/v1/asr"

echo "=== 火山引擎语音识别API Resource ID测试 ==="
echo "App ID: $APP_ID"
echo "API URL: $API_URL"
echo ""

# 函数：显示测试结果
show_result() {
    local test_name="$1"
    local status="$2"
    local resp_body="$3"

    echo "--- $test_name ---"
    echo "状态码: $status"
    if [ "$status" = "200" ]; then
        echo "✅ 成功: API请求成功"
        echo "响应: $resp_body"

        # 检查响应内容
        if echo "$resp_body" | grep -q '"code":0' && echo "$resp_body" | grep -q '"message":"success"'; then
            echo "🎉 语音识别实例已激活并正常工作！"
        else
            echo "⚠️  注意: API返回成功但内容可能有问题"
        fi
    elif [ "$status" = "400" ]; then
        echo "❌ 失败: 请求格式错误"
        echo "响应: $resp_body"
    elif [ "$status" = "401" ]; then
        echo "❌ 失败: 认证错误"
        echo "响应: $resp_body"
    elif [ "$status" = "403" ]; then
        echo "❌ 失败: 权限不足或实例不可用"
        echo "响应: $resp_body"
        if echo "$resp_body" | grep -q "no available instances"; then
            echo ""
            echo "🚨 问题诊断: 实例状态可能为'暂停'"
            echo "解决方案:"
            echo "1. 登录火山引擎控制台"
            echo "2. 找到实例 'xiaozhi'"
            echo "3. 点击'购买时长包'激活实例"
            echo "4. 等待1-5分钟生效"
            echo "5. 重新运行此测试脚本"
        fi
    else
        echo "❓ 未知状态: $status"
        echo "响应: $resp_body"
    fi
    echo ""
}

# 测试1: 使用用户提供的Resource ID
echo "📋 测试1: 使用用户提供的Resource ID"
echo "Resource ID: $RESOURCE_ID"
echo "注意：此实例需要在控制台购买时长包激活"
echo ""
response=$(curl -X POST "$API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Sequence: 1" \
  -H "Content-Type: application/json" \
  -d '{
    "app": {
      "appid": "'"$APP_ID"'",
      "token": "'"$TOKEN"'"
    },
    "audio": {"format": "pcm"},
    "request": {
      "reqid": "test_instance",
      "sequence": 1,
      "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
echo "状态码: $status"
echo "响应: $resp_body"
echo ""

# 测试2: 尝试使用标准格式（如果上面失败）
echo "测试2: 尝试标准Resource ID格式"
STANDARD_RESOURCE_ID="volc.seedasr.sauc.duration"
response=$(curl -X POST "$API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $STANDARD_RESOURCE_ID" \
  -H "X-Api-Sequence: 1" \
  -H "Content-Type: application/json" \
  -d '{
    "app": {
      "appid": "'"$APP_ID"'",
      "token": "'"$TOKEN"'"
    },
    "audio": {"format": "pcm"},
    "request": {
      "reqid": "test_standard",
      "sequence": 1,
      "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
echo "状态码: $status"
echo "响应: $resp_body"
echo ""

# 测试3: 测试ASR 1.0格式
echo "测试3: 测试ASR 1.0格式"
ASR1_RESOURCE_ID="volc.bigasr.sauc.duration"
response=$(curl -X POST "$API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $ASR1_RESOURCE_ID" \
  -H "X-Api-Sequence: 1" \
  -H "Content-Type: application/json" \
  -d '{
    "app": {
      "appid": "'"$APP_ID"'",
      "token": "'"$TOKEN"'"
    },
    "audio": {"format": "pcm"},
    "request": {
      "reqid": "test_asr1",
      "sequence": 1,
      "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
echo "状态码: $status"
echo "响应: $resp_body"
echo ""

echo "=== 测试完成 ==="
echo ""
echo "如果所有测试都失败（特别是测试1），问题很可能是："
echo "1. 实例处于'暂停'状态（需要恢复）"
echo "2. 用量限额为0（需要购买时长包）"
echo "3. Resource ID格式不对（尝试其他格式）"