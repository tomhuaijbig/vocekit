#include "function_flow_scheduler.h"

namespace {

void clearError(OperationError *error)
{
    if (error) {
        *error = OperationError();
    }
}

void setError(
    OperationError *error,
    const QString &code,
    const QString &message = QString())
{
    if (!error) {
        return;
    }
    error->code = code;
    error->message = message;
}

OperationError schedulerError(
    const QString &code,
    const QString &message = QString())
{
    OperationError error;
    error.code = code;
    error.message = message;
    return error;
}

bool isSource(FunctionFlowNodeType type)
{
    return type == FunctionFlowNodeType::VoiceSource
        || type == FunctionFlowNodeType::SelectionSource
        || type == FunctionFlowNodeType::ScreenshotSource;
}

bool hasPayload(const FunctionFlowValue &value)
{
    return !value.text.trimmed().isEmpty()
        || !value.voice.isNull()
        || !value.screenshot.isNull();
}

bool hasPayload(const QList<FunctionFlowValue> &values)
{
    for (const FunctionFlowValue &value : values) {
        if (hasPayload(value)) {
            return true;
        }
    }
    return false;
}

bool hasText(const QList<FunctionFlowValue> &values)
{
    for (const FunctionFlowValue &value : values) {
        if (!value.text.trimmed().isEmpty()) {
            return true;
        }
    }
    return false;
}

} // namespace

FunctionFlowScheduler::FunctionFlowScheduler(
    const FunctionFlowExecutionPlan &plan,
    FunctionFlowTrigger trigger)
    : m_plan(plan),
      m_trigger(trigger)
{
    const FunctionFlowTriggerPlan triggerPlan =
        m_plan.triggers.value(trigger);
    for (const QString &sourceId :
         triggerPlan.activeSourceNodeIds) {
        m_activeSourceNodeIds.insert(sourceId);
    }

    for (auto it = m_plan.nodes.constBegin();
         it != m_plan.nodes.constEnd();
         ++it) {
        m_states.insert(
            it.key(),
            isSource(it.value().type)
                && !m_activeSourceNodeIds.contains(it.key())
                ? FunctionFlowNodeState::Skipped
                : FunctionFlowNodeState::Pending
        );
    }

    if (!triggerPlan.available) {
        m_terminalError = schedulerError(
            QStringLiteral("flow_trigger_unavailable"),
            QStringLiteral("当前流程不支持此触发入口。")
        );
        for (auto it = m_states.begin();
             it != m_states.end();
             ++it) {
            if (it.value() == FunctionFlowNodeState::Pending) {
                it.value() = FunctionFlowNodeState::Cancelled;
            }
        }
    }
}

QString FunctionFlowScheduler::nextReadyNode()
{
    if (!m_readyNodeId.isEmpty()
        && m_states.value(m_readyNodeId)
            == FunctionFlowNodeState::Ready) {
        return m_readyNodeId;
    }
    m_readyNodeId.clear();

    for (auto it = m_states.constBegin();
         it != m_states.constEnd();
         ++it) {
        if (it.value() == FunctionFlowNodeState::Running
            || it.value() == FunctionFlowNodeState::Cancelling) {
            return QString();
        }
    }
    if (finished()) {
        return QString();
    }

    for (const QString &nodeId : m_plan.topologicalNodeIds) {
        if (m_states.value(
                nodeId,
                FunctionFlowNodeState::Failed
            ) != FunctionFlowNodeState::Pending) {
            continue;
        }
        if (!m_plan.nodes.contains(nodeId)) {
            continue;
        }
        const FunctionFlowCompiledNode &node =
            m_plan.nodes.value(nodeId);
        if (!canBecomeReady(node)) {
            continue;
        }

        if (node.type == FunctionFlowNodeType::Model
            && !hasText(inputValues(nodeId))) {
            failNode(
                nodeId,
                schedulerError(
                    QStringLiteral("flow_model_input_empty"),
                    QStringLiteral("模型没有可处理的文字输入。")
                )
            );
            return QString();
        }

        m_states[nodeId] = FunctionFlowNodeState::Ready;
        m_readyNodeId = nodeId;
        return nodeId;
    }

    failDeadlock();
    return QString();
}

