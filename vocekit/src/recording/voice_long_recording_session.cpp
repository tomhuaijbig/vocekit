#include "voice_long_recording_session.h"

void VoiceLongRecordingSession::begin(
    bool enabled,
    const QString &audioDirectory,
    const QString &fileBase
)
{
    m_recognitionState.clear();
    m_pcmBySegment.clear();
    m_active = enabled;
    m_finalizing = false;
    m_currentSegmentIndex = enabled ? 1 : 0;
    m_audioDirectory = enabled ? audioDirectory : QString();
    m_fileBase = enabled ? fileBase : QString();
}

void VoiceLongRecordingSession::disable()
{
    m_active = false;
    m_finalizing = false;
    m_currentSegmentIndex = 0;
}

bool VoiceLongRecordingSession::isActive() const
{
    return m_active;
}

bool VoiceLongRecordingSession::isFinalizing() const
{
    return m_finalizing;
}

int VoiceLongRecordingSession::currentSegmentIndex() const
{
    return m_currentSegmentIndex;
}

QString VoiceLongRecordingSession::audioDirectory() const
{
    return m_audioDirectory;
}

QString VoiceLongRecordingSession::fileBase() const
{
    return m_fileBase;
}

void VoiceLongRecordingSession::addCurrentSegment(
    const QString &wavPath,
    const QByteArray &pcm
)
{
    if (!m_active || m_currentSegmentIndex <= 0) {
        return;
    }
    m_recognitionState.addSegment(m_currentSegmentIndex, wavPath);
    m_pcmBySegment.insert(m_currentSegmentIndex, pcm);
}

void VoiceLongRecordingSession::recordCurrentTerminalFailure(
    const QString &error,
    qint64 recognitionElapsedMs
)
{
    if (!m_active || m_currentSegmentIndex <= 0) {
        return;
    }
    m_recognitionState.recordTerminalFailure(
        m_currentSegmentIndex,
        error,
        recognitionElapsedMs
    );
}

bool VoiceLongRecordingSession::advanceToNextSegment()
{
    if (!m_active
        || m_finalizing
        || !canStartRecordingSegment(m_currentSegmentIndex + 1)) {
        return false;
    }
    ++m_currentSegmentIndex;
    return true;
}

bool VoiceLongRecordingSession::beginFinalizing()
{
    if (!m_active || m_finalizing) {
        return false;
    }
    m_finalizing = true;
    return true;
}

void VoiceLongRecordingSession::complete()
{
    m_pcmBySegment.clear();
    m_active = false;
    m_finalizing = false;
    m_currentSegmentIndex = 0;
}

SegmentedRecordingState &VoiceLongRecordingSession::recognitionState()
{
    return m_recognitionState;
}

const SegmentedRecordingState &VoiceLongRecordingSession::recognitionState() const
{
    return m_recognitionState;
}

const QMap<int, QByteArray> &VoiceLongRecordingSession::pcmBySegment() const
{
    return m_pcmBySegment;
}
