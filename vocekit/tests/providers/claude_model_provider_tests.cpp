#include <QtTest>

#include "../../src/providers/claude_model_provider.h"
#include "../../src/tasks/cancellation_token.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

class FakeClaudeTransport : public IProviderNetworkTransport
{
public:
    NetworkResponse get(
        const QNetworkRequest &,
        const NetworkRequestOptions &,
        const CancellationToken &) override
    {
        return NetworkResponse();
    }

    NetworkResponse postJson(
        const QNetworkRequest &request,
        const QByteArray &body,
        const NetworkRequestOptions &options,
        const CancellationToken &) override
    {
        ++postJsonCount;
        lastRequest = request;
        lastBody = body;
        lastOptions = options;
        return jsonResponse;
    }

    NetworkResponse postEventStream(
        const QNetworkRequest &request,
        const QByteArray &body,
        const NetworkRequestOptions &options,
        const StreamDataCallback &onData,
        const CancellationToken &) override
    {
        ++postStreamCount;
        lastRequest = request;
        lastBody = body;
        lastOptions = options;
        for (const QByteArray &chunk : streamChunks) {
            if (onData) {
                onData(chunk);
            }
        }
        return streamResponse;
    }

    int postJsonCount = 0;
    int postStreamCount = 0;
    QNetworkRequest lastRequest;
    QByteArray lastBody;
    NetworkRequestOptions lastOptions;
    NetworkResponse jsonResponse;
    NetworkResponse streamResponse;
    QList<QByteArray> streamChunks;
};

class ClaudeModelProviderTests : public QObject
{
    Q_OBJECT

private:
    static QSharedPointer<FakeClaudeTransport> fakeTransport()
    {
        return QSharedPointer<FakeClaudeTransport>(
            new FakeClaudeTransport
        );
    }

    static SecretConfig claudeSecrets()
    {
        SecretConfig secrets;
        secrets.anthropicApiKey = QStringLiteral("claude-test-key");
        return secrets;
    }

private slots:
    void buildsAndParsesOrdinaryRequest()
    {
        const QSharedPointer<FakeClaudeTransport> transport =
            fakeTransport();
        transport->jsonResponse.statusCode = 200;
        transport->jsonResponse.durationMs = 17;
        transport->jsonResponse.body = QByteArrayLiteral(
            "{\"content\":["
            "{\"type\":\"text\",\"text\":\"  first  \"},"
            "{\"type\":\"tool_use\",\"name\":\"ignored\"},"
            "{\"type\":\"text\",\"text\":\"second\"}"
            "]}"
        );
        ClaudeModelProvider provider(
            transport,
            []() { return claudeSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.executionId = cancellation.executionId();
        request.modelId = QStringLiteral("claude:claude-opus-4-8");
        request.systemPrompt = QStringLiteral("system");
        request.userPrompt = QStringLiteral("user");
        request.stream = false;
        request.network.timeoutMs = 43210;
        request.network.networkPolicy = QStringLiteral("direct");

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(result.text, QStringLiteral("first\nsecond"));
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.executionId, request.executionId);
        QCOMPARE(result.durationMs, qint64(17));
        QCOMPARE(transport->postJsonCount, 1);
        QCOMPARE(transport->postStreamCount, 0);
        QCOMPARE(
            transport->lastRequest.url(),
            QUrl(QStringLiteral(
                "https://api.anthropic.com/v1/messages"
            ))
        );
        QCOMPARE(
            transport->lastRequest.rawHeader("x-api-key"),
            QByteArrayLiteral("claude-test-key")
        );
        QCOMPARE(
            transport->lastRequest.rawHeader("anthropic-version"),
            QByteArrayLiteral("2023-06-01")
        );
        QCOMPARE(transport->lastOptions.timeoutMs, 43210);
        QCOMPARE(
            transport->lastOptions.networkPolicy,
            QStringLiteral("direct")
        );

        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastBody).object();
        QCOMPARE(
            body.value(QStringLiteral("model")).toString(),
            QStringLiteral("claude-opus-4-8")
        );
        QCOMPARE(
            body.value(QStringLiteral("system")).toString(),
            QStringLiteral("system")
        );
        QCOMPARE(
            body.value(QStringLiteral("messages"))
                .toArray()
                .at(0)
                .toObject()
                .value(QStringLiteral("content"))
                .toString(),
            QStringLiteral("user")
        );
        QVERIFY(!body.value(QStringLiteral("stream")).toBool());
    }

