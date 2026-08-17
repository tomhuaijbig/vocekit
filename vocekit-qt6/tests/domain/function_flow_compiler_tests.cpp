#include <QtTest>

#include "../../src/domain/function_flow_compiler.h"

#include <algorithm>

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

FunctionFlowGraph compiledGraph()
{
    FunctionFlowGraph graph;

    FunctionFlowNode voice =
        node(QStringLiteral("voice"), FunctionFlowNodeType::VoiceSource);
    voice.config.voice.acquisitionSequence = 0;
    FunctionFlowNode instruction = node(
        QStringLiteral("input_instruction"),
        FunctionFlowNodeType::Input
    );
    instruction.config.input.role = QStringLiteral("instruction");
    instruction.config.input.sequence = 0;
    instruction.config.input.required = false;

    FunctionFlowNode selection = node(
        QStringLiteral("selection"),
        FunctionFlowNodeType::SelectionSource
    );
    selection.config.selection.acquisitionSequence = 1;
    FunctionFlowNode source = node(
        QStringLiteral("input_source"),
        FunctionFlowNodeType::Input
    );
    source.config.input.role = QStringLiteral("source");
    source.config.input.sequence = 1;
    source.config.input.required = false;

    FunctionFlowNode model =
        node(QStringLiteral("model"), FunctionFlowNodeType::Model);
    model.config.model.modelId = QStringLiteral("model");
    model.config.model.promptId = QStringLiteral("prompt");

    graph.nodes
        << voice
        << instruction
        << selection
        << source
        << model
        << node(QStringLiteral("output"), FunctionFlowNodeType::Output)
        << node(
            QStringLiteral("popup"),
            FunctionFlowNodeType::ResultPopup
        );
    graph.edges
        << edge(
            QStringLiteral("voice-instruction"),
            QStringLiteral("voice"),
            QStringLiteral("text_out"),
            QStringLiteral("input_instruction"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("instruction-model"),
            QStringLiteral("input_instruction"),
            QStringLiteral("text_out"),
            QStringLiteral("model"),
            QStringLiteral("text_in"),
            1
        )
        << edge(
            QStringLiteral("selection-source"),
            QStringLiteral("selection"),
            QStringLiteral("text_out"),
            QStringLiteral("input_source"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("source-model"),
            QStringLiteral("input_source"),
            QStringLiteral("text_out"),
            QStringLiteral("model"),
            QStringLiteral("text_in"),
            0
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
            QStringLiteral("action_in"),
            2
        );
    return graph;
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

QString fullHash()
{
    return QString(64, QLatin1Char('a'));
}

} // namespace

class FunctionFlowCompilerTests : public QObject
{
    Q_OBJECT

private slots:
    void compilesDeterministicallyAcrossInputOrder();
    void interleavesAcquisitionInputsBeforeTheNextSource();
    void preservesTypedSortedInputBindings();
    void compilesMultipleSourcesIntoOneInput();
    void compilesTriggerProfiles();
    void ordersTerminalActionsByEdgeOrderThenNodeId();
    void ordersRemainingNodesByTopologicalLayer();
    void prunesDisabledNodesAndEdges();
    void recordsTheStreamingPopupNode();
    void marksExplicitPopupAsCoveringAutoWriteFallback();
    void rejectsMalformedGraphsWithoutReturningAPartialPlan();
};

void FunctionFlowCompilerTests::
compilesDeterministicallyAcrossInputOrder()
{
    const FunctionFlowGraph firstGraph = compiledGraph();
    FunctionFlowGraph secondGraph = firstGraph;
    std::reverse(secondGraph.nodes.begin(), secondGraph.nodes.end());
    std::reverse(secondGraph.edges.begin(), secondGraph.edges.end());

    const FunctionFlowCompileResult first =
        FunctionFlowCompiler::compile(firstGraph, 4, fullHash());
    const FunctionFlowCompileResult second =
        FunctionFlowCompiler::compile(secondGraph, 4, fullHash());

    QVERIFY(first.ok);
    QVERIFY(second.ok);
    QCOMPARE(
        first.plan.topologicalNodeIds,
        second.plan.topologicalNodeIds
    );
    QCOMPARE(first.plan.publishedRevision, 4);
    QCOMPARE(first.plan.publishedHash, fullHash());
    QCOMPARE(first.plan.nodes.size(), firstGraph.nodes.size());
}

void FunctionFlowCompilerTests::
interleavesAcquisitionInputsBeforeTheNextSource()
{
    const FunctionFlowCompileResult result =
        FunctionFlowCompiler::compile(
            compiledGraph(),
            1,
            fullHash()
    );

    QVERIFY(result.ok);
    const QStringList firstFour =
        result.plan.topologicalNodeIds.mid(0, 4);
    QCOMPARE(
        firstFour,
        QStringList()
            << QStringLiteral("voice")
            << QStringLiteral("input_instruction")
            << QStringLiteral("selection")
            << QStringLiteral("input_source")
    );
}

void FunctionFlowCompilerTests::preservesTypedSortedInputBindings()
{
    FunctionFlowGraph graph = compiledGraph();
    findNode(&graph, QStringLiteral("input_source"))
        ->config.input.sequence = 0;

    const FunctionFlowCompileResult result =
        FunctionFlowCompiler::compile(graph, 1, fullHash());

    QVERIFY(result.ok);
    const FunctionFlowCompiledNode model =
        result.plan.nodes.value(QStringLiteral("model"));
    QCOMPARE(model.inputs.size(), 2);
    QCOMPARE(
        model.inputs.at(0).predecessorNodeId,
        QStringLiteral("input_instruction")
    );
    QCOMPARE(model.inputs.at(0).role, QStringLiteral("instruction"));
    QCOMPARE(model.inputs.at(0).sequence, 0);
    QCOMPARE(model.inputs.at(0).required, false);
    QCOMPARE(
        model.inputs.at(1).predecessorNodeId,
        QStringLiteral("input_source")
    );
    QCOMPARE(model.inputs.at(1).role, QStringLiteral("source"));
    QCOMPARE(model.inputs.at(1).sequence, 0);
    QCOMPARE(model.inputs.at(1).required, false);
}

void FunctionFlowCompilerTests::compilesMultipleSourcesIntoOneInput()
{
    FunctionFlowGraph graph = compiledGraph();
    for (int index = graph.edges.size() - 1; index >= 0; --index) {
        if (graph.edges.at(index).id
                == QStringLiteral("selection-source")
            || graph.edges.at(index).id
                == QStringLiteral("source-model")) {
            graph.edges.remove(index);
        }
    }
    for (int index = graph.nodes.size() - 1; index >= 0; --index) {
        if (graph.nodes.at(index).id
                == QStringLiteral("input_source")) {
            graph.nodes.remove(index);
        }
    }
    graph.edges << edge(
        QStringLiteral("selection-instruction"),
        QStringLiteral("selection"),
        QStringLiteral("text_out"),
        QStringLiteral("input_instruction"),
        QStringLiteral("text_in")
    );

    const FunctionFlowCompileResult result =
        FunctionFlowCompiler::compile(graph, 1, fullHash());

    QVERIFY(result.ok);
    const FunctionFlowCompiledNode input =
        result.plan.nodes.value(QStringLiteral("input_instruction"));
    QCOMPARE(input.inputs.size(), 2);
    QCOMPARE(
        input.inputs.at(0).predecessorNodeId,
        QStringLiteral("selection")
    );
    QCOMPARE(
        input.inputs.at(1).predecessorNodeId,
        QStringLiteral("voice")
    );
}

void FunctionFlowCompilerTests::compilesTriggerProfiles()
{
    const FunctionFlowCompileResult result =
        FunctionFlowCompiler::compile(
            compiledGraph(),
            1,
            fullHash()
        );

    QVERIFY(result.ok);
    const FunctionFlowTriggerPlan main =
        result.plan.triggers.value(FunctionFlowTrigger::MainHotkey);
    QVERIFY(main.available);
    QCOMPARE(
        main.activeSourceNodeIds,
        QStringList()
            << QStringLiteral("voice")
            << QStringLiteral("selection")
    );
    QCOMPARE(main.acquisitionNodeIds, main.activeSourceNodeIds);
    QVERIFY(!result.plan.triggers
        .value(FunctionFlowTrigger::ScreenshotHotkey).available);
    QVERIFY(!result.plan.triggers
        .value(FunctionFlowTrigger::ScreenshotLauncher).available);

    FunctionFlowGraph screenshot = compiledGraph();
    FunctionFlowNode source = node(
        QStringLiteral("screenshot"),
        FunctionFlowNodeType::ScreenshotSource
    );
    source.config.screenshot.triggerMode =
        QStringLiteral("separateAndLauncher");
    FunctionFlowNode input =
        node(QStringLiteral("screenshot_input"), FunctionFlowNodeType::Input);
    input.config.input.required = false;
    screenshot.nodes << source << input;
    screenshot.edges
        << edge(
            QStringLiteral("screenshot-input"),
            QStringLiteral("screenshot"),
            QStringLiteral("text_out"),
            QStringLiteral("screenshot_input"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("screenshot-model"),
            QStringLiteral("screenshot_input"),
            QStringLiteral("text_out"),
            QStringLiteral("model"),
            QStringLiteral("text_in")
        );

    const FunctionFlowCompileResult screenshotResult =
        FunctionFlowCompiler::compile(screenshot, 2, fullHash());
    QVERIFY(screenshotResult.ok);
    QVERIFY(screenshotResult.plan.triggers
        .value(FunctionFlowTrigger::ScreenshotHotkey).available);
    QVERIFY(screenshotResult.plan.triggers
        .value(FunctionFlowTrigger::ScreenshotLauncher).available);
}

void FunctionFlowCompilerTests::
ordersTerminalActionsByEdgeOrderThenNodeId()
{
    FunctionFlowGraph graph = compiledGraph();
    graph.nodes
        << node(QStringLiteral("write-z"), FunctionFlowNodeType::AutoWrite)
        << node(QStringLiteral("write-a"), FunctionFlowNodeType::AutoWrite);
    graph.edges
        << edge(
            QStringLiteral("output-write-z"),
            QStringLiteral("output"),
            QStringLiteral("action_out"),
            QStringLiteral("write-z"),
            QStringLiteral("action_in"),
            1
        )
        << edge(
            QStringLiteral("output-write-a"),
            QStringLiteral("output"),
            QStringLiteral("action_out"),
            QStringLiteral("write-a"),
            QStringLiteral("action_in"),
            1
        );

    const FunctionFlowCompileResult result =
        FunctionFlowCompiler::compile(graph, 1, fullHash());

    QVERIFY(result.ok);
    QCOMPARE(
        result.plan.terminalActionNodeIds,
        QStringList()
            << QStringLiteral("write-a")
            << QStringLiteral("write-z")
            << QStringLiteral("popup")
    );
}

void FunctionFlowCompilerTests::ordersRemainingNodesByTopologicalLayer()
{
    FunctionFlowGraph graph = compiledGraph();
    findNode(&graph, QStringLiteral("model"))->id =
        QStringLiteral("model-a");
    for (FunctionFlowEdge &candidate : graph.edges) {
        if (candidate.fromNodeId == QStringLiteral("model")) {
            candidate.fromNodeId = QStringLiteral("model-a");
        }
        if (candidate.toNodeId == QStringLiteral("model")) {
            candidate.toNodeId = QStringLiteral("model-a");
        }
    }
    graph.edges.removeAt(4);

    FunctionFlowNode modelZ =
        node(QStringLiteral("model-z"), FunctionFlowNodeType::Model);
    FunctionFlowNode deepInput =
        node(QStringLiteral("deep-input"), FunctionFlowNodeType::Input);
    FunctionFlowNode zInput =
        node(QStringLiteral("z-input"), FunctionFlowNodeType::Input);
    FunctionFlowNode finalModel =
        node(QStringLiteral("final-model"), FunctionFlowNodeType::Model);
    graph.nodes << modelZ << deepInput << zInput << finalModel;
    graph.edges
        << edge(
            QStringLiteral("instruction-model-z"),
            QStringLiteral("input_instruction"),
            QStringLiteral("text_out"),
            QStringLiteral("model-z"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("source-model-z"),
            QStringLiteral("input_source"),
            QStringLiteral("text_out"),
            QStringLiteral("model-z"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("model-a-deep"),
            QStringLiteral("model-a"),
            QStringLiteral("text_out"),
            QStringLiteral("deep-input"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("model-z-input"),
            QStringLiteral("model-z"),
            QStringLiteral("text_out"),
            QStringLiteral("z-input"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("deep-final"),
            QStringLiteral("deep-input"),
            QStringLiteral("text_out"),
            QStringLiteral("final-model"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("z-final"),
            QStringLiteral("z-input"),
            QStringLiteral("text_out"),
            QStringLiteral("final-model"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("final-output"),
            QStringLiteral("final-model"),
            QStringLiteral("text_out"),
            QStringLiteral("output"),
            QStringLiteral("text_in")
        );

    const FunctionFlowCompileResult result =
        FunctionFlowCompiler::compile(graph, 1, fullHash());

    QVERIFY(result.ok);
    const int modelAIndex =
        result.plan.topologicalNodeIds.indexOf(QStringLiteral("model-a"));
    const int modelZIndex =
        result.plan.topologicalNodeIds.indexOf(QStringLiteral("model-z"));
    const int deepInputIndex =
        result.plan.topologicalNodeIds.indexOf(QStringLiteral("deep-input"));
    QVERIFY(modelAIndex >= 0);
    QVERIFY(modelZIndex > modelAIndex);
    QVERIFY(deepInputIndex > modelZIndex);
}

void FunctionFlowCompilerTests::prunesDisabledNodesAndEdges()
{
    FunctionFlowGraph graph = compiledGraph();
    FunctionFlowNode disabled =
        node(QStringLiteral("disabled"), FunctionFlowNodeType::Input);
    disabled.enabled = false;
    graph.nodes << disabled;
    graph.edges << edge(
        QStringLiteral("voice-instruction"),
        QStringLiteral("voice"),
        QStringLiteral("future-port"),
        QStringLiteral("disabled"),
        QStringLiteral("text_in")
    );

    const FunctionFlowCompileResult result =
        FunctionFlowCompiler::compile(graph, 1, fullHash());

    QVERIFY(result.ok);
    QVERIFY(!result.plan.nodes.contains(QStringLiteral("disabled")));
    QVERIFY(!result.plan.topologicalNodeIds.contains(
        QStringLiteral("disabled")
    ));
}

void FunctionFlowCompilerTests::recordsTheStreamingPopupNode()
{
    FunctionFlowGraph graph = compiledGraph();
    findNode(&graph, QStringLiteral("model"))
        ->config.model.stream = true;

    const FunctionFlowCompileResult result =
        FunctionFlowCompiler::compile(graph, 1, fullHash());

    QVERIFY(result.ok);
    QCOMPARE(
        result.plan.nodes.value(QStringLiteral("model"))
            .streamingResultPopupNodeId,
        QStringLiteral("popup")
    );
}

void FunctionFlowCompilerTests::
marksExplicitPopupAsCoveringAutoWriteFallback()
{
    FunctionFlowGraph graph = compiledGraph();
    FunctionFlowNode write =
        node(QStringLiteral("write"), FunctionFlowNodeType::AutoWrite);
    write.config.autoWrite.fallbackToPopup = true;
    graph.nodes << write;
    graph.edges << edge(
        QStringLiteral("output-write"),
        QStringLiteral("output"),
        QStringLiteral("action_out"),
        QStringLiteral("write"),
        QStringLiteral("action_in"),
        1
    );

    const FunctionFlowCompileResult result =
        FunctionFlowCompiler::compile(graph, 1, fullHash());

    QVERIFY(result.ok);
    const FunctionFlowCompiledNode compiledWrite =
        result.plan.nodes.value(QStringLiteral("write"));
    QVERIFY(compiledWrite.config.autoWrite.fallbackToPopup);
    QVERIFY(compiledWrite.autoWriteFallbackCoveredByExplicitPopup);
}

void FunctionFlowCompilerTests::
rejectsMalformedGraphsWithoutReturningAPartialPlan()
{
    FunctionFlowGraph dangling = compiledGraph();
    dangling.edges[0].fromNodeId = QStringLiteral("missing");
    const FunctionFlowCompileResult danglingResult =
        FunctionFlowCompiler::compile(dangling, 1, fullHash());
    QVERIFY(!danglingResult.ok);
    QCOMPARE(
        danglingResult.error.code,
        QStringLiteral("flow_dangling_edge")
    );
    QVERIFY(danglingResult.plan.nodes.isEmpty());

    FunctionFlowGraph cycle = compiledGraph();
    cycle.edges << edge(
        QStringLiteral("cycle"),
        QStringLiteral("model"),
        QStringLiteral("text_out"),
        QStringLiteral("input_instruction"),
        QStringLiteral("text_in")
    );
    const FunctionFlowCompileResult cycleResult =
        FunctionFlowCompiler::compile(cycle, 1, fullHash());
    QVERIFY(!cycleResult.ok);
    QCOMPARE(cycleResult.error.code, QStringLiteral("flow_cycle"));
    QVERIFY(cycleResult.plan.nodes.isEmpty());

    FunctionFlowGraph noAction = compiledGraph();
    noAction.edges.removeLast();
    const FunctionFlowCompileResult noActionResult =
        FunctionFlowCompiler::compile(noAction, 1, fullHash());
    QVERIFY(!noActionResult.ok);
    QCOMPARE(
        noActionResult.error.code,
        QStringLiteral("flow_output_count")
    );
    QVERIFY(noActionResult.plan.nodes.isEmpty());

    FunctionFlowGraph cardinality = compiledGraph();
    cardinality.edges << edge(
        QStringLiteral("source-output-copy"),
        QStringLiteral("input_source"),
        QStringLiteral("text_out"),
        QStringLiteral("output"),
        QStringLiteral("text_in")
    );
    const FunctionFlowCompileResult cardinalityResult =
        FunctionFlowCompiler::compile(cardinality, 1, fullHash());
    QVERIFY(!cardinalityResult.ok);
    QCOMPARE(
        cardinalityResult.error.code,
        QStringLiteral("flow_port_cardinality")
    );
    QVERIFY(cardinalityResult.plan.nodes.isEmpty());

    FunctionFlowGraph unreachable = compiledGraph();
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
    const FunctionFlowCompileResult unreachableResult =
        FunctionFlowCompiler::compile(unreachable, 1, fullHash());
    QVERIFY(!unreachableResult.ok);
    QCOMPARE(
        unreachableResult.error.code,
        QStringLiteral("flow_enabled_node_unreachable")
    );
    QVERIFY(unreachableResult.plan.nodes.isEmpty());

    FunctionFlowGraph emptyNodeId = compiledGraph();
    findNode(&emptyNodeId, QStringLiteral("voice"))->id.clear();
    emptyNodeId.edges[0].fromNodeId.clear();
    const FunctionFlowCompileResult emptyNodeResult =
        FunctionFlowCompiler::compile(emptyNodeId, 1, fullHash());
    QVERIFY(!emptyNodeResult.ok);
    QCOMPARE(
        emptyNodeResult.error.code,
        QStringLiteral("flow_node_config_invalid")
    );

    FunctionFlowGraph emptyEdgeId = compiledGraph();
    emptyEdgeId.edges[0].id.clear();
    const FunctionFlowCompileResult emptyEdgeResult =
        FunctionFlowCompiler::compile(emptyEdgeId, 1, fullHash());
    QVERIFY(!emptyEdgeResult.ok);
    QCOMPARE(
        emptyEdgeResult.error.code,
        QStringLiteral("flow_node_config_invalid")
    );
}

QTEST_MAIN(FunctionFlowCompilerTests)

#include "function_flow_compiler_tests.moc"
