#include <QtTest>

#include "../../src/config/app_settings_json.h"
#include "../../src/config/function_flow_json.h"
#include "../../src/domain/function_settings.h"

#include <QJsonArray>
#include <QJsonDocument>

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
    const QString &from,
    const QString &fromPort,
    const QString &to,
    const QString &toPort,
    int order = 0)
{
    FunctionFlowEdge value;
    value.id = id;
    value.fromNodeId = from;
    value.fromPortId = fromPort;
    value.toNodeId = to;
    value.toPortId = toPort;
    value.order = order;
    return value;
}

FunctionFlowGraph graph()
{
    FunctionFlowGraph value;
    FunctionFlowNode voice =
        node(QStringLiteral("voice"), FunctionFlowNodeType::VoiceSource);
    voice.config.voice.speechProviderId = QStringLiteral("speech");
    value.nodes
        << voice
        << node(QStringLiteral("input"), FunctionFlowNodeType::Input)
        << node(QStringLiteral("output"), FunctionFlowNodeType::Output)
        << node(
            QStringLiteral("popup"),
            FunctionFlowNodeType::ResultPopup
        );
    value.edges
        << edge(
            QStringLiteral("voice-input"),
            QStringLiteral("voice"),
            QStringLiteral("text_out"),
            QStringLiteral("input"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("input-output"),
            QStringLiteral("input"),
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
            1
        );
    return value;
}

FunctionFlowState state()
{
    FunctionFlowState value;
    value.enabled = true;
    value.editor.viewportCenter = QPointF(120.5, -44.0);
    value.editor.zoom = 1.75;
    value.draft.revision = 3;
    value.draft.graph = graph();
    value.draft.graphHash = functionFlowGraphHash(value.draft.graph);
    value.published.revision = 2;
    value.published.sourceDraftRevision = 3;
    value.published.graph = graph();
    value.published.graphHash =
        functionFlowGraphHash(value.published.graph);
    return value;
}

QJsonObject validFunctionFlowJson()
{
    return functionFlowStateToJson(state());
}

QJsonObject customFunctionJson()
{
    QJsonObject custom;
    custom.insert(QStringLiteral("id"), QStringLiteral("custom_1"));
    custom.insert(QStringLiteral("name"), QStringLiteral("Custom"));
    custom.insert(QStringLiteral("shortcut"), QStringLiteral("Ctrl+Alt+C"));
    return custom;
}

} // namespace

class FunctionFlowJsonTests : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsKnownFieldsAndEditorState();
    void preservesUnknownFieldsAtEveryKnownLevel();
    void keepsRuntimePayloadsOutOfGraphJson();
    void preservesFutureAndCorruptBranchesVerbatim();
    void preservesUnknownCurrentNodeTypesVerbatim();
    void recomputesDraftHashButRejectsUntrustedPublishedHash();
    void keepsValidPublishedWhenDraftIsCorrupt();
    void loadsOldSettingsWithDisabledEmptyFlows();
    void mapsKnownAndOrphanFlowsWithoutDuplicatingTheRootKey();
    void normalizationPreservesExistingFlowState();
    void executionModeRoundTripsAndMirrorsEnabled();
    void legacyEnabledMigratesExecutionMode();
    void unknownExecutionModeIsRetainedButRunsClassic();
    void whitespaceExecutionModeLoadsCanvasAndWritesCanonicalId();
    void invalidExecutionModeValuesAreRetainedAndRunClassic_data();
    void invalidExecutionModeValuesAreRetainedAndRunClassic();
};

void FunctionFlowJsonTests::roundTripsKnownFieldsAndEditorState()
{
    const FunctionFlowState original = state();
    const QJsonObject raw = functionFlowStateToJson(original);

    QVERIFY(raw.value(QStringLiteral("editor")).isObject());
    QVERIFY(!raw.value(QStringLiteral("draft"))
        .toObject()
        .value(QStringLiteral("graph"))
        .toObject()
        .contains(QStringLiteral("editor")));

    const FunctionFlowState restored = functionFlowStateFromJson(raw);
    QVERIFY(restored.enabled);
    QCOMPARE(restored.editor.viewportCenter, QPointF(120.5, -44.0));
    QCOMPARE(restored.editor.zoom, qreal(1.75));
    QCOMPARE(restored.draft.revision, 3);
    QCOMPARE(restored.published.revision, 2);
    QCOMPARE(restored.published.sourceDraftRevision, 3);
    QCOMPARE(restored.draft.graph.nodes.size(), 4);
    QCOMPARE(restored.draft.graph.edges.size(), 3);
    QCOMPARE(
        restored.draft.graphHash,
        functionFlowGraphHash(restored.draft.graph)
    );
    QVERIFY(restored.published.supported);
}

