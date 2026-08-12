#include <QtTest>

#include "../../src/config/app_settings_data.h"
#include "../../src/controllers/voice_recording_workflow_controller.h"
#include "../../src/domain/function_catalog.h"
#include "../../src/recording/voice_audio_recorder_adapter.h"
#include "../../src/runtime_log.h"
#include "../../src/storage/history_paths.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

#include <atomic>
#include <thread>
#include <type_traits>

namespace {

struct FakeRecorder
{
    int startCount = 0;
    int stopCount = 0;
    QString lastTitle;
    QString lastDirectory;
    bool lastDirectDirectory = false;
    QString wavPath = QStringLiteral("C:/fake/voice.wav");
    QByteArray pcm = QByteArrayLiteral("fake-pcm");
    bool startSucceeds = true;
    QString startError = QStringLiteral("fake recorder failure");
    std::function<void(const QByteArray &)> pcmListener;

    void emitPcm(const QByteArray &pcmChunk)
    {
        if (pcmListener) {
            pcmListener(pcmChunk);
        }
    }

    VoiceRecordingCaptureHandlers handlers()
    {
        VoiceRecordingCaptureHandlers result;
        result.start = [this](
            const QString &title,
            const QString &directory,
            bool directDirectory,
            QString *error
        ) {
            ++startCount;
            lastTitle = title;
            lastDirectory = directory;
            lastDirectDirectory = directDirectory;
            if (!startSucceeds && error) {
                *error = startError;
            }
            return startSucceeds;
        };
        result.stop = [this]() {
            ++stopCount;
            return pcm;
        };
        result.lastWavPath = [this]() {
            return wavPath;
        };
        result.takePeakLevel = []() {
            return 0;
        };
        result.setPcmListener = [this](
            const std::function<void(const QByteArray &)> &listener
        ) {
            pcmListener = listener;
        };
        return result;
    }
};

class FakeStreamingSpeechSession : public IStreamingSpeechSession
{
public:
    bool start(QString *) override
    {
        ++startCount;
        currentState = StreamingSpeechState::Streaming;
        return startSucceeds;
    }

    bool pushAudio(const QByteArray &pcm) override
    {
        lastPushThread = QThread::currentThread();
        pushedAudio.append(pcm);
        return pushSucceeds;
    }

    void finish() override
    {
        ++finishCount;
        currentState = StreamingSpeechState::Finalizing;
    }

    void cancel() override
    {
        ++cancelCount;
        currentState = StreamingSpeechState::Cancelled;
    }

    StreamingSpeechState state() const override
    {
        return currentState;
    }

    void emitSnapshot(
        const QString &committed,
        const QString &provisional
    )
    {
        StreamingTranscriptSnapshot snapshot;
        snapshot.revision = ++revision;
        snapshot.committedText = committed;
        snapshot.provisionalText = provisional;
        if (callbacks.transcriptUpdated) {
            callbacks.transcriptUpdated(snapshot);
        }
    }

    void emitDegraded(const QString &message)
    {
        currentState = StreamingSpeechState::Degraded;
        if (callbacks.degraded) {
            callbacks.degraded(message);
        }
    }

    void emitCompleted(const QString &text)
    {
        currentState = StreamingSpeechState::Completed;
        if (callbacks.completed) {
            callbacks.completed(text);
        }
    }

    StreamingSpeechCallbacks callbacks;
    QList<QByteArray> pushedAudio;
    StreamingSpeechState currentState = StreamingSpeechState::Idle;
    quint64 revision = 0;
    int startCount = 0;
    int finishCount = 0;
    int cancelCount = 0;
    bool startSucceeds = true;
    bool pushSucceeds = true;
    QThread *lastPushThread = nullptr;
};

QSharedPointer<FunctionFlowResolvedDependencies> flowDependencies(
    const QString &nodeId,
    const QString &provider = QStringLiteral("xfyun"),
    const QString &networkPolicy = QStringLiteral("systemProxy"),
    const QString &title = QStringLiteral("Frozen voice flow"),
    const QString &recordDirectory = QStringLiteral("C:/frozen-records")
)
{
    QSharedPointer<FunctionFlowResolvedDependencies> result(
        new FunctionFlowResolvedDependencies
    );
    FunctionFlowResolvedNodeSettings settings;
    settings.speechProviderId = provider;
    settings.effectiveNetworkPolicy = networkPolicy;
    result->byNodeId.insert(nodeId, settings);
    result->functionTitle = title;
    result->recordDirectory = recordDirectory;
    return result;
}

FunctionFlowRunContext flowRun(
    const CancellationSource &source,
    const QSharedPointer<FunctionFlowResolvedDependencies> &dependencies,
    const QString &functionId = QStringLiteral("flow-function")
)
{
    FunctionFlowRunContext run;
    run.runId = source.executionId();
    run.functionId = functionId;
    run.publishedRevision = 7;
    run.publishedHash = QStringLiteral("published-hash");
    run.cancellation = source.token();
    run.dependencies = dependencies;
    return run;
}

FunctionFlowCompiledNode voiceNode(
    const QString &nodeId = QStringLiteral("voice-node")
)
{
    FunctionFlowCompiledNode node;
    node.nodeId = nodeId;
    node.type = FunctionFlowNodeType::VoiceSource;
    node.config.voice.recording.triggerMode = QStringLiteral("toggle");
    node.config.voice.recording.countdownSeconds = 0;
    node.config.voice.recording.beepEnabled = false;
    node.config.voice.recording.longRecordingEnabled = false;
    node.config.voice.recording.segmentSeconds = 20;
    node.config.voice.recording.maximumMinutes = 1;
    return node;
}

VoiceRecordingWorkflowAccess fakeAccess(FakeRecorder *recorder)
{
    VoiceRecordingWorkflowAccess access;
    access.recordingCapture = recorder->handlers();
    access.speechRecognition.recognizeProvider = [](
        const SpeechRecognitionProviderTaskRequest &
    ) {
        SpeechRecognitionTaskResult result;
        result.text = QStringLiteral("recognized");
        result.elapsedMs = 9;
        return result;
    };
    access.playRecordingBeep = [](const QString &) {};
    return access;
}

} // namespace

// The controller test intentionally replaces hardware, filesystem, logging and
// provider boundaries. No test below constructs a real recorder or provider.
struct VoiceAudioRecorderAdapter::Impl
{
};

VoiceAudioRecorderAdapter::VoiceAudioRecorderAdapter()
    : m_impl(new Impl)
{
}

VoiceAudioRecorderAdapter::~VoiceAudioRecorderAdapter()
{
}

VoiceRecordingCaptureHandlers VoiceAudioRecorderAdapter::handlers()
{
    return VoiceRecordingCaptureHandlers();
}

QStringList supportedFunctionFlowPopupActionIds()
{
    return QStringList();
}

QStringList defaultFunctionFlowPopupActionIds()
{
    return QStringList();
}

