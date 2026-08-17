#include <QtTest>

#include "../../src/domain/function_flow_graph.h"
#include "../../src/domain/function_flow_ports.h"
#include "../../src/domain/function_flow_runtime_types.h"

#include <QJsonObject>
#include <QLocale>

#include <algorithm>
#include <climits>
#include <limits>

namespace {

FunctionFlowNode makeNode(
    const QString &id,
    FunctionFlowNodeType type)
{
    FunctionFlowNode node;
    node.id = id;
    node.type = type;
    return node;
}

FunctionFlowEdge makeEdge(
    const QString &id,
    const QString &fromNodeId,
    const QString &fromPortId,
    const QString &toNodeId,
    const QString &toPortId,
    int order = 0)
{
    FunctionFlowEdge edge;
    edge.id = id;
    edge.fromNodeId = fromNodeId;
    edge.fromPortId = fromPortId;
    edge.toNodeId = toNodeId;
    edge.toPortId = toPortId;
    edge.order = order;
    return edge;
}

FunctionFlowGraph minimalHashGraph()
{
    FunctionFlowGraph graph;
    graph.nodes
        << makeNode(QStringLiteral("input"), FunctionFlowNodeType::Input)
        << makeNode(QStringLiteral("output"), FunctionFlowNodeType::Output);
    graph.edges << makeEdge(
        QStringLiteral("edge"),
        QStringLiteral("input"),
        QStringLiteral("text_out"),
        QStringLiteral("output"),
        QStringLiteral("text_in")
    );
    return graph;
}

bool expectedConnection(
    FunctionFlowNodeType from,
    FunctionFlowNodeType to)
{
    const bool sourceToInput =
        (from == FunctionFlowNodeType::VoiceSource
         || from == FunctionFlowNodeType::SelectionSource
         || from == FunctionFlowNodeType::ScreenshotSource)
        && to == FunctionFlowNodeType::Input;
    const bool inputToProcessing =
        from == FunctionFlowNodeType::Input
        && (to == FunctionFlowNodeType::Model
            || to == FunctionFlowNodeType::Output);
    const bool modelToProcessing =
        from == FunctionFlowNodeType::Model
        && (to == FunctionFlowNodeType::Input
            || to == FunctionFlowNodeType::Output);
    const bool outputToAction =
        from == FunctionFlowNodeType::Output
        && (to == FunctionFlowNodeType::ResultPopup
            || to == FunctionFlowNodeType::ScreenshotPanel
            || to == FunctionFlowNodeType::AutoWrite);
    return sourceToInput
        || inputToProcessing
        || modelToProcessing
        || outputToAction;
}

QString outputPortId(FunctionFlowNodeType type)
{
    return type == FunctionFlowNodeType::Output
        ? QStringLiteral("action_out")
        : QStringLiteral("text_out");
}

QString inputPortId(FunctionFlowNodeType type)
{
    return type == FunctionFlowNodeType::ResultPopup
            || type == FunctionFlowNodeType::ScreenshotPanel
            || type == FunctionFlowNodeType::AutoWrite
        ? QStringLiteral("action_in")
        : QStringLiteral("text_in");
}

} // namespace

class FunctionFlowGraphTests : public QObject
{
    Q_OBJECT

private slots:
    void usesSafeDefaults();
    void nodeTypeIdsRoundTripAndRejectUnknownIds();
    void portsComeFromNodeTypeRegistry();
    void connectionMatrixAcceptsOnlyDocumentedPairs();
    void modelChainingRequiresAnInputNode();
    void normalizationNeverDeletesDuplicateIdsOrDanglingEdges();
    void normalizationTrimsStringsAndRepairsOnlyEditorGeometry();
    void normalizationPreservesInvalidRuntimeNumbers();
    void graphHashIsStableAcrossInputOrder();
    void graphHashIsFullLowercaseSha256();
    void graphHashMatchesGoldenAcrossLocaleAndObjectOrder();
    void graphHashExcludesEditorOnlyFields();
    void graphHashIncludesRuntimeSemantics();
    void runtimePayloadsRemainOutsideTheGraph();
    void popupActionsUseTheFlowSpecificRegistry();
};

