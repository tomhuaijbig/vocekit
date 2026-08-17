#include <QtTest>

#include "../../src/providers/baidu_streaming_speech_session.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSharedPointer>
#include <QUrlQuery>

class FakeBaiduStreamingTransport
    : public IProviderStreamingWebSocketTransport
{
public:
    void open(
        const QUrl &requestUrl,
        const NetworkRequestOptions &requestOptions,
        const ProviderStreamingWebSocketCallbacks &requestCallbacks
    ) override
    {
        url = requestUrl;
        options = requestOptions;
        callbacks = requestCallbacks;
        ++openCount;
    }

    void sendText(const QByteArray &message) override
    {
        textFrames.append(message);
    }

    void sendBinary(const QByteArray &message) override
    {
        binaryFrames.append(message);
    }

    void closeNormally() override
    {
        ++normalCloseCount;
    }

    void cancel() override
    {
        ++cancelCount;
    }

    void emitOpened()
    {
        if (callbacks.opened) {
            callbacks.opened();
        }
    }

    void emitText(const QByteArray &message)
    {
        if (callbacks.textMessage) {
            callbacks.textMessage(message);
        }
    }

    QUrl url;
    NetworkRequestOptions options;
    ProviderStreamingWebSocketCallbacks callbacks;
    QList<QByteArray> textFrames;
    QList<QByteArray> binaryFrames;
    int openCount = 0;
    int normalCloseCount = 0;
    int cancelCount = 0;
};

namespace {

SecretConfig baiduRealtimeSecrets()
{
    SecretConfig secrets;
    secrets.baiduAppId = QStringLiteral("123456");
    secrets.baiduApiKey = QStringLiteral("baidu-realtime-key");
    secrets.baiduSecretKey = QStringLiteral("batch-secret");
    return secrets;
}

QByteArray baiduEvent(
    const QString &type,
    const QString &text = QString(),
    int errorNumber = 0
)
{
    QJsonObject root;
    root.insert(QStringLiteral("type"), type);
    if (!text.isNull()) {
        root.insert(QStringLiteral("result"), text);
    }
    if (errorNumber != 0) {
        root.insert(QStringLiteral("err_no"), errorNumber);
        root.insert(QStringLiteral("err_msg"), text);
    }
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QString frameType(const QByteArray &frame)
{
    return QJsonDocument::fromJson(frame).object()
        .value(QStringLiteral("type")).toString();
}

BaiduStreamingSpeechSession::Timing fastTiming()
{
    BaiduStreamingSpeechSession::Timing timing;
    timing.frameIntervalMs = 1;
    timing.queueLimitBytes = 64000;
    return timing;
}

} // namespace

class BaiduStreamingSpeechSessionTests : public QObject
{
    Q_OBJECT

private slots:
    void sendsStartAudioFinishAndCompletes()
    {
        QSharedPointer<FakeBaiduStreamingTransport> fake(
            new FakeBaiduStreamingTransport
        );
        QList<StreamingTranscriptSnapshot> snapshots;
        QString completedText;
        int degradedCount = 0;
        StreamingSpeechCallbacks callbacks;
        callbacks.transcriptUpdated = [&](const StreamingTranscriptSnapshot &snapshot) {
            snapshots.append(snapshot);
        };
        callbacks.completed = [&](const QString &text) {
            completedText = text;
        };
        callbacks.degraded = [&](const QString &) {
            ++degradedCount;
        };
        StreamingSpeechSessionRequest request;
        request.provider = QStringLiteral("baidu");
        request.networkPolicy = QStringLiteral("direct");
        BaiduStreamingSpeechSession session(
            [fake]() {
                return fake.staticCast<IProviderStreamingWebSocketTransport>();
            },
            baiduRealtimeSecrets,
            []() { return QStringLiteral("session-123"); },
            request,
            callbacks,
            fastTiming()
        );

        QString error;
        QVERIFY2(session.start(&error), qPrintable(error));
        QCOMPARE(fake->openCount, 1);
        QCOMPARE(fake->url.host(), QStringLiteral("vop.baidu.com"));
        QCOMPARE(
            QUrlQuery(fake->url).queryItemValue(QStringLiteral("sn")),
            QStringLiteral("session-123")
        );

        fake->emitOpened();
        QCOMPARE(fake->textFrames.size(), 1);
        QCOMPARE(frameType(fake->textFrames.first()), QStringLiteral("START"));
        const QJsonObject startData = QJsonDocument::fromJson(
            fake->textFrames.first()
        ).object().value(QStringLiteral("data")).toObject();
        QCOMPARE(startData.value(QStringLiteral("appid")).toInt(), 123456);
        QCOMPARE(
            startData.value(QStringLiteral("appkey")).toString(),
            QStringLiteral("baidu-realtime-key")
        );
        QCOMPARE(startData.value(QStringLiteral("dev_pid")).toInt(), 15372);
        QCOMPARE(startData.value(QStringLiteral("sample")).toInt(), 16000);
        QCOMPARE(startData.value(QStringLiteral("format")).toString(), QStringLiteral("pcm"));

        QVERIFY(session.pushAudio(QByteArray(10240, char(6))));
        QTRY_COMPARE(fake->binaryFrames.size(), 2);
        QCOMPARE(fake->binaryFrames.at(0).size(), 5120);
        QCOMPARE(fake->binaryFrames.at(1).size(), 5120);

        fake->emitText(baiduEvent(
            QStringLiteral("MID_TEXT"),
            QString::fromUtf8("你好世")
        ));
        QTRY_VERIFY(!snapshots.isEmpty());
        QCOMPARE(snapshots.last().displayText(), QString::fromUtf8("你好世"));

        session.finish();
        QTRY_COMPARE(frameType(fake->textFrames.last()), QStringLiteral("FINISH"));
        fake->emitText(baiduEvent(
            QStringLiteral("FIN_TEXT"),
            QString::fromUtf8("你好世界")
        ));

        QTRY_COMPARE(completedText, QString::fromUtf8("你好世界"));
        QCOMPARE(snapshots.last().committedText, QString::fromUtf8("你好世界"));
        QVERIFY(snapshots.last().provisionalText.isEmpty());
        QCOMPARE(degradedCount, 0);
        QCOMPARE(fake->normalCloseCount, 1);
        QCOMPARE(session.state(), StreamingSpeechState::Completed);
    }

