#ifndef VOCEKIT_VOICE_RECORDING_COUNTDOWN_H
#define VOCEKIT_VOICE_RECORDING_COUNTDOWN_H

#include <QString>
#include <QTimer>

#include <functional>

struct VoiceRecordingCountdownRequest
{
    QString modeId;
    int seconds = 0;
    bool playBeep = false;
    int tickIntervalMs = 1000;
    int beepDelayMs = 250;
};

struct VoiceRecordingCountdownCallbacks
{
    std::function<void(const QString &modeId, int seconds)> tick;
    std::function<void(const QString &modeId)> beepRequested;
    std::function<void(const QString &modeId)> startRequested;
};

// 录音倒计时模块：使用可停止计时器管理倒计时、提示音延迟和取消。
class VoiceRecordingCountdown
{
public:
    VoiceRecordingCountdown();

    void setCallbacks(const VoiceRecordingCountdownCallbacks &callbacks);
    void begin(const VoiceRecordingCountdownRequest &request);
    bool cancel();

    bool isActive() const;
    bool matchesMode(const QString &modeId) const;
    QString modeId() const;

private:
    void handleTick();
    void requestBeepThenStart();
    void requestStart();

    VoiceRecordingCountdownCallbacks m_callbacks;
    VoiceRecordingCountdownRequest m_request;
    QTimer m_tickTimer;
    QTimer m_startTimer;
    int m_remainingSeconds = 0;
    bool m_active = false;
};

#endif // VOCEKIT_VOICE_RECORDING_COUNTDOWN_H