void FunctionFlowJsonTests::preservesUnknownFieldsAtEveryKnownLevel()
{
    QJsonObject raw = validFunctionFlowJson();
    raw.insert(QStringLiteral("futureField"), 42);

    QJsonObject draft = raw.value(QStringLiteral("draft")).toObject();
    draft.insert(QStringLiteral("futureVersionField"), QStringLiteral("v"));
    QJsonObject graphObject =
        draft.value(QStringLiteral("graph")).toObject();
    graphObject.insert(QStringLiteral("futureGraphField"), true);
    QJsonArray nodes =
        graphObject.value(QStringLiteral("nodes")).toArray();
    QJsonObject firstNode = nodes.first().toObject();
    firstNode.insert(QStringLiteral("futureNodeField"), 7);
    QJsonObject config =
        firstNode.value(QStringLiteral("config")).toObject();
    config.insert(QStringLiteral("futureConfigField"), 8);
    firstNode.insert(QStringLiteral("config"), config);
    nodes[0] = firstNode;
    graphObject.insert(QStringLiteral("nodes"), nodes);
    QJsonArray edges =
        graphObject.value(QStringLiteral("edges")).toArray();
    QJsonObject firstEdge = edges.first().toObject();
    firstEdge.insert(QStringLiteral("futureEdgeField"), 9);
    edges[0] = firstEdge;
    graphObject.insert(QStringLiteral("edges"), edges);
    draft.insert(QStringLiteral("graph"), graphObject);
    raw.insert(QStringLiteral("draft"), draft);

    const FunctionFlowState restored =
        functionFlowStateFromJson(raw);
    const QJsonObject written = functionFlowStateToJson(restored);

    QCOMPARE(written.value(QStringLiteral("futureField")).toInt(), 42);
    const QJsonObject writtenDraft =
        written.value(QStringLiteral("draft")).toObject();
    QCOMPARE(
        writtenDraft.value(QStringLiteral("futureVersionField")).toString(),
        QStringLiteral("v")
    );
    const QJsonObject writtenGraph =
        writtenDraft.value(QStringLiteral("graph")).toObject();
    QVERIFY(writtenGraph
        .value(QStringLiteral("futureGraphField")).toBool());
    const QJsonObject writtenNode = writtenGraph
        .value(QStringLiteral("nodes")).toArray().first().toObject();
    QCOMPARE(
        writtenNode.value(QStringLiteral("futureNodeField")).toInt(),
        7
    );
    QCOMPARE(
        writtenNode.value(QStringLiteral("config"))
            .toObject()
            .value(QStringLiteral("futureConfigField"))
            .toInt(),
        8
    );
    QCOMPARE(
        writtenGraph.value(QStringLiteral("edges"))
            .toArray().first().toObject()
            .value(QStringLiteral("futureEdgeField")).toInt(),
        9
    );
}

void FunctionFlowJsonTests::keepsRuntimePayloadsOutOfGraphJson()
{
    const QByteArray bytes = QJsonDocument(
        functionFlowStateToJson(state())
    ).toJson(QJsonDocument::Compact);

    QVERIFY(!bytes.contains("apiKey"));
    QVERIFY(!bytes.contains("sourceAudioPath"));
    QVERIFY(!bytes.contains("recognizedText"));
    QVERIFY(!bytes.contains("recordingSegments"));
    QVERIFY(!bytes.contains("\"image\""));
    QVERIFY(!bytes.contains("runtimeValues"));
}

