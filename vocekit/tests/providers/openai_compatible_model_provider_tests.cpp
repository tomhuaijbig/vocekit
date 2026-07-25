#include <QtTest>

#include "../../src/providers/openai_compatible_model_provider.h"
#include "../../src/providers/provider_network_transport.h"
#include "../../src/tasks/cancellation_token.h"

#include <QJsonDocument>
#include <QJsonObject>

class FakeOpenAiTransport : public IProviderNetworkTransport
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

class OpenAiCompatibleModelProviderTests : public QObject
{
    Q_OBJECT

private:
    static QSharedPointer<FakeOpenAiTransport> fakeTransport()
    {
        return QSharedPointer<FakeOpenAiTransport>(
            new FakeOpenAiTransport
        );
    }

    static SecretConfig openAiSecrets()
    {
        SecretConfig secrets;
        secrets.openaiApiKey = QStringLiteral("openai-test-key");
        return secrets;
    }

    static SecretConfig customSecrets(bool includeApiKey = true)
    {
        SecretConfig secrets;
        CustomModelProfile profile;
        profile.id = QStringLiteral("office");
        profile.name = QStringLiteral("办公模型");
        profile.url = QStringLiteral("gateway.example.com/service");
        profile.apiKey = includeApiKey
            ? QStringLiteral("custom-test-key")
            : QString();
        profile.model = QStringLiteral("vendor-model");
        secrets.customModels.append(profile);
        return secrets;
    }

private slots:
    void buildsAndParsesOpenAiRequest()
    {
        const QSharedPointer<FakeOpenAiTransport> transport =
            fakeTransport();
        transport->jsonResponse.statusCode = 200;
        transport->jsonResponse.body = QByteArrayLiteral(
            "{\"choices\":[{\"message\":{\"content\":\"  完成  \"}}]}"
        );
        OpenAiCompatibleModelProvider provider(
            QStringLiteral("openai"),
            transport,
            []() { return openAiSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.executionId = cancellation.executionId();
        request.modelId = QStringLiteral("openai:gpt-5.5");
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

        QCOMPARE(result.text, QStringLiteral("完成"));
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.executionId, request.executionId);
        QCOMPARE(transport->postJsonCount, 1);
        QCOMPARE(transport->postStreamCount, 0);
        QCOMPARE(
            transport->lastRequest.url(),
            QUrl(QStringLiteral(
                "https://api.openai.com/v1/chat/completions"
            ))
        );
        QCOMPARE(
            transport->lastRequest.rawHeader("Authorization"),
            QByteArrayLiteral("Bearer openai-test-key")
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
            QStringLiteral("gpt-5.5")
        );
        QVERIFY(!body.value(QStringLiteral("stream")).toBool());
    }

    void customProviderUsesProfileEndpointAndModel()
    {
        const QSharedPointer<FakeOpenAiTransport> transport =
            fakeTransport();
        transport->jsonResponse.statusCode = 200;
        transport->jsonResponse.body = QByteArrayLiteral(
            "{\"data\":{\"text\":\"自定义结果\"}}"
        );
        OpenAiCompatibleModelProvider provider(
            QStringLiteral("custom:office"),
            transport,
            []() { return customSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.modelId = QStringLiteral("custom:office");
        request.systemPrompt = QStringLiteral("system");
        request.userPrompt = QStringLiteral("user");
        request.stream = false;

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(result.text, QStringLiteral("自定义结果"));
        QVERIFY(result.error.isEmpty());
        QCOMPARE(
            transport->lastRequest.url(),
            QUrl(QStringLiteral(
                "https://gateway.example.com/service/v1/chat/completions"
            ))
        );
        QCOMPARE(
            transport->lastRequest.rawHeader("Authorization"),
            QByteArrayLiteral("Bearer custom-test-key")
        );
        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastBody).object();
        QCOMPARE(
            body.value(QStringLiteral("model")).toString(),
            QStringLiteral("vendor-model")
        );
    }

    void customProviderAllowsMissingOptionalKey()
    {
        const QSharedPointer<FakeOpenAiTransport> transport =
            fakeTransport();
        transport->jsonResponse.statusCode = 200;
        transport->jsonResponse.body = QByteArrayLiteral(
            "{\"result\":\"本地接口结果\"}"
        );
        OpenAiCompatibleModelProvider provider(
            QStringLiteral("custom:office"),
            transport,
            []() { return customSecrets(false); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.modelId = QStringLiteral("custom:office");
        request.stream = false;

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(result.text, QStringLiteral("本地接口结果"));
        QVERIFY(result.error.isEmpty());
        QVERIFY(
            transport->lastRequest.rawHeader("Authorization").isEmpty()
        );
    }

    void parsesSplitStreamingEvents()
    {
        const QSharedPointer<FakeOpenAiTransport> transport =
            fakeTransport();
        transport->streamResponse.statusCode = 200;
        transport->streamChunks
            << QByteArrayLiteral(
                "data: {\"choices\":[{\"delta\":{\"content\":\"流\"}}]}\n"
                "data: {\"choices\":[{\"delta\":{\"cont"
            )
            << QByteArrayLiteral(
                "ent\":\"式\"}}]}\n"
                "data: [DONE]\n"
            );
        OpenAiCompatibleModelProvider provider(
            QStringLiteral("openai"),
            transport,
            []() { return openAiSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.modelId = QStringLiteral("openai:gpt-5.5");
        request.stream = true;
        QStringList deltas;

        const ModelResult result = provider.complete(
            request,
            [&deltas](const QString &delta) { deltas.append(delta); },
            cancellation.token()
        );

        QCOMPARE(result.text, QStringLiteral("流式"));
        QVERIFY(result.error.isEmpty());
        QCOMPARE(transport->postJsonCount, 0);
        QCOMPARE(transport->postStreamCount, 1);
        QCOMPARE(deltas, QStringList() << QStringLiteral("流")
                                      << QStringLiteral("式"));
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

    void selfCheckUsesConfiguredDefaultModel()
    {
        const QSharedPointer<FakeOpenAiTransport> transport =
            fakeTransport();
        transport->jsonResponse.statusCode = 200;
        transport->jsonResponse.body = QByteArrayLiteral(
            "{\"choices\":[{\"message\":{\"content\":\"OK\"}}]}"
        );
        OpenAiCompatibleModelProvider provider(
            QStringLiteral("openai"),
            transport,
            []() { return openAiSecrets(); }
        );

        const ProviderCheckResult result = provider.checkConfiguration();

        QVERIFY(result.available);
        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastBody).object();
        QCOMPARE(
            body.value(QStringLiteral("model")).toString(),
            QStringLiteral("gpt-5.5")
        );
    }

    void missingOpenAiKeyDoesNotReachNetwork()
    {
        const QSharedPointer<FakeOpenAiTransport> transport =
            fakeTransport();
        OpenAiCompatibleModelProvider provider(
            QStringLiteral("openai"),
            transport,
            []() { return SecretConfig(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.modelId = QStringLiteral("openai:gpt-5.5");

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("provider.configuration"));
        QVERIFY(result.error.message.contains(QStringLiteral("OpenAI")));
        QCOMPARE(transport->postJsonCount, 0);
        QCOMPARE(transport->postStreamCount, 0);
    }

    void unknownCustomProfileDoesNotReachNetwork()
    {
        const QSharedPointer<FakeOpenAiTransport> transport =
            fakeTransport();
        OpenAiCompatibleModelProvider provider(
            QStringLiteral("custom:missing"),
            transport,
            []() { return customSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.modelId = QStringLiteral("custom:missing");

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("provider.configuration"));
        QVERIFY(result.error.message.contains(QStringLiteral("自定义")));
        QCOMPARE(transport->postJsonCount, 0);
        QCOMPARE(transport->postStreamCount, 0);
    }

    void cancellationDoesNotReachNetwork()
    {
        const QSharedPointer<FakeOpenAiTransport> transport =
            fakeTransport();
        OpenAiCompatibleModelProvider provider(
            QStringLiteral("openai"),
            transport,
            []() { return openAiSecrets(); }
        );
        CancellationSource cancellation;
        cancellation.cancel();
        ModelRequest request;
        request.modelId = QStringLiteral("openai:gpt-5.5");

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

QTEST_MAIN(OpenAiCompatibleModelProviderTests)
#include "openai_compatible_model_provider_tests.moc"
