#include <QtTest>

#include "../../src/domain/function_flow_compiler.h"
#include "../../src/domain/function_flow_scheduler.h"

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

FunctionFlowGraph twoInputGraph(bool inputsRequired = true)
{
    FunctionFlowGraph graph;

    FunctionFlowNode voice =
        node(QStringLiteral("voice"), FunctionFlowNodeType::VoiceSource);
    voice.config.voice.speechProviderId = QStringLiteral("speech");
    voice.config.voice.acquisitionSequence = 0;

    FunctionFlowNode instruction = node(
        QStringLiteral("input_instruction"),
        FunctionFlowNodeType::Input
    );
    instruction.config.input.role = QStringLiteral("instruction");
    instruction.config.input.sequence = 0;
    instruction.config.input.required = inputsRequired;

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
    source.config.input.required = inputsRequired;

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
        )
        << node(
            QStringLiteral("write"),
            FunctionFlowNodeType::AutoWrite
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
            QStringLiteral("text_in")
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
            2
        )
        << edge(
            QStringLiteral("output-write"),
            QStringLiteral("output"),
            QStringLiteral("action_out"),
            QStringLiteral("write"),
            QStringLiteral("action_in"),
            1
        );
    return graph;
}

FunctionFlowExecutionPlan planFor(const FunctionFlowGraph &graph)
{
    const QString hash = functionFlowGraphHash(graph);
    const FunctionFlowCompileResult result =
        FunctionFlowCompiler::compile(graph, 4, hash);
    Q_ASSERT(result.ok);
    return result.plan;
}

QList<FunctionFlowValue> values(const QString &text)
{
    FunctionFlowValue value;
    value.text = text;
    return QList<FunctionFlowValue>() << value;
}

int stateCount(
    const FunctionFlowExecutionPlan &plan,
    const FunctionFlowScheduler &scheduler,
    FunctionFlowNodeState state)
{
    int count = 0;
    for (auto it = plan.nodes.constBegin();
         it != plan.nodes.constEnd();
         ++it) {
        if (scheduler.state(it.key()) == state) {
            ++count;
        }
    }
    return count;
}

void finishTwoInputs(
    FunctionFlowScheduler *scheduler,
    const QString &voiceText,
    const QString &selectionText)
{
    QCOMPARE(scheduler->nextReadyNode(), QStringLiteral("voice"));
    QVERIFY(scheduler->succeed(
        QStringLiteral("voice"),
        values(voiceText)
    ));
    QCOMPARE(
        scheduler->nextReadyNode(),
        QStringLiteral("input_instruction")
    );
    QVERIFY(scheduler->succeed(
        QStringLiteral("input_instruction"),
        scheduler->inputValues(QStringLiteral("input_instruction"))
    ));

    QCOMPARE(
        scheduler->nextReadyNode(),
        QStringLiteral("selection")
    );
    QVERIFY(scheduler->succeed(
        QStringLiteral("selection"),
        values(selectionText)
    ));
    QCOMPARE(
        scheduler->nextReadyNode(),
        QStringLiteral("input_source")
    );
    QVERIFY(scheduler->succeed(
        QStringLiteral("input_source"),
        scheduler->inputValues(QStringLiteral("input_source"))
    ));
}

} // namespace

class FunctionFlowSchedulerTests : public QObject
{
    Q_OBJECT

private slots:
    void waitsForAllInputsInCompiledOrder();
    void appliesRequiredAndOptionalEmptyInputRules();
    void preservesProvenanceThroughModelAndOutput();
    void failsFastForServiceErrorsAndEmptyModels();
    void rejectsDuplicateCompletionWithoutChangingValues();
    void serializesBranchesAndTerminalActions();
    void activatesOnlySourcesForTheSelectedTrigger();
    void cancelsRunningAndPendingNodes();
    void detectsInconsistentPlanDeadlocks();
};

