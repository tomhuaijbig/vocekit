#include "function_flow_graph.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <algorithm>

namespace {

QString trimmed(const QString &value)
{
    return value.trimmed();
}

QStringList trimmed(const QStringList &values)
{
    QStringList result;
    result.reserve(values.size());
    for (const QString &value : values) {
        result.append(value.trimmed());
    }
    return result;
}

qreal safeCoordinate(qreal value)
{
    return qIsFinite(value) ? value : qreal(0.0);
}

void normalizeNodeConfig(FunctionFlowNode *node)
{
    switch (node->type) {
    case FunctionFlowNodeType::VoiceSource:
        node->config.voice.speechProviderId =
            trimmed(node->config.voice.speechProviderId);
        node->config.voice.recording.triggerMode =
            trimmed(node->config.voice.recording.triggerMode);
        node->config.voice.recording.beepPath =
            trimmed(node->config.voice.recording.beepPath);
        node->config.voice.networkPolicy =
            trimmed(node->config.voice.networkPolicy);
        break;
    case FunctionFlowNodeType::SelectionSource:
        break;
    case FunctionFlowNodeType::ScreenshotSource:
        node->config.screenshot.ocrEngineId =
            trimmed(node->config.screenshot.ocrEngineId);
        node->config.screenshot.triggerMode =
            trimmed(node->config.screenshot.triggerMode);
        node->config.screenshot.separateShortcut =
            trimmed(node->config.screenshot.separateShortcut);
        node->config.screenshot.networkPolicy =
            trimmed(node->config.screenshot.networkPolicy);
        break;
    case FunctionFlowNodeType::Input:
        node->config.input.role = trimmed(node->config.input.role);
        break;
    case FunctionFlowNodeType::Model:
        node->config.model.modelId =
            trimmed(node->config.model.modelId);
        node->config.model.promptId =
            trimmed(node->config.model.promptId);
        node->config.model.networkPolicy =
            trimmed(node->config.model.networkPolicy);
        break;
    case FunctionFlowNodeType::Output:
        node->config.output.emptyResultPolicy =
            trimmed(node->config.output.emptyResultPolicy);
        break;
    case FunctionFlowNodeType::ResultPopup:
        node->config.popup.resultTemplate =
            trimmed(node->config.popup.resultTemplate);
        node->config.popup.resultActions =
            trimmed(node->config.popup.resultActions);
        break;
    case FunctionFlowNodeType::ScreenshotPanel:
        break;
    case FunctionFlowNodeType::AutoWrite:
        node->config.autoWrite.writeMode =
            trimmed(node->config.autoWrite.writeMode);
        break;
    }
}

bool nodeLess(const FunctionFlowNode &left, const FunctionFlowNode &right)
{
    const int idOrder = QString::compare(
        left.id,
        right.id,
        Qt::CaseSensitive
    );
    if (idOrder != 0) {
        return idOrder < 0;
    }
    return functionFlowNodeTypeId(left.type)
        < functionFlowNodeTypeId(right.type);
}

bool edgeLess(const FunctionFlowEdge &left, const FunctionFlowEdge &right)
{
    const QString leftValues[] = {
        left.id,
        left.fromNodeId,
        left.fromPortId,
        left.toNodeId,
        left.toPortId
    };
    const QString rightValues[] = {
        right.id,
        right.fromNodeId,
        right.fromPortId,
        right.toNodeId,
        right.toPortId
    };
    for (int index = 0; index < 5; ++index) {
        const int order = QString::compare(
            leftValues[index],
            rightValues[index],
            Qt::CaseSensitive
        );
        if (order != 0) {
            return order < 0;
        }
    }
    return left.order < right.order;
}

QJsonObject recordingConfigJson(
    const FunctionFlowRecordingConfig &config)
{
    QJsonObject object;
    object.insert(QStringLiteral("triggerMode"), config.triggerMode);
    object.insert(
        QStringLiteral("longRecordingEnabled"),
        config.longRecordingEnabled
    );
    object.insert(QStringLiteral("segmentSeconds"), config.segmentSeconds);
    object.insert(QStringLiteral("maximumMinutes"), config.maximumMinutes);
    object.insert(
        QStringLiteral("countdownSeconds"),
        config.countdownSeconds
    );
    object.insert(QStringLiteral("beepEnabled"), config.beepEnabled);
    object.insert(QStringLiteral("beepPath"), config.beepPath);
    return object;
}

QJsonArray stringListJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values) {
        array.append(value);
    }
    return array;
}