void FunctionFlowGraphTests::usesSafeDefaults()
{
    const FunctionFlowGraph graph;
    QCOMPARE(graph.schemaVersion, 1);
    QVERIFY(graph.nodes.isEmpty());
    QVERIFY(graph.edges.isEmpty());

    const FunctionFlowEditorState editor;
    QCOMPARE(editor.viewportCenter, QPointF());
    QCOMPARE(editor.zoom, qreal(1.0));

    const FunctionFlowRecordingConfig recording;
    QCOMPARE(recording.triggerMode, QStringLiteral("toggle"));
    QCOMPARE(recording.longRecordingEnabled, false);
    QCOMPARE(recording.segmentSeconds, 55);
    QCOMPARE(recording.maximumMinutes, 30);
    QCOMPARE(recording.countdownSeconds, 0);

    const FunctionFlowScreenshotSourceConfig screenshot;
    QCOMPARE(screenshot.ocrEngineId, QStringLiteral("automatic"));
    QCOMPARE(screenshot.timeoutMs, 45000);

    const FunctionFlowInputConfig input;
    QCOMPARE(input.role, QStringLiteral("source"));
    QCOMPARE(input.sequence, 0);
    QCOMPARE(input.required, true);

    const FunctionFlowResultPopupConfig popup;
    QCOMPARE(popup.displaySeconds, 0);
    QCOMPARE(popup.opacity, -1);

    const FunctionFlowState state;
    QCOMPARE(state.enabled, false);
    QCOMPARE(state.draft.revision, 0);
    QCOMPARE(state.published.revision, 0);
    QCOMPARE(state.draft.supported, true);
    QVERIFY(state.draft.graphHash.isEmpty());
}

void FunctionFlowGraphTests::nodeTypeIdsRoundTripAndRejectUnknownIds()
{
    const QVector<FunctionFlowNodeType> types = {
        FunctionFlowNodeType::VoiceSource,
        FunctionFlowNodeType::SelectionSource,
        FunctionFlowNodeType::ScreenshotSource,
        FunctionFlowNodeType::Input,
        FunctionFlowNodeType::Model,
        FunctionFlowNodeType::Output,
        FunctionFlowNodeType::ResultPopup,
        FunctionFlowNodeType::ScreenshotPanel,
        FunctionFlowNodeType::AutoWrite
    };

    for (FunctionFlowNodeType type : types) {
        bool ok = false;
        const QString id = functionFlowNodeTypeId(type);
        QVERIFY2(!id.isEmpty(), qPrintable(id));
        QVERIFY(functionFlowNodeTypeFromId(id, &ok) == type);
        QVERIFY(ok);
    }

    bool ok = true;
    const FunctionFlowNodeType fallback = functionFlowNodeTypeFromId(
        QStringLiteral("futureNode"),
        &ok
    );
    QVERIFY(!ok);
    QVERIFY(fallback == FunctionFlowNodeType::Input);
}

void FunctionFlowGraphTests::portsComeFromNodeTypeRegistry()
{
    const QVector<FunctionFlowPortSpec> modelPorts =
        functionFlowPortSpecs(FunctionFlowNodeType::Model);
    QCOMPARE(modelPorts.size(), 2);
    QVERIFY(hasFunctionFlowPort(
        FunctionFlowNodeType::Model,
        QStringLiteral("text_in"),
        FunctionFlowPortDirection::Input
    ));
    QVERIFY(hasFunctionFlowPort(
        FunctionFlowNodeType::Model,
        QStringLiteral("text_out"),
        FunctionFlowPortDirection::Output
    ));
    QVERIFY(!hasFunctionFlowPort(
        FunctionFlowNodeType::Model,
        QStringLiteral("custom"),
        FunctionFlowPortDirection::Input
    ));

    const QVector<FunctionFlowPortSpec> inputPorts =
        functionFlowPortSpecs(FunctionFlowNodeType::Input);
    QVERIFY(inputPorts.at(0).connectionRequired);
    QVERIFY(
        inputPorts.at(0).cardinality
        == FunctionFlowPortCardinality::Many
    );
    QVERIFY(
        inputPorts.at(1).cardinality
        == FunctionFlowPortCardinality::Many
    );

    const QVector<FunctionFlowPortSpec> outputPorts =
        functionFlowPortSpecs(FunctionFlowNodeType::Output);
    QCOMPARE(outputPorts.at(1).id, QStringLiteral("action_out"));
    QVERIFY(outputPorts.at(1).connectionRequired);
}