bool isFunctionFlowPopupActionSupported(const QString &)
{
    return false;
}

QStringList defaultResultActionIds()
{
    return QStringList();
}

QString functionDisplayTitle(
    const AppSettingsData &settings,
    const QString &id,
    const QString &fallback
)
{
    const FunctionSettings &function = settings.function(id);
    if (!function.name.trimmed().isEmpty()) {
        return function.name;
    }
    return fallback.trimmed().isEmpty() ? id : fallback;
}

QString speechProviderTitle(const QString &provider)
{
    return provider;
}

QString historyRootPath(const QString &recordDirectory)
{
    return recordDirectory;
}

QString historyAudioSubFolderName()
{
    return QStringLiteral("audio");
}

QString historyModeDateSubDirectory(
    const QString &recordDirectory,
    const QString &modeTitle,
    const QString &date,
    const QString &subFolder
)
{
    return QDir(recordDirectory).filePath(
        modeTitle + QLatin1Char('/')
        + date + QLatin1Char('/')
        + subFolder
    );
}

void ensureHistoryModeDateStructure(
    const QString &,
    const QString &,
    const QString &
)
{
}

QString uniqueFilePath(const QString &targetPath)
{
    return targetPath;
}

bool writeBytesAtomically(const QString &, const QByteArray &)
{
    return true;
}

void logRuntimeEvent(
    const QString &,
    const QString &,
    const QString &,
    qint64
)
{
}

SpeechRecognitionTaskResult runSpeechRecognitionProviderTask(
    const SpeechRecognitionProviderTaskRequest &
)
{
    SpeechRecognitionTaskResult result;
    result.error = QStringLiteral(
        "real provider boundary must not be called in this test"
    );
    return result;
}

class VoiceRecordingWorkflowControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesIndependentWorkflowInterface();
    void flowUsesFrozenNodeAndRunDependencies();
    void flowHoldReleaseOnlyFinishesCurrentRun();
    void normalEmptyRecognitionSucceedsForInput();
    void recognitionFailurePreservesPayloadWithoutValues();
    void cancellationStopsCountdownBeforeRecordingStarts();
    void cancellationPreservesPayloadAndSuppressesLateCompletion();
    void longRecordingSuccessCarriesCompletePayload();
    void longRecordingEmptyRecognitionSucceedsForInput();
    void longRecordingCancellationStopsRecognitionAndPreservesSegments();
    void classicRecognitionReturnsBeforeProviderCompletes();
    void classicStreamingForwardsPcmAndSkipsBatch();
    void streamingDegradeFallsBackOnceAndIgnoresLateCompletion();
    void disabledStreamingUsesBatchOnly();
    void flowStreamingUsesFrozenProviderAndCompletesOnce();
    void longRecordingStreamingSkipsSegmentRecognition();
    void holdReleaseWaitsForFirstPcmBeforeStopping();
    void holdReleaseStopsOnceAfterWarmupTimeout();
    void classicToggleAndHoldPathsRemainAvailable();
    void classicHoldReleaseSurvivesConfigurationRemoval();
    void ownsPressOnlyForTheCurrentActiveRecording();
    void voiceControllerDoesNotOwnRecordingImplementation();
};

void VoiceRecordingWorkflowControllerTests::
holdReleaseWaitsForFirstPcmBeforeStopping()
{
    FakeRecorder recorder;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.holdReleaseWarmupTimeoutMs = 1000;

    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );
    AppSettingsData settings;
    FunctionSettings hold;
    hold.id = QStringLiteral("hold-warmup");
    hold.name = QStringLiteral("Hold warmup");
    hold.recording.triggerMode = QStringLiteral("hold");
    settings.functions << hold;
    controller.updateConfiguration(settings);
    controller.setActiveHoldFunctions(
        QSet<QString>() << hold.id
    );

    QVERIFY(controller.begin(hold.id));
    QVERIFY(controller.isRecording());
    QVERIFY(controller.handleHotkeyReleased(hold.id));
    QCOMPARE(recorder.stopCount, 0);
    QVERIFY(controller.isRecording());

    recorder.emitPcm(QByteArrayLiteral("first-pcm"));
    QTRY_COMPARE_WITH_TIMEOUT(recorder.stopCount, 1, 500);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isBusy(), 1000);

    recorder.emitPcm(QByteArrayLiteral("late-pcm"));
    QTest::qWait(30);
    QCOMPARE(recorder.stopCount, 1);
}

void VoiceRecordingWorkflowControllerTests::
holdReleaseStopsOnceAfterWarmupTimeout()
{
    FakeRecorder recorder;
    recorder.pcm.clear();
    QStringList failures;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.holdReleaseWarmupTimeoutMs = 30;
    access.speechRecognition.recognizeProvider = [](
        const SpeechRecognitionProviderTaskRequest &request
    ) {
        SpeechRecognitionTaskResult result;
        if (request.audioData.isEmpty()) {
            result.error = QStringLiteral("录音为空。");
        }
        return result;
    };
    access.showFailure = [&](const QString &message) {
        failures.append(message);
    };

    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );
    AppSettingsData settings;
    FunctionSettings hold;
    hold.id = QStringLiteral("hold-timeout");
    hold.name = QStringLiteral("Hold timeout");
    hold.recording.triggerMode = QStringLiteral("hold");
    settings.functions << hold;
    controller.updateConfiguration(settings);
    controller.setActiveHoldFunctions(
        QSet<QString>() << hold.id
    );

    QVERIFY(controller.begin(hold.id));
    QVERIFY(controller.handleHotkeyReleased(hold.id));
    QCOMPARE(recorder.stopCount, 0);

    QTRY_COMPARE_WITH_TIMEOUT(recorder.stopCount, 1, 500);
    QTRY_VERIFY_WITH_TIMEOUT(!failures.isEmpty(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isBusy(), 1000);

    recorder.emitPcm(QByteArrayLiteral("too-late"));
    QTest::qWait(30);
    QCOMPARE(recorder.stopCount, 1);
}

void VoiceRecordingWorkflowControllerTests::
exposesIndependentWorkflowInterface()
{
    QVERIFY((std::is_default_constructible<
        VoiceRecordingWorkflowAccess
    >::value));
    QVERIFY((std::is_constructible<
        VoiceRecordingWorkflowController,
        const VoiceRecordingWorkflowAccess &,
        FloatingBar *,
        VoiceRunSession *,
        QObject *
    >::value));

    using FlowEntry = bool (VoiceRecordingWorkflowController::*)(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowNodeCompletion &
    );
    FlowEntry entry =
        &VoiceRecordingWorkflowController::beginForFlow;
    QVERIFY(entry != nullptr);
}

