#ifndef VOCEKIT_PROVIDER_NETWORK_TRANSPORT_H
#define VOCEKIT_PROVIDER_NETWORK_TRANSPORT_H

#include "provider_types.h"

#include "../tasks/cancellation_token.h"

#include <QNetworkRequest>
#include <QSharedPointer>

// Provider 只依赖这层网络接口，测试可替换传输实现，生产环境统一复用网络执行器。
class IProviderNetworkTransport
{
public:
    virtual ~IProviderNetworkTransport()
    {
    }

    virtual NetworkResponse get(
        const QNetworkRequest &request,
        const NetworkRequestOptions &options,
        const CancellationToken &cancellation
    ) = 0;

    virtual NetworkResponse postJson(
        const QNetworkRequest &request,
        const QByteArray &body,
        const NetworkRequestOptions &options,
        const CancellationToken &cancellation
    ) = 0;

    virtual NetworkResponse postEventStream(
        const QNetworkRequest &request,
        const QByteArray &body,
        const NetworkRequestOptions &options,
        const StreamDataCallback &onData,
        const CancellationToken &cancellation
    ) = 0;
};

QSharedPointer<IProviderNetworkTransport> createProviderNetworkTransport();

#endif // VOCEKIT_PROVIDER_NETWORK_TRANSPORT_H