void FunctionFlowGraphTests::connectionMatrixAcceptsOnlyDocumentedPairs()
{
    const QVector<FunctionFlowNodeType> types = {
        FunctionFlowNodeType::VoiceSource,
        FunctionFlowNodeType::SelectionSource,
        FunctionFlowNodeType::ScreenshotSource,
        FunctionFlowNodeType::Input,
        FunctionFlowNodeType::Model,
        FunctionFlowNodeType::Output,
        FunctionFlowNodeType::ResultPopup,
        FunctionFlowNodeType::ScreenshotPanel,
        FunctionFlowNodeType::AutoWrite
    };

    for (FunctionFlowNodeType from : types) {
        for (FunctionFlowNodeType to : types) {
            QCOMPARE(
                isFunctionFlowConnectionAllowed(
                    from,
                    outputPortId(from),
                    to,
                    inputPortId(to)
                ),
                expectedConnection(from, to)
            );
        }
    }

    QVERIFY(!isFunctionFlowConnectionAllowed(
        FunctionFlowNodeType::Input,
        QStringLiteral("text_in"),
        FunctionFlowNodeType::Model,
        QStringLiteral("text_in")
    ));
    QVERIFY(!isFunctionFlowConnectionAllowed(
        FunctionFlowNodeType::Input,
        QStringLiteral("text_out"),
        FunctionFlowNodeType::Model,
        QStringLiteral("text_out")
    ));
}

void FunctionFlowGraphTests::modelChainingRequiresAnInputNode()
{
    QVERIFY(!isFunctionFlowConnectionAllowed(
        FunctionFlowNodeType::Model,
        QStringLiteral("text_out"),
        FunctionFlowNodeType::Model,
        QStringLiteral("text_in")
    ));
    QVERIFY(isFunctionFlowConnectionAllowed(
        FunctionFlowNodeType::Model,
        QStringLiteral("text_out"),
        FunctionFlowNodeType::Input,
        QStringLiteral("text_in")
    ));
    QVERIFY(isFunctionFlowConnectionAllowed(
        FunctionFlowNodeType::Input,
        QStringLiteral("text_out"),
        FunctionFlowNodeType::Model,
        QStringLiteral("text_in")
    ));
}

void FunctionFlowGraphTests::
normalizationNeverDeletesDuplicateIdsOrDanglingEdges()
{
    FunctionFlowGraph graph;
    graph.nodes
        << makeNode(QStringLiteral(" same "), FunctionFlowNodeType::Input)
        << makeNode(QStringLiteral("same"), FunctionFlowNodeType::Output);
    graph.edges << makeEdge(
        QStringLiteral(" dangling "),
        QStringLiteral("missing"),
        QStringLiteral("text_out"),
        QStringLiteral("same"),
        QStringLiteral("text_in")
    );

    const FunctionFlowGraph normalized =
        normalizeFunctionFlowGraph(graph);

    QCOMPARE(normalized.nodes.size(), 2);
    QCOMPARE(normalized.edges.size(), 1);
    QCOMPARE(normalized.nodes.at(0).id, QStringLiteral("same"));
    QCOMPARE(normalized.nodes.at(1).id, QStringLiteral("same"));
    QCOMPARE(normalized.edges.first().id, QStringLiteral("dangling"));
    QCOMPARE(
        normalized.edges.first().fromNodeId,
        QStringLiteral("missing")
    );
}

