#ifndef VOCEKIT_HISTORY_EXPORT_H
#define VOCEKIT_HISTORY_EXPORT_H

#include "../domain/history_types.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <functional>

struct HistoryExportOptions
{
    QString filterMode;
    QString searchText;
    bool selectionOnly = true;
    QDateTime exportedAt;
};

struct HistoryAudioExportResult
{
    bool ok = true;
    int exported = 0;
    QString error;
};

typedef std::function<QString(const HistoryEntry &entry)> HistoryExportTextProvider;
typedef std::function<QJsonObject(const HistoryEntry &entry)> HistoryExportJsonProvider;

QString buildHistoryTextExportContent(
    const QVector<HistoryEntry> &entries,
    const HistoryExportTextProvider &detailTextProvider
);

QJsonObject buildHistoryDetailsExportObject(
    const QVector<HistoryEntry> &entries,
    const HistoryExportOptions &options,
    const HistoryExportJsonProvider &entryObjectProvider
);

bool writeHistoryTextExportFile(
    const QVector<HistoryEntry> &entries,
    const QString &path,
    const HistoryExportTextProvider &detailTextProvider
);

bool writeHistoryDetailsExportFile(
    const QVector<HistoryEntry> &entries,
    const QString &path,
    const HistoryExportOptions &options,
    const HistoryExportJsonProvider &entryObjectProvider
);

HistoryAudioExportResult exportHistoryAudioFilesToDirectory(
    const QVector<HistoryEntry> &entries,
    const QString &targetPath
);

#endif // VOCEKIT_HISTORY_EXPORT_H
