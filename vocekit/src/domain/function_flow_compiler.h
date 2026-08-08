#ifndef VOCEKIT_FUNCTION_FLOW_COMPILER_H
#define VOCEKIT_FUNCTION_FLOW_COMPILER_H

#include "function_flow_graph.h"
#include "function_flow_runtime_types.h"
#include "operation_error.h"

#include <QMap>
#include <QStringList>
#include <QVector>

struct FunctionFlowCompiledInput
{
    QString edgeId;
    QString predecessorNodeId;
    QString predecessorPortId;
    int edgeOrder = 0;
    QString role;
    int sequence = 0;
    bool required = true;
};

struct FunctionFlowCompiledNode
{
    QString nodeId;
    FunctionFlowNodeType type = FunctionFlowNodeType::Input;
    FunctionFlowNodeConfig config;
    QVector<FunctionFlowCompiledInput> inputs;
    QStringList successors;
    QString streamingResultPopupNodeId;
    bool autoWriteFallbackCoveredByExplicitPopup = false;
};

struct FunctionFlowExecutionPlan
{
    QString functionId;
    int publishedRevision = 0;
    QString publishedHash;
    QStringList topologicalNodeIds;
    QMap<QString, FunctionFlowCompiledNode> nodes;
    QMap<FunctionFlowTrigger, FunctionFlowTriggerPlan> triggers;
    QStringList terminalActionNodeIds;
};

struct FunctionFlowCompileResult
{
    bool ok = false;
    FunctionFlowExecutionPlan plan;
    OperationError error;
};

class FunctionFlowCompiler
{
public:
    static FunctionFlowCompileResult compile(
        const FunctionFlowGraph &graph,
        int publishedRevision,
        const QString &publishedHash
    );
};

#endif // VOCEKIT_FUNCTION_FLOW_COMPILER_H