void VoiceRecordingWorkflowControllerTests::
flowUsesFrozenNodeAndRunDependencies()
{
    FakeRecorder recorder;
    int beepCount = 0;
    QString beepPath;
    int classicResultCount = 0;
    SpeechRecognitionProviderTaskRequest capturedSpeech;

    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.playRecordingBeep = [&](const QString &path) {
        ++beepCount;
        beepPath = path;
    };
    access.processRecognizedSpeech =
        [&](const QString &, const QString &) {
            ++classicResultCount;
        };
    access.speechRecognition.recognizeProvider =
        [&](const SpeechRecognitionProviderTaskRequest &request) {
            capturedSpeech = request;
            SpeechRecognitionTaskResult result;
            result.text = QStringLiteral("frozen recognition");
            result.elapsedMs = 37;
            return result;
        };

    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );

    AppSettingsData classic;
    classic.preRecordCountdownEnabled = true;
    classic.recordingBeepEnabled = false;
    classic.speechProvider = QStringLiteral("baidu");
    classic.useSystemProxy = false;
    FunctionSettings classicFunction;
    classicFunction.id = QStringLiteral("flow-function");
    classicFunction.recording.countdownSeconds = 9;
    classicFunction.recording.longRecordingEnabled = true;
    classicFunction.recording.beepEnabled = false;
    classic.functions.append(classicFunction);
    controller.updateConfiguration(classic);

    CancellationSource source;
    QSharedPointer<FunctionFlowResolvedDependencies> dependencies =
        flowDependencies(QStringLiteral("voice-node"));
    FunctionFlowRunContext run = flowRun(source, dependencies);
    FunctionFlowCompiledNode node = voiceNode();
    node.config.voice.recording.triggerMode = QStringLiteral("hold");
    node.config.voice.recording.beepEnabled = true;
    node.config.voice.recording.beepPath =
        QStringLiteral("C:/missing-flow-beep.wav");

    int completionCount = 0;
    FunctionFlowNodeResult observed;
    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&](const FunctionFlowNodeResult &result) {
            ++completionCount;
            observed = result;
        }
    ));

    // Mutate every source object while the beep-delay is still pending.
    node.config.voice.recording.longRecordingEnabled = true;
    node.config.voice.recording.countdownSeconds = 8;
    dependencies->byNodeId[
        QStringLiteral("voice-node")
    ].speechProviderId = QStringLiteral("custom");
    dependencies->byNodeId[
        QStringLiteral("voice-node")
    ].effectiveNetworkPolicy = QStringLiteral("direct");
    dependencies->functionTitle = QStringLiteral("Mutated title");
    dependencies->recordDirectory = QStringLiteral("C:/mutated-records");
    classic.speechProvider = QStringLiteral("custom");
    classic.useSystemProxy = false;
    controller.updateConfiguration(classic);

    QCOMPARE(beepCount, 1);
    QVERIFY(beepPath.isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(recorder.startCount, 1, 1000);
    QCOMPARE(recorder.lastTitle, QStringLiteral("Frozen voice flow"));
    QCOMPARE(recorder.lastDirectory, QStringLiteral("C:/frozen-records"));
    QVERIFY(!recorder.lastDirectDirectory);

    // No active hold hook means the frozen hold request safely degrades to
    // toggle and the second press finishes the recording.
    QVERIFY(controller.handleHotkey(QStringLiteral("flow-function")));
    QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 1000);
    QCOMPARE(observed.state, FunctionFlowNodeState::Succeeded);
    QCOMPARE(observed.values.size(), 1);
    QCOMPARE(observed.historyObservations.size(), 0);
    QCOMPARE(observed.values.first().text, QStringLiteral("frozen recognition"));
    QCOMPARE(
        observed.values.first().sourceNodeId,
        QStringLiteral("voice-node")
    );
    QVERIFY(!observed.values.first().voice.isNull());
    QCOMPARE(
        observed.values.first().voice->sourceAudioPath,
        recorder.wavPath
    );
    QCOMPARE(observed.values.first().voice->speechElapsedMs, qint64(37));
    QCOMPARE(
        observed.values.first().voice->recordingTriggerMode,
        QStringLiteral("toggle")
    );
    QVERIFY(!observed.values.first().voice->longRecording);
    QVERIFY(observed.values.first().voice->segments.isEmpty());

    QCOMPARE(capturedSpeech.provider, QStringLiteral("xfyun"));
    QCOMPARE(capturedSpeech.networkPolicy, QStringLiteral("systemProxy"));
    QVERIFY(capturedSpeech.useSystemProxy);
    QCOMPARE(
        capturedSpeech.cancellation.executionId(),
        source.executionId()
    );
    QCOMPARE(classicResultCount, 0);
}

void VoiceRecordingWorkflowControllerTests::
flowHoldReleaseOnlyFinishesCurrentRun()
{
    FakeRecorder recorder;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );
    controller.setActiveHoldFunctions(
        QSet<QString>() << QStringLiteral("flow-function")
    );

    CancellationSource source;
    const QSharedPointer<FunctionFlowResolvedDependencies> dependencies =
        flowDependencies(QStringLiteral("voice-node"));
    const FunctionFlowRunContext run = flowRun(source, dependencies);
    FunctionFlowCompiledNode node = voiceNode();
    node.config.voice.recording.triggerMode = QStringLiteral("hold");

    int completionCount = 0;
    FunctionFlowNodeResult observed;
    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&](const FunctionFlowNodeResult &result) {
            ++completionCount;
            observed = result;
        }
    ));
    QVERIFY(controller.isRecording());

    CancellationSource competingSource;
    const FunctionFlowRunContext competingRun =
        flowRun(competingSource, dependencies);
    int competingCompletionCount = 0;
    QVERIFY(!controller.beginForFlow(
        competingRun,
        node,
        [&](const FunctionFlowNodeResult &) {
            ++competingCompletionCount;
        }
    ));
    QCOMPARE(competingCompletionCount, 0);
    QCOMPARE(recorder.startCount, 1);

    // Auto-repeat/another press cannot stop a hold run.
    QVERIFY(controller.handleHotkey(QStringLiteral("flow-function")));
    QVERIFY(controller.isRecording());
    QCOMPARE(recorder.stopCount, 0);

    recorder.emitPcm(QByteArrayLiteral("flow-pcm"));
    QVERIFY(controller.handleHotkeyReleased(
        QStringLiteral("flow-function")
    ));
    QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 1000);
    QCOMPARE(observed.state, FunctionFlowNodeState::Succeeded);
    QCOMPARE(
        observed.values.first().voice->recordingTriggerMode,
        QStringLiteral("hold")
    );

    // Once that runId has completed, a late release is not consumed.
    QVERIFY(!controller.handleHotkeyReleased(
        QStringLiteral("flow-function")
    ));
    QCOMPARE(completionCount, 1);
    QCOMPARE(competingCompletionCount, 0);
}