void FunctionFlowJsonTests::preservesFutureAndCorruptBranchesVerbatim()
{
    QJsonObject futureRaw = validFunctionFlowJson();
    QJsonObject futureDraft =
        futureRaw.value(QStringLiteral("draft")).toObject();
    futureDraft.insert(QStringLiteral("futureVersionField"), 77);
    QJsonObject futureGraph =
        futureDraft.value(QStringLiteral("graph")).toObject();
    futureGraph.insert(QStringLiteral("schemaVersion"), 2);
    futureDraft.insert(QStringLiteral("graph"), futureGraph);
    futureRaw.insert(QStringLiteral("draft"), futureDraft);

    QStringList warnings;
    const FunctionFlowState future =
        functionFlowStateFromJson(futureRaw, &warnings);
    QVERIFY(!future.draft.supported);
    QCOMPARE(
        future.draft.unavailableCode,
        QStringLiteral("flow_schema_newer")
    );
    QCOMPARE(
        warnings.count(QStringLiteral("flow_schema_newer")),
        1
    );
    QCOMPARE(
        functionFlowStateToJson(future)
            .value(QStringLiteral("draft")).toObject(),
        futureDraft
    );

    QJsonObject corruptRaw = validFunctionFlowJson();
    QJsonObject corruptDraft =
        corruptRaw.value(QStringLiteral("draft")).toObject();
    QJsonObject corruptGraph =
        corruptDraft.value(QStringLiteral("graph")).toObject();
    corruptGraph.insert(QStringLiteral("nodes"), QStringLiteral("broken"));
    corruptDraft.insert(QStringLiteral("graph"), corruptGraph);
    corruptRaw.insert(QStringLiteral("draft"), corruptDraft);

    const FunctionFlowState corrupt =
        functionFlowStateFromJson(corruptRaw);
    QVERIFY(!corrupt.draft.supported);
    QCOMPARE(
        corrupt.draft.unavailableCode,
        QStringLiteral("flow_json_invalid")
    );
    QCOMPARE(
        functionFlowStateToJson(corrupt)
            .value(QStringLiteral("draft")).toObject(),
        corruptDraft
    );
}

void FunctionFlowJsonTests::
preservesUnknownCurrentNodeTypesVerbatim()
{
    QJsonObject raw = validFunctionFlowJson();
    QJsonObject draft = raw.value(QStringLiteral("draft")).toObject();
    QJsonObject graphObject =
        draft.value(QStringLiteral("graph")).toObject();
    QJsonArray nodes =
        graphObject.value(QStringLiteral("nodes")).toArray();
    QJsonObject firstNode = nodes.first().toObject();
    firstNode.insert(QStringLiteral("type"), QStringLiteral("futureNode"));
    nodes[0] = firstNode;
    graphObject.insert(QStringLiteral("nodes"), nodes);
    draft.insert(QStringLiteral("graph"), graphObject);
    raw.insert(QStringLiteral("draft"), draft);

    const FunctionFlowState restored =
        functionFlowStateFromJson(raw);
    QVERIFY(!restored.draft.supported);
    QCOMPARE(
        restored.draft.unavailableCode,
        QStringLiteral("flow_node_type_unsupported")
    );
    QCOMPARE(
        functionFlowStateToJson(restored)
            .value(QStringLiteral("draft")).toObject(),
        draft
    );
}

void FunctionFlowJsonTests::
recomputesDraftHashButRejectsUntrustedPublishedHash()
{
    QJsonObject raw = validFunctionFlowJson();
    QJsonObject draft = raw.value(QStringLiteral("draft")).toObject();
    draft.insert(QStringLiteral("graphHash"), QString(64, QLatin1Char('0')));
    raw.insert(QStringLiteral("draft"), draft);

    FunctionFlowState restored = functionFlowStateFromJson(raw);
    QVERIFY(restored.draft.supported);
    QCOMPARE(
        restored.draft.graphHash,
        functionFlowGraphHash(restored.draft.graph)
    );

    QJsonObject invalidPublishedRaw = validFunctionFlowJson();
    QJsonObject published =
        invalidPublishedRaw.value(QStringLiteral("published")).toObject();
    published.insert(QStringLiteral("graphHash"), QStringLiteral("bad"));
    invalidPublishedRaw.insert(QStringLiteral("published"), published);
    restored = functionFlowStateFromJson(invalidPublishedRaw);
    QVERIFY(!restored.published.supported);
    QCOMPARE(
        restored.published.unavailableCode,
        QStringLiteral("flow_published_hash_mismatch")
    );
    QCOMPARE(
        functionFlowStateToJson(restored)
            .value(QStringLiteral("published")).toObject(),
        published
    );

    published.remove(QStringLiteral("graphHash"));
    invalidPublishedRaw.insert(QStringLiteral("published"), published);
    restored = functionFlowStateFromJson(invalidPublishedRaw);
    QVERIFY(!restored.published.supported);
    QCOMPARE(
        restored.published.unavailableCode,
        QStringLiteral("flow_published_hash_mismatch")
    );
}

void FunctionFlowJsonTests::keepsValidPublishedWhenDraftIsCorrupt()
{
    QJsonObject raw = validFunctionFlowJson();
    QJsonObject draft = raw.value(QStringLiteral("draft")).toObject();
    draft.insert(QStringLiteral("graph"), QStringLiteral("broken"));
    raw.insert(QStringLiteral("draft"), draft);

    const FunctionFlowState restored =
        functionFlowStateFromJson(raw);

    QVERIFY(!restored.draft.supported);
    QVERIFY(restored.published.supported);
    QCOMPARE(restored.published.revision, 2);
    QVERIFY(restored.enabled);
}

