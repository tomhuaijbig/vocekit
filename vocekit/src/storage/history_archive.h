#ifndef VOCEKIT_HISTORY_ARCHIVE_H
#define VOCEKIT_HISTORY_ARCHIVE_H

#include <QString>

struct HistoryBackupResult
{
    bool ok = false;
    int fileCount = 0;
    QString sourcePath;
    QString targetPath;
    QString error;
};

enum class HistoryImportStatus
{
    Success,
    UnsafeSource,
    CopyFailed,
    EmptySource
};

struct HistoryImportResult
{
    bool ok = false;
    HistoryImportStatus status = HistoryImportStatus::CopyFailed;
    int fileCount = 0;
    QString sourcePath;
    QString targetPath;
    QString error;
};

HistoryBackupResult backupHistoryRecordsToDirectory(
    const QString &recordDirectory,
    const QString &timestamp
);

HistoryImportResult importHistoryRecordsFromDirectory(
    const QString &sourceDirectory,
    const QString &recordDirectory
);

#endif // VOCEKIT_HISTORY_ARCHIVE_H