void VoiceRecordingWorkflowControllerTests::
normalEmptyRecognitionSucceedsForInput()
{
    FakeRecorder recorder;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.speechRecognition.recognizeProvider = [](
        const SpeechRecognitionProviderTaskRequest &
    ) {
        SpeechRecognitionTaskResult result;
        result.elapsedMs = 5;
        return result;
    };

    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );
    CancellationSource source;
    const QSharedPointer<FunctionFlowResolvedDependencies> dependencies =
        flowDependencies(QStringLiteral("voice-node"));
    const FunctionFlowRunContext run = flowRun(source, dependencies);
    const FunctionFlowCompiledNode node = voiceNode();

    int completionCount = 0;
    FunctionFlowNodeResult observed;
    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&](const FunctionFlowNodeResult &result) {
            ++completionCount;
            observed = result;
        }
    ));
    QVERIFY(controller.isRecording());
    QVERIFY(controller.handleHotkey(QStringLiteral("flow-function")));

    QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 1000);
    QCOMPARE(observed.state, FunctionFlowNodeState::Succeeded);
    QCOMPARE(observed.values.size(), 1);
    QVERIFY(observed.values.first().text.isEmpty());
    QVERIFY(!observed.values.first().voice.isNull());
    QCOMPARE(observed.values.first().voice->speechElapsedMs, qint64(5));
}

void VoiceRecordingWorkflowControllerTests::
recognitionFailurePreservesPayloadWithoutValues()
{
    FakeRecorder recorder;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.speechRecognition.recognizeProvider = [](
        const SpeechRecognitionProviderTaskRequest &
    ) {
        SpeechRecognitionTaskResult result;
        result.error = QStringLiteral("fake provider failure");
        result.elapsedMs = 17;
        return result;
    };

    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );
    CancellationSource source;
    const QSharedPointer<FunctionFlowResolvedDependencies> dependencies =
        flowDependencies(QStringLiteral("voice-node"));
    const FunctionFlowRunContext run = flowRun(source, dependencies);
    const FunctionFlowCompiledNode node = voiceNode();

    int completionCount = 0;
    FunctionFlowNodeResult observed;
    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&](const FunctionFlowNodeResult &result) {
            ++completionCount;
            observed = result;
        }
    ));
    QVERIFY(controller.handleHotkey(QStringLiteral("flow-function")));
    QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 1000);

    QCOMPARE(observed.state, FunctionFlowNodeState::Failed);
    QCOMPARE(
        observed.error.code,
        QStringLiteral("flow_voice_failed")
    );
    QCOMPARE(observed.values.size(), 0);
    QCOMPARE(observed.historyObservations.size(), 1);
    QVERIFY(!observed.historyObservations.first().voice.isNull());
    QCOMPARE(
        observed.historyObservations.first().voice->sourceAudioPath,
        recorder.wavPath
    );
    QCOMPARE(
        observed.historyObservations.first().voice->speechElapsedMs,
        qint64(17)
    );
}

void VoiceRecordingWorkflowControllerTests::
cancellationStopsCountdownBeforeRecordingStarts()
{
    FakeRecorder recorder;
    int beepCount = 0;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.playRecordingBeep = [&](const QString &) {
        ++beepCount;
    };

    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );
    CancellationSource source;
    const QSharedPointer<FunctionFlowResolvedDependencies> dependencies =
        flowDependencies(QStringLiteral("voice-node"));
    const FunctionFlowRunContext run = flowRun(source, dependencies);
    FunctionFlowCompiledNode node = voiceNode();
    node.config.voice.recording.beepEnabled = true;
    node.config.voice.recording.beepPath.clear();

    int completionCount = 0;
    FunctionFlowNodeResult observed;
    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&](const FunctionFlowNodeResult &result) {
            ++completionCount;
            observed = result;
        }
    ));
    QCOMPARE(beepCount, 1);
    QVERIFY(controller.isPreparing());

    source.cancel();
    QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 1000);
    QCOMPARE(observed.state, FunctionFlowNodeState::Cancelled);
    QCOMPARE(observed.values.size(), 0);
    QCOMPARE(observed.historyObservations.size(), 0);
    QVERIFY(!controller.isPreparing());
    QTest::qWait(300);
    QCOMPARE(recorder.startCount, 0);
    QCOMPARE(completionCount, 1);
}

void VoiceRecordingWorkflowControllerTests::
cancellationPreservesPayloadAndSuppressesLateCompletion()
{
    FakeRecorder recorder;
    VoiceRecordingFlowSpeechCompletion deferred;
    VoiceSpeechRecognitionRequest deferredRequest;
    VoiceSpeechRecognitionHandlers deferredHandlers;

    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.runSpeechRecognition =
        [&](const VoiceSpeechRecognitionRequest &request,
            const VoiceSpeechRecognitionHandlers &handlers,
            const VoiceRecordingFlowSpeechCompletion &completion) {
            deferredRequest = request;
            deferredHandlers = handlers;
            deferred = completion;
        };

    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );
    CancellationSource source;
    const QSharedPointer<FunctionFlowResolvedDependencies> dependencies =
        flowDependencies(QStringLiteral("voice-node"));
    const FunctionFlowRunContext run = flowRun(source, dependencies);
    const FunctionFlowCompiledNode node = voiceNode();

    int completionCount = 0;
    FunctionFlowNodeResult observed;
    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&](const FunctionFlowNodeResult &result) {
            ++completionCount;
            observed = result;
        }
    ));
    QVERIFY(controller.handleHotkey(QStringLiteral("flow-function")));
    QCOMPARE(completionCount, 0);
    QVERIFY(static_cast<bool>(deferred));
    QCOMPARE(
        deferredRequest.cancellation.executionId(),
        source.executionId()
    );

    source.cancel();
    QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 1000);
    QCOMPARE(observed.state, FunctionFlowNodeState::Cancelled);
    QCOMPARE(observed.values.size(), 0);
    QCOMPARE(observed.historyObservations.size(), 1);
    QVERIFY(!observed.historyObservations.first().voice.isNull());
    QCOMPARE(
        observed.historyObservations.first().voice->sourceAudioPath,
        recorder.wavPath
    );
    QVERIFY(deferredRequest.cancellation.isCancellationRequested());

    VoiceSpeechRecognitionResult late =
        VoiceSpeechRecognitionExecutor::run(
            deferredRequest,
            deferredHandlers
        );
    late.ok = true;
    late.cancelled = false;
    late.text = QStringLiteral("late text");
    deferred(late);
    deferred(late);
    QCoreApplication::processEvents();
    QCOMPARE(completionCount, 1);
    QVERIFY(!controller.isBusy());
}

