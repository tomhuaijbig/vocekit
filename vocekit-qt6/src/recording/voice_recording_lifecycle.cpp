#include "voice_recording_lifecycle.h"

VoiceRecordingLifecycle::VoiceRecordingLifecycle()
{
    m_segmentTimer.setSingleShot(true);
    m_limitTimer.setSingleShot(true);

    QObject::connect(&m_waveformTimer, &QTimer::timeout, [this]() {
        if (m_recording && m_callbacks.waveformTick) {
            m_callbacks.waveformTick();
        }
    });
    QObject::connect(&m_segmentTimer, &QTimer::timeout, [this]() {
        if (m_recording
            && m_longRecording
            && m_callbacks.segmentElapsed) {
            m_callbacks.segmentElapsed();
        }
    });
    QObject::connect(&m_limitTimer, &QTimer::timeout, [this]() {
        if (m_recording
            && m_longRecording
            && m_callbacks.limitElapsed) {
            m_callbacks.limitElapsed();
        }
    });
}

void VoiceRecordingLifecycle::setCallbacks(
    const VoiceRecordingLifecycleCallbacks &callbacks
)
{
    m_callbacks = callbacks;
}

void VoiceRecordingLifecycle::start(
    bool longRecording,
    int waveformIntervalMs,
    int segmentIntervalMs,
    int limitIntervalMs
)
{
    stop();
    m_recording = true;
    m_longRecording = longRecording;
    if (waveformIntervalMs > 0) {
        m_waveformTimer.start(waveformIntervalMs);
    }
    if (longRecording) {
        if (segmentIntervalMs > 0) {
            m_segmentTimer.start(segmentIntervalMs);
        }
        if (limitIntervalMs > 0) {
            m_limitTimer.start(limitIntervalMs);
        }
    }
}

void VoiceRecordingLifecycle::restartSegment(int intervalMs)
{
    if (!m_recording || !m_longRecording || intervalMs <= 0) {
        return;
    }
    m_segmentTimer.start(intervalMs);
}

void VoiceRecordingLifecycle::stop()
{
    m_waveformTimer.stop();
    m_segmentTimer.stop();
    m_limitTimer.stop();
    m_recording = false;
    m_longRecording = false;
}

bool VoiceRecordingLifecycle::isRecording() const
{
    return m_recording;
}

bool VoiceRecordingLifecycle::isLongRecording() const
{
    return m_longRecording;
}
