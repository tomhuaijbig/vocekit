#include <QtTest>

#include "../../src/controllers/function_flow_execution_controller.h"
#include "../../src/domain/function_flow_compiler.h"

#include <QSignalSpy>
#include <QTimer>

#include <thread>

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

FunctionFlowGraph executionGraph()
{
    FunctionFlowGraph graph;

    FunctionFlowNode voice =
        node(QStringLiteral("voice"), FunctionFlowNodeType::VoiceSource);
    voice.config.voice.speechProviderId = QStringLiteral("speech");
    voice.config.voice.acquisitionSequence = 0;
    FunctionFlowNode voiceInput = node(
        QStringLiteral("input_instruction"),
        FunctionFlowNodeType::Input
    );
    voiceInput.config.input.role = QStringLiteral("instruction");
    voiceInput.config.input.sequence = 0;

    FunctionFlowNode selection = node(
        QStringLiteral("selection"),
        FunctionFlowNodeType::SelectionSource
    );
    selection.config.selection.acquisitionSequence = 1;
    FunctionFlowNode selectionInput = node(
        QStringLiteral("input_source"),
        FunctionFlowNodeType::Input
    );
    selectionInput.config.input.role = QStringLiteral("source");
    selectionInput.config.input.sequence = 1;

    FunctionFlowNode model =
        node(QStringLiteral("model"), FunctionFlowNodeType::Model);
    model.config.model.modelId = QStringLiteral("model");
    model.config.model.promptId = QStringLiteral("prompt");

    graph.nodes
        << voice
        << voiceInput
        << selection
        << selectionInput
        << model
        << node(QStringLiteral("output"), FunctionFlowNodeType::Output)
        << node(
            QStringLiteral("popup"),
            FunctionFlowNodeType::ResultPopup
        )
        << node(
            QStringLiteral("auto_write"),
            FunctionFlowNodeType::AutoWrite
        );
    graph.edges
        << edge(
            QStringLiteral("voice-input"),
            QStringLiteral("voice"),
            QStringLiteral("text_out"),
            QStringLiteral("input_instruction"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("voice-model"),
            QStringLiteral("input_instruction"),
            QStringLiteral("text_out"),
            QStringLiteral("model"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("selection-input"),
            QStringLiteral("selection"),
            QStringLiteral("text_out"),
            QStringLiteral("input_source"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("selection-model"),
            QStringLiteral("input_source"),
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
            QStringLiteral("action_in"),
            1
        )
        << edge(
            QStringLiteral("output-write"),
            QStringLiteral("output"),
            QStringLiteral("action_out"),
            QStringLiteral("auto_write"),
            QStringLiteral("action_in"),
            2
        );
    return graph;
}

FunctionFlowExecutionPlan executionPlan()
{
    const FunctionFlowGraph graph = executionGraph();
    const QString hash = functionFlowGraphHash(graph);
    const FunctionFlowCompileResult result =
        FunctionFlowCompiler::compile(graph, 8, hash);
    Q_ASSERT(result.ok);
    return result.plan;
}

FunctionFlowExecutionPlan screenshotPlan()
{
    FunctionFlowGraph graph;
    FunctionFlowNode screenshot = node(
        QStringLiteral("screenshot"),
        FunctionFlowNodeType::ScreenshotSource
    );
    screenshot.config.screenshot.triggerMode =
        QStringLiteral("separate");
    screenshot.config.screenshot.ocrEngineId =
        QStringLiteral("windows");
    screenshot.config.screenshot.separateShortcut =
        QStringLiteral("Ctrl+Alt+S");
    FunctionFlowNode input =
        node(QStringLiteral("input"), FunctionFlowNodeType::Input);
    input.config.input.role = QStringLiteral("screenshot");
    FunctionFlowNode model =
        node(QStringLiteral("model"), FunctionFlowNodeType::Model);
    model.config.model.modelId = QStringLiteral("model");
    model.config.model.promptId = QStringLiteral("prompt");
    graph.nodes
        << screenshot
        << input
        << model
        << node(QStringLiteral("output"), FunctionFlowNodeType::Output)
        << node(
            QStringLiteral("popup"),
            FunctionFlowNodeType::ResultPopup
        );
    graph.edges
        << edge(
            QStringLiteral("screenshot-input"),
            QStringLiteral("screenshot"),
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
    const QString hash = functionFlowGraphHash(graph);
    const FunctionFlowCompileResult result =
        FunctionFlowCompiler::compile(graph, 3, hash);
    Q_ASSERT(result.ok);
    return result.plan;
}

FunctionFlowNodeResult success(const QString &text = QString())
{
    FunctionFlowNodeResult result;
    result.state = FunctionFlowNodeState::Succeeded;
    if (!text.isNull()) {
        FunctionFlowValue value;
        value.text = text;
        result.values << value;
    }
    return result;
}

FunctionFlowNodeResult failure(const QString &code)
{
    FunctionFlowNodeResult result;
    result.state = FunctionFlowNodeState::Failed;
    result.error.code = code;
    return result;
}

class RuntimeHarness
{
public:
    QStringList calls;
    QVector<FunctionFlowNodeCompletion> deferredVoice;
    QVector<FunctionFlowRunContext> contexts;
    QVector<FunctionFlowHistoryRequest> historyRequests;
    QVector<FunctionFlowHistoryEditRequest> editRequests;
    int modelCalls = 0;
    int actionCalls = 0;
    int resolveCalls = 0;
    bool resolveOk = true;
    OperationError resolveError;
    bool deferVoice = false;
    bool completeVoiceTwice = false;
    bool historyOk = true;
    QSharedPointer<FunctionFlowResolvedDependencies> dependencies;
    FunctionFlowNodeResult voiceResult =
        success(QStringLiteral("请翻译"));
    FunctionFlowNodeResult selectionResult =
        success(QStringLiteral("Hello"));
    FunctionFlowNodeResult modelResult =
        success(QStringLiteral("你好"));

    RuntimeHarness()
        : dependencies(new FunctionFlowResolvedDependencies)
    {
        dependencies->functionTitle = QStringLiteral("Frozen title");
        dependencies->recordDirectory = QStringLiteral("C:/frozen");
        dependencies->inheritedResultPopupOpacity = 83;
        FunctionFlowResolvedNodeSettings model;
        model.modelId = QStringLiteral("frozen-model");
        model.systemPrompt = QStringLiteral("frozen-prompt");
        model.promptVersion = QStringLiteral("prompt-v1");
        dependencies->byNodeId.insert(QStringLiteral("model"), model);
    }

    FunctionFlowRuntimeAccess access()
    {
        FunctionFlowRuntimeAccess access;
        access.resolveDependencies = [this](
            const FunctionFlowExecutionPlan &,
            FunctionFlowTrigger,
            FunctionFlowTargetWindowHandle,
            QSharedPointer<
                const FunctionFlowResolvedDependencies
            > *resolved,
            OperationError *error) {
            ++resolveCalls;
            calls << QStringLiteral("resolve");
            if (!resolveOk) {
                if (error) {
                    *error = resolveError;
                }
                return false;
            }
            *resolved = dependencies;
            return true;
        };
        access.collectVoice = [this](
            const FunctionFlowRunContext &context,
            const FunctionFlowCompiledNode &,
            const FunctionFlowNodeCompletion &completion) {
            calls << QStringLiteral("voice");
            contexts << context;
            if (deferVoice) {
                deferredVoice << completion;
                return;
            }
            completion(voiceResult);
            if (completeVoiceTwice) {
                completion(success(QStringLiteral("duplicate")));
            }
        };
        access.collectSelection = [this](
            const FunctionFlowRunContext &context,
            const FunctionFlowCompiledNode &,
            const FunctionFlowNodeCompletion &completion) {
            calls << QStringLiteral("selection");
            contexts << context;
            completion(selectionResult);
        };
        access.collectScreenshot = [this](
            const FunctionFlowRunContext &context,
            const FunctionFlowCompiledNode &,
            const FunctionFlowNodeCompletion &completion) {
            calls << QStringLiteral("screenshot");
            contexts << context;
            completion(success(QStringLiteral("screen")));
        };
        access.runModel = [this](
            const FunctionFlowRunContext &context,
            const FunctionFlowCompiledNode &,
            const QList<FunctionFlowValue> &,
            const FunctionFlowNodeCompletion &completion) {
            ++modelCalls;
            calls << QStringLiteral("model");
            contexts << context;
            completion(modelResult);
        };
        access.runResultAction = [this](
            const FunctionFlowRunContext &context,
            const FunctionFlowCompiledNode &node,
            const FunctionFlowResultActionRequest &request,
            const FunctionFlowNodeCompletion &completion) {
            ++actionCalls;
            calls << node.nodeId;
            contexts << context;
            QCOMPARE(request.output.text, QStringLiteral("你好"));
            QVERIFY(request.canonicalInput.contains(
                QStringLiteral("请翻译")
            ));
            QVERIFY(request.canonicalInput.contains(
                QStringLiteral("Hello")
            ));
            QVERIFY(request.collectedSelection);
            completion(success(QString()));
        };
        access.saveHistory = [this](
            const FunctionFlowHistoryRequest &request) {
            calls << QStringLiteral("save_history");
            historyRequests << request;
            FunctionFlowHistorySaveResult result;
            result.ok = historyOk;
            if (historyOk) {
                result.detailPath =
                    QStringLiteral("C:/frozen/run.json");
            } else {
                result.error.code =
                    QStringLiteral("storage_failed");
            }
            return result;
        };
        access.updateHistoryEditedText = [this](
            const FunctionFlowHistoryEditRequest &request) {
            editRequests << request;
            FunctionFlowHistoryEditResult result;
            result.ok = true;
            return result;
        };
        return access;
    }
};

QVector<FunctionFlowNodeExecutionEvent> nodeEvents(
    const QSignalSpy &spy)
{
    QVector<FunctionFlowNodeExecutionEvent> result;
    for (const QList<QVariant> &arguments : spy) {
        result << qvariant_cast<FunctionFlowNodeExecutionEvent>(
            arguments.at(0)
        );
    }
    return result;
}

QVector<FunctionFlowRunExecutionEvent> runEvents(
    const QSignalSpy &spy)
{
    QVector<FunctionFlowRunExecutionEvent> result;
    for (const QList<QVariant> &arguments : spy) {
        result << qvariant_cast<FunctionFlowRunExecutionEvent>(
            arguments.at(0)
        );
    }
    return result;
}

} // namespace

class FunctionFlowExecutionControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void freezesPlanDependenciesTargetAndCancellationIdentity();
    void missingTriggerIsAConfigurationError();
    void mapsDependencyResolutionStartFailures();
    void handlesSynchronousCompletionWithoutRecursiveDispatch();
    void acceptsOneAsynchronousCrossThreadCompletion();
    void cancelsWithBoundedGraceAndIgnoresLateSuccess();
    void rejectsAnOldRunCompletionDuringANewRun();
    void keepsFailedValuesOutOfTheGraphButSavesObservations();
    void savesFrozenVoiceMetadataExactlyOnce();
    void savesSafeScreenshotMetadataAndCancelledProvenance();
    void reportsHistoryFailureWithoutRepeatingActions();
    void coordinatesRetriggerBeforeBusyAndMissingPlanFallback();
    void missingSharedPlanIsAConfigurationError();
    void mergesEditsBeforeFinalizationAndUpdatesOldRunsAfterward();
    void onlyCancelsTheMatchingRunFromAResultSurface();
};

void FunctionFlowExecutionControllerTests::
freezesPlanDependenciesTargetAndCancellationIdentity()
{
    RuntimeHarness harness;
    harness.deferVoice = true;
    FunctionFlowRuntimeAccess access = harness.access();
    FunctionFlowExecutionController controller(access);
    QSignalSpy nodeSpy(
        &controller,
        &FunctionFlowExecutionController::nodeExecutionChanged
    );
    QSignalSpy runSpy(
        &controller,
        &FunctionFlowExecutionController::runExecutionChanged
    );
    FunctionFlowExecutionPlan plan = executionPlan();
    const QString originalHash = plan.publishedHash;
    FunctionFlowTargetWindowHandle target =
        reinterpret_cast<FunctionFlowTargetWindowHandle>(
            quintptr(0x1234)
        );

    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            plan,
            FunctionFlowTrigger::MainHotkey,
            target
        ),
        FunctionFlowStartOutcome::Started
    );
    plan.publishedRevision = 99;
    plan.publishedHash = QString(64, QLatin1Char('f'));
    harness.dependencies->functionTitle = QStringLiteral("Changed");
    harness.dependencies->recordDirectory = QStringLiteral("C:/changed");
    harness.dependencies->byNodeId[
        QStringLiteral("model")
    ].systemPrompt = QStringLiteral("changed-prompt");

    QTRY_COMPARE(harness.deferredVoice.size(), 1);
    QCOMPARE(harness.calls.first(), QStringLiteral("resolve"));
    QCOMPARE(harness.resolveCalls, 1);
    QCOMPARE(harness.contexts.first().targetWindow, target);
    QCOMPARE(harness.contexts.first().publishedRevision, 8);
    QCOMPARE(harness.contexts.first().publishedHash, originalHash);
    QCOMPARE(
        harness.contexts.first().runId,
        harness.contexts.first().cancellation.executionId()
    );
    QCOMPARE(
        harness.contexts.first().dependencies->functionTitle,
        QStringLiteral("Frozen title")
    );
    QCOMPARE(
        harness.contexts.first().dependencies->recordDirectory,
        QStringLiteral("C:/frozen")
    );

    harness.deferredVoice.first()(harness.voiceResult);
    QTRY_VERIFY(!controller.isRunning());
    QCOMPARE(harness.historyRequests.size(), 1);
    QCOMPARE(
        harness.historyRequests.first().publishedRevision,
        8
    );
    QCOMPARE(
        harness.historyRequests.first().publishedHash,
        originalHash
    );
    QCOMPARE(
        harness.historyRequests.first().recordDirectory,
        QStringLiteral("C:/frozen")
    );
    QVERIFY(nodeSpy.count() > 0);
    QCOMPARE(runSpy.count(), 2);

    const QVector<FunctionFlowNodeExecutionEvent> nodes =
        nodeEvents(nodeSpy);
    bool sawModelMetadata = false;
    for (const FunctionFlowNodeExecutionEvent &event : nodes) {
        QCOMPARE(event.runId, harness.contexts.first().runId);
        QCOMPARE(event.functionId, QStringLiteral("custom_1"));
        QCOMPARE(event.publishedRevision, 8);
        QCOMPARE(event.publishedHash, originalHash);
        QCOMPARE(
            event.trigger,
            FunctionFlowTrigger::MainHotkey
        );
        QVERIFY(!event.nodeId.isEmpty());
        if (event.nodeId == QStringLiteral("model")
            && event.state
                == FunctionFlowNodeState::Succeeded) {
            QCOMPARE(
                event.modelId,
                QStringLiteral("frozen-model")
            );
            QCOMPARE(
                event.promptVersion,
                QStringLiteral("prompt-v1")
            );
            sawModelMetadata = true;
        }
    }
    QVERIFY(sawModelMetadata);
}

