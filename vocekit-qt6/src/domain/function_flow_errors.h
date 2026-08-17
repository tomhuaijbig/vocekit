#ifndef VOCEKIT_FUNCTION_FLOW_ERRORS_H
#define VOCEKIT_FUNCTION_FLOW_ERRORS_H

#include "operation_error.h"

#include <QString>
#include <QStringList>

QStringList functionFlowStableErrorCodes();
bool isFunctionFlowStableErrorCode(const QString &code);
QString functionFlowErrorFaqId(const QString &code);
QString functionFlowUserMessage(const OperationError &error);

#endif // VOCEKIT_FUNCTION_FLOW_ERRORS_H
