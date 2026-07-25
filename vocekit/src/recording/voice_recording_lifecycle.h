#ifndef VOCEKIT_VOICE_RECORDING_LIFECYCLE_H
#define VOCEKIT_VOICE_RECORDING_LIFECYCLE_H

#include <QTimer>

#include <functional>

struct VoiceRecordingLifecycleCallbacks
{
    std::function<void()> waveformTick;
    std::function<void()> segmentElapsed;
    std::function<void()> limitElapsed;
};

// 录音生命周期模块：统一维护录音状态和波形、分段、最长时长计时器。
class VoiceRecordingLifecycle
{
public:
    VoiceRecordingLifecycle();

    void setCallbacks(const VoiceRecordingLifecycleCallbacks &callbacks);
    void start(
        bool longRecording,
        int waveformIntervalMs,
        int segmentIntervalMs,
        int limitIntervalMs
    );
    void restartSegment(int intervalMs);
    void stop();

    bool isRecording() const;
    bool isLongRecording() const;

private:
    VoiceRecordingLifecycleCallbacks m_callbacks;
    QTimer m_waveformTimer;
    QTimer m_segmentTimer;
    QTimer m_limitTimer;
    bool m_recording = false;
    bool m_longRecording = false;
};

#endif // VOCEKIT_VOICE_RECORDING_LIFECYCLE_H