void FunctionFlowSchedulerTests::waitsForAllInputsInCompiledOrder()
{
    const FunctionFlowExecutionPlan plan =
        planFor(twoInputGraph());
    FunctionFlowScheduler scheduler(
        plan,
        FunctionFlowTrigger::MainHotkey
    );

    QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("voice"));
    QVERIFY(scheduler.succeed(
        QStringLiteral("voice"),
        values(QStringLiteral("请翻译"))
    ));
    QCOMPARE(
        scheduler.nextReadyNode(),
        QStringLiteral("input_instruction")
    );
    QVERIFY(scheduler.succeed(
        QStringLiteral("input_instruction"),
        scheduler.inputValues(QStringLiteral("input_instruction"))
    ));

    QCOMPARE(
        scheduler.nextReadyNode(),
        QStringLiteral("selection")
    );
    QVERIFY(scheduler.succeed(
        QStringLiteral("selection"),
        values(QStringLiteral("Hello"))
    ));
    QCOMPARE(
        scheduler.nextReadyNode(),
        QStringLiteral("input_source")
    );
    QVERIFY(scheduler.succeed(
        QStringLiteral("input_source"),
        scheduler.inputValues(QStringLiteral("input_source"))
    ));

    QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("model"));
    const QList<FunctionFlowValue> inputs =
        scheduler.inputValues(QStringLiteral("model"));
    QCOMPARE(inputs.size(), 2);
    QCOMPARE(inputs.at(0).text, QStringLiteral("请翻译"));
    QCOMPARE(inputs.at(0).role, QStringLiteral("instruction"));
    QCOMPARE(inputs.at(1).text, QStringLiteral("Hello"));
    QCOMPARE(inputs.at(1).role, QStringLiteral("source"));
}

void FunctionFlowSchedulerTests::
appliesRequiredAndOptionalEmptyInputRules()
{
    FunctionFlowScheduler required(
        planFor(twoInputGraph(true)),
        FunctionFlowTrigger::MainHotkey
    );
    QCOMPARE(required.nextReadyNode(), QStringLiteral("voice"));
    QVERIFY(required.succeed(QStringLiteral("voice"), {}));
    QCOMPARE(
        required.nextReadyNode(),
        QStringLiteral("input_instruction")
    );
    QVERIFY(required.succeed(
        QStringLiteral("input_instruction"),
        required.inputValues(QStringLiteral("input_instruction"))
    ));
    QCOMPARE(
        required.state(QStringLiteral("input_instruction")),
        FunctionFlowNodeState::Failed
    );
    QCOMPARE(
        required.terminalError().code,
        QStringLiteral("flow_required_input_empty")
    );
    QVERIFY(required.finished());

    FunctionFlowScheduler optional(
        planFor(twoInputGraph(false)),
        FunctionFlowTrigger::MainHotkey
    );
    finishTwoInputs(
        &optional,
        QString(),
        QStringLiteral("Hello")
    );
    QCOMPARE(
        optional.state(QStringLiteral("input_instruction")),
        FunctionFlowNodeState::Skipped
    );
    QCOMPARE(optional.nextReadyNode(), QStringLiteral("model"));
    QCOMPARE(
        optional.inputValues(QStringLiteral("model")).size(),
        1
    );
}

void FunctionFlowSchedulerTests::
preservesProvenanceThroughModelAndOutput()
{
    FunctionFlowScheduler scheduler(
        planFor(twoInputGraph(false)),
        FunctionFlowTrigger::MainHotkey
    );

    FunctionFlowValue provenance;
    provenance.voice =
        QSharedPointer<const FunctionFlowVoicePayload>(
            new FunctionFlowVoicePayload
        );
    QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("voice"));
    QVERIFY(scheduler.succeed(
        QStringLiteral("voice"),
        QList<FunctionFlowValue>() << provenance
    ));
    QCOMPARE(
        scheduler.nextReadyNode(),
        QStringLiteral("input_instruction")
    );
    QVERIFY(scheduler.succeed(
        QStringLiteral("input_instruction"),
        scheduler.inputValues(QStringLiteral("input_instruction"))
    ));
    QCOMPARE(
        scheduler.state(QStringLiteral("input_instruction")),
        FunctionFlowNodeState::Succeeded
    );

    QCOMPARE(
        scheduler.nextReadyNode(),
        QStringLiteral("selection")
    );
    QVERIFY(scheduler.succeed(
        QStringLiteral("selection"),
        values(QStringLiteral("Hello"))
    ));
    QCOMPARE(
        scheduler.nextReadyNode(),
        QStringLiteral("input_source")
    );
    QVERIFY(scheduler.succeed(
        QStringLiteral("input_source"),
        scheduler.inputValues(QStringLiteral("input_source"))
    ));

    QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("model"));
    QCOMPARE(
        scheduler.inputValues(QStringLiteral("model")).size(),
        2
    );
    QVERIFY(scheduler.succeed(
        QStringLiteral("model"),
        values(QStringLiteral("你好"))
    ));
    QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("output"));
    const QList<FunctionFlowValue> modelOutput =
        scheduler.inputValues(QStringLiteral("output"));
    QCOMPARE(modelOutput.size(), 1);
    QVERIFY(!modelOutput.first().voice.isNull());
}

