#include "history_archive.h"

#include "../file_utils.h"
#include "history_paths.h"
#include "history_record_service.h"

#include <QDateTime>
#include <QDir>
#include <QSet>

namespace {

QString haTr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

HistoryBackupResult backupHistoryRecordsToDirectory(
    const QString &recordDirectory,
    const QString &timestamp
)
{
    HistoryBackupResult result;
    result.sourcePath = historyRootPath(recordDirectory);

    if (!QDir(result.sourcePath).exists()) {
        QDir().mkpath(result.sourcePath);
    }
    ensureHistoryRootStructure(result.sourcePath);

    const QString folderName = QStringLiteral("vocekit-records-backup_")
        + (timestamp.trimmed().isEmpty()
            ? QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))
            : timestamp.trimmed());
    result.targetPath = QDir(historyBackupDirectory(result.sourcePath)).filePath(folderName);

    result.ok = copyDirectoryContentsRecursiveExcept(
        result.sourcePath,
        result.targetPath,
        QSet<QString>() << historyBackupFolderName(),
        false,
        &result.error,
        &result.fileCount
    );
    if (!result.ok && result.error.trimmed().isEmpty()) {
        result.error = haTr8("无法复制历史记录文件。");
    }
    return result;
}

HistoryImportResult importHistoryRecordsFromDirectory(
    const QString &sourceDirectory,
    const QString &recordDirectory
)
{
    HistoryImportResult result;
    result.sourcePath = QDir(sourceDirectory).absolutePath();
    result.targetPath = historyRootPath(recordDirectory);

    if (pathIsSameOrInside(result.targetPath, result.sourcePath)) {
        result.status = HistoryImportStatus::UnsafeSource;
        result.error = haTr8("不能从当前历史记录保存目录或它的上级目录导入，请选择单独的备份目录。");
        return result;
    }

    ensureHistoryRootStructure(result.targetPath);
    if (!copyDirectoryContentsRecursive(result.sourcePath, result.targetPath, true, &result.error, &result.fileCount)) {
        result.status = HistoryImportStatus::CopyFailed;
        if (result.error.trimmed().isEmpty()) {
            result.error = haTr8("无法导入历史记录文件。");
        }
        return result;
    }

    if (result.fileCount == 0) {
        result.status = HistoryImportStatus::EmptySource;
        result.error = haTr8("选择的目录里没有可导入的文件。");
        return result;
    }

    HistoryRecordService(result.targetPath).rebuildIndex();
    result.ok = true;
    result.status = HistoryImportStatus::Success;
    return result;
}
