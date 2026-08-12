#ifndef VOCEKIT_INTERFACE_SELF_CHECK_TASK_H
#define VOCEKIT_INTERFACE_SELF_CHECK_TASK_H

#include "../config/secret_config.h"
#include "cancellation_token.h"

#include <QString>
#include <QStringList>

#include <functional>

typedef std::function<QStringList(
    const QString &,
    const QString &,
    const CancellationToken &
)> WindowsSpeechSelfCheckProbe;

struct InterfaceSelfCheckRequest
{
    bool useSystemProxy = false;
    QString target = QStringLiteral("all");
    QString ocrEngine;
    int ocrTimeoutMs = 45000;
    QString applicationBasePath;
    QString applicationDirPath;
    QString windowsSpeechLanguage;
    WindowsSpeechSelfCheckProbe windowsSpeechProbe;
    SecretConfig secrets;
    CancellationToken cancellation;
};

QStringList runInterfaceSelfCheckTask(const InterfaceSelfCheckRequest &request);

#endif // VOCEKIT_INTERFACE_SELF_CHECK_TASK_H
