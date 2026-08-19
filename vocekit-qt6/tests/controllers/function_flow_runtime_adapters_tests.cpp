#include <QtTest>

#include "../../src/controllers/function_flow_runtime_adapters.h"

#include <QAtomicInt>
#include <QThread>

namespace {

FunctionFlowCompiledNode node(
    const QString &id,
    FunctionFlowNodeType type)
{
    FunctionFlowCompiledNode result;
    result.nodeId = id;
    result.type = type;
    return result;
}

FunctionFlowExecutionPlan planForSelectionModel()
{
    FunctionFlowExecutionPlan plan;
    plan.functionId = QStringLiteral("translate");
    plan.publishedRevision = 4;
    plan.publishedHash = QStringLiteral("hash");

    FunctionFlowCompiledNode selection = node(
        QStringLiteral("selection"),
        FunctionFlowNodeType::SelectionSource
    );
    selection.successors << QStringLiteral("input");
    FunctionFlowCompiledNode input = node(
        QStringLiteral("input"),
        FunctionFlowNodeType::Input
    );
    input.successors << QStringLiteral("model");
    FunctionFlowCompiledNode model = node(
        QStringLiteral("model"),
        FunctionFlowNodeType::Model
    );
    model.config.model.modelId =
        QStringLiteral("fake-model");
    model.config.model.promptId =
        QStringLiteral("prompt");
    model.config.model.sampling.temperatureEnabled = true;
    model.config.model.sampling.temperature = 0.72;
    model.config.model.sampling.topPEnabled = true;
    model.config.model.sampling.topP = 0.58;
    model.config.model.stream = true;
    model.config.model.networkPolicy =
        QStringLiteral("inherit");
    model.streamingResultPopupNodeId =
        QStringLiteral("popup");
    model.successors << QStringLiteral("output");
    FunctionFlowCompiledNode output = node(
        QStringLiteral("output"),
        FunctionFlowNodeType::Output
    );
    output.successors << QStringLiteral("popup");
    FunctionFlowCompiledNode popup = node(
        QStringLiteral("popup"),
        FunctionFlowNodeType::ResultPopup
    );
    plan.nodes.insert(selection.nodeId, selection);
    plan.nodes.insert(input.nodeId, input);
    plan.nodes.insert(model.nodeId, model);
    plan.nodes.insert(output.nodeId, output);
    plan.nodes.insert(popup.nodeId, popup);
    plan.topologicalNodeIds
        << selection.nodeId
        << input.nodeId
        << model.nodeId
        << output.nodeId
        << popup.nodeId;
    FunctionFlowTriggerPlan trigger;
    trigger.trigger = FunctionFlowTrigger::MainHotkey;
    trigger.available = true;
    trigger.activeSourceNodeIds << selection.nodeId;
    trigger.acquisitionNodeIds << selection.nodeId;
    plan.triggers.insert(
        FunctionFlowTrigger::MainHotkey,
        trigger
    );
    return plan;
}

PromptRuntimeSnapshot snapshot(
    const QString &promptText,
    bool useProxy)
{
    PromptRuntimeSnapshot result;
    result.settings.useSystemProxy = useProxy;
    result.settings.strongSelectionEnabled = true;
    result.settings.resultPopupOpacity = 77;
    result.settings.recordDirectory =
        QStringLiteral("relative-history");
    FunctionSettings function;
    function.id = QStringLiteral("translate");
    function.name = QString::fromUtf8("翻译流程");
    result.settings.functions.append(function);
    PromptLibraryItem prompt;
    prompt.id = QStringLiteral("prompt");
    prompt.name = QStringLiteral("Prompt");
    prompt.content = promptText;
    result.libraryItems.append(prompt);
    return result;
}

FunctionFlowRunContext runContext(
    const CancellationSource &cancellation,
    const QSharedPointer<
        const FunctionFlowResolvedDependencies> &dependencies)
{
    FunctionFlowRunContext run;
    run.runId = cancellation.executionId();
    run.functionId = QStringLiteral("translate");
    run.targetWindow =
        reinterpret_cast<FunctionFlowTargetWindowHandle>(
            quintptr(101)
        );
    run.cancellation = cancellation.token();
    run.dependencies = dependencies;
    return run;
}

} // namespace

