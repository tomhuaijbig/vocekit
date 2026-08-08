#include <QtTest>

#include "../../src/app/application_events.h"
#include "../../src/controllers/function_flow_publication_service.h"
#include "../../src/domain/function_flow_compiler.h"

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

FunctionSettings functionSettings(
    const QString &id = QStringLiteral("custom_1"))
{
    FunctionSettings function;
    function.id = id;
    function.name = QStringLiteral("Custom");
    function.shortcut = QStringLiteral("Ctrl+Alt+V");
    function.modelId = QStringLiteral("model");
    function.promptId = QStringLiteral("prompt");
    function.flow.draft.revision = 3;
    function.flow.draft.graph = validGraph();
    function.flow.draft.graphHash =
        functionFlowGraphHash(function.flow.draft.graph);
    return function;
}

VersionedFunctionFlowGraph publishedVersion(
    const FunctionFlowGraph &graph,
    int revision,
    int sourceDraftRevision)
{
    VersionedFunctionFlowGraph version;
    version.revision = revision;
    version.sourceDraftRevision = sourceDraftRevision;
    version.graph = normalizeFunctionFlowGraph(graph);
    version.graphHash = functionFlowGraphHash(version.graph);
    return version;
}

struct PublishedEvent
{
    QString key;
    QString functionId;
};

class ServiceHarness
{
public:
    AppSettingsData settings;
    FunctionFlowValidationContext context;
    bool saveSucceeds = true;
    bool compileSucceeds = true;
    int saveCalls = 0;
    QVector<PublishedEvent> events;
    FunctionFlowPublicationService service;

    ServiceHarness()
        : service(makeAccess(this))
    {
        settings.functions << functionSettings();
        settings.functionOrder << QStringLiteral("custom_1");

        context.functionId = QStringLiteral("custom_1");
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
    }

    static FunctionFlowPublicationAccess makeAccess(
        ServiceHarness *harness)
    {
        FunctionFlowPublicationAccess access;
        access.settingsSnapshotProvider = [harness]() {
            return harness->settings;
        };
        access.replaceAndSave = [harness](
            const AppSettingsData &updated,
            OperationError *error) {
            ++harness->saveCalls;
            if (!harness->saveSucceeds) {
                if (error) {
                    error->code = QStringLiteral("test_save_failed");
                }
                return false;
            }
            harness->settings = updated;
            return true;
        };
        access.validationContextProvider = [harness](
            const AppSettingsData &,
            const QString &functionId) {
            FunctionFlowValidationContext result = harness->context;
            result.functionId = functionId;
            return result;
        };
        access.compileGraph = [harness](
            const FunctionFlowGraph &graph,
            int publishedRevision,
            const QString &publishedHash) {
            if (!harness->compileSucceeds) {
                FunctionFlowCompileResult result;
                result.error.code =
                    QStringLiteral("test_compile_failed");
                return result;
            }
            return FunctionFlowCompiler::compile(
                graph,
                publishedRevision,
                publishedHash
            );
        };
        access.publishSettingsChanged = [harness](
            const QString &key,
            const QString &functionId) {
            PublishedEvent event;
            event.key = key;
            event.functionId = functionId;
            harness->events << event;
        };
        return access;
    }

    FunctionSettings &function(const QString &id)
    {
        return settings.functions[settings.functionIndex(id)];
    }

    void clearActivity()
    {
        saveCalls = 0;
        events.clear();
    }
};

} // namespace

class FunctionFlowPublicationServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void readsAndAnalyzesWithoutSideEffects();
    void savesDraftWithOptimisticRevisionAndPreservesEditor();
    void savesEditorWithoutReplacingTheLatestDraft();
    void publishesWithoutChangingExecutionMode();
    void rejectsInvalidStaleAndUnavailablePublication();
    void repairsOnlyConfirmedCorruptPublishedState();
    void requiresValidPublishedGraphForCanvasMode();
    void preservesSpecificCanvasModeFailureReasons();
    void rejectsCanvasModeWhenPublishedCompilationFails();
    void alwaysAllowsClassicAndPreservesFlowData();
    void sameModeIsIdempotentDespiteStaleLegacyEnabledMirror();
    void sameClassicModeClearsRetainedUnknownExecutionMode();
    void sameCanvasModeClearsRetainedUnknownExecutionMode();
    void emitsModeEventsOnlyAfterSuccessfulSave();
    void rejectsPublicationRevisionOverflow();
    void createsAndRemovesOnlyCustomFunctions();
    void emitsEventsOnlyAfterSuccessfulSave();
};

void FunctionFlowPublicationServiceTests::
readsAndAnalyzesWithoutSideEffects()
{
    ServiceHarness harness;
    FunctionFlowState state;
    OperationError error;

    QVERIFY(harness.service.readState(
        QStringLiteral("custom_1"),
        &state,
        &error
    ));
    QCOMPARE(state.draft.revision, 3);
    QVERIFY(error.isEmpty());

    const FunctionFlowDraftAnalysis analysis =
        harness.service.analyzeDraft(
            QStringLiteral("custom_1"),
            validGraph()
        );
    QVERIFY(analysis.validation.ok);
    QCOMPARE(analysis.graphHash, functionFlowGraphHash(validGraph()));
    QVERIFY(analysis.triggerAvailability.value(
        FunctionFlowTrigger::MainHotkey
    ));
    QVERIFY(!analysis.triggerAvailability.value(
        FunctionFlowTrigger::ScreenshotHotkey
    ));
    QVERIFY(!analysis.triggerAvailability.value(
        FunctionFlowTrigger::ScreenshotLauncher
    ));
    QCOMPARE(harness.saveCalls, 0);
    QVERIFY(harness.events.isEmpty());

    QVERIFY(!harness.service.readState(
        QStringLiteral("missing"),
        &state,
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("flow_function_not_found"));
}

void FunctionFlowPublicationServiceTests::
savesDraftWithOptimisticRevisionAndPreservesEditor()
{
    ServiceHarness harness;
    harness.function(QStringLiteral("custom_1")).flow.editor.zoom = 1.75;

    FunctionFlowGraph graph = validGraph();
    graph.nodes[0].config.voice.recording.countdownSeconds = 5;
    int savedRevision = 0;
    OperationError error;

    QVERIFY(harness.service.updateDraft(
        QStringLiteral("custom_1"),
        3,
        graph,
        &savedRevision,
        &error
    ));
    QCOMPARE(savedRevision, 4);
    QCOMPARE(
        harness.function(QStringLiteral("custom_1"))
            .flow.draft.revision,
        4
    );
    QCOMPARE(
        harness.function(QStringLiteral("custom_1")).flow.editor.zoom,
        qreal(1.75)
    );
    QCOMPARE(harness.events.size(), 1);
    QCOMPARE(
        harness.events.first().key,
        functionFlowDraftSettingsKey()
    );

    QVERIFY(!harness.service.updateDraft(
        QStringLiteral("custom_1"),
        3,
        graph,
        &savedRevision,
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("flow_draft_stale"));
    QCOMPARE(harness.saveCalls, 1);
    QCOMPARE(harness.events.size(), 1);
}

void FunctionFlowPublicationServiceTests::
savesEditorWithoutReplacingTheLatestDraft()
{
    ServiceHarness harness;
    FunctionFlowGraph changed = validGraph();
    changed.nodes[0].config.voice.recording.countdownSeconds = 6;

    int savedRevision = 0;
    OperationError error;
    QVERIFY(harness.service.updateDraft(
        QStringLiteral("custom_1"),
        3,
        changed,
        &savedRevision,
        &error
    ));

    FunctionFlowEditorState editor;
    editor.viewportCenter = QPointF(120.0, -45.0);
    editor.zoom = 2.25;
    QVERIFY(harness.service.updateEditorState(
        QStringLiteral("custom_1"),
        editor,
        &error
    ));

    const FunctionFlowState state =
        harness.function(QStringLiteral("custom_1")).flow;
    QCOMPARE(state.draft.revision, 4);
    QCOMPARE(
        state.draft.graphHash,
        functionFlowGraphHash(changed)
    );
    QCOMPARE(state.editor.viewportCenter, QPointF(120.0, -45.0));
    QCOMPARE(state.editor.zoom, qreal(2.25));
    QCOMPARE(harness.events.size(), 2);
    QCOMPARE(
        harness.events.last().key,
        functionFlowEditorStateSettingsKey()
    );
}

void FunctionFlowPublicationServiceTests::
publishesWithoutChangingExecutionMode()
{
    ServiceHarness harness;

    FunctionFlowPublishResult result = harness.service.publish(
        QStringLiteral("custom_1"),
        3
    );
    QVERIFY(result.ok);
    QCOMPARE(result.publishedRevision, 1);
    const FunctionFlowState first =
        harness.function(QStringLiteral("custom_1")).flow;
    QCOMPARE(
        harness.function(QStringLiteral("custom_1")).executionMode,
        FunctionExecutionMode::Classic
    );
    QVERIFY(!first.enabled);
    QCOMPARE(first.published.revision, 1);
    QCOMPARE(first.published.sourceDraftRevision, 3);
    QCOMPARE(first.published.graphHash, first.draft.graphHash);
    QCOMPARE(harness.saveCalls, 1);
    QCOMPARE(harness.events.size(), 1);
    QCOMPARE(
        harness.events.first().key,
        functionFlowPublishedSettingsKey()
    );

    harness.clearActivity();
    result = harness.service.publish(QStringLiteral("custom_1"), 3);
    QVERIFY(result.ok);
    QCOMPARE(result.publishedRevision, 1);
    QCOMPARE(harness.saveCalls, 0);
    QVERIFY(harness.events.isEmpty());

    FunctionFlowGraph presentationOnly =
        harness.function(QStringLiteral("custom_1"))
            .flow.draft.graph;
    presentationOnly.nodes[0].title = QStringLiteral("Voice title");
    presentationOnly.nodes[0].position = QPointF(500.0, 200.0);
    int savedRevision = 0;
    OperationError error;
    QVERIFY(harness.service.updateDraft(
        QStringLiteral("custom_1"),
        3,
        presentationOnly,
        &savedRevision,
        &error
    ));
    QCOMPARE(savedRevision, 4);

    harness.clearActivity();
    result = harness.service.publish(QStringLiteral("custom_1"), 4);
    QVERIFY(result.ok);
    QCOMPARE(result.publishedRevision, 1);
    QCOMPARE(harness.saveCalls, 0);
    QVERIFY(harness.events.isEmpty());

    harness.clearActivity();
    result = harness.service.publish(QStringLiteral("custom_1"), 4);
    QVERIFY(result.ok);
    QCOMPARE(result.publishedRevision, 1);
    QCOMPARE(harness.saveCalls, 0);
    QVERIFY(harness.events.isEmpty());
}

void FunctionFlowPublicationServiceTests::
rejectsInvalidStaleAndUnavailablePublication()
{
    ServiceHarness harness;
    FunctionSettings &function =
        harness.function(QStringLiteral("custom_1"));
    function.flow.published =
        publishedVersion(validGraph(), 7, 2);
    const QString oldHash = function.flow.published.graphHash;

    FunctionFlowPublishResult result = harness.service.publish(
        QStringLiteral("custom_1"),
        2
    );
    QVERIFY(!result.ok);
    QCOMPARE(result.error.code, QStringLiteral("flow_draft_stale"));

    function.flow.draft.graph.nodes[2].config.model.modelId =
        QStringLiteral("missing");
    function.flow.draft.graphHash =
        functionFlowGraphHash(function.flow.draft.graph);
    result = harness.service.publish(QStringLiteral("custom_1"), 3);
    QVERIFY(!result.ok);
    QVERIFY(result.validation.issueCodes.contains(
        QStringLiteral("flow_model_reference_missing")
    ));
    QCOMPARE(function.flow.published.graphHash, oldHash);
    QCOMPARE(harness.saveCalls, 0);

    function.flow.draft.supported = false;
    function.flow.draft.unavailableCode =
        QStringLiteral("flow_schema_newer");
    result = harness.service.publish(QStringLiteral("custom_1"), 3);
    QVERIFY(!result.ok);
    QCOMPARE(result.error.code, QStringLiteral("flow_schema_newer"));
    QCOMPARE(harness.saveCalls, 0);

    function.flow.draft = VersionedFunctionFlowGraph();
    function.flow.draft.revision = 3;
    function.flow.draft.graph = validGraph();
    function.flow.draft.graphHash =
        functionFlowGraphHash(function.flow.draft.graph);
    function.flow.published.supported = false;
    function.flow.published.unavailableCode =
        QStringLiteral("flow_node_type_unsupported");
    function.flow.published.retainedRaw.insert(
        QStringLiteral("future"),
        42
    );
    result = harness.service.publish(
        QStringLiteral("custom_1"),
        3,
        true
    );
    QVERIFY(!result.ok);
    QCOMPARE(
        result.error.code,
        QStringLiteral("flow_node_type_unsupported")
    );
    QCOMPARE(
        function.flow.published.retainedRaw
            .value(QStringLiteral("future")).toInt(),
        42
    );
}

void FunctionFlowPublicationServiceTests::
repairsOnlyConfirmedCorruptPublishedState()
{
    ServiceHarness harness;
    FunctionSettings &function =
        harness.function(QStringLiteral("custom_1"));
    function.flow.published.supported = false;
    function.flow.published.unavailableCode =
        QStringLiteral("flow_published_hash_mismatch");
    function.flow.published.retainedRaw.insert(
        QStringLiteral("corrupt"),
        true
    );

    FunctionFlowPublishResult result = harness.service.publish(
        QStringLiteral("custom_1"),
        3
    );
    QVERIFY(!result.ok);
    QCOMPARE(
        result.error.code,
        QStringLiteral(
            "flow_published_repair_confirmation_required"
        )
    );
    QCOMPARE(harness.saveCalls, 0);

    result = harness.service.publish(
        QStringLiteral("custom_1"),
        3,
        true
    );
    QVERIFY(result.ok);
    QCOMPARE(result.publishedRevision, 1);
    const FunctionFlowState repaired =
        harness.function(QStringLiteral("custom_1")).flow;
    QVERIFY(repaired.published.supported);
    QVERIFY(!repaired.enabled);
    QCOMPARE(
        harness.function(QStringLiteral("custom_1")).executionMode,
        FunctionExecutionMode::Classic
    );
    QVERIFY(repaired.published.retainedRaw.isEmpty());
}

void FunctionFlowPublicationServiceTests::
requiresValidPublishedGraphForCanvasMode()
{
    ServiceHarness harness;
    OperationError error;
    QVERIFY(!harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("flow_published_unavailable"));
    QCOMPARE(harness.saveCalls, 0);
    QVERIFY(harness.events.isEmpty());

    FunctionSettings &function =
        harness.function(QStringLiteral("custom_1"));
    function.flow.published =
        publishedVersion(validGraph(), 2, 3);
    function.flow.published.graphHash = QStringLiteral("wrong-hash");
    QVERIFY(!harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        &error
    ));
    QCOMPARE(
        error.code,
        QStringLiteral("flow_published_hash_mismatch")
    );
    QCOMPARE(function.executionMode, FunctionExecutionMode::Classic);
    QCOMPARE(harness.saveCalls, 0);

    function.flow.published =
        publishedVersion(validGraph(), 2, 3);
    harness.context.references.modelIds.clear();
    QVERIFY(!harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        &error
    ));
    QVERIFY(error.code == QStringLiteral("flow_model_reference_missing"));
    QCOMPARE(function.executionMode, FunctionExecutionMode::Classic);
    QVERIFY(!function.flow.enabled);
    QCOMPARE(harness.saveCalls, 0);
    QVERIFY(harness.events.isEmpty());

    harness.context.references.modelIds << QStringLiteral("model");
    function.flow.retainedValues.insert(
        QStringLiteral("executionMode"),
        QStringLiteral("future-mode")
    );
    QVERIFY(harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        &error
    ));
    const FunctionSettings &savedCanvas =
        harness.function(QStringLiteral("custom_1"));
    QCOMPARE(
        savedCanvas.executionMode,
        FunctionExecutionMode::Canvas
    );
    QVERIFY(savedCanvas.flow.enabled);
    QVERIFY(!savedCanvas.flow.retainedValues.contains(
        QStringLiteral("executionMode")
    ));
    QCOMPARE(harness.saveCalls, 1);
    QCOMPARE(harness.events.size(), 1);
    QCOMPARE(
        harness.events.last().key,
        functionExecutionModeSettingsKey()
    );
    QCOMPARE(
        harness.events.last().functionId,
        QStringLiteral("custom_1")
    );
}

