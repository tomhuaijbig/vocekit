#ifndef VOCEKIT_VOICE_RUN_SESSION_H
#define VOCEKIT_VOICE_RUN_SESSION_H

#include "voice_run_context.h"

#include "../providers/provider_types.h"
#include "../recording/segmented_recording.h"

#include <QElapsedTimer>
#include <QString>
#include <QVector>

struct VoiceModelProcessingResult;

// 单次功能执行的可持久化运行状态。控制器只报告阶段结果，重置规则和
// 历史记录需要的快照由这个对象统一维护，避免不同入口残留上一次数据。
struct VoiceRunSessionSnapshot
{
    qint64 elapsedMs = -1;
    qint64 speechElapsedMs = -1;
    qint64 modelElapsedMs = -1;
    QString promptVersion;
    QByteArray rawModelResponse;
    ModelRequestTelemetry modelTelemetry;
    VoiceRunContext runContext;
    bool actionHadRecording = false;
    QString recordingAudioPath;
    QString recordingTriggerMode;
    bool longRecording = false;
    QVector<RecordingSegment> recordingSegments;

    QString sourceAudioPath(const QString &fallbackAudioPath) const;
};

class VoiceRunSession
{
public:
    void beginAction();
    void restartTimer();
    void beginTextInput();
    void beginModelAttempt();

    qint64 elapsedMs() const;
    VoiceRunSessionSnapshot snapshot() const;

    void setActionHadRecording(bool value);
    void setSpeechElapsedMs(qint64 elapsedMs);
    void addSpeechElapsedMs(qint64 elapsedMs);
    void setModelResult(qint64 elapsedMs, const QString &promptVersion);
    void setModelResult(const VoiceModelProcessingResult &result);
    void setRecordingAudioPath(const QString &path);
    void setRecordingSegments(const QVector<RecordingSegment> &segments);
    void setRecordingTriggerMode(const QString &mode);
    QString recordingTriggerMode() const;
    void setLongRecording(bool value);
    void setRunContext(const VoiceRunContext &context);
    const VoiceRunContext &runContext() const;

private:
    void resetRunState();
    void resetModelState();
    void resetRecordingState();

    QElapsedTimer m_timer;
    VoiceRunSessionSnapshot m_state;
};

#endif // VOCEKIT_VOICE_RUN_SESSION_H
