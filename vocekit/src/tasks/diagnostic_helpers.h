#ifndef VOCEKIT_DIAGNOSTIC_HELPERS_H
#define VOCEKIT_DIAGNOSTIC_HELPERS_H

#include "cancellation_token.h"

#include <QByteArray>
#include <QString>
#include <QUrl>

// 测试工具页共用的诊断格式和轻量探测逻辑，避免散落在主窗口里。
QString compactDiagnosticError(const QString &error);
QString diagnosticStatusLine(const QString &name, const QString &status, const QString &detail = QString());
int pcm16PeakLevel(const QByteArray &pcm);
QString networkDnsLookupLine(
    const QString &host,
    const CancellationToken &cancellation = CancellationToken(),
    int timeoutMs = 5000
);
QString networkProbeLine(
    const QString &name,
    const QUrl &url,
    bool useSystemProxy,
    const CancellationToken &cancellation = CancellationToken()
);

#endif // VOCEKIT_DIAGNOSTIC_HELPERS_H