    void parsesSplitStreamingEvents()
    {
        const QSharedPointer<FakeClaudeTransport> transport =
            fakeTransport();
        transport->streamResponse.statusCode = 200;
        transport->streamChunks
            << QByteArrayLiteral(
                "event: content_block_delta\n"
                "data: {\"type\":\"content_block_delta\","
                "\"delta\":{\"type\":\"text_delta\",\"text\":\"stream \"}}\n"
                "data: {\"type\":\"content_block_delta\","
                "\"delta\":{\"type\":\"text_delta\",\"te"
            )
            << QByteArrayLiteral(
                "xt\":\"result\"}}\n"
                "event: message_stop\n"
                "data: {\"type\":\"message_stop\"}\n"
            );
        ClaudeModelProvider provider(
            transport,
            []() { return claudeSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.modelId = QStringLiteral("claude:claude-sonnet-4-6");
        request.stream = true;
        QStringList deltas;

        const ModelResult result = provider.complete(
            request,
            [&deltas](const QString &delta) { deltas.append(delta); },
            cancellation.token()
        );

        QCOMPARE(result.text, QStringLiteral("stream result"));
        QVERIFY(result.error.isEmpty());
        QCOMPARE(transport->postJsonCount, 0);
        QCOMPARE(transport->postStreamCount, 1);
        QCOMPARE(
            deltas,
            QStringList()
                << QStringLiteral("stream ")
                << QStringLiteral("result")
        );
        QCOMPARE(
            transport->lastRequest.rawHeader("Accept"),
            QByteArrayLiteral("text/event-stream")
        );
        QVERIFY(
            QJsonDocument::fromJson(transport->lastBody)
                .object()
                .value(QStringLiteral("stream"))
                .toBool()
        );
    }

    void reportsOrdinaryApiError()
    {
        const QSharedPointer<FakeClaudeTransport> transport =
            fakeTransport();
        transport->jsonResponse.statusCode = 200;
        transport->jsonResponse.body = QByteArrayLiteral(
            "{\"type\":\"error\",\"error\":{"
            "\"type\":\"invalid_request_error\","
            "\"message\":\"bad model\"}}"
        );
        ClaudeModelProvider provider(
            transport,
            []() { return claudeSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.stream = false;

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("model.api"));
        QVERIFY(result.error.message.contains(QStringLiteral("bad model")));
        QVERIFY(result.text.isEmpty());
    }

    void reportsStreamingApiError()
    {
        const QSharedPointer<FakeClaudeTransport> transport =
            fakeTransport();
        transport->streamResponse.statusCode = 200;
        transport->streamChunks << QByteArrayLiteral(
            "event: error\n"
            "data: {\"type\":\"error\",\"error\":{"
            "\"type\":\"overloaded_error\","
            "\"message\":\"try later\"}}\n"
        );
        ClaudeModelProvider provider(
            transport,
            []() { return claudeSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.stream = true;

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("model.api"));
        QVERIFY(result.error.message.contains(QStringLiteral("try later")));
        QVERIFY(result.text.isEmpty());
    }

    void reportsInvalidJson()
    {
        const QSharedPointer<FakeClaudeTransport> transport =
            fakeTransport();
        transport->jsonResponse.statusCode = 200;
        transport->jsonResponse.body = QByteArrayLiteral("not-json");
        ClaudeModelProvider provider(
            transport,
            []() { return claudeSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.stream = false;

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(
            result.error.code,
            QStringLiteral("model.invalid_response")
        );
    }

    void selfCheckUsesCatalogDefaultModel()
    {
        const QSharedPointer<FakeClaudeTransport> transport =
            fakeTransport();
        transport->jsonResponse.statusCode = 200;
        transport->jsonResponse.body = QByteArrayLiteral(
            "{\"content\":[{\"type\":\"text\",\"text\":\"OK\"}]}"
        );
        ClaudeModelProvider provider(
            transport,
            []() { return claudeSecrets(); }
        );

        const ProviderCheckResult result =
            provider.checkConfiguration();

        QVERIFY(result.available);
        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastBody).object();
        QCOMPARE(
            body.value(QStringLiteral("model")).toString(),
            QStringLiteral("claude-opus-4-8")
        );
    }

    void missingKeyDoesNotReachNetwork()
    {
        const QSharedPointer<FakeClaudeTransport> transport =
            fakeTransport();
        ClaudeModelProvider provider(
            transport,
            []() { return SecretConfig(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.modelId = QStringLiteral("claude:claude-opus-4-8");

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(
            result.error.code,
            QStringLiteral("provider.configuration")
        );
        QVERIFY(result.error.message.contains(QStringLiteral("Claude")));
        QCOMPARE(transport->postJsonCount, 0);
        QCOMPARE(transport->postStreamCount, 0);
    }

    void cancellationDoesNotReachNetwork()
    {
        const QSharedPointer<FakeClaudeTransport> transport =
            fakeTransport();
        ClaudeModelProvider provider(
            transport,
            []() { return claudeSecrets(); }
        );
        CancellationSource cancellation;
        cancellation.cancel();
        ModelRequest request;
        request.modelId = QStringLiteral("claude:claude-opus-4-8");

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("task.cancelled"));
        QCOMPARE(transport->postJsonCount, 0);
        QCOMPARE(transport->postStreamCount, 0);
    }

    void convertsNetworkTimeout()
    {
        const QSharedPointer<FakeClaudeTransport> transport =
            fakeTransport();
        transport->jsonResponse.error.code =
            QStringLiteral("network.timeout");
        transport->jsonResponse.error.message =
            QStringLiteral("timed out");
        ClaudeModelProvider provider(
            transport,
            []() { return claudeSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.stream = false;

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("network.timeout"));
        QVERIFY(result.error.message.contains(QStringLiteral("Claude")));
        QVERIFY(result.error.message.contains(QStringLiteral("超时")));
        QVERIFY(result.error.retryable);
    }
};

QTEST_MAIN(ClaudeModelProviderTests)
#include "claude_model_provider_tests.moc"
