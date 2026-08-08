#include "function_flow_compiler.h"

#include <QMap>
#include <QSet>

#include <algorithm>

namespace {

using NodeMap = QMap<QString, const FunctionFlowNode *>;
using EdgeMap = QMap<QString, QVector<const FunctionFlowEdge *>>;

struct CompileGraph
{
    NodeMap nodes;
    QVector<const FunctionFlowEdge *> edges;
    EdgeMap outgoing;
    EdgeMap incoming;
};

FunctionFlowCompileResult failure(
    const QString &code,
    const QString &message = QString())
{
    FunctionFlowCompileResult result;
    result.error.code = code;
    result.error.message = message.isEmpty() ? code : message;
    return result;
}

bool isSource(FunctionFlowNodeType type)
{
    return type == FunctionFlowNodeType::VoiceSource
        || type == FunctionFlowNodeType::SelectionSource
        || type == FunctionFlowNodeType::ScreenshotSource;
}

bool isAction(FunctionFlowNodeType type)
{
    return type == FunctionFlowNodeType::ResultPopup
        || type == FunctionFlowNodeType::ScreenshotPanel
        || type == FunctionFlowNodeType::AutoWrite;
}

bool containsPort(
    FunctionFlowNodeType type,
    const QString &id)
{
    const QVector<FunctionFlowPortSpec> ports =
        functionFlowPortSpecs(type);
    for (const FunctionFlowPortSpec &port : ports) {
        if (port.id == id) {
            return true;
        }
    }
    return false;
}

int acquisitionSequence(const FunctionFlowNode &node)
{
    switch (node.type) {
    case FunctionFlowNodeType::VoiceSource:
        return node.config.voice.acquisitionSequence;
    case FunctionFlowNodeType::SelectionSource:
        return node.config.selection.acquisitionSequence;
    case FunctionFlowNodeType::ScreenshotSource:
        return node.config.screenshot.acquisitionSequence;
    default:
        return 0;
    }
}

bool sourceLess(
    const FunctionFlowNode *left,
    const FunctionFlowNode *right)
{
    const int leftSequence = acquisitionSequence(*left);
    const int rightSequence = acquisitionSequence(*right);
    if (leftSequence != rightSequence) {
        return leftSequence < rightSequence;
    }
    return left->id < right->id;
}

bool edgeOrderLess(
    const FunctionFlowEdge *left,
    const FunctionFlowEdge *right)
{
    if (left->order != right->order) {
        return left->order < right->order;
    }
    if (left->toNodeId != right->toNodeId) {
        return left->toNodeId < right->toNodeId;
    }
    return left->id < right->id;
}

bool inputNodeLess(
    const FunctionFlowNode *left,
    const FunctionFlowNode *right)
{
    if (left->config.input.sequence
        != right->config.input.sequence) {
        return left->config.input.sequence
            < right->config.input.sequence;
    }
    return left->id < right->id;
}

bool sourceIsActiveForTrigger(
    const FunctionFlowNode &node,
    FunctionFlowTrigger trigger)
{
    if (node.type == FunctionFlowNodeType::VoiceSource
        || node.type == FunctionFlowNodeType::SelectionSource) {
        return trigger == FunctionFlowTrigger::MainHotkey;
    }
    if (node.type != FunctionFlowNodeType::ScreenshotSource) {
        return false;
    }
    const QString mode = node.config.screenshot.triggerMode;
    if (trigger == FunctionFlowTrigger::MainHotkey) {
        return mode == QStringLiteral("primary");
    }
    if (trigger == FunctionFlowTrigger::ScreenshotHotkey) {
        return mode == QStringLiteral("separate")
            || mode == QStringLiteral("separateAndLauncher");
    }
    return mode == QStringLiteral("launcher")
        || mode == QStringLiteral("separateAndLauncher");
}

QSet<QString> reachableFrom(
    const QStringList &starts,
    const EdgeMap &outgoing)
{
    QSet<QString> visited;
    QStringList queue = starts;
    for (const QString &start : starts) {
        visited.insert(start);
    }
    while (!queue.isEmpty()) {
        const QString current = queue.takeFirst();
        for (const FunctionFlowEdge *edge : outgoing.value(current)) {
            if (!visited.contains(edge->toNodeId)) {
                visited.insert(edge->toNodeId);
                queue.append(edge->toNodeId);
            }
        }
    }
    return visited;
}

QSet<QString> reachableTo(
    const QStringList &starts,
    const EdgeMap &incoming)
{
    QSet<QString> visited;
    QStringList queue = starts;
    for (const QString &start : starts) {
        visited.insert(start);
    }
    while (!queue.isEmpty()) {
        const QString current = queue.takeFirst();
        for (const FunctionFlowEdge *edge : incoming.value(current)) {
            if (!visited.contains(edge->fromNodeId)) {
                visited.insert(edge->fromNodeId);
                queue.append(edge->fromNodeId);
            }
        }
    }
    return visited;
}

FunctionFlowTriggerPlan compileTrigger(
    const CompileGraph &graph,
    FunctionFlowTrigger trigger,
    const QString &outputId)
{
    FunctionFlowTriggerPlan plan;
    plan.trigger = trigger;

    QVector<const FunctionFlowNode *> sources;
    for (const FunctionFlowNode *node : graph.nodes) {
        if (isSource(node->type)
            && sourceIsActiveForTrigger(*node, trigger)) {
            sources.append(node);
        }
    }
    std::sort(sources.begin(), sources.end(), sourceLess);
    for (const FunctionFlowNode *source : sources) {
        plan.activeSourceNodeIds.append(source->id);
        plan.acquisitionNodeIds.append(source->id);
        plan.usesHoldToTalk =
            plan.usesHoldToTalk
            || (source->type == FunctionFlowNodeType::VoiceSource
                && source->config.voice.recording.triggerMode
                    == QStringLiteral("hold"));
    }
    if (sources.isEmpty()) {
        return plan;
    }

    const QSet<QString> reachable =
        reachableFrom(plan.activeSourceNodeIds, graph.outgoing);
    if (!reachable.contains(outputId)) {
        return plan;
    }
    for (const FunctionFlowNode *node : graph.nodes) {
        if (node->type == FunctionFlowNodeType::Input
            && node->config.input.required
            && !reachable.contains(node->id)) {
            return plan;
        }
    }
    for (const FunctionFlowEdge *edge : graph.outgoing.value(outputId)) {
        if (reachable.contains(edge->toNodeId)) {
            plan.available = true;
            break;
        }
    }
    return plan;
}

int minimumIncomingOrder(
    const QString &nodeId,
    const EdgeMap &incoming)
{
    const QVector<const FunctionFlowEdge *> edges =
        incoming.value(nodeId);
    if (edges.isEmpty()) {
        return 0;
    }
    int minimum = edges.first()->order;
    for (const FunctionFlowEdge *edge : edges) {
        minimum = qMin(minimum, edge->order);
    }
    return minimum;
}

bool readyNodeLess(
    const QString &leftId,
    const QString &rightId,
    const CompileGraph &graph,
    const QMap<QString, int> &levels)
{
    const FunctionFlowNode *left = graph.nodes.value(leftId);
    const FunctionFlowNode *right = graph.nodes.value(rightId);
    if (levels.value(leftId) != levels.value(rightId)) {
        return levels.value(leftId) < levels.value(rightId);
    }
    if (left->type == FunctionFlowNodeType::Input
        && right->type == FunctionFlowNodeType::Input) {
        return inputNodeLess(left, right);
    }
    if (left->type == FunctionFlowNodeType::Input
        || right->type == FunctionFlowNodeType::Input) {
        return left->type == FunctionFlowNodeType::Input;
    }
    const int leftOrder =
        minimumIncomingOrder(leftId, graph.incoming);
    const int rightOrder =
        minimumIncomingOrder(rightId, graph.incoming);
    if (leftOrder != rightOrder) {
        return leftOrder < rightOrder;
    }
    return leftId < rightId;
}

void sortReady(
    QStringList *ready,
    const CompileGraph &graph,
    const QMap<QString, int> &levels)
{
    std::sort(
        ready->begin(),
        ready->end(),
        [&graph, &levels](
            const QString &left,
            const QString &right) {
            return readyNodeLess(left, right, graph, levels);
        }
    );
}

bool appendReadyNode(
    const QString &nodeId,
    const CompileGraph &graph,
    QMap<QString, int> *indegree,
    QSet<QString> *processed,
    QStringList *order,
    QStringList *ready,
    const QMap<QString, int> &levels)
{
    if (processed->contains(nodeId)) {
        return false;
    }
    processed->insert(nodeId);
    order->append(nodeId);
    for (const FunctionFlowEdge *edge : graph.outgoing.value(nodeId)) {
        const int next = --(*indegree)[edge->toNodeId];
        if (next == 0 && !processed->contains(edge->toNodeId)) {
            ready->append(edge->toNodeId);
        }
    }
    sortReady(ready, graph, levels);
    return true;
}

bool topologicalLevels(
    const CompileGraph &graph,
    QMap<QString, int> *levels)
{
    QMap<QString, int> indegree;
    QStringList ready;
    for (auto it = graph.nodes.constBegin();
         it != graph.nodes.constEnd();
         ++it) {
        indegree.insert(it.key(), 0);
        levels->insert(it.key(), 0);
    }
    for (const FunctionFlowEdge *edge : graph.edges) {
        indegree[edge->toNodeId] += 1;
    }
    for (auto it = indegree.constBegin(); it != indegree.constEnd(); ++it) {
        if (it.value() == 0) {
            ready.append(it.key());
        }
    }
    std::sort(ready.begin(), ready.end());

    int processed = 0;
    while (!ready.isEmpty()) {
        const QString current = ready.takeFirst();
        ++processed;
        for (const FunctionFlowEdge *edge :
             graph.outgoing.value(current)) {
            (*levels)[edge->toNodeId] = qMax(
                levels->value(edge->toNodeId),
                levels->value(current) + 1
            );
            const int next = --indegree[edge->toNodeId];
            if (next == 0) {
                ready.append(edge->toNodeId);
                std::sort(ready.begin(), ready.end());
            }
        }
    }
    return processed == graph.nodes.size();
}

bool topologicalOrder(
    const CompileGraph &graph,
    QStringList *order)
{
    QMap<QString, int> levels;
    if (!topologicalLevels(graph, &levels)) {
        return false;
    }

    QMap<QString, int> indegree;
    QVector<const FunctionFlowNode *> sources;
    for (const FunctionFlowNode *node : graph.nodes) {
        indegree.insert(node->id, 0);
        if (isSource(node->type)) {
            sources.append(node);
        }
    }
    for (const FunctionFlowEdge *edge : graph.edges) {
        indegree[edge->toNodeId] += 1;
    }
    std::sort(sources.begin(), sources.end(), sourceLess);

    QSet<QString> processed;
    QStringList ready;
    for (const FunctionFlowNode *source : sources) {
        if (indegree.value(source->id) != 0) {
            continue;
        }
        appendReadyNode(
            source->id,
            graph,
            &indegree,
            &processed,
            order,
            &ready,
            levels
        );

        QVector<const FunctionFlowNode *> directInputs;
        for (const FunctionFlowEdge *edge :
             graph.outgoing.value(source->id)) {
            const FunctionFlowNode *target =
                graph.nodes.value(edge->toNodeId);
            if (target->type == FunctionFlowNodeType::Input
                && indegree.value(target->id) == 0
                && !processed.contains(target->id)) {
                directInputs.append(target);
            }
        }
        std::sort(
            directInputs.begin(),
            directInputs.end(),
            inputNodeLess
        );
        for (const FunctionFlowNode *input : directInputs) {
            ready.removeAll(input->id);
            appendReadyNode(
                input->id,
                graph,
                &indegree,
                &processed,
                order,
                &ready,
                levels
            );
        }
    }

    for (auto it = indegree.constBegin(); it != indegree.constEnd(); ++it) {
        if (it.value() == 0 && !processed.contains(it.key())) {
            ready.append(it.key());
        }
    }
    ready.removeDuplicates();
    sortReady(&ready, graph, levels);
    while (!ready.isEmpty()) {
        const QString current = ready.takeFirst();
        appendReadyNode(
            current,
            graph,
            &indegree,
            &processed,
            order,
            &ready,
            levels
        );
    }
    return processed.size() == graph.nodes.size();
}

QString portConsistencyError(const CompileGraph &graph)
{
    for (const FunctionFlowNode *node : graph.nodes) {
        const QVector<FunctionFlowPortSpec> ports =
            functionFlowPortSpecs(node->type);
        for (const FunctionFlowPortSpec &port : ports) {
            const QVector<const FunctionFlowEdge *> candidates =
                port.direction == FunctionFlowPortDirection::Input
                ? graph.incoming.value(node->id)
                : graph.outgoing.value(node->id);
            int count = 0;
            for (const FunctionFlowEdge *edge : candidates) {
                const QString edgePort =
                    port.direction == FunctionFlowPortDirection::Input
                    ? edge->toPortId
                    : edge->fromPortId;
                if (edgePort == port.id) {
                    ++count;
                }
            }
            if (port.cardinality == FunctionFlowPortCardinality::One
                && count > 1) {
                return QStringLiteral("flow_port_cardinality");
            }
            if (port.connectionRequired && count == 0) {
                return QStringLiteral("flow_port_connection_missing");
            }
        }
    }
    return QString();
}

bool allEnabledNodesReachable(const CompileGraph &graph)
{
    QStringList sources;
    QStringList actions;
    for (const FunctionFlowNode *node : graph.nodes) {
        if (isSource(node->type)) {
            sources.append(node->id);
        }
        if (isAction(node->type)) {
            actions.append(node->id);
        }
    }
    const QSet<QString> fromSources =
        reachableFrom(sources, graph.outgoing);
    const QSet<QString> toActions =
        reachableTo(actions, graph.incoming);
    for (const FunctionFlowNode *node : graph.nodes) {
        if (!fromSources.contains(node->id)
            || !toActions.contains(node->id)) {
            return false;
        }
    }
    return true;
}

FunctionFlowCompiledInput compiledInput(
    const FunctionFlowEdge &edge,
    const FunctionFlowNode &current,
    const FunctionFlowNode &predecessor)
{
    FunctionFlowCompiledInput input;
    input.edgeId = edge.id;
    input.predecessorNodeId = predecessor.id;
    input.predecessorPortId = edge.fromPortId;
    input.edgeOrder = edge.order;

    const FunctionFlowInputConfig *config = nullptr;
    if (predecessor.type == FunctionFlowNodeType::Input) {
        config = &predecessor.config.input;
    } else if (current.type == FunctionFlowNodeType::Input) {
        config = &current.config.input;
    }
    if (config) {
        input.role = config->role;
        input.sequence = config->sequence;
        input.required = config->required;
    }
    return input;
}

bool compiledInputLess(
    const FunctionFlowCompiledInput &left,
    const FunctionFlowCompiledInput &right)
{
    if (left.sequence != right.sequence) {
        return left.sequence < right.sequence;
    }
    if (left.predecessorNodeId != right.predecessorNodeId) {
        return left.predecessorNodeId < right.predecessorNodeId;
    }
    if (left.edgeOrder != right.edgeOrder) {
        return left.edgeOrder < right.edgeOrder;
    }
    return left.edgeId < right.edgeId;
}

} // namespace