class FunctionFlowRuntimeAdaptersTests : public QObject
{
    Q_OBJECT

private slots:
    void resolvesAndFreezesDependencies();
    void preservesAvailableLegacyModelWithoutSilentReplacement();
    void keepsInvalidModelReferencesInvalid_data();
    void keepsInvalidModelReferencesInvalid();
    void rejectsUnconfiguredSpeechProviderBeforeRecording();
    void rejectsInvalidTargetBeforeSelectionReader();
    void emptySelectionIsSuccessfulForInputValidation();
    void runsGenericModelTaskWithFrozenRequestAndStreaming();
    void rejectsEmptyModelInputWithoutStartingTask();
};

void FunctionFlowRuntimeAdaptersTests::
resolvesAndFreezesDependencies()
{
    PromptRuntimeSnapshot current = snapshot(
        QStringLiteral("original system"),
        true
    );
    FunctionFlowRuntimeAdapterAccess access;
    access.runtimeSnapshot = [&]() {
        return current;
    };
    access.availableModelIds = []() {
        return QStringList() << QStringLiteral("fake-model");
    };
    access.resolveRecordDirectory =
        [](const QString &value) {
            return QStringLiteral("C:/frozen/") + value;
        };
    access.isUsableExternalTargetWindow =
        [](FunctionFlowTargetWindowHandle target) {
            return target != nullptr;
        };

    FunctionFlowModelTaskRunnerAccess taskAccess;
    taskAccess.runTask = [](
        const ModelRequestTaskRequest &request,
        const ModelDeltaCallback &) {
        ModelRequestTaskResult result;
        result.text = QStringLiteral("done");
        result.executionId =
            request.cancellation.executionId();
        return result;
    };
    FunctionFlowModelTaskRunner runner(taskAccess);
    FunctionFlowRuntimeAdapters adapters(access, &runner);

    QSharedPointer<const FunctionFlowResolvedDependencies>
        resolved;
    OperationError error;
    const FunctionFlowExecutionPlan plan =
        planForSelectionModel();
    QVERIFY(adapters.resolveDependencies(
        plan,
        FunctionFlowTrigger::MainHotkey,
        reinterpret_cast<void *>(quintptr(101)),
        &resolved,
        &error
    ));
    QVERIFY(error.isEmpty());
    QVERIFY(!resolved.isNull());
    QCOMPARE(
        resolved->functionTitle,
        QString::fromUtf8("翻译流程")
    );
    QCOMPARE(
        resolved->recordDirectory,
        QStringLiteral("C:/frozen/relative-history")
    );
    QCOMPARE(resolved->inheritedResultPopupOpacity, 77);
    QCOMPARE(
        resolved->byNodeId.value(QStringLiteral("model"))
            .modelId,
        QStringLiteral("fake-model")
    );
    QCOMPARE(
        resolved->byNodeId.value(QStringLiteral("model"))
            .systemPrompt,
        QStringLiteral("original system")
    );
    QCOMPARE(
        resolved->byNodeId.value(QStringLiteral("model"))
            .effectiveNetworkPolicy,
        QStringLiteral("systemProxy")
    );

    current = snapshot(QStringLiteral("changed"), false);
    QCOMPARE(
        resolved->byNodeId.value(QStringLiteral("model"))
            .systemPrompt,
        QStringLiteral("original system")
    );
    QCOMPARE(
        resolved->byNodeId.value(QStringLiteral("model"))
            .effectiveNetworkPolicy,
        QStringLiteral("systemProxy")
    );
}

void FunctionFlowRuntimeAdaptersTests::
preservesAvailableLegacyModelWithoutSilentReplacement()
{
    FunctionFlowExecutionPlan plan = planForSelectionModel();
    plan.nodes[QStringLiteral("model")]
        .config.model.modelId =
            QStringLiteral("claude:claude-sonnet-4-6");

    FunctionFlowRuntimeAdapterAccess access;
    access.runtimeSnapshot = []() {
        return snapshot(QStringLiteral("system"), false);
    };
    access.availableModelIds = []() {
        return QStringList()
            << QStringLiteral("claude:claude-sonnet-4-6");
    };
    access.isUsableExternalTargetWindow =
        [](FunctionFlowTargetWindowHandle target) {
            return target != nullptr;
        };
    FunctionFlowRuntimeAdapters adapters(access);

    QSharedPointer<const FunctionFlowResolvedDependencies>
        resolved;
    OperationError error;
    QVERIFY(adapters.resolveDependencies(
        plan,
        FunctionFlowTrigger::MainHotkey,
        reinterpret_cast<void *>(quintptr(101)),
        &resolved,
        &error
    ));
    QVERIFY(error.isEmpty());
    QCOMPARE(
        resolved->byNodeId.value(QStringLiteral("model"))
            .modelId,
        QStringLiteral("claude:claude-sonnet-4-6")
    );
    QCOMPARE(
        plan.nodes.value(QStringLiteral("model"))
            .config.model.modelId,
        QStringLiteral("claude:claude-sonnet-4-6")
    );
}

