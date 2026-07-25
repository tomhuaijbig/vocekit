#include "provider_websocket_transport.h"

#include "../result_flow_config.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QTimer>
#include <QtWebSockets/QWebSocket>

namespace {

OperationError transportError(
    const QString &code,
    const QString &message,
    const QString &detail = QString(),
    bool retryable = false)
{
    OperationError error;
    error.code = code;
    error.message = message;
    error.detail = detail;
    error.retryable = retryable;
    return error;
}

QNetworkProxy proxyForRequest(const ProviderWebSocketRequest &request)
{
    const QString policy = resolveNetworkPolicy(
        request.network.networkPolicy,
        request.network.globalUseSystemProxy
    );
    if (policy != QStringLiteral("systemProxy")) {
        return QNetworkProxy(QNetworkProxy::NoProxy);
    }

    const QList<QNetworkProxy> proxies =
        QNetworkProxyFactory::systemProxyForQuery(
            QNetworkProxyQuery(request.url)
        );
    for (const QNetworkProxy &proxy : proxies) {
        if (proxy.type() != QNetworkProxy::DefaultProxy) {
            return proxy;
        }
    }
    return QNetworkProxy(QNetworkProxy::NoProxy);
}

class ProviderWebSocketTransport : public IProviderWebSocketTransport
{
public:
    ProviderWebSocketResult exchange(
        const ProviderWebSocketRequest &request,
        const WebSocketCompletionPredicate &completion,
        const CancellationToken &cancellation) override
    {
        ProviderWebSocketResult result;
        result.executionId = cancellation.executionId();
        if (cancellation.isCancellationRequested()) {
            result.cancelled = true;
            result.error = transportError(
                QStringLiteral("request.cancelled"),
                QString::fromUtf8("请求已取消。")
            );
            return result;
        }
        if (!request.url.isValid()
            || request.url.scheme().compare(
                   QStringLiteral("wss"),
                   Qt::CaseInsensitive
               ) != 0
            || request.textFrames.isEmpty()) {
            result.error = transportError(
                QStringLiteral("websocket.invalid_request"),
                QString::fromUtf8("WebSocket 请求参数不完整。")
            );
            return result;
        }

        QWebSocket socket;
        socket.setProxy(proxyForRequest(request));

        QEventLoop loop;
        QTimer frameTimer;
        frameTimer.setInterval(qBound(1, request.frameIntervalMs, 60000));
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QTimer cancellationTimer;
        cancellationTimer.setInterval(15);
        QElapsedTimer elapsed;

        int frameIndex = 0;
        bool completed = false;
        bool timedOut = false;
        bool cancelled = false;

        const auto sendNextFrame = [&]() {
            if (socket.state() != QAbstractSocket::ConnectedState
                || frameIndex >= request.textFrames.size()) {
                if (frameIndex >= request.textFrames.size()) {
                    frameTimer.stop();
                }
                return;
            }
            socket.sendTextMessage(
                QString::fromUtf8(request.textFrames.at(frameIndex))
            );
            ++frameIndex;
            if (frameIndex >= request.textFrames.size()) {
                frameTimer.stop();
            }
        };

        QObject::connect(
            &socket,
            &QWebSocket::connected,
            &loop,
            [&]() {
                sendNextFrame();
                if (frameIndex < request.textFrames.size()) {
                    frameTimer.start();
                }
            }
        );
        QObject::connect(
            &frameTimer,
            &QTimer::timeout,
            &loop,
            sendNextFrame
        );
        QObject::connect(
            &socket,
            &QWebSocket::textMessageReceived,
            &loop,
            [&](const QString &message) {
                const QByteArray bytes = message.toUtf8();
                result.messages.append(bytes);
                if (completion && completion(bytes)) {
                    completed = true;
                    socket.close();
                    loop.quit();
                }
            }
        );
        QObject::connect(
            &socket,
            static_cast<void (QWebSocket::*)(
                QAbstractSocket::SocketError
            )>(&QWebSocket::error),
            &loop,
            [&](QAbstractSocket::SocketError) {
                if (completed || timedOut || cancelled) {
                    return;
                }
                result.error = transportError(
                    QStringLiteral("network.websocket"),
                    QString::fromUtf8("WebSocket 网络请求失败。"),
                    socket.errorString(),
                    true
                );
                loop.quit();
            }
        );
        QObject::connect(
            &socket,
            &QWebSocket::disconnected,
            &loop,
            [&]() {
                if (!completed && !timedOut && !cancelled
                    && result.error.isEmpty()) {
                    result.error = transportError(
                        QStringLiteral("network.closed"),
                        QString::fromUtf8("WebSocket 连接被远端关闭。"),
                        QString::fromUtf8("远端主机关闭了这个连接"),
                        true
                    );
                }
                loop.quit();
            }
        );
        QObject::connect(
            &timeoutTimer,
            &QTimer::timeout,
            &loop,
            [&]() {
                timedOut = true;
                result.error = transportError(
                    QStringLiteral("network.timeout"),
                    QString::fromUtf8("网络请求超时。"),
                    QString(),
                    true
                );
                socket.abort();
                loop.quit();
            }
        );
        QObject::connect(
            &cancellationTimer,
            &QTimer::timeout,
            &loop,
            [&]() {
                if (!cancellation.isCancellationRequested()) {
                    return;
                }
                cancelled = true;
                result.cancelled = true;
                result.error = transportError(
                    QStringLiteral("request.cancelled"),
                    QString::fromUtf8("请求已取消。")
                );
                socket.abort();
                loop.quit();
            }
        );

        elapsed.start();
        timeoutTimer.start(
            qBound(1, request.network.timeoutMs, 10 * 60 * 1000)
        );
        cancellationTimer.start();
        socket.open(request.url);
        loop.exec();
        frameTimer.stop();
        timeoutTimer.stop();
        cancellationTimer.stop();
        result.durationMs = elapsed.elapsed();

        if (!completed && cancellation.isCancellationRequested()) {
            result.cancelled = true;
            result.error = transportError(
                QStringLiteral("request.cancelled"),
                QString::fromUtf8("请求已取消。")
            );
        }
        return result;
    }
};

} // namespace

QSharedPointer<IProviderWebSocketTransport>
createProviderWebSocketTransport()
{
    return QSharedPointer<IProviderWebSocketTransport>(
        new ProviderWebSocketTransport
    );
}
