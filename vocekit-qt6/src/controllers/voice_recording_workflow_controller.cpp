#include "voice_recording_workflow_controller.h"

#include "../config/app_settings_data.h"
#include "../config/app_settings_defaults.h"
#include "../domain/function_catalog.h"
#include "../domain/voice_run_session.h"
#include "../file_utils.h"
#include "../recording/segmented_recording.h"
#include "../recording/voice_audio_recorder_adapter.h"
#include "../recording/voice_long_recording_session.h"
#include "../recording/voice_recording_capture.h"
#include "../recording/voice_recording_coordinator.h"
#include "../recording/voice_recording_lifecycle.h"
#include "../runtime_log.h"
#include "../storage/history_paths.h"
#include "../tasks/speech_recognition_task.h"
#include "../tasks/voice_long_recording_completion_executor.h"
#include "../tasks/voice_long_recording_recognition_coordinator.h"
#include "../tasks/voice_speech_recognition_executor.h"
#include "../ui/floating_bar.h"
#include "../providers/windows_speech_helper_protocol.h"

#include <QtMultimedia>
#include <QtConcurrent>
#include <QtWidgets>

#include <QCoreApplication>
#include <QEvent>
#include <QUuid>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

int streamingPcmEventType()
{
    static const int type = QEvent::registerEventType();
    return type;
}

class StreamingPcmEvent : public QEvent
{
public:
    StreamingPcmEvent(
        quint64 generationValue,
        const QByteArray &pcmValue
    )
        : QEvent(static_cast<QEvent::Type>(streamingPcmEventType())),
          generation(generationValue),
          pcm(pcmValue)
    {
    }

    quint64 generation = 0;
    QByteArray pcm;
};

VoiceRecordingCaptureHandlers recordingCaptureHandlers(
    const VoiceRecordingWorkflowAccess &access,
    VoiceAudioRecorderAdapter *adapter
)
{
    if (access.recordingCapture.start) {
        return access.recordingCapture;
    }
    return adapter
        ? adapter->handlers()
        : VoiceRecordingCaptureHandlers();
}

OperationError flowVoiceError(
    const QString &code,
    const QString &message = QString()
)
{
    OperationError error;
    error.code = code;
    error.message = message;
    return error;
}

} // namespace

class VoiceRecordingWorkflowController::Impl : public QObject
{
private:
    struct FlowState
    {
        bool active = false;
        bool processing = false;
        quint64 generation = 0;
        ExecutionId runId;
        CancellationToken cancellation;
        QString functionId;
        QString nodeId;
        QString functionTitle;
        QString recordDirectory;
        QString provider;
        QString networkPolicy;
        FunctionFlowRecordingConfig recording;
        QString effectiveTriggerMode = QStringLiteral("toggle");
        FunctionFlowNodeCompletion completion;
        VoiceRecordingStopResult normalRecording;
        qint64 speechElapsedMs = -1;
        bool longRecognitionSawResult = false;
        bool longRecognitionOnlyEmpty = true;
    };

public:
    Impl(
        const VoiceRecordingWorkflowAccess &access,
        FloatingBar *bar,
        VoiceRunSession *runSession,
        QObject *parent
    )
        : QObject(parent),
          m_access(access),
          m_bar(bar),
          m_runSession(runSession),
          m_recordingCapture(recordingCaptureHandlers(
              access,
              &m_audioRecorderAdapter
          )),
          m_recordingCoordinator(
              m_recordingCapture,
              m_recordingLifecycle
          ),
          m_longRecognitionCoordinator(this),
          m_longRecordingSession(
              m_recordingCapture.longRecordingSession()
          )
    {
        configureLifecycleCallbacks();
        configureCoordinatorCallbacks();
        configureRecognitionCallbacks();
        m_flowCancellationTimer.setInterval(10);
        m_streamingFinalTimer.setSingleShot(true);
        m_holdReleaseWarmupTimer.setSingleShot(true);
        connect(
            &m_flowCancellationTimer,
            &QTimer::timeout,
            this,
            [this]() {
                cancelFlowIfRequested();
            }
        );
        connect(
            &m_streamingFinalTimer,
            &QTimer::timeout,
            this,
            [this]() {
                handleStreamingDegraded(
                    m_operationGeneration,
                    tr8("等待实时识别最终结果超时。")
                );
            }
        );
        connect(
            &m_holdReleaseWarmupTimer,
            &QTimer::timeout,
            this,
            [this]() {
                if (!m_holdReleasePending) {
                    return;
                }
                m_holdReleasePending = false;
                if (!m_recordingLifecycle.isRecording()) {
                    return;
                }
                logRuntimeEvent(
                    tr8("录音"),
                    tr8("麦克风预热超时"),
                    QStringLiteral("功能=") + m_modeId
                );
                stopAndProcess();
            }
        );
    }

    ~Impl() override
    {
        clearRecordingActions();
        m_flowCancellationTimer.stop();
        cancelStreamingSession();
        m_recordingCoordinator.cancelPreparation();
        m_longRecognitionCoordinator.cancel();
        if (m_recordingLifecycle.isRecording()) {
            m_recordingLifecycle.stop();
            m_recordingCapture.stopNormal();
        }
        m_longRecordingSession.complete();
    }

    bool event(QEvent *event) override
    {
        if (event && event->type() == streamingPcmEventType()) {
            StreamingPcmEvent *pcmEvent =
                static_cast<StreamingPcmEvent *>(event);
            if (pcmEvent->generation == m_operationGeneration) {
                if (!pcmEvent->pcm.isEmpty()) {
                    m_recordingReceivedPcm = true;
                }
                if (!m_streamingSession.isNull()) {
                    m_streamingSession->pushAudio(pcmEvent->pcm);
                }
                if (m_recordingReceivedPcm
                    && m_holdReleasePending
                    && m_recordingLifecycle.isRecording()) {
                    m_holdReleasePending = false;
                    m_holdReleaseWarmupTimer.stop();
                    logRuntimeEvent(
                        tr8("录音"),
                        tr8("麦克风已就绪，结束按住说话"),
                        QStringLiteral("功能=") + m_modeId
                    );
                    stopAndProcess();
                }
            }
            return true;
        }
        return QObject::event(event);
    }

    void updateConfiguration(const AppSettingsData &settings)
    {
        m_settings = settings;
    }

    void setActiveHoldFunctions(const QSet<QString> &ids)
    {
        m_activeHoldFunctions = ids;
    }

    bool begin(const QString &functionId)
    {
        const QString id = functionId.trimmed();
        if (id.isEmpty() || isBusy() || externalProcessing()) {
            return false;
        }

        ++m_operationGeneration;
        m_windowsSpeechFailureShown = false;
        m_windowsStructuredTerminalHandled = false;
        m_activeSpeechLanguage = normalizeWindowsSpeechLanguage(
            m_settings.windowsSpeechLanguage
        );
        m_modeId = id;
        m_coordinatorModeId = id;
        m_classicEffectiveTriggerMode =
            functionSettings(id).recording.triggerMode
                    == QStringLiteral("hold")
                && m_activeHoldFunctions.contains(id)
                ? QStringLiteral("hold")
                : QStringLiteral("toggle");
        m_longRecognitionCoordinator.reset();
        preparePcmTracking();
        if (m_runSession) {
            m_runSession->setRecordingTriggerMode(
                m_classicEffectiveTriggerMode
            );
        }

        const bool longRecordingEnabled =
            longRecordingEnabledFor(id);
        VoiceRecordingCoordinatorRequest request;
        request.modeId = m_coordinatorModeId;
        request.countdownSeconds = m_settings.preRecordCountdownEnabled
            ? countdownSecondsFor(id)
            : 0;
        request.playBeep = shouldPlayRecordingBeep(id);
        request.captureRequestBuilder = [this, id]() {
            return buildRecordingStartRequest(id);
        };
        request.segmentIntervalMs = longRecordingEnabled
            ? segmentSecondsFor(id) * 1000
            : 0;
        request.limitIntervalMs = longRecordingEnabled
            ? maxRecordingMinutesFor(id) * 60 * 1000
            : 0;
        installPreparationActions();
        m_recordingCoordinator.begin(request);
        return true;
    }