void VoiceRecordingWorkflowControllerTests::
longRecordingSuccessCarriesCompletePayload()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    FakeRecorder recorder;
    recorder.wavPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("segment-1.wav")
    );
    SpeechRecognitionProviderTaskRequest capturedSpeech;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.speechRecognition.recognizeProvider =
        [&](const SpeechRecognitionProviderTaskRequest &request) {
            capturedSpeech = request;
            SpeechRecognitionTaskResult result;
            result.text = QStringLiteral("long segment text");
            result.elapsedMs = 21;
            return result;
        };

    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );
    CancellationSource source;
    const QSharedPointer<FunctionFlowResolvedDependencies> dependencies =
        flowDependencies(
            QStringLiteral("voice-node"),
            QStringLiteral("xfyun"),
            QStringLiteral("direct"),
            QStringLiteral("Long success"),
            temporaryDirectory.path()
        );
    const FunctionFlowRunContext run = flowRun(source, dependencies);
    FunctionFlowCompiledNode node = voiceNode();
    node.config.voice.recording.longRecordingEnabled = true;

    int completionCount = 0;
    FunctionFlowNodeResult observed;
    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&](const FunctionFlowNodeResult &result) {
            ++completionCount;
            observed = result;
        }
    ));
    QVERIFY(controller.handleHotkey(QStringLiteral("flow-function")));
    QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 1000);

    QCOMPARE(observed.state, FunctionFlowNodeState::Succeeded);
    QCOMPARE(observed.values.size(), 1);
    QCOMPARE(
        observed.values.first().text,
        QStringLiteral("long segment text")
    );
    const QSharedPointer<const FunctionFlowVoicePayload> payload =
        observed.values.first().voice;
    QVERIFY(!payload.isNull());
    QVERIFY(payload->longRecording);
    QVERIFY(!payload->sourceAudioPath.trimmed().isEmpty());
    QCOMPARE(payload->segments.size(), 1);
    QCOMPARE(payload->segments.first().wavPath, recorder.wavPath);
    QCOMPARE(
        payload->segments.first().text,
        QStringLiteral("long segment text")
    );
    QCOMPARE(payload->speechElapsedMs, qint64(21));
    QCOMPARE(capturedSpeech.provider, QStringLiteral("xfyun"));
    QCOMPARE(capturedSpeech.networkPolicy, QStringLiteral("direct"));
    QVERIFY(!capturedSpeech.useSystemProxy);
    QCOMPARE(
        capturedSpeech.cancellation.executionId(),
        source.executionId()
    );
}

void VoiceRecordingWorkflowControllerTests::
longRecordingEmptyRecognitionSucceedsForInput()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    FakeRecorder recorder;
    recorder.wavPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("empty-segment.wav")
    );
    std::atomic<int> recognitionCalls(0);
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.speechRecognition.recognizeProvider =
        [&](const SpeechRecognitionProviderTaskRequest &) {
            ++recognitionCalls;
            SpeechRecognitionTaskResult result;
            result.elapsedMs = 7;
            return result;
        };

    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );
    CancellationSource source;
    const QSharedPointer<FunctionFlowResolvedDependencies> dependencies =
        flowDependencies(
            QStringLiteral("voice-node"),
            QStringLiteral("xfyun"),
            QStringLiteral("direct"),
            QStringLiteral("Long empty"),
            temporaryDirectory.path()
        );
    const FunctionFlowRunContext run = flowRun(source, dependencies);
    FunctionFlowCompiledNode node = voiceNode();
    node.config.voice.recording.longRecordingEnabled = true;

    FunctionFlowNodeResult observed;
    int completionCount = 0;
    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&](const FunctionFlowNodeResult &result) {
            ++completionCount;
            observed = result;
        }
    ));
    QVERIFY(controller.handleHotkey(QStringLiteral("flow-function")));
    QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 1000);

    QCOMPARE(observed.state, FunctionFlowNodeState::Succeeded);
    QCOMPARE(observed.values.size(), 1);
    QVERIFY(observed.values.first().text.isEmpty());
    QVERIFY(!observed.values.first().voice.isNull());
    QCOMPARE(observed.values.first().voice->segments.size(), 1);
    QVERIFY(
        observed.values.first().voice->segments.first().error.isEmpty()
    );
    QCOMPARE(observed.values.first().voice->speechElapsedMs, qint64(14));
    QCOMPARE(recognitionCalls.load(), 2);
}

void VoiceRecordingWorkflowControllerTests::
longRecordingCancellationStopsRecognitionAndPreservesSegments()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    FakeRecorder recorder;
    recorder.wavPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("segment-1.wav")
    );
    std::atomic<int> recognitionCalls(0);
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.speechRecognition.recognizeProvider =
        [&](const SpeechRecognitionProviderTaskRequest &request) {
            ++recognitionCalls;
            while (!request.cancellation.isCancellationRequested()) {
                QThread::msleep(2);
            }
            SpeechRecognitionTaskResult result;
            result.error = QStringLiteral("cancelled by fake provider");
            result.elapsedMs = 11;
            return result;
        };

    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );
    CancellationSource source;
    const QSharedPointer<FunctionFlowResolvedDependencies> dependencies =
        flowDependencies(
            QStringLiteral("voice-node"),
            QStringLiteral("xfyun"),
            QStringLiteral("direct"),
            QStringLiteral("Long flow"),
            temporaryDirectory.path()
        );
    const FunctionFlowRunContext run = flowRun(source, dependencies);
    FunctionFlowCompiledNode node = voiceNode();
    node.config.voice.recording.longRecordingEnabled = true;

    int completionCount = 0;
    FunctionFlowNodeResult observed;
    QVERIFY(controller.beginForFlow(
        run,
        node,
        [&](const FunctionFlowNodeResult &result) {
            ++completionCount;
            observed = result;
        }
    ));
    QVERIFY(controller.isRecording());
    QVERIFY(recorder.lastDirectDirectory);
    QVERIFY(controller.handleHotkey(QStringLiteral("flow-function")));
    QTRY_VERIFY_WITH_TIMEOUT(recognitionCalls.load() > 0, 1000);

    source.cancel();
    QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 1000);
    QCOMPARE(observed.state, FunctionFlowNodeState::Cancelled);
    QCOMPARE(observed.values.size(), 0);
    QCOMPARE(observed.historyObservations.size(), 1);
    const QSharedPointer<const FunctionFlowVoicePayload> payload =
        observed.historyObservations.first().voice;
    QVERIFY(!payload.isNull());
    QVERIFY(payload->longRecording);
    QCOMPARE(payload->recordingTriggerMode, QStringLiteral("toggle"));
    QCOMPARE(payload->segments.size(), 1);
    QCOMPARE(payload->segments.first().wavPath, recorder.wavPath);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isBusy(), 1000);
    QCOMPARE(recognitionCalls.load(), 1);
}

