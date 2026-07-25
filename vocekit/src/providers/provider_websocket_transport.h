#ifndef VOCEKIT_PROVIDER_WEBSOCKET_TRANSPORT_H
#define VOCEKIT_PROVIDER_WEBSOCKET_TRANSPORT_H

#include "provider_types.h"

#include "../tasks/cancellation_token.h"

#include <QList>
#include <QSharedPointer>
#include <QUrl>

#include <functional>

struct ProviderWebSocketRequest
{
    QUrl url;
    QList<QByteArray> textFrames;
    int frameIntervalMs = 40;
    NetworkRequestOptions network;
};

struct ProviderWebSocketResult
{
    ExecutionId executionId;
    QList<QByteArray> messages;
    OperationError error;
    bool cancelled = false;
    qint64 durationMs = -1;
};

using WebSocketCompletionPredicate =
    std::function<bool(const QByteArray &)>;

// WebSocket Provider 只描述地址、发送帧和完成条件；连接生命周期集中在传输层。
class IProviderWebSocketTransport
{
public:
    virtual ~IProviderWebSocketTransport()
    {
    }

    virtual ProviderWebSocketResult exchange(
        const ProviderWebSocketRequest &request,
        const WebSocketCompletionPredicate &completion,
        const CancellationToken &cancellation
    ) = 0;
};

QSharedPointer<IProviderWebSocketTransport>
createProviderWebSocketTransport();

#endif // VOCEKIT_PROVIDER_WEBSOCKET_TRANSPORT_H