void FunctionFlowJsonTests::loadsOldSettingsWithDisabledEmptyFlows()
{
    const AppSettingsData data =
        appSettingsDataFromJson(QJsonObject());

    QCOMPARE(data.functions.size(), 3);
    for (const FunctionSettings &function : data.functions) {
        QVERIFY(!function.flow.enabled);
        QCOMPARE(function.flow.draft.revision, 0);
        QVERIFY(function.flow.draft.graph.nodes.isEmpty());
        QVERIFY(function.flow.draft.graphHash.isEmpty());
    }
}

void FunctionFlowJsonTests::
mapsKnownAndOrphanFlowsWithoutDuplicatingTheRootKey()
{
    QJsonObject root;
    root.insert(
        QStringLiteral("customFunctions"),
        QJsonArray() << customFunctionJson()
    );
    QJsonObject flows;
    flows.insert(QStringLiteral("dictate"), validFunctionFlowJson());
    flows.insert(QStringLiteral("custom_1"), validFunctionFlowJson());
    QJsonObject orphan;
    orphan.insert(QStringLiteral("future"), 99);
    flows.insert(QStringLiteral("removed_function"), orphan);
    root.insert(QStringLiteral("functionFlows"), flows);

    const AppSettingsData data = appSettingsDataFromJson(root);
    QVERIFY(data.function(QStringLiteral("dictate")).flow.enabled);
    QVERIFY(data.function(QStringLiteral("custom_1")).flow.enabled);
    QCOMPARE(
        data.retainedOrphanFunctionFlows
            .value(QStringLiteral("removed_function")).toObject(),
        orphan
    );
    QVERIFY(!data.retainedRootValues.contains(
        QStringLiteral("functionFlows")
    ));

    const QJsonObject written = appSettingsDataToJson(data);
    QCOMPARE(written.keys().count(QStringLiteral("functionFlows")), 1);
    const QJsonObject writtenFlows =
        written.value(QStringLiteral("functionFlows")).toObject();
    QVERIFY(writtenFlows.contains(QStringLiteral("dictate")));
    QVERIFY(writtenFlows.contains(QStringLiteral("custom_1")));
    QCOMPARE(
        writtenFlows.value(QStringLiteral("removed_function")).toObject(),
        orphan
    );
}

void FunctionFlowJsonTests::normalizationPreservesExistingFlowState()
{
    FunctionSettings settings;
    settings.id = QStringLiteral(" custom ");
    settings.name = QStringLiteral(" Custom ");
    settings.executionMode = FunctionExecutionMode::Canvas;
    settings.flow = state();
    settings.flow.draft.revision = 8;

    const FunctionSettings normalized =
        normalizeFunctionSettings(settings);

    QCOMPARE(normalized.id, QStringLiteral("custom"));
    QCOMPARE(normalized.flow.draft.revision, 8);
    QCOMPARE(
        normalized.flow.draft.graphHash,
        settings.flow.draft.graphHash
    );
    QVERIFY(normalized.flow.enabled);
}

void FunctionFlowJsonTests::
executionModeRoundTripsAndMirrorsEnabled()
{
    QJsonObject flow = validFunctionFlowJson();
    flow.insert(QStringLiteral("executionMode"), QStringLiteral("canvas"));
    flow.insert(QStringLiteral("enabled"), false);

    QJsonObject root;
    QJsonObject flows;
    flows.insert(QStringLiteral("dictate"), flow);
    root.insert(QStringLiteral("functionFlows"), flows);

    const AppSettingsData data = appSettingsDataFromJson(root);
    const FunctionSettings &settings =
        data.function(QStringLiteral("dictate"));
    QVERIFY(settings.executionMode == FunctionExecutionMode::Canvas);
    QVERIFY(settings.flow.enabled);

    const QJsonObject writtenFlow = appSettingsDataToJson(data)
        .value(QStringLiteral("functionFlows")).toObject()
        .value(QStringLiteral("dictate")).toObject();
    QCOMPARE(
        writtenFlow.value(QStringLiteral("executionMode")).toString(),
        QStringLiteral("canvas")
    );
    QVERIFY(writtenFlow.value(QStringLiteral("enabled")).toBool());
}

void FunctionFlowJsonTests::legacyEnabledMigratesExecutionMode()
{
    QJsonObject flow = validFunctionFlowJson();
    flow.remove(QStringLiteral("executionMode"));
    flow.insert(QStringLiteral("enabled"), true);

    QJsonObject root;
    QJsonObject flows;
    flows.insert(QStringLiteral("dictate"), flow);
    root.insert(QStringLiteral("functionFlows"), flows);

    const AppSettingsData data = appSettingsDataFromJson(root);
    const FunctionSettings &settings =
        data.function(QStringLiteral("dictate"));
    QVERIFY(settings.executionMode == FunctionExecutionMode::Canvas);
    QVERIFY(settings.flow.enabled);
}

