#ifndef VOCEKIT_NETWORK_DIAGNOSTICS_TASK_H
#define VOCEKIT_NETWORK_DIAGNOSTICS_TASK_H

#include "../config/secret_config.h"
#include "cancellation_token.h"

#include <QStringList>

struct NetworkDiagnosticsRequest
{
    bool useSystemProxy = false;
    SecretConfig secrets;
    CancellationToken cancellation;
};

QStringList runNetworkDiagnosticsTask(const NetworkDiagnosticsRequest &request);

#endif // VOCEKIT_NETWORK_DIAGNOSTICS_TASK_H
