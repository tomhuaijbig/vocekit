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
#include "../tasks/voice_recording_completion_executor.h"
#include "../tasks/voice_speech_recognition_executor.h"
#include "../ui/floating_bar.h"

#include <QtMultimedia>
#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

class VoiceRecordingWorkflowController::Impl : public QObject
{
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
          m_recordingCapture(m_audioRecorderAdapter.handlers()),
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
    }

    ~Impl() override
    {
        m_recordingCoordinator.cancelPreparation();
        m_longRecognitionCoordinator.cancel();
        if (m_recordingLifecycle.isRecording()) {
            m_recordingLifecycle.stop();
            m_recordingCapture.stopNormal();
        }
        m_longRecordingSession.complete();
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

        m_modeId = id;
        m_longRecognitionCoordinator.reset();
        if (m_runSession) {
            m_runSession->setRecordingTriggerMode(
                recordingTriggerModeFor(id)
            );
        }

        const bool longRecordingEnabled =
            longRecordingEnabledFor(id);
        VoiceRecordingCoordinatorRequest request;
        request.modeId = id;
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
        m_recordingCoordinator.begin(request);
        return true;
    }

    bool handleHotkey(const QString &functionId)
    {
        const QString id = functionId.trimmed();
        if (m_recordingCoordinator.isPreparing()) {
            if (m_recordingCoordinator.preparationMatchesMode(id)) {
                if (usesHoldToTalk(id)) {
                    logRuntimeEvent(
                        tr8("快捷键"),
                        tr8("忽略自动重复"),
                        QStringLiteral("功能=") + id
                    );
                    return true;
                }
                m_recordingCoordinator.cancelPreparation();
                setStatus(tr8("已取消"), tr8("录音准备已取消"));
                hideBarLater();
                logRuntimeEvent(
                    tr8("录音"),
                    tr8("取消准备"),
                    QStringLiteral("功能=") + id
                );
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

    bool handleHotkeyReleased(const QString &functionId)
    {
        const QString id = functionId.trimmed();
        if (!usesHoldToTalk(id) || id != m_modeId) {
            return false;
        }
        if (m_recordingCoordinator.preparationMatchesMode(id)) {
            m_recordingCoordinator.cancelPreparation();
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
            return true;
        }
        if (m_recordingLifecycle.isRecording()) {
            logRuntimeEvent(
                tr8("录音"),
                tr8("按住说话松开"),
                QStringLiteral("功能=") + id,
                elapsedMs()
            );
            stopAndProcess();
            return true;
        }
        return false;
    }

    bool isBusy() const
    {
        return m_recordingCoordinator.isPreparing()
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
            if (id == m_modeId) {
                setStatus(
                    tr8("准备录音"),
                    tr8("%1 秒后开始").arg(seconds)
                );
            }
        };
        callbacks.beepRequested = [this](const QString &id) {
            if (id == m_modeId) {
                setStatus(tr8("准备录音"), tr8("提示音后开始"));
                playRecordingBeep(id);
            }
        };
        callbacks.started = [this](
            const QString &id,
            bool longRecording
        ) {
            if (id == m_modeId) {
                handleRecordingStarted(id, longRecording);
            }
        };
        callbacks.startFailed = [this](
            const QString &id,
            const QString &error
        ) {
            if (id == m_modeId) {
                handleRecordingStartFailed(id, error);
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
            if (m_runSession && result.elapsedMs >= 0) {
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
        return functionSettings(id).recording.longRecordingEnabled;
    }

    QString recordingTriggerModeFor(const QString &id) const
    {
        return functionSettings(id).recording.triggerMode;
    }

    int countdownSecondsFor(const QString &id) const
    {
        return functionSettings(id).recording.countdownSeconds;
    }

    int segmentSecondsFor(const QString &id) const
    {
        return functionSettings(id).recording.segmentSeconds;
    }

    int maxRecordingMinutesFor(const QString &id) const
    {
        return functionSettings(id).recording.maximumMinutes;
    }

    bool recordingBeepEnabledFor(const QString &id) const
    {
        return functionSettings(id).recording.beepEnabled;
    }

    QString recordingBeepPathFor(const QString &id) const
    {
        return functionSettings(id).recording.beepPath;
    }

    bool usesHoldToTalk(const QString &id) const
    {
        return recordingTriggerModeFor(id) == QStringLiteral("hold")
            && m_activeHoldFunctions.contains(id);
    }

    QString functionTitle(const QString &id) const
    {
        return functionDisplayTitle(
            m_settings,
            id,
            tr8("自定义功能")
        );
    }

    QString recordDirectoryPath() const
    {
        return historyRootPath(m_settings.recordDirectory);
    }

    bool shouldPlayRecordingBeep(const QString &id) const
    {
        return m_settings.recordingBeepEnabled
            && recordingBeepEnabledFor(id);
    }

    void playRecordingBeep(const QString &id) const
    {
        const QString path = recordingBeepPathFor(id);
        if (!path.isEmpty() && QFileInfo(path).isFile()) {
            QSound::play(path);
            return;
        }
        QApplication::beep();
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

    void handleRecordingStarted(
        const QString &id,
        bool longRecordingActive
    )
    {
        if (m_runSession) {
            m_runSession->setLongRecording(longRecordingActive);
        }
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
        const QString triggerMode = m_runSession
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
            if (m_runSession) {
                m_runSession->setSpeechElapsedMs(0);
            }
            return;
        }

        QTimer::singleShot(60000, this, [this, id]() {
            if (m_recordingLifecycle.isRecording()
                && m_modeId == id
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
            startNextSegmentRecognition();
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
            startNextSegmentRecognition();
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
        startNextSegmentRecognition();
    }

    void stopAndProcess()
    {
        if (externalProcessing()) {
            setStatus(tr8("正在处理"), tr8("请等待当前任务完成。"));
            return;
        }
        if (m_longRecordingSession.isActive()) {
            stopLongRecordingAndProcess();
            return;
        }

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

        VoiceRecordingCompletionRequest request;
        request.modeId = modeId;
        request.provider = m_settings.speechProvider;
        request.useSystemProxy = m_settings.useSystemProxy;
        request.networkPolicy = QStringLiteral("inherit");
        VoiceRecordingCompletionHandlers handlers;
        handlers.stopRecording = [this]() {
            return m_recordingCoordinator.stopNormal();
        };
        handlers.recognition = speechRecognitionHandlers();
        const VoiceRecordingCompletionResult result =
            VoiceRecordingCompletionExecutor::run(request, handlers);

        if (m_runSession) {
            m_runSession->setRecordingAudioPath(
                result.recording.wavPath
            );
            m_runSession->setRecordingTriggerMode(
                recordingTriggerModeFor(modeId)
            );
            m_runSession->setLongRecording(false);
            m_runSession->setSpeechElapsedMs(result.speech.elapsedMs);
        }
        logRuntimeEvent(
            tr8("录音"),
            tr8("结束"),
            QStringLiteral("功能=") + modeId
                + QStringLiteral("，PCM字节=")
                + QString::number(result.recording.pcm.size()),
            elapsedMs()
        );
        logRuntimeEvent(
            result.speech.logCategory,
            result.speech.logAction,
            result.speech.logDetail,
            elapsedMs()
        );

        if (!result.ok) {
            showFailure(result.error);
            saveFailureHistory(modeId, result.error);
            setProcessing(false);
            return;
        }
        processRecognizedSpeech(modeId, result.speech.text);
        setProcessing(false);
    }

    void stopLongRecordingAndProcess()
    {
        if (!m_longRecordingSession.isActive()
            || m_longRecordingSession.isFinalizing()) {
            return;
        }

        m_recordingLifecycle.stop();
        setProcessing(true);
        m_longRecordingSession.beginFinalizing();
        setWaveformVisible(false);
        if (m_runSession) {
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
        startNextSegmentRecognition();
    }

    VoiceSpeechRecognitionHandlers speechRecognitionHandlers() const
    {
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
        config.provider = m_settings.speechProvider;
        config.useSystemProxy = m_settings.useSystemProxy;
        config.networkPolicy = QStringLiteral("inherit");
        m_longRecognitionCoordinator.schedule(
            m_longRecordingSession,
            config,
            speechRecognitionHandlers()
        );
    }

    void finishLongRecordingRecognition()
    {
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
            showFailure(result.error);
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
    VoiceLongRecordingRecognitionCoordinator
        m_longRecognitionCoordinator;
    VoiceLongRecordingSession &m_longRecordingSession;
    QSet<QString> m_activeHoldFunctions;
    QString m_modeId;
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

bool VoiceRecordingWorkflowController::handleHotkey(
    const QString &functionId
)
{
    return d->handleHotkey(functionId);
}

bool VoiceRecordingWorkflowController::handleHotkeyReleased(
    const QString &functionId
)
{
    return d->handleHotkeyReleased(functionId);
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
