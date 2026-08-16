#include "function_flow_json.h"

#include <QJsonArray>
#include <QRegExp>

namespace {

QJsonArray stringListJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values) {
        array.append(value);
    }
    return array;
}

QStringList stringList(const QJsonValue &value)
{
    QStringList result;
    for (const QJsonValue &item : value.toArray()) {
        result.append(item.toString());
    }
    return result;
}

QJsonObject pointJson(const QPointF &point)
{
    QJsonObject object;
    object.insert(QStringLiteral("x"), point.x());
    object.insert(QStringLiteral("y"), point.y());
    return object;
}

bool readPoint(const QJsonValue &value, QPointF *point)
{
    if (!point || !value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    if (!object.value(QStringLiteral("x")).isDouble()
        || !object.value(QStringLiteral("y")).isDouble()) {
        return false;
    }
    *point = QPointF(
        object.value(QStringLiteral("x")).toDouble(),
        object.value(QStringLiteral("y")).toDouble()
    );
    return true;
}

QJsonObject configJson(const FunctionFlowNode &node)
{
    QJsonObject object =
        node.retainedValues.value(QStringLiteral("config")).toObject();
    switch (node.type) {
    case FunctionFlowNodeType::VoiceSource: {
        QJsonObject recording =
            object.value(QStringLiteral("recording")).toObject();
        recording.insert(
            QStringLiteral("triggerMode"),
            node.config.voice.recording.triggerMode
        );
        recording.insert(
            QStringLiteral("longRecordingEnabled"),
            node.config.voice.recording.longRecordingEnabled
        );
        recording.insert(
            QStringLiteral("segmentSeconds"),
            node.config.voice.recording.segmentSeconds
        );
        recording.insert(
            QStringLiteral("maximumMinutes"),
            node.config.voice.recording.maximumMinutes
        );
        recording.insert(
            QStringLiteral("countdownSeconds"),
            node.config.voice.recording.countdownSeconds
        );
        recording.insert(
            QStringLiteral("beepEnabled"),
            node.config.voice.recording.beepEnabled
        );
        recording.insert(
            QStringLiteral("beepPath"),
            node.config.voice.recording.beepPath
        );
        object.insert(
            QStringLiteral("speechProviderId"),
            node.config.voice.speechProviderId
        );
        object.insert(QStringLiteral("recording"), recording);
        object.insert(
            QStringLiteral("acquisitionSequence"),
            node.config.voice.acquisitionSequence
        );
        object.insert(
            QStringLiteral("networkPolicy"),
            node.config.voice.networkPolicy
        );
        break;
    }
    case FunctionFlowNodeType::SelectionSource:
        object.insert(
            QStringLiteral("inheritStrongSelection"),
            node.config.selection.inheritStrongSelection
        );
        object.insert(
            QStringLiteral("acquisitionSequence"),
            node.config.selection.acquisitionSequence
        );
        break;
    case FunctionFlowNodeType::ScreenshotSource:
        object.insert(
            QStringLiteral("ocrEngineId"),
            node.config.screenshot.ocrEngineId
        );
        object.insert(
            QStringLiteral("timeoutMs"),
            node.config.screenshot.timeoutMs
        );
        object.insert(
            QStringLiteral("triggerMode"),
            node.config.screenshot.triggerMode
        );
        object.insert(
            QStringLiteral("separateShortcut"),
            node.config.screenshot.separateShortcut
        );
        object.insert(
            QStringLiteral("acquisitionSequence"),
            node.config.screenshot.acquisitionSequence
        );
        object.insert(
            QStringLiteral("networkPolicy"),
            node.config.screenshot.networkPolicy
        );
        break;
    case FunctionFlowNodeType::Input:
        object.insert(QStringLiteral("role"), node.config.input.role);
        object.insert(
            QStringLiteral("sequence"),
            node.config.input.sequence
        );
        object.insert(
            QStringLiteral("required"),
            node.config.input.required
        );
        break;
    case FunctionFlowNodeType::Model:
        object.insert(
            QStringLiteral("modelId"),
            node.config.model.modelId
        );
        object.insert(
            QStringLiteral("promptId"),
            node.config.model.promptId
        );
        object.insert(QStringLiteral("stream"), node.config.model.stream);
        {
            const ModelSamplingSettings sampling =
                normalizeModelSamplingSettings(
                    node.config.model.sampling
                );
            if (sampling.temperatureEnabled) {
                object.insert(
                    QStringLiteral("temperature"),
                    sampling.temperature
                );
            } else {
                object.remove(QStringLiteral("temperature"));
            }
            if (sampling.topPEnabled) {
                object.insert(QStringLiteral("topP"), sampling.topP);
            } else {
                object.remove(QStringLiteral("topP"));
            }
        }
        object.insert(
            QStringLiteral("networkPolicy"),
            node.config.model.networkPolicy
        );
        break;
    case FunctionFlowNodeType::Output:
        object.insert(
            QStringLiteral("emptyResultPolicy"),
            node.config.output.emptyResultPolicy
        );
        break;
    case FunctionFlowNodeType::ResultPopup:
        object.insert(
            QStringLiteral("resultTemplate"),
            node.config.popup.resultTemplate
        );
        object.insert(
            QStringLiteral("resultActions"),
            stringListJson(node.config.popup.resultActions)
        );
        object.insert(
            QStringLiteral("displaySeconds"),
            node.config.popup.displaySeconds
        );
        object.insert(
            QStringLiteral("opacity"),
            node.config.popup.opacity
        );
        break;
    case FunctionFlowNodeType::ScreenshotPanel:
        object.insert(
            QStringLiteral("displaySeconds"),
            node.config.screenshotPanel.displaySeconds
        );
        object.insert(
            QStringLiteral("opacity"),
            node.config.screenshotPanel.opacity
        );
        break;
    case FunctionFlowNodeType::AutoWrite:
        object.insert(
            QStringLiteral("writeMode"),
            node.config.autoWrite.writeMode
        );
        object.insert(
            QStringLiteral("fallbackToPopup"),
            node.config.autoWrite.fallbackToPopup
        );
        break;
    }
    return object;
}

void readConfig(
    const QJsonObject &object,
    FunctionFlowNode *node)
{
    switch (node->type) {
    case FunctionFlowNodeType::VoiceSource: {
        node->config.voice.speechProviderId =
            object.value(QStringLiteral("speechProviderId")).toString();
        node->config.voice.acquisitionSequence =
            object.value(QStringLiteral("acquisitionSequence"))
                .toInt(node->config.voice.acquisitionSequence);
        node->config.voice.networkPolicy =
            object.value(QStringLiteral("networkPolicy"))
                .toString(node->config.voice.networkPolicy);
        const QJsonObject recording =
            object.value(QStringLiteral("recording")).toObject();
        node->config.voice.recording.triggerMode =
            recording.value(QStringLiteral("triggerMode"))
                .toString(node->config.voice.recording.triggerMode);
        node->config.voice.recording.longRecordingEnabled =
            recording.value(QStringLiteral("longRecordingEnabled"))
                .toBool(
                    node->config.voice.recording.longRecordingEnabled
                );
        node->config.voice.recording.segmentSeconds =
            recording.value(QStringLiteral("segmentSeconds"))
                .toInt(node->config.voice.recording.segmentSeconds);
        node->config.voice.recording.maximumMinutes =
            recording.value(QStringLiteral("maximumMinutes"))
                .toInt(node->config.voice.recording.maximumMinutes);
        node->config.voice.recording.countdownSeconds =
            recording.value(QStringLiteral("countdownSeconds"))
                .toInt(node->config.voice.recording.countdownSeconds);
        node->config.voice.recording.beepEnabled =
            recording.value(QStringLiteral("beepEnabled"))
                .toBool(node->config.voice.recording.beepEnabled);
        node->config.voice.recording.beepPath =
            recording.value(QStringLiteral("beepPath")).toString();
        break;
    }
    case FunctionFlowNodeType::SelectionSource:
        node->config.selection.inheritStrongSelection =
            object.value(QStringLiteral("inheritStrongSelection"))
                .toBool(
                    node->config.selection.inheritStrongSelection
                );
        node->config.selection.acquisitionSequence =
            object.value(QStringLiteral("acquisitionSequence"))
                .toInt(node->config.selection.acquisitionSequence);
        break;
    case FunctionFlowNodeType::ScreenshotSource:
        node->config.screenshot.ocrEngineId =
            object.value(QStringLiteral("ocrEngineId"))
                .toString(node->config.screenshot.ocrEngineId);
        node->config.screenshot.timeoutMs =
            object.value(QStringLiteral("timeoutMs"))
                .toInt(node->config.screenshot.timeoutMs);
        node->config.screenshot.triggerMode =
            object.value(QStringLiteral("triggerMode"))
                .toString(node->config.screenshot.triggerMode);
        node->config.screenshot.separateShortcut =
            object.value(QStringLiteral("separateShortcut")).toString();
        node->config.screenshot.acquisitionSequence =
            object.value(QStringLiteral("acquisitionSequence"))
                .toInt(node->config.screenshot.acquisitionSequence);
        node->config.screenshot.networkPolicy =
            object.value(QStringLiteral("networkPolicy"))
                .toString(node->config.screenshot.networkPolicy);
        break;
    case FunctionFlowNodeType::Input:
        node->config.input.role =
            object.value(QStringLiteral("role"))
                .toString(node->config.input.role);
        node->config.input.sequence =
            object.value(QStringLiteral("sequence"))
                .toInt(node->config.input.sequence);
        node->config.input.required =
            object.value(QStringLiteral("required"))
                .toBool(node->config.input.required);
        break;
    case FunctionFlowNodeType::Model:
        node->config.model.modelId =
            object.value(QStringLiteral("modelId")).toString();
        node->config.model.promptId =
            object.value(QStringLiteral("promptId")).toString();
        if (object.value(QStringLiteral("temperature")).isDouble()
            && isValidModelTemperature(
                object.value(QStringLiteral("temperature")).toDouble()
            )) {
            node->config.model.sampling.temperatureEnabled = true;
            node->config.model.sampling.temperature =
                object.value(QStringLiteral("temperature")).toDouble();
        }
        if (object.value(QStringLiteral("topP")).isDouble()
            && isValidModelTopP(
                object.value(QStringLiteral("topP")).toDouble()
            )) {
            node->config.model.sampling.topPEnabled = true;
            node->config.model.sampling.topP =
                object.value(QStringLiteral("topP")).toDouble();
        }
        node->config.model.stream =
            object.value(QStringLiteral("stream"))
                .toBool(node->config.model.stream);
        node->config.model.networkPolicy =
            object.value(QStringLiteral("networkPolicy"))
                .toString(node->config.model.networkPolicy);
        break;
    case FunctionFlowNodeType::Output:
        node->config.output.emptyResultPolicy =
            object.value(QStringLiteral("emptyResultPolicy"))
                .toString(node->config.output.emptyResultPolicy);
        break;
    case FunctionFlowNodeType::ResultPopup:
        node->config.popup.resultTemplate =
            object.value(QStringLiteral("resultTemplate"))
                .toString(node->config.popup.resultTemplate);
        if (object.value(QStringLiteral("resultActions")).isArray()) {
            node->config.popup.resultActions =
                stringList(object.value(QStringLiteral("resultActions")));
        }
        node->config.popup.displaySeconds =
            object.value(QStringLiteral("displaySeconds"))
                .toInt(node->config.popup.displaySeconds);
        node->config.popup.opacity =
            object.value(QStringLiteral("opacity"))
                .toInt(node->config.popup.opacity);
        break;
    case FunctionFlowNodeType::ScreenshotPanel:
        node->config.screenshotPanel.displaySeconds =
            object.value(QStringLiteral("displaySeconds"))
                .toInt(node->config.screenshotPanel.displaySeconds);
        node->config.screenshotPanel.opacity =
            object.value(QStringLiteral("opacity"))
                .toInt(node->config.screenshotPanel.opacity);
        break;
    case FunctionFlowNodeType::AutoWrite:
        node->config.autoWrite.writeMode =
            object.value(QStringLiteral("writeMode"))
                .toString(node->config.autoWrite.writeMode);
        node->config.autoWrite.fallbackToPopup =
            object.value(QStringLiteral("fallbackToPopup"))
                .toBool(node->config.autoWrite.fallbackToPopup);
        break;
    }
}

QJsonObject graphJson(const FunctionFlowGraph &input)
{
    const FunctionFlowGraph graph =
        normalizeFunctionFlowGraph(input);
    QJsonObject object = graph.retainedValues;
    QJsonArray nodes;
    for (const FunctionFlowNode &node : graph.nodes) {
        QJsonObject nodeObject = node.retainedValues;
        nodeObject.insert(QStringLiteral("id"), node.id);
        nodeObject.insert(
            QStringLiteral("type"),
            functionFlowNodeTypeId(node.type)
        );
        nodeObject.insert(QStringLiteral("title"), node.title);
        nodeObject.insert(
            QStringLiteral("position"),
            pointJson(node.position)
        );
        nodeObject.insert(QStringLiteral("enabled"), node.enabled);
        nodeObject.insert(QStringLiteral("config"), configJson(node));
        nodes.append(nodeObject);
    }
    QJsonArray edges;
    for (const FunctionFlowEdge &edge : graph.edges) {
        QJsonObject edgeObject = edge.retainedValues;
        edgeObject.insert(QStringLiteral("id"), edge.id);
        edgeObject.insert(
            QStringLiteral("fromNodeId"),
            edge.fromNodeId
        );
        edgeObject.insert(
            QStringLiteral("fromPortId"),
            edge.fromPortId
        );
        edgeObject.insert(QStringLiteral("toNodeId"), edge.toNodeId);
        edgeObject.insert(QStringLiteral("toPortId"), edge.toPortId);
        edgeObject.insert(QStringLiteral("order"), edge.order);
        edges.append(edgeObject);
    }
    object.insert(QStringLiteral("schemaVersion"), graph.schemaVersion);
    object.insert(QStringLiteral("nodes"), nodes);
    object.insert(QStringLiteral("edges"), edges);
    return object;
}

bool readGraph(
    const QJsonValue &value,
    FunctionFlowGraph *graph,
    QString *unavailableCode)
{
    if (!graph || !value.isObject()) {
        if (unavailableCode) {
            *unavailableCode = QStringLiteral("flow_json_invalid");
        }
        return false;
    }
    const QJsonObject object = value.toObject();
    if (!object.value(QStringLiteral("schemaVersion")).isDouble()) {
        if (unavailableCode) {
            *unavailableCode = QStringLiteral("flow_json_invalid");
        }
        return false;
    }
    const int schemaVersion =
        object.value(QStringLiteral("schemaVersion")).toInt();
    if (schemaVersion > 1) {
        if (unavailableCode) {
            *unavailableCode = QStringLiteral("flow_schema_newer");
        }
        return false;
    }
    if (schemaVersion != 1
        || !object.value(QStringLiteral("nodes")).isArray()
        || !object.value(QStringLiteral("edges")).isArray()) {
        if (unavailableCode) {
            *unavailableCode = QStringLiteral("flow_json_invalid");
        }
        return false;
    }

    FunctionFlowGraph parsed;
    parsed.schemaVersion = schemaVersion;
    parsed.retainedValues = object;
    for (const QJsonValue &nodeValue :
         object.value(QStringLiteral("nodes")).toArray()) {
        if (!nodeValue.isObject()) {
            if (unavailableCode) {
                *unavailableCode = QStringLiteral("flow_json_invalid");
            }
            return false;
        }
        const QJsonObject nodeObject = nodeValue.toObject();
        bool ok = false;
        FunctionFlowNode node;
        node.retainedValues = nodeObject;
        node.id = nodeObject.value(QStringLiteral("id")).toString();
        node.type = functionFlowNodeTypeFromId(
            nodeObject.value(QStringLiteral("type")).toString(),
            &ok
        );
        if (!ok) {
            if (unavailableCode) {
                *unavailableCode =
                    QStringLiteral("flow_node_type_unsupported");
            }
            return false;
        }
        node.title =
            nodeObject.value(QStringLiteral("title")).toString();
        node.enabled =
            nodeObject.value(QStringLiteral("enabled")).toBool(true);
        if (nodeObject.contains(QStringLiteral("position"))
            && !readPoint(
                nodeObject.value(QStringLiteral("position")),
                &node.position)) {
            if (unavailableCode) {
                *unavailableCode = QStringLiteral("flow_json_invalid");
            }
            return false;
        }
        if (nodeObject.contains(QStringLiteral("config"))
            && !nodeObject.value(QStringLiteral("config")).isObject()) {
            if (unavailableCode) {
                *unavailableCode = QStringLiteral("flow_json_invalid");
            }
            return false;
        }
        readConfig(
            nodeObject.value(QStringLiteral("config")).toObject(),
            &node
        );
        parsed.nodes.append(node);
    }
    for (const QJsonValue &edgeValue :
         object.value(QStringLiteral("edges")).toArray()) {
        if (!edgeValue.isObject()) {
            if (unavailableCode) {
                *unavailableCode = QStringLiteral("flow_json_invalid");
            }
            return false;
        }
        const QJsonObject edgeObject = edgeValue.toObject();
        FunctionFlowEdge edge;
        edge.retainedValues = edgeObject;
        edge.id = edgeObject.value(QStringLiteral("id")).toString();
        edge.fromNodeId =
            edgeObject.value(QStringLiteral("fromNodeId")).toString();
        edge.fromPortId =
            edgeObject.value(QStringLiteral("fromPortId")).toString();
        edge.toNodeId =
            edgeObject.value(QStringLiteral("toNodeId")).toString();
        edge.toPortId =
            edgeObject.value(QStringLiteral("toPortId")).toString();
        edge.order =
            edgeObject.value(QStringLiteral("order")).toInt();
        parsed.edges.append(edge);
    }
    *graph = normalizeFunctionFlowGraph(parsed);
    return true;
}

bool validHash(const QString &hash)
{
    return QRegExp(QStringLiteral("^[0-9a-f]{64}$"))
        .exactMatch(hash);
}

VersionedFunctionFlowGraph readVersion(
    const QJsonValue &value,
    bool published,
    bool hashRequired)
{
    VersionedFunctionFlowGraph version;
    if (value.isUndefined()) {
        if (published && hashRequired) {
            version.supported = false;
            version.unavailableCode =
                QStringLiteral("flow_published_hash_mismatch");
        }
        return version;
    }
    if (!value.isObject()) {
        version.supported = false;
        version.unavailableCode = QStringLiteral("flow_json_invalid");
        return version;
    }
    const QJsonObject object = value.toObject();
    version.retainedRaw = object;
    if (!object.value(QStringLiteral("revision")).isDouble()
        || !object.value(QStringLiteral("sourceDraftRevision")).isDouble()) {
        version.supported = false;
        version.unavailableCode = QStringLiteral("flow_json_invalid");
        return version;
    }
    version.revision =
        object.value(QStringLiteral("revision")).toInt();
    version.sourceDraftRevision =
        object.value(QStringLiteral("sourceDraftRevision")).toInt();
    const QJsonValue hashValue =
        object.value(QStringLiteral("graphHash"));
    const bool hashHasStringType = hashValue.isString();
    const QString diskHash =
        hashHasStringType ? hashValue.toString() : QString();
    if (published
        && (version.revision > 0 || hashRequired)
        && !hashHasStringType) {
        version.supported = false;
        version.unavailableCode =
            QStringLiteral("flow_published_hash_mismatch");
        return version;
    }
    QString graphError;
    if (!readGraph(
            object.value(QStringLiteral("graph")),
            &version.graph,
            &graphError)) {
        version.supported = false;
        version.unavailableCode = graphError;
        return version;
    }
    const QString computedHash =
        functionFlowGraphHash(version.graph);
    if (!published) {
        version.graphHash = computedHash;
        return version;
    }
    if ((version.revision > 0 || hashRequired)
        && (!validHash(diskHash) || diskHash != computedHash)) {
        version.supported = false;
        version.unavailableCode =
            QStringLiteral("flow_published_hash_mismatch");
        return version;
    }
    if (!diskHash.isEmpty()
        && (!validHash(diskHash) || diskHash != computedHash)) {
        version.supported = false;
        version.unavailableCode =
            QStringLiteral("flow_published_hash_mismatch");
        return version;
    }
    version.graphHash = diskHash;
    return version;
}

QJsonObject versionJson(const VersionedFunctionFlowGraph &version)
{
    if (!version.supported) {
        return version.retainedRaw;
    }
    QJsonObject object = version.retainedRaw;
    object.insert(QStringLiteral("revision"), version.revision);
    object.insert(
        QStringLiteral("sourceDraftRevision"),
        version.sourceDraftRevision
    );
    object.insert(QStringLiteral("graphHash"), version.graphHash);
    object.insert(QStringLiteral("graph"), graphJson(version.graph));
    return object;
}

} // namespace

