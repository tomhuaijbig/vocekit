#ifndef VOCEKIT_PROVIDER_STREAMING_WEBSOCKET_TRANSPORT_H
#define VOCEKIT_PROVIDER_STREAMING_WEBSOCKET_TRANSPORT_H

#include "provider_types.h"

#include <QByteArray>
#include <QSharedPointer>
#include <QUrl>

#include <functional>

struct ProviderStreamingWebSocketCallbacks
{
    std::function<void()> opened;
    std::function<void(const QByteArray &)> textMessage;
    std::function<void(const OperationError &)> failed;
    std::function<void(bool cancelled)> closed;
};

// 一个实例只负责一个实时 WebSocket 连接，支持录音期间持续推送帧。
class IProviderStreamingWebSocketTransport
{
public:
    virtual ~IProviderStreamingWebSocketTransport()
    {
    }

    virtual void open(
        const QUrl &url,
        const NetworkRequestOptions &options,
        const ProviderStreamingWebSocketCallbacks &callbacks
    ) = 0;
    virtual void sendText(const QByteArray &message) = 0;
    virtual void sendBinary(const QByteArray &message) = 0;
    virtual void closeNormally() = 0;
    virtual void cancel() = 0;
};

QSharedPointer<IProviderStreamingWebSocketTransport>
createProviderStreamingWebSocketTransport();

#endif // VOCEKIT_PROVIDER_STREAMING_WEBSOCKET_TRANSPORT_H
