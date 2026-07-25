#ifndef VOCEKIT_HISTORY_TYPES_H
#define VOCEKIT_HISTORY_TYPES_H

#include "../recording/segmented_recording.h"

#include <QString>
#include <QStringList>
#include <QVector>

// 历史记录详情结构：对应每条 detail json 中保存的核心字段。
struct HistoryEntry
{
    QString modeId;
    QString mode;
    QString time;
    QString input;
    QString output;
    QString error;
    QString audio;
    QString textFile;
    QString allAudioFile;
    QString allTextFile;
    QString allDetailFile;
    QString filePath;
    QString model;
    qint64 elapsedMs = -1;
    qint64 speechElapsedMs = -1;
    qint64 modelElapsedMs = -1;
    QString recordingTriggerMode;
    bool longRecording = false;
    QVector<RecordingSegment> segments;
    QString ocrEngine;
    QStringList ocrLanguages;
    qint64 ocrElapsedMs = -1;
    bool ocrUsedFallback = false;
    QString imageFileName;
    QString promptVersion;
    bool favorite = false;
    QString favoriteFolder;
    bool draft = false;
};

// 历史列表查询条件：后续 HistoryStore 使用，当前先集中定义避免继续散落在 UI 里。
struct HistoryQuery
{
    QString modeId;
    QString searchText;
    QString favoriteFolder;
    int offset = 0;
    int limit = 25;
};

// 历史列表摘要：列表页只需要少量字段时可以避免加载全部详情。
struct HistorySummary
{
    QString id;
    QString modeId;
    QString mode;
    QString time;
    QString preview;
    QString filePath;
    bool favorite = false;
    QString favoriteFolder;
    bool hasError = false;
};

struct HistoryQueryResult
{
    QVector<HistorySummary> records;
    int total = 0;
};

// 分段录音重试结果：历史详情页重试单段识别时使用。
struct HistorySegmentRetryResult
{
    int index = 0;
    QString text;
    QString error;
    qint64 elapsedMs = -1;
};

struct HistoryTabDef
{
    QString id;
    QString title;
};

#endif // VOCEKIT_HISTORY_TYPES_H
