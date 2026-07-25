#include <QtTest>

#include "../../src/providers/network_request_executor.h"

#include <QHostAddress>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

class FakeHttpServer : public QObject
{
    Q_OBJECT

public:
    explicit FakeHttpServer(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(
            &m_server,
            &QTcpServer::newConnection,
            this,
            &FakeHttpServer::handleConnection
        );
    }

    bool listen()
    {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/test")
            .arg(m_server.serverPort()));
    }

    void setResponse(const QByteArray &response, int delayMs = 0)
    {
        m_response = response;
        m_delayMs = delayMs;
        m_chunks.clear();
    }

    void setChunks(const QList<QByteArray> &chunks, int intervalMs)
    {
        m_chunks = chunks;
        m_chunkIntervalMs = intervalMs;
        m_response.clear();
    }

private slots:
    void handleConnection()
    {
        QTcpSocket *socket = m_server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            QByteArray requestData =
                socket->property("requestData").toByteArray();
            requestData.append(socket->readAll());
            socket->setProperty("requestData", requestData);
            if (socket->property("responseStarted").toBool()) {
                return;
            }

            const int headerEnd = requestData.indexOf("\r\n\r\n");
            if (headerEnd < 0) {
                return;
            }
            int contentLength = 0;
            const QList<QByteArray> headerLines =
                requestData.left(headerEnd).split('\n');
            for (const QByteArray &rawLine : headerLines) {
                const QByteArray line = rawLine.trimmed();
                if (line.toLower().startsWith("content-length:")) {
                    contentLength =
                        line.mid(QByteArray("content-length:").size())
                            .trimmed()
                            .toInt();
                    break;
                }
            }
            if (requestData.size() < headerEnd + 4 + contentLength) {
                return;
            }
            socket->setProperty("responseStarted", true);

            if (!m_chunks.isEmpty()) {
                writeChunk(socket, 0);
                return;
            }
            QTimer::singleShot(m_delayMs, socket, [this, socket]() {
                socket->write(m_response);
                socket->flush();
                socket->disconnectFromHost();
            });
        });
    }

private:
    void writeChunk(QTcpSocket *socket, int index)
    {
        if (!socket || index >= m_chunks.size()) {
            socket->disconnectFromHost();
            return;
        }
        socket->write(m_chunks.at(index));
        socket->flush();
        QTimer::singleShot(
            m_chunkIntervalMs,
            socket,
            [this, socket, index]() {
                writeChunk(socket, index + 1);
            }
        );
    }

    QTcpServer m_server;
    QByteArray m_response;
    QList<QByteArray> m_chunks;
    int m_delayMs = 0;
    int m_chunkIntervalMs = 5;
};

namespace {

QByteArray response(
    int status,
    const QByteArray &reason,
    const QByteArray &body)
{
    return "HTTP/1.1 " + QByteArray::number(status) + " " + reason
        + "\r\nContent-Type: application/json"
        + "\r\nContent-Length: " + QByteArray::number(body.size())
        + "\r\nConnection: close\r\n\r\n" + body;
}

} // namespace

class NetworkRequestExecutorTests : public QObject
{
    Q_OBJECT

private slots:
    void postsJson()
    {
        FakeHttpServer server;
        QVERIFY(server.listen());
        server.setResponse(response(200, "OK", "{\"ok\":true}"));

        NetworkRequestExecutor executor;
        NetworkRequestOptions options;
        options.timeoutMs = 1000;
        options.networkPolicy = QStringLiteral("direct");
        const NetworkResponse result = executor.postJson(
            QNetworkRequest(server.url()),
            QByteArray("{}"),
            options,
            CancellationSource().token()
        );

        QVERIFY2(result.isSuccess(), qPrintable(result.error.message));
        QCOMPARE(result.statusCode, 200);
        QCOMPARE(result.body, QByteArray("{\"ok\":true}"));
    }

    void streamsResponseChunks()
    {
        FakeHttpServer server;
        QVERIFY(server.listen());
        server.setChunks(
            QList<QByteArray>()
                << QByteArray(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/event-stream\r\n"
                    "Connection: close\r\n\r\n"
                    "data: one\n\n"
                )
                << QByteArray("data: two\n\n"),
            10
        );

        QByteArray streamed;
        NetworkRequestExecutor executor;
        NetworkRequestOptions options;
        options.timeoutMs = 1000;
        const NetworkResponse result = executor.postEventStream(
            QNetworkRequest(server.url()),
            QByteArray("{}"),
            options,
            [&streamed](const QByteArray &chunk) {
                streamed.append(chunk);
            },
            CancellationSource().token()
        );

        QVERIFY2(result.isSuccess(), qPrintable(result.error.message));
        QVERIFY(streamed.contains("data: one"));
        QVERIFY(streamed.contains("data: two"));
    }

    void reportsTimeout()
    {
        FakeHttpServer server;
        QVERIFY(server.listen());
        server.setResponse(response(200, "OK", "{}"), 300);

        NetworkRequestExecutor executor;
        NetworkRequestOptions options;
        options.timeoutMs = 30;
        const NetworkResponse result = executor.get(
            QNetworkRequest(server.url()),
            options,
            CancellationSource().token()
        );

        QCOMPARE(result.error.code, QStringLiteral("network.timeout"));
        QVERIFY(result.error.retryable);
    }

    void reportsHttpAuthenticationError()
    {
        FakeHttpServer server;
        QVERIFY(server.listen());
        server.setResponse(response(401, "Unauthorized", "{\"error\":1}"));

        NetworkRequestExecutor executor;
        NetworkRequestOptions options;
        options.timeoutMs = 1000;
        const NetworkResponse result = executor.get(
            QNetworkRequest(server.url()),
            options,
            CancellationSource().token()
        );

        QCOMPARE(result.statusCode, 401);
        QCOMPARE(result.error.code, QStringLiteral("http.401"));
        QVERIFY(!result.error.retryable);
    }

    void cancelsPendingRequest()
    {
        FakeHttpServer server;
        QVERIFY(server.listen());
        server.setResponse(response(200, "OK", "{}"), 1000);

        CancellationSource cancellation;
        QTimer::singleShot(20, [&cancellation]() {
            cancellation.cancel();
        });

        NetworkRequestExecutor executor;
        NetworkRequestOptions options;
        options.timeoutMs = 2000;
        const NetworkResponse result = executor.get(
            QNetworkRequest(server.url()),
            options,
            cancellation.token()
        );

        QVERIFY(result.cancelled);
        QCOMPARE(result.error.code, QStringLiteral("request.cancelled"));
    }

    void resolvesNetworkPolicy()
    {
        NetworkRequestOptions options;
        options.networkPolicy = QStringLiteral("inherit");
        options.globalUseSystemProxy = false;
        QCOMPARE(
            NetworkRequestExecutor::resolvedNetworkPolicy(options),
            QStringLiteral("direct")
        );
        options.globalUseSystemProxy = true;
        QCOMPARE(
            NetworkRequestExecutor::resolvedNetworkPolicy(options),
            QStringLiteral("systemProxy")
        );
        options.networkPolicy = QStringLiteral("direct");
        QCOMPARE(
            NetworkRequestExecutor::resolvedNetworkPolicy(options),
            QStringLiteral("direct")
        );
    }
};

QTEST_MAIN(NetworkRequestExecutorTests)
#include "network_request_executor_tests.moc"