QList<FunctionFlowValue> FunctionFlowScheduler::inputValues(
    const QString &nodeId) const
{
    QList<FunctionFlowValue> result;
    if (!m_plan.nodes.contains(nodeId)) {
        return result;
    }

    const FunctionFlowCompiledNode &node =
        m_plan.nodes.value(nodeId);
    for (const FunctionFlowCompiledInput &input : node.inputs) {
        if (m_states.value(input.predecessorNodeId)
            != FunctionFlowNodeState::Succeeded) {
            continue;
        }
        const QList<FunctionFlowValue> predecessorValues =
            m_values.value(input.predecessorNodeId);
        for (FunctionFlowValue value : predecessorValues) {
            if (!input.role.trimmed().isEmpty()) {
                value.role = input.role;
            }
            value.sequence = input.sequence;
            result.append(value);
        }
    }
    return result;
}

bool FunctionFlowScheduler::start(
    const QString &nodeId,
    OperationError *error)
{
    clearError(error);
    if (!m_plan.nodes.contains(nodeId)) {
        setError(
            error,
            QStringLiteral("flow_scheduler_node_missing"),
            QStringLiteral("调度节点不存在。")
        );
        return false;
    }
    if (m_states.value(nodeId) != FunctionFlowNodeState::Ready
        || m_readyNodeId != nodeId) {
        setError(
            error,
            QStringLiteral("flow_scheduler_state_invalid"),
            QStringLiteral("节点当前不能开始执行。")
        );
        return false;
    }
    m_states[nodeId] = FunctionFlowNodeState::Running;
    m_readyNodeId.clear();
    return true;
}

bool FunctionFlowScheduler::succeed(
    const QString &nodeId,
    const QList<FunctionFlowValue> &values,
    OperationError *error)
{
    clearError(error);
    if (!m_plan.nodes.contains(nodeId)) {
        setError(
            error,
            QStringLiteral("flow_scheduler_node_missing"),
            QStringLiteral("调度节点不存在。")
        );
        return false;
    }
    if (!isTransitionable(nodeId)) {
        setError(
            error,
            QStringLiteral("flow_scheduler_state_invalid"),
            QStringLiteral("节点当前不能完成。")
        );
        return false;
    }

    const FunctionFlowCompiledNode &node =
        m_plan.nodes.value(nodeId);
    QList<FunctionFlowValue> stored = values;
    if (isSource(node.type)
        || node.type == FunctionFlowNodeType::Model) {
        for (FunctionFlowValue &value : stored) {
            if (value.sourceNodeId.trimmed().isEmpty()) {
                value.sourceNodeId = nodeId;
            }
        }
    }

    if (node.type == FunctionFlowNodeType::Input) {
        for (FunctionFlowValue &value : stored) {
            value.role = node.config.input.role;
            value.sequence = node.config.input.sequence;
        }
        if (!hasPayload(stored)) {
            m_readyNodeId.clear();
            if (node.config.input.required) {
                failNode(
                    nodeId,
                    schedulerError(
                        QStringLiteral("flow_required_input_empty"),
                        QStringLiteral("必需输入为空。")
                    )
                );
            } else {
                m_states[nodeId] = FunctionFlowNodeState::Skipped;
                m_values.remove(nodeId);
            }
            return true;
        }
    }

    if (node.type == FunctionFlowNodeType::Model) {
        const QList<FunctionFlowValue> inputs =
            inputValues(nodeId);
        QSharedPointer<const FunctionFlowVoicePayload> voice;
        QSharedPointer<const FunctionFlowScreenshotPayload> screenshot;
        for (const FunctionFlowValue &input : inputs) {
            if (voice.isNull() && !input.voice.isNull()) {
                voice = input.voice;
            }
            if (screenshot.isNull() && !input.screenshot.isNull()) {
                screenshot = input.screenshot;
            }
        }
        for (FunctionFlowValue &value : stored) {
            if (value.voice.isNull()) {
                value.voice = voice;
            }
            if (value.screenshot.isNull()) {
                value.screenshot = screenshot;
            }
        }
    }

    if (node.type == FunctionFlowNodeType::Output
        && !hasText(stored)) {
        if (node.config.output.emptyResultPolicy
            == QStringLiteral("skipActions")) {
            m_states[nodeId] = FunctionFlowNodeState::Succeeded;
            m_values.insert(nodeId, stored);
            m_readyNodeId.clear();
            skipTerminalActions();
            return true;
        }
        m_readyNodeId.clear();
        failNode(
            nodeId,
            schedulerError(
                QStringLiteral("flow_output_empty"),
                QStringLiteral("流程输出为空。")
            )
        );
        return true;
    }

    m_states[nodeId] = FunctionFlowNodeState::Succeeded;
    m_values.insert(nodeId, stored);
    m_readyNodeId.clear();
    return true;
}