    bool beginForFlow(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowNodeCompletion &completion
    )
    {
        if (!completion
            || !run.runId.isValid()
            || !run.cancellation.isValid()
            || run.cancellation.executionId() != run.runId
            || run.functionId.trimmed().isEmpty()
            || node.nodeId.trimmed().isEmpty()
            || node.type != FunctionFlowNodeType::VoiceSource
            || run.dependencies.isNull()
            || !run.dependencies->byNodeId.contains(node.nodeId)
            || isBusy()
            || externalProcessing()) {
            return false;
        }

        const FunctionFlowResolvedNodeSettings resolved =
            run.dependencies->byNodeId.value(node.nodeId);
        if (resolved.speechProviderId.trimmed().isEmpty()
            || resolved.effectiveNetworkPolicy.trimmed().isEmpty()) {
            return false;
        }

        FlowState flow;
        m_windowsSpeechFailureShown = false;
        m_windowsStructuredTerminalHandled = false;
        m_activeSpeechLanguage = normalizeWindowsSpeechLanguage(
            m_settings.windowsSpeechLanguage
        );
        flow.active = true;
        flow.generation = ++m_operationGeneration;
        flow.runId = run.runId;
        flow.cancellation = run.cancellation;
        flow.functionId = run.functionId.trimmed();
        flow.nodeId = node.nodeId;
        flow.functionTitle =
            run.dependencies->functionTitle.trimmed().isEmpty()
                ? flow.functionId
                : run.dependencies->functionTitle;
        flow.recordDirectory = run.dependencies->recordDirectory;
        flow.provider = resolved.speechProviderId;
        flow.networkPolicy = resolved.effectiveNetworkPolicy;
        flow.recording = node.config.voice.recording;
        flow.effectiveTriggerMode =
            flow.recording.triggerMode == QStringLiteral("hold")
            && m_activeHoldFunctions.contains(flow.functionId)
                ? QStringLiteral("hold")
                : QStringLiteral("toggle");
        flow.completion = completion;
        flow.speechElapsedMs =
            flow.recording.longRecordingEnabled ? 0 : -1;
        m_flow = flow;
        m_modeId = flow.functionId;
        m_coordinatorModeId =
            QStringLiteral("flow:") + flow.runId.value;
        m_longRecognitionCoordinator.reset();
        preparePcmTracking();
        m_flowCancellationTimer.start();

        if (m_flow.cancellation.isCancellationRequested()) {
            cancelActiveFlow();
            return true;
        }

        const quint64 generation = m_flow.generation;
        const ExecutionId runId = m_flow.runId;
        VoiceRecordingCoordinatorRequest request;
        request.modeId = m_coordinatorModeId;
        request.countdownSeconds =
            m_flow.recording.countdownSeconds;
        request.playBeep = m_flow.recording.beepEnabled;
        request.captureRequestBuilder =
            [this, generation, runId]() {
                return flowMatches(generation, runId)
                    ? buildFlowRecordingStartRequest()
                    : VoiceRecordingStartRequest();
            };
        request.segmentIntervalMs =
            m_flow.recording.longRecordingEnabled
                ? m_flow.recording.segmentSeconds * 1000
                : 0;
        request.limitIntervalMs =
            m_flow.recording.longRecordingEnabled
                ? m_flow.recording.maximumMinutes * 60 * 1000
                : 0;
        installPreparationActions();
        m_recordingCoordinator.begin(request);
        return true;
    }

    bool handleHotkey(const QString &functionId)
    {
        const QString id = functionId.trimmed();
        if (m_flow.active
            && m_flow.cancellation.isCancellationRequested()) {
            const bool matched = id == m_modeId;
            cancelActiveFlow();
            return matched;
        }
        if (m_recordingCoordinator.isPreparing()) {
            if (preparationMatchesCurrent(id)) {
                if (usesHoldToTalk(id)) {
                    logRuntimeEvent(
                        tr8("快捷键"),
                        tr8("忽略自动重复"),
                        QStringLiteral("功能=") + id
                    );
                    return true;
                }
                if (m_flow.active) {
                    cancelActiveFlow();
                } else {
                    m_recordingCoordinator.cancelPreparation();
                    clearPcmTracking();
                    setStatus(tr8("已取消"), tr8("录音准备已取消"));
                    hideBarLater();
                    logRuntimeEvent(
                        tr8("录音"),
                        tr8("取消准备"),
                        QStringLiteral("功能=") + id
                    );
                }
            } else {
                setStatus(
                    tr8("正在准备录音"),
                    tr8("请等待倒计时结束，或再次按当前快捷键取消。")
                );
                logRuntimeEvent(
                    tr8("快捷键"),
                    tr8("忽略"),
                    QStringLiteral("原因=录音准备中，功能=") + id
                );
            }
            return true;
        }

        if (!m_recordingLifecycle.isRecording()) {
            return false;
        }

        if (id == m_modeId) {
            if (usesHoldToTalk(id)) {
                logRuntimeEvent(
                    tr8("快捷键"),
                    tr8("忽略自动重复"),
                    QStringLiteral("功能=") + id
                );
                return true;
            }
            logRuntimeEvent(
                tr8("录音"),
                tr8("停止快捷键"),
                QStringLiteral("功能=") + id,
                elapsedMs()
            );
            stopAndProcess();
        } else {
            setStatus(tr8("正在录音"), tr8("请先结束当前录音。"));
            logRuntimeEvent(
                tr8("快捷键"),
                tr8("忽略"),
                QStringLiteral("原因=正在录音，当前功能=")
                    + m_modeId
                    + QStringLiteral("，触发功能=")
                    + id
            );
        }
        return true;
    }

    bool ownsPress(const QString &functionId) const
    {
        const QString id = functionId.trimmed();
        if (id.isEmpty() || id != m_modeId) {
            return false;
        }
        if (m_flow.active
            && m_flow.cancellation.isCancellationRequested()) {
            return true;
        }
        return m_recordingCoordinator.isPreparing()
            || m_recordingLifecycle.isRecording();
    }

    bool handleHotkeyReleased(const QString &functionId)
    {
        const QString id = functionId.trimmed();
        if (!usesHoldToTalk(id) || id != m_modeId) {
            return false;
        }
        if (preparationMatchesCurrent(id)) {
            if (m_flow.active) {
                cancelActiveFlow();
            } else {
                m_recordingCoordinator.cancelPreparation();
                clearPcmTracking();
                setStatus(
                    tr8("已取消"),
                    tr8("已在录音开始前松开快捷键")
                );
                hideBarLater();
                logRuntimeEvent(
                    tr8("录音"),
                    tr8("按住说话取消准备"),
                    QStringLiteral("功能=") + id
                );
            }
            return true;
        }
        if (m_recordingLifecycle.isRecording()) {
            logRuntimeEvent(
                tr8("录音"),
                tr8("按住说话松开"),
                QStringLiteral("功能=") + id,
                elapsedMs()
            );
            if (!m_recordingReceivedPcm) {
                if (!m_holdReleasePending) {
                    m_holdReleasePending = true;
                    m_holdReleaseWarmupTimer.start(qMax(
                        1,
                        m_access.holdReleaseWarmupTimeoutMs
                    ));
                    setStatus(
                        tr8("正在启动麦克风"),
                        tr8("收到音频后将自动结束录音")
                    );
                    logRuntimeEvent(
                        tr8("录音"),
                        tr8("等待麦克风首帧"),
                        QStringLiteral("功能=") + id
                    );
                }
                return true;
            }
            stopAndProcess();
            return true;
        }
        return false;
    }

    bool handleFlowHotkeyReleased(const QString &functionId)
    {
        return m_flow.active
            && handleHotkeyReleased(functionId);
    }

    bool confirmActiveRecording()
    {
        if (!m_recordingLifecycle.isRecording()) {
            return false;
        }
        stopAndProcess();
        return true;
    }

    bool cancelActiveRecording()
    {
        if (m_flow.active) {
            if (!m_recordingCoordinator.isPreparing()
                && !m_recordingLifecycle.isRecording()) {
                return false;
            }
            cancelActiveFlow();
            return true;
        }
        if (m_recordingCoordinator.isPreparing()) {
            ++m_operationGeneration;
            m_recordingCoordinator.cancelPreparation();
            cancelStreamingSession();
            clearRecordingActions();
            m_modeId.clear();
            m_coordinatorModeId.clear();
            if (m_bar) {
                m_bar->setStage(FloatingBarStage::Completed);
            }
            setStatus(tr8("已取消"), tr8("录音准备已取消"));
            hideBarLater();
            return true;
        }
        if (!m_recordingLifecycle.isRecording()) {
            return false;
        }
        ++m_operationGeneration;
        cancelStreamingSession();
        m_longRecognitionCoordinator.cancel();
        if (m_longRecordingSession.isActive()) {
            m_recordingLifecycle.stop();
            captureCurrentLongRecordingSegment();
            m_longRecordingSession.complete();
        } else {
            m_recordingCoordinator.stopNormal();
        }
        clearRecordingActions();
        setWaveformVisible(false);
        m_modeId.clear();
        m_coordinatorModeId.clear();
        if (m_bar) {
            m_bar->setStage(FloatingBarStage::Completed);
        }
        setStatus(tr8("已取消"), tr8("录音已取消，未进行识别"));
        hideBarLater();
        return true;
    }

    bool isBusy() const
    {
        return m_flow.active
            || m_classicRecognitionRunning
            || m_recordingCoordinator.isPreparing()
            || m_recordingLifecycle.isRecording()
            || m_longRecordingSession.isActive()
            || m_longRecognitionCoordinator.isRunning();
    }

    bool isPreparing() const
    {
        return m_recordingCoordinator.isPreparing();
    }

    bool isRecording() const
    {
        return m_recordingLifecycle.isRecording();
    }

    QString lastWavPath() const
    {
        return m_recordingCapture.lastWavPath();
    }

private:
    void configureLifecycleCallbacks()
    {
        VoiceRecordingLifecycleCallbacks callbacks;
        callbacks.waveformTick = [this]() {
            if (m_bar) {
                m_bar->setWaveformLevel(
                    m_recordingCapture.takePeakLevel()
                );
            }
        };
        callbacks.segmentElapsed = [this]() {
            rotateLongRecordingSegment();
        };
        callbacks.limitElapsed = [this]() {
            if (!m_longRecordingSession.isActive()) {
                return;
            }
            logRuntimeEvent(
                tr8("长录音"),
                tr8("达到最长时间"),
                QStringLiteral("功能=") + m_modeId,
                elapsedMs()
            );
            stopLongRecordingAndProcess();
        };
        m_recordingLifecycle.setCallbacks(callbacks);
    }