void FunctionFlowExecutionControllerTests::
missingTriggerIsAConfigurationError()
{
    RuntimeHarness harness;
    FunctionFlowExecutionPlan unavailable = executionPlan();
    unavailable.triggers[
        FunctionFlowTrigger::MainHotkey
    ].available = false;
    FunctionFlowExecutionController controller(harness.access());

    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            unavailable,
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::ConfigurationError
    );
    QCOMPARE(harness.resolveCalls, 0);
    QCOMPARE(
        controller.lastStartError().code,
        QStringLiteral("flow_trigger_not_configured")
    );
    QCOMPARE(
        controller.lastStartError().message,
        QString::fromUtf8("当前画布未配置此入口。")
    );
    QCOMPARE(harness.historyRequests.size(), 0);
}

void FunctionFlowExecutionControllerTests::
mapsDependencyResolutionStartFailures()
{
    RuntimeHarness harness;
    FunctionFlowExecutionController controller(harness.access());
    harness.resolveOk = false;
    harness.resolveError.code =
        QStringLiteral("flow_target_window_unavailable");
    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            executionPlan(),
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::TargetUnavailable
    );
    QCOMPARE(
        controller.lastStartError().code,
        QStringLiteral("flow_target_window_unavailable")
    );
    QVERIFY(harness.calls.isEmpty()
        || !harness.calls.contains(QStringLiteral("voice")));

    harness.resolveError.code =
        QStringLiteral("flow_model_reference_missing");
    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            executionPlan(),
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::ConfigurationError
    );
    QCOMPARE(
        controller.lastStartError().code,
        QStringLiteral("flow_model_reference_missing")
    );
    QVERIFY(!controller.isRunning());
}