void FunctionFlowJsonTests::
unknownExecutionModeIsRetainedButRunsClassic()
{
    QJsonObject flow = validFunctionFlowJson();
    flow.insert(
        QStringLiteral("executionMode"),
        QStringLiteral("future-mode")
    );
    flow.insert(QStringLiteral("enabled"), true);

    QJsonObject root;
    QJsonObject flows;
    flows.insert(QStringLiteral("dictate"), flow);
    root.insert(QStringLiteral("functionFlows"), flows);

    const AppSettingsData data = appSettingsDataFromJson(root);
    const FunctionSettings &settings =
        data.function(QStringLiteral("dictate"));
    QVERIFY(settings.executionMode == FunctionExecutionMode::Classic);
    QVERIFY(!settings.flow.enabled);

    const QJsonObject writtenFlow = appSettingsDataToJson(data)
        .value(QStringLiteral("functionFlows")).toObject()
        .value(QStringLiteral("dictate")).toObject();
    QCOMPARE(
        writtenFlow.value(QStringLiteral("executionMode")).toString(),
        QStringLiteral("future-mode")
    );
    QVERIFY(!writtenFlow.value(QStringLiteral("enabled")).toBool(true));
}

void FunctionFlowJsonTests::
whitespaceExecutionModeLoadsCanvasAndWritesCanonicalId()
{
    QJsonObject flow = validFunctionFlowJson();
    flow.insert(
        QStringLiteral("executionMode"),
        QStringLiteral(" canvas ")
    );
    flow.insert(QStringLiteral("enabled"), false);

    QJsonObject root;
    QJsonObject flows;
    flows.insert(QStringLiteral("dictate"), flow);
    root.insert(QStringLiteral("functionFlows"), flows);

    const AppSettingsData data = appSettingsDataFromJson(root);
    const FunctionSettings &settings =
        data.function(QStringLiteral("dictate"));
    QVERIFY(settings.executionMode == FunctionExecutionMode::Canvas);
    QVERIFY(settings.flow.enabled);

    const QJsonObject writtenFlow = appSettingsDataToJson(data)
        .value(QStringLiteral("functionFlows")).toObject()
        .value(QStringLiteral("dictate")).toObject();
    QCOMPARE(
        writtenFlow.value(QStringLiteral("executionMode")).toString(),
        QStringLiteral("canvas")
    );
    QVERIFY(writtenFlow.value(QStringLiteral("enabled")).toBool());
}

void FunctionFlowJsonTests::
invalidExecutionModeValuesAreRetainedAndRunClassic_data()
{
    QTest::addColumn<QJsonValue>("rawMode");

    QJsonObject object;
    object.insert(QStringLiteral("future"), 1);
    QJsonArray array;
    array.append(QStringLiteral("future"));

    QTest::newRow("null") << QJsonValue(QJsonValue::Null);
    QTest::newRow("bool") << QJsonValue(true);
    QTest::newRow("number") << QJsonValue(7);
    QTest::newRow("object") << QJsonValue(object);
    QTest::newRow("array") << QJsonValue(array);
}

void FunctionFlowJsonTests::
invalidExecutionModeValuesAreRetainedAndRunClassic()
{
    QFETCH(QJsonValue, rawMode);

    QJsonObject flow = validFunctionFlowJson();
    flow.insert(QStringLiteral("executionMode"), rawMode);
    flow.insert(QStringLiteral("enabled"), true);

    QJsonObject root;
    QJsonObject flows;
    flows.insert(QStringLiteral("dictate"), flow);
    root.insert(QStringLiteral("functionFlows"), flows);

    const AppSettingsData data = appSettingsDataFromJson(root);
    const FunctionSettings &settings =
        data.function(QStringLiteral("dictate"));
    QVERIFY(settings.executionMode == FunctionExecutionMode::Classic);
    QVERIFY(!settings.flow.enabled);

    const QJsonObject writtenFlow = appSettingsDataToJson(data)
        .value(QStringLiteral("functionFlows")).toObject()
        .value(QStringLiteral("dictate")).toObject();
    QVERIFY(
        writtenFlow.value(QStringLiteral("executionMode")) == rawMode
    );
    QVERIFY(!writtenFlow.value(QStringLiteral("enabled")).toBool(true));
}

QTEST_MAIN(FunctionFlowJsonTests)

#include "function_flow_json_tests.moc"