FunctionFlowState functionFlowStateFromJson(
    const QJsonObject &object,
    QStringList *warnings)
{
    FunctionFlowState state;
    state.retainedValues = object;
    state.enabled =
        object.value(QStringLiteral("enabled")).toBool(false);

    const QJsonValue editorValue =
        object.value(QStringLiteral("editor"));
    if (editorValue.isObject()) {
        const QJsonObject editorObject = editorValue.toObject();
        readPoint(
            editorObject.value(QStringLiteral("viewport")),
            &state.editor.viewportCenter
        );
        state.editor.zoom =
            editorObject.value(QStringLiteral("zoom"))
                .toDouble(state.editor.zoom);
        state.editor =
            normalizeFunctionFlowEditorState(state.editor);
    }

    state.draft = readVersion(
        object.value(QStringLiteral("draft")),
        false,
        false
    );
    state.published = readVersion(
        object.value(QStringLiteral("published")),
        true,
        state.enabled
    );
    if (warnings) {
        if (!state.draft.supported) {
            warnings->append(state.draft.unavailableCode);
        }
        if (!state.published.supported) {
            warnings->append(state.published.unavailableCode);
        }
    }
    return state;
}

QJsonObject functionFlowStateToJson(const FunctionFlowState &state)
{
    QJsonObject object = state.retainedValues;
    QJsonObject editor;
    editor.insert(
        QStringLiteral("viewport"),
        pointJson(state.editor.viewportCenter)
    );
    editor.insert(QStringLiteral("zoom"), state.editor.zoom);
    object.insert(QStringLiteral("enabled"), state.enabled);
    object.insert(QStringLiteral("editor"), editor);
    object.insert(QStringLiteral("draft"), versionJson(state.draft));
    object.insert(
        QStringLiteral("published"),
        versionJson(state.published)
    );
    return object;
}
