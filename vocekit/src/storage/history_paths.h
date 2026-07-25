#ifndef VOCEKIT_HISTORY_PATHS_H
#define VOCEKIT_HISTORY_PATHS_H

#include "history_store.h"

#include <QDate>
#include <QJsonObject>
#include <QString>

// 历史目录的兼容入口。页面层仍使用这些名字，但真实规则集中到 HistoryStore。
QString historyRootPath(const QString &recordDirectory);
HistoryStore historyStoreForRecordDirectory(const QString &recordDirectory);
QString recordDirectoryForDate(
    const QString &recordDirectory,
    const QDate &date = QDate::currentDate()
);
void openDirectoryPath(const QString &path);
QString normalizedPathForCompare(const QString &path);
bool pathIsSameOrInside(const QString &candidate, const QString &parent);
QString safeFileNamePart(
    const QString &text,
    const QString &fallback = QStringLiteral("history")
);
QString historyBackupFolderName();
QString historyAllTextFolderName();
QString historyAllDetailFolderName();
QString historyAudioSubFolderName();
QString historyBackupDirectory(const QString &recordDirectory);
QString historyAllDateDirectory(
    const QString &recordDirectory,
    const QString &folderName,
    const QString &date
);
QString historyModeDateSubDirectory(
    const QString &recordDirectory,
    const QString &modeTitle,
    const QString &date,
    const QString &subFolder
);
void ensureHistoryRootStructure(const QString &recordDirectory);
void ensureHistoryModeDateStructure(
    const QString &recordDirectory,
    const QString &modeTitle,
    const QString &date
);
QString historyTextFromJsonObject(const QJsonObject &item);

#endif // VOCEKIT_HISTORY_PATHS_H