void FunctionFlowSchedulerTests::
failsFastForServiceErrorsAndEmptyModels()
{
    FunctionFlowScheduler failedSource(
        planFor(twoInputGraph(false)),
        FunctionFlowTrigger::MainHotkey
    );
    QCOMPARE(failedSource.nextReadyNode(), QStringLiteral("voice"));
    OperationError providerFailure;
    providerFailure.code = QStringLiteral("speech_failed");
    QVERIFY(failedSource.fail(
        QStringLiteral("voice"),
        providerFailure
    ));
    QCOMPARE(
        failedSource.terminalError().code,
        QStringLiteral("speech_failed")
    );
    QCOMPARE(
        failedSource.state(QStringLiteral("input_instruction")),
        FunctionFlowNodeState::Blocked
    );
    QVERIFY(failedSource.finished());

    FunctionFlowScheduler allEmpty(
        planFor(twoInputGraph(false)),
        FunctionFlowTrigger::MainHotkey
    );
    finishTwoInputs(&allEmpty, QString(), QString());
    QCOMPARE(allEmpty.nextReadyNode(), QString());
    QCOMPARE(
        allEmpty.state(QStringLiteral("model")),
        FunctionFlowNodeState::Failed
    );
    QCOMPARE(
        allEmpty.terminalError().code,
        QStringLiteral("flow_model_input_empty")
    );
    QVERIFY(allEmpty.finished());
}

void FunctionFlowSchedulerTests::
rejectsDuplicateCompletionWithoutChangingValues()
{
    FunctionFlowScheduler scheduler(
        planFor(twoInputGraph()),
        FunctionFlowTrigger::MainHotkey
    );
    QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("voice"));
    QVERIFY(scheduler.succeed(
        QStringLiteral("voice"),
        values(QStringLiteral("first"))
    ));

    OperationError error;
    QVERIFY(!scheduler.succeed(
        QStringLiteral("voice"),
        values(QStringLiteral("second")),
        &error
    ));
    QCOMPARE(
        error.code,
        QStringLiteral("flow_scheduler_state_invalid")
    );
    QCOMPARE(
        scheduler.inputValues(QStringLiteral("input_instruction"))
            .first().text,
        QStringLiteral("first")
    );
}

void FunctionFlowSchedulerTests::
serializesBranchesAndTerminalActions()
{
    const FunctionFlowExecutionPlan plan =
        planFor(twoInputGraph());
    FunctionFlowScheduler scheduler(
        plan,
        FunctionFlowTrigger::MainHotkey
    );
    finishTwoInputs(
        &scheduler,
        QStringLiteral("Instruction"),
        QStringLiteral("Source")
    );

    QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("model"));
    QVERIFY(scheduler.start(QStringLiteral("model")));
    QCOMPARE(
        stateCount(plan, scheduler, FunctionFlowNodeState::Running),
        1
    );
    QCOMPARE(scheduler.nextReadyNode(), QString());

    OperationError busyError;
    QVERIFY(!scheduler.start(QStringLiteral("output"), &busyError));
    QCOMPARE(
        busyError.code,
        QStringLiteral("flow_scheduler_state_invalid")
    );
    QVERIFY(scheduler.succeed(
        QStringLiteral("model"),
        values(QStringLiteral("Result"))
    ));
    OperationError duplicateError;
    QVERIFY(!scheduler.succeed(
        QStringLiteral("model"),
        values(QStringLiteral("Changed")),
        &duplicateError
    ));
    QCOMPARE(
        duplicateError.code,
        QStringLiteral("flow_scheduler_state_invalid")
    );
    QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("output"));
    QCOMPARE(
        scheduler.inputValues(QStringLiteral("output")).first().text,
        QStringLiteral("Result")
    );
    QVERIFY(scheduler.succeed(
        QStringLiteral("output"),
        scheduler.inputValues(QStringLiteral("output"))
    ));

    QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("write"));
    QVERIFY(scheduler.succeed(
        QStringLiteral("write"),
        scheduler.inputValues(QStringLiteral("write"))
    ));
    QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("popup"));
    QVERIFY(scheduler.succeed(
        QStringLiteral("popup"),
        scheduler.inputValues(QStringLiteral("popup"))
    ));
    QVERIFY(scheduler.finished());
    QVERIFY(scheduler.terminalError().isEmpty());
}