void FunctionFlowGraphTests::
normalizationTrimsStringsAndRepairsOnlyEditorGeometry()
{
    FunctionFlowGraph graph;
    FunctionFlowNode node =
        makeNode(QStringLiteral(" input "), FunctionFlowNodeType::Input);
    node.title = QStringLiteral(" 标题 ");
    node.position = QPointF(
        std::numeric_limits<qreal>::quiet_NaN(),
        std::numeric_limits<qreal>::infinity()
    );
    node.config.input.role = QStringLiteral(" source ");
    graph.nodes << node;
    graph.edges << makeEdge(
        QStringLiteral(" edge "),
        QStringLiteral(" input "),
        QStringLiteral(" text_out "),
        QStringLiteral(" output "),
        QStringLiteral(" text_in ")
    );

    const FunctionFlowGraph normalized =
        normalizeFunctionFlowGraph(graph);
    QCOMPARE(normalized.nodes.first().id, QStringLiteral("input"));
    QCOMPARE(normalized.nodes.first().title, QString::fromUtf8("标题"));
    QCOMPARE(normalized.nodes.first().position, QPointF());
    QCOMPARE(
        normalized.nodes.first().config.input.role,
        QStringLiteral("source")
    );
    QCOMPARE(normalized.edges.first().id, QStringLiteral("edge"));
    QCOMPARE(
        normalized.edges.first().fromPortId,
        QStringLiteral("text_out")
    );

    FunctionFlowEditorState editor;
    editor.viewportCenter = QPointF(
        std::numeric_limits<qreal>::infinity(),
        std::numeric_limits<qreal>::quiet_NaN()
    );
    editor.zoom = std::numeric_limits<qreal>::infinity();
    editor = normalizeFunctionFlowEditorState(editor);
    QCOMPARE(editor.viewportCenter, QPointF());
    QCOMPARE(editor.zoom, qreal(1.0));

    editor.zoom = 0.1;
    QCOMPARE(
        normalizeFunctionFlowEditorState(editor).zoom,
        qreal(0.35)
    );
    editor.zoom = 4.0;
    QCOMPARE(
        normalizeFunctionFlowEditorState(editor).zoom,
        qreal(3.0)
    );
}

void FunctionFlowGraphTests::normalizationPreservesInvalidRuntimeNumbers()
{
    FunctionFlowGraph graph;

    FunctionFlowNode voice =
        makeNode(QStringLiteral("voice"), FunctionFlowNodeType::VoiceSource);
    voice.config.voice.recording.segmentSeconds = 1;
    voice.config.voice.recording.maximumMinutes = 99;
    voice.config.voice.recording.countdownSeconds = -1;
    voice.config.voice.acquisitionSequence = -3;

    FunctionFlowNode screenshot = makeNode(
        QStringLiteral("screenshot"),
        FunctionFlowNodeType::ScreenshotSource
    );
    screenshot.config.screenshot.timeoutMs = 999;
    screenshot.config.screenshot.acquisitionSequence = 10001;

    FunctionFlowNode input =
        makeNode(QStringLiteral("input"), FunctionFlowNodeType::Input);
    input.config.input.sequence = INT_MAX;

    FunctionFlowNode popup = makeNode(
        QStringLiteral("popup"),
        FunctionFlowNodeType::ResultPopup
    );
    popup.config.popup.displaySeconds = -8;
    popup.config.popup.opacity = 101;

    FunctionFlowNode panel = makeNode(
        QStringLiteral("panel"),
        FunctionFlowNodeType::ScreenshotPanel
    );
    panel.config.screenshotPanel.displaySeconds = 601;
    panel.config.screenshotPanel.opacity = -2;

    graph.nodes << voice << screenshot << input << popup << panel;
    graph.edges << makeEdge(
        QStringLiteral("edge"),
        QStringLiteral("input"),
        QStringLiteral("text_out"),
        QStringLiteral("missing"),
        QStringLiteral("text_in"),
        INT_MAX
    );

    const FunctionFlowGraph normalized =
        normalizeFunctionFlowGraph(graph);

    QMap<QString, FunctionFlowNode> byId;
    for (const FunctionFlowNode &normalizedNode : normalized.nodes) {
        byId.insert(normalizedNode.id, normalizedNode);
    }
    QCOMPARE(byId.value(QStringLiteral("voice"))
                 .config.voice.recording.segmentSeconds, 1);
    QCOMPARE(byId.value(QStringLiteral("voice"))
                 .config.voice.recording.maximumMinutes, 99);
    QCOMPARE(byId.value(QStringLiteral("voice"))
                 .config.voice.recording.countdownSeconds, -1);
    QCOMPARE(byId.value(QStringLiteral("voice"))
                 .config.voice.acquisitionSequence, -3);
    QCOMPARE(byId.value(QStringLiteral("screenshot"))
                 .config.screenshot.timeoutMs, 999);
    QCOMPARE(byId.value(QStringLiteral("screenshot"))
                 .config.screenshot.acquisitionSequence, 10001);
    QCOMPARE(byId.value(QStringLiteral("input"))
                 .config.input.sequence, INT_MAX);
    QCOMPARE(byId.value(QStringLiteral("popup"))
                 .config.popup.displaySeconds, -8);
    QCOMPARE(byId.value(QStringLiteral("popup"))
                 .config.popup.opacity, 101);
    QCOMPARE(byId.value(QStringLiteral("panel"))
                 .config.screenshotPanel.displaySeconds, 601);
    QCOMPARE(byId.value(QStringLiteral("panel"))
                 .config.screenshotPanel.opacity, -2);
    QCOMPARE(normalized.edges.first().order, INT_MAX);
}