FunctionFlowCompileResult FunctionFlowCompiler::compile(
    const FunctionFlowGraph &input,
    int publishedRevision,
    const QString &publishedHash)
{
    const FunctionFlowGraph graph =
        normalizeFunctionFlowGraph(input);
    if (graph.nodes.isEmpty()) {
        return failure(QStringLiteral("flow_empty"));
    }
    if (graph.nodes.size() > 128 || graph.edges.size() > 256) {
        return failure(QStringLiteral("flow_size_limit"));
    }

    QSet<QString> nodeIds;
    CompileGraph active;
    NodeMap allNodes;
    for (const FunctionFlowNode &node : graph.nodes) {
        if (node.id.isEmpty()) {
            return failure(
                QStringLiteral("flow_node_config_invalid")
            );
        }
        if (nodeIds.contains(node.id)) {
            return failure(
                QStringLiteral("flow_duplicate_node_id")
            );
        }
        nodeIds.insert(node.id);
        allNodes.insert(node.id, &node);
        if (node.enabled) {
            active.nodes.insert(node.id, &node);
        }
    }

    QSet<QString> edgeIds;
    QSet<QString> connections;
    for (const FunctionFlowEdge &edge : graph.edges) {
        const FunctionFlowNode *from =
            allNodes.value(edge.fromNodeId, nullptr);
        const FunctionFlowNode *to =
            allNodes.value(edge.toNodeId, nullptr);
        if (!from || !to) {
            return failure(QStringLiteral("flow_dangling_edge"));
        }
        if (!from->enabled || !to->enabled) {
            continue;
        }
        if (edge.id.isEmpty()) {
            return failure(
                QStringLiteral("flow_node_config_invalid")
            );
        }
        if (edgeIds.contains(edge.id)) {
            return failure(
                QStringLiteral("flow_duplicate_edge_id")
            );
        }
        edgeIds.insert(edge.id);
        const QString connection =
            edge.fromNodeId + QChar(0x1f)
            + edge.fromPortId + QChar(0x1f)
            + edge.toNodeId + QChar(0x1f)
            + edge.toPortId;
        if (connections.contains(connection)) {
            return failure(
                QStringLiteral("flow_duplicate_connection")
            );
        }
        connections.insert(connection);
        if (edge.order < 0 || edge.order > 10000) {
            return failure(
                QStringLiteral("flow_edge_order_invalid")
            );
        }
        if (edge.fromNodeId == edge.toNodeId) {
            return failure(QStringLiteral("flow_self_edge"));
        }
        if (!containsPort(from->type, edge.fromPortId)
            || !containsPort(to->type, edge.toPortId)) {
            return failure(QStringLiteral("flow_unknown_port"));
        }
        if (!hasFunctionFlowPort(
                from->type,
                edge.fromPortId,
                FunctionFlowPortDirection::Output)
            || !hasFunctionFlowPort(
                to->type,
                edge.toPortId,
                FunctionFlowPortDirection::Input)) {
            return failure(QStringLiteral("flow_port_direction"));
        }
        if (!isFunctionFlowConnectionAllowed(
                from->type,
                edge.fromPortId,
                to->type,
                edge.toPortId)) {
            return failure(
                QStringLiteral("flow_edge_type_unsupported")
            );
        }
        active.edges.append(&edge);
        active.outgoing[edge.fromNodeId].append(&edge);
        active.incoming[edge.toNodeId].append(&edge);
    }
    for (auto it = active.outgoing.begin();
         it != active.outgoing.end();
         ++it) {
        std::sort(it.value().begin(), it.value().end(), edgeOrderLess);
    }

    QString outputId;
    int outputCount = 0;
    for (const FunctionFlowNode *node : active.nodes) {
        if (node->type == FunctionFlowNodeType::Output) {
            ++outputCount;
            outputId = node->id;
        }
    }
    if (outputCount != 1) {
        return failure(QStringLiteral("flow_output_count"));
    }

    QVector<const FunctionFlowEdge *> actionEdges =
        active.outgoing.value(outputId);
    actionEdges.erase(
        std::remove_if(
            actionEdges.begin(),
            actionEdges.end(),
            [&active](const FunctionFlowEdge *edge) {
                const FunctionFlowNode *target =
                    active.nodes.value(edge->toNodeId, nullptr);
                return !target || !isAction(target->type);
            }
        ),
        actionEdges.end()
    );
    if (actionEdges.isEmpty()) {
        return failure(QStringLiteral("flow_output_count"));
    }
    bool hasExplicitPopup = false;
    for (const FunctionFlowEdge *edge : actionEdges) {
        const FunctionFlowNode *target =
            active.nodes.value(edge->toNodeId, nullptr);
        hasExplicitPopup =
            hasExplicitPopup
            || (target
                && target->type
                    == FunctionFlowNodeType::ResultPopup);
    }

    QStringList topologicalNodeIds;
    if (!topologicalOrder(active, &topologicalNodeIds)) {
        return failure(QStringLiteral("flow_cycle"));
    }

    const QString portError = portConsistencyError(active);
    if (!portError.isEmpty()) {
        return failure(portError);
    }
    if (!allEnabledNodesReachable(active)) {
        return failure(
            QStringLiteral("flow_enabled_node_unreachable")
        );
    }

    FunctionFlowExecutionPlan plan;
    plan.publishedRevision = publishedRevision;
    plan.publishedHash = publishedHash;
    plan.topologicalNodeIds = topologicalNodeIds;

    for (const FunctionFlowNode *node : active.nodes) {
        FunctionFlowCompiledNode compiled;
        compiled.nodeId = node->id;
        compiled.type = node->type;
        compiled.config = node->config;
        compiled.autoWriteFallbackCoveredByExplicitPopup =
            node->type == FunctionFlowNodeType::AutoWrite
            && node->config.autoWrite.fallbackToPopup
            && hasExplicitPopup;

        for (const FunctionFlowEdge *incoming :
             active.incoming.value(node->id)) {
            const FunctionFlowNode *predecessor =
                active.nodes.value(incoming->fromNodeId);
            compiled.inputs.append(
                compiledInput(*incoming, *node, *predecessor)
            );
        }
        std::sort(
            compiled.inputs.begin(),
            compiled.inputs.end(),
            compiledInputLess
        );
        for (const FunctionFlowEdge *outgoing :
             active.outgoing.value(node->id)) {
            compiled.successors.append(outgoing->toNodeId);
        }
        plan.nodes.insert(node->id, compiled);
    }

    std::sort(
        actionEdges.begin(),
        actionEdges.end(),
        edgeOrderLess
    );
    for (const FunctionFlowEdge *edge : actionEdges) {
        const FunctionFlowNode *target =
            active.nodes.value(edge->toNodeId);
        if (target && isAction(target->type)) {
            plan.terminalActionNodeIds.append(target->id);
        }
    }

    const FunctionFlowTrigger triggers[] = {
        FunctionFlowTrigger::MainHotkey,
        FunctionFlowTrigger::ScreenshotHotkey,
        FunctionFlowTrigger::ScreenshotLauncher
    };
    for (FunctionFlowTrigger trigger : triggers) {
        plan.triggers.insert(
            trigger,
            compileTrigger(active, trigger, outputId)
        );
    }

    int modelCount = 0;
    QString streamingModelId;
    for (const FunctionFlowNode *node : active.nodes) {
        if (node->type == FunctionFlowNodeType::Model) {
            ++modelCount;
            if (node->config.model.stream) {
                streamingModelId = node->id;
            }
        }
    }
    if (!streamingModelId.isEmpty()) {
        const QVector<const FunctionFlowEdge *> modelEdges =
            active.outgoing.value(streamingModelId);
        if (modelCount != 1
            || modelEdges.size() != 1
            || modelEdges.first()->toNodeId != outputId
            || actionEdges.size() != 1
            || active.nodes.value(actionEdges.first()->toNodeId)->type
                != FunctionFlowNodeType::ResultPopup) {
            return failure(
                QStringLiteral("flow_stream_topology_unsupported")
            );
        }
        plan.nodes[streamingModelId]
            .streamingResultPopupNodeId =
                actionEdges.first()->toNodeId;
    }

    FunctionFlowCompileResult result;
    result.ok = true;
    result.plan = plan;
    return result;
}
