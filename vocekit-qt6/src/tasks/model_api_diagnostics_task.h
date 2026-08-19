#ifndef VOCEKIT_MODEL_API_DIAGNOSTICS_TASK_H
#define VOCEKIT_MODEL_API_DIAGNOSTICS_TASK_H

#include "../providers/provider_types.h"
#include "cancellation_token.h"

#include <QJsonObject>
#include <QStringList>

struct ModelApiDiagnosticsRequest
{
    QString modelId;
    QString modelNameOverride;
    QString endpointOverride;
    bool useSystemProxy = false;
    CancellationToken cancellation;
};

struct ModelApiDiagnosticsResult
{
    bool success = false;
    int httpStatusCode = 0;
    QString category;
    QString message;
    QByteArray rawResponse;
    QStringList models;
    QJsonObject data;
    qint64 durationMs = -1;
};

ModelApiDiagnosticsResult testModelApiKey(
    const ModelApiDiagnosticsRequest &request
);
ModelApiDiagnosticsResult testModelConnection(
    const ModelApiDiagnosticsRequest &request
);
ModelApiDiagnosticsResult fetchModelApiModels(
    const ModelApiDiagnosticsRequest &request
);

#endif // VOCEKIT_MODEL_API_DIAGNOSTICS_TASK_H
