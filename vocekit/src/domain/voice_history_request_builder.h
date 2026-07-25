#ifndef VOCEKIT_VOICE_HISTORY_REQUEST_BUILDER_H
#define VOCEKIT_VOICE_HISTORY_REQUEST_BUILDER_H

#include "voice_run_context.h"

#include "../recording/segmented_recording.h"
#include "../storage/history_record_service.h"

#include <QString>
#include <QVector>

struct VoiceHistoryBuildRequest
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

// 把一次功能执行的运行状态转换成 HistoryRecordService 能保存的请求。
// Controller 只负责收集运行状态，历史字段如何落入详情 JSON 由这里统一处理。
class VoiceHistoryRequestBuilder
{
public:
    static HistoryRecordSaveRequest build(
        const VoiceHistoryBuildRequest &request
    );
};

#endif // VOCEKIT_VOICE_HISTORY_REQUEST_BUILDER_H
