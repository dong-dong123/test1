#!/bin/bash

# 测试火山引擎V3异步录音文件识别API
# 根据用户提供的官方示例

TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"
RESOURCE_ID="volc.seedasr.auc"  # V3异步API的Resource ID
API_URL="https://openspeech.bytedance.com/api/v3/auc/bigmodel/submit"

# 生成随机UUID
UUID=$(uuidgen 2>/dev/null || echo "test-$(date +%s)-$RANDOM")

echo "=== 火山引擎V3异步录音文件识别API测试 ==="
echo "API URL: $API_URL"
echo "Resource ID: $RESOURCE_ID"
echo "Request ID: $UUID"
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
        if echo "$resp_body" | grep -q '"code":0'; then
            echo "🎉 V3 API正常工作！实例可访问。"
            # 提取task_id用于后续查询
            task_id=$(echo "$resp_body" | grep -o '"task_id":"[^"]*"' | cut -d'"' -f4)
            if [ -n "$task_id" ]; then
                echo "任务ID: $task_id"
                echo "注意：这是异步任务，需要轮询获取结果"
            fi
        else
            echo "⚠️  API返回非零错误码"
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
            echo "🚨 问题诊断: V3 API也返回'no available instances'"
            echo "这表明实例配置有问题，需要检查："
            echo "1. Resource ID是否正确（当前: $RESOURCE_ID）"
            echo "2. 实例类型是否支持此API"
            echo "3. 实例区域配置"
        fi
    else
        echo "❓ 未知状态: $status"
        echo "响应: $resp_body"
    fi
    echo ""
}

# 测试1: 使用官方示例格式（需要音频URL）
echo "📋 测试1: 官方示例格式（需要音频URL）"
echo "注意：需要提供真实的音频URL，这里使用测试URL"
echo ""
response=$(curl -X POST "$API_URL" \
  -H "X-Api-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Request-Id: $UUID" \
  -H "X-Api-Sequence: -1" \
  -H "Content-Type: application/json" \
  -d '{
    "user": {
        "uid": "test_user"
    },
    "audio": {
        "url": "https://example.com/test.wav",
        "format": "wav",
        "language": "zh-CN"
    },
    "request": {
        "model_name": "bigmodel",
        "enable_itn": true,
        "enable_punc": true,
        "enable_ddc": true,
        "enable_speaker_info": false
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
show_result "官方示例格式" "$status" "$resp_body"

# 测试2: 尝试本地音频数据（base64编码）
echo "📋 测试2: 使用base64音频数据（替代URL）"
echo "注意：测试是否支持直接音频数据上传"
echo ""
# 创建一个小的base64测试音频数据（静音）
BASE64_AUDIO="UklGRnoGAABXQVZFZm10IBAAAAABAAEAQB8AAEAfAAABAAgAZGF0YQoGAABXQVZFZm10IBAAAAABAAEAQB8AAEAfAAABAAgAZGF0YQo="

response=$(curl -X POST "$API_URL" \
  -H "X-Api-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Request-Id: test-$(date +%s)-001" \
  -H "X-Api-Sequence: -1" \
  -H "Content-Type: application/json" \
  -d '{
    "user": {
        "uid": "esp32_user"
    },
    "audio": {
        "format": "pcm",
        "language": "zh-CN",
        "data": "'"$BASE64_AUDIO"'"
    },
    "request": {
        "model_name": "bigmodel",
        "enable_itn": true,
        "enable_punc": true
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
show_result "Base64音频数据" "$status" "$resp_body"

# 测试3: 简化格式
echo "📋 测试3: 简化格式（最少必需字段）"
echo ""
response=$(curl -X POST "$API_URL" \
  -H "X-Api-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Request-Id: test-$(date +%s)-002" \
  -H "X-Api-Sequence: 1" \
  -H "Content-Type: application/json" \
  -d '{
    "audio": {
        "format": "pcm",
        "language": "zh-CN"
    },
    "request": {
        "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

status=$(echo "$response" | tail -1)
resp_body=$(echo "$response" | sed '$d')
show_result "简化格式" "$status" "$resp_body"

# 测试4: 测试不同的Resource ID
echo "📋 测试4: 测试不同的Resource ID"
echo "尝试可能的其他Resource ID格式"
echo ""

RESOURCE_IDS=(
    "volc.seedasr.auc"
    "volc.seedasr.sauc.duration"
    "volc.bigasr.sauc.duration"
    "volc.bigasr.auc"
)

for test_id in "${RESOURCE_IDS[@]}"; do
    echo "测试 Resource ID: $test_id"
    response=$(curl -X POST "$API_URL" \
      -H "X-Api-Key: $TOKEN" \
      -H "X-Api-Resource-Id: $test_id" \
      -H "X-Api-Request-Id: test-$(date +%s)-$RANDOM" \
      -H "X-Api-Sequence: 1" \
      -H "Content-Type: application/json" \
      -d '{
        "audio": {
            "format": "pcm",
            "language": "zh-CN"
        },
        "request": {
            "model_name": "bigmodel"
        }
      }' \
      -w "\n%{http_code}" \
      -s 2>/dev/null)

    status=$(echo "$response" | tail -1)
    resp_body=$(echo "$response" | sed '$d')

    if [ "$status" = "200" ]; then
        echo "✅ 成功: $test_id 可用"
    else
        echo "❌ 失败: $test_id 不可用"
    fi
    echo "响应: $resp_body"
    echo ""
done

echo "=== 测试完成 ==="
echo ""
echo "总结："
echo "1. 如果V3 API成功，说明我们之前使用了错误的API版本"
echo "2. 如果仍然失败，说明实例配置有问题"
echo "3. 成功的关键是找到正确的Resource ID和API端点"