void FunctionFlowPublicationServiceTests::
preservesSpecificCanvasModeFailureReasons()
{
    ServiceHarness harness;
    FunctionSettings &function =
        harness.function(QStringLiteral("custom_1"));
    function.flow.published =
        publishedVersion(validGraph(), 2, 3);
    function.flow.published.supported = false;
    function.flow.published.unavailableCode =
        QStringLiteral("flow_schema_newer");
    OperationError error;

    QVERIFY(!harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("flow_schema_newer"));
    QVERIFY(error.message.contains(QString::fromUtf8("更高版本")));

    function.flow.published.unavailableCode =
        QStringLiteral("flow_node_type_unsupported");
    QVERIFY(!harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        &error
    ));
    QCOMPARE(
        error.code,
        QStringLiteral("flow_node_type_unsupported")
    );
    QVERIFY(error.message.contains(QString::fromUtf8("不支持的节点")));

    function.flow.published =
        publishedVersion(validGraph(), 2, 3);
    harness.context.references.modelIds.clear();
    const FunctionFlowValidationResult validation =
        FunctionFlowValidator::validateForPublish(
            function.flow.published.graph,
            harness.context
        );
    QVERIFY(!validation.ok);
    QVERIFY(!validation.issues.isEmpty());
    QVERIFY(!validation.issues.first().message.isEmpty());
    QVERIFY(!harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        &error
    ));
    QCOMPARE(
        error.message,
        validation.issues.first().message
    );
    QCOMPARE(harness.saveCalls, 0);
    QVERIFY(harness.events.isEmpty());
}

