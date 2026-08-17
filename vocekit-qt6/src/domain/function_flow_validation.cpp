#include "function_flow_validation.h"

#include "function_flow_runtime_types.h"
#include "../input/hotkey_parser.h"

#include <QChar>
#include <QMap>
#include <QQueue>
#include <QSet>

#include <algorithm>

namespace {

using NodeMap = QMap<QString, const FunctionFlowNode *>;
using EdgeMap = QMap<QString, QVector<const FunctionFlowEdge *>>;

struct ActiveGraph
{
    NodeMap nodes;
    QVector<const FunctionFlowEdge *> edges;
    EdgeMap outgoing;
    EdgeMap incoming;
};

void addIssue(
    FunctionFlowValidationResult *result,
    const QString &code,
    const QString &nodeId = QString(),
    const QString &edgeId = QString(),
    const QString &message = QString())
{
    FunctionFlowValidationIssue issue;
    issue.code = code;
    issue.nodeId = nodeId;
    issue.edgeId = edgeId;
    issue.message = message.isEmpty() ? code : message;
    result->issues.append(issue);
    if (!result->issueCodes.contains(code)) {
        result->issueCodes.append(code);
    }
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

bool isOneOf(const QString &value, const QStringList &allowed)
{
    return allowed.contains(value);
}

bool inRange(int value, int minimum, int maximum)
{
    return value >= minimum && value <= maximum;
}

bool validOpacity(int value)
{
    return value == -1 || inRange(value, 20, 100);
}

bool roleContainsForbiddenCharacter(const QString &role)
{
    for (const QChar character : role) {
        if (character == QLatin1Char('[')
            || character == QLatin1Char(']')
            || character.category() == QChar::Other_Control) {
            return true;
        }
    }
    return false;
}

void validateNodeConfig(
    const FunctionFlowNode &node,
    const FunctionFlowValidationContext &context,
    FunctionFlowValidationResult *result)
{
    const QString configCode =
        QStringLiteral("flow_node_config_invalid");
    if (node.title.size() > 80) {
        addIssue(result, configCode, node.id);
    }

    switch (node.type) {
    case FunctionFlowNodeType::VoiceSource: {
        const FunctionFlowVoiceSourceConfig &config = node.config.voice;
        const FunctionFlowRecordingConfig &recording = config.recording;
        if (!isOneOf(
                recording.triggerMode,
                QStringList()
                    << QStringLiteral("toggle")
                    << QStringLiteral("hold"))
            || !inRange(recording.segmentSeconds, 20, 55)
            || !inRange(recording.maximumMinutes, 1, 30)
            || !inRange(recording.countdownSeconds, 0, 60)
            || !inRange(config.acquisitionSequence, 0, 10000)
            || !isOneOf(
                config.networkPolicy,
                QStringList()
                    << QStringLiteral("inherit")
                    << QStringLiteral("direct")
                    << QStringLiteral("systemProxy"))) {
            addIssue(result, configCode, node.id);
        }
        const QString providerId = config.speechProviderId.isEmpty()
            ? context.references.defaultSpeechProviderId
            : config.speechProviderId;
        if (providerId.isEmpty()
            || !context.references.speechProviderIds.contains(providerId)) {
            addIssue(
                result,
                QStringLiteral("flow_speech_provider_reference_missing"),
                node.id
            );
        }
        break;
    }
    case FunctionFlowNodeType::SelectionSource:
        if (!inRange(
                node.config.selection.acquisitionSequence,
                0,
                10000)) {
            addIssue(result, configCode, node.id);
        }
        break;
    case FunctionFlowNodeType::ScreenshotSource: {
        const FunctionFlowScreenshotSourceConfig &config =
            node.config.screenshot;
        if (!inRange(config.timeoutMs, 1000, 120000)
            || !inRange(config.acquisitionSequence, 0, 10000)
            || !isOneOf(
                config.triggerMode,
                QStringList()
                    << QStringLiteral("primary")
                    << QStringLiteral("separate")
                    << QStringLiteral("launcher")
                    << QStringLiteral("separateAndLauncher"))
            || !isOneOf(
                config.networkPolicy,
                QStringList()
                    << QStringLiteral("inherit")
                    << QStringLiteral("direct")
                    << QStringLiteral("systemProxy"))) {
            addIssue(result, configCode, node.id);
        }
        const QString engineId = config.ocrEngineId.isEmpty()
            ? context.references.defaultOcrEngineId
            : config.ocrEngineId;
        if (engineId.isEmpty()
            || !context.references.ocrEngineIds.contains(engineId)) {
            addIssue(
                result,
                QStringLiteral("flow_ocr_engine_reference_missing"),
                node.id
            );
        }
        break;
    }
    case FunctionFlowNodeType::Input:
        if (!inRange(node.config.input.sequence, 0, 10000)) {
            addIssue(result, configCode, node.id);
        }
        if (node.config.input.role.size() > 40
            || roleContainsForbiddenCharacter(node.config.input.role)) {
            addIssue(
                result,
                QStringLiteral("flow_input_role_invalid"),
                node.id
            );
        }
        break;
    case FunctionFlowNodeType::Model:
        if (!isOneOf(
                node.config.model.networkPolicy,
                QStringList()
                    << QStringLiteral("inherit")
                    << QStringLiteral("direct")
                    << QStringLiteral("systemProxy"))) {
            addIssue(result, configCode, node.id);
        }
        if (node.config.model.modelId.isEmpty()
            || !context.references.modelIds.contains(
                node.config.model.modelId)) {
            addIssue(
                result,
                QStringLiteral("flow_model_reference_missing"),
                node.id
            );
        }
        if (node.config.model.promptId.isEmpty()
            || !context.references.promptIds.contains(
                node.config.model.promptId)) {
            addIssue(
                result,
                QStringLiteral("flow_prompt_reference_missing"),
                node.id
            );
        }
        break;
    case FunctionFlowNodeType::Output:
        if (!isOneOf(
                node.config.output.emptyResultPolicy,
                QStringList()
                    << QStringLiteral("fail")
                    << QStringLiteral("skipActions"))) {
            addIssue(result, configCode, node.id);
        }
        break;
    case FunctionFlowNodeType::ResultPopup: {
        const FunctionFlowResultPopupConfig &config = node.config.popup;
        if (!isOneOf(
                config.resultTemplate,
                QStringList()
                    << QStringLiteral("simple")
                    << QStringLiteral("detail")
                    << QStringLiteral("compare")
                    << QStringLiteral("outputOnly"))
            || !inRange(config.displaySeconds, 0, 600)
            || !validOpacity(config.opacity)) {
            addIssue(result, configCode, node.id);
        }
        QSet<QString> seenActions;
        for (const QString &actionId : config.resultActions) {
            if (!isFunctionFlowPopupActionSupported(actionId)) {
                addIssue(
                    result,
                    QStringLiteral("flow_popup_action_unsupported"),
                    node.id
                );
            } else if (seenActions.contains(actionId)) {
                addIssue(
                    result,
                    QStringLiteral("flow_popup_action_duplicate"),
                    node.id
                );
            }
            seenActions.insert(actionId);
        }
        break;
    }
    case FunctionFlowNodeType::ScreenshotPanel:
        if (!inRange(
                node.config.screenshotPanel.displaySeconds,
                0,
                600)
            || !validOpacity(node.config.screenshotPanel.opacity)) {
            addIssue(result, configCode, node.id);
        }
        break;
    case FunctionFlowNodeType::AutoWrite:
        if (!isOneOf(
                node.config.autoWrite.writeMode,
                QStringList()
                    << QStringLiteral("insert")
                    << QStringLiteral("replace"))) {
            addIssue(result, configCode, node.id);
        }
        break;
    }
}

QSet<QString> traverse(
    const QStringList &starts,
    const EdgeMap &edges,
    bool forward)
{
    QSet<QString> visited;
    QQueue<QString> queue;
    for (const QString &start : starts) {
        if (!visited.contains(start)) {
            visited.insert(start);
            queue.enqueue(start);
        }
    }
    while (!queue.isEmpty()) {
        const QString current = queue.dequeue();
        for (const FunctionFlowEdge *edge : edges.value(current)) {
            const QString next = forward
                ? edge->toNodeId
                : edge->fromNodeId;
            if (!visited.contains(next)) {
                visited.insert(next);
                queue.enqueue(next);
            }
        }
    }
    return visited;
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

QVector<const FunctionFlowNode *> activeSourcesForTrigger(
    const ActiveGraph &graph,
    FunctionFlowTrigger trigger)
{
    QVector<const FunctionFlowNode *> sources;
    for (const FunctionFlowNode *node : graph.nodes) {
        if (isSource(node->type)
            && sourceIsActiveForTrigger(*node, trigger)) {
            sources.append(node);
        }
    }
    std::sort(sources.begin(), sources.end(), sourceLess);
    return sources;
}

QStringList sourceIds(
    const QVector<const FunctionFlowNode *> &sources)
{
    QStringList ids;
    for (const FunctionFlowNode *source : sources) {
        ids.append(source->id);
    }
    return ids;
}

bool nativeHotkeysEqual(
    const QString &left,
    const QString &right)
{
    NativeHotkey leftHotkey;
    NativeHotkey rightHotkey;
    return parseNativeHotkey(left, &leftHotkey)
        && parseNativeHotkey(right, &rightHotkey)
        && leftHotkey.modifiers == rightHotkey.modifiers
        && leftHotkey.key == rightHotkey.key;
}

bool conflictsWithOccupiedShortcut(
    const QString &shortcut,
    const FunctionFlowValidationContext &context)
{
    for (auto it = context.occupiedShortcutOwners.constBegin();
         it != context.occupiedShortcutOwners.constEnd();
         ++it) {
        if (it.value() == context.functionId
            || it.value()
                == QStringLiteral("screenshot:") + context.functionId) {
            continue;
        }
        if (nativeHotkeysEqual(shortcut, it.key())) {
            return true;
        }
    }
    return false;
}

bool hasCycle(const ActiveGraph &graph)
{
    QMap<QString, int> indegree;
    for (auto it = graph.nodes.constBegin();
         it != graph.nodes.constEnd();
         ++it) {
        indegree.insert(it.key(), 0);
    }
    for (const FunctionFlowEdge *edge : graph.edges) {
        indegree[edge->toNodeId] += 1;
    }

    QStringList ready;
    for (auto it = indegree.constBegin(); it != indegree.constEnd(); ++it) {
        if (it.value() == 0) {
            ready.append(it.key());
        }
    }
    std::sort(ready.begin(), ready.end());

    int visited = 0;
    while (!ready.isEmpty()) {
        const QString current = ready.takeFirst();
        ++visited;
        for (const FunctionFlowEdge *edge : graph.outgoing.value(current)) {
            const int value = --indegree[edge->toNodeId];
            if (value == 0) {
                ready.append(edge->toNodeId);
                std::sort(ready.begin(), ready.end());
            }
        }
    }
    return visited != graph.nodes.size();
}

void validatePortConnections(
    const ActiveGraph &graph,
    FunctionFlowValidationResult *result)
{
    for (const FunctionFlowNode *node : graph.nodes) {
        const QVector<FunctionFlowPortSpec> ports =
            functionFlowPortSpecs(node->type);
        for (const FunctionFlowPortSpec &port : ports) {
            int connectionCount = 0;
            const QVector<const FunctionFlowEdge *> candidates =
                port.direction == FunctionFlowPortDirection::Input
                ? graph.incoming.value(node->id)
                : graph.outgoing.value(node->id);
            for (const FunctionFlowEdge *edge : candidates) {
                const QString edgePort =
                    port.direction == FunctionFlowPortDirection::Input
                    ? edge->toPortId
                    : edge->fromPortId;
                if (edgePort == port.id) {
                    ++connectionCount;
                }
            }
            if (port.cardinality == FunctionFlowPortCardinality::One
                && connectionCount > 1) {
                addIssue(
                    result,
                    QStringLiteral("flow_port_cardinality"),
                    node->id
                );
            }
            if (port.connectionRequired && connectionCount == 0) {
                addIssue(
                    result,
                    QStringLiteral("flow_port_connection_missing"),
                    node->id
                );
            }
        }
    }
}

void validateReachability(
    const ActiveGraph &graph,
    FunctionFlowValidationResult *result)
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
        traverse(sources, graph.outgoing, true);
    const QSet<QString> toActions =
        traverse(actions, graph.incoming, false);
    for (const FunctionFlowNode *node : graph.nodes) {
        if (!fromSources.contains(node->id)
            || !toActions.contains(node->id)) {
            addIssue(
                result,
                QStringLiteral("flow_enabled_node_unreachable"),
                node->id
            );
        }
    }
}

bool outputConnectsType(
    const ActiveGraph &graph,
    const QString &outputId,
    FunctionFlowNodeType type,
    const QString &writeMode = QString())
{
    for (const FunctionFlowEdge *edge : graph.outgoing.value(outputId)) {
        const FunctionFlowNode *target =
            graph.nodes.value(edge->toNodeId, nullptr);
        if (target && target->type == type) {
            if (type != FunctionFlowNodeType::AutoWrite
                || writeMode.isEmpty()
                || target->config.autoWrite.writeMode == writeMode) {
                return true;
            }
        }
    }
    return false;
}

void validateTriggerProfiles(
    const ActiveGraph &graph,
    const FunctionFlowValidationContext &context,
    const QString &outputId,
    FunctionFlowValidationResult *result)
{
    const FunctionFlowTrigger triggers[] = {
        FunctionFlowTrigger::MainHotkey,
        FunctionFlowTrigger::ScreenshotHotkey,
        FunctionFlowTrigger::ScreenshotLauncher
    };

    QString separateShortcut;
    for (const FunctionFlowNode *node : graph.nodes) {
        if (node->type == FunctionFlowNodeType::ScreenshotSource) {
            const QString mode = node->config.screenshot.triggerMode;
            if (mode == QStringLiteral("separate")
                || mode == QStringLiteral("separateAndLauncher")) {
                separateShortcut =
                    node->config.screenshot.separateShortcut;
            }
        }
    }

    for (FunctionFlowTrigger trigger : triggers) {
        const QVector<const FunctionFlowNode *> sources =
            activeSourcesForTrigger(graph, trigger);
        if (sources.isEmpty()) {
            continue;
        }
        const QStringList starts = sourceIds(sources);
        const QSet<QString> reachable =
            traverse(starts, graph.outgoing, true);

        for (const FunctionFlowNode *node : graph.nodes) {
            if (node->type == FunctionFlowNodeType::Input
                && node->config.input.required
                && !reachable.contains(node->id)) {
                addIssue(
                    result,
                    QStringLiteral("flow_enabled_node_unreachable"),
                    node->id
                );
            }
        }
        if (!outputId.isEmpty() && !reachable.contains(outputId)) {
            addIssue(
                result,
                QStringLiteral("flow_enabled_node_unreachable"),
                outputId
            );
        }

        bool screenshotReachesOutput = false;
        bool selectionReachesOutput = false;
        for (const FunctionFlowNode *source : sources) {
            const QSet<QString> sourceReachable = traverse(
                QStringList() << source->id,
                graph.outgoing,
                true
            );
            if (sourceReachable.contains(outputId)) {
                screenshotReachesOutput =
                    screenshotReachesOutput
                    || source->type
                        == FunctionFlowNodeType::ScreenshotSource;
                selectionReachesOutput =
                    selectionReachesOutput
                    || source->type
                        == FunctionFlowNodeType::SelectionSource;
            }
        }
        if (outputConnectsType(
                graph,
                outputId,
                FunctionFlowNodeType::ScreenshotPanel)
            && !screenshotReachesOutput) {
            addIssue(
                result,
                QStringLiteral("flow_screenshot_context_missing")
            );
        }
        if (outputConnectsType(
                graph,
                outputId,
                FunctionFlowNodeType::AutoWrite,
                QStringLiteral("replace"))
            && !selectionReachesOutput) {
            addIssue(
                result,
                QStringLiteral("flow_replace_selection_context_missing")
            );
        }
    }

    const QVector<const FunctionFlowNode *> mainSources =
        activeSourcesForTrigger(
            graph,
            FunctionFlowTrigger::MainHotkey
        );
    if (!mainSources.isEmpty()) {
        NativeHotkey hotkey;
        if (!parseNativeHotkey(context.mainShortcut, &hotkey)) {
            addIssue(
                result,
                QStringLiteral("flow_trigger_shortcut_missing")
            );
        } else if (conflictsWithOccupiedShortcut(
                       context.mainShortcut,
                       context)) {
            addIssue(
                result,
                QStringLiteral("flow_trigger_shortcut_conflict")
            );
        }
    }

    const QVector<const FunctionFlowNode *> screenshotHotkeySources =
        activeSourcesForTrigger(
            graph,
            FunctionFlowTrigger::ScreenshotHotkey
        );
    if (!screenshotHotkeySources.isEmpty()) {
        NativeHotkey hotkey;
        if (!parseNativeHotkey(separateShortcut, &hotkey)) {
            addIssue(
                result,
                QStringLiteral("flow_trigger_shortcut_missing")
            );
        } else if (conflictsWithOccupiedShortcut(
                       separateShortcut,
                       context)) {
            addIssue(
                result,
                QStringLiteral("flow_trigger_shortcut_conflict")
            );
        }
    }
    if (!mainSources.isEmpty()
        && !screenshotHotkeySources.isEmpty()
        && nativeHotkeysEqual(
            context.mainShortcut,
            separateShortcut)) {
        addIssue(
            result,
            QStringLiteral("flow_trigger_shortcut_conflict")
        );
    }

    QVector<const FunctionFlowNode *> holdVoices;
    bool hasPrimaryScreenshot = false;
    for (const FunctionFlowNode *source : mainSources) {
        if (source->type == FunctionFlowNodeType::VoiceSource
            && source->config.voice.recording.triggerMode
                == QStringLiteral("hold")) {
            holdVoices.append(source);
        }
        if (source->type == FunctionFlowNodeType::ScreenshotSource
            && source->config.screenshot.triggerMode
                == QStringLiteral("primary")) {
            hasPrimaryScreenshot = true;
        }
    }
    if (holdVoices.size() > 1
        || (!holdVoices.isEmpty()
            && (mainSources.first()->id != holdVoices.first()->id
                || hasPrimaryScreenshot))) {
        addIssue(
            result,
            QStringLiteral("flow_node_config_invalid"),
            holdVoices.isEmpty() ? QString() : holdVoices.first()->id
        );
    }
}

void validateStreamingTopology(
    const ActiveGraph &graph,
    const QString &outputId,
    FunctionFlowValidationResult *result)
{
    QVector<const FunctionFlowNode *> models;
    for (const FunctionFlowNode *node : graph.nodes) {
        if (node->type == FunctionFlowNodeType::Model) {
            models.append(node);
        }
    }
    for (const FunctionFlowNode *model : models) {
        if (!model->config.model.stream) {
            continue;
        }
        const QVector<const FunctionFlowEdge *> modelEdges =
            graph.outgoing.value(model->id);
        const QVector<const FunctionFlowEdge *> outputEdges =
            graph.outgoing.value(outputId);
        bool valid = models.size() == 1
            && modelEdges.size() == 1
            && modelEdges.first()->toNodeId == outputId
            && outputEdges.size() == 1;
        if (valid) {
            const FunctionFlowNode *action =
                graph.nodes.value(
                    outputEdges.first()->toNodeId,
                    nullptr
                );
            valid = action
                && action->type
                    == FunctionFlowNodeType::ResultPopup;
        }
        if (!valid) {
            addIssue(
                result,
                QStringLiteral("flow_stream_topology_unsupported"),
                model->id
            );
        }
    }
}

} // namespace

FunctionFlowValidationResult FunctionFlowValidator::validateForPublish(
    const FunctionFlowGraph &input,
    const FunctionFlowValidationContext &context)
{
    FunctionFlowValidationResult result;
    const FunctionFlowGraph graph =
        normalizeFunctionFlowGraph(input);

    if (graph.nodes.isEmpty()) {
        addIssue(&result, QStringLiteral("flow_empty"));
    }
    if (graph.nodes.size() > 128 || graph.edges.size() > 256) {
        addIssue(&result, QStringLiteral("flow_size_limit"));
    }

    QSet<QString> nodeIds;
    bool duplicateNodeIds = false;
    for (const FunctionFlowNode &node : graph.nodes) {
        if (node.id.isEmpty()) {
            addIssue(
                &result,
                QStringLiteral("flow_node_config_invalid"),
                node.id
            );
        }
        if (nodeIds.contains(node.id)) {
            duplicateNodeIds = true;
            addIssue(
                &result,
                QStringLiteral("flow_duplicate_node_id"),
                node.id
            );
        }
        nodeIds.insert(node.id);
    }

    QSet<QString> edgeIds;
    QSet<QString> connections;
    bool duplicateEdgeIds = false;
    for (const FunctionFlowEdge &edge : graph.edges) {
        if (edge.id.isEmpty()) {
            addIssue(
                &result,
                QStringLiteral("flow_node_config_invalid"),
                QString(),
                edge.id
            );
        }
        if (edgeIds.contains(edge.id)) {
            duplicateEdgeIds = true;
            addIssue(
                &result,
                QStringLiteral("flow_duplicate_edge_id"),
                QString(),
                edge.id
            );
        }
        edgeIds.insert(edge.id);
        const QString connectionKey =
            edge.fromNodeId + QChar(0x1f)
            + edge.fromPortId + QChar(0x1f)
            + edge.toNodeId + QChar(0x1f)
            + edge.toPortId;
        if (connections.contains(connectionKey)) {
            addIssue(
                &result,
                QStringLiteral("flow_duplicate_connection"),
                QString(),
                edge.id
            );
        }
        connections.insert(connectionKey);
    }

    if (graph.nodes.isEmpty()
        || duplicateNodeIds
        || duplicateEdgeIds) {
        result.ok = false;
        return result;
    }

    NodeMap allNodes;
    ActiveGraph active;
    for (const FunctionFlowNode &node : graph.nodes) {
        allNodes.insert(node.id, &node);
        if (node.enabled) {
            active.nodes.insert(node.id, &node);
        }
    }

    for (const FunctionFlowEdge &edge : graph.edges) {
        const FunctionFlowNode *from =
            allNodes.value(edge.fromNodeId, nullptr);
        const FunctionFlowNode *to =
            allNodes.value(edge.toNodeId, nullptr);
        if (!from || !to) {
            addIssue(
                &result,
                QStringLiteral("flow_dangling_edge"),
                QString(),
                edge.id
            );
            continue;
        }
        if (edge.order < 0 || edge.order > 10000) {
            addIssue(
                &result,
                QStringLiteral("flow_edge_order_invalid"),
                QString(),
                edge.id
            );
        }
        if (!from->enabled || !to->enabled) {
            continue;
        }
        if (edge.fromNodeId == edge.toNodeId) {
            addIssue(
                &result,
                QStringLiteral("flow_self_edge"),
                edge.fromNodeId,
                edge.id
            );
        }

        const bool fromPortKnown =
            containsPort(from->type, edge.fromPortId);
        const bool toPortKnown =
            containsPort(to->type, edge.toPortId);
        if (!fromPortKnown || !toPortKnown) {
            addIssue(
                &result,
                QStringLiteral("flow_unknown_port"),
                QString(),
                edge.id
            );
            continue;
        }
        if (!hasFunctionFlowPort(
                from->type,
                edge.fromPortId,
                FunctionFlowPortDirection::Output)
            || !hasFunctionFlowPort(
                to->type,
                edge.toPortId,
                FunctionFlowPortDirection::Input)) {
            addIssue(
                &result,
                QStringLiteral("flow_port_direction"),
                QString(),
                edge.id
            );
            continue;
        }
        if (!isFunctionFlowConnectionAllowed(
                from->type,
                edge.fromPortId,
                to->type,
                edge.toPortId)) {
            addIssue(
                &result,
                QStringLiteral("flow_edge_type_unsupported"),
                QString(),
                edge.id
            );
            continue;
        }
        active.edges.append(&edge);
        active.outgoing[edge.fromNodeId].append(&edge);
        active.incoming[edge.toNodeId].append(&edge);
    }

    if (hasCycle(active)) {
        addIssue(&result, QStringLiteral("flow_cycle"));
    }
    validatePortConnections(active, &result);

    int outputCount = 0;
    int autoWriteCount = 0;
    int voiceCount = 0;
    int selectionCount = 0;
    int screenshotCount = 0;
    QString outputId;
    for (const FunctionFlowNode *node : active.nodes) {
        validateNodeConfig(*node, context, &result);
        switch (node->type) {
        case FunctionFlowNodeType::VoiceSource:
            ++voiceCount;
            break;
        case FunctionFlowNodeType::SelectionSource:
            ++selectionCount;
            break;
        case FunctionFlowNodeType::ScreenshotSource:
            ++screenshotCount;
            break;
        case FunctionFlowNodeType::Output:
            ++outputCount;
            outputId = node->id;
            break;
        case FunctionFlowNodeType::AutoWrite:
            ++autoWriteCount;
            break;
        default:
            break;
        }
    }
    if (outputCount != 1) {
        addIssue(&result, QStringLiteral("flow_output_count"));
        outputId.clear();
    } else {
        bool hasResultAction = false;
        for (const FunctionFlowEdge *edge :
             active.outgoing.value(outputId)) {
            const FunctionFlowNode *target =
                active.nodes.value(edge->toNodeId, nullptr);
            if (target && isAction(target->type)) {
                hasResultAction = true;
                break;
            }
        }
        if (!hasResultAction) {
            addIssue(&result, QStringLiteral("flow_output_count"));
        }
    }
    if (voiceCount > 1
        || selectionCount > 1
        || screenshotCount > 1
        || autoWriteCount > 1) {
        addIssue(
            &result,
            QStringLiteral("flow_node_config_invalid")
        );
    }

    validateReachability(active, &result);
    if (!outputId.isEmpty()) {
        validateTriggerProfiles(
            active,
            context,
            outputId,
            &result
        );
        validateStreamingTopology(active, outputId, &result);
    }

    result.ok = result.issues.isEmpty();
    return result;
}
