#include <QtTest>
#include <QDataStream>

#include "../../src/input/hold_to_talk.h"
#include "../../src/recording/segmented_recording.h"

class RecordingCoreTests : public QObject
{
    Q_OBJECT

private slots:
    void usesSafeDefaults();
    void normalizesPersistedValues();
    void segmentStartsEmpty();
    void holdShortcutEmitsOnePressAndRelease();
    void holdShortcutIgnoresRepeatAndUnrelatedKeys();
    void mergesSegmentsByIndex();
    void retriesFailedSegmentOnlyOnce();
    void countsSuccessfulSegments();
    void enforcesMaximumSegmentCount();
    void extractsPcmFromStandardWav();
    void rejectsInvalidWav();
};

void RecordingCoreTests::usesSafeDefaults()
{
    const RecordingFunctionSettings settings =
        recordingFunctionSettingsFromJson(QJsonObject());

    QCOMPARE(settings.triggerMode, QStringLiteral("toggle"));
    QVERIFY(!settings.longRecordingEnabled);
    QCOMPARE(settings.segmentSeconds, 55);
    QCOMPARE(settings.maxRecordingMinutes, 30);
}

void RecordingCoreTests::normalizesPersistedValues()
{
    QJsonObject object;
    object.insert(QStringLiteral("recordingTriggerMode"), QStringLiteral("invalid"));
    object.insert(QStringLiteral("longRecordingEnabled"), true);
    object.insert(QStringLiteral("segmentSeconds"), 5);
    object.insert(QStringLiteral("maxRecordingMinutes"), 99);

    const RecordingFunctionSettings settings =
        recordingFunctionSettingsFromJson(object);

    QCOMPARE(settings.triggerMode, QStringLiteral("toggle"));
    QVERIFY(settings.longRecordingEnabled);
    QCOMPARE(settings.segmentSeconds, 20);
    QCOMPARE(settings.maxRecordingMinutes, 30);
}

void RecordingCoreTests::segmentStartsEmpty()
{
    const RecordingSegment segment;

    QCOMPARE(segment.index, 0);
    QVERIFY(segment.wavPath.isEmpty());
    QVERIFY(segment.text.isEmpty());
    QVERIFY(segment.error.isEmpty());
    QCOMPARE(segment.recognitionElapsedMs, qint64(-1));
    QCOMPARE(segment.attempts, 0);
}

void RecordingCoreTests::holdShortcutEmitsOnePressAndRelease()
{
    HoldShortcutMatcher matcher;
    QVERIFY(matcher.configure(QStringLiteral("Ctrl+Alt+X")));

    QCOMPARE(
        matcher.process('X', HoldModifierControl | HoldModifierAlt, true, false),
        HoldShortcutTransition::Pressed
    );
    QCOMPARE(
        matcher.process('X', HoldModifierControl | HoldModifierAlt, false, false),
        HoldShortcutTransition::Released
    );
}

void RecordingCoreTests::holdShortcutIgnoresRepeatAndUnrelatedKeys()
{
    HoldShortcutMatcher matcher;
    QVERIFY(matcher.configure(QStringLiteral("Ctrl+Alt+X")));

    QCOMPARE(
        matcher.process('A', HoldModifierControl | HoldModifierAlt, true, false),
        HoldShortcutTransition::None
    );
    QCOMPARE(
        matcher.process('X', HoldModifierControl | HoldModifierAlt, true, false),
        HoldShortcutTransition::Pressed
    );
    QCOMPARE(
        matcher.process('X', HoldModifierControl | HoldModifierAlt, true, true),
        HoldShortcutTransition::None
    );
    QCOMPARE(
        matcher.process('X', 0, false, false),
        HoldShortcutTransition::Released
    );
}

void RecordingCoreTests::mergesSegmentsByIndex()
{
    SegmentedRecordingState state;
    state.addSegment(2, QStringLiteral("2.wav"));
    state.addSegment(1, QStringLiteral("1.wav"));
    state.addSegment(3, QStringLiteral("3.wav"));

    state.recordResult(2, QStringLiteral("第二段"), QString(), 20);
    state.recordResult(1, QStringLiteral("第一段"), QString(), 10);
    state.recordResult(3, QStringLiteral("第三段"), QString(), 30);

    QCOMPARE(
        state.mergedText(),
        QStringLiteral("第一段\n第二段\n第三段")
    );
}

void RecordingCoreTests::retriesFailedSegmentOnlyOnce()
{
    SegmentedRecordingState state;
    state.addSegment(2, QStringLiteral("2.wav"));

    QCOMPARE(state.nextPendingIndex(), 2);
    QVERIFY(state.markAttemptStarted(2));
    state.recordResult(2, QString(), QStringLiteral("网络失败"), 100);
    QCOMPARE(state.nextPendingIndex(), 2);

    QVERIFY(state.markAttemptStarted(2));
    state.recordResult(2, QString(), QStringLiteral("仍然失败"), 120);
    QCOMPARE(state.nextPendingIndex(), -1);
    QCOMPARE(state.segment(2).attempts, 2);
    QCOMPARE(state.mergedText(), QStringLiteral("[第 2 段识别失败]"));
}

void RecordingCoreTests::countsSuccessfulSegments()
{
    SegmentedRecordingState state;
    state.addSegment(1, QStringLiteral("1.wav"));
    state.addSegment(2, QStringLiteral("2.wav"));
    state.addSegment(3, QStringLiteral("3.wav"));

    state.recordResult(1, QStringLiteral("第一段"), QString(), 20);
    state.recordResult(2, QString(), QStringLiteral("识别失败"), 30);
    state.recordResult(3, QStringLiteral("第三段"), QString(), 40);

    QCOMPARE(state.successfulSegmentCount(), 2);
    QCOMPARE(state.failedSegmentCount(), 1);
}

void RecordingCoreTests::enforcesMaximumSegmentCount()
{
    QVERIFY(canStartRecordingSegment(1));
    QVERIFY(canStartRecordingSegment(33));
    QVERIFY(!canStartRecordingSegment(34));
}

void RecordingCoreTests::extractsPcmFromStandardWav()
{
    const QByteArray pcm = QByteArray::fromHex("01000200ff7f0080");
    QByteArray wav;
    QDataStream stream(&wav, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + pcm.size());
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(16) << quint16(1) << quint16(1);
    stream << quint32(16000) << quint32(32000);
    stream << quint16(2) << quint16(16);
    stream.writeRawData("data", 4);
    stream << quint32(pcm.size());
    stream.writeRawData(pcm.constData(), pcm.size());

    QString error;
    QCOMPARE(pcm16FromWavData(wav, &error), pcm);
    QVERIFY(error.isEmpty());
}

void RecordingCoreTests::rejectsInvalidWav()
{
    QString error;
    QVERIFY(pcm16FromWavData(QByteArray("not a wav"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_APPLESS_MAIN(RecordingCoreTests)

#include "recording_core_tests.moc"