void FunctionFlowPublicationServiceTests::
rejectsCanvasModeWhenPublishedCompilationFails()
{
    ServiceHarness harness;
    FunctionSettings &function =
        harness.function(QStringLiteral("custom_1"));
    function.flow.published =
        publishedVersion(validGraph(), 2, 3);
    harness.compileSucceeds = false;
    OperationError error;

    QVERIFY(!harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("test_compile_failed"));
    QCOMPARE(function.executionMode, FunctionExecutionMode::Classic);
    QVERIFY(!function.flow.enabled);
    QCOMPARE(harness.saveCalls, 0);
    QVERIFY(harness.events.isEmpty());
}

void FunctionFlowPublicationServiceTests::
alwaysAllowsClassicAndPreservesFlowData()
{
    ServiceHarness harness;
    FunctionSettings &function =
        harness.function(QStringLiteral("custom_1"));
    function.executionMode = FunctionExecutionMode::Canvas;
    function.flow.enabled = true;
    function.flow.published =
        publishedVersion(validGraph(), 2, 3);
    function.flow.editor.viewportCenter = QPointF(42.0, -9.0);
    function.flow.editor.zoom = 1.75;
    function.flow.draft.supported = false;
    function.flow.draft.unavailableCode =
        QStringLiteral("flow_json_invalid");

    const VersionedFunctionFlowGraph draftBefore = function.flow.draft;
    const VersionedFunctionFlowGraph publishedBefore =
        function.flow.published;
    const FunctionFlowEditorState editorBefore = function.flow.editor;
    OperationError error;
    QVERIFY(harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Classic,
        &error
    ));
    const FunctionSettings &savedClassic =
        harness.function(QStringLiteral("custom_1"));
    QCOMPARE(
        savedClassic.executionMode,
        FunctionExecutionMode::Classic
    );
    QVERIFY(!savedClassic.flow.enabled);
    QCOMPARE(
        savedClassic.flow.draft.revision,
        draftBefore.revision
    );
    QCOMPARE(
        savedClassic.flow.draft.unavailableCode,
        draftBefore.unavailableCode
    );
    QCOMPARE(
        savedClassic.flow.draft.graphHash,
        draftBefore.graphHash
    );
    QCOMPARE(
        savedClassic.flow.published.revision,
        publishedBefore.revision
    );
    QCOMPARE(
        savedClassic.flow.published.graphHash,
        publishedBefore.graphHash
    );
    QCOMPARE(
        savedClassic.flow.editor.viewportCenter,
        editorBefore.viewportCenter
    );
    QCOMPARE(savedClassic.flow.editor.zoom, editorBefore.zoom);
    QCOMPARE(harness.saveCalls, 1);
    QCOMPARE(harness.events.size(), 1);
    QCOMPARE(
        harness.events.last().key,
        functionExecutionModeSettingsKey()
    );

    harness.clearActivity();
    QVERIFY(harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Classic,
        &error
    ));
    QCOMPARE(harness.saveCalls, 0);
    QVERIFY(harness.events.isEmpty());
}

