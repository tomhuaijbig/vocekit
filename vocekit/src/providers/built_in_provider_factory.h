#ifndef VOCEKIT_BUILT_IN_PROVIDER_FACTORY_H
#define VOCEKIT_BUILT_IN_PROVIDER_FACTORY_H

#include "model_provider.h"
#include "provider_registry.h"
#include "speech_provider.h"

#include <QSharedPointer>

// 集中创建和注册内置 Provider，业务任务只依赖 Provider 接口。
QSharedPointer<ISpeechProvider> createBuiltInSpeechProvider(
    const QString &providerId,
    bool useSystemProxy = false
);

QSharedPointer<IModelProvider> createBuiltInModelProvider(
    const QString &providerId,
    bool useSystemProxy = false
);

void registerBuiltInProviders(
    ProviderRegistry *registry,
    bool useSystemProxy = false
);

#endif // VOCEKIT_BUILT_IN_PROVIDER_FACTORY_H