bool FunctionFlowScheduler::skip(
    const QString &nodeId,
    OperationError *error)
{
    clearError(error);
    if (!m_plan.nodes.contains(nodeId)) {
        setError(
            error,
            QStringLiteral("flow_scheduler_node_missing"),
            QStringLiteral("调度节点不存在。")
        );
        return false;
    }
    if (!isTransitionable(nodeId)) {
        setError(
            error,
            QStringLiteral("flow_scheduler_state_invalid"),
            QStringLiteral("节点当前不能跳过。")
        );
        return false;
    }
    m_states[nodeId] = FunctionFlowNodeState::Skipped;
    m_values.remove(nodeId);
    m_readyNodeId.clear();
    return true;
}

bool FunctionFlowScheduler::fail(
    const QString &nodeId,
    const OperationError &failure,
    OperationError *error)
{
    clearError(error);
    if (!m_plan.nodes.contains(nodeId)) {
        setError(
            error,
            QStringLiteral("flow_scheduler_node_missing"),
            QStringLiteral("调度节点不存在。")
        );
        return false;
    }
    if (!isTransitionable(nodeId)) {
        setError(
            error,
            QStringLiteral("flow_scheduler_state_invalid"),
            QStringLiteral("节点当前不能失败。")
        );
        return false;
    }

    OperationError effectiveFailure = failure;
    if (effectiveFailure.code.trimmed().isEmpty()) {
        effectiveFailure = schedulerError(
            QStringLiteral("flow_node_failed"),
            QStringLiteral("流程节点执行失败。")
        );
    }
    failNode(nodeId, effectiveFailure);
    return true;
}

bool FunctionFlowScheduler::completeCancelled(
    const QString &nodeId,
    OperationError *error)
{
    clearError(error);
    if (!m_plan.nodes.contains(nodeId)) {
        setError(
            error,
            QStringLiteral("flow_scheduler_node_missing"),
            QStringLiteral("调度节点不存在。")
        );
        return false;
    }
    if (m_states.value(nodeId)
        != FunctionFlowNodeState::Cancelling) {
        setError(
            error,
            QStringLiteral("flow_scheduler_state_invalid"),
            QStringLiteral("节点当前不在取消中。")
        );
        return false;
    }
    m_states[nodeId] = FunctionFlowNodeState::Cancelled;
    return true;
}

void FunctionFlowScheduler::cancel()
{
    if (finished()) {
        return;
    }
    if (m_terminalError.isEmpty()) {
        m_terminalError = schedulerError(
            QStringLiteral("flow_cancelled"),
            QStringLiteral("流程已取消。")
        );
    }
    m_readyNodeId.clear();
    for (auto it = m_states.begin();
         it != m_states.end();
         ++it) {
        if (it.value() == FunctionFlowNodeState::Running) {
            it.value() = FunctionFlowNodeState::Cancelling;
        } else if (it.value() == FunctionFlowNodeState::Pending
                   || it.value() == FunctionFlowNodeState::Ready) {
            it.value() = FunctionFlowNodeState::Cancelled;
        }
    }
}

FunctionFlowNodeState FunctionFlowScheduler::state(
    const QString &nodeId) const
{
    return m_states.value(
        nodeId,
        FunctionFlowNodeState::Failed
    );
}