    void configureCoordinatorCallbacks()
    {
        VoiceRecordingCoordinatorCallbacks callbacks;
        callbacks.countdownTick = [this](
            const QString &id,
            int seconds
        ) {
            if (id == m_coordinatorModeId) {
                if (cancelFlowIfRequested()) {
                    return;
                }
                setStatus(
                    tr8("准备录音"),
                    tr8("%1 秒后开始").arg(seconds)
                );
            }
        };
        callbacks.beepRequested = [this](const QString &id) {
            if (id == m_coordinatorModeId) {
                if (cancelFlowIfRequested()) {
                    return;
                }
                setStatus(tr8("准备录音"), tr8("提示音后开始"));
                playRecordingBeep(m_modeId);
            }
        };
        callbacks.started = [this](
            const QString &id,
            bool longRecording
        ) {
            if (id == m_coordinatorModeId) {
                if (cancelFlowIfRequested()) {
                    return;
                }
                handleRecordingStarted(m_modeId, longRecording);
            }
        };
        callbacks.startFailed = [this](
            const QString &id,
            const QString &error
        ) {
            if (id == m_coordinatorModeId) {
                handleRecordingStartFailed(m_modeId, error);
            }
        };
        m_recordingCoordinator.setCallbacks(callbacks);
    }

    void configureRecognitionCallbacks()
    {
        VoiceLongRecordingRecognitionCallbacks callbacks;
        callbacks.segmentStarted = [this](
            int index,
            int attempt,
            const QString &provider
        ) {
            logRuntimeEvent(
                tr8("长录音"),
                tr8("分段识别开始"),
                QStringLiteral("段号=") + QString::number(index)
                    + QStringLiteral("，尝试=")
                    + QString::number(attempt)
                    + QStringLiteral("，服务=")
                    + provider,
                elapsedMs()
            );
        };
        callbacks.segmentFinished = [this](
            const VoiceLongRecordingSegmentResult &result
        ) {
            if (isWindowsSpeechConfigurationErrorCode(result.errorCode)) {
                showWindowsSpeechFailure(
                    result.errorCode,
                    result.error
                );
                m_windowsStructuredTerminalHandled = true;
            }
            if (m_flow.active) {
                if (cancelFlowIfRequested()) {
                    return;
                }
                if (result.elapsedMs >= 0) {
                    m_flow.speechElapsedMs =
                        qMax<qint64>(0, m_flow.speechElapsedMs)
                        + result.elapsedMs;
                }
                bool onlyEmpty =
                    !result.attemptResults.isEmpty();
                for (const VoiceSpeechRecognitionResult &attempt :
                     result.attemptResults) {
                    if (!attempt.emptyRecognition) {
                        onlyEmpty = false;
                    }
                }
                m_flow.longRecognitionSawResult = true;
                m_flow.longRecognitionOnlyEmpty =
                    m_flow.longRecognitionOnlyEmpty && onlyEmpty;
            } else if (m_runSession && result.elapsedMs >= 0) {
                m_runSession->addSpeechElapsedMs(result.elapsedMs);
            }
            logRuntimeEvent(
                tr8("长录音"),
                result.text.trimmed().isEmpty()
                    ? tr8("分段识别失败")
                    : tr8("分段识别完成"),
                QStringLiteral("段号=") + QString::number(result.index)
                    + QStringLiteral("，字数=")
                    + QString::number(result.text.size())
                    + (!result.error.trimmed().isEmpty()
                        ? QStringLiteral("，错误=") + result.error
                        : QString()),
                result.elapsedMs
            );
        };
        callbacks.allFinished = [this]() {
            finishLongRecordingRecognition();
        };
        m_longRecognitionCoordinator.setCallbacks(callbacks);
    }

    const FunctionSettings &functionSettings(const QString &id) const
    {
        return m_settings.function(id);
    }

    bool longRecordingEnabledFor(const QString &id) const
    {
        if (m_flow.active && id == m_flow.functionId) {
            return m_flow.recording.longRecordingEnabled;
        }
        return functionSettings(id).recording.longRecordingEnabled;
    }

    QString recordingTriggerModeFor(const QString &id) const
    {
        if (m_flow.active && id == m_flow.functionId) {
            return m_flow.effectiveTriggerMode;
        }
        if (id == m_modeId
            && !m_classicEffectiveTriggerMode.isEmpty()) {
            return m_classicEffectiveTriggerMode;
        }
        return functionSettings(id).recording.triggerMode;
    }

    int countdownSecondsFor(const QString &id) const
    {
        if (m_flow.active && id == m_flow.functionId) {
            return m_flow.recording.countdownSeconds;
        }
        return functionSettings(id).recording.countdownSeconds;
    }

    int segmentSecondsFor(const QString &id) const
    {
        if (m_flow.active && id == m_flow.functionId) {
            return m_flow.recording.segmentSeconds;
        }
        return functionSettings(id).recording.segmentSeconds;
    }

    int maxRecordingMinutesFor(const QString &id) const
    {
        if (m_flow.active && id == m_flow.functionId) {
            return m_flow.recording.maximumMinutes;
        }
        return functionSettings(id).recording.maximumMinutes;
    }

    bool recordingBeepEnabledFor(const QString &id) const
    {
        if (m_flow.active && id == m_flow.functionId) {
            return m_flow.recording.beepEnabled;
        }
        return functionSettings(id).recording.beepEnabled;
    }

    QString recordingBeepPathFor(const QString &id) const
    {
        if (m_flow.active && id == m_flow.functionId) {
            return m_flow.recording.beepPath;
        }
        return functionSettings(id).recording.beepPath;
    }

    bool usesHoldToTalk(const QString &id) const
    {
        if (m_flow.active && id == m_flow.functionId) {
            return m_flow.effectiveTriggerMode == QStringLiteral("hold");
        }
        return recordingTriggerModeFor(id) == QStringLiteral("hold")
            && m_activeHoldFunctions.contains(id);
    }

    QString functionTitle(const QString &id) const
    {
        if (m_flow.active && id == m_flow.functionId) {
            return m_flow.functionTitle;
        }
        return functionDisplayTitle(
            m_settings,
            id,
            tr8("自定义功能")
        );
    }

    QString recordDirectoryPath() const
    {
        if (m_flow.active) {
            return m_flow.recordDirectory;
        }
        return historyRootPath(m_settings.recordDirectory);
    }

    bool shouldPlayRecordingBeep(const QString &id) const
    {
        return m_settings.recordingBeepEnabled
            && recordingBeepEnabledFor(id);
    }

    void playRecordingBeep(const QString &id)
    {
        const QString path = recordingBeepPathFor(id);
        const QString playablePath =
            !path.isEmpty() && QFileInfo(path).isFile()
                ? path
                : QString();
        if (m_access.playRecordingBeep) {
            m_access.playRecordingBeep(playablePath);
            return;
        }
        if (!playablePath.isEmpty()) {
            m_recordingBeep.stop();
            m_recordingBeep.setSource(QUrl::fromLocalFile(playablePath));
            m_recordingBeep.play();
            return;
        }
        QApplication::beep();
    }

    bool preparationMatchesCurrent(const QString &id) const
    {
        return id == m_modeId
            && m_recordingCoordinator.preparationMatchesMode(
                m_coordinatorModeId
            );
    }

    VoiceRecordingStartRequest buildFlowRecordingStartRequest() const
    {
        QString longRecordingAudioDirectory;
        QString longRecordingFileBase;
        if (m_flow.recording.longRecordingEnabled) {
            const QString date = QDate::currentDate().toString(
                QStringLiteral("yyyy-MM-dd")
            );
            ensureHistoryModeDateStructure(
                m_flow.recordDirectory,
                m_flow.functionTitle,
                date
            );
            longRecordingAudioDirectory =
                historyModeDateSubDirectory(
                    m_flow.recordDirectory,
                    m_flow.functionTitle,
                    date,
                    historyAudioSubFolderName()
                );
            longRecordingFileBase =
                QDateTime::currentDateTime().toString(
                    QStringLiteral("HHmmss_zzz")
                );
        }

        VoiceRecordingStartRequest request;
        request.normalTitle = m_flow.functionTitle;
        request.normalDirectory = m_flow.recordDirectory;
        request.longRecordingEnabled =
            m_flow.recording.longRecordingEnabled;
        request.firstSegmentTitle =
            m_flow.functionTitle + tr8("_第1段");
        request.longRecordingDirectory =
            longRecordingAudioDirectory;
        request.longRecordingFileBase = longRecordingFileBase;
        return request;
    }

    VoiceRecordingStartRequest buildRecordingStartRequest(
        const QString &id
    ) const
    {
        const bool longRecordingEnabled =
            longRecordingEnabledFor(id);
        QString longRecordingAudioDirectory;
        QString longRecordingFileBase;
        if (longRecordingEnabled) {
            const QString date = QDate::currentDate().toString(
                QStringLiteral("yyyy-MM-dd")
            );
            ensureHistoryModeDateStructure(
                recordDirectoryPath(),
                functionTitle(id),
                date
            );
            longRecordingAudioDirectory = historyModeDateSubDirectory(
                recordDirectoryPath(),
                functionTitle(id),
                date,
                historyAudioSubFolderName()
            );
            longRecordingFileBase = QDateTime::currentDateTime().toString(
                QStringLiteral("HHmmss_zzz")
            );
        }

        VoiceRecordingStartRequest request;
        request.normalTitle = functionTitle(id);
        request.normalDirectory = recordDirectoryPath();
        request.longRecordingEnabled = longRecordingEnabled;
        request.firstSegmentTitle =
            functionTitle(id) + tr8("_第1段");
        request.longRecordingDirectory =
            longRecordingAudioDirectory;
        request.longRecordingFileBase = longRecordingFileBase;
        return request;
    }

