#ifndef VOCEKIT_FUNCTION_FLOW_GRAPH_H
#define VOCEKIT_FUNCTION_FLOW_GRAPH_H

#include "function_flow_ports.h"
#include "model_sampling_settings.h"

#include <QJsonObject>
#include <QPointF>
#include <QStringList>
#include <QVector>

QStringList supportedFunctionFlowPopupActionIds();
QStringList defaultFunctionFlowPopupActionIds();
bool isFunctionFlowPopupActionSupported(const QString &id);

struct FunctionFlowRecordingConfig
{
    QString triggerMode = QStringLiteral("toggle");
    bool longRecordingEnabled = false;
    int segmentSeconds = 55;
    int maximumMinutes = 30;
    int countdownSeconds = 0;
    bool beepEnabled = false;
    QString beepPath;
};

struct FunctionFlowVoiceSourceConfig
{
    QString speechProviderId;
    FunctionFlowRecordingConfig recording;
    int acquisitionSequence = 0;
    QString networkPolicy = QStringLiteral("inherit");
};

struct FunctionFlowSelectionSourceConfig
{
    bool inheritStrongSelection = true;
    int acquisitionSequence = 0;
};

struct FunctionFlowScreenshotSourceConfig
{
    QString ocrEngineId = QStringLiteral("automatic");
    int timeoutMs = 45000;
    QString triggerMode = QStringLiteral("primary");
    QString separateShortcut;
    int acquisitionSequence = 0;
    QString networkPolicy = QStringLiteral("inherit");
};

struct FunctionFlowInputConfig
{
    QString role = QStringLiteral("source");
    int sequence = 0;
    bool required = true;
};

struct FunctionFlowModelConfig
{
    QString modelId;
    QString promptId;
    ModelSamplingSettings sampling;
    bool stream = false;
    QString networkPolicy = QStringLiteral("inherit");
};

struct FunctionFlowOutputConfig
{
    QString emptyResultPolicy = QStringLiteral("fail");
};

struct FunctionFlowResultPopupConfig
{
    QString resultTemplate = QStringLiteral("simple");
    QStringList resultActions = defaultFunctionFlowPopupActionIds();
    int displaySeconds = 0;
    int opacity = -1;
};

struct FunctionFlowScreenshotPanelConfig
{
    int displaySeconds = 0;
    int opacity = -1;
};

struct FunctionFlowAutoWriteConfig
{
    QString writeMode = QStringLiteral("insert");
    bool fallbackToPopup = true;
};

struct FunctionFlowNodeConfig
{
    FunctionFlowVoiceSourceConfig voice;
    FunctionFlowSelectionSourceConfig selection;
    FunctionFlowScreenshotSourceConfig screenshot;
    FunctionFlowInputConfig input;
    FunctionFlowModelConfig model;
    FunctionFlowOutputConfig output;
    FunctionFlowResultPopupConfig popup;
    FunctionFlowScreenshotPanelConfig screenshotPanel;
    FunctionFlowAutoWriteConfig autoWrite;
};

struct FunctionFlowNode
{
    QString id;
    FunctionFlowNodeType type = FunctionFlowNodeType::Input;
    QString title;
    QPointF position;
    bool enabled = true;
    FunctionFlowNodeConfig config;
    QJsonObject retainedValues;
};

struct FunctionFlowEdge
{
    QString id;
    QString fromNodeId;
    QString fromPortId;
    QString toNodeId;
    QString toPortId;
    int order = 0;
    QJsonObject retainedValues;
};

struct FunctionFlowEndpoint
{
    QString nodeId;
    QString portId;
};

struct FunctionFlowGraph
{
    int schemaVersion = 1;
    QVector<FunctionFlowNode> nodes;
    QVector<FunctionFlowEdge> edges;
    QJsonObject retainedValues;
};

struct FunctionFlowEditorState
{
    QPointF viewportCenter;
    qreal zoom = 1.0;
};

struct VersionedFunctionFlowGraph
{
    int revision = 0;
    int sourceDraftRevision = 0;
    QString graphHash;
    bool supported = true;
    QString unavailableCode;
    FunctionFlowGraph graph;
    QJsonObject retainedRaw;
};

struct FunctionFlowState
{
    bool enabled = false;
    VersionedFunctionFlowGraph draft;
    VersionedFunctionFlowGraph published;
    FunctionFlowEditorState editor;
    QJsonObject retainedValues;
};

FunctionFlowGraph normalizeFunctionFlowGraph(
    const FunctionFlowGraph &graph
);
FunctionFlowEditorState normalizeFunctionFlowEditorState(
    const FunctionFlowEditorState &editor
);
QString functionFlowGraphHash(const FunctionFlowGraph &graph);
QString newFunctionFlowObjectId();

#endif // VOCEKIT_FUNCTION_FLOW_GRAPH_H