void FunctionFlowRuntimeAdaptersTests::
keepsInvalidModelReferencesInvalid_data()
{
    QTest::addColumn<QString>("configuredModelId");

    QTest::newRow("empty") << QString();
    QTest::newRow("unknown")
        << QStringLiteral("unknown-model");
}

void FunctionFlowRuntimeAdaptersTests::
keepsInvalidModelReferencesInvalid()
{
    QFETCH(QString, configuredModelId);
    FunctionFlowExecutionPlan plan = planForSelectionModel();
    plan.nodes[QStringLiteral("model")]
        .config.model.modelId = configuredModelId;

    FunctionFlowRuntimeAdapterAccess access;
    access.runtimeSnapshot = []() {
        return snapshot(QStringLiteral("system"), false);
    };
    access.availableModelIds = []() {
        return QStringList()
            << QStringLiteral("deepseek-v4-flash");
    };
    access.isUsableExternalTargetWindow =
        [](FunctionFlowTargetWindowHandle target) {
            return target != nullptr;
        };
    FunctionFlowRuntimeAdapters adapters(access);

    QSharedPointer<const FunctionFlowResolvedDependencies>
        resolved;
    OperationError error;
    QVERIFY(!adapters.resolveDependencies(
        plan,
        FunctionFlowTrigger::MainHotkey,
        reinterpret_cast<void *>(quintptr(101)),
        &resolved,
        &error
    ));
    QCOMPARE(
        error.code,
        QStringLiteral("flow_model_reference_missing")
    );
    QVERIFY(resolved.isNull());
}

void FunctionFlowRuntimeAdaptersTests::
rejectsUnconfiguredSpeechProviderBeforeRecording()
{
    QString observedProviderId;
    FunctionFlowExecutionPlan plan;
    plan.functionId = QStringLiteral("dictate");
    FunctionFlowCompiledNode voice = node(
        QStringLiteral("voice"),
        FunctionFlowNodeType::VoiceSource
    );
    voice.config.voice.speechProviderId =
        QStringLiteral("baidu");
    plan.nodes.insert(voice.nodeId, voice);
    FunctionFlowTriggerPlan trigger;
    trigger.available = true;
    trigger.activeSourceNodeIds << voice.nodeId;
    plan.triggers.insert(
        FunctionFlowTrigger::MainHotkey,
        trigger
    );

    FunctionFlowRuntimeAdapterAccess access;
    access.runtimeSnapshot = []() {
        return snapshot(QStringLiteral("system"), false);
    };
    access.speechConfigurationError =
        [&](const QString &providerId) {
            observedProviderId = providerId;
            return QStringLiteral("missing secret");
        };
    FunctionFlowRuntimeAdapters adapters(access);
    QSharedPointer<const FunctionFlowResolvedDependencies>
        resolved;
    OperationError error;
    QVERIFY(!adapters.resolveDependencies(
        plan,
        FunctionFlowTrigger::MainHotkey,
        nullptr,
        &resolved,
        &error
    ));
    QCOMPARE(
        error.code,
        QStringLiteral(
            "flow_speech_provider_reference_missing"
        )
    );
    QCOMPARE(observedProviderId, QStringLiteral("baidu"));
}

void FunctionFlowRuntimeAdaptersTests::
rejectsInvalidTargetBeforeSelectionReader()
{
    int readerCalls = 0;
    FunctionFlowRuntimeAdapterAccess access;
    access.runtimeSnapshot = []() {
        return snapshot(QStringLiteral("system"), false);
    };
    access.availableModelIds = []() {
        return QStringList() << QStringLiteral("fake-model");
    };
    access.isUsableExternalTargetWindow =
        [](FunctionFlowTargetWindowHandle) {
            return false;
        };
    access.readSelectedText =
        [&](const SelectedTextWorkflowRequest &) {
            ++readerCalls;
            return SelectedTextWorkflowResult();
        };
    FunctionFlowRuntimeAdapters adapters(access);

    QSharedPointer<const FunctionFlowResolvedDependencies>
        resolved;
    OperationError error;
    QVERIFY(!adapters.resolveDependencies(
        planForSelectionModel(),
        FunctionFlowTrigger::MainHotkey,
        nullptr,
        &resolved,
        &error
    ));
    QCOMPARE(
        error.code,
        QStringLiteral("flow_target_window_unavailable")
    );
    QCOMPARE(readerCalls, 0);
}