    void handleRecordingStartFailed(
        const QString &id,
        const QString &error
    )
    {
        clearRecordingActions();
        if (m_bar) {
            m_bar->setStage(FloatingBarStage::Failed);
        }
        clearPcmTracking();
        if (m_flow.active) {
            if (m_flow.cancellation.isCancellationRequested()) {
                cancelActiveFlow();
                return;
            }
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = flowVoiceError(
                QStringLiteral("flow_voice_failed"),
                error
            );
            finishFlow(result, m_flow.generation, m_flow.runId);
            return;
        }
        if (m_runSession) {
            m_runSession->setLongRecording(
                m_longRecordingSession.isActive()
            );
        }
        setWaveformVisible(false);
        logRuntimeEvent(
            tr8("录音"),
            tr8("启动失败"),
            QStringLiteral("功能=") + id
                + QStringLiteral("，错误=")
                + error,
            elapsedMs()
        );
        showFailure(error);
    }

    void preparePcmTracking()
    {
        m_holdReleaseWarmupTimer.stop();
        m_recordingReceivedPcm = false;
        m_holdReleasePending = false;

        const quint64 generation = m_operationGeneration;
        const QPointer<Impl> self(this);
        m_recordingCapture.setPcmListener(
            [self, generation](const QByteArray &pcm) {
                if (!self) {
                    return;
                }
                QCoreApplication::postEvent(
                    self.data(),
                    new StreamingPcmEvent(generation, pcm)
                );
            }
        );
    }

    void clearPcmTracking()
    {
        m_holdReleaseWarmupTimer.stop();
        m_recordingReceivedPcm = false;
        m_holdReleasePending = false;
        m_recordingCapture.setPcmListener(
            std::function<void(const QByteArray &)>()
        );
    }

    bool startStreamingForCurrentRecording()
    {
        m_streamingFinalTimer.stop();
        m_streamingSession.reset();
        m_streamingStoppedRecording = VoiceRecordingStopResult();
        m_streamingFinalizing = false;
        m_streamingTerminalHandled = false;
        m_streamingFallbackActive = false;
        m_windowsSpeechFailureShown = false;
        m_windowsStructuredTerminalHandled = false;
        if (!m_settings.streamingSpeechRecognitionEnabled
            || !m_access.createStreamingSpeechSession) {
            return true;
        }

        StreamingSpeechSessionRequest request;
        request.provider = m_flow.active
            ? m_flow.provider
            : m_settings.speechProvider;
        request.networkPolicy = m_flow.active
            ? m_flow.networkPolicy
            : QStringLiteral("inherit");
        request.useSystemProxy = m_flow.active
            ? m_flow.networkPolicy == QStringLiteral("systemProxy")
            : m_settings.useSystemProxy;
        if (request.provider == speechProviderWindowsLocal()) {
            request.language = normalizeWindowsSpeechLanguage(
                m_activeSpeechLanguage
            );
        }
        request.runId = QString::number(m_operationGeneration)
            + QLatin1Char('-')
            + QUuid::createUuid().toString().remove(
                QLatin1Char('{')
            ).remove(QLatin1Char('}'));

        const quint64 generation = m_operationGeneration;
        const QPointer<Impl> self(this);
        StreamingSpeechCallbacks callbacks;
        callbacks.transcriptUpdated = [self, generation](
            const StreamingTranscriptSnapshot &snapshot
        ) {
            if (!self) {
                return;
            }
            QTimer::singleShot(0, self.data(), [self, generation, snapshot]() {
                if (self) {
                    self->handleStreamingSnapshot(generation, snapshot);
                }
            });
        };
        callbacks.degraded = [self, generation](const QString &message) {
            if (!self) {
                return;
            }
            QTimer::singleShot(0, self.data(), [self, generation, message]() {
                if (self) {
                    self->handleStreamingDegraded(generation, message);
                }
            });
        };
        callbacks.completed = [self, generation](const QString &text) {
            if (!self) {
                return;
            }
            QTimer::singleShot(0, self.data(), [self, generation, text]() {
                if (self) {
                    self->handleStreamingCompleted(generation, text);
                }
            });
        };
        callbacks.configurationFailed = [self, generation](
            const QString &errorCode,
            const QString &message
        ) {
            if (!self) {
                return;
            }
            QTimer::singleShot(
                0,
                self.data(),
                [self, generation, errorCode, message]() {
                    if (self) {
                        self->handleStreamingConfigurationFailed(
                            generation,
                            errorCode,
                            message
                        );
                    }
                }
            );
        };

        const StreamingSpeechSessionCreation creation =
            m_access.createStreamingSpeechSession(request, callbacks);
        if (creation.session.isNull()) {
            if (!creation.unavailableReason.trimmed().isEmpty()) {
                logRuntimeEvent(
                    tr8("实时语音识别"),
                    tr8("使用整段识别"),
                    creation.unavailableReason
                );
            }
            return true;
        }

        QString error;
        if (!creation.session->start(&error)) {
            logRuntimeEvent(
                tr8("实时语音识别"),
                tr8("启动失败"),
                error
            );
            if (request.provider == speechProviderWindowsLocal()) {
                handleStreamingConfigurationFailed(
                    generation,
                    QStringLiteral(
                        "speech.windows.process_start_failed"
                    ),
                    error
                );
                return false;
            }
            return true;
        }
        m_streamingSession = creation.session;
        logRuntimeEvent(
            tr8("实时语音识别"),
            tr8("开始"),
            QStringLiteral("服务=") + request.provider
        );
        return true;
    }

    bool hasHealthyStreamingSession() const
    {
        return !m_streamingSession.isNull()
            && !m_streamingFallbackActive
            && !m_streamingTerminalHandled;
    }

    void handleStreamingSnapshot(
        quint64 generation,
        const StreamingTranscriptSnapshot &snapshot
    )
    {
        if (generation != m_operationGeneration
            || m_streamingFallbackActive
            || m_streamingTerminalHandled) {
            return;
        }
        if (m_bar) {
            m_bar->setStreamingTranscript(
                snapshot.committedText,
                snapshot.provisionalText
            );
        }
    }

    void handleStreamingConfigurationFailed(
        quint64 generation,
        const QString &errorCode,
        const QString &message
    )
    {
        if (generation != m_operationGeneration
            || m_streamingTerminalHandled
            || m_streamingFallbackActive) {
            return;
        }
        m_streamingTerminalHandled = true;
        m_streamingFinalTimer.stop();
        clearRecordingActions();
        clearPcmTracking();
        const QSharedPointer<IStreamingSpeechSession> session =
            m_streamingSession;
        m_streamingSession.reset();
        if (!session.isNull()
            && session->state() != StreamingSpeechState::Completed
            && session->state() != StreamingSpeechState::Cancelled) {
            session->cancel();
        }

        VoiceRecordingStopResult stopped;
        const bool wasLongRecording =
            m_longRecordingSession.isActive();
        if (m_recordingLifecycle.isRecording()) {
            if (m_longRecordingSession.isActive()) {
                m_recordingLifecycle.stop();
                captureCurrentLongRecordingSegment();
            } else {
                stopped = m_recordingCoordinator.stopNormal();
            }
        }
        if (!m_flow.active && m_runSession) {
            if (wasLongRecording) {
                const QVector<RecordingSegment> segments =
                    m_longRecordingSession
                        .recognitionState()
                        .segments();
                m_runSession->setLongRecording(true);
                m_runSession->setRecordingSegments(segments);
                if (!segments.isEmpty()) {
                    m_runSession->setRecordingAudioPath(
                        segments.first().wavPath
                    );
                }
            } else {
                m_runSession->setRecordingAudioPath(stopped.wavPath);
                m_runSession->setLongRecording(false);
            }
        }
        setWaveformVisible(false);
        if (m_bar) {
            m_bar->setStage(FloatingBarStage::Failed);
        }
        showWindowsSpeechFailure(errorCode, message);

        if (m_flow.active) {
            const quint64 flowGeneration = m_flow.generation;
            const ExecutionId runId = m_flow.runId;
            if (!wasLongRecording) {
                m_flow.normalRecording = stopped;
            }
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = flowVoiceError(errorCode, message);
            appendFlowObservation(currentFlowPayload(), &result);
            finishFlow(result, flowGeneration, runId);
            return;
        }

        m_longRecognitionCoordinator.cancel();
        m_longRecordingSession.complete();
        m_classicRecognitionRunning = false;
        m_modeId.clear();
        m_coordinatorModeId.clear();
        setProcessing(false);
    }

    void handleStreamingDegraded(
        quint64 generation,
        const QString &message
    )
    {
        if (generation != m_operationGeneration
            || m_streamingFallbackActive
            || m_streamingTerminalHandled) {
            return;
        }
        const bool wasFinalizing = m_streamingFinalizing;
        m_streamingFallbackActive = true;
        m_streamingFinalTimer.stop();
        const QSharedPointer<IStreamingSpeechSession> session =
            m_streamingSession;
        m_streamingSession.reset();
        if (!session.isNull()
            && session->state() != StreamingSpeechState::Degraded) {
            session->cancel();
        }
        if (m_bar) {
            m_bar->setStreamingFallback();
        }
        const QString provider = m_flow.active
            ? m_flow.provider
            : m_settings.speechProvider;
        if (provider == speechProviderWindowsLocal()) {
            setStatus(
                tr8("实时识别已中断，确认后将本地重新识别"),
                message
            );
        }
        logRuntimeEvent(
            tr8("实时语音识别"),
            tr8("自动降级"),
            message
        );
        if (!wasFinalizing) {
            if (m_longRecordingSession.isActive()) {
                startNextSegmentRecognition();
            }
            return;
        }
        if (provider == speechProviderWindowsLocal()) {
            setStatus(
                tr8("正在使用 Windows 本地语音识别重新识别"),
                QString()
            );
        }
        if (m_longRecordingSession.isFinalizing()) {
            startNextSegmentRecognition();
        } else if (m_flow.active) {
            beginFlowBatchRecognition(m_flow.generation, m_flow.runId);
        } else {
            beginClassicBatchRecognition(
                m_modeId,
                m_streamingStoppedRecording,
                generation
            );
        }
    }

