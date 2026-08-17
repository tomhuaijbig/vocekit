#include <QtTest>

#include "../../src/providers/xfyun_streaming_speech_session.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSharedPointer>
#include <QUrlQuery>

class FakeXfyunStreamingTransport
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

SecretConfig testSecrets()
{
    SecretConfig secrets;
    secrets.xfyunAppId = QStringLiteral("app-123");
    secrets.xfyunApiKey = QStringLiteral("key-123");
    secrets.xfyunApiSecret = QStringLiteral("secret-123");
    return secrets;
}

QDateTime fixedUtcNow()
{
    return QDateTime(
        QDate(2026, 8, 11),
        QTime(1, 2, 3),
        Qt::UTC
    );
}

QByteArray xfyunMessage(
    int status,
    int sequence,
    const QString &text,
    const QString &pgs = QStringLiteral("apd"),
    int rangeStart = -1,
    int rangeEnd = -1,
    int code = 0
)
{
    QJsonObject root;
    root.insert(QStringLiteral("code"), code);
    if (code != 0) {
        root.insert(QStringLiteral("message"), QStringLiteral("failed"));
    }
    QJsonObject data;
    data.insert(QStringLiteral("status"), status);
    QJsonObject result;
    result.insert(QStringLiteral("sn"), sequence);
    result.insert(QStringLiteral("pgs"), pgs);
    if (rangeStart > 0 && rangeEnd >= rangeStart) {
        QJsonArray range;
        range.append(rangeStart);
        range.append(rangeEnd);
        result.insert(QStringLiteral("rg"), range);
    }
    QJsonObject candidate;
    candidate.insert(QStringLiteral("w"), text);
    QJsonArray candidates;
    candidates.append(candidate);
    QJsonObject word;
    word.insert(QStringLiteral("cw"), candidates);
    QJsonArray words;
    words.append(word);
    result.insert(QStringLiteral("ws"), words);
    data.insert(QStringLiteral("result"), result);
    root.insert(QStringLiteral("data"), data);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

int frameStatus(const QByteArray &frame)
{
    return QJsonDocument::fromJson(frame).object()
        .value(QStringLiteral("data")).toObject()
        .value(QStringLiteral("status")).toInt(-1);
}

int frameAudioSize(const QByteArray &frame)
{
    return QByteArray::fromBase64(
        QJsonDocument::fromJson(frame).object()
            .value(QStringLiteral("data")).toObject()
            .value(QStringLiteral("audio")).toString().toLatin1()
    ).size();
}

XfyunStreamingSpeechSession::Timing fastTiming()
{
    XfyunStreamingSpeechSession::Timing timing;
    timing.frameIntervalMs = 1;
    timing.rotationIntervalMs = 60000;
    timing.queueLimitBytes = 64000;
    return timing;
}

} // namespace

class XfyunStreamingSpeechSessionTests : public QObject
{
    Q_OBJECT

private slots:
    void sendsDynamicCorrectionFramesAndCompletesWithFinalText()
    {
        QList<QSharedPointer<FakeXfyunStreamingTransport>> transports;
        const auto factory = [&transports]() {
            QSharedPointer<FakeXfyunStreamingTransport> transport(
                new FakeXfyunStreamingTransport
            );
            transports.append(transport);
            return transport.staticCast<IProviderStreamingWebSocketTransport>();
        };
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
        request.provider = QStringLiteral("xfyun");
        request.networkPolicy = QStringLiteral("direct");
        XfyunStreamingSpeechSession session(
            factory,
            testSecrets,
            fixedUtcNow,
            request,
            callbacks,
            fastTiming()
        );

        QString error;
        QVERIFY2(session.start(&error), qPrintable(error));
        QCOMPARE(transports.size(), 1);
        transports.first()->emitOpened();
        QVERIFY(session.pushAudio(QByteArray(2560, char(7))));
        QTRY_COMPARE(transports.first()->textFrames.size(), 2);

        const QJsonObject first = QJsonDocument::fromJson(
            transports.first()->textFrames.at(0)
        ).object();
        QCOMPARE(frameStatus(transports.first()->textFrames.at(0)), 0);
        QCOMPARE(frameAudioSize(transports.first()->textFrames.at(0)), 1280);
        QCOMPARE(frameStatus(transports.first()->textFrames.at(1)), 1);
        QCOMPARE(
            first.value(QStringLiteral("business")).toObject()
                .value(QStringLiteral("dwa")).toString(),
            QStringLiteral("wpgs")
        );
        QCOMPARE(
            QUrlQuery(transports.first()->url)
                .queryItemValue(QStringLiteral("host")),
            QStringLiteral("iat-api.xfyun.cn")
        );

        transports.first()->emitText(xfyunMessage(
            1,
            1,
            QString::fromUtf8("你好")
        ));
        QTRY_VERIFY(!snapshots.isEmpty());
        QCOMPARE(snapshots.last().displayText(), QString::fromUtf8("你好"));

        session.finish();
        QTRY_COMPARE(transports.first()->textFrames.size(), 3);
        QCOMPARE(frameStatus(transports.first()->textFrames.last()), 2);
        transports.first()->emitText(xfyunMessage(2, 2, QString()));

        QTRY_COMPARE(completedText, QString::fromUtf8("你好"));
        QCOMPARE(degradedCount, 0);
        QCOMPARE(transports.first()->normalCloseCount, 1);
        QCOMPARE(session.state(), StreamingSpeechState::Completed);
    }

