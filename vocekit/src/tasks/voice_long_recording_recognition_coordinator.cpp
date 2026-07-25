#include "voice_long_recording_recognition_coordinator.h"

#include <QtConcurrent>

VoiceLongRecordingRecognitionCoordinator::
VoiceLongRecordingRecognitionCoordinator(QObject *parent)
    : QObject(parent)
{
    connect(
        &m_watcher,
        &QFutureWatcher<VoiceLongRecordingSegmentResult>::finished,
        this,
        [this]() {
            handleFinished();
        }
    );
}

VoiceLongRecordingRecognitionCoordinator::
~VoiceLongRecordingRecognitionCoordinator()
{
    m_cancellation.cancel();
    if (m_watcher.isRunning()) {
        m_watcher.waitForFinished();
    }
}

void VoiceLongRecordingRecognitionCoordinator::setCallbacks(
    const VoiceLongRecordingRecognitionCallbacks &callbacks
)
{
    m_callbacks = callbacks;
}

void VoiceLongRecordingRecognitionCoordinator::reset()
{
    m_cancellation.cancel();
    m_cancellation = CancellationSource();
    m_session = nullptr;
    m_completionNotified = false;
}

void VoiceLongRecordingRecognitionCoordinator::schedule(
    VoiceLongRecordingSession &session,
    const VoiceLongRecordingRecognitionConfig &config,
    const VoiceSpeechRecognitionHandlers &handlers
)
{
    m_session = &session;
    m_config = config;
    m_handlers = handlers;
    processNext();
}

void VoiceLongRecordingRecognitionCoordinator::cancel()
{
    reset();
}

bool VoiceLongRecordingRecognitionCoordinator::isRunning() const
{
    return m_watcher.isRunning();
}

ExecutionId VoiceLongRecordingRecognitionCoordinator::executionId() const
{
    return m_cancellation.executionId();
}

void VoiceLongRecordingRecognitionCoordinator::processNext()
{
    if (!m_session || m_watcher.isRunning()) {
        return;
    }

    const int index =
        m_session->recognitionState().nextPendingIndex();
    if (index < 0) {
        if (m_session->isFinalizing() && !m_completionNotified) {
            m_completionNotified = true;
            if (m_callbacks.allFinished) {
                m_callbacks.allFinished();
            }
        }
        return;
    }

    m_completionNotified = false;
    const QByteArray pcm = m_session->pcmBySegment().value(index);
    const int attempt =
        m_session->recognitionState().segment(index).attempts + 1;
    if (m_callbacks.segmentStarted) {
        m_callbacks.segmentStarted(index, attempt, m_config.provider);
    }

    VoiceLongRecordingSegmentRequest request;
    request.speech.index = index;
    request.speech.modeId = m_config.modeId;
    request.speech.audioData = pcm;
    request.speech.audioFormat = QStringLiteral("pcm");
    request.speech.sampleRate = 16000;
    request.speech.provider = m_config.provider;
    request.speech.useSystemProxy = m_config.useSystemProxy;
    request.speech.networkPolicy = m_config.networkPolicy;
    request.cancellation = m_cancellation.token();
    const VoiceSpeechRecognitionHandlers handlers = m_handlers;
    m_watcher.setFuture(QtConcurrent::run([request, handlers]() {
        return VoiceLongRecordingSegmentExecutor::run(
            request,
            handlers
        );
    }));
}

void VoiceLongRecordingRecognitionCoordinator::handleFinished()
{
    const VoiceLongRecordingSegmentResult result = m_watcher.result();
    if (result.executionId != m_cancellation.executionId()) {
        processNext();
        return;
    }
    if (result.cancelled || !m_session) {
        return;
    }

    VoiceLongRecordingSegmentExecutor::apply(
        result,
        &m_session->recognitionState()
    );
    if (m_callbacks.segmentFinished) {
        m_callbacks.segmentFinished(result);
    }
    processNext();
}
