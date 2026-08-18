#include "voice_run_session.h"

#include "../tasks/voice_model_processing_task.h"

QString VoiceRunSessionSnapshot::sourceAudioPath(
    const QString &fallbackAudioPath
) const
{
    if (!actionHadRecording) {
        return QString();
    }
    return recordingAudioPath.trimmed().isEmpty()
        ? fallbackAudioPath
        : recordingAudioPath;
}

void VoiceRunSession::beginAction()
{
    restartTimer();
    resetRunState();
}

void VoiceRunSession::restartTimer()
{
    m_timer.restart();
}

void VoiceRunSession::beginTextInput()
{
    resetRecordingState();
    m_state.speechElapsedMs = -1;
    resetModelState();
}

void VoiceRunSession::beginModelAttempt()
{
    restartTimer();
    resetModelState();
}

qint64 VoiceRunSession::elapsedMs() const
{
    return m_timer.isValid() ? m_timer.elapsed() : -1;
}

VoiceRunSessionSnapshot VoiceRunSession::snapshot() const
{
    VoiceRunSessionSnapshot result = m_state;
    result.elapsedMs = elapsedMs();
    return result;
}

void VoiceRunSession::setActionHadRecording(bool value)
{
    m_state.actionHadRecording = value;
}

void VoiceRunSession::setSpeechElapsedMs(qint64 elapsedMs)
{
    m_state.speechElapsedMs = elapsedMs;
}

void VoiceRunSession::addSpeechElapsedMs(qint64 elapsedMs)
{
    if (elapsedMs < 0) {
        return;
    }
    m_state.speechElapsedMs = qMax<qint64>(
        0,
        m_state.speechElapsedMs
    ) + elapsedMs;
}

void VoiceRunSession::setModelResult(
    qint64 elapsedMs,
    const QString &promptVersion
)
{
    m_state.modelElapsedMs = elapsedMs;
    m_state.promptVersion = promptVersion;
}

void VoiceRunSession::setModelResult(
    const VoiceModelProcessingResult &result)
{
    setModelResult(result.durationMs, result.promptVersion);
    m_state.rawModelResponse = result.rawResponse;
    m_state.modelTelemetry = result.telemetry;
}

void VoiceRunSession::setRecordingAudioPath(const QString &path)
{
    m_state.recordingAudioPath = path;
}

void VoiceRunSession::setRecordingSegments(
    const QVector<RecordingSegment> &segments
)
{
    m_state.recordingSegments = segments;
}

void VoiceRunSession::setRecordingTriggerMode(const QString &mode)
{
    m_state.recordingTriggerMode = mode;
}

QString VoiceRunSession::recordingTriggerMode() const
{
    return m_state.recordingTriggerMode;
}

void VoiceRunSession::setLongRecording(bool value)
{
    m_state.longRecording = value;
}

void VoiceRunSession::setRunContext(const VoiceRunContext &context)
{
    m_state.runContext = context;
}

const VoiceRunContext &VoiceRunSession::runContext() const
{
    return m_state.runContext;
}

void VoiceRunSession::resetRunState()
{
    m_state = VoiceRunSessionSnapshot();
}

void VoiceRunSession::resetModelState()
{
    m_state.modelElapsedMs = -1;
    m_state.promptVersion.clear();
    m_state.rawModelResponse.clear();
    m_state.modelTelemetry = ModelRequestTelemetry();
}

void VoiceRunSession::resetRecordingState()
{
    m_state.actionHadRecording = false;
    m_state.recordingAudioPath.clear();
    m_state.recordingTriggerMode.clear();
    m_state.longRecording = false;
    m_state.recordingSegments.clear();
}
