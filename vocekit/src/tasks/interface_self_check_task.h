#ifndef VOCEKIT_INTERFACE_SELF_CHECK_TASK_H
#define VOCEKIT_INTERFACE_SELF_CHECK_TASK_H

#include "../config/secret_config.h"
#include "cancellation_token.h"

#include <QString>
#include <QStringList>

struct InterfaceSelfCheckRequest
{
    bool useSystemProxy = false;
    QString target = QStringLiteral("all");
    QString ocrEngine;
    int ocrTimeoutMs = 45000;
    QString applicationBasePath;
    SecretConfig secrets;
    CancellationToken cancellation;
};

QStringList runInterfaceSelfCheckTask(const InterfaceSelfCheckRequest &request);

#endif // VOCEKIT_INTERFACE_SELF_CHECK_TASK_H