    void handleStreamingCompleted(
        quint64 generation,
        const QString &text
    )
    {
        if (generation != m_operationGeneration
            || m_streamingFallbackActive
            || m_streamingTerminalHandled
            || !m_streamingFinalizing) {
            return;
        }
        const QString finalText = text.trimmed();
        if (finalText.isEmpty()) {
            handleStreamingDegraded(
                generation,
                tr8("实时识别没有返回文字。")
            );
            return;
        }
        m_streamingTerminalHandled = true;
        m_streamingFinalTimer.stop();
        m_recordingCapture.setPcmListener(
            std::function<void(const QByteArray &)>()
        );
        m_streamingSession.reset();
        if (m_bar) {
            m_bar->setStreamingTranscript(finalText, QString());
        }

        if (m_longRecordingSession.isFinalizing()) {
            completeLongRecordingFromStreaming(finalText);
        } else if (m_flow.active) {
            const QSharedPointer<const FunctionFlowVoicePayload> payload =
                flowPayload(
                    m_flow.normalRecording.wavPath,
                    QVector<RecordingSegment>(),
                    m_flow.speechElapsedMs,
                    false
                );
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Succeeded;
            result.values.append(flowValue(finalText, payload));
            finishFlow(result, m_flow.generation, m_flow.runId);
        } else {
            m_classicRecognitionRunning = false;
            processRecognizedSpeech(m_modeId, finalText);
            setProcessing(false);
        }
    }

    void cancelStreamingSession()
    {
        m_streamingFinalTimer.stop();
        clearPcmTracking();
        const QSharedPointer<IStreamingSpeechSession> session =
            m_streamingSession;
        m_streamingSession.reset();
        if (!session.isNull()
            && session->state() != StreamingSpeechState::Completed
            && session->state() != StreamingSpeechState::Cancelled) {
            session->cancel();
        }
        m_streamingTerminalHandled = true;
        m_streamingFinalizing = false;
    }

    void handleRecordingStarted(
        const QString &id,
        bool longRecordingActive
    )
    {
        if (m_flow.active
            && m_flow.cancellation.isCancellationRequested()) {
            cancelActiveFlow();
            return;
        }
        if (!m_flow.active && m_runSession) {
            m_runSession->setLongRecording(longRecordingActive);
        }
        if (!startStreamingForCurrentRecording()) {
            return;
        }
        installRecordingActions();
        setWaveformVisible(true);
        setTimedStatus(
            tr8("正在录音"),
            functionTitle(id)
                + (usesHoldToTalk(id)
                    ? tr8(" · 松开快捷键结束")
                    : tr8(" · 再次按同一快捷键结束"))
                + (longRecordingActive
                    ? tr8(" · 长录音分段识别")
                    : QString())
        );
        const QString triggerMode = !m_flow.active && m_runSession
            ? m_runSession->recordingTriggerMode()
            : recordingTriggerModeFor(id);
        logRuntimeEvent(
            tr8("录音"),
            tr8("开始"),
            QStringLiteral("功能=") + id
                + QStringLiteral("，方式=")
                + triggerMode
                + QStringLiteral("，长录音=")
                + (longRecordingActive
                    ? QStringLiteral("是")
                    : QStringLiteral("否"))
        );

        if (longRecordingActive) {
            if (!m_flow.active && m_runSession) {
                m_runSession->setSpeechElapsedMs(0);
            }
            return;
        }

        const quint64 generation = m_operationGeneration;
        QTimer::singleShot(60000, this, [this, id, generation]() {
            if (m_recordingLifecycle.isRecording()
                && m_modeId == id
                && m_operationGeneration == generation
                && !m_longRecordingSession.isActive()) {
                stopAndProcess();
            }
        });
    }

    void captureCurrentLongRecordingSegment()
    {
        if (!m_longRecordingSession.isActive()
            || m_longRecordingSession.currentSegmentIndex() <= 0) {
            return;
        }

        const VoiceRecordingSegmentCapture captured =
            m_recordingCapture.captureCurrentLongSegment(
                tr8("这一段没有录到声音数据。")
            );
        if (!captured.valid) {
            return;
        }
        logRuntimeEvent(
            tr8("长录音"),
            tr8("分段结束"),
            QStringLiteral("功能=") + m_modeId
                + QStringLiteral("，段号=")
                + QString::number(captured.index)
                + QStringLiteral("，PCM字节=")
                + QString::number(captured.pcm.size()),
            elapsedMs()
        );
    }

    void rotateLongRecordingSegment()
    {
        if (cancelFlowIfRequested()) {
            return;
        }
        if (!m_recordingLifecycle.isRecording()
            || !m_longRecordingSession.isActive()
            || m_longRecordingSession.isFinalizing()) {
            return;
        }

        captureCurrentLongRecordingSegment();
        const int nextSegmentIndex =
            m_longRecordingSession.currentSegmentIndex() + 1;
        const QString nextSegmentTitle =
            functionTitle(m_modeId)
            + tr8("_第")
            + QString::number(nextSegmentIndex)
            + tr8("段");
        const VoiceRecordingNextSegmentResult nextSegment =
            m_recordingCapture.startNextLongSegment(
                nextSegmentTitle,
                tr8("无法开始下一段录音：")
            );
        if (nextSegment.status
            == VoiceRecordingNextSegmentStatus::LimitReached) {
            m_recordingLifecycle.stop();
            setProcessing(true);
            m_longRecordingSession.beginFinalizing();
            setWaveformVisible(false);
            setTimedStatus(
                tr8("识别中"),
                tr8("已达到 33 段上限，正在等待分段识别完成")
            );
            logRuntimeEvent(
                tr8("长录音"),
                tr8("达到分段上限"),
                QStringLiteral("功能=") + m_modeId
                    + QStringLiteral("，分段数=33"),
                elapsedMs()
            );
            finishLongStreamingOrBatch();
            return;
        }
        if (nextSegment.status
            == VoiceRecordingNextSegmentStatus::StartFailed) {
            m_recordingLifecycle.stop();
            m_longRecordingSession.beginFinalizing();
            setProcessing(true);
            setWaveformVisible(false);
            logRuntimeEvent(
                tr8("长录音"),
                tr8("下一段启动失败"),
                QStringLiteral("功能=") + m_modeId
                    + QStringLiteral("，段号=")
                    + QString::number(nextSegment.index)
                    + QStringLiteral("，错误=")
                    + nextSegment.error,
                elapsedMs()
            );
            finishLongStreamingOrBatch();
            return;
        }

        setTimedStatus(
            tr8("正在录音"),
            functionTitle(m_modeId)
                + tr8(" · 第 ")
                + QString::number(
                    m_longRecordingSession.currentSegmentIndex()
                )
                + tr8(" 段")
        );
        m_recordingLifecycle.restartSegment(
            segmentSecondsFor(m_modeId) * 1000
        );
        if (!hasHealthyStreamingSession()) {
            startNextSegmentRecognition();
        }
    }

    void finishLongStreamingOrBatch()
    {
        m_recordingCapture.setPcmListener(
            std::function<void(const QByteArray &)>()
        );
        if (!hasHealthyStreamingSession()) {
            startNextSegmentRecognition();
            return;
        }
        m_streamingFinalizing = true;
        if (m_bar) {
            m_bar->setStreamingFinalizing();
        }
        m_streamingSession->finish();
        m_streamingFinalTimer.start(qMax(
            1,
            m_access.streamingFinalTimeoutMs
        ));
    }

    void stopAndProcess()
    {
        m_holdReleasePending = false;
        m_holdReleaseWarmupTimer.stop();
        if (m_flow.active) {
            stopFlowAndProcess();
            return;
        }
        if (externalProcessing()) {
            setStatus(tr8("正在处理"), tr8("请等待当前任务完成。"));
            return;
        }
        clearRecordingActions();
        if (m_bar) {
            m_bar->setStage(FloatingBarStage::Recognizing);
        }
        if (m_longRecordingSession.isActive()) {
            stopLongRecordingAndProcess();
            return;
        }

        const bool finishWithStreaming = hasHealthyStreamingSession();
        m_recordingCapture.setPcmListener(
            std::function<void(const QByteArray &)>()
        );
        setProcessing(true);
        const QString modeId = m_modeId;
        setWaveformVisible(false);
        if (m_runSession) {
            m_runSession->setActionHadRecording(true);
        }
        const bool willCallModel =
            modeId != QStringLiteral("dictate")
            || m_settings.dictatePolishEnabled;
        const QString speechService = speechProviderTitle(
            m_settings.speechProvider
        );
        setTimedStatus(
            tr8("识别中"),
            willCallModel
                ? tr8("正在使用")
                    + speechService
                    + tr8("识别语音并调用模型")
                : tr8("正在使用")
                    + speechService
                    + tr8("识别语音")
        );

        const quint64 generation = m_operationGeneration;
        const VoiceRecordingStopResult recording =
            m_recordingCoordinator.stopNormal();

        if (m_runSession) {
            m_runSession->setRecordingAudioPath(
                recording.wavPath
            );
            m_runSession->setRecordingTriggerMode(
                recordingTriggerModeFor(modeId)
            );
            m_runSession->setLongRecording(false);
        }
        logRuntimeEvent(
            tr8("录音"),
            tr8("结束"),
            QStringLiteral("功能=") + modeId
                + QStringLiteral("，PCM字节=")
                + QString::number(recording.pcm.size()),
            elapsedMs()
        );

        if (finishWithStreaming) {
            m_streamingStoppedRecording = recording;
            m_streamingFinalizing = true;
            m_classicRecognitionRunning = true;
            if (m_bar) {
                m_bar->setStreamingFinalizing();
            }
            m_streamingSession->finish();
            m_streamingFinalTimer.start(qMax(
                1,
                m_access.streamingFinalTimeoutMs
            ));
            return;
        }

        beginClassicBatchRecognition(modeId, recording, generation);
    }