bool FunctionFlowScheduler::finished() const
{
    for (auto it = m_states.constBegin();
         it != m_states.constEnd();
         ++it) {
        if (!isTerminal(it.value())) {
            return false;
        }
    }
    return true;
}

OperationError FunctionFlowScheduler::terminalError() const
{
    return m_terminalError;
}

bool FunctionFlowScheduler::canBecomeReady(
    const FunctionFlowCompiledNode &node) const
{
    if (isSource(node.type)) {
        return m_activeSourceNodeIds.contains(node.nodeId);
    }
    if (node.inputs.isEmpty()) {
        return false;
    }

    for (const FunctionFlowCompiledInput &input : node.inputs) {
        if (!m_states.contains(input.predecessorNodeId)) {
            return false;
        }
        const FunctionFlowNodeState predecessor =
            m_states.value(input.predecessorNodeId);
        if (predecessor != FunctionFlowNodeState::Succeeded
            && predecessor != FunctionFlowNodeState::Skipped) {
            return false;
        }
    }

    const int actionIndex =
        m_plan.terminalActionNodeIds.indexOf(node.nodeId);
    if (actionIndex >= 0) {
        for (int i = 0; i < actionIndex; ++i) {
            if (!isTerminal(m_states.value(
                    m_plan.terminalActionNodeIds.at(i),
                    FunctionFlowNodeState::Failed
                ))) {
                return false;
            }
        }
    }
    return true;
}

bool FunctionFlowScheduler::isTerminal(
    FunctionFlowNodeState value) const
{
    return value == FunctionFlowNodeState::Succeeded
        || value == FunctionFlowNodeState::Skipped
        || value == FunctionFlowNodeState::Failed
        || value == FunctionFlowNodeState::Blocked
        || value == FunctionFlowNodeState::Cancelled;
}

bool FunctionFlowScheduler::isTransitionable(
    const QString &nodeId) const
{
    const FunctionFlowNodeState current =
        m_states.value(nodeId, FunctionFlowNodeState::Failed);
    return current == FunctionFlowNodeState::Ready
        || current == FunctionFlowNodeState::Running;
}

void FunctionFlowScheduler::failNode(
    const QString &nodeId,
    const OperationError &failure)
{
    m_states[nodeId] = FunctionFlowNodeState::Failed;
    m_values.remove(nodeId);
    m_readyNodeId.clear();
    if (m_terminalError.isEmpty()) {
        m_terminalError = failure;
    }

    QSet<QString> descendants;
    QStringList pending =
        m_plan.nodes.value(nodeId).successors;
    while (!pending.isEmpty()) {
        const QString descendant = pending.takeFirst();
        if (descendants.contains(descendant)) {
            continue;
        }
        descendants.insert(descendant);
        pending.append(
            m_plan.nodes.value(descendant).successors
        );
    }

    for (auto it = m_states.begin();
         it != m_states.end();
         ++it) {
        if (it.key() == nodeId
            || it.value() == FunctionFlowNodeState::Succeeded
            || it.value() == FunctionFlowNodeState::Skipped
            || isTerminal(it.value())) {
            continue;
        }
        it.value() = descendants.contains(it.key())
            ? FunctionFlowNodeState::Blocked
            : FunctionFlowNodeState::Cancelled;
    }
}

void FunctionFlowScheduler::failDeadlock()
{
    if (m_terminalError.isEmpty()) {
        m_terminalError = schedulerError(
            QStringLiteral("flow_scheduler_deadlock"),
            QStringLiteral("流程执行计划无法继续。")
        );
    }
    m_readyNodeId.clear();
    for (auto it = m_states.begin();
         it != m_states.end();
         ++it) {
        if (it.value() == FunctionFlowNodeState::Pending
            || it.value() == FunctionFlowNodeState::Ready) {
            it.value() = FunctionFlowNodeState::Blocked;
        }
    }
}

void FunctionFlowScheduler::skipTerminalActions()
{
    for (const QString &nodeId :
         m_plan.terminalActionNodeIds) {
        if (m_states.value(nodeId)
            == FunctionFlowNodeState::Pending) {
            m_states[nodeId] = FunctionFlowNodeState::Skipped;
        }
    }
}
