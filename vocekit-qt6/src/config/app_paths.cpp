#include "app_paths.h"

#include <QCoreApplication>
#include <QDir>

QString appBasePath()
{
    return appBasePathForApplicationDir(
        QCoreApplication::applicationDirPath()
    );
}

QString defaultRecordDirectory()
{
    return QDir(appBasePath()).filePath(QStringLiteral("records"));
}
