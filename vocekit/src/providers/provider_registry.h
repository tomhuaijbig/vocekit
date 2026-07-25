#ifndef VOCEKIT_PROVIDER_REGISTRY_H
#define VOCEKIT_PROVIDER_REGISTRY_H

#include "model_provider.h"
#include "speech_provider.h"

#include <QMap>
#include <QReadWriteLock>
#include <QSharedPointer>
#include <QStringList>

// 提供商注册中心通过稳定编号路由具体语音和模型适配器。
class ProviderRegistry
{
public:
    bool addSpeechProvider(
        const QSharedPointer<ISpeechProvider> &provider
    );
    bool addModelProvider(
        const QSharedPointer<IModelProvider> &provider
    );

    QSharedPointer<ISpeechProvider> speechProvider(
        const QString &id
    ) const;
    QSharedPointer<IModelProvider> modelProvider(
        const QString &id
    ) const;

    QStringList speechProviderIds() const;
    QStringList modelProviderIds() const;
    ProviderCheckResult checkSpeechProvider(
        const QString &id,
        const CancellationToken &cancellation = CancellationToken()
    ) const;
    ProviderCheckResult checkModelProvider(
        const QString &id,
        const CancellationToken &cancellation = CancellationToken()
    ) const;
    void refreshConfiguration();

private:
    static ProviderCheckResult missingProvider(const QString &id);

    mutable QReadWriteLock m_lock;
    QMap<QString, QSharedPointer<ISpeechProvider>> m_speechProviders;
    QMap<QString, QSharedPointer<IModelProvider>> m_modelProviders;
};

#endif // VOCEKIT_PROVIDER_REGISTRY_H
