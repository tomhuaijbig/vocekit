#include <QtTest>

#include "../../src/providers/custom_speech_provider.h"
#include "../../src/tasks/cancellation_token.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

class FakeCustomSpeechTransport : public IProviderNetworkTransport
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
        return response;
    }

    NetworkResponse postEventStream(
        const QNetworkRequest &,
        const QByteArray &,
        const NetworkRequestOptions &,
        const StreamDataCallback &,
        const CancellationToken &) override
    {
        ++postStreamCount;
        return NetworkResponse();
    }

    int postJsonCount = 0;
    int postStreamCount = 0;
    QNetworkRequest lastRequest;
    QByteArray lastBody;
    NetworkRequestOptions lastOptions;
    NetworkResponse response;
};

namespace {

QSharedPointer<FakeCustomSpeechTransport> fakeTransport()
{
    return QSharedPointer<FakeCustomSpeechTransport>(
        new FakeCustomSpeechTransport
    );
}

SecretConfig customSpeechSecrets()
{
    SecretConfig secrets;
    secrets.customSpeechUrl = QStringLiteral("speech.example.com/asr");
    secrets.customSpeechApiKey = QStringLiteral("speech-test-key");
    secrets.customSpeechModel = QStringLiteral("whisper-test");
    return secrets;
}

SpeechRecognitionRequest recognitionRequest(
    const CancellationSource &cancellation)
{
    SpeechRecognitionRequest request;
    request.executionId = cancellation.executionId();
    request.audioData = QByteArray::fromHex("00010203");
    request.audioFormat = QStringLiteral("pcm");
    request.sampleRate = 16000;
    request.network.timeoutMs = 45678;
    request.network.networkPolicy = QStringLiteral("proxy");
    return request;
}

} // namespace

