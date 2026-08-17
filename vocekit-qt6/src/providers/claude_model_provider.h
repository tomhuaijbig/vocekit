#ifndef VOCEKIT_CLAUDE_MODEL_PROVIDER_H
#define VOCEKIT_CLAUDE_MODEL_PROVIDER_H

#include "model_provider.h"
#include "provider_network_transport.h"

#include "../config/secret_config.h"

#include <QSharedPointer>

#include <functional>

// Claude Provider 独立负责 Anthropic 普通请求、流式解析、自检和错误转换。
class ClaudeModelProvider : public IModelProvider
{
public:
    using SecretLoader = std::function<SecretConfig()>;

    explicit ClaudeModelProvider(bool useSystemProxy = false);
    ClaudeModelProvider(
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

QSharedPointer<IModelProvider> createClaudeModelProvider(
    bool useSystemProxy = false
);
QSharedPointer<IModelProvider> createClaudeModelProvider(
    const SecretConfig &secrets,
    bool useSystemProxy = false
);

#endif // VOCEKIT_CLAUDE_MODEL_PROVIDER_H
