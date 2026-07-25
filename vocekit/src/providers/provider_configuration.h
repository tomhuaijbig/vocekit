#ifndef VOCEKIT_PROVIDER_CONFIGURATION_H
#define VOCEKIT_PROVIDER_CONFIGURATION_H

#include "../config/secret_config.h"

#include <QString>

// 纯配置校验：只判断所需字段是否填写，不创建网络客户端或发送请求。
QString speechProviderConfigurationErrorForSecrets(
    const SecretConfig &secrets,
    const QString &provider
);

bool isModelProviderConfiguredForSecrets(
    const SecretConfig &secrets,
    const QString &modelId
);

QString speechProviderConfigurationErrorFromStore(
    const QString &provider
);

bool isModelProviderConfiguredFromStore(const QString &modelId);

#endif // VOCEKIT_PROVIDER_CONFIGURATION_H
