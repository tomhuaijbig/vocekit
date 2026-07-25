#ifndef VOCEKIT_VOICE_LONG_RECORDING_RECOGNITION_COORDINATOR_H
#define VOCEKIT_VOICE_LONG_RECORDING_RECOGNITION_COORDINATOR_H

#include "voice_long_recording_segment_executor.h"
#include "../recording/voice_long_recording_session.h"

#include <QFutureWatcher>
#include <QObject>

#include <functional>

struct VoiceLongRecordingRecognitionConfig
{
    QString modeId;
    QString provider;
    QString networkPolicy = QStringLiteral("inherit");
    bool useSystemProxy = false;
};

struct VoiceLongRecordingRecognitionCallbacks
{
    std::function<void(
        int index,
        int attempt,
        const QString &provider
    )> segmentStarted;
    std::function<void(
        const VoiceLongRecordingSegmentResult &result
    )> segmentFinished;
    std::function<void()> allFinished;
};

// 长录音识别协调器：管理后台任务、取消令牌和连续分段调度。
class VoiceLongRecordingRecognitionCoordinator : public QObject
{
public:
    explicit VoiceLongRecordingRecognitionCoordinator(
        QObject *parent = nullptr
    );
    ~VoiceLongRecordingRecognitionCoordinator();

    void setCallbacks(
        const VoiceLongRecordingRecognitionCallbacks &callbacks
    );
    void reset();
    void schedule(
        VoiceLongRecordingSession &session,
        const VoiceLongRecordingRecognitionConfig &config,
        const VoiceSpeechRecognitionHandlers &handlers
    );
    void cancel();
    bool isRunning() const;
    ExecutionId executionId() const;

private:
    void processNext();
    void handleFinished();

    VoiceLongRecordingSession *m_session = nullptr;
    VoiceLongRecordingRecognitionConfig m_config;
    VoiceSpeechRecognitionHandlers m_handlers;
    VoiceLongRecordingRecognitionCallbacks m_callbacks;
    QFutureWatcher<VoiceLongRecordingSegmentResult> m_watcher;
    CancellationSource m_cancellation;
    bool m_completionNotified = false;
};

#endif // VOCEKIT_VOICE_LONG_RECORDING_RECOGNITION_COORDINATOR_H