void FunctionFlowPublicationServiceTests::
sameModeIsIdempotentDespiteStaleLegacyEnabledMirror()
{
    ServiceHarness classicHarness;
    FunctionSettings &classic =
        classicHarness.function(QStringLiteral("custom_1"));
    classic.executionMode = FunctionExecutionMode::Classic;
    classic.flow.enabled = true;
    OperationError error;

    QVERIFY(classicHarness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Classic,
        &error
    ));
    QCOMPARE(classicHarness.saveCalls, 0);
    QVERIFY(classicHarness.events.isEmpty());
    QVERIFY(classic.flow.enabled);

    ServiceHarness canvasHarness;
    FunctionSettings &canvas =
        canvasHarness.function(QStringLiteral("custom_1"));
    canvas.executionMode = FunctionExecutionMode::Canvas;
    canvas.flow.enabled = false;
    canvas.flow.published =
        publishedVersion(validGraph(), 2, 3);

    QVERIFY(canvasHarness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        &error
    ));
    QCOMPARE(canvasHarness.saveCalls, 0);
    QVERIFY(canvasHarness.events.isEmpty());
    QVERIFY(!canvas.flow.enabled);
}

void FunctionFlowPublicationServiceTests::
sameClassicModeClearsRetainedUnknownExecutionMode()
{
    ServiceHarness harness;
    FunctionSettings &function =
        harness.function(QStringLiteral("custom_1"));
    function.executionMode = FunctionExecutionMode::Classic;
    function.flow.retainedValues.insert(
        QStringLiteral("executionMode"),
        QStringLiteral("future-mode")
    );
    OperationError error;

    QVERIFY(harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Classic,
        &error
    ));
    const FunctionSettings &saved =
        harness.function(QStringLiteral("custom_1"));
    QCOMPARE(saved.executionMode, FunctionExecutionMode::Classic);
    QVERIFY(!saved.flow.enabled);
    QVERIFY(!saved.flow.retainedValues.contains(
        QStringLiteral("executionMode")
    ));
    QCOMPARE(harness.saveCalls, 1);
    QCOMPARE(harness.events.size(), 1);
    QCOMPARE(
        harness.events.first().key,
        functionExecutionModeSettingsKey()
    );
    QCOMPARE(
        harness.events.first().functionId,
        QStringLiteral("custom_1")
    );
}

