#include "voice_recording_countdown.h"

#include <QtGlobal>

VoiceRecordingCountdown::VoiceRecordingCountdown()
{
    m_startTimer.setSingleShot(true);
    QObject::connect(&m_tickTimer, &QTimer::timeout, [this]() {
        handleTick();
    });
    QObject::connect(&m_startTimer, &QTimer::timeout, [this]() {
        requestStart();
    });
}

void VoiceRecordingCountdown::setCallbacks(
    const VoiceRecordingCountdownCallbacks &callbacks
)
{
    m_callbacks = callbacks;
}

void VoiceRecordingCountdown::begin(
    const VoiceRecordingCountdownRequest &request
)
{
    cancel();
    m_request = request;
    m_request.seconds = qMax(0, request.seconds);
    m_request.tickIntervalMs = qMax(1, request.tickIntervalMs);
    m_request.beepDelayMs = qMax(0, request.beepDelayMs);
    m_remainingSeconds = m_request.seconds;

    if (m_remainingSeconds > 0) {
        m_active = true;
        if (m_callbacks.tick) {
            m_callbacks.tick(m_request.modeId, m_remainingSeconds);
        }
        m_tickTimer.start(m_request.tickIntervalMs);
        return;
    }
    if (m_request.playBeep) {
        m_active = true;
        requestBeepThenStart();
        return;
    }
    requestStart();
}

bool VoiceRecordingCountdown::cancel()
{
    const bool wasActive = m_active;
    m_tickTimer.stop();
    m_startTimer.stop();
    m_remainingSeconds = 0;
    m_active = false;
    m_request.modeId.clear();
    return wasActive;
}

bool VoiceRecordingCountdown::isActive() const
{
    return m_active;
}

bool VoiceRecordingCountdown::matchesMode(const QString &modeId) const
{
    return m_active && m_request.modeId == modeId;
}

QString VoiceRecordingCountdown::modeId() const
{
    return m_active ? m_request.modeId : QString();
}

void VoiceRecordingCountdown::handleTick()
{
    if (!m_active) {
        return;
    }
    --m_remainingSeconds;
    if (m_remainingSeconds > 0) {
        if (m_callbacks.tick) {
            m_callbacks.tick(m_request.modeId, m_remainingSeconds);
        }
        return;
    }

    m_tickTimer.stop();
    if (m_request.playBeep) {
        requestBeepThenStart();
    } else {
        requestStart();
    }
}

void VoiceRecordingCountdown::requestBeepThenStart()
{
    if (!m_active) {
        return;
    }
    if (m_callbacks.beepRequested) {
        m_callbacks.beepRequested(m_request.modeId);
    }
    if (m_request.beepDelayMs <= 0) {
        requestStart();
    } else {
        m_startTimer.start(m_request.beepDelayMs);
    }
}

void VoiceRecordingCountdown::requestStart()
{
    if (m_request.modeId.trimmed().isEmpty()) {
        return;
    }
    const QString modeId = m_request.modeId;
    m_tickTimer.stop();
    m_startTimer.stop();
    m_remainingSeconds = 0;
    m_active = false;
    m_request.modeId.clear();
    if (m_callbacks.startRequested) {
        m_callbacks.startRequested(modeId);
    }
}