void FunctionFlowExecutionControllerTests::
handlesSynchronousCompletionWithoutRecursiveDispatch()
{
    RuntimeHarness harness;
    harness.completeVoiceTwice = true;
    FunctionFlowExecutionController controller(harness.access());
    QSignalSpy runSpy(
        &controller,
        &FunctionFlowExecutionController::runExecutionChanged
    );

    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            executionPlan(),
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::Started
    );
    QCOMPARE(harness.calls, QStringList() << QStringLiteral("resolve"));

    QTRY_VERIFY(!controller.isRunning());
    QCOMPARE(
        harness.calls,
        QStringList()
            << QStringLiteral("resolve")
            << QStringLiteral("voice")
            << QStringLiteral("selection")
            << QStringLiteral("model")
            << QStringLiteral("popup")
            << QStringLiteral("auto_write")
            << QStringLiteral("save_history")
    );
    QCOMPARE(harness.modelCalls, 1);
    QCOMPARE(harness.actionCalls, 2);
    QCOMPARE(harness.historyRequests.size(), 1);
    const QVector<FunctionFlowRunExecutionEvent> runs =
        runEvents(runSpy);
    QCOMPARE(runs.size(), 2);
    QVERIFY(runs.first().running);
    QVERIFY(!runs.last().running);
    QVERIFY(runs.last().terminalError.isEmpty());
}

