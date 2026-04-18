#!/bin/bash

# 测试V1双头部认证在V3 API上的有效性
# 关键测试：V1认证方式 + V3必需的头部

TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"
RESOURCE_ID="volc.seedasr.auc"
API_URL="https://openspeech.bytedance.com/api/v3/auc/bigmodel/submit"
UUID="test-v1v3-$(date +%s)"

echo "=== V1双头部认证在V3 API上的测试 ==="
echo "假设：V3 API可能支持V1的认证方式"
echo ""

# 测试1: V1认证 + V3必需头部
echo "🔍 测试1: V1双头部 + X-Api-Request-Id + X-Api-Sequence: -1"
response=$(curl -X POST "$API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Request-Id: $UUID" \
  -H "X-Api-Sequence: -1" \
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

echo "状态码: $status"
echo "响应: $resp_body"
echo ""

if [ "$status" = "200" ]; then
    echo "✅ 成功！V3 API支持V1双头部认证"
    echo "这意味着："
    echo "1. 我们的token有效"
    echo "2. 实例配置可能有问题"
    echo "3. 需要检查Resource ID"
elif [ "$status" = "400" ]; then
    if echo "$resp_body" | grep -q "no available instances"; then
        echo "⚠️  认证通过，但实例不可用"
        echo "这表明："
        echo "1. 认证正确"
        echo "2. Resource ID或实例配置有问题"
    else
        echo "📊 其他400错误"
        echo "可能需要不同的请求格式"
    fi
elif [ "$status" = "401" ] || [ "$status" = "403" ]; then
    echo "❌ 认证失败"
    echo "V3 API不支持V1双头部认证"
else
    echo "📊 其他状态"
fi

echo ""

# 测试2: 对比V1 API和V3 API
echo "🔍 测试2: 对比V1 API和V3 API的响应"
echo "运行相同的请求到V1 API（/api/v1/asr）"
echo ""

V1_RESPONSE=$(curl -X POST "https://openspeech.bytedance.com/api/v1/asr" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: volc.seedasr.sauc.duration" \
  -H "X-Api-Sequence: 1" \
  -H "Content-Type: application/json" \
  -d '{
    "app": {
      "appid": "'"$APP_ID"'",
      "token": "'"$TOKEN"'"
    },
    "audio": {"format": "pcm"},
    "request": {
      "reqid": "test_compare",
      "sequence": 1,
      "model_name": "bigmodel"
    }
  }' \
  -w "\n%{http_code}" \
  -s 2>/dev/null)

V1_STATUS=$(echo "$V1_RESPONSE" | tail -1)
V1_BODY=$(echo "$V1_RESPONSE" | sed '$d')

echo "V1 API状态码: $V1_STATUS"
echo "V1 API响应: $V1_BODY"
echo ""

# 测试3: 尝试不同的请求格式（匹配V3示例）
echo "🔍 测试3: 使用V3示例的完整格式（V1认证方式）"
response=$(curl -X POST "$API_URL" \
  -H "X-Api-App-Key: $APP_ID" \
  -H "X-Api-Access-Key: $TOKEN" \
  -H "X-Api-Resource-Id: $RESOURCE_ID" \
  -H "X-Api-Request-Id: test-format-$(date +%s)" \
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

echo "状态码: $status"
echo "响应: $resp_body"
echo ""

# 测试4: 测试不同的Resource ID（使用V1认证 + V3头部）
echo "🔍 测试4: 测试不同Resource ID（V1认证 + V3头部）"
RESOURCE_IDS=(
    "volc.seedasr.auc"
    "volc.seedasr.sauc.duration"
    "volc.bigasr.sauc.duration"
    "Speech_Recognition_Seed_streaming2000000693011331714"
)

for rid in "${RESOURCE_IDS[@]}"; do
    echo "测试 Resource ID: $rid"
    response=$(curl -X POST "$API_URL" \
      -H "X-Api-App-Key: $APP_ID" \
      -H "X-Api-Access-Key: $TOKEN" \
      -H "X-Api-Resource-Id: $rid" \
      -H "X-Api-Request-Id: test-rid-$(date +%s)-$RANDOM" \
      -H "X-Api-Sequence: -1" \
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

    echo "状态码: $status"
    if [ "$status" = "200" ]; then
        echo "✅ 成功！正确的Resource ID是: $rid"
    elif [ "$status" = "400" ]; then
        if echo "$resp_body" | grep -q "no available instances"; then
            echo "⚠️  实例问题: 认证通过但实例不可用"
        else
            echo "📊 其他400错误"
        fi
    else
        echo "📊 状态: $status"
    fi
    echo "响应: $resp_body"
    echo ""
done

echo "=== 结论 ==="
echo "根据测试结果："
echo "1. 如果V1认证在V3 API上成功，说明我们的token有效但用错了API版本"
echo "2. 如果返回'no available instances'，说明需要正确的Resource ID"
echo "3. 如果全部失败，需要获取正确的X-Api-Key"