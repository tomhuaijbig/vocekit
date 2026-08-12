#include <QtTest>

#include "../../src/recording/voice_long_recording_session.h"
#include "../../src/recording/voice_recording_capture.h"
#include "../../src/recording/voice_recording_coordinator.h"
#include "../../src/recording/voice_recording_countdown.h"
#include "../../src/recording/voice_recording_lifecycle.h"
#include "../../src/tasks/voice_long_recording_result_builder.h"
#include "../../src/tasks/voice_long_recording_segment_executor.h"
#include "../../src/tasks/voice_long_recording_completion_executor.h"
#include "../../src/tasks/voice_long_recording_recognition_coordinator.h"
#include "../../src/tasks/voice_recording_completion_executor.h"
#include "../../src/tasks/voice_speech_recognition_executor.h"
#include "../../src/providers/provider_types.h"

class VoiceSpeechRecognitionExecutorTests : public QObject
{
    Q_OBJECT

private slots:
    void coordinatesLongRecordingSegmentsUntilFinalized()
    {
        VoiceLongRecordingSession session;
        session.begin(
            true,
            QStringLiteral("records/audio"),
            QStringLiteral("session")
        );
        session.addCurrentSegment(
            QStringLiteral("one.wav"),
            QByteArray("one-pcm")
        );
        QVERIFY(session.advanceToNextSegment());
        session.addCurrentSegment(
            QStringLiteral("two.wav"),
            QByteArray("two-pcm")
        );
        QVERIFY(session.beginFinalizing());

        QVector<int> startedIndexes;
        QVector<int> finishedIndexes;
        bool allFinished = false;
        VoiceLongRecordingRecognitionCoordinator coordinator;
        VoiceLongRecordingRecognitionCallbacks callbacks;
        callbacks.segmentStarted = [&startedIndexes](
            int index,
            int attempt,
            const QString &provider
        ) {
            Q_UNUSED(attempt);
            Q_UNUSED(provider);
            startedIndexes.append(index);
        };
        callbacks.segmentFinished = [&finishedIndexes](
            const VoiceLongRecordingSegmentResult &result
        ) {
            finishedIndexes.append(result.index);
        };
        callbacks.allFinished = [&allFinished]() {
            allFinished = true;
        };
        coordinator.setCallbacks(callbacks);
        VoiceSpeechRecognitionHandlers recognition;
        recognition.recognizeProvider = [](
            const SpeechRecognitionProviderTaskRequest &request
        ) {
            SpeechRecognitionTaskResult result;
            result.text = QStringLiteral("segment-%1").arg(request.index);
            result.elapsedMs = 5;
            return result;
        };
        VoiceLongRecordingRecognitionConfig config;
        config.modeId = QStringLiteral("dictate");
        config.provider = QStringLiteral("xfyun");
        config.language = QStringLiteral("en-US");
        coordinator.reset();
        coordinator.schedule(session, config, recognition);

        QTRY_VERIFY(allFinished);
        QCOMPARE(startedIndexes, QVector<int>({1, 2}));
        QCOMPARE(finishedIndexes, QVector<int>({1, 2}));
        QCOMPARE(
            session.recognitionState().segment(1).text,
            QStringLiteral("segment-1")
        );
        QCOMPARE(
            session.recognitionState().segment(2).text,
            QStringLiteral("segment-2")
        );
        QVERIFY(!coordinator.isRunning());
    }

    void propagatesLongRecordingLanguage()
    {
        VoiceLongRecordingSession session;
        session.begin(true, QString(), QStringLiteral("session"));
        session.addCurrentSegment(QStringLiteral("one.wav"), QByteArray("pcm"));
        QVERIFY(session.beginFinalizing());

        QString capturedLanguage;
        bool allFinished = false;
        VoiceSpeechRecognitionHandlers recognition;
        recognition.recognizeProvider = [&capturedLanguage](
            const SpeechRecognitionProviderTaskRequest &request
        ) {
            capturedLanguage = request.language;
            SpeechRecognitionTaskResult result;
            result.text = QStringLiteral("recognized");
            return result;
        };
        VoiceLongRecordingRecognitionCoordinator coordinator;
        VoiceLongRecordingRecognitionCallbacks callbacks;
        callbacks.allFinished = [&allFinished]() { allFinished = true; };
        coordinator.setCallbacks(callbacks);
        VoiceLongRecordingRecognitionConfig config;
        config.provider = QStringLiteral("windows-local");
        config.language = QStringLiteral("en-US");
        coordinator.schedule(session, config, recognition);

        QTRY_VERIFY(allFinished);
        QCOMPARE(capturedLanguage, QStringLiteral("en-US"));
    }