void FunctionFlowRuntimeAdaptersTests::
emptySelectionIsSuccessfulForInputValidation()
{
    bool suppressMissing = false;
    FunctionFlowRuntimeAdapterAccess access;
    access.runtimeSnapshot = []() {
        return snapshot(QStringLiteral("system"), false);
    };
    access.availableModelIds = []() {
        return QStringList() << QStringLiteral("fake-model");
    };
    access.isUsableExternalTargetWindow =
        [](FunctionFlowTargetWindowHandle target) {
            return target != nullptr;
        };
    access.readSelectedText =
        [&](const SelectedTextWorkflowRequest &request) {
            suppressMissing = request.suppressMissingPrompt;
            return SelectedTextWorkflowResult();
        };
    FunctionFlowRuntimeAdapters adapters(access);

    QSharedPointer<const FunctionFlowResolvedDependencies>
        resolved;
    OperationError error;
    const FunctionFlowExecutionPlan plan =
        planForSelectionModel();
    QVERIFY(adapters.resolveDependencies(
        plan,
        FunctionFlowTrigger::MainHotkey,
        reinterpret_cast<void *>(quintptr(101)),
        &resolved,
        &error
    ));

    CancellationSource cancellation;
    FunctionFlowNodeResult observed;
    adapters.collectSelection(
        runContext(cancellation, resolved),
        plan.nodes.value(QStringLiteral("selection")),
        [&](const FunctionFlowNodeResult &result) {
            observed = result;
        }
    );

    QVERIFY(suppressMissing);
    QCOMPARE(
        observed.state,
        FunctionFlowNodeState::Succeeded
    );
    QCOMPARE(observed.values.size(), 1);
    QVERIFY(observed.values.first().text.isEmpty());
}