    void beginClassicBatchRecognition(
        const QString &modeId,
        const VoiceRecordingStopResult &recording,
        quint64 generation
    )
    {
        if (m_streamingTerminalHandled && !m_streamingFallbackActive) {
            return;
        }

        VoiceSpeechRecognitionRequest request;
        request.modeId = modeId;
        request.audioData = recording.pcm;
        request.provider = m_settings.speechProvider;
        if (request.provider == speechProviderWindowsLocal()) {
            request.language = normalizeWindowsSpeechLanguage(
                m_activeSpeechLanguage
            );
        }
        request.networkPolicy = QStringLiteral("inherit");
        request.useSystemProxy = m_settings.useSystemProxy;
        const VoiceSpeechRecognitionHandlers handlers =
            speechRecognitionHandlers();
        m_classicRecognitionRunning = true;
        const QPointer<Impl> self(this);
        const VoiceRecordingFlowSpeechCompletion completion =
            [self, generation, modeId](
                const VoiceSpeechRecognitionResult &speech) {
                if (!self) {
                    return;
                }
                if (QThread::currentThread() != self->thread()) {
                    QTimer::singleShot(
                        0,
                        self.data(),
                        [self, generation, modeId, speech]() {
                            if (self) {
                                self->handleClassicSpeechFinished(
                                    generation,
                                    modeId,
                                    speech
                                );
                            }
                        }
                    );
                    return;
                }
                self->handleClassicSpeechFinished(
                    generation,
                    modeId,
                    speech
                );
            };
        if (m_access.runSpeechRecognition) {
            m_access.runSpeechRecognition(
                request,
                handlers,
                completion
            );
            return;
        }
        QFutureWatcher<VoiceSpeechRecognitionResult> *watcher =
            new QFutureWatcher<VoiceSpeechRecognitionResult>(this);
        connect(
            watcher,
            &QFutureWatcher<VoiceSpeechRecognitionResult>::finished,
            this,
            [watcher, completion]() {
                const VoiceSpeechRecognitionResult result =
                    watcher->result();
                watcher->deleteLater();
                completion(result);
            }
        );
        watcher->setFuture(QtConcurrent::run(
            [request, handlers]() {
                return VoiceSpeechRecognitionExecutor::run(
                    request,
                    handlers
                );
            }
        ));
    }

    void handleClassicSpeechFinished(
        quint64 generation,
        const QString &modeId,
        const VoiceSpeechRecognitionResult &speech
    )
    {
        if (!m_classicRecognitionRunning
            || generation != m_operationGeneration
            || modeId != m_modeId) {
            return;
        }
        m_classicRecognitionRunning = false;
        if (m_runSession) {
            m_runSession->setSpeechElapsedMs(speech.elapsedMs);
        }
        logRuntimeEvent(
            speech.logCategory,
            speech.logAction,
            speech.logDetail,
            elapsedMs()
        );

        if (!speech.ok) {
            if (isWindowsSpeechConfigurationErrorCode(speech.errorCode)) {
                showWindowsSpeechFailure(speech.errorCode, speech.error);
            } else {
                showFailure(speech.error);
            }
            saveFailureHistory(modeId, speech.error);
            setProcessing(false);
            return;
        }
        processRecognizedSpeech(modeId, speech.text);
        setProcessing(false);
    }

    void stopFlowAndProcess()
    {
        if (!m_flow.active) {
            return;
        }
        if (m_flow.cancellation.isCancellationRequested()) {
            cancelActiveFlow();
            return;
        }
        if (m_longRecordingSession.isActive()) {
            stopLongRecordingAndProcess();
            return;
        }
        clearRecordingActions();
        if (m_bar) {
            m_bar->setStage(FloatingBarStage::Recognizing);
        }

        const bool finishWithStreaming = hasHealthyStreamingSession();
        m_recordingCapture.setPcmListener(
            std::function<void(const QByteArray &)>()
        );
        const quint64 generation = m_flow.generation;
        const ExecutionId runId = m_flow.runId;
        m_flow.processing = true;
        setProcessing(true);
        setWaveformVisible(false);
        setTimedStatus(
            tr8("识别中"),
            tr8("正在识别流程语音")
        );
        m_flow.normalRecording =
            m_recordingCoordinator.stopNormal();
        logRuntimeEvent(
            tr8("录音"),
            tr8("结束"),
            QStringLiteral("功能=") + m_flow.functionId
                + QStringLiteral("，PCM字节=")
                + QString::number(
                    m_flow.normalRecording.pcm.size()
                ),
            elapsedMs()
        );

        if (m_flow.cancellation.isCancellationRequested()) {
            cancelActiveFlow();
            return;
        }

        if (finishWithStreaming) {
            m_streamingStoppedRecording = m_flow.normalRecording;
            m_streamingFinalizing = true;
            if (m_bar) {
                m_bar->setStreamingFinalizing();
            }
            m_streamingSession->finish();
            m_streamingFinalTimer.start(qMax(
                1,
                m_access.streamingFinalTimeoutMs
            ));
            return;
        }

        beginFlowBatchRecognition(generation, runId);
    }

    void beginFlowBatchRecognition(
        quint64 generation,
        const ExecutionId &runId
    )
    {
        if (!flowMatches(generation, runId)) {
            return;
        }
        if (m_flow.cancellation.isCancellationRequested()) {
            cancelActiveFlow();
            return;
        }

        VoiceSpeechRecognitionRequest request;
        request.modeId = m_flow.functionId;
        request.audioData = m_flow.normalRecording.pcm;
        request.provider = m_flow.provider;
        if (request.provider == speechProviderWindowsLocal()) {
            request.language = normalizeWindowsSpeechLanguage(
                m_activeSpeechLanguage
            );
        }
        request.networkPolicy = m_flow.networkPolicy;
        request.useSystemProxy =
            m_flow.networkPolicy == QStringLiteral("systemProxy");
        request.cancellation = m_flow.cancellation;
        const VoiceSpeechRecognitionHandlers handlers =
            speechRecognitionHandlers();
        const QPointer<Impl> self(this);
        const VoiceRecordingFlowSpeechCompletion completion =
            [self, generation, runId](
                const VoiceSpeechRecognitionResult &result) {
                if (!self) {
                    return;
                }
                if (QThread::currentThread() != self->thread()) {
                    QTimer::singleShot(
                        0,
                        self.data(),
                        [self, generation, runId, result]() {
                            if (self) {
                                self->handleFlowSpeechFinished(
                                    generation,
                                    runId,
                                    result
                                );
                            }
                        }
                    );
                    return;
                }
                self->handleFlowSpeechFinished(
                    generation,
                    runId,
                    result
                );
            };
        if (m_access.runSpeechRecognition) {
            m_access.runSpeechRecognition(
                request,
                handlers,
                completion
            );
            return;
        }
        QFutureWatcher<VoiceSpeechRecognitionResult> *watcher =
            new QFutureWatcher<VoiceSpeechRecognitionResult>(this);
        connect(
            watcher,
            &QFutureWatcher<VoiceSpeechRecognitionResult>::finished,
            this,
            [watcher, completion]() {
                const VoiceSpeechRecognitionResult result =
                    watcher->result();
                watcher->deleteLater();
                completion(result);
            }
        );
        watcher->setFuture(QtConcurrent::run(
            [request, handlers]() {
                return VoiceSpeechRecognitionExecutor::run(
                    request,
                    handlers
                );
            }
        ));
    }

    void handleFlowSpeechFinished(
        quint64 generation,
        const ExecutionId &runId,
        const VoiceSpeechRecognitionResult &speech
    )
    {
        if (!flowMatches(generation, runId)) {
            return;
        }
        m_flow.speechElapsedMs = speech.elapsedMs;
        logRuntimeEvent(
            speech.logCategory,
            speech.logAction,
            speech.logDetail,
            elapsedMs()
        );

        if (m_flow.cancellation.isCancellationRequested()
            || speech.cancelled) {
            cancelActiveFlow();
            return;
        }

        const QSharedPointer<const FunctionFlowVoicePayload> payload =
            flowPayload(
                m_flow.normalRecording.wavPath,
                QVector<RecordingSegment>(),
                speech.elapsedMs,
                false
            );
        FunctionFlowNodeResult result;
        if (speech.ok || speech.emptyRecognition) {
            result.state = FunctionFlowNodeState::Succeeded;
            result.values.append(flowValue(speech.text, payload));
        } else {
            if (isWindowsSpeechConfigurationErrorCode(speech.errorCode)) {
                showWindowsSpeechFailure(speech.errorCode, speech.error);
            }
            result.state = FunctionFlowNodeState::Failed;
            result.error = flowVoiceError(
                QStringLiteral("flow_voice_failed"),
                speech.error
            );
            appendFlowObservation(payload, &result);
        }
        finishFlow(result, generation, runId);
    }

    void stopLongRecordingAndProcess()
    {
        if (cancelFlowIfRequested()) {
            return;
        }
        if (!m_longRecordingSession.isActive()
            || m_longRecordingSession.isFinalizing()) {
            return;
        }

        clearRecordingActions();

        if (m_bar) {
            m_bar->setStage(FloatingBarStage::Recognizing);
        }

        m_recordingLifecycle.stop();
        setProcessing(true);
        if (m_flow.active) {
            m_flow.processing = true;
        }
        m_longRecordingSession.beginFinalizing();
        setWaveformVisible(false);
        if (!m_flow.active && m_runSession) {
            m_runSession->setActionHadRecording(true);
        }
        captureCurrentLongRecordingSegment();
        setTimedStatus(
            tr8("识别中"),
            tr8("正在等待所有录音分段识别完成")
        );
        logRuntimeEvent(
            tr8("长录音"),
            tr8("停止录音"),
            QStringLiteral("功能=") + m_modeId
                + QStringLiteral("，分段数=")
                + QString::number(
                    m_longRecordingSession
                        .recognitionState()
                        .segments()
                        .size()
                ),
            elapsedMs()
        );
        finishLongStreamingOrBatch();
    }

