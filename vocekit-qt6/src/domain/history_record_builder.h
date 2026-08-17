#ifndef VOCEKIT_HISTORY_RECORD_BUILDER_H
#define VOCEKIT_HISTORY_RECORD_BUILDER_H

#include "history_types.h"
#include "voice_run_context.h"

#include "../recording/segmented_recording.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct HistoryRecordMetadataRequest
{
    QString input;
    QString output;
    QString error;
    QString model;
    qint64 elapsedMs = -1;
    qint64 speechElapsedMs = -1;
    qint64 modelElapsedMs = -1;
    QString promptVersion;
    VoiceRunContext runContext;
    bool actionHadRecording = false;
    QString recordingTriggerMode;
    bool longRecording = false;
    QVector<RecordingSegment> recordingSegments;
    QString flowRunId;
    int flowPublishedRevision = 0;
    QString flowPublishedHash;
    QString flowTrigger;
    QVector<HistoryFlowNodeTrace> flowNodeTraces;
    QString flowFailedNodeId;
    QString flowFailedNodeType;
    bool flowCancelled = false;
};

struct OcrPageHistoryMetadataRequest
{
    OcrResult result;
    QString imagePath;
    QStringList languages;
};

// Builds business metadata stored inside history detail JSON files.
class HistoryRecordBuilder
{
public:
    static QJsonObject buildMetadata(const HistoryRecordMetadataRequest &request);
    static QJsonObject buildOcrPageMetadata(
        const OcrPageHistoryMetadataRequest &request
    );
    static QString ocrEngineName(OcrEngine engine);
};

#endif // VOCEKIT_HISTORY_RECORD_BUILDER_H
