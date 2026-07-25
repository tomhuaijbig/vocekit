#include "file_utils.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

namespace {

QString fuTr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

QString uniqueFilePath(const QString &targetPath)
{
    if (!QFileInfo::exists(targetPath)) {
        return targetPath;
    }
    QFileInfo info(targetPath);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix();
    for (int i = 1; i < 10000; ++i) {
        const QString candidate = info.dir().filePath(base + QStringLiteral("_") + QString::number(i) + suffix);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return info.dir().filePath(base + QStringLiteral("_copy_") + QString::number(QDateTime::currentMSecsSinceEpoch()) + suffix);
}

bool copyFileToPath(const QString &sourcePath, const QString &targetPath, bool keepExisting, QString *error, int *fileCount)
{
    QFileInfo targetInfo(targetPath);
    if (!targetInfo.dir().exists() && !targetInfo.dir().mkpath(QStringLiteral("."))) {
        if (error) {
            *error = fuTr8("无法创建目标目录：") + targetInfo.dir().absolutePath();
        }
        return false;
    }

    QString finalPath = targetPath;
    if (QFileInfo::exists(finalPath)) {
        if (keepExisting) {
            finalPath = uniqueFilePath(finalPath);
        } else if (!QFile::remove(finalPath)) {
            if (error) {
                *error = fuTr8("无法覆盖文件：") + finalPath;
            }
            return false;
        }
    }

    if (!QFile::copy(sourcePath, finalPath)) {
        if (error) {
            *error = fuTr8("无法复制文件：") + sourcePath;
        }
        return false;
    }
    if (fileCount) {
        ++(*fileCount);
    }
    return true;
}

bool copyDirectoryContentsRecursive(const QString &sourcePath, const QString &targetPath, bool keepExistingFiles, QString *error, int *fileCount)
{
    QDir source(sourcePath);
    if (!source.exists()) {
        if (error) {
            *error = fuTr8("源目录不存在：") + sourcePath;
        }
        return false;
    }
    QDir target(targetPath);
    if (!target.exists() && !target.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = fuTr8("无法创建目标目录：") + targetPath;
        }
        return false;
    }

    const QFileInfoList entries = source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QString targetEntryPath = target.filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryContentsRecursive(entry.absoluteFilePath(), targetEntryPath, keepExistingFiles, error, fileCount)) {
                return false;
            }
        } else if (entry.isFile()) {
            if (!copyFileToPath(entry.absoluteFilePath(), targetEntryPath, keepExistingFiles, error, fileCount)) {
                return false;
            }
        }
    }
    return true;
}

bool copyDirectoryContentsRecursiveExcept(
    const QString &sourcePath,
    const QString &targetPath,
    const QSet<QString> &excludedFolderNames,
    bool keepExistingFiles,
    QString *error,
    int *fileCount
)
{
    QDir source(sourcePath);
    if (!source.exists()) {
        if (error) {
            *error = fuTr8("源目录不存在：") + sourcePath;
        }
        return false;
    }
    QDir target(targetPath);
    if (!target.exists() && !target.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = fuTr8("无法创建目标目录：") + targetPath;
        }
        return false;
    }

    const QFileInfoList entries = source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir() && excludedFolderNames.contains(entry.fileName())) {
            continue;
        }
        const QString targetEntryPath = target.filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryContentsRecursiveExcept(entry.absoluteFilePath(), targetEntryPath, excludedFolderNames, keepExistingFiles, error, fileCount)) {
                return false;
            }
        } else if (entry.isFile()) {
            if (!copyFileToPath(entry.absoluteFilePath(), targetEntryPath, keepExistingFiles, error, fileCount)) {
                return false;
            }
        }
    }
    return true;
}

bool ensureParentDirectoryForFile(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
    QDir dir = info.dir();
    return dir.exists() || dir.mkpath(QStringLiteral("."));
}

bool writeBytesAtomically(const QString &path, const QByteArray &data)
{
    if (!ensureParentDirectoryForFile(path)) {
        return false;
    }

    // 配置、密钥和历史 JSON 使用 QSaveFile，避免程序退出时留下半截文件。
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

bool readJsonObjectFile(const QString &path, QJsonObject *object)
{
    if (!object) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    *object = doc.object();
    return true;
}

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

bool writeTextFile(const QString &path, const QString &text)
{
    return writeBytesAtomically(path, text.toUtf8());
}