    void completesLongRecordingAndSavesMergedAudio()
    {
        SegmentedRecordingState state;
        state.addSegment(1, QStringLiteral("one.wav"));
        QVERIFY(state.markAttemptStarted(1));
        state.recordResult(1, QStringLiteral("第一段"), QString(), 10);
        state.addSegment(2, QStringLiteral("two.wav"));
        QVERIFY(state.markAttemptStarted(2));
        state.recordResult(2, QStringLiteral("第二段"), QString(), 20);
        QMap<int, QByteArray> pcm;
        pcm.insert(1, QByteArray("aa"));
        pcm.insert(2, QByteArray("bb"));

        bool saveCalled = false;
        VoiceLongRecordingCompletionHandlers handlers;
        handlers.saveCompleteAudio = [&saveCalled](
            const QByteArray &completePcm,
            const QString &audioDirectory,
            const QString &fileBase,
            QString *error
        ) {
            Q_UNUSED(error);
            saveCalled = completePcm == QByteArray("aabb")
                && audioDirectory == QStringLiteral("records/audio")
                && fileBase == QStringLiteral("session");
            return QStringLiteral("records/audio/session.wav");
        };
        VoiceLongRecordingCompletionRequest request;
        request.state = &state;
        request.segmentPcm = pcm;
        request.audioDirectory = QStringLiteral("records/audio");
        request.fileBase = QStringLiteral("session");
        const VoiceLongRecordingCompletionResult result =
            VoiceLongRecordingCompletionExecutor::run(request, handlers);

        QVERIFY(result.ok);
        QVERIFY(saveCalled);
        QCOMPARE(result.audioPath, QStringLiteral("records/audio/session.wav"));
        QCOMPARE(result.build.mergedText, QStringLiteral("第一段\n第二段"));
        QCOMPARE(result.build.successfulSegmentCount, 2);
    }

    void reportsLongRecordingWithoutSuccessfulSegments()
    {
        SegmentedRecordingState state;
        state.addSegment(1, QStringLiteral("one.wav"));
        QVERIFY(state.markAttemptStarted(1));
        QVERIFY(state.markAttemptStarted(1));
        state.recordResult(1, QString(), QStringLiteral("network error"), 11);
        VoiceLongRecordingCompletionRequest request;
        request.state = &state;
        const VoiceLongRecordingCompletionResult result =
            VoiceLongRecordingCompletionExecutor::run(
                request,
                VoiceLongRecordingCompletionHandlers()
            );

        QVERIFY(!result.ok);
        QVERIFY(!result.error.isEmpty());
        QCOMPARE(result.build.failedSegmentCount, 1);
    }

    void stopsRecordingAndRecognizesCapturedAudio()
    {
        bool stopped = false;
        bool requestMatched = false;
        VoiceRecordingCompletionHandlers handlers;
        handlers.stopRecording = [&stopped]() {
            stopped = true;
            VoiceRecordingStopResult result;
            result.pcm = QByteArray("captured-pcm");
            result.wavPath = QStringLiteral("records/dictate.wav");
            return result;
        };
        handlers.recognition.recognizeProvider = [&requestMatched](
            const SpeechRecognitionProviderTaskRequest &request
        ) {
            requestMatched = request.audioData == QByteArray("captured-pcm")
                && request.provider == QStringLiteral("xfyun")
                && request.networkPolicy == QStringLiteral("direct")
                && !request.useSystemProxy;
            SpeechRecognitionTaskResult result;
            result.text = QStringLiteral("识别结果");
            result.elapsedMs = 42;
            return result;
        };

        VoiceRecordingCompletionRequest request;
        request.modeId = QStringLiteral("dictate");
        request.provider = QStringLiteral("xfyun");
        request.networkPolicy = QStringLiteral("direct");
        const VoiceRecordingCompletionResult result =
            VoiceRecordingCompletionExecutor::run(request, handlers);

        QVERIFY(stopped);
        QVERIFY(requestMatched);
        QVERIFY(result.ok);
        QCOMPARE(result.recording.pcm, QByteArray("captured-pcm"));
        QCOMPARE(result.recording.wavPath, QStringLiteral("records/dictate.wav"));
        QCOMPARE(result.speech.text, QStringLiteral("识别结果"));
        QCOMPARE(result.speech.elapsedMs, qint64(42));
    }