void FunctionFlowSchedulerTests::
activatesOnlySourcesForTheSelectedTrigger()
{
    FunctionFlowGraph graph = twoInputGraph(false);
    FunctionFlowNode screenshot = node(
        QStringLiteral("screenshot"),
        FunctionFlowNodeType::ScreenshotSource
    );
    screenshot.config.screenshot.triggerMode =
        QStringLiteral("separate");
    screenshot.config.screenshot.ocrEngineId =
        QStringLiteral("automatic");
    screenshot.config.screenshot.separateShortcut =
        QStringLiteral("Ctrl+Alt+S");
    screenshot.config.screenshot.acquisitionSequence = 2;
    FunctionFlowNode screenshotInput = node(
        QStringLiteral("input_screenshot"),
        FunctionFlowNodeType::Input
    );
    screenshotInput.config.input.role = QStringLiteral("screenshot");
    screenshotInput.config.input.sequence = 2;
    screenshotInput.config.input.required = false;
    graph.nodes << screenshot << screenshotInput;
    graph.edges
        << edge(
            QStringLiteral("screenshot-input"),
            QStringLiteral("screenshot"),
            QStringLiteral("text_out"),
            QStringLiteral("input_screenshot"),
            QStringLiteral("text_in")
        )
        << edge(
            QStringLiteral("screenshot-model"),
            QStringLiteral("input_screenshot"),
            QStringLiteral("text_out"),
            QStringLiteral("model"),
            QStringLiteral("text_in")
        );

    FunctionFlowScheduler scheduler(
        planFor(graph),
        FunctionFlowTrigger::ScreenshotHotkey
    );
    QCOMPARE(
        scheduler.state(QStringLiteral("voice")),
        FunctionFlowNodeState::Skipped
    );
    QCOMPARE(
        scheduler.state(QStringLiteral("selection")),
        FunctionFlowNodeState::Skipped
    );
    QCOMPARE(
        scheduler.nextReadyNode(),
        QStringLiteral("input_instruction")
    );
    QVERIFY(scheduler.succeed(
        QStringLiteral("input_instruction"),
        scheduler.inputValues(QStringLiteral("input_instruction"))
    ));
    QCOMPARE(
        scheduler.nextReadyNode(),
        QStringLiteral("input_source")
    );
    QVERIFY(scheduler.succeed(
        QStringLiteral("input_source"),
        scheduler.inputValues(QStringLiteral("input_source"))
    ));
    QCOMPARE(
        scheduler.nextReadyNode(),
        QStringLiteral("screenshot")
    );
}

void FunctionFlowSchedulerTests::cancelsRunningAndPendingNodes()
{
    const FunctionFlowExecutionPlan plan =
        planFor(twoInputGraph());
    FunctionFlowScheduler scheduler(
        plan,
        FunctionFlowTrigger::MainHotkey
    );
    QCOMPARE(scheduler.nextReadyNode(), QStringLiteral("voice"));
    QVERIFY(scheduler.start(QStringLiteral("voice")));

    scheduler.cancel();
    QCOMPARE(
        scheduler.state(QStringLiteral("voice")),
        FunctionFlowNodeState::Cancelling
    );
    QCOMPARE(
        scheduler.state(QStringLiteral("selection")),
        FunctionFlowNodeState::Cancelled
    );
    QCOMPARE(scheduler.nextReadyNode(), QString());
    QVERIFY(!scheduler.finished());

    OperationError error;
    QVERIFY(!scheduler.completeCancelled(
        QStringLiteral("selection"),
        &error
    ));
    QCOMPARE(
        error.code,
        QStringLiteral("flow_scheduler_state_invalid")
    );
    QVERIFY(scheduler.completeCancelled(QStringLiteral("voice")));
    QVERIFY(scheduler.finished());
    QCOMPARE(
        scheduler.terminalError().code,
        QStringLiteral("flow_cancelled")
    );
}

void FunctionFlowSchedulerTests::detectsInconsistentPlanDeadlocks()
{
    FunctionFlowExecutionPlan plan;
    FunctionFlowCompiledNode input;
    input.nodeId = QStringLiteral("input");
    input.type = FunctionFlowNodeType::Input;
    FunctionFlowCompiledInput binding;
    binding.predecessorNodeId = QStringLiteral("missing");
    input.inputs << binding;
    plan.nodes.insert(input.nodeId, input);
    plan.topologicalNodeIds << input.nodeId;

    FunctionFlowTriggerPlan trigger;
    trigger.trigger = FunctionFlowTrigger::MainHotkey;
    trigger.available = true;
    plan.triggers.insert(trigger.trigger, trigger);

    FunctionFlowScheduler scheduler(
        plan,
        FunctionFlowTrigger::MainHotkey
    );
    QCOMPARE(scheduler.nextReadyNode(), QString());
    QCOMPARE(
        scheduler.terminalError().code,
        QStringLiteral("flow_scheduler_deadlock")
    );
    QVERIFY(scheduler.finished());
}

QTEST_MAIN(FunctionFlowSchedulerTests)

#include "function_flow_scheduler_tests.moc"
