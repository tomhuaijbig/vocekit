#include <QtTest>

#include "../../src/providers/baidu_speech_provider.h"
#include "../../src/tasks/cancellation_token.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QUrlQuery>

class FakeBaiduSpeechTransport : public IProviderNetworkTransport
{
public:
    NetworkResponse get(
        const QNetworkRequest &request,
        const NetworkRequestOptions &options,
        const CancellationToken &) override
    {
        ++getCount;
        lastGetRequest = request;
        lastGetOptions = options;
        return getResponses.isEmpty()
            ? NetworkResponse()
            : getResponses.takeFirst();
    }

    NetworkResponse postJson(
        const QNetworkRequest &request,
        const QByteArray &body,
        const NetworkRequestOptions &options,
        const CancellationToken &) override
    {
        ++postJsonCount;
        lastPostRequest = request;
        lastPostBody = body;
        lastPostOptions = options;
        return postResponses.isEmpty()
            ? NetworkResponse()
            : postResponses.takeFirst();
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

    int getCount = 0;
    int postJsonCount = 0;
    int postStreamCount = 0;
    QNetworkRequest lastGetRequest;
    QNetworkRequest lastPostRequest;
    QByteArray lastPostBody;
    NetworkRequestOptions lastGetOptions;
    NetworkRequestOptions lastPostOptions;
    QList<NetworkResponse> getResponses;
    QList<NetworkResponse> postResponses;
};

namespace {

QSharedPointer<FakeBaiduSpeechTransport> fakeTransport()
{
    return QSharedPointer<FakeBaiduSpeechTransport>(
        new FakeBaiduSpeechTransport
    );
}

SecretConfig baiduSecrets()
{
    SecretConfig secrets;
    secrets.baiduApiKey = QStringLiteral("baidu-test-key");
    secrets.baiduSecretKey = QStringLiteral("baidu-test-secret");
    secrets.baiduAppId = QStringLiteral("baidu-test-app");
    return secrets;
}

NetworkResponse tokenResponse(
    const QByteArray &token = QByteArrayLiteral("token-123"),
    int expiresIn = 2592000,
    qint64 durationMs = 7)
{
    NetworkResponse response;
    response.statusCode = 200;
    response.durationMs = durationMs;
    QJsonObject body;
    body.insert(
        QStringLiteral("access_token"),
        QString::fromUtf8(token)
    );
    body.insert(QStringLiteral("expires_in"), expiresIn);
    response.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return response;
}

NetworkResponse recognitionResponse(
    const QString &text,
    qint64 durationMs = 13)
{
    NetworkResponse response;
    response.statusCode = 200;
    response.durationMs = durationMs;
    QJsonObject body;
    body.insert(QStringLiteral("err_no"), 0);
    QJsonArray results;
    if (!text.isNull()) {
        results.append(text);
    }
    body.insert(QStringLiteral("result"), results);
    response.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return response;
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

class BaiduSpeechProviderTests : public QObject
{
    Q_OBJECT

private slots:
    void obtainsTokenAndSendsExpectedRecognitionRequest()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        transport->getResponses << tokenResponse();
        const NetworkResponse expectedRecognition =
            recognitionResponse(QString::fromUtf8("识别成功"));
        transport->postResponses << expectedRecognition;
        BaiduSpeechProvider provider(
            transport,
            []() { return baiduSecrets(); }
        );
        CancellationSource cancellation;
        const SpeechRecognitionRequest request =
            recognitionRequest(cancellation);

        const SpeechRecognitionResult result =
            provider.recognize(request, cancellation.token());

        QCOMPARE(result.text, QString::fromUtf8("识别成功"));
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.executionId, request.executionId);
        QCOMPARE(result.durationMs, qint64(20));
        QCOMPARE(result.rawResponse, expectedRecognition.body);
        QCOMPARE(transport->getCount, 1);
        QCOMPARE(transport->postJsonCount, 1);
        QCOMPARE(transport->postStreamCount, 0);

        const QUrl tokenUrl = transport->lastGetRequest.url();
        QCOMPARE(
            tokenUrl.adjusted(QUrl::RemoveQuery),
            QUrl(QStringLiteral(
                "https://aip.baidubce.com/oauth/2.0/token"
            ))
        );
        const QUrlQuery tokenQuery(tokenUrl);
        QCOMPARE(
            tokenQuery.queryItemValue(QStringLiteral("grant_type")),
            QStringLiteral("client_credentials")
        );
        QCOMPARE(
            tokenQuery.queryItemValue(QStringLiteral("client_id")),
            QStringLiteral("baidu-test-key")
        );
        QCOMPARE(
            tokenQuery.queryItemValue(QStringLiteral("client_secret")),
            QStringLiteral("baidu-test-secret")
        );

        QCOMPARE(
            transport->lastPostRequest.url(),
            QUrl(QStringLiteral("https://vop.baidu.com/server_api"))
        );
        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastPostBody).object();
        QCOMPARE(
            body.value(QStringLiteral("format")).toString(),
            QStringLiteral("pcm")
        );
        QCOMPARE(body.value(QStringLiteral("rate")).toInt(), 16000);
        QCOMPARE(body.value(QStringLiteral("channel")).toInt(), 1);
        QCOMPARE(
            body.value(QStringLiteral("token")).toString(),
            QStringLiteral("token-123")
        );
        QCOMPARE(body.value(QStringLiteral("len")).toInt(), 4);
        QCOMPARE(
            body.value(QStringLiteral("speech")).toString(),
            QString::fromLatin1(request.audioData.toBase64())
        );
        QCOMPARE(body.value(QStringLiteral("dev_pid")).toInt(), 1537);
        QVERIFY(!body.value(QStringLiteral("cuid")).toString().isEmpty());
        QCOMPARE(
            transport->lastPostOptions.networkPolicy,
            QStringLiteral("proxy")
        );
        QCOMPARE(transport->lastPostOptions.timeoutMs, 45678);
    }

