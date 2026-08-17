#ifndef VOCEKIT_HISTORY_SEGMENT_RETRY_TASK_H
#define VOCEKIT_HISTORY_SEGMENT_RETRY_TASK_H

#include "../domain/history_types.h"

#include <QByteArray>
#include <QString>

struct HistorySegmentRetryTaskRequest
{
    RecordingSegment segment;
    QByteArray pcm;
    QString speechProvider;
    QString networkPolicy = QStringLiteral("inherit");
    bool useSystemProxy = false;
};

struct HistorySegmentRetryTaskPreflight
{
    bool ok = false;
    QString error;
    QByteArray pcm;
};

QString historySegmentRetryConfigurationError(
    const QString &speechProvider
);

HistorySegmentRetryTaskPreflight prepareHistorySegmentRetryTask(
    const HistorySegmentRetryTaskRequest &request
);

HistorySegmentRetryResult runHistorySegmentRetryTask(
    const HistorySegmentRetryTaskRequest &request
);

#endif // VOCEKIT_HISTORY_SEGMENT_RETRY_TASK_H
