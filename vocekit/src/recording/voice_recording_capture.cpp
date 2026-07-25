#include "voice_recording_capture.h"

namespace {

QString missingRecorderError()
{
    return QString::fromUtf8(
        "\xE5\xBD\x95\xE9\x9F\xB3\xE9\x87\x87\xE9\x9B\x86\xE6\xA8\xA1\xE5\x9D\x97\xE6\x9C\xAA\xE9\x85\x8D\xE7\xBD\xAE\xE3\x80\x82"
    );
}

} // namespace

VoiceRecordingCapture::VoiceRecordingCapture(
    const VoiceRecordingCaptureHandlers &handlers
)
    : m_handlers(handlers)
{
}

void VoiceRecordingCapture::setHandlers(
    const VoiceRecordingCaptureHandlers &handlers
)
{
    m_handlers = handlers;
}

bool VoiceRecordingCapture::begin(
    const VoiceRecordingStartRequest &request,
    QString *error
)
{
    m_longRecordingSession.begin(
        request.longRecordingEnabled,
        request.longRecordingDirectory,
        request.longRecordingFileBase
    );
    if (!m_handlers.start) {
        if (error) {
            *error = missingRecorderError();
        }
        m_longRecordingSession.disable();
        return false;
    }

    const QString title = request.longRecordingEnabled
        ? request.firstSegmentTitle
        : request.normalTitle;
    const QString directory = request.longRecordingEnabled
        ? request.longRecordingDirectory
        : request.normalDirectory;
    const bool started = m_handlers.start(
        title,
        directory,
        request.longRecordingEnabled,
        error
    );
    if (!started) {
        m_longRecordingSession.disable();
    }
    return started;
}

VoiceRecordingSegmentCapture
VoiceRecordingCapture::captureCurrentLongSegment(
    const QString &emptyAudioError
)
{
    VoiceRecordingSegmentCapture result;
    if (!m_longRecordingSession.isActive()
        || m_longRecordingSession.currentSegmentIndex() <= 0) {
        return result;
    }

    result.valid = true;
    result.index = m_longRecordingSession.currentSegmentIndex();
    result.pcm = m_handlers.stop ? m_handlers.stop() : QByteArray();
    result.wavPath = m_handlers.lastWavPath
        ? m_handlers.lastWavPath()
        : QString();
    m_longRecordingSession.addCurrentSegment(result.wavPath, result.pcm);
    if (result.pcm.isEmpty()) {
        m_longRecordingSession.recordCurrentTerminalFailure(
            emptyAudioError,
            0
        );
    }
    return result;
}

VoiceRecordingNextSegmentResult
VoiceRecordingCapture::startNextLongSegment(
    const QString &segmentTitle,
    const QString &startFailurePrefix
)
{
    VoiceRecordingNextSegmentResult result;
    if (!m_longRecordingSession.advanceToNextSegment()) {
        return result;
    }

    result.index = m_longRecordingSession.currentSegmentIndex();
    QString error;
    const bool started = m_handlers.start
        && m_handlers.start(
            segmentTitle,
            m_longRecordingSession.audioDirectory(),
            true,
            &error
        );
    if (started) {
        result.status = VoiceRecordingNextSegmentStatus::Started;
        return result;
    }

    if (error.trimmed().isEmpty()) {
        error = missingRecorderError();
    }
    result.status = VoiceRecordingNextSegmentStatus::StartFailed;
    result.error = error;
    m_longRecordingSession.addCurrentSegment(QString(), QByteArray());
    m_longRecordingSession.recordCurrentTerminalFailure(
        startFailurePrefix + error,
        0
    );
    return result;
}

VoiceRecordingStopResult VoiceRecordingCapture::stopNormal()
{
    VoiceRecordingStopResult result;
    result.pcm = m_handlers.stop ? m_handlers.stop() : QByteArray();
    result.wavPath = m_handlers.lastWavPath
        ? m_handlers.lastWavPath()
        : QString();
    return result;
}

int VoiceRecordingCapture::takePeakLevel() const
{
    return m_handlers.takePeakLevel ? m_handlers.takePeakLevel() : 0;
}

QString VoiceRecordingCapture::lastWavPath() const
{
    return m_handlers.lastWavPath ? m_handlers.lastWavPath() : QString();
}

VoiceLongRecordingSession &VoiceRecordingCapture::longRecordingSession()
{
    return m_longRecordingSession;
}

const VoiceLongRecordingSession &
VoiceRecordingCapture::longRecordingSession() const
{
    return m_longRecordingSession;
}
