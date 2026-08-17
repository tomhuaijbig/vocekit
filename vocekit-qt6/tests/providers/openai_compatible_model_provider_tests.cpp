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
        secrets.openaiBaseUrl = QStringLiteral(
            "https://official-proxy.example.test/openai"
        );
        CustomModelProfile profile;
        profile.id = QStringLiteral("office");
        profile.name = QStringLiteral("办公模型");
        profile.url = QStringLiteral("gateway.example.com/service");
        profile.apiKey = includeApiKey
            ? QStringLiteral("custom-test-key")
            : QString();
        profile.model = QStringLiteral("vendor-model");
        profile.temperatureEnabled = true;
        profile.temperature = 0.85;
        profile.topPEnabled = true;
        profile.topP = 0.65;
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
        request.modelId = QString();
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
        QVERIFY2(
            result.error.isEmpty(),
            qPrintable(result.error.code + QStringLiteral(": ")
                       + result.error.message)
        );
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
            QStringLiteral("gpt-5.6-terra")
        );
        QVERIFY(!body.contains(QStringLiteral("temperature")));
        QVERIFY(!body.contains(QStringLiteral("top_p")));
        QVERIFY(!body.value(QStringLiteral("stream")).toBool());
    }

    void officialProviderUsesFunctionSamplingOverrides()
    {
        const QSharedPointer<FakeOpenAiTransport> transport =
            fakeTransport();
        transport->jsonResponse.statusCode = 200;
        transport->jsonResponse.body = QByteArrayLiteral(
            "{\"choices\":[{\"message\":{\"content\":\"ok\"}}]}"
        );
        OpenAiCompatibleModelProvider provider(
            QStringLiteral("openai"),
            transport,
            []() { return openAiSecrets(); }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.modelId = QStringLiteral("openai:gpt-5.6-terra");
        request.stream = false;
        request.sampling.temperatureEnabled = true;
        request.sampling.temperature = 1.25;
        request.sampling.topPEnabled = true;
        request.sampling.topP = 0.5;

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QVERIFY2(
            result.error.isEmpty(),
            qPrintable(result.error.code + QStringLiteral(": ")
                       + result.error.message)
        );
        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastBody).object();
        QCOMPARE(body.value(QStringLiteral("temperature")).toDouble(), 1.25);
        QCOMPARE(body.value(QStringLiteral("top_p")).toDouble(), 0.5);
    }

    void officialProviderUsesConfiguredBaseUrl_data()
    {
        QTest::addColumn<QString>("baseUrl");
        QTest::addColumn<QString>("expectedUrl");

        QTest::newRow("root")
            << QStringLiteral("https://gateway.example.test")
            << QStringLiteral(
                "https://gateway.example.test/v1/chat/completions"
            );
        QTest::newRow("v1")
            << QStringLiteral("https://gateway.example.test/v1")
            << QStringLiteral(
                "https://gateway.example.test/v1/chat/completions"
            );
        QTest::newRow("full-endpoint")
            << QStringLiteral(
                "https://gateway.example.test/proxy/v1/chat/completions"
            )
            << QStringLiteral(
                "https://gateway.example.test/proxy/v1/chat/completions"
            );
        QTest::newRow("service-prefix")
            << QStringLiteral("https://gateway.example.test/openai")
            << QStringLiteral(
                "https://gateway.example.test/openai/v1/chat/completions"
            );
    }

    void officialProviderUsesConfiguredBaseUrl()
    {
        QFETCH(QString, baseUrl);
        QFETCH(QString, expectedUrl);

        const QSharedPointer<FakeOpenAiTransport> transport =
            fakeTransport();
        transport->jsonResponse.statusCode = 200;
        transport->jsonResponse.body = QByteArrayLiteral(
            "{\"choices\":[{\"message\":{\"content\":\"OK\"}}]}"
        );
        SecretConfig secrets = openAiSecrets();
        secrets.openaiBaseUrl = baseUrl;
        OpenAiCompatibleModelProvider provider(
            QStringLiteral("openai"),
            transport,
            [secrets]() { return secrets; }
        );
        CancellationSource cancellation;
        ModelRequest request;
        request.stream = false;

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QVERIFY2(
            result.error.isEmpty(),
            qPrintable(result.error.code + QStringLiteral(": ")
                       + result.error.message)
        );
        QCOMPARE(transport->postJsonCount, 1);
        QCOMPARE(transport->postStreamCount, 0);
        QCOMPARE(transport->lastRequest.url(), QUrl(expectedUrl));
        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastBody).object();
        QCOMPARE(
            body.value(QStringLiteral("model")).toString(),
            QStringLiteral("gpt-5.6-terra")
        );
    }

    void invalidOfficialBaseUrlDoesNotReachNetwork()
    {
        const QSharedPointer<FakeOpenAiTransport> transport =
            fakeTransport();
        SecretConfig secrets = openAiSecrets();
        secrets.openaiBaseUrl = QStringLiteral("not a host");
        OpenAiCompatibleModelProvider provider(
            QStringLiteral("openai"),
            transport,
            [secrets]() { return secrets; }
        );
        CancellationSource cancellation;
        ModelRequest request;

        const ModelResult result = provider.complete(
            request,
            ModelDeltaCallback(),
            cancellation.token()
        );

        QCOMPARE(
            result.error.code,
            QStringLiteral("provider.configuration")
        );
        QVERIFY(
            result.error.message.contains(
                QStringLiteral("OpenAI Base URL")
            )
        );
        QCOMPARE(transport->postJsonCount, 0);
        QCOMPARE(transport->postStreamCount, 0);
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
        request.sampling.temperatureEnabled = true;
        request.sampling.temperature = 1.15;
        request.sampling.topPEnabled = true;
        request.sampling.topP = 0.45;

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
        QCOMPARE(
            body.value(QStringLiteral("temperature")).toDouble(),
            1.15
        );
        QCOMPARE(body.value(QStringLiteral("top_p")).toDouble(), 0.45);
    }

    void customProviderOmitsDisabledSamplingParameters()
    {
        const QSharedPointer<FakeOpenAiTransport> transport =
            fakeTransport();
        transport->jsonResponse.statusCode = 200;
        transport->jsonResponse.body = QByteArrayLiteral(
            "{\"choices\":[{\"message\":{\"content\":\"默认采样\"}}]}"
        );
        SecretConfig secrets = customSecrets();
        QVERIFY(!secrets.customModels.isEmpty());
        secrets.customModels[0].temperatureEnabled = false;
        secrets.customModels[0].topPEnabled = false;
        OpenAiCompatibleModelProvider provider(
            QStringLiteral("custom:office"),
            transport,
            [secrets]() { return secrets; }
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

        QCOMPARE(result.text, QStringLiteral("默认采样"));
        QVERIFY(result.error.isEmpty());
        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastBody).object();
        QVERIFY(!body.contains(QStringLiteral("temperature")));
        QVERIFY(!body.contains(QStringLiteral("top_p")));
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
            QStringLiteral("gpt-5.6-terra")
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