void FunctionFlowExecutionControllerTests::
acceptsOneAsynchronousCrossThreadCompletion()
{
    RuntimeHarness harness;
    harness.deferVoice = true;
    FunctionFlowExecutionController controller(harness.access());
    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            executionPlan(),
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::Started
    );
    QTRY_COMPARE(harness.deferredVoice.size(), 1);

    const FunctionFlowNodeCompletion completion =
        harness.deferredVoice.first();
    const FunctionFlowNodeResult result = harness.voiceResult;
    std::thread worker([completion, result]() {
        completion(result);
        completion(success(QStringLiteral("late duplicate")));
    });
    worker.join();

    QTRY_VERIFY(!controller.isRunning());
    QCOMPARE(harness.modelCalls, 1);
    QCOMPARE(harness.historyRequests.size(), 1);
}

void FunctionFlowExecutionControllerTests::
cancelsWithBoundedGraceAndIgnoresLateSuccess()
{
    RuntimeHarness harness;
    harness.deferVoice = true;
    FunctionFlowExecutionOptions options;
    options.cancellationGraceMs = 1;
    FunctionFlowExecutionController controller(
        harness.access(),
        options
    );
    QSignalSpy runSpy(
        &controller,
        &FunctionFlowExecutionController::runExecutionChanged
    );
    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            executionPlan(),
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::Started
    );
    QTRY_COMPARE(harness.deferredVoice.size(), 1);

    controller.cancel();
    QTRY_VERIFY(!controller.isRunning());
    QCOMPARE(harness.modelCalls, 0);
    QCOMPARE(harness.actionCalls, 0);
    QCOMPARE(harness.historyRequests.size(), 0);
    const int eventCount = runSpy.count();
    const QVector<FunctionFlowRunExecutionEvent> beforeLate =
        runEvents(runSpy);
    QVERIFY(beforeLate.last().cancelled);
    QCOMPARE(
        beforeLate.last().terminalError.code,
        QStringLiteral("flow_cancelled")
    );

    harness.deferredVoice.first()(harness.voiceResult);
    QTest::qWait(5);
    QCOMPARE(runSpy.count(), eventCount);
    QCOMPARE(harness.historyRequests.size(), 0);
}