void FunctionFlowGraphTests::graphHashIsStableAcrossInputOrder()
{
    FunctionFlowGraph first = minimalHashGraph();
    FunctionFlowGraph second = first;
    std::reverse(second.nodes.begin(), second.nodes.end());
    std::reverse(second.edges.begin(), second.edges.end());

    QCOMPARE(
        functionFlowGraphHash(first),
        functionFlowGraphHash(second)
    );
}

void FunctionFlowGraphTests::graphHashIsFullLowercaseSha256()
{
    const QString hash = functionFlowGraphHash(minimalHashGraph());
    QCOMPARE(hash.size(), 64);
    QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
        .match(hash)
        .hasMatch());
}

void FunctionFlowGraphTests::
graphHashMatchesGoldenAcrossLocaleAndObjectOrder()
{
    FunctionFlowGraph first = minimalHashGraph();
    first.retainedValues.insert(QStringLiteral("z"), 1);
    first.retainedValues.insert(QStringLiteral("a"), 2);
    first.nodes[0].retainedValues.insert(QStringLiteral("z"), 1);
    first.nodes[0].retainedValues.insert(QStringLiteral("a"), 2);

    FunctionFlowGraph second = minimalHashGraph();
    second.retainedValues.insert(QStringLiteral("a"), 2);
    second.retainedValues.insert(QStringLiteral("z"), 1);
    second.nodes[0].retainedValues.insert(QStringLiteral("a"), 2);
    second.nodes[0].retainedValues.insert(QStringLiteral("z"), 1);

    const QLocale previousLocale;
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    const QString firstHash = functionFlowGraphHash(first);
    QLocale::setDefault(QLocale(QLocale::Turkish, QLocale::Turkey));
    const QString secondHash = functionFlowGraphHash(second);
    QLocale::setDefault(previousLocale);

    const QString golden = QStringLiteral(
        "2c2973c8d8dba30b05279b0b7527986"
        "ab2cffff98ba0318849ff46066a53a17e"
    );
    QCOMPARE(firstHash, golden);
    QCOMPARE(secondHash, golden);
}

void FunctionFlowGraphTests::graphHashExcludesEditorOnlyFields()
{
    FunctionFlowGraph first = minimalHashGraph();
    FunctionFlowGraph second = first;
    second.nodes[0].title = QString::fromUtf8("另一个标题");
    second.nodes[0].position = QPointF(250.0, -500.0);
    second.nodes[0].retainedValues.insert(QStringLiteral("future"), 1);
    second.edges[0].retainedValues.insert(QStringLiteral("future"), 2);
    second.retainedValues.insert(QStringLiteral("future"), 3);

    FunctionFlowState state;
    state.editor.viewportCenter = QPointF(99.0, 100.0);
    state.editor.zoom = 2.5;
    state.draft.graph = first;

    QCOMPARE(
        functionFlowGraphHash(first),
        functionFlowGraphHash(second)
    );
    QCOMPARE(
        functionFlowGraphHash(first),
        functionFlowGraphHash(state.draft.graph)
    );
}

