#ifndef VOCEKIT_VOICE_RECORDING_COORDINATOR_H
#define VOCEKIT_VOICE_RECORDING_COORDINATOR_H

#include "voice_recording_capture.h"
#include "voice_recording_countdown.h"
#include "voice_recording_lifecycle.h"

#include <functional>

struct VoiceRecordingCoordinatorRequest
{
    QString modeId;
    VoiceRecordingStartRequest captureRequest;
    std::function<VoiceRecordingStartRequest()> captureRequestBuilder;
    int countdownSeconds = 0;
    bool playBeep = false;
    int countdownTickIntervalMs = 1000;
    int beepDelayMs = 250;
    int waveformIntervalMs = 120;
    int segmentIntervalMs = 0;
    int limitIntervalMs = 0;
};

struct VoiceRecordingCoordinatorCallbacks
{
    std::function<void(const QString &modeId, int seconds)> countdownTick;
    std::function<void(const QString &modeId)> beepRequested;
    std::function<void(const QString &modeId, bool longRecording)> started;
    std::function<void(const QString &modeId, const QString &error)> startFailed;
};

// 录音协调器：统一串联准备倒计时、录音采集和生命周期计时器。
class VoiceRecordingCoordinator
{
public:
    VoiceRecordingCoordinator(
        VoiceRecordingCapture &capture,
        VoiceRecordingLifecycle &lifecycle
    );

    void setCallbacks(const VoiceRecordingCoordinatorCallbacks &callbacks);
    void begin(const VoiceRecordingCoordinatorRequest &request);
    bool cancelPreparation();

    bool isPreparing() const;
    bool preparationMatchesMode(const QString &modeId) const;
    bool isRecording() const;
    VoiceRecordingStopResult stopNormal();

private:
    void startNow(const QString &modeId);

    VoiceRecordingCapture &m_capture;
    VoiceRecordingLifecycle &m_lifecycle;
    VoiceRecordingCountdown m_countdown;
    VoiceRecordingCoordinatorCallbacks m_callbacks;
    VoiceRecordingCoordinatorRequest m_request;
};

#endif // VOCEKIT_VOICE_RECORDING_COORDINATOR_H