void FunctionFlowExecutionControllerTests::
rejectsAnOldRunCompletionDuringANewRun()
{
    RuntimeHarness harness;
    harness.deferVoice = true;
    FunctionFlowExecutionOptions options;
    options.cancellationGraceMs = 1;
    FunctionFlowExecutionController controller(
        harness.access(),
        options
    );
    const FunctionFlowExecutionPlan plan = executionPlan();

    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            plan,
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::Started
    );
    QTRY_COMPARE(harness.deferredVoice.size(), 1);
    controller.cancel();
    QTRY_VERIFY(!controller.isRunning());

    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            plan,
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::Started
    );
    QTRY_COMPARE(harness.deferredVoice.size(), 2);
    harness.deferredVoice.at(0)(harness.voiceResult);
    QTest::qWait(5);
    QVERIFY(controller.isRunning());
    QCOMPARE(harness.modelCalls, 0);

    harness.deferredVoice.at(1)(harness.voiceResult);
    QTRY_VERIFY(!controller.isRunning());
    QCOMPARE(harness.modelCalls, 1);
    QCOMPARE(harness.historyRequests.size(), 1);
}

void FunctionFlowExecutionControllerTests::
keepsFailedValuesOutOfTheGraphButSavesObservations()
{
    RuntimeHarness harness;
    FunctionFlowNodeResult result = failure(
        QStringLiteral("speech_failed")
    );
    result.values = success(QStringLiteral("partial")).values;
    FunctionFlowValue observation;
    observation.voice =
        QSharedPointer<const FunctionFlowVoicePayload>(
            new FunctionFlowVoicePayload
        );
    result.historyObservations << observation;
    harness.voiceResult = result;
    FunctionFlowExecutionController controller(harness.access());

    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            executionPlan(),
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::Started
    );
    QTRY_VERIFY(!controller.isRunning());
    QCOMPARE(harness.modelCalls, 0);
    QCOMPARE(harness.actionCalls, 0);
    QCOMPARE(harness.historyRequests.size(), 1);
    QVERIFY(harness.historyRequests.first().canonicalInput.isEmpty());
    QVERIFY(harness.historyRequests.first().finalOutput.isEmpty());
    QCOMPARE(
        harness.historyRequests.first().terminalError.code,
        QStringLiteral("speech_failed")
    );
}

