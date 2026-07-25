#ifndef VOCEKIT_DEEPSEEK_MODEL_PROVIDER_H
#define VOCEKIT_DEEPSEEK_MODEL_PROVIDER_H

#include "model_provider.h"
#include "provider_network_transport.h"

#include "../config/secret_config.h"

#include <QSharedPointer>

#include <functional>

// DeepSeek Provider 自己负责请求构造、响应解析和流式增量，不依赖旧接口实现。
class DeepSeekModelProvider : public IModelProvider
{
public:
    using SecretLoader = std::function<SecretConfig()>;

    explicit DeepSeekModelProvider(bool useSystemProxy = false);
    DeepSeekModelProvider(
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

    QSharedPointer<IProviderNetworkTransport> m_transport;
    SecretLoader m_secretLoader;
    SecretConfig m_secrets;
    bool m_useSystemProxy = false;
};

QSharedPointer<IModelProvider> createDeepSeekModelProvider(
    bool useSystemProxy = false
);

#endif // VOCEKIT_DEEPSEEK_MODEL_PROVIDER_H
