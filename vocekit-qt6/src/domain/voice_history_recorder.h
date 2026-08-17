#ifndef VOCEKIT_VOICE_HISTORY_RECORDER_H
#define VOCEKIT_VOICE_HISTORY_RECORDER_H

#include "voice_run_context.h"

#include "../recording/segmented_recording.h"
#include "../storage/history_store.h"

#include <QString>
#include <QVector>

struct VoiceHistorySaveRequest
{
    QString recordDirectory;
    QString modeId;
    QString modeTitle;
    QString sourceAudioPath;
    QString input;
    QString output;
    QString error;
    bool draft = false;
    QString model;
    bool usedModel = false;
    qint64 elapsedMs = -1;
    qint64 speechElapsedMs = -1;
    qint64 modelElapsedMs = -1;
    QString promptVersion;
    VoiceRunContext runContext;
    bool actionHadRecording = false;
    QString recordingTriggerMode;
    bool longRecording = false;
    QVector<RecordingSegment> recordingSegments;
};

struct VoiceHistorySaveResult
{
    HistoryAppendResult saved;
    QString logAction;
    QString logDetail;
};

// 语音功能历史写入器：集中处理 VoiceController 产生的运行结果如何保存为历史记录。
class VoiceHistoryRecorder
{
public:
    static VoiceHistorySaveResult save(const VoiceHistorySaveRequest &request);
};

#endif // VOCEKIT_VOICE_HISTORY_RECORDER_H
