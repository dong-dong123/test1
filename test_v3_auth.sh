#!/bin/bash

# 测试火山引擎V3 API的不同认证方式
# 尝试找出正确的认证头部格式

TOKEN="R23gVDqaVB_j-TaRfNywkJnerpGGJtcB"
APP_ID="2015527679"
RESOURCE_ID="volc.seedasr.auc"
API_URL="https://openspeech.bytedance.com/api/v3/auc/bigmodel/submit"
UUID="test-auth-$(date +%s)"

echo "=== 火山引擎V3 API认证方式测试 ==="
echo "目标：找出正确的认证头部格式"
echo ""

# 函数：测试特定认证头部
test_auth() {
    local test_name="$1"
    local headers="$2"

    echo "🔍 测试: $test_name"
    echo "头部: $headers"

    # 构建curl命令
    cmd="curl -X POST '$API_URL' $headers"
    cmd="$cmd -H 'Content-Type: application/json'"
    cmd="$cmd -d '{
        \"audio\": {
            \"format\": \"pcm\",
            \"language\": \"zh-CN\"
        },
        \"request\": {
            \"model_name\": \"bigmodel\"
        }
    }'"
    cmd="$cmd -w '\n%{http_code}' -s 2>/dev/null"

    response=$(eval $cmd)
    status=$(echo "$response" | tail -1)
    resp_body=$(echo "$response" | sed '$d')

    echo "状态码: $status"
    if [ "$status" = "200" ]; then
        echo "✅ 认证成功！"
        echo "响应: $resp_body"
    elif [ "$status" = "401" ] || [ "$status" = "403" ]; then
        echo "❌ 认证失败"
        echo "响应: $resp_body"
    else
        echo "📊 其他状态"
        echo "响应: $resp_body"
    fi
    echo ""
}

# 测试1: 官方示例格式（X-Api-Key）
test_auth "官方示例格式（X-Api-Key）" "-H 'X-Api-Key: $TOKEN' -H 'X-Api-Resource-Id: $RESOURCE_ID' -H 'X-Api-Request-Id: $UUID' -H 'X-Api-Sequence: -1'"

# 测试2: X-Api-Key 带Bearer前缀
test_auth "X-Api-Key带Bearer前缀" "-H 'X-Api-Key: Bearer $TOKEN' -H 'X-Api-Resource-Id: $RESOURCE_ID' -H 'X-Api-Request-Id: $UUID' -H 'X-Api-Sequence: -1'"

# 测试3: X-Api-Key 带Bearer;前缀（V2 WebSocket格式）
test_auth "X-Api-Key带Bearer;前缀" "-H 'X-Api-Key: Bearer;$TOKEN' -H 'X-Api-Resource-Id: $RESOURCE_ID' -H 'X-Api-Request-Id: $UUID' -H 'X-Api-Sequence: -1'"

# 测试4: V1 API格式（X-Api-App-Key + X-Api-Access-Key）
test_auth "V1 API格式（双头部）" "-H 'X-Api-App-Key: $APP_ID' -H 'X-Api-Access-Key: $TOKEN' -H 'X-Api-Resource-Id: $RESOURCE_ID' -H 'X-Api-Sequence: 1'"

# 测试5: 仅X-Api-App-Key
test_auth "仅X-Api-App-Key" "-H 'X-Api-App-Key: $APP_ID' -H 'X-Api-Resource-Id: $RESOURCE_ID' -H 'X-Api-Sequence: 1'"

# 测试6: 仅X-Api-Access-Key
test_auth "仅X-Api-Access-Key" "-H 'X-Api-Access-Key: $TOKEN' -H 'X-Api-Resource-Id: $RESOURCE_ID' -H 'X-Api-Sequence: 1'"

# 测试7: Authorization头部（传统方式）
test_auth "Authorization Bearer" "-H 'Authorization: Bearer $TOKEN' -H 'X-Api-Resource-Id: $RESOURCE_ID' -H 'X-Api-Sequence: 1'"

# 测试8: Authorization Bearer;格式
test_auth "Authorization Bearer;" "-H 'Authorization: Bearer;$TOKEN' -H 'X-Api-Resource-Id: $RESOURCE_ID' -H 'X-Api-Sequence: 1'"

# 测试9: 无认证头部（测试是否需要认证）
test_auth "无认证头部" "-H 'X-Api-Resource-Id: $RESOURCE_ID' -H 'X-Api-Sequence: 1'"

# 测试10: 测试不同的Resource ID格式
echo "🔍 测试不同Resource ID（使用V1认证方式）"
RESOURCE_IDS=(
    "volc.seedasr.auc"
    "volc.seedasr.sauc.duration"
    "volc.bigasr.sauc.duration"
    "volc.bigasr.auc"
    "Speech_Recognition_Seed_streaming2000000693011331714"
)

for rid in "${RESOURCE_IDS[@]}"; do
    echo "测试 Resource ID: $rid"
    response=$(curl -X POST "$API_URL" \
      -H "X-Api-App-Key: $APP_ID" \
      -H "X-Api-Access-Key: $TOKEN" \
      -H "X-Api-Resource-Id: $rid" \
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
        echo "✅ 成功: $rid 可用"
    elif [ "$status" = "400" ] && echo "$resp_body" | grep -q "no available instances"; then
        echo "⚠️  实例问题: $rid 认证通过但实例不可用"
    elif [ "$status" = "401" ] || [ "$status" = "403" ]; then
        echo "❌ 认证失败: $rid"
    else
        echo "📊 其他: $rid (状态码: $status)"
    fi
    echo "响应: $resp_body"
    echo ""
done

echo "=== 测试总结 ==="
echo "1. 如果任何测试返回200，说明找到了正确的认证方式"
echo "2. 如果返回'no available instances'，说明认证通过但实例配置有问题"
echo "3. 如果全部返回401/403，说明需要正确的API Key"
echo ""
echo "建议："
echo "- 登录火山引擎控制台，查找'API Key'或'应用密钥'"
echo "- 检查文档确认V3 API的认证方式"
echo "- 尝试创建新的应用或实例"