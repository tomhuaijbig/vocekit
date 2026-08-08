#ifndef VOCEKIT_FUNCTION_FLOW_MODEL_MESSAGE_H
#define VOCEKIT_FUNCTION_FLOW_MODEL_MESSAGE_H

#include "function_flow_runtime_types.h"

QString buildFunctionFlowUserPrompt(
    const QList<FunctionFlowValue> &values
);

#endif // VOCEKIT_FUNCTION_FLOW_MODEL_MESSAGE_H
