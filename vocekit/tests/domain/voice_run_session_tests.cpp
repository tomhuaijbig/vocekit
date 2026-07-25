#include <QtTest>

#include "../../src/domain/voice_run_session.h"

class VoiceRunSessionTests : public QObject
{
    Q_OBJECT

private slots:
    void beginActionClearsPreviousRunState()
    {
        VoiceRunSession session;
        VoiceRunContext context;
        context.modeId = QStringLiteral("ask");
        context.selectedText = QStringLiteral("old input");
        RecordingSegment segment;
        segment.index = 1;
        segment.wavPath = QStringLiteral("segment.wav");

        session.setActionHadRecording(true);
        session.setSpeechElapsedMs(120);
        session.setModelResult(240, QStringLiteral("prompt-v1"));
        session.setRecordingAudioPath(QStringLiteral("recording.wav"));
        session.setRecordingSegments(QVector<RecordingSegment>() << segment);
        session.setRecordingTriggerMode(QStringLiteral("hold"));
        session.setLongRecording(true);
        session.setRunContext(context);

        session.beginAction();

        const VoiceRunSessionSnapshot snapshot = session.snapshot();
        QVERIFY(snapshot.elapsedMs >= 0);
        QCOMPARE(snapshot.actionHadRecording, false);
        QCOMPARE(snapshot.speechElapsedMs, qint64(-1));
        QCOMPARE(snapshot.modelElapsedMs, qint64(-1));
        QVERIFY(snapshot.promptVersion.isEmpty());
        QVERIFY(snapshot.recordingAudioPath.isEmpty());
        QVERIFY(snapshot.recordingSegments.isEmpty());
        QVERIFY(snapshot.recordingTriggerMode.isEmpty());
        QCOMPARE(snapshot.longRecording, false);
        QVERIFY(snapshot.runContext.modeId.isEmpty());
    }

    void modelAttemptResetPreservesCollectedInputState()
    {
        VoiceRunSession session;
        session.beginAction();
        VoiceRunContext context;
        context.modeId = QStringLiteral("translate");
        context.selectedText = QStringLiteral("hello");
        session.setRunContext(context);
        session.setActionHadRecording(true);
        session.setSpeechElapsedMs(80);
        session.setRecordingAudioPath(QStringLiteral("recording.wav"));
        session.setModelResult(160, QStringLiteral("prompt-v2"));

        session.beginModelAttempt();

        const VoiceRunSessionSnapshot snapshot = session.snapshot();
        QVERIFY(snapshot.elapsedMs >= 0);
        QCOMPARE(snapshot.actionHadRecording, true);
        QCOMPARE(snapshot.speechElapsedMs, qint64(80));
        QCOMPARE(snapshot.recordingAudioPath, QStringLiteral("recording.wav"));
        QCOMPARE(snapshot.runContext.modeId, QStringLiteral("translate"));
        QCOMPARE(snapshot.modelElapsedMs, qint64(-1));
        QVERIFY(snapshot.promptVersion.isEmpty());
    }

    void speechElapsedAccumulatesAcrossLongRecordingSegments()
    {
        VoiceRunSession session;
        session.beginAction();

        session.addSpeechElapsedMs(35);
        session.addSpeechElapsedMs(45);
        session.addSpeechElapsedMs(-1);

        QCOMPARE(session.snapshot().speechElapsedMs, qint64(80));
    }

    void sourceAudioPathRequiresRecordingAndUsesFallback()
    {
        VoiceRunSession session;
        session.beginAction();
        session.setRecordingAudioPath(QStringLiteral("preferred.wav"));
        QVERIFY(session.snapshot().sourceAudioPath(
            QStringLiteral("fallback.wav")
        ).isEmpty());

        session.setActionHadRecording(true);
        QCOMPARE(
            session.snapshot().sourceAudioPath(
                QStringLiteral("fallback.wav")
            ),
            QStringLiteral("preferred.wav")
        );

        session.setRecordingAudioPath(QString());
        QCOMPARE(
            session.snapshot().sourceAudioPath(
                QStringLiteral("fallback.wav")
            ),
            QStringLiteral("fallback.wav")
        );
    }
};

QTEST_APPLESS_MAIN(VoiceRunSessionTests)
#include "voice_run_session_tests.moc"