class CustomSpeechProviderTests : public QObject
{
    Q_OBJECT

private slots:
    void sendsExpectedJsonAndParsesText()
    {
        const QSharedPointer<FakeCustomSpeechTransport> transport =
            fakeTransport();
        transport->response.statusCode = 200;
        transport->response.durationMs = 23;
        transport->response.body =
            QByteArrayLiteral("{\"text\":\"recognized text\"}");
        CustomSpeechProvider provider(
            transport,
            []() { return customSpeechSecrets(); }
        );
        CancellationSource cancellation;
        const SpeechRecognitionRequest request =
            recognitionRequest(cancellation);

        const SpeechRecognitionResult result =
            provider.recognize(request, cancellation.token());

        QCOMPARE(result.text, QStringLiteral("recognized text"));
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.executionId, request.executionId);
        QCOMPARE(result.durationMs, qint64(23));
        QCOMPARE(result.rawResponse, transport->response.body);
        QCOMPARE(transport->postJsonCount, 1);
        QCOMPARE(transport->postStreamCount, 0);
        QCOMPARE(
            transport->lastRequest.url(),
            QUrl(QStringLiteral("https://speech.example.com/asr"))
        );
        QCOMPARE(
            transport->lastRequest.rawHeader("Authorization"),
            QByteArrayLiteral("Bearer speech-test-key")
        );
        QCOMPARE(transport->lastOptions.timeoutMs, 45678);
        QCOMPARE(
            transport->lastOptions.networkPolicy,
            QStringLiteral("proxy")
        );

        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastBody).object();
        QCOMPARE(
            body.value(QStringLiteral("format")).toString(),
            QStringLiteral("pcm")
        );
        QCOMPARE(body.value(QStringLiteral("rate")).toInt(), 16000);
        QCOMPARE(body.value(QStringLiteral("channel")).toInt(), 1);
        QCOMPARE(body.value(QStringLiteral("len")).toInt(), 4);
        QCOMPARE(
            body.value(QStringLiteral("speech")).toString(),
            QString::fromLatin1(request.audioData.toBase64())
        );
        QCOMPARE(
            body.value(QStringLiteral("model")).toString(),
            QStringLiteral("whisper-test")
        );
    }

    void parsesSupportedNestedTextFields()
    {
        const QSharedPointer<FakeCustomSpeechTransport> transport =
            fakeTransport();
        transport->response.statusCode = 200;
        transport->response.body =
            QByteArrayLiteral("{\"data\":{\"result\":\"nested text\"}}");
        CustomSpeechProvider provider(
            transport,
            []() { return customSpeechSecrets(); }
        );
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.text, QStringLiteral("nested text"));
        QVERIFY(result.error.isEmpty());
    }

    void loadsAudioFromFileWhenDataIsEmpty()
    {
        QTemporaryFile audioFile;
        QVERIFY(audioFile.open());
        QCOMPARE(audioFile.write(QByteArray::fromHex("102030")), qint64(3));
        audioFile.flush();

        const QSharedPointer<FakeCustomSpeechTransport> transport =
            fakeTransport();
        transport->response.statusCode = 200;
        transport->response.body =
            QByteArrayLiteral("{\"transcript\":\"from file\"}");
        CustomSpeechProvider provider(
            transport,
            []() { return customSpeechSecrets(); }
        );
        CancellationSource cancellation;
        SpeechRecognitionRequest request;
        request.audioPath = audioFile.fileName();

        const SpeechRecognitionResult result =
            provider.recognize(request, cancellation.token());

        QCOMPARE(result.text, QStringLiteral("from file"));
        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastBody).object();
        QCOMPARE(body.value(QStringLiteral("len")).toInt(), 3);
    }

    void reportsApiErrorObject()
    {
        const QSharedPointer<FakeCustomSpeechTransport> transport =
            fakeTransport();
        transport->response.statusCode = 200;
        transport->response.body = QByteArrayLiteral(
            "{\"error\":{\"message\":\"bad audio\"}}"
        );
        CustomSpeechProvider provider(
            transport,
            []() { return customSpeechSecrets(); }
        );
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("speech.api"));
        QVERIFY(result.error.message.contains(QStringLiteral("bad audio")));
    }

    void reportsInvalidJson()
    {
        const QSharedPointer<FakeCustomSpeechTransport> transport =
            fakeTransport();
        transport->response.statusCode = 200;
        transport->response.body = QByteArrayLiteral("not-json");
        CustomSpeechProvider provider(
            transport,
            []() { return customSpeechSecrets(); }
        );
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(
            result.error.code,
            QStringLiteral("speech.invalid_response")
        );
    }

    void reportsMissingRecognitionText()
    {
        const QSharedPointer<FakeCustomSpeechTransport> transport =
            fakeTransport();
        transport->response.statusCode = 200;
        transport->response.body = QByteArrayLiteral("{\"ok\":true}");
        CustomSpeechProvider provider(
            transport,
            []() { return customSpeechSecrets(); }
        );
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("speech.empty_result"));
    }

    void missingEndpointDoesNotReachNetwork()
    {
        const QSharedPointer<FakeCustomSpeechTransport> transport =
            fakeTransport();
        CustomSpeechProvider provider(
            transport,
            []() { return SecretConfig(); }
        );
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(
            result.error.code,
            QStringLiteral("provider.configuration")
        );
        QCOMPARE(transport->postJsonCount, 0);
    }

    void invalidEndpointDoesNotReachNetwork()
    {
        const QSharedPointer<FakeCustomSpeechTransport> transport =
            fakeTransport();
        CustomSpeechProvider provider(
            transport,
            []() {
                SecretConfig secrets;
                secrets.customSpeechUrl = QStringLiteral("https:///");
                return secrets;
            }
        );
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(
            result.error.code,
            QStringLiteral("speech.invalid_endpoint")
        );
        QCOMPARE(transport->postJsonCount, 0);
    }

    void emptyAudioDoesNotReachNetwork()
    {
        const QSharedPointer<FakeCustomSpeechTransport> transport =
            fakeTransport();
        CustomSpeechProvider provider(
            transport,
            []() { return customSpeechSecrets(); }
        );
        CancellationSource cancellation;
        SpeechRecognitionRequest request;

        const SpeechRecognitionResult result =
            provider.recognize(request, cancellation.token());

        QCOMPARE(result.error.code, QStringLiteral("speech.empty_audio"));
        QCOMPARE(transport->postJsonCount, 0);
    }

    void cancellationDoesNotReachNetwork()
    {
        const QSharedPointer<FakeCustomSpeechTransport> transport =
            fakeTransport();
        CustomSpeechProvider provider(
            transport,
            []() { return customSpeechSecrets(); }
        );
        CancellationSource cancellation;
        cancellation.cancel();

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("task.cancelled"));
        QCOMPARE(transport->postJsonCount, 0);
    }

    void convertsNetworkTimeout()
    {
        const QSharedPointer<FakeCustomSpeechTransport> transport =
            fakeTransport();
        transport->response.error.code =
            QStringLiteral("network.timeout");
        transport->response.error.message =
            QStringLiteral("timed out");
        CustomSpeechProvider provider(
            transport,
            []() { return customSpeechSecrets(); }
        );
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("network.timeout"));
        QVERIFY(
            result.error.message.contains(QString::fromUtf8("自定义语音"))
        );
        QVERIFY(result.error.message.contains(QString::fromUtf8("超时")));
        QVERIFY(result.error.retryable);
    }

    void selfCheckAcceptsSilentEmptyResult()
    {
        const QSharedPointer<FakeCustomSpeechTransport> transport =
            fakeTransport();
        transport->response.statusCode = 200;
        transport->response.durationMs = 12;
        transport->response.body = QByteArrayLiteral("{\"ok\":true}");
        CustomSpeechProvider provider(
            transport,
            []() { return customSpeechSecrets(); }
        );

        const ProviderCheckResult result =
            provider.checkConfiguration();

        QVERIFY(result.available);
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.durationMs, qint64(12));
        QVERIFY(result.message.contains(QString::fromUtf8("已连通")));
        QCOMPARE(transport->postJsonCount, 1);
    }
};

QTEST_MAIN(CustomSpeechProviderTests)
#include "custom_speech_provider_tests.moc"
