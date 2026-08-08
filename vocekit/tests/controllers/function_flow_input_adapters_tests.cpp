#include <QtTest>

#include "../../src/controllers/function_flow_runtime_adapters.h"

QVector<ModelOption> modelOptions()
{
    return QVector<ModelOption>();
}

QString normalizeModelId(const QString &value, const QString &fallback)
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? fallback : trimmed;
}

ModelRequestTaskResult runModelProviderRequestTask(
    const ModelRequestTaskRequest &request,
    const ModelDeltaCallback &)
{
    ModelRequestTaskResult result;
    result.executionId = request.cancellation.executionId();
    return result;
}

class FunctionFlowInputAdaptersTests : public QObject
{
    Q_OBJECT

private slots:
    void delegatesVoiceAndScreenshotWithTheFrozenRun();
};

void FunctionFlowInputAdaptersTests::
delegatesVoiceAndScreenshotWithTheFrozenRun()
{
    CancellationSource cancellation;
    FunctionFlowRunContext run;
    run.runId = cancellation.executionId();
    run.functionId = QStringLiteral("flow-inputs");
    run.trigger = FunctionFlowTrigger::ScreenshotLauncher;
    run.targetWindow = reinterpret_cast<void *>(quintptr(77));
    run.cancellation = cancellation.token();

    int voiceCalls = 0;
    int screenshotCalls = 0;
    FunctionFlowRuntimeAdapterAccess access;
    access.collectVoice =
        [&](const FunctionFlowRunContext &observedRun,
            const FunctionFlowCompiledNode &node,
            const FunctionFlowNodeCompletion &completion) {
            ++voiceCalls;
            QCOMPARE(observedRun.runId, run.runId);
            QCOMPARE(observedRun.targetWindow, run.targetWindow);
            QCOMPARE(node.nodeId, QStringLiteral("voice"));
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Succeeded;
            completion(result);
        };
    access.collectScreenshot =
        [&](const FunctionFlowRunContext &observedRun,
            const FunctionFlowCompiledNode &node,
            const FunctionFlowNodeCompletion &completion) {
            ++screenshotCalls;
            QCOMPARE(observedRun.runId, run.runId);
            QCOMPARE(
                observedRun.trigger,
                FunctionFlowTrigger::ScreenshotLauncher
            );
            QCOMPARE(node.nodeId, QStringLiteral("screenshot"));
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Succeeded;
            completion(result);
        };

    FunctionFlowModelTaskRunnerAccess taskAccess;
    taskAccess.runTask = [](
        const ModelRequestTaskRequest &request,
        const ModelDeltaCallback &) {
        ModelRequestTaskResult result;
        result.executionId = request.cancellation.executionId();
        return result;
    };
    FunctionFlowModelTaskRunner runner(taskAccess);
    FunctionFlowRuntimeAdapters adapters(access, &runner);
    const FunctionFlowRuntimeAccess runtime = adapters.runtimeAccess();

    FunctionFlowCompiledNode voice;
    voice.nodeId = QStringLiteral("voice");
    voice.type = FunctionFlowNodeType::VoiceSource;
    FunctionFlowCompiledNode screenshot;
    screenshot.nodeId = QStringLiteral("screenshot");
    screenshot.type = FunctionFlowNodeType::ScreenshotSource;
    int completions = 0;
    const FunctionFlowNodeCompletion completion =
        [&](const FunctionFlowNodeResult &result) {
            QCOMPARE(
                result.state,
                FunctionFlowNodeState::Succeeded
            );
            ++completions;
        };

    runtime.collectVoice(run, voice, completion);
    runtime.collectScreenshot(run, screenshot, completion);

    QCOMPARE(voiceCalls, 1);
    QCOMPARE(screenshotCalls, 1);
    QCOMPARE(completions, 2);
}

QTEST_MAIN(FunctionFlowInputAdaptersTests)

#include "function_flow_input_adapters_tests.moc"