    VoiceSpeechRecognitionHandlers speechRecognitionHandlers() const
    {
        if (m_access.speechRecognition.recognizeProvider) {
            return m_access.speechRecognition;
        }
        VoiceSpeechRecognitionHandlers handlers;
        handlers.recognizeProvider = [](
            const SpeechRecognitionProviderTaskRequest &request
        ) {
            return runSpeechRecognitionProviderTask(request);
        };
        return handlers;
    }

    void startNextSegmentRecognition()
    {
        VoiceLongRecordingRecognitionConfig config;
        config.modeId = m_modeId;
        if (m_flow.active) {
            config.provider = m_flow.provider;
            config.networkPolicy = m_flow.networkPolicy;
            config.useSystemProxy =
                m_flow.networkPolicy == QStringLiteral("systemProxy");
            config.cancellation = m_flow.cancellation;
        } else {
            config.provider = m_settings.speechProvider;
            config.useSystemProxy = m_settings.useSystemProxy;
            config.networkPolicy = QStringLiteral("inherit");
        }
        if (config.provider == speechProviderWindowsLocal()) {
            config.language = normalizeWindowsSpeechLanguage(
                m_activeSpeechLanguage
            );
        }
        m_longRecognitionCoordinator.schedule(
            m_longRecordingSession,
            config,
            speechRecognitionHandlers()
        );
    }

    void completeLongRecordingFromStreaming(const QString &text)
    {
        if (!m_longRecordingSession.isFinalizing()) {
            return;
        }
        const VoiceLongRecordingBuildResult build =
            VoiceLongRecordingResultBuilder::build(
                m_longRecordingSession.recognitionState(),
                m_longRecordingSession.pcmBySegment()
            );
        QString audioPath;
        QString audioSaveError;
        if (!build.completePcm.isEmpty()) {
            audioPath = uniqueFilePath(
                QDir(m_longRecordingSession.audioDirectory()).filePath(
                    m_longRecordingSession.fileBase()
                        + tr8("_完整录音.wav")
                )
            );
            if (!writeBytesAtomically(
                    audioPath,
                    wavFromPcm(build.completePcm, 16000, 1, 16))) {
                audioSaveError = tr8("无法保存完整录音：") + audioPath;
                audioPath.clear();
            }
        }

        if (!audioSaveError.isEmpty()) {
            logRuntimeEvent(
                tr8("长录音"),
                tr8("完整录音保存失败"),
                audioSaveError,
                elapsedMs()
            );
        }

        if (m_flow.active) {
            const quint64 generation = m_flow.generation;
            const ExecutionId runId = m_flow.runId;
            const QSharedPointer<const FunctionFlowVoicePayload> payload =
                flowPayload(
                    audioPath,
                    build.segments,
                    m_flow.speechElapsedMs,
                    true
                );
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Succeeded;
            result.values.append(flowValue(text, payload));
            m_longRecordingSession.complete();
            finishFlow(result, generation, runId);
            return;
        }

        if (m_runSession) {
            m_runSession->setRecordingAudioPath(audioPath);
            m_runSession->setRecordingSegments(build.segments);
            m_runSession->setSpeechElapsedMs(0);
        }
        m_longRecordingSession.complete();
        processRecognizedSpeech(m_modeId, text);
        setProcessing(false);
    }

    void finishLongRecordingRecognition()
    {
        if (cancelFlowIfRequested()) {
            return;
        }
        if (!m_longRecordingSession.isFinalizing()) {
            return;
        }

        VoiceLongRecordingCompletionRequest request;
        request.state = &m_longRecordingSession.recognitionState();
        request.segmentPcm = m_longRecordingSession.pcmBySegment();
        request.audioDirectory =
            m_longRecordingSession.audioDirectory();
        request.fileBase = m_longRecordingSession.fileBase();
        VoiceLongRecordingCompletionHandlers handlers;
        handlers.saveCompleteAudio = [](
            const QByteArray &completePcm,
            const QString &audioDirectory,
            const QString &fileBase,
            QString *error
        ) {
            const QString path = uniqueFilePath(
                QDir(audioDirectory).filePath(
                    fileBase + tr8("_完整录音.wav")
                )
            );
            if (!writeBytesAtomically(
                    path,
                    wavFromPcm(completePcm, 16000, 1, 16))) {
                if (error) {
                    *error = tr8("无法保存完整录音：") + path;
                }
                return QString();
            }
            return path;
        };
        const VoiceLongRecordingCompletionResult result =
            VoiceLongRecordingCompletionExecutor::run(
                request,
                handlers
            );
        if (m_flow.active) {
            QVector<RecordingSegment> segments =
                result.build.segments;
            const bool normalEmptyRecognition =
                m_flow.longRecognitionSawResult
                && m_flow.longRecognitionOnlyEmpty
                && result.build.successfulSegmentCount <= 0;
            if (normalEmptyRecognition) {
                for (RecordingSegment &segment : segments) {
                    segment.error.clear();
                }
            }
            const QSharedPointer<const FunctionFlowVoicePayload>
                payload = flowPayload(
                    result.audioPath,
                    segments,
                    m_flow.speechElapsedMs,
                    true
                );
            FunctionFlowNodeResult flowResult;
            if (result.ok || normalEmptyRecognition) {
                flowResult.state =
                    FunctionFlowNodeState::Succeeded;
                flowResult.values.append(flowValue(
                    normalEmptyRecognition
                        ? QString()
                        : result.build.mergedText,
                    payload
                ));
            } else {
                flowResult.state = FunctionFlowNodeState::Failed;
                flowResult.error = flowVoiceError(
                    QStringLiteral("flow_voice_failed"),
                    result.error
                );
                appendFlowObservation(payload, &flowResult);
            }
            finishFlow(
                flowResult,
                m_flow.generation,
                m_flow.runId
            );
            return;
        }
        if (m_runSession) {
            m_runSession->setRecordingAudioPath(result.audioPath);
            m_runSession->setRecordingSegments(
                result.build.segments
            );
        }
        if (!result.audioSaveError.isEmpty()) {
            logRuntimeEvent(
                tr8("长录音"),
                tr8("完整录音保存失败"),
                result.audioSaveError,
                elapsedMs()
            );
        }
        m_longRecordingSession.complete();

        if (!result.ok) {
            if (!m_windowsStructuredTerminalHandled) {
                showFailure(result.error);
            }
            saveFailureHistory(m_modeId, result.error);
            setProcessing(false);
            return;
        }

        const QString mergedText = result.build.mergedText;
        logRuntimeEvent(
            tr8("长录音"),
            tr8("分段合并完成"),
            QStringLiteral("功能=") + m_modeId
                + QStringLiteral("，成功=")
                + QString::number(
                    result.build.successfulSegmentCount
                )
                + QStringLiteral("，失败=")
                + QString::number(
                    result.build.failedSegmentCount
                )
                + QStringLiteral("，字数=")
                + QString::number(mergedText.size()),
            elapsedMs()
        );
        processRecognizedSpeech(m_modeId, mergedText);
        setProcessing(false);
    }

    bool flowMatches(
        quint64 generation,
        const ExecutionId &runId
    ) const
    {
        return m_flow.active
            && m_flow.generation == generation
            && m_flow.runId == runId;
    }

    QSharedPointer<const FunctionFlowVoicePayload> flowPayload(
        const QString &sourceAudioPath,
        const QVector<RecordingSegment> &segments,
        qint64 speechElapsedMs,
        bool longRecording
    ) const
    {
        FunctionFlowVoicePayload *payload =
            new FunctionFlowVoicePayload;
        payload->sourceAudioPath = sourceAudioPath;
        payload->segments = segments;
        payload->speechElapsedMs = speechElapsedMs;
        payload->recordingTriggerMode =
            m_flow.effectiveTriggerMode;
        payload->longRecording = longRecording;
        return QSharedPointer<const FunctionFlowVoicePayload>(
            payload
        );
    }

    FunctionFlowValue flowValue(
        const QString &text,
        const QSharedPointer<const FunctionFlowVoicePayload> &payload
    ) const
    {
        FunctionFlowValue value;
        value.text = text;
        value.sourceNodeId = m_flow.nodeId;
        value.voice = payload;
        return value;
    }

    bool payloadHasControlledRecording(
        const QSharedPointer<const FunctionFlowVoicePayload> &payload
    ) const
    {
        return !payload.isNull()
            && (!payload->sourceAudioPath.trimmed().isEmpty()
                || !payload->segments.isEmpty());
    }

    void appendFlowObservation(
        const QSharedPointer<const FunctionFlowVoicePayload> &payload,
        FunctionFlowNodeResult *result
    ) const
    {
        if (!result || !payloadHasControlledRecording(payload)) {
            return;
        }
        result->historyObservations.append(
            flowValue(QString(), payload)
        );
    }

    QSharedPointer<const FunctionFlowVoicePayload>
    currentFlowPayload() const
    {
        if (m_flow.recording.longRecordingEnabled) {
            return flowPayload(
                QString(),
                m_longRecordingSession
                    .recognitionState()
                    .segments(),
                m_flow.speechElapsedMs,
                true
            );
        }
        return flowPayload(
            m_flow.normalRecording.wavPath,
            QVector<RecordingSegment>(),
            m_flow.speechElapsedMs,
            false
        );
    }