void FunctionFlowExecutionControllerTests::
savesFrozenVoiceMetadataExactlyOnce()
{
    RuntimeHarness harness;
    FunctionFlowVoicePayload *payload =
        new FunctionFlowVoicePayload;
    payload->sourceAudioPath = QStringLiteral("C:/audio/source.wav");
    RecordingSegment segment;
    segment.wavPath = QStringLiteral("C:/audio/part.wav");
    payload->segments << segment;
    payload->speechElapsedMs = 321;
    payload->recordingTriggerMode = QStringLiteral("hold");
    payload->longRecording = true;
    harness.voiceResult.values.first().voice =
        QSharedPointer<const FunctionFlowVoicePayload>(payload);
    harness.deferVoice = true;
    FunctionFlowExecutionController controller(harness.access());

    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            executionPlan(),
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::Started
    );
    QTRY_COMPARE(harness.deferredVoice.size(), 1);
    harness.dependencies->recordDirectory =
        QStringLiteral("C:/new-directory");
    harness.deferredVoice.first()(harness.voiceResult);
    QTRY_VERIFY(!controller.isRunning());

    QCOMPARE(harness.historyRequests.size(), 1);
    const FunctionFlowHistoryRequest request =
        harness.historyRequests.first();
    QCOMPARE(request.recordDirectory, QStringLiteral("C:/frozen"));
    QCOMPARE(
        request.sourceAudioPath,
        QStringLiteral("C:/audio/source.wav")
    );
    QCOMPARE(request.recordingSegments.size(), 1);
    QCOMPARE(request.speechElapsedMs, qint64(321));
    QCOMPARE(request.recordingTriggerMode, QStringLiteral("hold"));
    QVERIFY(request.longRecording);
}

