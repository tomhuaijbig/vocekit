#include "history_paths.h"

#include "../config/app_paths.h"

#include <QDesktopServices>
#include <QDir>
#include <QUrl>

QString historyRootPath(const QString &recordDirectory)
{
    const QString trimmed = recordDirectory.trimmed();
    return trimmed.isEmpty() ? defaultRecordDirectory() : trimmed;
}

HistoryStore historyStoreForRecordDirectory(const QString &recordDirectory)
{
    return HistoryStore(historyRootPath(recordDirectory));
}

QString recordDirectoryForDate(const QString &recordDirectory, const QDate &date)
{
    return historyStoreForRecordDirectory(recordDirectory).allDateDirectory(
        HistoryStore::allAudioFolderName(),
        date.toString(QStringLiteral("yyyy-MM-dd"))
    );
}

void openDirectoryPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    QDir().mkpath(trimmed);
    QDesktopServices::openUrl(QUrl::fromLocalFile(trimmed));
}

QString normalizedPathForCompare(const QString &path)
{
    return HistoryStore::normalizedPathForCompare(path);
}

bool pathIsSameOrInside(const QString &candidate, const QString &parent)
{
    return HistoryStore::pathIsSameOrInside(candidate, parent);
}

QString safeFileNamePart(const QString &text, const QString &fallback)
{
    return HistoryStore::safeFileNamePart(text, fallback);
}

QString historyBackupFolderName()
{
    return HistoryStore::backupFolderName();
}

QString historyAllTextFolderName()
{
    return HistoryStore::allTextFolderName();
}

QString historyAllDetailFolderName()
{
    return HistoryStore::allDetailFolderName();
}

QString historyAudioSubFolderName()
{
    return HistoryStore::audioSubFolderName();
}

QString historyBackupDirectory(const QString &recordDirectory)
{
    return historyStoreForRecordDirectory(recordDirectory).backupDirectory();
}

QString historyAllDateDirectory(
    const QString &recordDirectory,
    const QString &folderName,
    const QString &date
)
{
    return historyStoreForRecordDirectory(recordDirectory).allDateDirectory(folderName, date);
}

QString historyModeDateSubDirectory(
    const QString &recordDirectory,
    const QString &modeTitle,
    const QString &date,
    const QString &subFolder
)
{
    return historyStoreForRecordDirectory(recordDirectory).modeDateSubDirectory(
        modeTitle,
        date,
        subFolder
    );
}

void ensureHistoryRootStructure(const QString &recordDirectory)
{
    historyStoreForRecordDirectory(recordDirectory).ensureRootStructure();
}

void ensureHistoryModeDateStructure(
    const QString &recordDirectory,
    const QString &modeTitle,
    const QString &date
)
{
    historyStoreForRecordDirectory(recordDirectory).ensureModeDateStructure(modeTitle, date);
}

QString historyTextFromJsonObject(const QJsonObject &item)
{
    return HistoryStore::textFromJsonObject(item);
}
