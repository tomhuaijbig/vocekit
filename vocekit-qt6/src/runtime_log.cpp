#include "runtime_log.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>

static QString runtimeAppBasePath()
{
    QDir dir(QCoreApplication::applicationDirPath());
    const QString folder = dir.dirName().toLower();
    if (folder == QStringLiteral("debug") || folder == QStringLiteral("release")) {
        dir.cdUp();
    }
    return dir.absolutePath();
}

static QString compactRuntimeLogText(QString text, int maxLength = 700)
{
    text.replace(QRegularExpression(QStringLiteral("[\\r\\n\\t]+")), QStringLiteral(" "));
    text = text.trimmed();
    if (text.size() > maxLength) {
        text = text.left(maxLength) + QStringLiteral("...");
    }
    return text;
}

static bool writeRuntimeBytesAtomically(const QString &path, const QByteArray &data)
{
    QFileInfo info(path);
    QDir().mkpath(info.dir().absolutePath());
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

QString runtimeLogDirectory()
{
    return QDir(runtimeAppBasePath()).filePath(QStringLiteral("logs"));
}

static QString runtimeLogLine(const QString &category, const QString &action, const QString &detail, qint64 elapsedMs)
{
    QStringList parts;
    parts << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
          << compactRuntimeLogText(category, 80)
          << compactRuntimeLogText(action, 140);
    if (elapsedMs >= 0) {
        parts << (QStringLiteral("耗时=") + QString::number(elapsedMs) + QStringLiteral("ms"));
    }
    if (!detail.trimmed().isEmpty()) {
        parts << compactRuntimeLogText(detail);
    }
    return parts.join(QStringLiteral(" | "));
}

void logRuntimeEvent(const QString &category, const QString &action, const QString &detail, qint64 elapsedMs)
{
    const QDir dir(runtimeLogDirectory());
    if (!dir.exists()) {
        QDir().mkpath(dir.absolutePath());
    }

    const QString line = runtimeLogLine(category, action, detail, elapsedMs);
    QFile file(dir.filePath(QStringLiteral("vocekit-") + QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")) + QStringLiteral(".log")));
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        stream.setGenerateByteOrderMark(file.size() == 0);
        stream << line << '\n';
    }

    writeRuntimeBytesAtomically(dir.filePath(QStringLiteral("last_action.txt")), line.toUtf8());
}
