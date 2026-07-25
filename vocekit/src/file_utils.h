#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QSet>
#include <QString>

QString uniqueFilePath(const QString &targetPath);
bool copyFileToPath(const QString &sourcePath, const QString &targetPath, bool keepExisting, QString *error, int *fileCount);
bool copyDirectoryContentsRecursive(const QString &sourcePath, const QString &targetPath, bool keepExistingFiles, QString *error, int *fileCount);
bool copyDirectoryContentsRecursiveExcept(const QString &sourcePath, const QString &targetPath, const QSet<QString> &excludedFolderNames, bool keepExistingFiles, QString *error, int *fileCount);
bool ensureParentDirectoryForFile(const QString &path);
bool writeBytesAtomically(const QString &path, const QByteArray &data);
bool readJsonObjectFile(const QString &path, QJsonObject *object);
QString readTextFile(const QString &path);
bool writeTextFile(const QString &path, const QString &text);
