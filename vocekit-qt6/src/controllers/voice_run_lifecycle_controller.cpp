#include "voice_run_lifecycle_controller.h"

#include "../domain/voice_run_formatter.h"
#include "../domain/voice_run_session.h"

namespace {

QString lifecycleText(const char *text)
{
    return QString::fromUtf8(text);
}

bool contextHasVoiceInput(const VoiceRunContext &context)
{
    return !context.textOnly
        && !context.voiceText.trimmed().isEmpty();
}

VoiceRunSessionSnapshot runSnapshot(const VoiceRunSession *session)
{
    return session
        ? session->snapshot()
        : VoiceRunSessionSnapshot();
}

} // namespace

VoiceRunLifecycleController::VoiceRunLifecycleController(
    const VoiceRunLifecycleAccess &access,
    VoiceRunSession *runSession,
    QObject *parent
)
    : QObject(parent),
      m_access(access),
      m_runSession(runSession)
{
}

VoiceRunLifecycleController::~VoiceRunLifecycleController()
{
    cancelActiveModel();
}

void VoiceRunLifecycleController::updateConfiguration(
    const AppSettingsData &settings
)
{
    m_settings = settings;
}

void VoiceRunLifecycleController::cancelActiveModel()
{
    m_modelCancellation.cancel();
}

bool VoiceRunLifecycleController::lastModelRunCancelled() const
{
    return m_lastModelRunCancelled;
}

QString VoiceRunLifecycleController::runModel(
    const VoiceRunContext &context,
    const QString &modelOverride,
    const QString &extraInstruction,
    QString *error,
    const VoiceRunDeltaCallback &onDelta
)
{
    m_modelCancellation.cancel();
    m_modelCancellation = CancellationSource();
    m_lastModelRunCancelled = false;

    VoiceRunExecutionRequest request;
    request.context = context;
    request.modelOverride = modelOverride;
    request.extraInstruction = extraInstruction;
    request.onDelta = onDelta;
    request.cancellation = m_modelCancellation.token();

    VoiceRunExecutionHandlers handlers;
    handlers.runtimeSettings = [this](const QString &modeId) {
        PromptRuntimeSnapshot prompts;
        if (m_access.promptSnapshot) {
            prompts = m_access.promptSnapshot();
        } else {
            prompts.settings = m_settings;
        }
        return m_access.runtimeSettings
            ? m_access.runtimeSettings(m_settings, prompts, modeId)
            : VoiceModelRuntimeSettings();
    };
    handlers.vocabularyPromptBlockBuilder = [this](
        const QString &modeId,
        const QString &userText,
        bool hasVoiceInput
    ) {
        return m_access.vocabularyPromptBlock
            ? m_access.vocabularyPromptBlock(
                m_settings,
                modeId,
                userText,
                hasVoiceInput
            )
            : QString();
    };
    handlers.processModelRequest = m_access.processModelRequest;
    handlers.modelResultRecorded = [this](
        qint64 durationMs,
        const QString &promptVersion
    ) {
        if (m_runSession) {
            m_runSession->setModelResult(
                durationMs,
                promptVersion
            );
        }
    };
    handlers.modelDetailsRecorded = [this](
        const VoiceModelProcessingResult &result
    ) {
        if (m_runSession) {
            m_runSession->setModelResult(result);
        }
    };

    const QString output = VoiceRunExecutor::run(
        request,
        handlers,
        error
    );
    m_lastModelRunCancelled =
        request.cancellation.isCancellationRequested();
    return output;
}

QString VoiceRunLifecycleController::finalizeOutput(
    const VoiceRunContext &context,
    const QString &output
) const
{
    return m_access.postCorrectOutput
        ? m_access.postCorrectOutput(
            m_settings,
            output,
            context.modeId,
            contextHasVoiceInput(context)
        )
        : output;
}

void VoiceRunLifecycleController::saveHistory(
    const VoiceRunContext &context,
    const QString &output,
    const QString &error,
    bool draft,
    const QString &modelOverride
)
{
    VoiceRunLifecycleHistoryRequest request;
    request.modeId = context.modeId;
    request.input = VoiceRunFormatter::historyInput(context);
    request.output = output;
    request.error = error;
    request.draft = draft;
    request.modelOverride = modelOverride;
    saveHistory(request);
}

void VoiceRunLifecycleController::saveHistory(
    const VoiceRunLifecycleHistoryRequest &request
)
{
    const VoiceRunSessionSnapshot snapshot =
        runSnapshot(m_runSession);
    const QString fallbackAudioPath = m_access.fallbackAudioPath
        ? m_access.fallbackAudioPath()
        : QString();
    const QString model = request.modelOverride.trimmed().isEmpty()
        ? m_settings.function(request.modeId).modelId
        : request.modelOverride;

    VoiceHistorySaveRequest saveRequest;
    saveRequest.recordDirectory = m_access.resolveHistoryRoot
        ? m_access.resolveHistoryRoot(m_settings.recordDirectory)
        : m_settings.recordDirectory;
    saveRequest.modeId = request.modeId;
    saveRequest.modeTitle = m_access.modeTitle
        ? m_access.modeTitle(m_settings, request.modeId)
        : request.modeId;
    saveRequest.sourceAudioPath =
        snapshot.sourceAudioPath(fallbackAudioPath);
    saveRequest.input = request.input;
    saveRequest.output = request.output;
    saveRequest.error = request.error;
    saveRequest.draft = request.draft;
    saveRequest.model = model;
    saveRequest.usedModel =
        !(request.modeId == QStringLiteral("dictate")
            && !m_settings.dictatePolishEnabled
            && request.modelOverride.trimmed().isEmpty());
    saveRequest.elapsedMs = snapshot.elapsedMs;
    saveRequest.speechElapsedMs = snapshot.speechElapsedMs;
    saveRequest.modelElapsedMs = snapshot.modelElapsedMs;
    saveRequest.promptVersion = snapshot.promptVersion;
    saveRequest.runContext = snapshot.runContext;
    saveRequest.actionHadRecording =
        snapshot.actionHadRecording;
    saveRequest.recordingTriggerMode =
        snapshot.recordingTriggerMode;
    saveRequest.longRecording = snapshot.longRecording;
    saveRequest.recordingSegments =
        snapshot.recordingSegments;

    VoiceHistorySaveResult result;
    if (m_access.persistHistory) {
        result = m_access.persistHistory(saveRequest);
    } else {
        result.logAction = lifecycleText("保存失败");
        result.logDetail =
            lifecycleText("历史写入器未配置。");
    }

    if (result.saved.ok && m_access.historySaved) {
        m_access.historySaved(result.saved.modeDetailPath);
    }
    if (m_access.historyLogged) {
        m_access.historyLogged(
            result.logAction,
            result.logDetail,
            m_access.elapsedMs
                ? m_access.elapsedMs()
                : snapshot.elapsedMs
        );
    }
}
