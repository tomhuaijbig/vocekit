#include <QtTest>

#include "../../src/controllers/function_flow_plan_cache.h"

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
    const QString &toPortId)
{
    FunctionFlowEdge value;
    value.id = id;
    value.fromNodeId = fromNodeId;
    value.fromPortId = fromPortId;
    value.toNodeId = toNodeId;
    value.toPortId = toPortId;
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

AppSettingsData settingsWithPublishedFlow()
{
    AppSettingsData settings;
    FunctionSettings function;
    function.id = QStringLiteral("custom_1");
    function.name = QStringLiteral("Custom");
    function.executionMode = FunctionExecutionMode::Canvas;
    function.flow.enabled = false;
    function.flow.published.revision = 4;
    function.flow.published.sourceDraftRevision = 6;
    function.flow.published.graph = validGraph();
    function.flow.published.graphHash =
        functionFlowGraphHash(function.flow.published.graph);
    function = normalizeFunctionSettings(function);
    settings.functions << function;
    settings.functionOrder << function.id;
    return settings;
}

} // namespace

class FunctionFlowPlanCacheTests : public QObject
{
    Q_OBJECT

private slots:
    void cachesOnlyCanvasSupportedPublishedPlans();
    void removesClassicDeletedAndUnsupportedEntries();
    void rejectsHashMismatchAndCompilationFailure();
    void replacesCacheWithoutMutatingHeldPlans();
};

void FunctionFlowPlanCacheTests::
cachesOnlyCanvasSupportedPublishedPlans()
{
    AppSettingsData settings = settingsWithPublishedFlow();
    QCOMPARE(
        settings.functions.first().executionMode,
        FunctionExecutionMode::Canvas
    );
    QVERIFY(settings.functions.first().flow.enabled);
    settings.functions.first().flow.enabled = false;
    FunctionFlowPlanCache cache;
    cache.rebuildAll(settings);

    const QSharedPointer<const FunctionFlowExecutionPlan> plan =
        cache.plan(QStringLiteral("custom_1"));
    QVERIFY(!plan.isNull());
    QCOMPARE(plan->publishedRevision, 4);
    QCOMPARE(
        plan->publishedHash,
        settings.functions.first().flow.published.graphHash
    );
    QVERIFY(plan->triggers.value(
        FunctionFlowTrigger::MainHotkey
    ).available);
    QVERIFY(cache.error(QStringLiteral("custom_1")).isEmpty());
}

void FunctionFlowPlanCacheTests::
removesClassicDeletedAndUnsupportedEntries()
{
    AppSettingsData settings = settingsWithPublishedFlow();
    FunctionFlowPlanCache cache;
    cache.rebuildAll(settings);
    QVERIFY(!cache.plan(QStringLiteral("custom_1")).isNull());

    settings.functions[0].executionMode =
        FunctionExecutionMode::Classic;
    settings.functions[0].flow.enabled = true;
    settings.functions[0] =
        normalizeFunctionSettings(settings.functions[0]);
    QCOMPARE(
        settings.functions[0].executionMode,
        FunctionExecutionMode::Classic
    );
    QVERIFY(!settings.functions[0].flow.enabled);
    settings.functions[0].flow.enabled = true;
    cache.rebuildFunction(settings, QStringLiteral("custom_1"));
    QVERIFY(cache.plan(QStringLiteral("custom_1")).isNull());
    QVERIFY(cache.error(QStringLiteral("custom_1")).isEmpty());

    settings.functions[0].executionMode =
        FunctionExecutionMode::Canvas;
    settings.functions[0].flow.enabled = false;
    settings.functions[0] =
        normalizeFunctionSettings(settings.functions[0]);
    QCOMPARE(
        settings.functions[0].executionMode,
        FunctionExecutionMode::Canvas
    );
    QVERIFY(settings.functions[0].flow.enabled);
    settings.functions[0].flow.enabled = false;
    settings.functions[0].flow.published.supported = false;
    settings.functions[0].flow.published.unavailableCode =
        QStringLiteral("flow_schema_newer");
    cache.rebuildFunction(settings, QStringLiteral("custom_1"));
    QVERIFY(cache.plan(QStringLiteral("custom_1")).isNull());
    QCOMPARE(
        cache.error(QStringLiteral("custom_1")).code,
        QStringLiteral("flow_schema_newer")
    );

    settings.functions.clear();
    settings.functionOrder.clear();
    cache.rebuildFunction(settings, QStringLiteral("custom_1"));
    QVERIFY(cache.plan(QStringLiteral("custom_1")).isNull());
    QVERIFY(cache.error(QStringLiteral("custom_1")).isEmpty());
}

void FunctionFlowPlanCacheTests::
rejectsHashMismatchAndCompilationFailure()
{
    AppSettingsData settings = settingsWithPublishedFlow();
    FunctionFlowPlanCache cache;

    settings.functions[0].flow.published.graphHash =
        QString(64, QLatin1Char('0'));
    cache.rebuildFunction(settings, QStringLiteral("custom_1"));
    QVERIFY(cache.plan(QStringLiteral("custom_1")).isNull());
    QCOMPARE(
        cache.error(QStringLiteral("custom_1")).code,
        QStringLiteral("flow_published_hash_mismatch")
    );

    FunctionFlowGraph malformed = validGraph();
    malformed.edges[0].fromNodeId = QStringLiteral("missing");
    VersionedFunctionFlowGraph &published =
        settings.functions[0].flow.published;
    published.graph = malformed;
    published.graphHash = functionFlowGraphHash(malformed);
    cache.rebuildFunction(settings, QStringLiteral("custom_1"));
    QVERIFY(cache.plan(QStringLiteral("custom_1")).isNull());
    QCOMPARE(
        cache.error(QStringLiteral("custom_1")).code,
        QStringLiteral("flow_dangling_edge")
    );
}

void FunctionFlowPlanCacheTests::
replacesCacheWithoutMutatingHeldPlans()
{
    AppSettingsData settings = settingsWithPublishedFlow();
    FunctionFlowPlanCache cache;
    cache.rebuildAll(settings);
    const QSharedPointer<const FunctionFlowExecutionPlan> oldPlan =
        cache.plan(QStringLiteral("custom_1"));
    QVERIFY(!oldPlan.isNull());

    VersionedFunctionFlowGraph &published =
        settings.functions[0].flow.published;
    published.revision = 5;
    published.graph.nodes[0]
        .config.voice.recording.countdownSeconds = 4;
    published.graphHash = functionFlowGraphHash(published.graph);
    cache.rebuildFunction(settings, QStringLiteral("custom_1"));

    const QSharedPointer<const FunctionFlowExecutionPlan> newPlan =
        cache.plan(QStringLiteral("custom_1"));
    QVERIFY(!newPlan.isNull());
    QVERIFY(oldPlan != newPlan);
    QCOMPARE(oldPlan->publishedRevision, 4);
    QCOMPARE(newPlan->publishedRevision, 5);
    QVERIFY(oldPlan->publishedHash != newPlan->publishedHash);
}

QTEST_MAIN(FunctionFlowPlanCacheTests)

#include "function_flow_plan_cache_tests.moc"
