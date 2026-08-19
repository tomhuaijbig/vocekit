#include "runtime_session.h"

#include "runtime_log.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

namespace {

RuntimeSessionInfo g_session;
QString g_applicationVersion;
QDateTime g_startedUtc;

bool writeJsonAtomically(const QString &path, const QJsonObject &object)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.dir().absolutePath())) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

QJsonObject readJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    return error.error == QJsonParseError::NoError && document.isObject()
        ? document.object()
        : QJsonObject();
}

QDateTime parseUtc(const QJsonValue &value)
{
    QDateTime parsed = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(value.toString(), Qt::ISODate);
    }
    return parsed.isValid() ? parsed.toUTC() : QDateTime();
}

QVector<QDateTime> crashTimes(const QString &crashDirectory)
{
    QVector<QDateTime> result;
    const QDir dir(crashDirectory);
    const QFileInfoList files = dir.entryInfoList(
        QStringList() << QStringLiteral("crash-*.json"),
        QDir::Files | QDir::Readable,
        QDir::Name
    );
    result.reserve(files.size());
    for (const QFileInfo &file : files) {
        const QDateTime timestamp = parseUtc(
            readJsonObject(file.absoluteFilePath()).value(QStringLiteral("timestamp_utc"))
        );
        if (timestamp.isValid()) {
            result.append(timestamp);
        }
    }
    return result;
}

QString sessionStatePath()
{
    return QDir(runtimeLogDirectory()).filePath(QStringLiteral("session-last.json"));
}

QString healthStatePath()
{
    return QDir(runtimeLogDirectory()).filePath(QStringLiteral("runtime-health.json"));
}

QJsonObject sessionObject(bool cleanExit, int exitCode)
{
    QJsonObject object;
    object.insert(QStringLiteral("schema_version"), 1);
    object.insert(QStringLiteral("session_id"), g_session.sessionId);
    object.insert(QStringLiteral("application_version"), g_applicationVersion);
    object.insert(QStringLiteral("pid"), static_cast<qint64>(QCoreApplication::applicationPid()));
    object.insert(QStringLiteral("executable"), QFileInfo(QCoreApplication::applicationFilePath()).fileName());
    object.insert(QStringLiteral("working_directory"), QDir::current().dirName());
    object.insert(QStringLiteral("started_utc"), g_startedUtc.toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("safe_mode"), g_session.safeMode);
    object.insert(QStringLiteral("automatic_safe_mode"), g_session.automaticSafeMode);
    object.insert(QStringLiteral("recent_crash_count"), g_session.recentCrashCount);
    object.insert(QStringLiteral("clean_exit"), cleanExit);
    if (cleanExit) {
        object.insert(
            QStringLiteral("ended_utc"),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
        );
        object.insert(QStringLiteral("exit_code"), exitCode);
    }
    return object;
}

} // namespace

bool runtimeSafeModeRequired(
    const QVector<QDateTime> &crashTimesUtc,
    const QDateTime &lastRecoveryUtc,
    const QDateTime &nowUtc,
    int threshold,
    int windowSeconds,
    int *recentCrashCount)
{
    int count = 0;
    const QDateTime normalizedNow = nowUtc.toUTC();
    const QDateTime lowerBound = normalizedNow.addSecs(-qMax(1, windowSeconds));
    const QDateTime normalizedRecovery = lastRecoveryUtc.isValid()
        ? lastRecoveryUtc.toUTC()
        : QDateTime();
    for (const QDateTime &value : crashTimesUtc) {
        if (!value.isValid()) {
            continue;
        }
        const QDateTime timestamp = value.toUTC();
        if (timestamp < lowerBound || timestamp > normalizedNow) {
            continue;
        }
        if (normalizedRecovery.isValid() && timestamp <= normalizedRecovery) {
            continue;
        }
        ++count;
    }
    if (recentCrashCount) {
        *recentCrashCount = count;
    }
    return count >= qMax(1, threshold);
}

RuntimeSessionInfo beginRuntimeSession(
    const QStringList &arguments,
    const QString &applicationVersion)
{
    g_applicationVersion = applicationVersion.trimmed();
    g_startedUtc = QDateTime::currentDateTimeUtc();
    g_session = RuntimeSessionInfo();
    g_session.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    const bool manualSafeMode = arguments.contains(
        QStringLiteral("--safe-mode"),
        Qt::CaseInsensitive
    );
    const QJsonObject health = readJsonObject(healthStatePath());
    const QDateTime lastRecovery = parseUtc(
        health.value(QStringLiteral("last_safe_mode_recovery_utc"))
    );
    const QString crashDirectory = QDir(runtimeLogDirectory()).filePath(
        QStringLiteral("crashes")
    );
    const bool automaticSafeMode = runtimeSafeModeRequired(
        crashTimes(crashDirectory),
        lastRecovery,
        g_startedUtc,
        2,
        10 * 60,
        &g_session.recentCrashCount
    );
    g_session.automaticSafeMode = automaticSafeMode;
    g_session.safeMode = manualSafeMode || automaticSafeMode;
    if (automaticSafeMode) {
        g_session.safeModeReason = QStringLiteral("recent_repeated_crashes");
    } else if (manualSafeMode) {
        g_session.safeModeReason = QStringLiteral("command_line");
    }

    setRuntimeLogSessionId(g_session.sessionId);
    writeJsonAtomically(sessionStatePath(), sessionObject(false, 0));
    return g_session;
}