void VoiceRecordingWorkflowControllerTests::
classicRecognitionReturnsBeforeProviderCompletes()
{
    FakeRecorder recorder;
    VoiceRecordingFlowSpeechCompletion deferred;
    QStringList recognized;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.runSpeechRecognition =
        [&](const VoiceSpeechRecognitionRequest &,
            const VoiceSpeechRecognitionHandlers &,
            const VoiceRecordingFlowSpeechCompletion &completion) {
            deferred = completion;
        };
    access.processRecognizedSpeech =
        [&](const QString &modeId, const QString &text) {
            recognized.append(modeId + QLatin1Char(':') + text);
        };

    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );
    AppSettingsData settings;
    settings.speechProvider = QStringLiteral("baidu");
    FunctionSettings function;
    function.id = QStringLiteral("classic-async");
    function.recording.triggerMode = QStringLiteral("toggle");
    settings.functions.append(function);
    controller.updateConfiguration(settings);

    QVERIFY(controller.begin(QStringLiteral("classic-async")));
    QVERIFY(controller.handleHotkey(QStringLiteral("classic-async")));

    QVERIFY2(deferred, "classic recognition must use the asynchronous runner");
    QVERIFY(recognized.isEmpty());
    QVERIFY(controller.isBusy());

    VoiceSpeechRecognitionResult result;
    result.ok = true;
    result.text = QStringLiteral("recognized later");
    result.elapsedMs = 25;
    result.logCategory = QString::fromUtf8("语音识别");
    result.logAction = QString::fromUtf8("得到文本");
    result.logDetail = QStringLiteral("classic-async");
    deferred(result);

    QTRY_COMPARE_WITH_TIMEOUT(recognized.size(), 1, 1000);
    QCOMPARE(
        recognized.first(),
        QStringLiteral("classic-async:recognized later")
    );
    QVERIFY(!controller.isBusy());
}

