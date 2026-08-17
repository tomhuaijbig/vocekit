#ifndef VOCEKIT_OPENAI_COMPATIBLE_MODEL_PROVIDER_H
#define VOCEKIT_OPENAI_COMPATIBLE_MODEL_PROVIDER_H

#include "model_provider.h"
#include "provider_network_transport.h"

#include "../config/secret_config.h"

#include <QSharedPointer>

#include <functional>

// OpenAI 兼容 Provider 同时承载官方 OpenAI 和用户配置的多个兼容接口。
class OpenAiCompatibleModelProvider : public IModelProvider
{
public:
    using SecretLoader = std::function<SecretConfig()>;

    explicit OpenAiCompatibleModelProvider(
        const QString &providerId,
        bool useSystemProxy = false
    );
    OpenAiCompatibleModelProvider(
        const QString &providerId,
        const QSharedPointer<IProviderNetworkTransport> &transport,
        const SecretLoader &secretLoader,
        bool useSystemProxy = false
    );

    QString id() const override;
    ProviderCheckResult checkConfiguration(
        const CancellationToken &cancellation = CancellationToken()
    ) const override;
    ModelResult complete(
        const ModelRequest &request,
        const ModelDeltaCallback &onDelta,
        const CancellationToken &cancellation
    ) override;
    void refreshConfiguration() override;

private:
    ModelResult execute(
        const ModelRequest &request,
        const ModelDeltaCallback &onDelta,
        const CancellationToken &cancellation
    ) const;

    QString m_providerId;
    QSharedPointer<IProviderNetworkTransport> m_transport;
    SecretLoader m_secretLoader;
    SecretConfig m_secrets;
    bool m_useSystemProxy = false;
};

QSharedPointer<IModelProvider> createOpenAiCompatibleModelProvider(
    const QString &providerId,
    bool useSystemProxy = false
);
QSharedPointer<IModelProvider> createOpenAiCompatibleModelProvider(
    const QString &providerId,
    const SecretConfig &secrets,
    bool useSystemProxy = false
);

#endif // VOCEKIT_OPENAI_COMPATIBLE_MODEL_PROVIDER_H