void finishRuntimeSession(int exitCode)
{
    if (g_session.sessionId.isEmpty()) {
        return;
    }
    writeJsonAtomically(sessionStatePath(), sessionObject(true, exitCode));
    if (g_session.safeMode && exitCode == 0) {
        QJsonObject health = readJsonObject(healthStatePath());
        health.insert(QStringLiteral("schema_version"), 1);
        health.insert(
            QStringLiteral("last_safe_mode_recovery_utc"),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
        );
        health.insert(QStringLiteral("recovery_session_id"), g_session.sessionId);
        writeJsonAtomically(healthStatePath(), health);
    }
}

RuntimeSessionInfo currentRuntimeSession()
{
    return g_session;
}

int runtimeDiagnosticExitDelayMs(const QStringList &arguments)
{
    const QString prefix = QStringLiteral("--diagnostic-exit-after-ms=");
    for (const QString &argument : arguments) {
        if (!argument.startsWith(prefix, Qt::CaseInsensitive)) {
            continue;
        }
        bool ok = false;
        const int value = argument.mid(prefix.size()).toInt(&ok);
        return ok && value >= 100 && value <= 60000 ? value : -1;
    }
    return -1;
}

QString recordRuntimeCrash(
    const QString &kind,
    quint64 exceptionCode,
    quintptr exceptionAddress,
    const QString &dumpPath,
    const QString &dumpErrorStage,
    quint64 dumpErrorCode,
    const QString &dumpMode,
    bool dumpFallbackUsed,
    quint64 dumpPrimaryErrorCode)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString safeSessionId = g_session.sessionId.isEmpty()
        ? QStringLiteral("unknown-session")
        : g_session.sessionId;
    const QString stamp = now.toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString crashDirectory = QDir(runtimeLogDirectory()).filePath(
        QStringLiteral("crashes")
    );
    const QString metadataPath = QDir(crashDirectory).filePath(
        QStringLiteral("crash-%1-%2.json").arg(stamp, safeSessionId)
    );

    QJsonObject object;
    object.insert(QStringLiteral("schema_version"), 2);
    object.insert(QStringLiteral("session_id"), safeSessionId);
    object.insert(QStringLiteral("timestamp_utc"), now.toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("kind"), kind.left(80));
    object.insert(
        QStringLiteral("exception_code"),
        QStringLiteral("0x") + QString::number(exceptionCode, 16).toUpper()
    );
    object.insert(
        QStringLiteral("exception_address"),
        QStringLiteral("0x") + QString::number(exceptionAddress, 16).toUpper()
    );
    object.insert(QStringLiteral("dump_file"), QFileInfo(dumpPath).fileName());
    object.insert(QStringLiteral("dump_written"), !dumpPath.isEmpty());
    object.insert(QStringLiteral("dump_mode"), dumpMode.left(40));
    object.insert(QStringLiteral("dump_fallback_used"), dumpFallbackUsed);
    object.insert(
        QStringLiteral("dump_primary_error_code"),
        static_cast<qint64>(dumpPrimaryErrorCode)
    );
    object.insert(
        QStringLiteral("dump_primary_error_code_hex"),
        QStringLiteral("0x") + QStringLiteral("%1").arg(
            dumpPrimaryErrorCode,
            8,
            16,
            QLatin1Char('0')
        ).toUpper()
    );
    object.insert(QStringLiteral("dump_error_stage"), dumpErrorStage.left(80));
    object.insert(
        QStringLiteral("dump_error_code"),
        static_cast<qint64>(dumpErrorCode)
    );
    object.insert(
        QStringLiteral("dump_error_code_hex"),
        QStringLiteral("0x") + QStringLiteral("%1").arg(
            dumpErrorCode,
            8,
            16,
            QLatin1Char('0')
        ).toUpper()
    );
    object.insert(QStringLiteral("last_action_file"), QStringLiteral("../last_action.txt"));
    writeJsonAtomically(metadataPath, object);
    writeJsonAtomically(
        QDir(runtimeLogDirectory()).filePath(QStringLiteral("last_crash.json")),
        object
    );
    return metadataPath;
}