QJsonObject nodeConfigJson(const FunctionFlowNode &node)
{
    QJsonObject object;
    switch (node.type) {
    case FunctionFlowNodeType::VoiceSource:
        object.insert(
            QStringLiteral("speechProviderId"),
            node.config.voice.speechProviderId
        );
        object.insert(
            QStringLiteral("recording"),
            recordingConfigJson(node.config.voice.recording)
        );
        object.insert(
            QStringLiteral("acquisitionSequence"),
            node.config.voice.acquisitionSequence
        );
        object.insert(
            QStringLiteral("networkPolicy"),
            node.config.voice.networkPolicy
        );
        break;
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

QString portDirectionId(FunctionFlowPortDirection direction)
{
    return direction == FunctionFlowPortDirection::Input
        ? QStringLiteral("input")
        : QStringLiteral("output");
}

QString portCardinalityId(FunctionFlowPortCardinality cardinality)
{
    return cardinality == FunctionFlowPortCardinality::One
        ? QStringLiteral("one")
        : QStringLiteral("many");
}

QJsonArray nodePortsJson(FunctionFlowNodeType type)
{
    QJsonArray array;
    const QVector<FunctionFlowPortSpec> ports =
        functionFlowPortSpecs(type);
    for (const FunctionFlowPortSpec &portSpec : ports) {
        QJsonObject object;
        FunctionFlowPortCardinality hashCardinality =
            portSpec.cardinality;
        if (type == FunctionFlowNodeType::Input
            && portSpec.id == QStringLiteral("text_in")) {
            // Widening this editor/validation policy does not change
            // the execution of graphs that were publishable under v1.
            // Keep their canonical hash stable; additional incoming
            // edges are already part of the semantic graph JSON.
            hashCardinality = FunctionFlowPortCardinality::One;
        }
        object.insert(QStringLiteral("id"), portSpec.id);
        object.insert(
            QStringLiteral("direction"),
            portDirectionId(portSpec.direction)
        );
        object.insert(
            QStringLiteral("cardinality"),
            portCardinalityId(hashCardinality)
        );
        object.insert(
            QStringLiteral("connectionRequired"),
            portSpec.connectionRequired
        );
        array.append(object);
    }
    return array;
}

QJsonObject semanticGraphJson(const FunctionFlowGraph &input)
{
    const FunctionFlowGraph graph = normalizeFunctionFlowGraph(input);
    QJsonArray nodes;
    for (const FunctionFlowNode &node : graph.nodes) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), node.id);
        object.insert(
            QStringLiteral("type"),
            functionFlowNodeTypeId(node.type)
        );
        object.insert(QStringLiteral("enabled"), node.enabled);
        object.insert(QStringLiteral("config"), nodeConfigJson(node));
        object.insert(QStringLiteral("ports"), nodePortsJson(node.type));
        nodes.append(object);
    }

    QJsonArray edges;
    for (const FunctionFlowEdge &edge : graph.edges) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), edge.id);
        object.insert(QStringLiteral("fromNodeId"), edge.fromNodeId);
        object.insert(QStringLiteral("fromPortId"), edge.fromPortId);
        object.insert(QStringLiteral("toNodeId"), edge.toNodeId);
        object.insert(QStringLiteral("toPortId"), edge.toPortId);
        object.insert(QStringLiteral("order"), edge.order);
        edges.append(object);
    }

    QJsonObject object;
    object.insert(QStringLiteral("schemaVersion"), graph.schemaVersion);
    object.insert(QStringLiteral("nodes"), nodes);
    object.insert(QStringLiteral("edges"), edges);
    return object;
}