    bool cancelFlowIfRequested()
    {
        if (!m_flow.active
            || !m_flow.cancellation.isCancellationRequested()) {
            return false;
        }
        cancelActiveFlow();
        return true;
    }

    void cancelActiveFlow()
    {
        if (!m_flow.active) {
            return;
        }
        ++m_operationGeneration;
        clearRecordingActions();
        if (m_bar) {
            m_bar->setStage(FloatingBarStage::Completed);
        }
        cancelStreamingSession();
        const quint64 generation = m_flow.generation;
        const ExecutionId runId = m_flow.runId;
        m_recordingCoordinator.cancelPreparation();
        if (m_recordingLifecycle.isRecording()) {
            if (m_longRecordingSession.isActive()) {
                m_recordingLifecycle.stop();
                captureCurrentLongRecordingSegment();
            } else {
                m_flow.normalRecording =
                    m_recordingCoordinator.stopNormal();
            }
        }
        setWaveformVisible(false);

        FunctionFlowNodeResult result;
        result.state = FunctionFlowNodeState::Cancelled;
        appendFlowObservation(currentFlowPayload(), &result);
        finishFlow(result, generation, runId);
    }

    void finishFlow(
        const FunctionFlowNodeResult &result,
        quint64 generation,
        const ExecutionId &runId
    )
    {
        if (!flowMatches(generation, runId)) {
            return;
        }
        clearRecordingActions();
        cancelStreamingSession();
        const bool wasProcessing = m_flow.processing;
        const FunctionFlowNodeCompletion completion =
            m_flow.completion;
        m_flowCancellationTimer.stop();
        m_recordingCoordinator.cancelPreparation();
        m_longRecognitionCoordinator.cancel();
        m_longRecordingSession.complete();
        setWaveformVisible(false);
        m_flow = FlowState();
        m_modeId.clear();
        m_coordinatorModeId.clear();
        if (wasProcessing) {
            setProcessing(false);
        }
        if (completion) {
            completion(result);
        }
    }

    qint64 elapsedMs() const
    {
        if (m_access.elapsedMs) {
            return qMax<qint64>(0, m_access.elapsedMs());
        }
        return m_runSession
            ? qMax<qint64>(0, m_runSession->elapsedMs())
            : 0;
    }

    QString elapsedStatusText() const
    {
        const qint64 elapsed = elapsedMs();
        if (elapsed < 1000) {
            return tr8("已用时 ")
                + QString::number(elapsed)
                + tr8(" ms");
        }
        return tr8("已用时 ")
            + QString::number(elapsed / 1000.0, 'f', 1)
            + tr8(" 秒");
    }

    void setTimedStatus(
        const QString &title,
        const QString &detail
    )
    {
        setStatus(
            title,
            detail.trimmed().isEmpty()
                ? elapsedStatusText()
                : detail + tr8(" · ") + elapsedStatusText()
        );
    }

    void installPreparationActions()
    {
        if (!m_bar) {
            return;
        }
        const quint64 generation = m_operationGeneration;
        const QPointer<Impl> self(this);
        m_bar->setStage(
            FloatingBarStage::Preparing,
            tr8("准备录音"),
            QString()
        );
        FloatingBarActions actions;
        actions.cancel = [self, generation]() {
            if (self && self->m_operationGeneration == generation) {
                self->cancelActiveRecording();
            }
        };
        m_bar->setActions(actions);
    }

    void installRecordingActions()
    {
        if (!m_bar) {
            return;
        }
        const quint64 generation = m_operationGeneration;
        const QPointer<Impl> self(this);
        FloatingBarActions actions;
        actions.cancel = [self, generation]() {
            if (self && self->m_operationGeneration == generation) {
                self->cancelActiveRecording();
            }
        };
        actions.confirm = [self, generation]() {
            if (self && self->m_operationGeneration == generation) {
                self->confirmActiveRecording();
            }
        };
        m_bar->setActions(actions);
    }

    void clearRecordingActions()
    {
        if (m_bar) {
            m_bar->setActions(FloatingBarActions());
        }
    }

    void setStatus(
        const QString &title,
        const QString &detail
    )
    {
        if (m_bar) {
            m_bar->setStatus(title, detail);
        }
    }

    void hideBarLater()
    {
        if (m_bar) {
            m_bar->hideLater();
        }
    }

    void setWaveformVisible(bool visible)
    {
        if (m_bar) {
            m_bar->setWaveformVisible(visible);
        }
    }

    bool externalProcessing() const
    {
        return m_access.externalProcessing
            && m_access.externalProcessing();
    }

    void setProcessing(bool processing)
    {
        if (m_flow.active && processing) {
            m_flow.processing = true;
        }
        if (m_access.processingChanged) {
            m_access.processingChanged(processing);
        }
    }

    void showFailure(const QString &error)
    {
        if (m_access.showFailure) {
            m_access.showFailure(error);
        }
    }

    void showWindowsSpeechFailure(
        const QString &errorCode,
        const QString &message
    )
    {
        if (m_windowsSpeechFailureShown) {
            return;
        }
        m_windowsSpeechFailureShown = true;
        if (m_access.showWindowsSpeechFailure) {
            m_access.showWindowsSpeechFailure(errorCode, message);
            return;
        }
        showFailure(message);
    }

    void saveFailureHistory(
        const QString &modeId,
        const QString &error
    )
    {
        if (m_access.saveFailureHistory) {
            m_access.saveFailureHistory(modeId, error);
        }
    }

    void processRecognizedSpeech(
        const QString &modeId,
        const QString &text
    )
    {
        if (m_access.processRecognizedSpeech) {
            m_access.processRecognizedSpeech(modeId, text);
        }
    }

    VoiceRecordingWorkflowAccess m_access;
    FloatingBar *m_bar = nullptr;
    VoiceRunSession *m_runSession = nullptr;
    AppSettingsData m_settings;
    VoiceAudioRecorderAdapter m_audioRecorderAdapter;
    VoiceRecordingCapture m_recordingCapture;
    VoiceRecordingLifecycle m_recordingLifecycle;
    VoiceRecordingCoordinator m_recordingCoordinator;
    QSoundEffect m_recordingBeep;
    VoiceLongRecordingRecognitionCoordinator
        m_longRecognitionCoordinator;
    VoiceLongRecordingSession &m_longRecordingSession;
    QSet<QString> m_activeHoldFunctions;
    FlowState m_flow;
    QTimer m_flowCancellationTimer;
    QTimer m_streamingFinalTimer;
    QTimer m_holdReleaseWarmupTimer;
    QSharedPointer<IStreamingSpeechSession> m_streamingSession;
    VoiceRecordingStopResult m_streamingStoppedRecording;
    bool m_streamingFinalizing = false;
    bool m_streamingTerminalHandled = false;
    bool m_streamingFallbackActive = false;
    bool m_windowsSpeechFailureShown = false;
    bool m_windowsStructuredTerminalHandled = false;
    bool m_recordingReceivedPcm = false;
    bool m_holdReleasePending = false;
    quint64 m_operationGeneration = 0;
    bool m_classicRecognitionRunning = false;
    QString m_modeId;
    QString m_coordinatorModeId;
    QString m_classicEffectiveTriggerMode;
    QString m_activeSpeechLanguage = QStringLiteral("follow-windows");
};

VoiceRecordingWorkflowController::VoiceRecordingWorkflowController(
    const VoiceRecordingWorkflowAccess &access,
    FloatingBar *bar,
    VoiceRunSession *runSession,
    QObject *parent
)
    : QObject(parent),
      d(new Impl(access, bar, runSession, this))
{
}

VoiceRecordingWorkflowController::~VoiceRecordingWorkflowController()
{
    delete d;
    d = nullptr;
}

void VoiceRecordingWorkflowController::updateConfiguration(
    const AppSettingsData &settings
)
{
    d->updateConfiguration(settings);
}

void VoiceRecordingWorkflowController::setActiveHoldFunctions(
    const QSet<QString> &ids
)
{
    d->setActiveHoldFunctions(ids);
}

bool VoiceRecordingWorkflowController::begin(
    const QString &functionId
)
{
    return d->begin(functionId);
}

bool VoiceRecordingWorkflowController::beginForFlow(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowNodeCompletion &completion
)
{
    return d->beginForFlow(run, node, completion);
}

bool VoiceRecordingWorkflowController::handleHotkey(
    const QString &functionId
)
{
    return d->handleHotkey(functionId);
}

bool VoiceRecordingWorkflowController::ownsPress(
    const QString &functionId) const
{
    return d->ownsPress(functionId);
}

bool VoiceRecordingWorkflowController::handleFlowHotkeyReleased(
    const QString &functionId
)
{
    return d->handleFlowHotkeyReleased(functionId);
}

bool VoiceRecordingWorkflowController::handleHotkeyReleased(
    const QString &functionId
)
{
    return d->handleHotkeyReleased(functionId);
}

bool VoiceRecordingWorkflowController::confirmActiveRecording()
{
    return d->confirmActiveRecording();
}

bool VoiceRecordingWorkflowController::cancelActiveRecording()
{
    return d->cancelActiveRecording();
}

bool VoiceRecordingWorkflowController::isBusy() const
{
    return d->isBusy();
}

bool VoiceRecordingWorkflowController::isPreparing() const
{
    return d->isPreparing();
}

bool VoiceRecordingWorkflowController::isRecording() const
{
    return d->isRecording();
}

QString VoiceRecordingWorkflowController::lastWavPath() const
{
    return d->lastWavPath();
}
