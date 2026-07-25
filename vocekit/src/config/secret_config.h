#pragma once

#include <QString>
#include <QVector>

struct CustomModelProfile
{
    QString id;
    QString name;
    QString url;
    QString apiKey;
    QString model;

    bool hasEndpoint() const { return !url.trimmed().isEmpty(); }
};

// 自定义大模型 ID 会写入配置和功能设置，统一清洗可以避免同一模型出现多个写法。
QString normalizeCustomModelProfileId(QString id);

// 接口密钥结构：集中保存语音识别、OCR 和大模型服务的密钥，不把密钥散落在业务代码里。
struct SecretConfig
{
    QString deepseekApiKey;
    QString openaiApiKey;
    QString anthropicApiKey;
    QString baiduApiKey;
    QString baiduSecretKey;
    QString baiduAppId;
    QString xfyunAppId;
    QString xfyunApiKey;
    QString xfyunApiSecret;
    QString customSpeechUrl;
    QString customSpeechApiKey;
    QString customSpeechModel;
    QString customOcrUrl;
    QString customOcrApiKey;
    QString customOcrModel;
    QString customModelUrl;
    QString customModelApiKey;
    QString customModelName;
    QVector<CustomModelProfile> customModels;

    bool hasDeepSeek() const { return !deepseekApiKey.trimmed().isEmpty(); }
    bool hasOpenAI() const { return !openaiApiKey.trimmed().isEmpty(); }
    bool hasAnthropic() const { return !anthropicApiKey.trimmed().isEmpty(); }
    bool hasBaidu() const { return !baiduApiKey.trimmed().isEmpty() && !baiduSecretKey.trimmed().isEmpty(); }
    bool hasXfyun() const;
    bool hasCustomSpeech() const;
    bool hasCustomModel() const;
    QVector<CustomModelProfile> effectiveCustomModels() const;
    CustomModelProfile customModelProfileForProviderId(const QString &providerId) const;
};

SecretConfig loadSecrets();
bool saveSecrets(const SecretConfig &secrets);
