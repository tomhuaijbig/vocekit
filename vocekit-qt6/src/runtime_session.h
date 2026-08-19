#ifndef VOCEKIT_RUNTIME_SESSION_H
#define VOCEKIT_RUNTIME_SESSION_H

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

struct RuntimeSessionInfo
{
    QString sessionId;
    bool safeMode = false;
    bool automaticSafeMode = false;
    int recentCrashCount = 0;
    QString safeModeReason;
};

// A deterministic decision seam used by the startup path and its regression
// tests. Only crashes newer than both the recovery marker and the time window
// contribute to automatic safe mode.
bool runtimeSafeModeRequired(
    const QVector<QDateTime> &crashTimesUtc,
    const QDateTime &lastRecoveryUtc,
    const QDateTime &nowUtc,
    int threshold = 2,
    int windowSeconds = 10 * 60,
    int *recentCrashCount = nullptr
);

RuntimeSessionInfo beginRuntimeSession(
    const QStringList &arguments,
    const QString &applicationVersion
);
void finishRuntimeSession(int exitCode);
RuntimeSessionInfo currentRuntimeSession();
int runtimeDiagnosticExitDelayMs(const QStringList &arguments);

// Called only by the process-level crash handler. The metadata intentionally
// contains no request text, prompts, API keys, command-line arguments or other
// user content.
QString recordRuntimeCrash(
    const QString &kind,
    quint64 exceptionCode,
    quintptr exceptionAddress,
    const QString &dumpPath
);

#endif // VOCEKIT_RUNTIME_SESSION_H
