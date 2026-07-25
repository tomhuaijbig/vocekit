#include "app_paths.h"

#include <QCoreApplication>
#include <QDir>

QString appBasePath()
{
    QDir dir(QCoreApplication::applicationDirPath());
    const QString folder = dir.dirName().toLower();
    if (folder == QStringLiteral("debug") || folder == QStringLiteral("release")) {
        dir.cdUp();
    }
    return dir.absolutePath();
}

QString defaultRecordDirectory()
{
    return QDir(appBasePath()).filePath(QStringLiteral("records"));
}
