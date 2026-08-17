#include <QtTest>

#include "../../src/providers/deepseek_model_provider.h"
#include "../../src/providers/provider_network_transport.h"
#include "../../src/tasks/cancellation_token.h"

#include <QJsonDocument>
#include <QJsonObject>

class FakeProviderNetworkTransport : public IProviderNetworkTransport
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

class DeepSeekModelProviderTests : public QObject
{
    Q_OBJECT

private:
    static SecretConfig configuredSecrets()
    {
        SecretConfig secrets;
        secrets.deepseekApiKey = QStringLiteral("deepseek-test-key");
        return secrets;
    }

    static QSharedPointer<FakeProviderNetworkTransport> fakeTransport()
    {
        return QSharedPointer<FakeProviderNetworkTransport>(
            new FakeProviderNetworkTransport
        );
    }

private slots:
    void buildsAndParsesNonStreamingRequest()
    {
        const QSharedPointer<FakeProviderNetworkTransport> transport =
            fakeTransport();
        transport->jsonResponse.statusCode = 200;
        transport->jsonResponse.body = QByteArrayLiteral(
            "{\"choices\":[{\"message\":{\"content\":\"  已完成  \"}}]}"
        );
        DeepSeekModelProvider provider(
            transport,
            []() { return configuredSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.executionId = cancellation.executionId();
        request.modelId = QStringLiteral("deepseek-v4-flash");
        request.systemPrompt = QStringLiteral("system");
        request.userPrompt = QStringLiteral("user");
        request.stream = false;
        request.network.timeoutMs = 43210;
        request.network.networkPolicy = QStringLiteral("direct");
        request.sampling.temperatureEnabled = true;
        request.sampling.temperature = 0.95;
        request.sampling.topPEnabled = true;
        request.sampling.topP = 0.7;

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(result.text, QStringLiteral("已完成"));
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.executionId, request.executionId);
        QCOMPARE(transport->postJsonCount, 1);
        QCOMPARE(transport->postStreamCount, 0);
        QCOMPARE(
            transport->lastRequest.url(),
            QUrl(QStringLiteral("https://api.deepseek.com/chat/completions"))
        );
        QCOMPARE(
            transport->lastRequest.rawHeader("Authorization"),
            QByteArrayLiteral("Bearer deepseek-test-key")
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
            QStringLiteral("deepseek-v4-flash")
        );
        QCOMPARE(
            body.value(QStringLiteral("thinking"))
                .toObject()
                .value(QStringLiteral("type"))
                .toString(),
            QStringLiteral("disabled")
        );
        QVERIFY(!body.value(QStringLiteral("stream")).toBool());
        QCOMPARE(body.value(QStringLiteral("temperature")).toDouble(), 0.95);
        QCOMPARE(body.value(QStringLiteral("top_p")).toDouble(), 0.7);
    }

    void parsesSplitStreamingEvents()
    {
        const QSharedPointer<FakeProviderNetworkTransport> transport =
            fakeTransport();
        transport->streamResponse.statusCode = 200;
        transport->streamChunks
            << QByteArrayLiteral(
                "data: {\"choices\":[{\"delta\":{\"content\":\"你\"}}]}\n"
                "data: {\"choices\":[{\"delta\":{\"cont"
            )
            << QByteArrayLiteral(
                "ent\":\"好\"}}]}\n"
                "data: [DONE]\n"
            );
        DeepSeekModelProvider provider(
            transport,
            []() { return configuredSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.executionId = cancellation.executionId();
        request.modelId = QStringLiteral("deepseek-v4-flash");
        request.systemPrompt = QStringLiteral("system");
        request.userPrompt = QStringLiteral("user");
        request.stream = true;
        QStringList deltas;

        const ModelResult result = provider.complete(
            request,
            [&deltas](const QString &delta) { deltas.append(delta); },
            cancellation.token()
        );

        QCOMPARE(result.text, QStringLiteral("你好"));
        QVERIFY(result.error.isEmpty());
        QCOMPARE(transport->postJsonCount, 0);
        QCOMPARE(transport->postStreamCount, 1);
        QCOMPARE(deltas, QStringList() << QStringLiteral("你")
                                      << QStringLiteral("好"));
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
        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastBody).object();
        QVERIFY(!body.contains(QStringLiteral("temperature")));
        QVERIFY(!body.contains(QStringLiteral("top_p")));
    }

    void missingKeyDoesNotReachNetwork()
    {
        const QSharedPointer<FakeProviderNetworkTransport> transport =
            fakeTransport();
        DeepSeekModelProvider provider(
            transport,
            []() { return SecretConfig(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.modelId = QStringLiteral("deepseek-v4-flash");

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("provider.configuration"));
        QVERIFY(result.error.message.contains(QStringLiteral("DeepSeek")));
        QCOMPARE(transport->postJsonCount, 0);
        QCOMPARE(transport->postStreamCount, 0);
    }

    void cancellationDoesNotReachNetwork()
    {
        const QSharedPointer<FakeProviderNetworkTransport> transport =
            fakeTransport();
        DeepSeekModelProvider provider(
            transport,
            []() { return configuredSecrets(); }
        );
        CancellationSource cancellation;
        cancellation.cancel();
        ModelRequest request;
        request.modelId = QStringLiteral("deepseek-v4-flash");

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("task.cancelled"));
        QCOMPARE(transport->postJsonCount, 0);
        QCOMPARE(transport->postStreamCount, 0);
    }
};

QTEST_MAIN(DeepSeekModelProviderTests)
#include "deepseek_model_provider_tests.moc"