void FunctionFlowGraphTests::graphHashIncludesRuntimeSemantics()
{
    const FunctionFlowGraph base = minimalHashGraph();

    FunctionFlowGraph configChanged = base;
    configChanged.nodes[0].config.input.role = QStringLiteral("instruction");
    QVERIFY(
        functionFlowGraphHash(base)
        != functionFlowGraphHash(configChanged)
    );

    FunctionFlowGraph enabledChanged = base;
    enabledChanged.nodes[0].enabled = false;
    QVERIFY(
        functionFlowGraphHash(base)
        != functionFlowGraphHash(enabledChanged)
    );

    FunctionFlowGraph edgeOrderChanged = base;
    edgeOrderChanged.edges[0].order = 1;
    QVERIFY(
        functionFlowGraphHash(base)
        != functionFlowGraphHash(edgeOrderChanged)
    );

    FunctionFlowGraph nodeTypeChanged = base;
    nodeTypeChanged.nodes[0].type =
        FunctionFlowNodeType::Model;
    QVERIFY(
        functionFlowGraphHash(base)
        != functionFlowGraphHash(nodeTypeChanged)
    );
}

void FunctionFlowGraphTests::runtimePayloadsRemainOutsideTheGraph()
{
    FunctionFlowVoicePayload voice;
    voice.sourceAudioPath = QStringLiteral("C:/managed/audio.wav");
    RecordingSegment segment;
    segment.index = 2;
    segment.wavPath = QStringLiteral("C:/managed/segment.wav");
    segment.recognitionElapsedMs = 321;
    voice.segments << segment;
    voice.speechElapsedMs = 654;
    voice.recordingTriggerMode = QStringLiteral("hold");
    voice.longRecording = true;

    QCOMPARE(voice.sourceAudioPath, QStringLiteral("C:/managed/audio.wav"));
    QCOMPARE(voice.segments.size(), 1);
    QCOMPARE(voice.segments.first().index, 2);
    QCOMPARE(voice.speechElapsedMs, qint64(654));
    QCOMPARE(voice.recordingTriggerMode, QStringLiteral("hold"));
    QCOMPARE(voice.longRecording, true);

    FunctionFlowScreenshotPayload screenshot;
    screenshot.image = QImage(2, 2, QImage::Format_ARGB32);
    OcrTextBlock block;
    block.text = QStringLiteral("recognized");
    screenshot.blocks << block;
    screenshot.recognizedText = block.text;
    screenshot.engine = OcrEngine::WindowsOcr;
    screenshot.elapsedMs = 77;
    screenshot.usedFallback = true;
    screenshot.rect = QRect(1, 2, 3, 4);

    FunctionFlowValue value;
    value.voice = QSharedPointer<const FunctionFlowVoicePayload>(
        new FunctionFlowVoicePayload(voice)
    );
    value.screenshot = QSharedPointer<const FunctionFlowScreenshotPayload>(
        new FunctionFlowScreenshotPayload(screenshot)
    );
    QCOMPARE(value.voice->segments.size(), 1);
    QCOMPARE(value.screenshot->recognizedText, QStringLiteral("recognized"));
    QCOMPARE(value.screenshot->rect, QRect(1, 2, 3, 4));

    const FunctionFlowGraph graph = minimalHashGraph();
    const QString before = functionFlowGraphHash(graph);
    value.voice.clear();
    value.screenshot.clear();
    QCOMPARE(functionFlowGraphHash(graph), before);
}

void FunctionFlowGraphTests::popupActionsUseTheFlowSpecificRegistry()
{
    const QStringList expected = QStringList()
        << QStringLiteral("expand")
        << QStringLiteral("vocabulary")
        << QStringLiteral("copy")
        << QStringLiteral("write")
        << QStringLiteral("replace");
    QCOMPARE(supportedFunctionFlowPopupActionIds(), expected);
    QCOMPARE(defaultFunctionFlowPopupActionIds(), expected);

    for (const QString &id : expected) {
        QVERIFY(isFunctionFlowPopupActionSupported(id));
    }
    QVERIFY(!isFunctionFlowPopupActionSupported(
        QStringLiteral("regenerate")
    ));
    QVERIFY(!isFunctionFlowPopupActionSupported(
        QStringLiteral("retryModel")
    ));
    QVERIFY(!isFunctionFlowPopupActionSupported(
        QStringLiteral("followUp")
    ));
}

QTEST_MAIN(FunctionFlowGraphTests)

#include "function_flow_graph_tests.moc"
