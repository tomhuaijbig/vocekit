#ifndef VOCEKIT_FUNCTION_FLOW_RUNTIME_LOG_H
#define VOCEKIT_FUNCTION_FLOW_RUNTIME_LOG_H

#include "../domain/execution_types.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

struct FunctionFlowRuntimeLogEntry
{
    QString functionId;
    int publishedRevision = 0;
    QString publishedHash;
    ExecutionId runId;
    QString trigger;
    QString nodeId;
    QString nodeType;
    QString status;
    qint64 elapsedMs = -1;
    OperationError error;
    QString modelId;
    QString promptVersion;
    int httpStatus = 0;
};

QJsonObject functionFlowRuntimeLogMetadata(
    const FunctionFlowRuntimeLogEntry &entry
);
QByteArray functionFlowRuntimeLogLine(
    const FunctionFlowRuntimeLogEntry &entry
);
bool appendFunctionFlowRuntimeLog(
    const QString &filePath,
    const FunctionFlowRuntimeLogEntry &entry
);

#endif // VOCEKIT_FUNCTION_FLOW_RUNTIME_LOG_H
