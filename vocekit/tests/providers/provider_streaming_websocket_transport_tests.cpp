#include <QtTest>

#include "../../src/providers/provider_streaming_websocket_transport.h"

#include <QHostAddress>
#include <QSharedPointer>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>

class ProviderStreamingWebSocketTransportTests : public QObject
{
    Q_OBJECT

private slots:
    void exchangesLiveTextAndBinaryFramesThenClosesNormally()
    {
        QWebSocketServer server(
            QStringLiteral("vocekit-live-test"),
            QWebSocketServer::NonSecureMode
        );
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        QWebSocket *serverSocket = nullptr;
        QList<QString> serverTextFrames;
        QList<QByteArray> serverBinaryFrames;
        connect(
            &server,
            &QWebSocketServer::newConnection,
            &server,
            [&]() {
                serverSocket = server.nextPendingConnection();
                connect(
                    serverSocket,
                    &QWebSocket::textMessageReceived,
                    &server,
                    [&](const QString &message) {
                        serverTextFrames.append(message);
                    }
                );
                connect(
                    serverSocket,
                    &QWebSocket::binaryMessageReceived,
                    &server,
                    [&](const QByteArray &message) {
                        serverBinaryFrames.append(message);
                    }
                );
            }
        );

        int openedCount = 0;
        int failedCount = 0;
        int closedCount = 0;
        bool closedAsCancelled = true;
        QList<QByteArray> clientMessages;
        ProviderStreamingWebSocketCallbacks callbacks;
        callbacks.opened = [&]() { ++openedCount; };
        callbacks.textMessage = [&](const QByteArray &message) {
            clientMessages.append(message);
        };
        callbacks.failed = [&](const OperationError &) {
            ++failedCount;
        };
        callbacks.closed = [&](bool cancelled) {
            ++closedCount;
            closedAsCancelled = cancelled;
        };

        QSharedPointer<IProviderStreamingWebSocketTransport> transport =
            createProviderStreamingWebSocketTransport();
        NetworkRequestOptions options;
        options.timeoutMs = 5000;
        options.networkPolicy = QStringLiteral("direct");
        transport->open(
            QUrl(QStringLiteral("ws://127.0.0.1:%1")
                .arg(server.serverPort())),
            options,
            callbacks
        );

        QTRY_COMPARE(openedCount, 1);
        QVERIFY(serverSocket);
        transport->sendText(QByteArrayLiteral("{\"type\":\"START\"}"));
        transport->sendBinary(QByteArray::fromHex("00010203"));
        QTRY_COMPARE(serverTextFrames.size(), 1);
        QTRY_COMPARE(serverBinaryFrames.size(), 1);
        QCOMPARE(
            serverBinaryFrames.first(),
            QByteArray::fromHex("00010203")
        );

        serverSocket->sendTextMessage(QStringLiteral("partial"));
        QTRY_COMPARE(clientMessages.size(), 1);
        QCOMPARE(clientMessages.first(), QByteArrayLiteral("partial"));

        transport->closeNormally();
        QTRY_COMPARE(closedCount, 1);
        QCOMPARE(failedCount, 0);
        QVERIFY(!closedAsCancelled);
    }

    void cancelReportsOneCancelledTerminalCallback()
    {
        QWebSocketServer server(
            QStringLiteral("vocekit-live-cancel"),
            QWebSocketServer::NonSecureMode
        );
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        int openedCount = 0;
        int failedCount = 0;
        int closedCount = 0;
        bool cancelled = false;
        ProviderStreamingWebSocketCallbacks callbacks;
        callbacks.opened = [&]() { ++openedCount; };
        callbacks.failed = [&](const OperationError &) {
            ++failedCount;
        };
        callbacks.closed = [&](bool wasCancelled) {
            ++closedCount;
            cancelled = wasCancelled;
        };

        QSharedPointer<IProviderStreamingWebSocketTransport> transport =
            createProviderStreamingWebSocketTransport();
        transport->open(
            QUrl(QStringLiteral("ws://127.0.0.1:%1")
                .arg(server.serverPort())),
            NetworkRequestOptions(),
            callbacks
        );
        QTRY_COMPARE(openedCount, 1);

        transport->cancel();

        QTRY_COMPARE(closedCount, 1);
        QVERIFY(cancelled);
        QCOMPARE(failedCount, 0);
        QTest::qWait(30);
        QCOMPARE(closedCount, 1);
    }

    void rejectsInvalidUrlWithoutOpeningSocket()
    {
        int failedCount = 0;
        OperationError observed;
        ProviderStreamingWebSocketCallbacks callbacks;
        callbacks.failed = [&](const OperationError &error) {
            ++failedCount;
            observed = error;
        };
        QSharedPointer<IProviderStreamingWebSocketTransport> transport =
            createProviderStreamingWebSocketTransport();

        transport->open(
            QUrl(QStringLiteral("https://example.com/not-websocket")),
            NetworkRequestOptions(),
            callbacks
        );

        QCOMPARE(failedCount, 1);
        QCOMPARE(
            observed.code,
            QStringLiteral("websocket.invalid_request")
        );
    }
};

QTEST_MAIN(ProviderStreamingWebSocketTransportTests)

#include "provider_streaming_websocket_transport_tests.moc"