void VoiceRecordingWorkflowControllerTests::
classicStreamingForwardsPcmAndSkipsBatch()
{
    FakeRecorder recorder;
    QSharedPointer<FakeStreamingSpeechSession> streaming(
        new FakeStreamingSpeechSession
    );
    int batchCount = 0;
    QStringList recognized;
    QString requestedProvider;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.createStreamingSpeechSession = [
        streaming,
        &requestedProvider
    ](
        const StreamingSpeechSessionRequest &request,
        const StreamingSpeechCallbacks &callbacks
    ) -> StreamingSpeechSessionCreation {
        requestedProvider = request.provider;
        streaming->callbacks = callbacks;
        StreamingSpeechSessionCreation result;
        result.session = streaming;
        return result;
    };
    access.runSpeechRecognition = [
        &batchCount
    ](const VoiceSpeechRecognitionRequest &,
      const VoiceSpeechRecognitionHandlers &,
      const VoiceRecordingFlowSpeechCompletion &) {
        ++batchCount;
    };
    access.processRecognizedSpeech = [
        &recognized
    ](const QString &modeId, const QString &text) {
        recognized.append(modeId + QLatin1Char(':') + text);
    };

    VoiceRecordingWorkflowController controller(access, nullptr, nullptr);
    AppSettingsData settings;
    settings.streamingSpeechRecognitionEnabled = true;
    settings.speechProvider = QStringLiteral("xfyun");
    FunctionSettings function;
    function.id = QStringLiteral("classic-stream");
    settings.functions.append(function);
    controller.updateConfiguration(settings);

    QVERIFY(controller.begin(QStringLiteral("classic-stream")));
    QCOMPARE(requestedProvider, QStringLiteral("xfyun"));
    QCOMPARE(streaming->startCount, 1);
    const std::function<void(const QByteArray &)> pcmListener =
        recorder.pcmListener;
    QVERIFY(pcmListener);
    std::thread audioThread([pcmListener]() {
        pcmListener(QByteArrayLiteral("live-pcm"));
    });
    audioThread.join();

    // The audio callback must never enter a WebSocket session from the
    // recorder thread. Delivery is queued to the controller's Qt thread.
    QVERIFY(streaming->pushedAudio.isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(
        streaming->pushedAudio,
        QList<QByteArray>() << QByteArrayLiteral("live-pcm"),
        1000
    );
    QCOMPARE(streaming->lastPushThread, QThread::currentThread());
    streaming->emitSnapshot(
        QStringLiteral("final "),
        QStringLiteral("draft")
    );

    QVERIFY(controller.handleHotkey(QStringLiteral("classic-stream")));
    QCOMPARE(streaming->finishCount, 1);
    QCOMPARE(batchCount, 0);
    QVERIFY(recognized.isEmpty());
    streaming->emitCompleted(QStringLiteral("stream result"));

    QTRY_COMPARE_WITH_TIMEOUT(recognized.size(), 1, 1000);
    QCOMPARE(
        recognized.first(),
        QStringLiteral("classic-stream:stream result")
    );
    QCOMPARE(batchCount, 0);
    streaming->emitCompleted(QStringLiteral("late duplicate"));
    QTest::qWait(20);
    QCOMPARE(recognized.size(), 1);
    QVERIFY(!recorder.pcmListener);
}

void VoiceRecordingWorkflowControllerTests::
streamingDegradeFallsBackOnceAndIgnoresLateCompletion()
{
    FakeRecorder recorder;
    QSharedPointer<FakeStreamingSpeechSession> streaming(
        new FakeStreamingSpeechSession
    );
    int batchCount = 0;
    QStringList recognized;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.createStreamingSpeechSession = [streaming](
        const StreamingSpeechSessionRequest &,
        const StreamingSpeechCallbacks &callbacks
    ) -> StreamingSpeechSessionCreation {
        streaming->callbacks = callbacks;
        StreamingSpeechSessionCreation result;
        result.session = streaming;
        return result;
    };
    access.runSpeechRecognition = [
        &batchCount
    ](const VoiceSpeechRecognitionRequest &,
      const VoiceSpeechRecognitionHandlers &,
      const VoiceRecordingFlowSpeechCompletion &completion) {
        ++batchCount;
        VoiceSpeechRecognitionResult result;
        result.ok = true;
        result.text = QStringLiteral("batch result");
        completion(result);
    };
    access.processRecognizedSpeech = [
        &recognized
    ](const QString &, const QString &text) {
        recognized.append(text);
    };

    VoiceRecordingWorkflowController controller(access, nullptr, nullptr);
    AppSettingsData settings;
    settings.streamingSpeechRecognitionEnabled = true;
    settings.speechProvider = QStringLiteral("baidu");
    FunctionSettings function;
    function.id = QStringLiteral("fallback");
    settings.functions.append(function);
    controller.updateConfiguration(settings);

    QVERIFY(controller.begin(QStringLiteral("fallback")));
    streaming->emitDegraded(QStringLiteral("socket failed"));
    streaming->emitDegraded(QStringLiteral("duplicate"));
    QTRY_VERIFY_WITH_TIMEOUT(bool(recorder.pcmListener), 1000);
    const int pushedBeforeDegrade = streaming->pushedAudio.size();
    recorder.emitPcm(QByteArrayLiteral("after-degrade"));
    QTest::qWait(20);
    QCOMPARE(streaming->pushedAudio.size(), pushedBeforeDegrade);
    QVERIFY(controller.handleHotkey(QStringLiteral("fallback")));

    QTRY_COMPARE_WITH_TIMEOUT(batchCount, 1, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(recognized.size(), 1, 1000);
    QCOMPARE(recognized.first(), QStringLiteral("batch result"));
    streaming->emitCompleted(QStringLiteral("late stream"));
    QTest::qWait(20);
    QCOMPARE(batchCount, 1);
    QCOMPARE(recognized.size(), 1);
}

void VoiceRecordingWorkflowControllerTests::
disabledStreamingUsesBatchOnly()
{
    FakeRecorder recorder;
    int factoryCount = 0;
    int batchCount = 0;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.createStreamingSpeechSession = [
        &factoryCount
    ](const StreamingSpeechSessionRequest &,
      const StreamingSpeechCallbacks &)
        -> StreamingSpeechSessionCreation {
        ++factoryCount;
        return StreamingSpeechSessionCreation();
    };
    access.runSpeechRecognition = [
        &batchCount
    ](const VoiceSpeechRecognitionRequest &,
      const VoiceSpeechRecognitionHandlers &,
      const VoiceRecordingFlowSpeechCompletion &completion) {
        ++batchCount;
        VoiceSpeechRecognitionResult result;
        result.ok = true;
        result.text = QStringLiteral("batch only");
        completion(result);
    };

    VoiceRecordingWorkflowController controller(access, nullptr, nullptr);
    AppSettingsData settings;
    settings.streamingSpeechRecognitionEnabled = false;
    settings.speechProvider = QStringLiteral("xfyun");
    FunctionSettings function;
    function.id = QStringLiteral("disabled");
    settings.functions.append(function);
    controller.updateConfiguration(settings);

    QVERIFY(controller.begin(QStringLiteral("disabled")));
    QVERIFY(controller.handleHotkey(QStringLiteral("disabled")));
    QTRY_COMPARE_WITH_TIMEOUT(batchCount, 1, 1000);
    QCOMPARE(factoryCount, 0);
}

void VoiceRecordingWorkflowControllerTests::
flowStreamingUsesFrozenProviderAndCompletesOnce()
{
    FakeRecorder recorder;
    QSharedPointer<FakeStreamingSpeechSession> streaming(
        new FakeStreamingSpeechSession
    );
    QString requestedProvider;
    int batchCount = 0;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.createStreamingSpeechSession = [
        streaming,
        &requestedProvider
    ](const StreamingSpeechSessionRequest &request,
      const StreamingSpeechCallbacks &callbacks)
        -> StreamingSpeechSessionCreation {
        requestedProvider = request.provider;
        streaming->callbacks = callbacks;
        StreamingSpeechSessionCreation result;
        result.session = streaming;
        return result;
    };
    access.runSpeechRecognition = [
        &batchCount
    ](const VoiceSpeechRecognitionRequest &,
      const VoiceSpeechRecognitionHandlers &,
      const VoiceRecordingFlowSpeechCompletion &) {
        ++batchCount;
    };

    VoiceRecordingWorkflowController controller(access, nullptr, nullptr);
    AppSettingsData settings;
    settings.streamingSpeechRecognitionEnabled = true;
    settings.speechProvider = QStringLiteral("baidu");
    controller.updateConfiguration(settings);

    CancellationSource source;
    FunctionFlowNodeResult observed;
    int completionCount = 0;
    QVERIFY(controller.beginForFlow(
        flowRun(source, flowDependencies(
            QStringLiteral("voice-node"),
            QStringLiteral("xfyun")
        )),
        voiceNode(),
        [&](const FunctionFlowNodeResult &result) {
            ++completionCount;
            observed = result;
        }
    ));
    QCOMPARE(requestedProvider, QStringLiteral("xfyun"));
    QVERIFY(controller.handleHotkey(QStringLiteral("flow-function")));
    QCOMPARE(streaming->finishCount, 1);
    QCOMPARE(batchCount, 0);

    streaming->emitCompleted(QStringLiteral("flow stream result"));
    QTRY_COMPARE_WITH_TIMEOUT(completionCount, 1, 1000);
    QCOMPARE(observed.state, FunctionFlowNodeState::Succeeded);
    QCOMPARE(observed.values.size(), 1);
    QCOMPARE(observed.values.first().text, QStringLiteral("flow stream result"));
    streaming->emitCompleted(QStringLiteral("late"));
    QTest::qWait(20);
    QCOMPARE(completionCount, 1);
    QCOMPARE(batchCount, 0);
}

void VoiceRecordingWorkflowControllerTests::
longRecordingStreamingSkipsSegmentRecognition()
{
    FakeRecorder recorder;
    QSharedPointer<FakeStreamingSpeechSession> streaming(
        new FakeStreamingSpeechSession
    );
    int providerRecognitionCount = 0;
    QStringList recognized;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.createStreamingSpeechSession = [streaming](
        const StreamingSpeechSessionRequest &,
        const StreamingSpeechCallbacks &callbacks
    ) -> StreamingSpeechSessionCreation {
        streaming->callbacks = callbacks;
        StreamingSpeechSessionCreation result;
        result.session = streaming;
        return result;
    };
    access.speechRecognition.recognizeProvider = [
        &providerRecognitionCount
    ](const SpeechRecognitionProviderTaskRequest &) {
        ++providerRecognitionCount;
        SpeechRecognitionTaskResult result;
        result.text = QStringLiteral("batch segment");
        return result;
    };
    access.processRecognizedSpeech = [
        &recognized
    ](const QString &, const QString &text) {
        recognized.append(text);
    };

    VoiceRecordingWorkflowController controller(access, nullptr, nullptr);
    AppSettingsData settings;
    settings.streamingSpeechRecognitionEnabled = true;
    settings.speechProvider = QStringLiteral("xfyun");
    FunctionSettings function;
    function.id = QStringLiteral("long-stream");
    function.recording.longRecordingEnabled = true;
    function.recording.segmentSeconds = 20;
    function.recording.maximumMinutes = 1;
    settings.functions.append(function);
    controller.updateConfiguration(settings);

    QVERIFY(controller.begin(QStringLiteral("long-stream")));
    QVERIFY(controller.handleHotkey(QStringLiteral("long-stream")));
    QCOMPARE(streaming->finishCount, 1);
    QCOMPARE(providerRecognitionCount, 0);
    streaming->emitCompleted(QStringLiteral("complete long stream"));

    QTRY_COMPARE_WITH_TIMEOUT(recognized.size(), 1, 1000);
    QCOMPARE(recognized.first(), QStringLiteral("complete long stream"));
    QCOMPARE(providerRecognitionCount, 0);
    QVERIFY(!controller.isBusy());
}

void VoiceRecordingWorkflowControllerTests::
classicToggleAndHoldPathsRemainAvailable()
{
    FakeRecorder recorder;
    QStringList recognized;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    access.processRecognizedSpeech =
        [&](const QString &modeId, const QString &text) {
            recognized.append(modeId + QLatin1Char(':') + text);
        };

    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );

    AppSettingsData settings;
    settings.speechProvider = QStringLiteral("xfyun");
    FunctionSettings toggle;
    toggle.id = QStringLiteral("classic-toggle");
    toggle.name = QStringLiteral("Classic toggle");
    toggle.recording.triggerMode = QStringLiteral("toggle");
    FunctionSettings hold;
    hold.id = QStringLiteral("classic-hold");
    hold.name = QStringLiteral("Classic hold");
    hold.recording.triggerMode = QStringLiteral("hold");
    settings.functions << toggle << hold;
    controller.updateConfiguration(settings);
    controller.setActiveHoldFunctions(
        QSet<QString>() << QStringLiteral("classic-hold")
    );

    QVERIFY(controller.begin(QStringLiteral("classic-toggle")));
    QVERIFY(controller.isRecording());
    QVERIFY(controller.handleHotkey(QStringLiteral("classic-toggle")));
    QTRY_COMPARE_WITH_TIMEOUT(recognized.size(), 1, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isBusy(), 1000);

    QVERIFY(controller.begin(QStringLiteral("classic-hold")));
    QVERIFY(controller.isRecording());
    QVERIFY(controller.handleHotkey(QStringLiteral("classic-hold")));
    QVERIFY(controller.isRecording());
    recorder.emitPcm(QByteArrayLiteral("hold-pcm"));
    QVERIFY(controller.handleHotkeyReleased(
        QStringLiteral("classic-hold")
    ));
    QTRY_COMPARE_WITH_TIMEOUT(recognized.size(), 2, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isBusy(), 1000);
    QCOMPARE(recorder.stopCount, 2);
}

void VoiceRecordingWorkflowControllerTests::
classicHoldReleaseSurvivesConfigurationRemoval()
{
    FakeRecorder recorder;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );

    AppSettingsData settings;
    settings.speechProvider = QStringLiteral("xfyun");
    FunctionSettings hold;
    hold.id = QStringLiteral("classic-hold");
    hold.name = QStringLiteral("Classic hold");
    hold.recording.triggerMode = QStringLiteral("hold");
    settings.functions << hold;
    controller.updateConfiguration(settings);
    controller.setActiveHoldFunctions(
        QSet<QString>() << QStringLiteral("classic-hold")
    );

    QVERIFY(controller.begin(QStringLiteral("classic-hold")));
    QVERIFY(controller.isRecording());

    settings.functions.clear();
    controller.updateConfiguration(settings);
    recorder.emitPcm(QByteArrayLiteral("first-pcm"));
    QCoreApplication::processEvents();
    QVERIFY(controller.handleHotkeyReleased(
        QStringLiteral("classic-hold")
    ));
    QCOMPARE(recorder.stopCount, 1);
}

