#ifndef VOCEKIT_FUNCTION_FLOW_SCHEDULER_H
#define VOCEKIT_FUNCTION_FLOW_SCHEDULER_H

#include "function_flow_compiler.h"

#include <QList>
#include <QMap>
#include <QSet>
#include <QString>

class FunctionFlowScheduler
{
public:
    FunctionFlowScheduler(
        const FunctionFlowExecutionPlan &plan,
        FunctionFlowTrigger trigger
    );

    QString nextReadyNode();
    QList<FunctionFlowValue> inputValues(
        const QString &nodeId
    ) const;
    bool start(
        const QString &nodeId,
        OperationError *error = nullptr
    );
    bool succeed(
        const QString &nodeId,
        const QList<FunctionFlowValue> &values,
        OperationError *error = nullptr
    );
    bool skip(
        const QString &nodeId,
        OperationError *error = nullptr
    );
    bool fail(
        const QString &nodeId,
        const OperationError &failure,
        OperationError *error = nullptr
    );
    bool completeCancelled(
        const QString &nodeId,
        OperationError *error = nullptr
    );
    void cancel();
    FunctionFlowNodeState state(const QString &nodeId) const;
    bool finished() const;
    OperationError terminalError() const;

private:
    bool canBecomeReady(
        const FunctionFlowCompiledNode &node
    ) const;
    bool isTerminal(FunctionFlowNodeState state) const;
    bool isTransitionable(const QString &nodeId) const;
    void failNode(
        const QString &nodeId,
        const OperationError &failure
    );
    void failDeadlock();
    void skipTerminalActions();

    FunctionFlowExecutionPlan m_plan;
    FunctionFlowTrigger m_trigger =
        FunctionFlowTrigger::MainHotkey;
    QMap<QString, FunctionFlowNodeState> m_states;
    QMap<QString, QList<FunctionFlowValue>> m_values;
    QSet<QString> m_activeSourceNodeIds;
    QString m_readyNodeId;
    OperationError m_terminalError;
};

#endif // VOCEKIT_FUNCTION_FLOW_SCHEDULER_H
