#include "provider_network_transport.h"

#include "network_request_executor.h"

namespace {

class ProviderNetworkTransport : public IProviderNetworkTransport
{
public:
    NetworkResponse get(
        const QNetworkRequest &request,
        const NetworkRequestOptions &options,
        const CancellationToken &cancellation) override
    {
        return NetworkRequestExecutor::get(
            request,
            options,
            cancellation
        );
    }

    NetworkResponse postJson(
        const QNetworkRequest &request,
        const QByteArray &body,
        const NetworkRequestOptions &options,
        const CancellationToken &cancellation) override
    {
        return NetworkRequestExecutor::postJson(
            request,
            body,
            options,
            cancellation
        );
    }

    NetworkResponse postEventStream(
        const QNetworkRequest &request,
        const QByteArray &body,
        const NetworkRequestOptions &options,
        const StreamDataCallback &onData,
        const CancellationToken &cancellation) override
    {
        return NetworkRequestExecutor::postEventStream(
            request,
            body,
            options,
            onData,
            cancellation
        );
    }
};

} // namespace

QSharedPointer<IProviderNetworkTransport> createProviderNetworkTransport()
{
    return QSharedPointer<IProviderNetworkTransport>(
        new ProviderNetworkTransport
    );
}
