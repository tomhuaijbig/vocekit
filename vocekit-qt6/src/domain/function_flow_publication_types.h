#ifndef VOCEKIT_FUNCTION_FLOW_PUBLICATION_TYPES_H
#define VOCEKIT_FUNCTION_FLOW_PUBLICATION_TYPES_H

#include "function_flow_runtime_types.h"
#include "function_flow_validation.h"
#include "operation_error.h"

#include <QMap>
#include <QString>

struct FunctionFlowPublishResult
{
    bool ok = false;
    int publishedRevision = 0;
    FunctionFlowValidationResult validation;
    OperationError error;
};

struct FunctionFlowDraftAnalysis
{
    QString graphHash;
    FunctionFlowValidationResult validation;
    QMap<FunctionFlowTrigger, bool> triggerAvailability;
};

#endif // VOCEKIT_FUNCTION_FLOW_PUBLICATION_TYPES_H
