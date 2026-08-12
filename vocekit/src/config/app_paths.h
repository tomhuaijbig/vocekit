#ifndef VOCEKIT_APP_PATHS_H
#define VOCEKIT_APP_PATHS_H

#include <QCoreApplication>
#include <QDir>
#include <QString>

// 应用级路径规则集中在这里，避免设置、历史、词库和 OCR 各自复制判断 debug/release 目录的逻辑。
inline QString appBasePathForApplicationDir(const QString &applicationDir)
{
    QDir dir(applicationDir);
    const QString folder = dir.dirName().toLower();
    if (folder == QStringLiteral("debug")
        || folder == QStringLiteral("release")) {
        dir.cdUp();
    }
    return dir.absolutePath();
}

inline QString appConfigFilePathForApplicationDir(
    const QString &applicationDir,
    const QString &fileName)
{
    return QDir(appBasePathForApplicationDir(applicationDir))
        .filePath(QStringLiteral("config/") + fileName);
}

QString appBasePath();
inline QString appConfigFilePath(const QString &fileName)
{
    return appConfigFilePathForApplicationDir(
        QCoreApplication::applicationDirPath(),
        fileName
    );
}
QString defaultRecordDirectory();

#endif // VOCEKIT_APP_PATHS_H
