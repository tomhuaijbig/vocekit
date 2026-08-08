#include <QtTest>

#include "../../src/domain/function_flow_validation.h"

#include <climits>

namespace {

FunctionFlowNode node(
    const QString &id,
    FunctionFlowNodeType type)
{
    FunctionFlowNode value;
    value.id = id;
    value.type = type;
    return value;
}

FunctionFlowEdge edge(
    const QString &id,
    const QString &fromNodeId,
    const QString &fromPortId,
    const QString &toNodeId,
    const QString &toPortId,
    int order = 0)
{
    FunctionFlowEdge value;
    value.id = id;
    value.fromNodeId = fromNodeId;
    value.fromPortId = fromPortId;
    value.toNodeId = toNodeId;
    value.toPortId = toPortId;
    value.order = order;
    return value;
}

FunctionFlowGraph validGraph()
{
    FunctionFlowGraph graph;

    FunctionFlowNode voice =
        node(QStringLiteral("voice"), FunctionFlowNodeType::VoiceSource);
    voice.config.voice.speechProviderId = QStringLiteral("speech");

    FunctionFlowNode input =
        node(QStringLiteral("input"), FunctionFlowNodeType::Input);
    input.config.input.role = QStringLiteral("source");

    FunctionFlowNode model =
        node(QStringLiteral("model"), FunctionFlowNodeType::Model);
    model.config.model.modelId = QStringLiteral("model");
    model.config.model.promptId = QStringLiteral("prompt");

    graph.nodes
        << voice
        << input
        << model
        << node(QStringLiteral("output"), FunctionFlowNodeType::Output)
        << node(
            QStringLiteral("popup"),
            FunctionFlowNodeType::ResultPopup
        );
    graph.edges
        << edge(
            QStringLiteral("voice-input"),
            QStringLiteral("voice"),
            QStringLiteral("text_out"),
            QStringLiteral("input"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("input-model"),
            QStringLiteral("input"),
            QStringLiteral("text_out"),
            QStringLiteral("model"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("model-output"),
            QStringLiteral("model"),
            QStringLiteral("text_out"),
            QStringLiteral("output"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("output-popup"),
            QStringLiteral("output"),
            QStringLiteral("action_out"),
            QStringLiteral("popup"),
            QStringLiteral("action_in")
        );
    return graph;
}

FunctionFlowValidationContext validContext()
{
    FunctionFlowValidationContext context;
    context.functionId = QStringLiteral("custom");
    context.mainShortcut = QStringLiteral("Ctrl+Alt+V");
    context.references.modelIds << QStringLiteral("model");
    context.references.promptIds << QStringLiteral("prompt");
    context.references.speechProviderIds << QStringLiteral("speech");
    context.references.ocrEngineIds
        << QStringLiteral("automatic")
        << QStringLiteral("windows");
    context.references.defaultSpeechProviderId =
        QStringLiteral("speech");
    context.references.defaultOcrEngineId =
        QStringLiteral("automatic");
    return context;
}

FunctionFlowNode *findNode(
    FunctionFlowGraph *graph,
    const QString &id)
{
    for (FunctionFlowNode &candidate : graph->nodes) {
        if (candidate.id == id) {
            return &candidate;
        }
    }
    return nullptr;
}

bool hasCode(
    const FunctionFlowGraph &graph,
    const QString &code,
    const FunctionFlowValidationContext &context = validContext())
{
    return FunctionFlowValidator::validateForPublish(
        graph,
        context
    ).issueCodes.contains(code);
}

FunctionFlowGraph screenshotOnlyGraph(const QString &triggerMode)
{
    FunctionFlowGraph graph = validGraph();
    FunctionFlowNode *source = findNode(&graph, QStringLiteral("voice"));
    source->type = FunctionFlowNodeType::ScreenshotSource;
    source->config.screenshot.ocrEngineId = QStringLiteral("automatic");
    source->config.screenshot.triggerMode = triggerMode;
    source->config.screenshot.separateShortcut =
        QStringLiteral("Ctrl+Alt+S");
    return graph;
}

FunctionFlowGraph dualTriggerGraph()
{
    FunctionFlowGraph graph = validGraph();
    findNode(&graph, QStringLiteral("input"))
        ->config.input.required = false;

    FunctionFlowNode screenshot = node(
        QStringLiteral("screenshot"),
        FunctionFlowNodeType::ScreenshotSource
    );
    screenshot.config.screenshot.ocrEngineId =
        QStringLiteral("automatic");
    screenshot.config.screenshot.triggerMode =
        QStringLiteral("separate");
    screenshot.config.screenshot.separateShortcut =
        QStringLiteral("Ctrl+Alt+S");
    screenshot.config.screenshot.acquisitionSequence = 1;

    FunctionFlowNode screenshotInput =
        node(QStringLiteral("screenshot-input"), FunctionFlowNodeType::Input);
    screenshotInput.config.input.role = QStringLiteral("screenshot");
    screenshotInput.config.input.sequence = 1;
    screenshotInput.config.input.required = false;

    graph.nodes << screenshot << screenshotInput;
    graph.edges
        << edge(
            QStringLiteral("screenshot-input-edge"),
            QStringLiteral("screenshot"),
            QStringLiteral("text_out"),
            QStringLiteral("screenshot-input"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("screenshot-model-edge"),
            QStringLiteral("screenshot-input"),
            QStringLiteral("text_out"),
            QStringLiteral("model"),
            QStringLiteral("text_in")
        );
    return graph;
}

} // namespace

class FunctionFlowValidationTests : public QObject
{
    Q_OBJECT

private slots:
    void acceptsAValidGraphWithoutMutatingIt();
    void rejectsEmptyAndOversizedGraphs();
    void rejectsDuplicateIdsAndConnections();
    void rejectsDanglingSelfAndCyclicEdges();
    void distinguishesUnknownDirectionCardinalityAndMissingPorts();
    void rejectsUnsupportedEdgeTypesAndInvalidOrders();
    void requiresOneOutputAndReachableEnabledNodes();
    void validatesAllNodeConfigurationDomains();
    void rejectsEmptyObjectIds();
    void validatesExternalReferences();
    void validatesSourceRoleAndAutoWriteConstraints();
    void validatesPopupActionsAndStreamingTopology();
    void validatesTriggerShortcutsAndHoldRules();
    void validatesEachTriggerProfileIndependently();
    void validatesTriggerSpecificProvenance();
};

void FunctionFlowValidationTests::
acceptsAValidGraphWithoutMutatingIt()
{
    const FunctionFlowGraph graph = validGraph();
    const int nodeCount = graph.nodes.size();
    const int edgeCount = graph.edges.size();

    const FunctionFlowValidationResult result =
        FunctionFlowValidator::validateForPublish(
            graph,
            validContext()
        );

    QVERIFY(result.ok);
    QVERIFY(result.issueCodes.isEmpty());
    QVERIFY(result.issues.isEmpty());
    QCOMPARE(graph.nodes.size(), nodeCount);
    QCOMPARE(graph.edges.size(), edgeCount);
}

void FunctionFlowValidationTests::rejectsEmptyAndOversizedGraphs()
{
    QVERIFY(hasCode(FunctionFlowGraph(), QStringLiteral("flow_empty")));

    FunctionFlowGraph tooManyNodes = validGraph();
    while (tooManyNodes.nodes.size() <= 128) {
        FunctionFlowNode extra = node(
            QStringLiteral("disabled-%1").arg(tooManyNodes.nodes.size()),
            FunctionFlowNodeType::Input
        );
        extra.enabled = false;
        tooManyNodes.nodes << extra;
    }
    QVERIFY(hasCode(
        tooManyNodes,
        QStringLiteral("flow_size_limit")
    ));

    FunctionFlowGraph tooManyEdges = validGraph();
    while (tooManyEdges.edges.size() <= 256) {
        FunctionFlowEdge extra = edge(
            QStringLiteral("disabled-edge-%1")
                .arg(tooManyEdges.edges.size()),
            QStringLiteral("voice"),
            QStringLiteral("text_out"),
            QStringLiteral("input"),
            QStringLiteral("text_in")
        );
        tooManyEdges.edges << extra;
    }
    QVERIFY(hasCode(
        tooManyEdges,
        QStringLiteral("flow_size_limit")
    ));
}

void FunctionFlowValidationTests::rejectsDuplicateIdsAndConnections()
{
    FunctionFlowGraph duplicateNode = validGraph();
    duplicateNode.nodes << node(
        QStringLiteral("input"),
        FunctionFlowNodeType::Input
    );
    QVERIFY(hasCode(
        duplicateNode,
        QStringLiteral("flow_duplicate_node_id")
    ));
    QCOMPARE(duplicateNode.nodes.size(), 6);

    FunctionFlowGraph duplicateEdge = validGraph();
    duplicateEdge.edges << edge(
        QStringLiteral("voice-input"),
        QStringLiteral("voice"),
        QStringLiteral("text_out"),
        QStringLiteral("input"),
        QStringLiteral("text_in")
    );
    QVERIFY(hasCode(
        duplicateEdge,
        QStringLiteral("flow_duplicate_edge_id")
    ));

    FunctionFlowGraph duplicateConnection = validGraph();
    duplicateConnection.edges << edge(
        QStringLiteral("voice-input-copy"),
        QStringLiteral("voice"),
        QStringLiteral("text_out"),
        QStringLiteral("input"),
        QStringLiteral("text_in")
    );
    QVERIFY(hasCode(
        duplicateConnection,
        QStringLiteral("flow_duplicate_connection")
    ));
}

void FunctionFlowValidationTests::rejectsDanglingSelfAndCyclicEdges()
{
    FunctionFlowGraph dangling = validGraph();
    dangling.edges[0].fromNodeId = QStringLiteral("missing");
    QVERIFY(hasCode(
        dangling,
        QStringLiteral("flow_dangling_edge")
    ));

    FunctionFlowGraph self = validGraph();
    self.edges << edge(
        QStringLiteral("self"),
        QStringLiteral("input"),
        QStringLiteral("text_out"),
        QStringLiteral("input"),
        QStringLiteral("text_in")
    );
    QVERIFY(hasCode(self, QStringLiteral("flow_self_edge")));

    FunctionFlowGraph cycle = validGraph();
    cycle.edges << edge(
        QStringLiteral("cycle"),
        QStringLiteral("model"),
        QStringLiteral("text_out"),
        QStringLiteral("input"),
        QStringLiteral("text_in")
    );
    QVERIFY(hasCode(cycle, QStringLiteral("flow_cycle")));
}

void FunctionFlowValidationTests::
distinguishesUnknownDirectionCardinalityAndMissingPorts()
{
    FunctionFlowGraph unknown = validGraph();
    unknown.edges[0].fromPortId = QStringLiteral("future");
    QVERIFY(hasCode(unknown, QStringLiteral("flow_unknown_port")));

    FunctionFlowGraph wrongDirection = validGraph();
    wrongDirection.edges[1].fromPortId = QStringLiteral("text_in");
    QVERIFY(hasCode(
        wrongDirection,
        QStringLiteral("flow_port_direction")
    ));

    FunctionFlowGraph cardinality = validGraph();
    cardinality.edges << edge(
        QStringLiteral("input-output-copy"),
        QStringLiteral("input"),
        QStringLiteral("text_out"),
        QStringLiteral("output"),
        QStringLiteral("text_in")
    );
    QVERIFY(hasCode(
        cardinality,
        QStringLiteral("flow_port_cardinality")
    ));

    FunctionFlowGraph missing = validGraph();
    missing.edges.removeFirst();
    QVERIFY(hasCode(
        missing,
        QStringLiteral("flow_port_connection_missing")
    ));
}

void FunctionFlowValidationTests::
rejectsUnsupportedEdgeTypesAndInvalidOrders()
{
    FunctionFlowGraph unsupported = validGraph();
    unsupported.edges[0].toNodeId = QStringLiteral("model");
    QVERIFY(hasCode(
        unsupported,
        QStringLiteral("flow_edge_type_unsupported")
    ));

    FunctionFlowGraph negativeOrder = validGraph();
    negativeOrder.edges[0].order = -1;
    QVERIFY(hasCode(
        negativeOrder,
        QStringLiteral("flow_edge_order_invalid")
    ));

    FunctionFlowGraph largeOrder = validGraph();
    largeOrder.edges[0].order = 10001;
    QVERIFY(hasCode(
        largeOrder,
        QStringLiteral("flow_edge_order_invalid")
    ));

    FunctionFlowGraph overflowOrder = validGraph();
    overflowOrder.edges[0].order = INT_MAX;
    QVERIFY(hasCode(
        overflowOrder,
        QStringLiteral("flow_edge_order_invalid")
    ));
}

void FunctionFlowValidationTests::
requiresOneOutputAndReachableEnabledNodes()
{
    FunctionFlowGraph noOutput = validGraph();
    findNode(&noOutput, QStringLiteral("output"))->enabled = false;
    QVERIFY(hasCode(
        noOutput,
        QStringLiteral("flow_output_count")
    ));

    FunctionFlowGraph twoOutputs = validGraph();
    twoOutputs.nodes << node(
        QStringLiteral("output-2"),
        FunctionFlowNodeType::Output
    );
    QVERIFY(hasCode(
        twoOutputs,
        QStringLiteral("flow_output_count")
    ));

    FunctionFlowGraph noAction = validGraph();
    noAction.edges.removeLast();
    QVERIFY(hasCode(
        noAction,
        QStringLiteral("flow_output_count")
    ));

    FunctionFlowGraph unreachable = validGraph();
    FunctionFlowNode unused =
        node(QStringLiteral("unused"), FunctionFlowNodeType::Input);
    unused.config.input.required = false;
    unreachable.nodes << unused;
    unreachable.edges << edge(
        QStringLiteral("voice-unused"),
        QStringLiteral("voice"),
        QStringLiteral("text_out"),
        QStringLiteral("unused"),
        QStringLiteral("text_in")
    );
    QVERIFY(hasCode(
        unreachable,
        QStringLiteral("flow_enabled_node_unreachable")
    ));
}

void FunctionFlowValidationTests::validatesAllNodeConfigurationDomains()
{
    const QString configCode = QStringLiteral("flow_node_config_invalid");

    FunctionFlowGraph graph = validGraph();
    findNode(&graph, QStringLiteral("voice"))
        ->config.voice.recording.triggerMode = QStringLiteral("press");
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    findNode(&graph, QStringLiteral("voice"))
        ->config.voice.recording.segmentSeconds = 19;
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    findNode(&graph, QStringLiteral("voice"))
        ->config.voice.recording.maximumMinutes = 31;
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    findNode(&graph, QStringLiteral("voice"))
        ->config.voice.recording.countdownSeconds = 61;
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    findNode(&graph, QStringLiteral("voice"))
        ->config.voice.acquisitionSequence = -1;
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    findNode(&graph, QStringLiteral("voice"))
        ->config.voice.networkPolicy = QStringLiteral("unknown");
    QVERIFY(hasCode(graph, configCode));

    graph = screenshotOnlyGraph(QStringLiteral("primary"));
    findNode(&graph, QStringLiteral("voice"))
        ->config.screenshot.timeoutMs = 999;
    QVERIFY(hasCode(graph, configCode));

    graph = screenshotOnlyGraph(QStringLiteral("primary"));
    findNode(&graph, QStringLiteral("voice"))
        ->config.screenshot.triggerMode = QStringLiteral("unknown");
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    findNode(&graph, QStringLiteral("input"))
        ->config.input.sequence = 10001;
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    findNode(&graph, QStringLiteral("output"))
        ->config.output.emptyResultPolicy = QStringLiteral("empty");
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    findNode(&graph, QStringLiteral("popup"))
        ->config.popup.resultTemplate = QStringLiteral("future");
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    findNode(&graph, QStringLiteral("popup"))
        ->config.popup.displaySeconds = 601;
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    findNode(&graph, QStringLiteral("popup"))
        ->config.popup.opacity = 19;
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    FunctionFlowNode *popup = findNode(&graph, QStringLiteral("popup"));
    popup->type = FunctionFlowNodeType::ScreenshotPanel;
    popup->config.screenshotPanel.opacity = 101;
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    popup = findNode(&graph, QStringLiteral("popup"));
    popup->type = FunctionFlowNodeType::AutoWrite;
    popup->config.autoWrite.writeMode = QStringLiteral("append");
    QVERIFY(hasCode(graph, configCode));

    graph = validGraph();
    findNode(&graph, QStringLiteral("input"))->title =
        QString(81, QLatin1Char('x'));
    QVERIFY(hasCode(graph, configCode));
}

void FunctionFlowValidationTests::rejectsEmptyObjectIds()
{
    FunctionFlowGraph emptyNodeId = validGraph();
    findNode(&emptyNodeId, QStringLiteral("voice"))->id.clear();
    emptyNodeId.edges[0].fromNodeId.clear();
    QVERIFY(hasCode(
        emptyNodeId,
        QStringLiteral("flow_node_config_invalid")
    ));

    FunctionFlowGraph emptyEdgeId = validGraph();
    emptyEdgeId.edges[0].id.clear();
    QVERIFY(hasCode(
        emptyEdgeId,
        QStringLiteral("flow_node_config_invalid")
    ));
}

void FunctionFlowValidationTests::validatesExternalReferences()
{
    FunctionFlowGraph graph = validGraph();
    findNode(&graph, QStringLiteral("model"))
        ->config.model.modelId = QStringLiteral("missing");
    QVERIFY(hasCode(
        graph,
        QStringLiteral("flow_model_reference_missing")
    ));

    graph = validGraph();
    findNode(&graph, QStringLiteral("model"))
        ->config.model.promptId = QStringLiteral("missing");
    QVERIFY(hasCode(
        graph,
        QStringLiteral("flow_prompt_reference_missing")
    ));

    graph = validGraph();
    findNode(&graph, QStringLiteral("voice"))
        ->config.voice.speechProviderId = QStringLiteral("missing");
    QVERIFY(hasCode(
        graph,
        QStringLiteral("flow_speech_provider_reference_missing")
    ));

    graph = screenshotOnlyGraph(QStringLiteral("primary"));
    findNode(&graph, QStringLiteral("voice"))
        ->config.screenshot.ocrEngineId = QStringLiteral("missing");
    QVERIFY(hasCode(
        graph,
        QStringLiteral("flow_ocr_engine_reference_missing")
    ));
}

void FunctionFlowValidationTests::
validatesSourceRoleAndAutoWriteConstraints()
{
    FunctionFlowGraph duplicateSource = validGraph();
    FunctionFlowNode voice2 =
        node(QStringLiteral("voice-2"), FunctionFlowNodeType::VoiceSource);
    voice2.config.voice.speechProviderId = QStringLiteral("speech");
    FunctionFlowNode input2 =
        node(QStringLiteral("input-2"), FunctionFlowNodeType::Input);
    input2.config.input.required = false;
    duplicateSource.nodes << voice2 << input2;
    duplicateSource.edges
        << edge(
            QStringLiteral("voice-2-input"),
            QStringLiteral("voice-2"),
            QStringLiteral("text_out"),
            QStringLiteral("input-2"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("input-2-model"),
            QStringLiteral("input-2"),
            QStringLiteral("text_out"),
            QStringLiteral("model"),
            QStringLiteral("text_in")
        );
    QVERIFY(hasCode(
        duplicateSource,
        QStringLiteral("flow_node_config_invalid")
    ));

    FunctionFlowGraph role = validGraph();
    findNode(&role, QStringLiteral("input"))
        ->config.input.role = QStringLiteral("bad\nrole");
    QVERIFY(hasCode(
        role,
        QStringLiteral("flow_input_role_invalid")
    ));

    role = validGraph();
    findNode(&role, QStringLiteral("input"))
        ->config.input.role = QStringLiteral("[source]");
    QVERIFY(hasCode(
        role,
        QStringLiteral("flow_input_role_invalid")
    ));

    FunctionFlowGraph twoAutoWrites = validGraph();
    twoAutoWrites.nodes
        << node(QStringLiteral("write-1"), FunctionFlowNodeType::AutoWrite)
        << node(QStringLiteral("write-2"), FunctionFlowNodeType::AutoWrite);
    twoAutoWrites.edges
        << edge(
            QStringLiteral("output-write-1"),
            QStringLiteral("output"),
            QStringLiteral("action_out"),
            QStringLiteral("write-1"),
            QStringLiteral("action_in"),
            1
        )
        << edge(
            QStringLiteral("output-write-2"),
            QStringLiteral("output"),
            QStringLiteral("action_out"),
            QStringLiteral("write-2"),
            QStringLiteral("action_in"),
            2
        );
    QVERIFY(hasCode(
        twoAutoWrites,
        QStringLiteral("flow_node_config_invalid")
    ));
}

void FunctionFlowValidationTests::
validatesPopupActionsAndStreamingTopology()
{
    FunctionFlowGraph duplicate = validGraph();
    findNode(&duplicate, QStringLiteral("popup"))
        ->config.popup.resultActions =
            QStringList() << QStringLiteral("copy")
                          << QStringLiteral("copy");
    QVERIFY(hasCode(
        duplicate,
        QStringLiteral("flow_popup_action_duplicate")
    ));

    FunctionFlowGraph unsupported = validGraph();
    findNode(&unsupported, QStringLiteral("popup"))
        ->config.popup.resultActions =
            QStringList() << QStringLiteral("regenerate");
    QVERIFY(hasCode(
        unsupported,
        QStringLiteral("flow_popup_action_unsupported")
    ));

    FunctionFlowGraph invalidStream = validGraph();
    findNode(&invalidStream, QStringLiteral("model"))
        ->config.model.stream = true;
    invalidStream.nodes << node(
        QStringLiteral("write"),
        FunctionFlowNodeType::AutoWrite
    );
    invalidStream.edges << edge(
        QStringLiteral("output-write"),
        QStringLiteral("output"),
        QStringLiteral("action_out"),
        QStringLiteral("write"),
        QStringLiteral("action_in"),
        1
    );
    QVERIFY(hasCode(
        invalidStream,
        QStringLiteral("flow_stream_topology_unsupported")
    ));

    FunctionFlowGraph validStream = validGraph();
    findNode(&validStream, QStringLiteral("model"))
        ->config.model.stream = true;
    QVERIFY(FunctionFlowValidator::validateForPublish(
        validStream,
        validContext()
    ).ok);
}

void FunctionFlowValidationTests::
validatesTriggerShortcutsAndHoldRules()
{
    FunctionFlowValidationContext context = validContext();
    context.mainShortcut.clear();
    QVERIFY(hasCode(
        validGraph(),
        QStringLiteral("flow_trigger_shortcut_missing"),
        context
    ));

    context = validContext();
    context.occupiedShortcutOwners.insert(
        QStringLiteral("Alt+Ctrl+V"),
        QStringLiteral("other")
    );
    QVERIFY(hasCode(
        validGraph(),
        QStringLiteral("flow_trigger_shortcut_conflict"),
        context
    ));

    FunctionFlowGraph screenshot =
        screenshotOnlyGraph(QStringLiteral("separate"));
    findNode(&screenshot, QStringLiteral("voice"))
        ->config.screenshot.separateShortcut.clear();
    QVERIFY(hasCode(
        screenshot,
        QStringLiteral("flow_trigger_shortcut_missing")
    ));

    FunctionFlowGraph dual = dualTriggerGraph();
    findNode(&dual, QStringLiteral("screenshot"))
        ->config.screenshot.separateShortcut =
            QStringLiteral("Alt+Ctrl+V");
    QVERIFY(hasCode(
        dual,
        QStringLiteral("flow_trigger_shortcut_conflict")
    ));

    FunctionFlowGraph hold = validGraph();
    FunctionFlowNode *voice = findNode(&hold, QStringLiteral("voice"));
    voice->config.voice.recording.triggerMode = QStringLiteral("hold");
    voice->config.voice.acquisitionSequence = 2;
    FunctionFlowNode selection = node(
        QStringLiteral("selection"),
        FunctionFlowNodeType::SelectionSource
    );
    selection.config.selection.acquisitionSequence = 1;
    FunctionFlowNode selectionInput =
        node(QStringLiteral("selection-input"), FunctionFlowNodeType::Input);
    selectionInput.config.input.required = false;
    hold.nodes << selection << selectionInput;
    hold.edges
        << edge(
            QStringLiteral("selection-input-edge"),
            QStringLiteral("selection"),
            QStringLiteral("text_out"),
            QStringLiteral("selection-input"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("selection-model-edge"),
            QStringLiteral("selection-input"),
            QStringLiteral("text_out"),
            QStringLiteral("model"),
            QStringLiteral("text_in")
        );
    QVERIFY(hasCode(
        hold,
        QStringLiteral("flow_node_config_invalid")
    ));

    FunctionFlowGraph holdScreenshot = dualTriggerGraph();
    findNode(&holdScreenshot, QStringLiteral("voice"))
        ->config.voice.recording.triggerMode = QStringLiteral("hold");
    findNode(&holdScreenshot, QStringLiteral("screenshot"))
        ->config.screenshot.triggerMode = QStringLiteral("primary");
    QVERIFY(hasCode(
        holdScreenshot,
        QStringLiteral("flow_node_config_invalid")
    ));
}

void FunctionFlowValidationTests::
validatesEachTriggerProfileIndependently()
{
    FunctionFlowGraph graph = dualTriggerGraph();
    findNode(&graph, QStringLiteral("input"))
        ->config.input.required = true;
    findNode(&graph, QStringLiteral("screenshot-input"))
        ->config.input.required = true;

    QVERIFY(hasCode(
        graph,
        QStringLiteral("flow_enabled_node_unreachable")
    ));

    findNode(&graph, QStringLiteral("input"))
        ->config.input.required = false;
    findNode(&graph, QStringLiteral("screenshot-input"))
        ->config.input.required = false;
    QVERIFY(FunctionFlowValidator::validateForPublish(
        graph,
        validContext()
    ).ok);
}

void FunctionFlowValidationTests::
validatesTriggerSpecificProvenance()
{
    FunctionFlowGraph screenshotPanel = validGraph();
    findNode(&screenshotPanel, QStringLiteral("popup"))
        ->type = FunctionFlowNodeType::ScreenshotPanel;
    QVERIFY(hasCode(
        screenshotPanel,
        QStringLiteral("flow_screenshot_context_missing")
    ));

    screenshotPanel = dualTriggerGraph();
    findNode(&screenshotPanel, QStringLiteral("popup"))
        ->type = FunctionFlowNodeType::ScreenshotPanel;
    QVERIFY(hasCode(
        screenshotPanel,
        QStringLiteral("flow_screenshot_context_missing")
    ));

    FunctionFlowGraph replace = validGraph();
    FunctionFlowNode *action = findNode(&replace, QStringLiteral("popup"));
    action->type = FunctionFlowNodeType::AutoWrite;
    action->config.autoWrite.writeMode = QStringLiteral("replace");
    QVERIFY(hasCode(
        replace,
        QStringLiteral("flow_replace_selection_context_missing")
    ));
}

QTEST_MAIN(FunctionFlowValidationTests)

#include "function_flow_validation_tests.moc"