void FunctionFlowExecutionControllerTests::
savesSafeScreenshotMetadataAndCancelledProvenance()
{
    RuntimeHarness screenshotHarness;
    FunctionFlowRuntimeAccess screenshotAccess =
        screenshotHarness.access();
    screenshotAccess.collectScreenshot = [&screenshotHarness](
        const FunctionFlowRunContext &context,
        const FunctionFlowCompiledNode &,
        const FunctionFlowNodeCompletion &completion) {
        screenshotHarness.calls << QStringLiteral("screenshot");
        screenshotHarness.contexts << context;
        FunctionFlowScreenshotPayload *payload =
            new FunctionFlowScreenshotPayload;
        payload->image = QImage(4, 4, QImage::Format_ARGB32);
        OcrTextBlock block;
        block.text = QStringLiteral("must not be copied to history");
        payload->blocks << block;
        payload->recognizedText = QStringLiteral("Screen text");
        payload->engine = OcrEngine::WindowsOcr;
        payload->elapsedMs = 456;
        payload->usedFallback = true;
        payload->rect = QRect(10, 20, 300, 200);
        FunctionFlowNodeResult result =
            success(QStringLiteral("Screen text"));
        result.values.first().screenshot =
            QSharedPointer<const FunctionFlowScreenshotPayload>(
                payload
            );
        completion(result);
    };
    screenshotAccess.runResultAction = [](
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowResultActionRequest &,
        const FunctionFlowNodeCompletion &completion) {
        completion(success(QString()));
    };
    FunctionFlowExecutionController screenshotController(
        screenshotAccess
    );
    QCOMPARE(
        screenshotController.start(
            QStringLiteral("custom_1"),
            screenshotPlan(),
            FunctionFlowTrigger::ScreenshotHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::Started
    );
    QTRY_VERIFY(!screenshotController.isRunning());
    QCOMPARE(screenshotHarness.historyRequests.size(), 1);
    const FunctionFlowHistoryRequest screenshotRequest =
        screenshotHarness.historyRequests.first();
    QCOMPARE(
        screenshotRequest.ocrEngineId,
        QStringLiteral("windows")
    );
    QCOMPARE(screenshotRequest.ocrElapsedMs, qint64(456));
    QVERIFY(screenshotRequest.ocrUsedFallback);
    QCOMPARE(
        screenshotRequest.screenshotRect,
        QRect(10, 20, 300, 200)
    );

    RuntimeHarness cancelledHarness;
    FunctionFlowNodeResult cancelled;
    cancelled.state = FunctionFlowNodeState::Cancelled;
    FunctionFlowValue observation;
    FunctionFlowVoicePayload *voice =
        new FunctionFlowVoicePayload;
    voice->sourceAudioPath = QStringLiteral("C:/audio/cancelled.wav");
    observation.voice =
        QSharedPointer<const FunctionFlowVoicePayload>(voice);
    cancelled.historyObservations << observation;
    cancelledHarness.voiceResult = cancelled;
    FunctionFlowExecutionController cancelledController(
        cancelledHarness.access()
    );
    QCOMPARE(
        cancelledController.start(
            QStringLiteral("custom_1"),
            executionPlan(),
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::Started
    );
    QTRY_VERIFY(!cancelledController.isRunning());
    QCOMPARE(cancelledHarness.historyRequests.size(), 1);
    QVERIFY(cancelledHarness.historyRequests.first().cancelled);
    QCOMPARE(
        cancelledHarness.historyRequests.first().sourceAudioPath,
        QStringLiteral("C:/audio/cancelled.wav")
    );
}

void FunctionFlowExecutionControllerTests::
reportsHistoryFailureWithoutRepeatingActions()
{
    RuntimeHarness harness;
    harness.historyOk = false;
    FunctionFlowExecutionController controller(harness.access());
    QSignalSpy runSpy(
        &controller,
        &FunctionFlowExecutionController::runExecutionChanged
    );

    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            executionPlan(),
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::Started
    );
    QTRY_VERIFY(!controller.isRunning());
    QCOMPARE(harness.historyRequests.size(), 1);
    QCOMPARE(harness.modelCalls, 1);
    QCOMPARE(harness.actionCalls, 2);
    const QVector<FunctionFlowRunExecutionEvent> events =
        runEvents(runSpy);
    QCOMPARE(
        events.last().terminalError.code,
        QStringLiteral("flow_history_save_failed")
    );
}

void FunctionFlowExecutionControllerTests::
coordinatesRetriggerBeforeBusyAndMissingPlanFallback()
{
    RuntimeHarness harness;
    harness.deferVoice = true;
    FunctionFlowExecutionOptions options;
    options.cancellationGraceMs = 1;
    FunctionFlowExecutionController controller(
        harness.access(),
        options
    );
    QSharedPointer<FunctionFlowExecutionPlan> plan(
        new FunctionFlowExecutionPlan(executionPlan())
    );
    FunctionFlowTriggerRequest request;
    request.functionId = QStringLiteral("custom_1");
    request.trigger = FunctionFlowTrigger::MainHotkey;

    QCOMPARE(
        controller.start(
            request,
            QSharedPointer<
                const FunctionFlowExecutionPlan
            >(plan)
        ),
        FunctionFlowStartOutcome::Started
    );
    QTRY_VERIFY(controller.isRunning());

    FunctionFlowTriggerRequest other = request;
    other.functionId = QStringLiteral("custom_2");
    QCOMPARE(
        controller.start(
            other,
            QSharedPointer<
                const FunctionFlowExecutionPlan
            >()
        ),
        FunctionFlowStartOutcome::Busy
    );
    QCOMPARE(
        controller.start(
            request,
            QSharedPointer<
                const FunctionFlowExecutionPlan
            >(plan)
        ),
        FunctionFlowStartOutcome::CancelledExisting
    );
    QTRY_VERIFY(!controller.isRunning());

    request.classicWorkflowBusy = true;
    QCOMPARE(
        controller.start(
            request,
            QSharedPointer<
                const FunctionFlowExecutionPlan
            >()
        ),
        FunctionFlowStartOutcome::Busy
    );
    request.classicWorkflowBusy = false;
    QCOMPARE(
        controller.start(
            request,
            QSharedPointer<
                const FunctionFlowExecutionPlan
            >()
        ),
        FunctionFlowStartOutcome::ConfigurationError
    );
}

void FunctionFlowExecutionControllerTests::
missingSharedPlanIsAConfigurationError()
{
    RuntimeHarness harness;
    FunctionFlowExecutionController controller(harness.access());
    FunctionFlowTriggerRequest request;
    request.functionId = QStringLiteral("custom_1");
    request.trigger = FunctionFlowTrigger::MainHotkey;

    QCOMPARE(
        controller.start(
            request,
            QSharedPointer<
                const FunctionFlowExecutionPlan
            >()
        ),
        FunctionFlowStartOutcome::ConfigurationError
    );
    QCOMPARE(
        controller.lastStartError().code,
        QStringLiteral("flow_published_unavailable")
    );
    QCOMPARE(
        controller.lastStartError().message,
        QString::fromUtf8(
            "当前画布没有可运行的发布流程。"
        )
    );
}

void FunctionFlowExecutionControllerTests::
mergesEditsBeforeFinalizationAndUpdatesOldRunsAfterward()
{
    RuntimeHarness harness;
    FunctionFlowRuntimeAccess access = harness.access();
    FunctionFlowExecutionController *controllerPointer = nullptr;
    ExecutionId oldRunId;
    access.runResultAction =
        [&harness, &controllerPointer, &oldRunId](
            const FunctionFlowRunContext &context,
            const FunctionFlowCompiledNode &node,
            const FunctionFlowResultActionRequest &,
            const FunctionFlowNodeCompletion &completion) {
            ++harness.actionCalls;
            if (node.nodeId == QStringLiteral("popup")) {
                oldRunId = context.runId;
                controllerPointer->editableSurfaceOpened(
                    context.runId
                );
                controllerPointer->editedTextCommitted(
                    context.runId,
                    QStringLiteral("finalizer 前编辑")
                );
            }
            completion(success(QString()));
        };
    FunctionFlowExecutionController controller(access);
    controllerPointer = &controller;

    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            executionPlan(),
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::Started
    );
    QTRY_VERIFY(!controller.isRunning());
    QCOMPARE(harness.historyRequests.size(), 1);
    QCOMPARE(
        harness.historyRequests.first().pendingEditedText,
        QStringLiteral("finalizer 前编辑")
    );
    QCOMPARE(harness.editRequests.size(), 0);

    controller.editedTextCommitted(
        oldRunId,
        QStringLiteral("新运行后关闭旧窗口")
    );
    QCOMPARE(harness.editRequests.size(), 1);
    QCOMPARE(harness.editRequests.first().runId, oldRunId);
    QCOMPARE(
        harness.editRequests.first().recordDirectory,
        QStringLiteral("C:/frozen")
    );
    QCOMPARE(
        harness.editRequests.first().detailPath,
        QStringLiteral("C:/frozen/run.json")
    );
    QCOMPARE(
        harness.editRequests.first().editedText,
        QStringLiteral("新运行后关闭旧窗口")
    );

    controller.editableSurfaceClosed(oldRunId);
    controller.editedTextCommitted(
        oldRunId,
        QStringLiteral("关闭后的迟到编辑")
    );
    QCOMPARE(harness.editRequests.size(), 1);
}

void FunctionFlowExecutionControllerTests::
onlyCancelsTheMatchingRunFromAResultSurface()
{
    RuntimeHarness harness;
    harness.deferVoice = true;
    FunctionFlowExecutionOptions options;
    options.cancellationGraceMs = 1;
    FunctionFlowExecutionController controller(
        harness.access(),
        options
    );
    QSignalSpy runSpy(
        &controller,
        &FunctionFlowExecutionController::runExecutionChanged
    );

    QCOMPARE(
        controller.start(
            QStringLiteral("custom_1"),
            executionPlan(),
            FunctionFlowTrigger::MainHotkey,
            nullptr
        ),
        FunctionFlowStartOutcome::Started
    );
    QTRY_VERIFY(controller.isRunning());
    const ExecutionId activeRunId =
        runEvents(runSpy).first().runId;
    ExecutionId staleRunId;
    staleRunId.value = QStringLiteral("stale-run");

    QVERIFY(!controller.cancel(staleRunId));
    QVERIFY(controller.isRunning());
    QVERIFY(controller.cancel(activeRunId));
    QTRY_VERIFY(!controller.isRunning());
}

QTEST_MAIN(FunctionFlowExecutionControllerTests)

#include "function_flow_execution_controller_tests.moc"
