#include <QtTest>

#include "../../src/providers/xfyun_speech_provider.h"
#include "../../src/tasks/cancellation_token.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QUrlQuery>

class FakeXfyunWebSocketTransport : public IProviderWebSocketTransport
{
public:
    ProviderWebSocketResult exchange(
        const ProviderWebSocketRequest &request,
        const WebSocketCompletionPredicate &completion,
        const CancellationToken &) override
    {
        ++exchangeCount;
        lastRequest = request;
        completionResults.clear();
        for (const QByteArray &message : nextResult.messages) {
            completionResults.append(completion(message));
        }
        return nextResult;
    }

    int exchangeCount = 0;
    ProviderWebSocketRequest lastRequest;
    ProviderWebSocketResult nextResult;
    QList<bool> completionResults;
};

namespace {

QSharedPointer<FakeXfyunWebSocketTransport> fakeTransport()
{
    return QSharedPointer<FakeXfyunWebSocketTransport>(
        new FakeXfyunWebSocketTransport
    );
}

SecretConfig xfyunSecrets(const QString &appId = QStringLiteral("app-123"))
{
    SecretConfig secrets;
    secrets.xfyunAppId = appId;
    secrets.xfyunApiKey = QStringLiteral("api-key-123");
    secrets.xfyunApiSecret = QStringLiteral("api-secret-123");
    return secrets;
}

QDateTime fixedUtcNow()
{
    return QDateTime(
        QDate(2026, 7, 24),
        QTime(1, 2, 3),
        Qt::UTC
    );
}

QByteArray recognitionMessage(
    int status,
    const QString &word = QString(),
    int code = 0,
    const QString &message = QString())
{
    QJsonObject root;
    root.insert(QStringLiteral("code"), code);
    if (!message.isEmpty()) {
        root.insert(QStringLiteral("message"), message);
    }

    QJsonObject data;
    data.insert(QStringLiteral("status"), status);
    if (!word.isNull()) {
        QJsonObject candidate;
        candidate.insert(QStringLiteral("w"), word);
        QJsonArray candidates;
        candidates.append(candidate);
        QJsonObject wordGroup;
        wordGroup.insert(QStringLiteral("cw"), candidates);
        QJsonArray words;
        words.append(wordGroup);
        QJsonObject recognition;
        recognition.insert(QStringLiteral("ws"), words);
        data.insert(QStringLiteral("result"), recognition);
    }
    root.insert(QStringLiteral("data"), data);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

SpeechRecognitionRequest recognitionRequest(
    const CancellationSource &cancellation)
{
    SpeechRecognitionRequest request;
    request.executionId = cancellation.executionId();
    request.audioData = QByteArray(2000, char(7));
    request.audioFormat = QStringLiteral("pcm");
    request.sampleRate = 16000;
    request.network.timeoutMs = 45678;
    request.network.networkPolicy = QStringLiteral("systemProxy");
    return request;
}

XfyunSpeechProvider providerWith(
    const QSharedPointer<FakeXfyunWebSocketTransport> &transport,
    const XfyunSpeechProvider::SecretLoader &secretLoader =
        []() { return xfyunSecrets(); })
{
    return XfyunSpeechProvider(
        transport,
        secretLoader,
        fixedUtcNow
    );
}

} // namespace

class XfyunSpeechProviderTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsSignedRequestFramesAndCombinesRecognitionText()
    {
        const QSharedPointer<FakeXfyunWebSocketTransport> transport =
            fakeTransport();
        transport->nextResult.messages
            << recognitionMessage(1, QString::fromUtf8("你好"))
            << recognitionMessage(2, QString::fromUtf8("世界"));
        transport->nextResult.durationMs = 23;
        XfyunSpeechProvider provider = providerWith(transport);
        CancellationSource cancellation;
        const SpeechRecognitionRequest request =
            recognitionRequest(cancellation);

        const SpeechRecognitionResult result =
            provider.recognize(request, cancellation.token());