    void appliesReplacementRangeInPlace()
    {
        QSharedPointer<FakeXfyunStreamingTransport> fake(
            new FakeXfyunStreamingTransport
        );
        const auto factory = [fake]() {
            return fake.staticCast<IProviderStreamingWebSocketTransport>();
        };
        StreamingTranscriptSnapshot latest;
        StreamingSpeechCallbacks callbacks;
        callbacks.transcriptUpdated = [&](const StreamingTranscriptSnapshot &snapshot) {
            latest = snapshot;
        };
        XfyunStreamingSpeechSession session(
            factory,
            testSecrets,
            fixedUtcNow,
            StreamingSpeechSessionRequest(),
            callbacks,
            fastTiming()
        );
        QString error;
        QVERIFY(session.start(&error));
        fake->emitOpened();

        fake->emitText(xfyunMessage(1, 1, QString::fromUtf8("今天")));
        fake->emitText(xfyunMessage(1, 2, QString::fromUtf8("天气")));
        fake->emitText(xfyunMessage(
            1,
            3,
            QString::fromUtf8("今天天气很好"),
            QStringLiteral("rpl"),
            1,
            2
        ));

        QCOMPARE(
            latest.displayText(),
            QString::fromUtf8("今天天气很好")
        );
    }

    void degradesOnceWhenAudioQueueOverflows()
    {
        QSharedPointer<FakeXfyunStreamingTransport> fake(
            new FakeXfyunStreamingTransport
        );
        XfyunStreamingSpeechSession::Timing timing = fastTiming();
        timing.queueLimitBytes = 1280;
        int degradedCount = 0;
        StreamingSpeechCallbacks callbacks;
        callbacks.degraded = [&](const QString &) { ++degradedCount; };
        XfyunStreamingSpeechSession session(
            [fake]() { return fake.staticCast<IProviderStreamingWebSocketTransport>(); },
            testSecrets,
            fixedUtcNow,
            StreamingSpeechSessionRequest(),
            callbacks,
            timing
        );
        QString error;
        QVERIFY(session.start(&error));

        QVERIFY(!session.pushAudio(QByteArray(1281, char(1))));
        QCOMPARE(degradedCount, 1);
        QVERIFY(!session.pushAudio(QByteArray(1, char(2))));
        QCOMPARE(degradedCount, 1);
        QCOMPARE(session.state(), StreamingSpeechState::Degraded);
    }

    void rotatesConnectionAndSealsPreviousText()
    {
        QList<QSharedPointer<FakeXfyunStreamingTransport>> transports;
        XfyunStreamingSpeechSession::Timing timing = fastTiming();
        timing.rotationIntervalMs = 15;
        StreamingTranscriptSnapshot latest;
        StreamingSpeechCallbacks callbacks;
        callbacks.transcriptUpdated = [&](const StreamingTranscriptSnapshot &snapshot) {
            latest = snapshot;
        };
        XfyunStreamingSpeechSession session(
            [&transports]() {
                QSharedPointer<FakeXfyunStreamingTransport> transport(
                    new FakeXfyunStreamingTransport
                );
                transports.append(transport);
                return transport.staticCast<IProviderStreamingWebSocketTransport>();
            },
            testSecrets,
            fixedUtcNow,
            StreamingSpeechSessionRequest(),
            callbacks,
            timing
        );
        QString error;
        QVERIFY(session.start(&error));
        transports.at(0)->emitOpened();
        transports.at(0)->emitText(xfyunMessage(
            1,
            1,
            QString::fromUtf8("第一段。")
        ));

        QTRY_VERIFY(!transports.at(0)->textFrames.isEmpty());
        QCOMPARE(frameStatus(transports.at(0)->textFrames.last()), 2);
        transports.at(0)->emitText(xfyunMessage(2, 2, QString()));
        QTRY_COMPARE(transports.size(), 2);
        transports.at(1)->emitOpened();
        transports.at(1)->emitText(xfyunMessage(
            1,
            1,
            QString::fromUtf8("第二段。")
        ));

        QCOMPARE(
            latest.displayText(),
            QString::fromUtf8("第一段。第二段。")
        );
        QCOMPARE(transports.at(0)->normalCloseCount, 1);
    }

    void cancelStopsTransportWithoutCompletion()
    {
        QSharedPointer<FakeXfyunStreamingTransport> fake(
            new FakeXfyunStreamingTransport
        );
        int completedCount = 0;
        StreamingSpeechCallbacks callbacks;
        callbacks.completed = [&](const QString &) { ++completedCount; };
        XfyunStreamingSpeechSession session(
            [fake]() { return fake.staticCast<IProviderStreamingWebSocketTransport>(); },
            testSecrets,
            fixedUtcNow,
            StreamingSpeechSessionRequest(),
            callbacks,
            fastTiming()
        );
        QString error;
        QVERIFY(session.start(&error));
        fake->emitOpened();

        session.cancel();

        QCOMPARE(fake->cancelCount, 1);
        QCOMPARE(completedCount, 0);
        QCOMPARE(session.state(), StreamingSpeechState::Cancelled);
    }
};

QTEST_MAIN(XfyunStreamingSpeechSessionTests)

#include "xfyun_streaming_speech_session_tests.moc"
