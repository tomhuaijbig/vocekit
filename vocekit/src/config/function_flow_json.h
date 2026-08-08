#ifndef VOCEKIT_FUNCTION_FLOW_JSON_H
#define VOCEKIT_FUNCTION_FLOW_JSON_H

#include "../domain/function_flow_graph.h"

#include <QJsonObject>
#include <QStringList>

FunctionFlowState functionFlowStateFromJson(
    const QJsonObject &object,
    QStringList *warnings = nullptr
);
QJsonObject functionFlowStateToJson(const FunctionFlowState &state);

#endif // VOCEKIT_FUNCTION_FLOW_JSON_H