    void replacesMidTextAndCommitsFinText()
    {
        QSharedPointer<FakeBaiduStreamingTransport> fake(
            new FakeBaiduStreamingTransport
        );
        QList<StreamingTranscriptSnapshot> snapshots;
        StreamingSpeechCallbacks callbacks;
        callbacks.transcriptUpdated = [&](const StreamingTranscriptSnapshot &snapshot) {
            snapshots.append(snapshot);
        };
        BaiduStreamingSpeechSession session(
            [fake]() { return fake.staticCast<IProviderStreamingWebSocketTransport>(); },
            baiduRealtimeSecrets,
            []() { return QStringLiteral("session-mid"); },
            StreamingSpeechSessionRequest(),
            callbacks,
            fastTiming()
        );
        QString error;
        QVERIFY(session.start(&error));
        fake->emitOpened();

        fake->emitText(baiduEvent(QStringLiteral("MID_TEXT"), QString::fromUtf8("今天")));
        fake->emitText(baiduEvent(QStringLiteral("MID_TEXT"), QString::fromUtf8("今天天气")));
        QCOMPARE(snapshots.last().displayText(), QString::fromUtf8("今天天气"));

        fake->emitText(baiduEvent(QStringLiteral("FIN_TEXT"), QString::fromUtf8("今天天气很好。")));
        QCOMPARE(snapshots.last().committedText, QString::fromUtf8("今天天气很好。"));
        QVERIFY(snapshots.last().provisionalText.isEmpty());

        fake->emitText(baiduEvent(QStringLiteral("MID_TEXT"), QString::fromUtf8("明天")));
        QCOMPARE(
            snapshots.last().displayText(),
            QString::fromUtf8("今天天气很好。明天")
        );
    }

    void requiresNumericAppIdAndApiKey()
    {
        QSharedPointer<FakeBaiduStreamingTransport> fake(
            new FakeBaiduStreamingTransport
        );
        SecretConfig secrets = baiduRealtimeSecrets();
        secrets.baiduAppId = QStringLiteral("not-a-number");
        BaiduStreamingSpeechSession session(
            [fake]() { return fake.staticCast<IProviderStreamingWebSocketTransport>(); },
            [secrets]() { return secrets; },
            []() { return QStringLiteral("session-invalid"); },
            StreamingSpeechSessionRequest(),
            StreamingSpeechCallbacks(),
            fastTiming()
        );

        QString error;
        QVERIFY(!session.start(&error));
        QVERIFY(error.contains(QString::fromUtf8("AppID")));
        QCOMPARE(fake->openCount, 0);
    }

    void degradesOnlyOnceOnQueueOverflowOrProtocolError()
    {
        QSharedPointer<FakeBaiduStreamingTransport> fake(
            new FakeBaiduStreamingTransport
        );
        BaiduStreamingSpeechSession::Timing timing = fastTiming();
        timing.queueLimitBytes = 5120;
        int degradedCount = 0;
        StreamingSpeechCallbacks callbacks;
        callbacks.degraded = [&](const QString &) { ++degradedCount; };
        BaiduStreamingSpeechSession session(
            [fake]() { return fake.staticCast<IProviderStreamingWebSocketTransport>(); },
            baiduRealtimeSecrets,
            []() { return QStringLiteral("session-overflow"); },
            StreamingSpeechSessionRequest(),
            callbacks,
            timing
        );
        QString error;
        QVERIFY(session.start(&error));

        QVERIFY(!session.pushAudio(QByteArray(5121, char(1))));
        QCOMPARE(degradedCount, 1);
        fake->emitText(baiduEvent(QStringLiteral("ERROR"), QStringLiteral("bad"), 1001));
        QCOMPARE(degradedCount, 1);
        QCOMPARE(session.state(), StreamingSpeechState::Degraded);
    }

    void cancelSendsCancelAndStopsTransport()
    {
        QSharedPointer<FakeBaiduStreamingTransport> fake(
            new FakeBaiduStreamingTransport
        );
        int completedCount = 0;
        StreamingSpeechCallbacks callbacks;
        callbacks.completed = [&](const QString &) { ++completedCount; };
        BaiduStreamingSpeechSession session(
            [fake]() { return fake.staticCast<IProviderStreamingWebSocketTransport>(); },
            baiduRealtimeSecrets,
            []() { return QStringLiteral("session-cancel"); },
            StreamingSpeechSessionRequest(),
            callbacks,
            fastTiming()
        );
        QString error;
        QVERIFY(session.start(&error));
        fake->emitOpened();

        session.cancel();

        QCOMPARE(frameType(fake->textFrames.last()), QStringLiteral("CANCEL"));
        QCOMPARE(fake->cancelCount, 1);
        QCOMPARE(completedCount, 0);
        QCOMPARE(session.state(), StreamingSpeechState::Cancelled);
    }
};

QTEST_MAIN(BaiduStreamingSpeechSessionTests)

#include "baidu_streaming_speech_session_tests.moc"
