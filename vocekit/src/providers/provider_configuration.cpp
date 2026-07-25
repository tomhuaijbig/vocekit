#include "provider_configuration.h"

#include "../config/app_settings_defaults.h"

QString speechProviderConfigurationErrorForSecrets(
    const SecretConfig &secrets,
    const QString &provider
)
{
    const QString normalized = normalizeSpeechProvider(provider);
    if (normalized == speechProviderXfyun()) {
        return secrets.hasXfyun()
            ? QString()
            : QString::fromUtf8(
                "缺少讯飞语音听写密钥。请在“设置 -> 接口”中填写讯飞 AppID、API Key 和 API Secret。"
            );
    }
    if (normalized == speechProviderCustom()) {
        return secrets.hasCustomSpeech()
            ? QString()
            : QString::fromUtf8(
                "缺少自定义语音接口地址。请在“设置 -> 接口”中填写自定义语音接口地址。"
            );
    }
    return secrets.hasBaidu()
        ? QString()
        : QString::fromUtf8(
            "缺少百度语音识别密钥。请在“设置 -> 接口”中填写百度 API Key 和 Secret Key。"
        );
}

bool isModelProviderConfiguredForSecrets(
    const SecretConfig &secrets,
    const QString &modelId
)
{
    const QString provider = modelProvider(modelId);
    if (provider == QStringLiteral("openai")) {
        return secrets.hasOpenAI();
    }
    if (provider == QStringLiteral("claude")) {
        return secrets.hasAnthropic();
    }
    if (provider == QStringLiteral("custom")) {
        return secrets
            .customModelProfileForProviderId(providerModelId(modelId))
            .hasEndpoint();
    }
    return secrets.hasDeepSeek();
}

QString speechProviderConfigurationErrorFromStore(
    const QString &provider
)
{
    return speechProviderConfigurationErrorForSecrets(
        loadSecrets(),
        provider
    );
}

bool isModelProviderConfiguredFromStore(const QString &modelId)
{
    return isModelProviderConfiguredForSecrets(loadSecrets(), modelId);
}