void FunctionFlowPublicationServiceTests::
sameCanvasModeClearsRetainedUnknownExecutionMode()
{
    ServiceHarness harness;
    FunctionSettings &function =
        harness.function(QStringLiteral("custom_1"));
    function.executionMode = FunctionExecutionMode::Canvas;
    function.flow.enabled = false;
    function.flow.published =
        publishedVersion(validGraph(), 2, 3);
    function.flow.retainedValues.insert(
        QStringLiteral("executionMode"),
        QStringLiteral("future-mode")
    );
    OperationError error;

    QVERIFY(harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        &error
    ));
    const FunctionSettings &saved =
        harness.function(QStringLiteral("custom_1"));
    QCOMPARE(saved.executionMode, FunctionExecutionMode::Canvas);
    QVERIFY(saved.flow.enabled);
    QVERIFY(!saved.flow.retainedValues.contains(
        QStringLiteral("executionMode")
    ));
    QCOMPARE(harness.saveCalls, 1);
    QCOMPARE(harness.events.size(), 1);
    QCOMPARE(
        harness.events.first().key,
        functionExecutionModeSettingsKey()
    );
    QCOMPARE(
        harness.events.first().functionId,
        QStringLiteral("custom_1")
    );
}

void FunctionFlowPublicationServiceTests::
emitsModeEventsOnlyAfterSuccessfulSave()
{
    ServiceHarness harness;
    FunctionSettings &function =
        harness.function(QStringLiteral("custom_1"));
    function.flow.published =
        publishedVersion(validGraph(), 2, 3);
    harness.saveSucceeds = false;
    OperationError error;

    QVERIFY(!harness.service.setExecutionMode(
        QStringLiteral("custom_1"),
        FunctionExecutionMode::Canvas,
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("test_save_failed"));
    QCOMPARE(function.executionMode, FunctionExecutionMode::Classic);
    QVERIFY(!function.flow.enabled);
    QCOMPARE(harness.saveCalls, 1);
    QVERIFY(harness.events.isEmpty());
}

void FunctionFlowPublicationServiceTests::
rejectsPublicationRevisionOverflow()
{
    ServiceHarness harness;
    FunctionSettings &function =
        harness.function(QStringLiteral("custom_1"));
    function.flow.published =
        publishedVersion(validGraph(), INT_MAX, 3);
    const QString publishedHash = function.flow.published.graphHash;
    function.flow.draft.graph.nodes[0]
        .config.voice.recording.countdownSeconds = 5;
    function.flow.draft.graphHash =
        functionFlowGraphHash(function.flow.draft.graph);

    const FunctionFlowPublishResult result =
        harness.service.publish(QStringLiteral("custom_1"), 3);
    QVERIFY(!result.ok);
    QCOMPARE(
        result.error.code,
        QStringLiteral("flow_published_revision_invalid")
    );
    QCOMPARE(function.flow.published.revision, INT_MAX);
    QCOMPARE(function.flow.published.graphHash, publishedHash);
    QCOMPARE(harness.saveCalls, 0);
    QVERIFY(harness.events.isEmpty());
}

void FunctionFlowPublicationServiceTests::
createsAndRemovesOnlyCustomFunctions()
{
    ServiceHarness harness;
    FunctionSettings candidate =
        functionSettings(QStringLiteral("custom_2"));
    candidate.name = QStringLiteral("Second");
    candidate.executionMode = FunctionExecutionMode::Canvas;
    candidate.flow.enabled = true;
    candidate.flow.published =
        publishedVersion(validGraph(), 9, 3);
    OperationError error;

    QVERIFY(harness.service.addCustomFunction(candidate, &error));
    const int createdIndex =
        harness.settings.functionIndex(QStringLiteral("custom_2"));
    QVERIFY(createdIndex >= 0);
    const FunctionFlowState created =
        harness.settings.functions.at(createdIndex).flow;
    QCOMPARE(
        harness.settings.functions.at(createdIndex).executionMode,
        FunctionExecutionMode::Classic
    );
    QVERIFY(!created.enabled);
    QCOMPARE(created.draft.revision, 0);
    QCOMPARE(created.published.revision, 0);
    QVERIFY(created.draft.graph.nodes.isEmpty());
    QVERIFY(created.published.graph.nodes.isEmpty());
    QVERIFY(harness.settings.functionOrder.contains(
        QStringLiteral("custom_2")
    ));
    QCOMPARE(harness.events.last().key, functionDefinitionsSettingsKey());

    harness.settings.retainedOrphanFunctionFlows.insert(
        QStringLiteral("custom_2"),
        QJsonObject{{QStringLiteral("orphan"), true}}
    );
    QVERIFY(harness.service.removeCustomFunction(
        QStringLiteral("custom_2"),
        &error
    ));
    QCOMPARE(
        harness.settings.functionIndex(QStringLiteral("custom_2")),
        -1
    );
    QVERIFY(!harness.settings.functionOrder.contains(
        QStringLiteral("custom_2")
    ));
    QVERIFY(!harness.settings.retainedOrphanFunctionFlows.contains(
        QStringLiteral("custom_2")
    ));

    harness.clearActivity();
    candidate.id = QStringLiteral("custom_1");
    QVERIFY(!harness.service.addCustomFunction(candidate, &error));
    candidate.id = QStringLiteral("orphan");
    harness.settings.retainedOrphanFunctionFlows.insert(
        candidate.id,
        QJsonObject{{QStringLiteral("future"), 1}}
    );
    QVERIFY(!harness.service.addCustomFunction(candidate, &error));
    candidate.id = QStringLiteral("builtin_candidate");
    candidate.builtIn = true;
    QVERIFY(!harness.service.addCustomFunction(candidate, &error));

    harness.function(QStringLiteral("custom_1")).builtIn = true;
    QVERIFY(!harness.service.removeCustomFunction(
        QStringLiteral("custom_1"),
        &error
    ));
    QCOMPARE(harness.saveCalls, 0);
    QVERIFY(harness.events.isEmpty());
}

void FunctionFlowPublicationServiceTests::
emitsEventsOnlyAfterSuccessfulSave()
{
    ServiceHarness harness;
    harness.saveSucceeds = false;
    const FunctionFlowState before =
        harness.function(QStringLiteral("custom_1")).flow;
    FunctionFlowGraph graph = validGraph();
    graph.nodes[0].config.voice.recording.countdownSeconds = 9;
    int savedRevision = 0;
    OperationError error;

    QVERIFY(!harness.service.updateDraft(
        QStringLiteral("custom_1"),
        3,
        graph,
        &savedRevision,
        &error
    ));
    QCOMPARE(error.code, QStringLiteral("test_save_failed"));
    QCOMPARE(
        harness.function(QStringLiteral("custom_1"))
            .flow.draft.revision,
        before.draft.revision
    );
    QCOMPARE(
        harness.function(QStringLiteral("custom_1"))
            .flow.draft.graphHash,
        before.draft.graphHash
    );
    QVERIFY(harness.events.isEmpty());
}

QTEST_MAIN(FunctionFlowPublicationServiceTests)

#include "function_flow_publication_service_tests.moc"
