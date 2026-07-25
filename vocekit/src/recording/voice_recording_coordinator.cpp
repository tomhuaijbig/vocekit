#include "voice_recording_coordinator.h"

VoiceRecordingCoordinator::VoiceRecordingCoordinator(
    VoiceRecordingCapture &capture,
    VoiceRecordingLifecycle &lifecycle
)
    : m_capture(capture),
      m_lifecycle(lifecycle)
{
    VoiceRecordingCountdownCallbacks countdownCallbacks;
    countdownCallbacks.tick = [this](const QString &modeId, int seconds) {
        if (m_callbacks.countdownTick) {
            m_callbacks.countdownTick(modeId, seconds);
        }
    };
    countdownCallbacks.beepRequested = [this](const QString &modeId) {
        if (m_callbacks.beepRequested) {
            m_callbacks.beepRequested(modeId);
        }
    };
    countdownCallbacks.startRequested = [this](const QString &modeId) {
        startNow(modeId);
    };
    m_countdown.setCallbacks(countdownCallbacks);
}

void VoiceRecordingCoordinator::setCallbacks(
    const VoiceRecordingCoordinatorCallbacks &callbacks
)
{
    m_callbacks = callbacks;
}

void VoiceRecordingCoordinator::begin(
    const VoiceRecordingCoordinatorRequest &request
)
{
    m_request = request;
    VoiceRecordingCountdownRequest countdownRequest;
    countdownRequest.modeId = request.modeId;
    countdownRequest.seconds = request.countdownSeconds;
    countdownRequest.playBeep = request.playBeep;
    countdownRequest.tickIntervalMs = request.countdownTickIntervalMs;
    countdownRequest.beepDelayMs = request.beepDelayMs;
    m_countdown.begin(countdownRequest);
}

bool VoiceRecordingCoordinator::cancelPreparation()
{
    return m_countdown.cancel();
}

bool VoiceRecordingCoordinator::isPreparing() const
{
    return m_countdown.isActive();
}

bool VoiceRecordingCoordinator::preparationMatchesMode(
    const QString &modeId
) const
{
    return m_countdown.matchesMode(modeId);
}

bool VoiceRecordingCoordinator::isRecording() const
{
    return m_lifecycle.isRecording();
}

VoiceRecordingStopResult VoiceRecordingCoordinator::stopNormal()
{
    m_lifecycle.stop();
    return m_capture.stopNormal();
}

void VoiceRecordingCoordinator::startNow(const QString &modeId)
{
    if (modeId != m_request.modeId) {
        return;
    }

    const VoiceRecordingStartRequest captureRequest =
        m_request.captureRequestBuilder
            ? m_request.captureRequestBuilder()
            : m_request.captureRequest;
    QString error;
    if (!m_capture.begin(captureRequest, &error)) {
        if (m_callbacks.startFailed) {
            m_callbacks.startFailed(modeId, error);
        }
        return;
    }

    const bool longRecording =
        m_capture.longRecordingSession().isActive();
    m_lifecycle.start(
        longRecording,
        m_request.waveformIntervalMs,
        longRecording ? m_request.segmentIntervalMs : 0,
        longRecording ? m_request.limitIntervalMs : 0
    );
    if (m_callbacks.started) {
        m_callbacks.started(modeId, longRecording);
    }
}