QByteArray scalarJson(const QJsonValue &value)
{
    QJsonArray wrapper;
    wrapper.append(value);
    const QByteArray bytes =
        QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    return bytes.mid(1, bytes.size() - 2);
}

QByteArray canonicalJson(const QJsonValue &value)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        QStringList keys = object.keys();
        std::sort(
            keys.begin(),
            keys.end(),
            [](const QString &left, const QString &right) {
                return left.toUtf8() < right.toUtf8();
            }
        );

        QByteArray bytes("{");
        for (int index = 0; index < keys.size(); ++index) {
            if (index > 0) {
                bytes.append(',');
            }
            const QString &key = keys.at(index);
            bytes.append(scalarJson(QJsonValue(key)));
            bytes.append(':');
            bytes.append(canonicalJson(object.value(key)));
        }
        bytes.append('}');
        return bytes;
    }
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        QByteArray bytes("[");
        for (int index = 0; index < array.size(); ++index) {
            if (index > 0) {
                bytes.append(',');
            }
            bytes.append(canonicalJson(array.at(index)));
        }
        bytes.append(']');
        return bytes;
    }
    return scalarJson(value);
}

} // namespace

QStringList supportedFunctionFlowPopupActionIds()
{
    return QStringList()
        << QStringLiteral("expand")
        << QStringLiteral("vocabulary")
        << QStringLiteral("copy")
        << QStringLiteral("write")
        << QStringLiteral("replace");
}

QStringList defaultFunctionFlowPopupActionIds()
{
    return supportedFunctionFlowPopupActionIds();
}

bool isFunctionFlowPopupActionSupported(const QString &id)
{
    return supportedFunctionFlowPopupActionIds().contains(id.trimmed());
}

FunctionFlowGraph normalizeFunctionFlowGraph(
    const FunctionFlowGraph &input)
{
    FunctionFlowGraph graph = input;
    for (FunctionFlowNode &node : graph.nodes) {
        node.id = node.id.trimmed();
        node.title = node.title.trimmed();
        node.position.setX(safeCoordinate(node.position.x()));
        node.position.setY(safeCoordinate(node.position.y()));
        normalizeNodeConfig(&node);
    }
    for (FunctionFlowEdge &edge : graph.edges) {
        edge.id = edge.id.trimmed();
        edge.fromNodeId = edge.fromNodeId.trimmed();
        edge.fromPortId = edge.fromPortId.trimmed();
        edge.toNodeId = edge.toNodeId.trimmed();
        edge.toPortId = edge.toPortId.trimmed();
    }
    std::stable_sort(graph.nodes.begin(), graph.nodes.end(), nodeLess);
    std::stable_sort(graph.edges.begin(), graph.edges.end(), edgeLess);
    return graph;
}

FunctionFlowEditorState normalizeFunctionFlowEditorState(
    const FunctionFlowEditorState &input)
{
    FunctionFlowEditorState editor = input;
    editor.viewportCenter.setX(
        safeCoordinate(editor.viewportCenter.x())
    );
    editor.viewportCenter.setY(
        safeCoordinate(editor.viewportCenter.y())
    );
    if (!qIsFinite(editor.zoom)) {
        editor.zoom = 1.0;
    }
    editor.zoom = qBound(qreal(0.35), editor.zoom, qreal(3.0));
    return editor;
}

QString functionFlowGraphHash(const FunctionFlowGraph &graph)
{
    const QByteArray bytes = canonicalJson(semanticGraphJson(graph));
    return QString::fromLatin1(
        QCryptographicHash::hash(
            bytes,
            QCryptographicHash::Sha256
        ).toHex()
    );
}

QString newFunctionFlowObjectId()
{
    QString id = QUuid::createUuid().toString();
    id.remove(QLatin1Char('{'));
    id.remove(QLatin1Char('}'));
    return id;
}