void FunctionFlowRuntimeAdaptersTests::
runsGenericModelTaskWithFrozenRequestAndStreaming()
{
    ModelRequestTaskRequest captured;
    QStringList streamed;
    int previewCount = 0;
    FunctionFlowNodeResult observed;

    FunctionFlowModelTaskRunnerAccess taskAccess;
    taskAccess.runTask = [&](
        const ModelRequestTaskRequest &request,
        const ModelDeltaCallback &onDelta) {
        captured = request;
        onDelta(QStringLiteral("流"));
        onDelta(QStringLiteral("式"));
        ModelRequestTaskResult result;
        result.text = QString::fromUtf8("流式结果");
        result.durationMs = 12;
        result.promptVersion = QStringLiteral("version");
        result.executionId =
            request.cancellation.executionId();
        return result;
    };
    FunctionFlowModelTaskRunner runner(taskAccess);

    FunctionFlowRuntimeAdapterAccess access;
    access.runtimeSnapshot = []() {
        return snapshot(QStringLiteral("frozen system"), true);
    };
    access.availableModelIds = []() {
        return QStringList() << QStringLiteral("fake-model");
    };
    access.isUsableExternalTargetWindow =
        [](FunctionFlowTargetWindowHandle target) {
            return target != nullptr;
        };
    access.beginStreamingPreview =
        [&](const FunctionFlowRunContext &,
            const QString &modelNodeId,
            const QString &popupNodeId) {
            QCOMPARE(modelNodeId, QStringLiteral("model"));
            QCOMPARE(popupNodeId, QStringLiteral("popup"));
            ++previewCount;
        };
    access.appendStreamingDelta =
        [&](const ExecutionId &,
            const QString &,
            const QString &popupNodeId,
            const QString &delta) {
            QCOMPARE(popupNodeId, QStringLiteral("popup"));
            streamed.append(delta);
        };
    FunctionFlowRuntimeAdapters adapters(access, &runner);

    const FunctionFlowExecutionPlan plan =
        planForSelectionModel();
    QSharedPointer<const FunctionFlowResolvedDependencies>
        resolved;
    OperationError error;
    QVERIFY(adapters.resolveDependencies(
        plan,
        FunctionFlowTrigger::MainHotkey,
        reinterpret_cast<void *>(quintptr(101)),
        &resolved,
        &error
    ));
    CancellationSource cancellation;
    QList<FunctionFlowValue> values;
    FunctionFlowValue second;
    second.text = QStringLiteral("Hello");
    second.role = QString::fromUtf8("待处理原文");
    second.sequence = 1;
    second.sourceNodeId = QStringLiteral("selection");
    second.screenshot =
        QSharedPointer<const FunctionFlowScreenshotPayload>(
            new FunctionFlowScreenshotPayload
        );
    FunctionFlowValue first;
    first.text = QString::fromUtf8("请翻译");
    first.role = QString::fromUtf8("用户要求");
    first.sequence = 0;
    first.sourceNodeId = QStringLiteral("instruction");
    values << second << first;

    adapters.runModel(
        runContext(cancellation, resolved),
        plan.nodes.value(QStringLiteral("model")),
        values,
        [&](const FunctionFlowNodeResult &result) {
            observed = result;
        }
    );

    QTRY_COMPARE_WITH_TIMEOUT(
        observed.state,
        FunctionFlowNodeState::Succeeded,
        3000
    );
    QCOMPARE(previewCount, 1);
    QCOMPARE(streamed.join(QString()), QString::fromUtf8("流式"));
    QCOMPARE(captured.modelId, QStringLiteral("fake-model"));
    QCOMPARE(captured.systemPrompt, QStringLiteral("frozen system"));
    QCOMPARE(
        captured.userPrompt,
        QString::fromUtf8(
            "[用户要求]\n请翻译\n\n"
            "[待处理原文]\nHello"
        )
    );
    QVERIFY(captured.stream);
    QCOMPARE(
        captured.networkPolicy,
        QStringLiteral("systemProxy")
    );
    QVERIFY(captured.useSystemProxy);
    QVERIFY(captured.sampling.temperatureEnabled);
    QCOMPARE(captured.sampling.temperature, 0.72);
    QVERIFY(captured.sampling.topPEnabled);
    QCOMPARE(captured.sampling.topP, 0.58);
    QCOMPARE(
        captured.cancellation.executionId(),
        cancellation.executionId()
    );
    QCOMPARE(
        observed.values.first().text,
        QString::fromUtf8("流式结果")
    );
    QCOMPARE(
        observed.values.first().screenshot,
        second.screenshot
    );
}

void FunctionFlowRuntimeAdaptersTests::
rejectsEmptyModelInputWithoutStartingTask()
{
    int taskCalls = 0;
    FunctionFlowModelTaskRunnerAccess taskAccess;
    taskAccess.runTask = [&](
        const ModelRequestTaskRequest &request,
        const ModelDeltaCallback &) {
        ++taskCalls;
        ModelRequestTaskResult result;
        result.executionId =
            request.cancellation.executionId();
        return result;
    };
    FunctionFlowModelTaskRunner runner(taskAccess);
    FunctionFlowRuntimeAdapterAccess access;
    access.runtimeSnapshot = []() {
        return snapshot(QStringLiteral("system"), false);
    };
    access.availableModelIds = []() {
        return QStringList() << QStringLiteral("fake-model");
    };
    access.isUsableExternalTargetWindow =
        [](FunctionFlowTargetWindowHandle target) {
            return target != nullptr;
        };
    FunctionFlowRuntimeAdapters adapters(access, &runner);

    const FunctionFlowExecutionPlan plan =
        planForSelectionModel();
    QSharedPointer<const FunctionFlowResolvedDependencies>
        resolved;
    OperationError error;
    QVERIFY(adapters.resolveDependencies(
        plan,
        FunctionFlowTrigger::MainHotkey,
        reinterpret_cast<void *>(quintptr(101)),
        &resolved,
        &error
    ));
    CancellationSource cancellation;
    FunctionFlowNodeResult observed;
    adapters.runModel(
        runContext(cancellation, resolved),
        plan.nodes.value(QStringLiteral("model")),
        QList<FunctionFlowValue>(),
        [&](const FunctionFlowNodeResult &result) {
            observed = result;
        }
    );
    QCOMPARE(
        observed.error.code,
        QStringLiteral("flow_model_input_empty")
    );
    QCOMPARE(taskCalls, 0);
}

QTEST_MAIN(FunctionFlowRuntimeAdaptersTests)

#include "function_flow_runtime_adapters_tests.moc"