    void reusesCachedAccessToken()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        transport->getResponses << tokenResponse();
        transport->postResponses
            << recognitionResponse(QStringLiteral("first"))
            << recognitionResponse(QStringLiteral("second"));
        BaiduSpeechProvider provider(
            transport,
            []() { return baiduSecrets(); }
        );

        CancellationSource firstCancellation;
        CancellationSource secondCancellation;
        const SpeechRecognitionResult first = provider.recognize(
            recognitionRequest(firstCancellation),
            firstCancellation.token()
        );
        const SpeechRecognitionResult second = provider.recognize(
            recognitionRequest(secondCancellation),
            secondCancellation.token()
        );

        QCOMPARE(first.text, QStringLiteral("first"));
        QCOMPARE(second.text, QStringLiteral("second"));
        QCOMPARE(transport->getCount, 1);
        QCOMPARE(transport->postJsonCount, 2);
    }

    void reusesSharedAccessTokenAcrossProviderInstances()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> firstTransport =
            fakeTransport();
        firstTransport->getResponses
            << tokenResponse(QByteArrayLiteral("shared-token"));
        firstTransport->postResponses
            << recognitionResponse(QStringLiteral("first"));
        BaiduSpeechProvider firstProvider(
            firstTransport,
            []() { return baiduSecrets(); },
            false,
            true
        );

        CancellationSource firstCancellation;
        QCOMPARE(
            firstProvider.recognize(
                recognitionRequest(firstCancellation),
                firstCancellation.token()
            ).text,
            QStringLiteral("first")
        );

        const QSharedPointer<FakeBaiduSpeechTransport> secondTransport =
            fakeTransport();
        secondTransport->postResponses
            << recognitionResponse(QStringLiteral("second"));
        BaiduSpeechProvider secondProvider(
            secondTransport,
            []() { return baiduSecrets(); },
            false,
            true
        );
        CancellationSource secondCancellation;
        const SpeechRecognitionResult second = secondProvider.recognize(
            recognitionRequest(secondCancellation),
            secondCancellation.token()
        );

        QCOMPARE(second.text, QStringLiteral("second"));
        QCOMPARE(secondTransport->getCount, 0);
        QCOMPARE(secondTransport->postJsonCount, 1);
        QCOMPARE(second.durationMs, qint64(13));
    }

    void refreshConfigurationClearsCachedAccessToken()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        transport->getResponses
            << tokenResponse(QByteArrayLiteral("first-token"))
            << tokenResponse(QByteArrayLiteral("second-token"));
        transport->postResponses
            << recognitionResponse(QStringLiteral("first"))
            << recognitionResponse(QStringLiteral("second"));
        BaiduSpeechProvider provider(
            transport,
            []() { return baiduSecrets(); }
        );

        CancellationSource firstCancellation;
        provider.recognize(
            recognitionRequest(firstCancellation),
            firstCancellation.token()
        );
        provider.refreshConfiguration();
        CancellationSource secondCancellation;
        provider.recognize(
            recognitionRequest(secondCancellation),
            secondCancellation.token()
        );

        QCOMPARE(transport->getCount, 2);
        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastPostBody).object();
        QCOMPARE(
            body.value(QStringLiteral("token")).toString(),
            QStringLiteral("second-token")
        );
    }

    void selfCheckOnlyObtainsAccessToken()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        transport->getResponses << tokenResponse();
        BaiduSpeechProvider provider(
            transport,
            []() { return baiduSecrets(); }
        );

        const ProviderCheckResult result =
            provider.checkConfiguration();

        QVERIFY(result.available);
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.durationMs, qint64(7));
        QVERIFY(result.message.contains(QString::fromUtf8("令牌获取成功")));
        QCOMPARE(transport->getCount, 1);
        QCOMPARE(transport->postJsonCount, 0);
    }

    void missingCredentialsDoNotReachNetwork()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        BaiduSpeechProvider provider(
            transport,
            []() { return SecretConfig(); }
        );

        const ProviderCheckResult check =
            provider.checkConfiguration();
        CancellationSource cancellation;
        const SpeechRecognitionResult recognition = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(
            check.error.code,
            QStringLiteral("provider.configuration")
        );
        QCOMPARE(
            recognition.error.code,
            QStringLiteral("provider.configuration")
        );
        QCOMPARE(transport->getCount, 0);
        QCOMPARE(transport->postJsonCount, 0);
    }

    void emptyAudioDoesNotReachNetwork()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        BaiduSpeechProvider provider(
            transport,
            []() { return baiduSecrets(); }
        );
        CancellationSource cancellation;
        SpeechRecognitionRequest request;

        const SpeechRecognitionResult result =
            provider.recognize(request, cancellation.token());

        QCOMPARE(result.error.code, QStringLiteral("speech.empty_audio"));
        QCOMPARE(transport->getCount, 0);
        QCOMPARE(transport->postJsonCount, 0);
    }

    void loadsAudioFromFile()
    {
        QTemporaryFile audioFile;
        QVERIFY(audioFile.open());
        QCOMPARE(audioFile.write(QByteArray::fromHex("102030")), qint64(3));
        audioFile.flush();

        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        transport->getResponses << tokenResponse();
        transport->postResponses
            << recognitionResponse(QStringLiteral("from-file"));
        BaiduSpeechProvider provider(
            transport,
            []() { return baiduSecrets(); }
        );
        CancellationSource cancellation;
        SpeechRecognitionRequest request;
        request.audioPath = audioFile.fileName();

        const SpeechRecognitionResult result =
            provider.recognize(request, cancellation.token());

        QCOMPARE(result.text, QStringLiteral("from-file"));
        const QJsonObject body =
            QJsonDocument::fromJson(transport->lastPostBody).object();
        QCOMPARE(body.value(QStringLiteral("len")).toInt(), 3);
    }

    void reportsMissingAccessToken()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        NetworkResponse response;
        response.statusCode = 200;
        response.body =
            QByteArrayLiteral("{\"error_description\":\"invalid key\"}");
        transport->getResponses << response;
        BaiduSpeechProvider provider(
            transport,
            []() { return baiduSecrets(); }
        );
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("speech.baidu_token"));
        QVERIFY(result.error.message.contains(QStringLiteral("invalid key")));
        QCOMPARE(transport->postJsonCount, 0);
    }

    void reportsRecognitionApiError()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        transport->getResponses << tokenResponse();
        NetworkResponse response;
        response.statusCode = 200;
        response.body = QByteArrayLiteral(
            "{\"err_no\":3301,\"err_msg\":\"speech quality error\"}"
        );
        transport->postResponses << response;
        BaiduSpeechProvider provider(
            transport,
            []() { return baiduSecrets(); }
        );
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("speech.api"));
        QVERIFY(
            result.error.message.contains(
                QStringLiteral("speech quality error")
            )
        );
    }

    void reportsEmptyRecognitionResult()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        transport->getResponses << tokenResponse();
        transport->postResponses
            << recognitionResponse(QString());
        BaiduSpeechProvider provider(
            transport,
            []() { return baiduSecrets(); }
        );
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("speech.empty_result"));
    }

    void convertsTokenNetworkTimeout()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        NetworkResponse response;
        response.error.code = QStringLiteral("network.timeout");
        response.error.message = QStringLiteral("timed out");
        response.error.retryable = true;
        transport->getResponses << response;
        BaiduSpeechProvider provider(
            transport,
            []() { return baiduSecrets(); }
        );
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("network.timeout"));
        QVERIFY(result.error.message.contains(QString::fromUtf8("百度令牌")));
        QVERIFY(result.error.message.contains(QString::fromUtf8("超时")));
        QVERIFY(result.error.retryable);
        QCOMPARE(transport->postJsonCount, 0);
    }

    void convertsRecognitionNetworkTimeout()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        transport->getResponses << tokenResponse();
        NetworkResponse response;
        response.error.code = QStringLiteral("network.timeout");
        response.error.message = QStringLiteral("timed out");
        response.error.retryable = true;
        transport->postResponses << response;
        BaiduSpeechProvider provider(
            transport,
            []() { return baiduSecrets(); }
        );
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("network.timeout"));
        QVERIFY(
            result.error.message.contains(QString::fromUtf8("百度语音识别"))
        );
        QVERIFY(result.error.message.contains(QString::fromUtf8("超时")));
        QVERIFY(result.error.retryable);
    }

    void cancellationDoesNotReachNetwork()
    {
        const QSharedPointer<FakeBaiduSpeechTransport> transport =
            fakeTransport();
        BaiduSpeechProvider provider(
            transport,
            []() { return baiduSecrets(); }
        );
        CancellationSource cancellation;
        cancellation.cancel();

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("task.cancelled"));
        QCOMPARE(transport->getCount, 0);
        QCOMPARE(transport->postJsonCount, 0);
    }
};

QTEST_MAIN(BaiduSpeechProviderTests)
#include "baidu_speech_provider_tests.moc"