        QCOMPARE(result.text, QString::fromUtf8("你好世界"));
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.executionId, request.executionId);
        QCOMPARE(result.durationMs, qint64(23));
        QCOMPARE(transport->exchangeCount, 1);
        QCOMPARE(transport->lastRequest.url.scheme(), QStringLiteral("wss"));
        QCOMPARE(
            transport->lastRequest.url.host(),
            QStringLiteral("iat-api.xfyun.cn")
        );
        QCOMPARE(
            transport->lastRequest.url.path(),
            QStringLiteral("/v2/iat")
        );
        const QUrlQuery query(transport->lastRequest.url);
        QCOMPARE(
            query.queryItemValue(QStringLiteral("date")),
            QStringLiteral("Fri, 24 Jul 2026 01:02:03 GMT")
        );
        QCOMPARE(
            query.queryItemValue(QStringLiteral("host")),
            QStringLiteral("iat-api.xfyun.cn")
        );
        const QByteArray authorization = QByteArray::fromBase64(
            query.queryItemValue(QStringLiteral("authorization")).toLatin1()
        );
        QVERIFY(authorization.contains("api_key=\"api-key-123\""));
        QVERIFY(authorization.contains("algorithm=\"hmac-sha256\""));
        QVERIFY(authorization.contains("signature=\""));