    void reportsMissingRecordingStopHandler()
    {
        VoiceRecordingCompletionRequest request;
        request.modeId = QStringLiteral("translate");
        const VoiceRecordingCompletionResult result =
            VoiceRecordingCompletionExecutor::run(
                request,
                VoiceRecordingCompletionHandlers()
            );

        QVERIFY(!result.ok);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.recording.pcm.isEmpty());
    }

    void coordinatesNormalRecordingStartAndStop()
    {
        bool captureStarted = false;
        bool captureStopped = false;
        VoiceRecordingCaptureHandlers handlers;
        handlers.start = [&captureStarted](
            const QString &title,
            const QString &directory,
            bool directDirectory,
            QString *error
        ) {
            Q_UNUSED(error);
            captureStarted = title == QStringLiteral("听写")
                && directory == QStringLiteral("records")
                && !directDirectory;
            return captureStarted;
        };
        handlers.stop = [&captureStopped]() {
            captureStopped = true;
            return QByteArray("pcm");
        };
        handlers.lastWavPath = []() {
            return QStringLiteral("records/test.wav");
        };
        VoiceRecordingCapture capture(handlers);
        VoiceRecordingLifecycle lifecycle;
        VoiceRecordingCoordinator coordinator(capture, lifecycle);
        QString startedMode;
        VoiceRecordingCoordinatorCallbacks callbacks;
        callbacks.started = [&startedMode](
            const QString &modeId,
            bool longRecording
        ) {
            QVERIFY(!longRecording);
            startedMode = modeId;
        };
        coordinator.setCallbacks(callbacks);

        VoiceRecordingCoordinatorRequest request;
        request.modeId = QStringLiteral("dictate");
        request.captureRequest.normalTitle = QStringLiteral("听写");
        request.captureRequest.normalDirectory = QStringLiteral("records");
        coordinator.begin(request);

        QCOMPARE(startedMode, request.modeId);
        QVERIFY(captureStarted);
        QVERIFY(coordinator.isRecording());
        const VoiceRecordingStopResult result = coordinator.stopNormal();
        QVERIFY(captureStopped);
        QVERIFY(!coordinator.isRecording());
        QCOMPARE(result.pcm, QByteArray("pcm"));
        QCOMPARE(result.wavPath, QStringLiteral("records/test.wav"));
    }

    void reportsRecordingStartFailureWithoutStartingLifecycle()
    {
        VoiceRecordingCaptureHandlers handlers;
        handlers.start = [](
            const QString &title,
            const QString &directory,
            bool directDirectory,
            QString *error
        ) {
            Q_UNUSED(title);
            Q_UNUSED(directory);
            Q_UNUSED(directDirectory);
            if (error) {
                *error = QStringLiteral("microphone unavailable");
            }
            return false;
        };
        VoiceRecordingCapture capture(handlers);
        VoiceRecordingLifecycle lifecycle;
        VoiceRecordingCoordinator coordinator(capture, lifecycle);
        QString failedMode;
        QString failure;
        VoiceRecordingCoordinatorCallbacks callbacks;
        callbacks.startFailed = [&failedMode, &failure](
            const QString &modeId,
            const QString &error
        ) {
            failedMode = modeId;
            failure = error;
        };
        coordinator.setCallbacks(callbacks);

        VoiceRecordingCoordinatorRequest request;
        request.modeId = QStringLiteral("ask");
        coordinator.begin(request);

        QCOMPARE(failedMode, request.modeId);
        QCOMPARE(failure, QStringLiteral("microphone unavailable"));
        QVERIFY(!coordinator.isRecording());
    }

    void runsRecordingCountdownBeepAndStart()
    {
        QVector<int> ticks;
        QString beepMode;
        QString startedMode;
        VoiceRecordingCountdown countdown;
        VoiceRecordingCountdownCallbacks callbacks;
        callbacks.tick = [&ticks](const QString &modeId, int seconds) {
            Q_UNUSED(modeId);
            ticks.append(seconds);
        };
        callbacks.beepRequested = [&beepMode](const QString &modeId) {
            beepMode = modeId;
        };
        callbacks.startRequested = [&startedMode](const QString &modeId) {
            startedMode = modeId;
        };
        countdown.setCallbacks(callbacks);

        VoiceRecordingCountdownRequest request;
        request.modeId = QStringLiteral("dictate");
        request.seconds = 2;
        request.playBeep = true;
        request.tickIntervalMs = 5;
        request.beepDelayMs = 5;
        countdown.begin(request);

        QVERIFY(countdown.isActive());
        QVERIFY(countdown.matchesMode(request.modeId));
        QTRY_COMPARE(startedMode, request.modeId);
        QCOMPARE(ticks, QVector<int>({2, 1}));
        QCOMPARE(beepMode, request.modeId);
        QVERIFY(!countdown.isActive());
    }

    void cancelsRecordingCountdownBeforeStart()
    {
        int startCount = 0;
        VoiceRecordingCountdown countdown;
        VoiceRecordingCountdownCallbacks callbacks;
        callbacks.startRequested = [&startCount](const QString &modeId) {
            Q_UNUSED(modeId);
            ++startCount;
        };
        countdown.setCallbacks(callbacks);

        VoiceRecordingCountdownRequest request;
        request.modeId = QStringLiteral("ask");
        request.seconds = 3;
        request.tickIntervalMs = 5;
        countdown.begin(request);
        QVERIFY(countdown.cancel());
        QTest::qWait(30);

        QCOMPARE(startCount, 0);
        QVERIFY(!countdown.isActive());
        QVERIFY(!countdown.matchesMode(request.modeId));
    }

    void startsRecordingImmediatelyWithoutPreparation()
    {
        QString startedMode;
        VoiceRecordingCountdown countdown;
        VoiceRecordingCountdownCallbacks callbacks;
        callbacks.startRequested = [&startedMode](const QString &modeId) {
            startedMode = modeId;
        };
        countdown.setCallbacks(callbacks);

        VoiceRecordingCountdownRequest request;
        request.modeId = QStringLiteral("translate");
        countdown.begin(request);

        QCOMPARE(startedMode, request.modeId);
        QVERIFY(!countdown.isActive());
    }

    void managesNormalRecordingLifecycleTimers()
    {
        int waveformTicks = 0;
        int segmentTicks = 0;
        int limitTicks = 0;
        VoiceRecordingLifecycle lifecycle;
        VoiceRecordingLifecycleCallbacks callbacks;
        callbacks.waveformTick = [&waveformTicks]() { ++waveformTicks; };
        callbacks.segmentElapsed = [&segmentTicks]() { ++segmentTicks; };
        callbacks.limitElapsed = [&limitTicks]() { ++limitTicks; };
        lifecycle.setCallbacks(callbacks);

        lifecycle.start(false, 5, 10, 15);
        QVERIFY(lifecycle.isRecording());
        QTest::qWait(40);
        QVERIFY(waveformTicks > 0);
        QCOMPARE(segmentTicks, 0);
        QCOMPARE(limitTicks, 0);

        lifecycle.stop();
        QVERIFY(!lifecycle.isRecording());
        const int stoppedWaveformTicks = waveformTicks;
        QTest::qWait(20);
        QCOMPARE(waveformTicks, stoppedWaveformTicks);
    }

    void managesLongRecordingLifecycleTimers()
    {
        int segmentTicks = 0;
        int limitTicks = 0;
        VoiceRecordingLifecycle lifecycle;
        VoiceRecordingLifecycleCallbacks callbacks;
        callbacks.segmentElapsed = [&segmentTicks]() { ++segmentTicks; };
        callbacks.limitElapsed = [&limitTicks]() { ++limitTicks; };
        lifecycle.setCallbacks(callbacks);

        lifecycle.start(true, 0, 10, 25);
        QVERIFY(lifecycle.isRecording());
        QVERIFY(lifecycle.isLongRecording());
        QTest::qWait(50);
        QCOMPARE(segmentTicks, 1);
        QCOMPARE(limitTicks, 1);

        lifecycle.restartSegment(10);
        QTest::qWait(25);
        QCOMPARE(segmentTicks, 2);
        lifecycle.stop();
    }

    void startsAndCapturesLongRecordingThroughAdapter()
    {
        QString startedTitle;
        QString startedDirectory;
        bool usedDirectDirectory = false;

        VoiceRecordingCaptureHandlers handlers;
        handlers.start = [&startedTitle, &startedDirectory, &usedDirectDirectory](
            const QString &title,
            const QString &directory,
            bool directDirectory,
            QString *error
        ) {
            Q_UNUSED(error);
            startedTitle = title;
            startedDirectory = directory;
            usedDirectDirectory = directDirectory;
            return true;
        };
        handlers.stop = []() { return QByteArray("segment-pcm"); };
        handlers.lastWavPath = []() { return QStringLiteral("one.wav"); };

        VoiceRecordingCapture capture(handlers);
        VoiceRecordingStartRequest request;
        request.normalTitle = QStringLiteral("dictate");
        request.normalDirectory = QStringLiteral("records");
        request.longRecordingEnabled = true;
        request.firstSegmentTitle = QStringLiteral("dictate-segment-1");
        request.longRecordingDirectory = QStringLiteral("records/audio");
        request.longRecordingFileBase = QStringLiteral("recording-001");

        QString error;
        QVERIFY(capture.begin(request, &error));
        QCOMPARE(startedTitle, request.firstSegmentTitle);
        QCOMPARE(startedDirectory, request.longRecordingDirectory);
        QVERIFY(usedDirectDirectory);

        const VoiceRecordingSegmentCapture segment =
            capture.captureCurrentLongSegment(QStringLiteral("empty"));
        QVERIFY(segment.valid);
        QCOMPARE(segment.index, 1);
        QCOMPARE(segment.pcm, QByteArray("segment-pcm"));
        QCOMPARE(segment.wavPath, QStringLiteral("one.wav"));
        QCOMPARE(
            capture.longRecordingSession().recognitionState().segments().size(),
            1
        );
    }

    void recordsNextLongSegmentStartFailureThroughAdapter()
    {
        int startCalls = 0;
        VoiceRecordingCaptureHandlers handlers;
        handlers.start = [&startCalls](
            const QString &title,
            const QString &directory,
            bool directDirectory,
            QString *error
        ) {
            Q_UNUSED(title);
            Q_UNUSED(directory);
            Q_UNUSED(directDirectory);
            ++startCalls;
            if (startCalls == 1) {
                return true;
            }
            if (error) {
                *error = QStringLiteral("device busy");
            }
            return false;
        };

        VoiceRecordingCapture capture(handlers);
        VoiceRecordingStartRequest request;
        request.longRecordingEnabled = true;
        request.firstSegmentTitle = QStringLiteral("segment-1");
        request.longRecordingDirectory = QStringLiteral("audio");
        QVERIFY(capture.begin(request, nullptr));

        const VoiceRecordingNextSegmentResult result =
            capture.startNextLongSegment(
                QStringLiteral("segment-2"),
                QStringLiteral("cannot start: ")
            );

        QCOMPARE(
            result.status,
            VoiceRecordingNextSegmentStatus::StartFailed
        );
        QCOMPARE(result.index, 2);
        QCOMPARE(startCalls, 2);
        const RecordingSegment segment =
            capture.longRecordingSession().recognitionState().segment(2);
        QCOMPARE(segment.attempts, 2);
        QCOMPARE(segment.error, QStringLiteral("cannot start: device busy"));
    }

    void stopsNormalRecordingThroughAdapter()
    {
        QString startedDirectory;
        bool usedDirectDirectory = true;
        VoiceRecordingCaptureHandlers handlers;
        handlers.start = [&startedDirectory, &usedDirectDirectory](
            const QString &title,
            const QString &directory,
            bool directDirectory,
            QString *error
        ) {
            Q_UNUSED(title);
            Q_UNUSED(error);
            startedDirectory = directory;
            usedDirectDirectory = directDirectory;
            return true;
        };
        handlers.stop = []() { return QByteArray("normal-pcm"); };
        handlers.lastWavPath = []() { return QStringLiteral("normal.wav"); };
        handlers.takePeakLevel = []() { return 37; };

        VoiceRecordingCapture capture(handlers);
        VoiceRecordingStartRequest request;
        request.normalTitle = QStringLiteral("dictate");
        request.normalDirectory = QStringLiteral("records");

        QVERIFY(capture.begin(request, nullptr));
        QCOMPARE(startedDirectory, request.normalDirectory);
        QVERIFY(!usedDirectDirectory);
        QCOMPARE(capture.takePeakLevel(), 37);

        const VoiceRecordingStopResult result = capture.stopNormal();
        QCOMPARE(result.pcm, QByteArray("normal-pcm"));
        QCOMPARE(result.wavPath, QStringLiteral("normal.wav"));
    }

    void managesLongRecordingSessionLifecycle()
    {
        VoiceLongRecordingSession session;
        session.begin(
            true,
            QStringLiteral("records/audio"),
            QStringLiteral("recording-001")
        );

        QVERIFY(session.isActive());
        QVERIFY(!session.isFinalizing());
        QCOMPARE(session.currentSegmentIndex(), 1);
        QCOMPARE(session.audioDirectory(), QStringLiteral("records/audio"));
        QCOMPARE(session.fileBase(), QStringLiteral("recording-001"));

        session.addCurrentSegment(
            QStringLiteral("one.wav"),
            QByteArray("one")
        );
        QCOMPARE(session.recognitionState().segments().size(), 1);
        QCOMPARE(session.pcmBySegment().value(1), QByteArray("one"));
        QVERIFY(session.advanceToNextSegment());
        QCOMPARE(session.currentSegmentIndex(), 2);

        QVERIFY(session.beginFinalizing());
        QVERIFY(session.isFinalizing());
        QVERIFY(!session.beginFinalizing());

        session.complete();
        QVERIFY(!session.isActive());
        QVERIFY(!session.isFinalizing());
        QCOMPARE(session.currentSegmentIndex(), 0);
        QVERIFY(session.pcmBySegment().isEmpty());
    }

    void recordsEmptyLongRecordingSegmentAsTerminalFailure()
    {
        VoiceLongRecordingSession session;
        session.begin(true, QString(), QString());
        session.addCurrentSegment(QStringLiteral("empty.wav"), QByteArray());
        session.recordCurrentTerminalFailure(
            QStringLiteral("no audio data"),
            0
        );

        const RecordingSegment segment =
            session.recognitionState().segment(1);
        QCOMPARE(segment.attempts, 2);
        QCOMPARE(segment.error, QStringLiteral("no audio data"));
        QCOMPARE(session.recognitionState().nextPendingIndex(), -1);
    }

    void stopsLongRecordingSessionAtSegmentLimit()
    {
        VoiceLongRecordingSession session;
        session.begin(true, QString(), QString());

        for (int index = 2; index <= 33; ++index) {
            QVERIFY(session.advanceToNextSegment());
        }

        QCOMPARE(session.currentSegmentIndex(), 33);
        QVERIFY(!session.advanceToNextSegment());
        QCOMPARE(session.currentSegmentIndex(), 33);
    }

    void recognizesSpeechAndBuildsProviderRequest()
    {
        VoiceSpeechRecognitionRequest request;
        request.index = 3;
        request.modeId = QStringLiteral("dictate");
        request.audioData = QByteArray("pcm");
        request.provider = QStringLiteral("iflytek");
        request.language = QStringLiteral("en-US");
        request.networkPolicy = QStringLiteral("direct");
        request.useSystemProxy = true;
        request.sampleRate = 16000;

        SpeechRecognitionProviderTaskRequest captured;
        VoiceSpeechRecognitionHandlers handlers;
        handlers.recognizeProvider = [&captured](
            const SpeechRecognitionProviderTaskRequest &providerRequest
        ) {
            captured = providerRequest;
            SpeechRecognitionTaskResult result;
            result.text = QStringLiteral("recognized text");
            result.errorCode = QStringLiteral("speech.test.code");
            result.elapsedMs = 42;
            return result;
        };

        const VoiceSpeechRecognitionResult result =
            VoiceSpeechRecognitionExecutor::run(request, handlers);

        QVERIFY(result.ok);
        QCOMPARE(result.text, QStringLiteral("recognized text"));
        QCOMPARE(result.index, 3);
        QCOMPARE(result.elapsedMs, qint64(42));
        QCOMPARE(captured.index, 3);
        QCOMPARE(captured.audioData, QByteArray("pcm"));
        QCOMPARE(captured.provider, QStringLiteral("iflytek"));
        QCOMPARE(captured.language, QStringLiteral("en-US"));
        QCOMPARE(captured.networkPolicy, QStringLiteral("direct"));
        QVERIFY(captured.useSystemProxy);
        QCOMPARE(result.errorCode, QStringLiteral("speech.test.code"));
        QVERIFY(result.logDetail.contains(QStringLiteral("dictate")));
        QVERIFY(result.logDetail.contains(QString::number(result.text.size())));
    }

    void reportsEmptySpeechRecognition()
    {
        VoiceSpeechRecognitionRequest request;
        request.modeId = QStringLiteral("translate");

        VoiceSpeechRecognitionHandlers handlers;
        handlers.recognizeProvider = [](
            const SpeechRecognitionProviderTaskRequest &providerRequest
        ) {
            Q_UNUSED(providerRequest);
            SpeechRecognitionTaskResult result;
            result.elapsedMs = 7;
            return result;
        };

        const VoiceSpeechRecognitionResult result =
            VoiceSpeechRecognitionExecutor::run(request, handlers);

        QVERIFY(!result.ok);
        QVERIFY(!result.error.trimmed().isEmpty());
        QCOMPARE(result.elapsedMs, qint64(7));
        QVERIFY(result.logDetail.contains(QStringLiteral("translate")));
        QVERIFY(result.logDetail.contains(result.error));
    }

    void reportsMissingRecognizerHandler()
    {
        VoiceSpeechRecognitionRequest request;
        request.modeId = QStringLiteral("ask");

        const VoiceSpeechRecognitionResult result =
            VoiceSpeechRecognitionExecutor::run(
                request,
                VoiceSpeechRecognitionHandlers()
            );

        QVERIFY(!result.ok);
        QVERIFY(!result.error.trimmed().isEmpty());
        QVERIFY(result.logDetail.contains(QStringLiteral("ask")));
        QVERIFY(result.logDetail.contains(result.error));
    }

    void cancellationOverridesReturnedProviderErrorCode()
    {
        CancellationSource cancellation;
        VoiceSpeechRecognitionRequest request;
        request.modeId = QStringLiteral("dictate");
        request.cancellation = cancellation.token();

        VoiceSpeechRecognitionHandlers handlers;
        handlers.recognizeProvider = [&cancellation](
            const SpeechRecognitionProviderTaskRequest &
        ) {
            cancellation.cancel();
            SpeechRecognitionTaskResult result;
            result.error = QStringLiteral("recognizer missing");
            result.errorCode = QStringLiteral(
                "speech.windows.recognizer_missing"
            );
            return result;
        };

        const VoiceSpeechRecognitionResult result =
            VoiceSpeechRecognitionExecutor::run(request, handlers);

        QVERIFY(result.cancelled);
        QVERIFY(result.error.contains(QString::fromUtf8("取消")));
        QCOMPARE(result.errorCode, QStringLiteral("operation.cancelled"));
    }

    void preCancelledRecognitionUsesCancellationErrorCode()
    {
        CancellationSource cancellation;
        cancellation.cancel();
        VoiceSpeechRecognitionRequest request;
        request.cancellation = cancellation.token();

        const VoiceSpeechRecognitionResult result =
            VoiceSpeechRecognitionExecutor::run(
                request,
                VoiceSpeechRecognitionHandlers()
            );

        QVERIFY(result.cancelled);
        QCOMPARE(result.errorCode, QStringLiteral("operation.cancelled"));
    }

    void mapsProviderResultIntoTaskResult()
    {
        SpeechRecognitionResult providerResult;
        providerResult.text = QStringLiteral("provider text");
        providerResult.error.code = QStringLiteral("speech.test.code");
        providerResult.error.message = QStringLiteral("provider error");
        providerResult.durationMs = 27;

        const SpeechRecognitionTaskResult result =
            speechRecognitionTaskResultFromProviderResult(
                8,
                providerResult,
                99
            );

        QCOMPARE(result.index, 8);
        QCOMPARE(result.text, providerResult.text);
        QCOMPARE(result.error, providerResult.error.message);
        QCOMPARE(result.errorCode, providerResult.error.code);
        QCOMPARE(result.elapsedMs, qint64(27));
    }

    void buildsLongRecordingResultInSegmentOrder()
    {
        SegmentedRecordingState state;
        state.addSegment(1, QStringLiteral("one.wav"));
        QVERIFY(state.markAttemptStarted(1));
        state.recordResult(1, QStringLiteral("first"), QString(), 10);
        state.addSegment(2, QStringLiteral("two.wav"));
        QVERIFY(state.markAttemptStarted(2));
        state.recordResult(2, QStringLiteral("second"), QString(), 20);

        QMap<int, QByteArray> pcm;
        pcm.insert(2, QByteArray("bb"));
        pcm.insert(1, QByteArray("aa"));

        const VoiceLongRecordingBuildResult result =
            VoiceLongRecordingResultBuilder::build(state, pcm);

        QCOMPARE(result.segments.size(), 2);
        QCOMPARE(result.completePcm, QByteArray("aabb"));
        QCOMPARE(result.mergedText, QStringLiteral("first\nsecond"));
        QCOMPARE(result.successfulSegmentCount, 2);
        QCOMPARE(result.failedSegmentCount, 0);
    }

    void buildsLongRecordingFailureSummary()
    {
        SegmentedRecordingState state;
        state.addSegment(1, QStringLiteral("one.wav"));
        QVERIFY(state.markAttemptStarted(1));
        QVERIFY(state.markAttemptStarted(1));
        state.recordResult(1, QString(), QStringLiteral("network error"), 11);

        const VoiceLongRecordingBuildResult result =
            VoiceLongRecordingResultBuilder::build(
                state,
                QMap<int, QByteArray>()
            );

        QCOMPARE(result.successfulSegmentCount, 0);
        QCOMPARE(result.failedSegmentCount, 1);
        QVERIFY(result.mergedText.contains(QString::number(1)));
        QVERIFY(!result.noSuccessfulSegmentError.trimmed().isEmpty());
    }

    void recordsTerminalSegmentFailureOnce()
    {
        SegmentedRecordingState state;
        state.addSegment(5, QStringLiteral("five.wav"));

        state.recordTerminalFailure(
            5,
            QStringLiteral("no audio data"),
            4
        );

        QCOMPARE(state.segment(5).attempts, 2);
        QCOMPARE(state.segment(5).error, QStringLiteral("no audio data"));
        QCOMPARE(state.segment(5).recognitionElapsedMs, qint64(4));
        QCOMPARE(state.nextPendingIndex(), -1);
    }

    void retriesLongRecordingSegmentOnceAndSucceeds()
    {
        VoiceLongRecordingSegmentRequest request;
        request.speech.index = 4;
        request.speech.modeId = QStringLiteral("dictate");
        request.speech.audioData = QByteArray("pcm");

        int recognitionCalls = 0;
        VoiceSpeechRecognitionHandlers handlers;
        handlers.recognizeProvider = [&recognitionCalls](
            const SpeechRecognitionProviderTaskRequest &providerRequest
        ) {
            Q_UNUSED(providerRequest);
            ++recognitionCalls;
            SpeechRecognitionTaskResult result;
            result.elapsedMs = 10;
            if (recognitionCalls == 1) {
                result.error = QStringLiteral("temporary network error");
            } else {
                result.text = QStringLiteral("recognized after retry");
            }
            return result;
        };

        const VoiceLongRecordingSegmentResult result =
            VoiceLongRecordingSegmentExecutor::run(request, handlers);

        QVERIFY(result.ok);
        QVERIFY(!result.cancelled);
        QCOMPARE(result.index, 4);
        QCOMPARE(result.attempts, 2);
        QCOMPARE(result.elapsedMs, qint64(20));
        QCOMPARE(result.text, QStringLiteral("recognized after retry"));
        QCOMPARE(recognitionCalls, 2);

        SegmentedRecordingState state;
        state.addSegment(4, QStringLiteral("four.wav"));
        VoiceLongRecordingSegmentExecutor::apply(result, &state);
        QCOMPARE(state.segment(4).attempts, 2);
        QCOMPARE(state.segment(4).text, result.text);
        QCOMPARE(state.nextPendingIndex(), -1);
    }

    void stopsLongRecordingSegmentAfterTwoFailures()
    {
        VoiceLongRecordingSegmentRequest request;
        request.speech.index = 2;
        request.speech.audioData = QByteArray("pcm");

        int recognitionCalls = 0;
        VoiceSpeechRecognitionHandlers handlers;
        handlers.recognizeProvider = [&recognitionCalls](
            const SpeechRecognitionProviderTaskRequest &providerRequest
        ) {
            Q_UNUSED(providerRequest);
            ++recognitionCalls;
            SpeechRecognitionTaskResult result;
            result.error = QStringLiteral("recognition failed");
            result.errorCode = recognitionCalls == 1
                ? QStringLiteral("speech.windows.recognizer_missing")
                : QStringLiteral("speech.windows.local");
            result.elapsedMs = 6;
            return result;
        };

        const VoiceLongRecordingSegmentResult result =
            VoiceLongRecordingSegmentExecutor::run(request, handlers);

        QVERIFY(!result.ok);
        QVERIFY(!result.cancelled);
        QCOMPARE(result.attempts, 2);
        QCOMPARE(result.elapsedMs, qint64(12));
        QCOMPARE(result.errorCode, QStringLiteral("speech.windows.local"));
        QCOMPARE(recognitionCalls, 2);

        SegmentedRecordingState state;
        state.addSegment(2, QStringLiteral("two.wav"));
        VoiceLongRecordingSegmentExecutor::apply(result, &state);
        QCOMPARE(state.segment(2).attempts, 2);
        QCOMPARE(state.segment(2).error, QStringLiteral("recognition failed"));
        QCOMPARE(state.nextPendingIndex(), -1);
    }

    void doesNotStartCancelledLongRecordingSegment()
    {
        CancellationSource cancellation;
        cancellation.cancel();

        VoiceLongRecordingSegmentRequest request;
        request.speech.index = 1;
        request.speech.audioData = QByteArray("pcm");
        request.cancellation = cancellation.token();

        int recognitionCalls = 0;
        VoiceSpeechRecognitionHandlers handlers;
        handlers.recognizeProvider = [&recognitionCalls](
            const SpeechRecognitionProviderTaskRequest &providerRequest
        ) {
            Q_UNUSED(providerRequest);
            ++recognitionCalls;
            return SpeechRecognitionTaskResult();
        };

        const VoiceLongRecordingSegmentResult result =
            VoiceLongRecordingSegmentExecutor::run(request, handlers);

        QVERIFY(!result.ok);
        QVERIFY(result.cancelled);
        QCOMPARE(result.attempts, 0);
        QCOMPARE(recognitionCalls, 0);
    }
};

QTEST_MAIN(VoiceSpeechRecognitionExecutorTests)
#include "voice_speech_recognition_executor_tests.moc"
