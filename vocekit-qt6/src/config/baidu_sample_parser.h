#pragma once

#include <QString>

// 从百度智能云 AccessToken 示例代码中提取 client_id/client_secret。
// client_id 对应百度 API Key，client_secret 对应百度 Secret Key。
bool extractBaiduCredentialsFromSampleCode(const QString &code, QString *apiKey, QString *secretKey);
