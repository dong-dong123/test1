@echo off
chcp 65001 >nul
echo ========================================
echo 火山引擎语音识别API Token验证
echo ========================================
echo Token: R23gVDqaVB_j-TaRfNywkJnerpGGJtcB
echo App ID: 2015527679
echo API URL: https://openspeech.bytedance.com/api/v1/asr
echo.

echo 测试1: 基本请求（无音频）
curl -X POST "https://openspeech.bytedance.com/api/v1/asr" ^
  -H "Authorization: Bearer;R23gVDqaVB_j-TaRfNywkJnerpGGJtcB" ^
  -H "X-App-Id: 2015527679" ^
  -H "Content-Type: application/json" ^
  -d "{\"user\":{\"uid\":\"test_user\"},\"audio\":{\"format\":\"pcm\",\"codec\":\"raw\",\"rate\":16000,\"bits\":16,\"channel\":1,\"language\":\"zh-CN\"},\"request\":{\"reqid\":\"test_req_123\",\"model_name\":\"bigmodel\",\"enable_itn\":true,\"enable_punc\":true,\"enable_ddc\":false},\"audio_data\":\"UklGRlQAAABXQVZFZm10IBAAAAABAAEARKwAAIhYAQACABAAZGF0YQ==\"}"

echo.
echo ========================================
echo.

echo 测试2: 最小化请求
curl -X POST "https://openspeech.bytedance.com/api/v1/asr" ^
  -H "Authorization: Bearer;R23gVDqaVB_j-TaRfNywkJnerpGGJtcB" ^
  -H "X-App-Id: 2015527679" ^
  -H "Content-Type: application/json" ^
  -d "{\"user\":{\"uid\":\"test\"},\"audio\":{\"format\":\"pcm\"},\"request\":{\"reqid\":\"test\"}}"

echo.
echo ========================================
echo.

echo 测试3: 头部验证测试（GET请求）
curl -X GET "https://openspeech.bytedance.com/api/v1/asr" ^
  -H "Authorization: Bearer;R23gVDqaVB_j-TaRfNywkJnerpGGJtcB" ^
  -H "X-App-Id: 2015527679" ^
  -I

echo.
echo ========================================
echo 测试完成
pause