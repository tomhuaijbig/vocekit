#include "provider_streaming_websocket_transport.h"

#include "../result_flow_config.h"

#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QObject>
#include <QTimer>
#include <QtWebSockets/QWebSocket>

namespace {

OperationError streamingTransportError(
    const QString &code,
    const QString &message,
    const QString &detail = QString(),
    bool retryable = false
)
{
    OperationError error;
    error.code = code;
    error.message = message;
    error.detail = detail;
    error.retryable = retryable;
    return error;
}

QNetworkProxy proxyForStreamingRequest(
    const QUrl &url,
    const NetworkRequestOptions &options
)
{
    const QString policy = resolveNetworkPolicy(
        options.networkPolicy,
        options.globalUseSystemProxy
    );
    if (policy != QStringLiteral("systemProxy")) {
        return QNetworkProxy(QNetworkProxy::NoProxy);
    }

    const QList<QNetworkProxy> proxies =
        QNetworkProxyFactory::systemProxyForQuery(
            QNetworkProxyQuery(url)
        );
    for (const QNetworkProxy &proxy : proxies) {
        if (proxy.type() != QNetworkProxy::DefaultProxy) {
            return proxy;
        }
    }
    return QNetworkProxy(QNetworkProxy::NoProxy);
}

class ProviderStreamingWebSocketTransport
    : public QObject,
      public IProviderStreamingWebSocketTransport
{
public:
    ProviderStreamingWebSocketTransport()
        : m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this)),
          m_timeout(new QTimer(this))
    {
        m_timeout->setSingleShot(true);
        QObject::connect(
            m_timeout,
            &QTimer::timeout,
            this,
            [this]() {
                fail(streamingTransportError(
                    QStringLiteral("network.timeout"),
                    QString::fromUtf8("WebSocket 连接超时。"),
                    QString(),
                    true
                ));
            }
        );
        QObject::connect(
            m_socket,
            &QWebSocket::connected,
            this,
            [this]() {
                if (m_terminal) {
                    return;
                }
                m_timeout->stop();
                m_opened = true;
                if (m_callbacks.opened) {
                    m_callbacks.opened();
                }
            }
        );
        QObject::connect(
            m_socket,
            &QWebSocket::textMessageReceived,
            this,
            [this](const QString &message) {
                if (!m_terminal && m_callbacks.textMessage) {
                    m_callbacks.textMessage(message.toUtf8());
                }
            }
        );
        QObject::connect(
            m_socket,
            static_cast<void (QWebSocket::*)(
                QAbstractSocket::SocketError
            )>(&QWebSocket::error),
            this,
            [this](QAbstractSocket::SocketError) {
                if (m_terminal) {
                    return;
                }
                fail(streamingTransportError(
                    QStringLiteral("network.websocket"),
                    QString::fromUtf8("WebSocket 网络请求失败。"),
                    m_socket->errorString(),
                    true
                ));
            }
        );
        QObject::connect(
            m_socket,
            &QWebSocket::disconnected,
            this,
            [this]() {
                if (m_terminal) {
                    return;
                }
                if (m_cancelRequested) {
                    closeOnce(true);
                    return;
                }
                if (m_normalCloseRequested) {
                    closeOnce(false);
                    return;
                }
                fail(streamingTransportError(
                    QStringLiteral("network.closed"),
                    QString::fromUtf8("WebSocket 连接被远端关闭。"),
                    QString(),
                    true
                ));
            }
        );
    }

    ~ProviderStreamingWebSocketTransport() override
    {
        m_terminal = true;
        m_callbacks = ProviderStreamingWebSocketCallbacks();
        m_timeout->stop();
        m_socket->abort();
    }

    void open(
        const QUrl &url,
        const NetworkRequestOptions &options,
        const ProviderStreamingWebSocketCallbacks &callbacks
    ) override
    {
        m_callbacks = callbacks;
        const QString scheme = url.scheme().toLower();
        if (m_terminal || m_opened
            || !url.isValid() || url.host().trimmed().isEmpty()
            || (scheme != QStringLiteral("ws")
                && scheme != QStringLiteral("wss"))) {
            fail(streamingTransportError(
                QStringLiteral("websocket.invalid_request"),
                QString::fromUtf8("WebSocket 请求地址无效。")
            ));
            return;
        }

        m_socket->setProxy(proxyForStreamingRequest(url, options));
        m_timeout->start(
            qBound(1, options.timeoutMs, 10 * 60 * 1000)
        );
        m_socket->open(url);
    }

    void sendText(const QByteArray &message) override
    {
        if (!m_terminal
            && m_socket->state() == QAbstractSocket::ConnectedState) {
            m_socket->sendTextMessage(QString::fromUtf8(message));
        }
    }

    void sendBinary(const QByteArray &message) override
    {
        if (!m_terminal
            && m_socket->state() == QAbstractSocket::ConnectedState) {
            m_socket->sendBinaryMessage(message);
        }
    }

    void closeNormally() override
    {
        if (m_terminal) {
            return;
        }
        m_normalCloseRequested = true;
        if (m_socket->state() == QAbstractSocket::UnconnectedState) {
            closeOnce(false);
            return;
        }
        m_socket->close(
            QWebSocketProtocol::CloseCodeNormal,
            QString::fromUtf8("完成")
        );
    }

    void cancel() override
    {
        if (m_terminal) {
            return;
        }
        m_cancelRequested = true;
        m_socket->abort();
        closeOnce(true);
    }

private:
    void fail(const OperationError &error)
    {
        if (m_terminal) {
            return;
        }
        m_terminal = true;
        m_timeout->stop();
        m_socket->abort();
        const std::function<void(const OperationError &)> callback =
            m_callbacks.failed;
        if (callback) {
            callback(error);
        }
    }

    void closeOnce(bool cancelled)
    {
        if (m_terminal) {
            return;
        }
        m_terminal = true;
        m_timeout->stop();
        const std::function<void(bool)> callback = m_callbacks.closed;
        if (callback) {
            callback(cancelled);
        }
    }

    QWebSocket *m_socket = nullptr;
    QTimer *m_timeout = nullptr;
    ProviderStreamingWebSocketCallbacks m_callbacks;
    bool m_opened = false;
    bool m_terminal = false;
    bool m_normalCloseRequested = false;
    bool m_cancelRequested = false;
};

} // namespace

QSharedPointer<IProviderStreamingWebSocketTransport>
createProviderStreamingWebSocketTransport()
{
    return QSharedPointer<IProviderStreamingWebSocketTransport>(
        new ProviderStreamingWebSocketTransport
    );
}