        QCOMPARE(transport->lastRequest.textFrames.size(), 3);
        const QJsonObject first = QJsonDocument::fromJson(
            transport->lastRequest.textFrames.at(0)
        ).object();
        const QJsonObject middle = QJsonDocument::fromJson(
            transport->lastRequest.textFrames.at(1)
        ).object();
        const QJsonObject last = QJsonDocument::fromJson(
            transport->lastRequest.textFrames.at(2)
        ).object();
        QCOMPARE(
            first.value(QStringLiteral("common")).toObject()
                .value(QStringLiteral("app_id")).toString(),
            QStringLiteral("app-123")
        );
        QCOMPARE(
            first.value(QStringLiteral("business")).toObject()
                .value(QStringLiteral("language")).toString(),
            QStringLiteral("zh_cn")
        );
        QCOMPARE(
            first.value(QStringLiteral("data")).toObject()
                .value(QStringLiteral("status")).toInt(),
            0
        );
        QCOMPARE(
            middle.value(QStringLiteral("data")).toObject()
                .value(QStringLiteral("status")).toInt(),
            1
        );
        QCOMPARE(
            last.value(QStringLiteral("data")).toObject()
                .value(QStringLiteral("status")).toInt(),
            2
        );
        QCOMPARE(
            QByteArray::fromBase64(
                first.value(QStringLiteral("data")).toObject()
                    .value(QStringLiteral("audio")).toString().toLatin1()
            ).size(),
            1280
        );
        QCOMPARE(
            QByteArray::fromBase64(
                middle.value(QStringLiteral("data")).toObject()
                    .value(QStringLiteral("audio")).toString().toLatin1()
            ).size(),
            720
        );
        QVERIFY(
            last.value(QStringLiteral("data")).toObject()
                .value(QStringLiteral("audio")).toString().isEmpty()
        );
        QCOMPARE(transport->lastRequest.frameIntervalMs, 40);
        QCOMPARE(
            transport->lastRequest.network.networkPolicy,
            QStringLiteral("systemProxy")
        );
        QCOMPARE(transport->lastRequest.network.timeoutMs, 45678);
        QCOMPARE(transport->completionResults, QList<bool>() << false << true);
    }

    void loadsAudioFromFile()
    {
        QTemporaryFile audioFile;
        QVERIFY(audioFile.open());
        QCOMPARE(audioFile.write(QByteArray(3, char(9))), qint64(3));
        audioFile.flush();

        const QSharedPointer<FakeXfyunWebSocketTransport> transport =
            fakeTransport();
        transport->nextResult.messages
            << recognitionMessage(2, QStringLiteral("file"));
        XfyunSpeechProvider provider = providerWith(transport);
        CancellationSource cancellation;
        SpeechRecognitionRequest request;
        request.executionId = cancellation.executionId();
        request.audioPath = audioFile.fileName();

        const SpeechRecognitionResult result =
            provider.recognize(request, cancellation.token());

        QCOMPARE(result.text, QStringLiteral("file"));
        const QJsonObject first = QJsonDocument::fromJson(
            transport->lastRequest.textFrames.constFirst()
        ).object();
        QCOMPARE(
            QByteArray::fromBase64(
                first.value(QStringLiteral("data")).toObject()
                    .value(QStringLiteral("audio")).toString().toLatin1()
            ).size(),
            3
        );
    }

    void missingCredentialsDoNotReachTransport()
    {
        const QSharedPointer<FakeXfyunWebSocketTransport> transport =
            fakeTransport();
        XfyunSpeechProvider provider = providerWith(
            transport,
            []() { return SecretConfig(); }
        );
        CancellationSource cancellation;

        const ProviderCheckResult check = provider.checkConfiguration();
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
        QCOMPARE(transport->exchangeCount, 0);
    }

    void emptyAudioDoesNotReachTransport()
    {
        const QSharedPointer<FakeXfyunWebSocketTransport> transport =
            fakeTransport();
        XfyunSpeechProvider provider = providerWith(transport);
        CancellationSource cancellation;
        SpeechRecognitionRequest request;

        const SpeechRecognitionResult result =
            provider.recognize(request, cancellation.token());

        QCOMPARE(result.error.code, QStringLiteral("speech.empty_audio"));
        QCOMPARE(transport->exchangeCount, 0);
    }

    void reportsApiError()
    {
        const QSharedPointer<FakeXfyunWebSocketTransport> transport =
            fakeTransport();
        transport->nextResult.messages
            << recognitionMessage(
                   2,
                   QString(),
                   10105,
                   QStringLiteral("invalid authorization")
               );
        XfyunSpeechProvider provider = providerWith(transport);
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("speech.api"));
        QVERIFY(
            result.error.message.contains(
                QStringLiteral("invalid authorization")
            )
        );
    }

    void reportsEmptyRecognitionResult()
    {
        const QSharedPointer<FakeXfyunWebSocketTransport> transport =
            fakeTransport();
        transport->nextResult.messages
            << recognitionMessage(2, QString());
        XfyunSpeechProvider provider = providerWith(transport);
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("speech.empty_result"));
    }

    void convertsNetworkTimeout()
    {
        const QSharedPointer<FakeXfyunWebSocketTransport> transport =
            fakeTransport();
        transport->nextResult.error.code =
            QStringLiteral("network.timeout");
        transport->nextResult.error.message =
            QStringLiteral("network timed out");
        transport->nextResult.error.retryable = true;
        XfyunSpeechProvider provider = providerWith(transport);
        CancellationSource cancellation;

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("network.timeout"));
        QVERIFY(result.error.message.contains(QString::fromUtf8("讯飞")));
        QVERIFY(result.error.message.contains(QString::fromUtf8("超时")));
        QVERIFY(result.error.retryable);
    }

    void cancellationDoesNotReachTransport()
    {
        const QSharedPointer<FakeXfyunWebSocketTransport> transport =
            fakeTransport();
        XfyunSpeechProvider provider = providerWith(transport);
        CancellationSource cancellation;
        cancellation.cancel();

        const SpeechRecognitionResult result = provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        QCOMPARE(result.error.code, QStringLiteral("task.cancelled"));
        QCOMPARE(transport->exchangeCount, 0);
    }

    void selfCheckAcceptsSuccessfulSilentRecognition()
    {
        const QSharedPointer<FakeXfyunWebSocketTransport> transport =
            fakeTransport();
        transport->nextResult.messages
            << recognitionMessage(2, QString());
        transport->nextResult.durationMs = 12;
        XfyunSpeechProvider provider = providerWith(transport);

        const ProviderCheckResult result = provider.checkConfiguration();

        QVERIFY(result.available);
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.durationMs, qint64(12));
        QVERIFY(result.message.contains(QString::fromUtf8("鉴权成功")));
        QCOMPARE(transport->exchangeCount, 1);
    }

    void refreshConfigurationUsesNewSecrets()
    {
        const QSharedPointer<FakeXfyunWebSocketTransport> transport =
            fakeTransport();
        transport->nextResult.messages
            << recognitionMessage(2, QStringLiteral("ok"));
        int loadCount = 0;
        XfyunSpeechProvider provider(
            transport,
            [&loadCount]() {
                ++loadCount;
                return xfyunSecrets(
                    loadCount == 1
                        ? QStringLiteral("first")
                        : QStringLiteral("second")
                );
            },
            fixedUtcNow
        );
        provider.refreshConfiguration();
        CancellationSource cancellation;

        provider.recognize(
            recognitionRequest(cancellation),
            cancellation.token()
        );

        const QJsonObject first = QJsonDocument::fromJson(
            transport->lastRequest.textFrames.constFirst()
        ).object();
        QCOMPARE(
            first.value(QStringLiteral("common")).toObject()
                .value(QStringLiteral("app_id")).toString(),
            QStringLiteral("second")
        );
    }
};

QTEST_MAIN(XfyunSpeechProviderTests)
#include "xfyun_speech_provider_tests.moc"