void VoiceRecordingWorkflowControllerTests::
ownsPressOnlyForTheCurrentActiveRecording()
{
    FakeRecorder recorder;
    VoiceRecordingWorkflowAccess access = fakeAccess(&recorder);
    VoiceRecordingWorkflowController controller(
        access,
        nullptr,
        nullptr
    );

    AppSettingsData settings;
    settings.speechProvider = QStringLiteral("xfyun");
    FunctionSettings active;
    active.id = QStringLiteral("active");
    active.recording.triggerMode = QStringLiteral("toggle");
    FunctionSettings other;
    other.id = QStringLiteral("other");
    other.recording.triggerMode = QStringLiteral("toggle");
    settings.functions << active << other;
    controller.updateConfiguration(settings);

    QVERIFY(!controller.ownsPress(QStringLiteral("active")));
    QVERIFY(controller.begin(QStringLiteral("active")));
    QVERIFY(controller.ownsPress(QStringLiteral("active")));
    QVERIFY(!controller.ownsPress(QStringLiteral("other")));
    QVERIFY(!controller.ownsPress(QStringLiteral("missing")));

    settings.functions.clear();
    controller.updateConfiguration(settings);
    QVERIFY(controller.ownsPress(QStringLiteral("active")));
    QVERIFY(controller.handleHotkey(QStringLiteral("active")));
    QVERIFY(!controller.ownsPress(QStringLiteral("active")));
}

void VoiceRecordingWorkflowControllerTests::
voiceControllerDoesNotOwnRecordingImplementation()
{
    const QString voicePath = QFINDTESTDATA(
        "../../src/controllers/voice_controller.cpp"
    );
    const QString workflowPath = QFINDTESTDATA(
        "../../src/controllers/voice_recording_workflow_controller.cpp"
    );
    QVERIFY2(!voicePath.isEmpty(), "VoiceController source was not found");
    QVERIFY2(
        !workflowPath.isEmpty(),
        "Voice recording workflow source was not found"
    );

    QFile voiceSource(voicePath);
    QFile workflowSource(workflowPath);
    QVERIFY(voiceSource.open(QIODevice::ReadOnly));
    QVERIFY(workflowSource.open(QIODevice::ReadOnly));
    const QByteArray voiceContents = voiceSource.readAll();
    const QByteArray workflowContents = workflowSource.readAll();

    QVERIFY(voiceContents.contains("VoiceRecordingWorkflowController"));
    QVERIFY(!voiceContents.contains("VoiceAudioRecorderAdapter"));
    QVERIFY(!voiceContents.contains("VoiceRecordingCapture"));
    QVERIFY(!voiceContents.contains("VoiceRecordingLifecycle"));
    QVERIFY(!voiceContents.contains("VoiceRecordingCoordinator"));
    QVERIFY(!voiceContents.contains(
        "VoiceLongRecordingRecognitionCoordinator"
    ));
    QVERIFY(!voiceContents.contains("beginRecordingWithPreparation("));
    QVERIFY(!voiceContents.contains("rotateLongRecordingSegment("));
    QVERIFY(!voiceContents.contains("stopLongRecordingAndProcess("));
    QVERIFY(!voiceContents.contains("finishLongRecordingRecognition("));
    QVERIFY(!voiceContents.contains("stopAndProcess("));

    QVERIFY(workflowContents.contains("VoiceAudioRecorderAdapter"));
    QVERIFY(workflowContents.contains("VoiceRecordingCapture"));
    QVERIFY(workflowContents.contains("VoiceRecordingLifecycle"));
    QVERIFY(workflowContents.contains("VoiceRecordingCoordinator"));
    QVERIFY(workflowContents.contains(
        "VoiceLongRecordingRecognitionCoordinator"
    ));
    QVERIFY(!workflowContents.contains("VoiceRecordingCompletionExecutor"));
    QVERIFY(workflowContents.contains("VoiceSpeechRecognitionExecutor"));
    QVERIFY(workflowContents.contains(
        "VoiceLongRecordingCompletionExecutor"
    ));
}

QTEST_MAIN(VoiceRecordingWorkflowControllerTests)

#include "voice_recording_workflow_controller_tests.moc"
