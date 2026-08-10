#ifndef VOCEKIT_VOICE_RECORDING_CAPTURE_H
#define VOCEKIT_VOICE_RECORDING_CAPTURE_H

#include "voice_long_recording_session.h"

#include <functional>

struct VoiceRecordingCaptureHandlers
{
    std::function<bool(
        const QString &title,
        const QString &directory,
        bool directDirectory,
        QString *error
    )> start;
    std::function<QByteArray()> stop;
    std::function<QString()> lastWavPath;
    std::function<int()> takePeakLevel;
    std::function<void(
        const std::function<void(const QByteArray &)> &
    )> setPcmListener;
};

struct VoiceRecordingStartRequest
{
    QString normalTitle;
    QString normalDirectory;
    bool longRecordingEnabled = false;
    QString firstSegmentTitle;
    QString longRecordingDirectory;
    QString longRecordingFileBase;
};

struct VoiceRecordingStopResult
{
    QByteArray pcm;
    QString wavPath;
};

struct VoiceRecordingSegmentCapture
{
    bool valid = false;
    int index = 0;
    QByteArray pcm;
    QString wavPath;
};

enum class VoiceRecordingNextSegmentStatus
{
    Started,
    LimitReached,
    StartFailed
};

struct VoiceRecordingNextSegmentResult
{
    VoiceRecordingNextSegmentStatus status =
        VoiceRecordingNextSegmentStatus::LimitReached;
    int index = 0;
    QString error;
};

// 录音采集模块：隐藏设备调用，并统一普通录音和长录音分段规则。
class VoiceRecordingCapture
{
public:
    explicit VoiceRecordingCapture(
        const VoiceRecordingCaptureHandlers &handlers =
            VoiceRecordingCaptureHandlers()
    );

    void setHandlers(const VoiceRecordingCaptureHandlers &handlers);
    bool begin(const VoiceRecordingStartRequest &request, QString *error);
    VoiceRecordingSegmentCapture captureCurrentLongSegment(
        const QString &emptyAudioError
    );
    VoiceRecordingNextSegmentResult startNextLongSegment(
        const QString &segmentTitle,
        const QString &startFailurePrefix
    );
    VoiceRecordingStopResult stopNormal();
    void setPcmListener(
        const std::function<void(const QByteArray &)> &listener
    );

    int takePeakLevel() const;
    QString lastWavPath() const;
    VoiceLongRecordingSession &longRecordingSession();
    const VoiceLongRecordingSession &longRecordingSession() const;

private:
    VoiceRecordingCaptureHandlers m_handlers;
    VoiceLongRecordingSession m_longRecordingSession;
};

#endif // VOCEKIT_VOICE_RECORDING_CAPTURE_H
