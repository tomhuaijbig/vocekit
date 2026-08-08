#include "function_flow_execution_controller.h"

#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QTimer>

namespace {

int completionEventType()
{
    static const int type = QEvent::registerEventType();
    return type;
}

class FunctionFlowCompletionEvent : public QEvent
{
public:
    FunctionFlowCompletionEvent(
        int generationValue,
        const ExecutionId &runIdValue,
        const QString &nodeIdValue,
        const FunctionFlowNodeResult &resultValue)
        : QEvent(static_cast<QEvent::Type>(completionEventType())),
          generation(generationValue),
          runId(runIdValue),
          nodeId(nodeIdValue),
          result(resultValue)
    {
    }

    int generation = 0;
    ExecutionId runId;
    QString nodeId;
    FunctionFlowNodeResult result;
};

OperationError executionError(
    const QString &code,
    const QString &message = QString(),
    const QString &detail = QString())
{
    OperationError error;
    error.code = code;
    error.message = message;
    error.detail = detail;
    return error;
}

QString nodeStateId(FunctionFlowNodeState state)
{
    switch (state) {
    case FunctionFlowNodeState::Pending:
        return QStringLiteral("pending");
    case FunctionFlowNodeState::Ready:
        return QStringLiteral("ready");
    case FunctionFlowNodeState::Running:
        return QStringLiteral("running");
    case FunctionFlowNodeState::Cancelling:
        return QStringLiteral("cancelling");
    case FunctionFlowNodeState::Succeeded:
        return QStringLiteral("succeeded");
    case FunctionFlowNodeState::Skipped:
        return QStringLiteral("skipped");
    case FunctionFlowNodeState::Failed:
        return QStringLiteral("failed");
    case FunctionFlowNodeState::Blocked:
        return QStringLiteral("blocked");
    case FunctionFlowNodeState::Cancelled:
        return QStringLiteral("cancelled");
    }
    return QStringLiteral("failed");
}

bool terminalState(FunctionFlowNodeState state)
{
    return state == FunctionFlowNodeState::Succeeded
        || state == FunctionFlowNodeState::Skipped
        || state == FunctionFlowNodeState::Failed
        || state == FunctionFlowNodeState::Blocked
        || state == FunctionFlowNodeState::Cancelled;
}

bool isSource(FunctionFlowNodeType type)
{
    return type == FunctionFlowNodeType::VoiceSource
        || type == FunctionFlowNodeType::SelectionSource
        || type == FunctionFlowNodeType::ScreenshotSource;
}

QString ocrEngineId(OcrEngine engine)
{
    if (engine == OcrEngine::RapidOcr) {
        return QStringLiteral("rapid");
    }
    if (engine == OcrEngine::WindowsOcr) {
        return QStringLiteral("windows");
    }
    if (engine == OcrEngine::CustomCloud) {
        return QStringLiteral("customCloud");
    }
    if (engine == OcrEngine::VisionModel) {
        return QStringLiteral("vision");
    }
    return QStringLiteral("automatic");
}

bool valueHasText(const FunctionFlowValue &value)
{
    return !value.text.trimmed().isEmpty();
}

bool valueHasMaterial(const FunctionFlowValue &value)
{
    return valueHasText(value)
        || !value.voice.isNull()
        || !value.screenshot.isNull();
}

QString joinedText(const QList<FunctionFlowValue> &values)
{
    QStringList parts;
    for (const FunctionFlowValue &value : values) {
        const QString text = value.text.trimmed();
        if (!text.isEmpty()) {
            parts.append(text);
        }
    }
    return parts.join(QStringLiteral("\n\n"));
}

} // namespace

FunctionFlowExecutionController::FunctionFlowExecutionController(
    const FunctionFlowRuntimeAccess &access,
    QObject *parent)
    : FunctionFlowExecutionController(
        access,
        FunctionFlowExecutionOptions(),
        parent
    )
{
}

FunctionFlowExecutionController::FunctionFlowExecutionController(
    const FunctionFlowRuntimeAccess &access,
    const FunctionFlowExecutionOptions &options,
    QObject *parent)
    : QObject(parent),
      m_access(access),
      m_options(options)
{
    qRegisterMetaType<FunctionFlowNodeExecutionEvent>(
        "FunctionFlowNodeExecutionEvent"
    );
    qRegisterMetaType<FunctionFlowRunExecutionEvent>(
        "FunctionFlowRunExecutionEvent"
    );
}

FunctionFlowExecutionController::~FunctionFlowExecutionController()
{
}

FunctionFlowStartOutcome
FunctionFlowExecutionController::start(
    const QString &functionId,
    const FunctionFlowExecutionPlan &plan,
    FunctionFlowTrigger trigger,
    FunctionFlowTargetWindowHandle targetWindow)
{
    m_lastStartError = OperationError();
    if (m_running) {
        return FunctionFlowStartOutcome::Busy;
    }
    FunctionFlowExecutionPlan effectivePlan = plan;
    if (effectivePlan.functionId.trimmed().isEmpty()) {
        effectivePlan.functionId = functionId;
    }
    const FunctionFlowTriggerPlan triggerPlan =
        effectivePlan.triggers.value(trigger);
    if (!triggerPlan.available) {
        m_lastStartError = executionError(
            QStringLiteral("flow_trigger_not_configured"),
            QString::fromUtf8("当前画布未配置此入口。")
        );
        return FunctionFlowStartOutcome::ConfigurationError;
    }
    if (!m_access.resolveDependencies) {
        m_lastStartError = executionError(
            QStringLiteral("flow_runtime_adapter_missing")
        );
        return FunctionFlowStartOutcome::ConfigurationError;
    }

    QSharedPointer<const FunctionFlowResolvedDependencies> resolved;
    OperationError resolveError;
    if (!m_access.resolveDependencies(
            effectivePlan,
            trigger,
            targetWindow,
            &resolved,
            &resolveError)) {
        m_lastStartError = resolveError;
        return resolveError.code
                == QStringLiteral("flow_target_window_unavailable")
            ? FunctionFlowStartOutcome::TargetUnavailable
            : FunctionFlowStartOutcome::ConfigurationError;
    }
    if (resolved.isNull()) {
        m_lastStartError = executionError(
            QStringLiteral("flow_dependency_resolution_failed")
        );
        return FunctionFlowStartOutcome::ConfigurationError;
    }

    ++m_generation;
    m_plan = effectivePlan;
    m_dependencies =
        QSharedPointer<const FunctionFlowResolvedDependencies>(
            new FunctionFlowResolvedDependencies(*resolved)
        );
    m_cancellation = CancellationSource();
    m_context = FunctionFlowRunContext();
    m_context.runId = m_cancellation.executionId();
    m_context.functionId = functionId;
    m_context.publishedRevision = m_plan.publishedRevision;
    m_context.publishedHash = m_plan.publishedHash;
    m_context.trigger = trigger;
    m_context.targetWindow = targetWindow;
    m_context.cancellation = m_cancellation.token();
    m_context.dependencies = m_dependencies;
    m_scheduler.reset(new FunctionFlowScheduler(m_plan, trigger));

    m_currentNodeId.clear();
    m_acceptedCompletions.clear();
    m_lastObservedStates.clear();
    m_nodeStartedMs.clear();
    m_tracedNodeIds.clear();
    m_nodeValues.clear();
    m_historyObservations.clear();
    m_traces.clear();
    m_canonicalInput.clear();
    m_collectedSelection = false;
    m_historyDetailPath.clear();
    m_pumpScheduled = false;
    m_finalized = false;
    m_running = true;
    m_runTimer.start();
    for (auto it = m_plan.nodes.constBegin();
         it != m_plan.nodes.constEnd();
         ++it) {
        m_lastObservedStates.insert(
            it.key(),
            FunctionFlowNodeState::Pending
        );
    }

    FunctionFlowRunExecutionEvent started;
    started.runId = m_context.runId;
    started.functionId = m_context.functionId;
    started.publishedRevision = m_context.publishedRevision;
    started.publishedHash = m_context.publishedHash;
    started.trigger = m_context.trigger;
    started.running = true;
    Q_EMIT runExecutionChanged(started);

    emitChangedNodeStates();
    schedulePump();
    return FunctionFlowStartOutcome::Started;
}

FunctionFlowStartOutcome
FunctionFlowExecutionController::start(
    const FunctionFlowTriggerRequest &request,
    const QSharedPointer<const FunctionFlowExecutionPlan> &plan)
{
    m_lastStartError = OperationError();
    if (m_running) {
        const bool sameTrigger =
            m_context.functionId == request.functionId.trimmed()
            && m_context.trigger == request.trigger;
        const bool holdToTalk =
            m_plan.triggers.value(m_context.trigger)
                .usesHoldToTalk;
        if (sameTrigger && !holdToTalk) {
            cancel();
            return FunctionFlowStartOutcome::CancelledExisting;
        }
        return FunctionFlowStartOutcome::Busy;
    }
    if (request.classicWorkflowBusy) {
        return FunctionFlowStartOutcome::Busy;
    }
    if (plan.isNull()) {
        m_lastStartError = executionError(
            QStringLiteral("flow_published_unavailable"),
            QString::fromUtf8(
                "当前画布没有可运行的发布流程。"
            )
        );
        return FunctionFlowStartOutcome::ConfigurationError;
    }
    return start(
        request.functionId,
        *plan,
        request.trigger,
        request.targetWindow
    );
}

void FunctionFlowExecutionController::cancel()
{
    if (!m_running || !m_scheduler) {
        return;
    }
    const int generation = m_generation;
    const QString runningNode = m_currentNodeId;
    m_cancellation.cancel();
    m_scheduler->cancel();
    emitChangedNodeStates();

    if (!runningNode.isEmpty()
        && m_scheduler->state(runningNode)
            == FunctionFlowNodeState::Cancelling) {
        const int graceMs =
            qMax(0, m_options.cancellationGraceMs);
        QTimer::singleShot(
            graceMs,
            this,
            [this, generation, runningNode]() {
                if (!m_running
                    || generation != m_generation
                    || !m_scheduler
                    || m_scheduler->state(runningNode)
                        != FunctionFlowNodeState::Cancelling) {
                    return;
                }
                m_scheduler->completeCancelled(runningNode);
                m_currentNodeId.clear();
                emitChangedNodeStates();
                if (m_scheduler->finished()) {
                    finalizeRun();
                }
            }
        );
        return;
    }
    if (m_scheduler->finished()) {
        finalizeRun();
    }
}

bool FunctionFlowExecutionController::cancel(
    const ExecutionId &runId)
{
    if (!m_running || runId != m_context.runId) {
        return false;
    }
    cancel();
    return true;
}

bool FunctionFlowExecutionController::isRunning() const
{
    return m_running;
}

OperationError
FunctionFlowExecutionController::lastStartError() const
{
    return m_lastStartError;
}

void FunctionFlowExecutionController::editableSurfaceOpened(
    const ExecutionId &runId)
{
    const QString key = runKey(runId);
    if (key.isEmpty()) {
        return;
    }
    if (!m_editableRuns.contains(key)) {
        if (runId != m_context.runId) {
            return;
        }
        EditableRunState state;
        state.runId = runId;
        state.recordDirectory = m_dependencies
            ? m_dependencies->recordDirectory
            : QString();
        m_editableRuns.insert(key, state);
    }
    ++m_editableRuns[key].openSurfaceCount;
}

void FunctionFlowExecutionController::editedTextCommitted(
    const ExecutionId &runId,
    const QString &editedText)
{
    const QString key = runKey(runId);
    if (key.isEmpty() || !m_editableRuns.contains(key)) {
        return;
    }
    EditableRunState &state = m_editableRuns[key];
    state.pendingEditedText = editedText;
    if (!state.finalized || state.detailPath.trimmed().isEmpty()
        || !m_access.updateHistoryEditedText) {
        return;
    }

    FunctionFlowHistoryEditRequest request;
    request.runId = state.runId;
    request.recordDirectory = state.recordDirectory;
    request.detailPath = state.detailPath;
    request.editedText = editedText;
    m_access.updateHistoryEditedText(request);
}

void FunctionFlowExecutionController::editableSurfaceClosed(
    const ExecutionId &runId)
{
    const QString key = runKey(runId);
    if (key.isEmpty() || !m_editableRuns.contains(key)) {
        return;
    }
    EditableRunState &state = m_editableRuns[key];
    state.openSurfaceCount =
        qMax(0, state.openSurfaceCount - 1);
    if (state.finalized && state.openSurfaceCount == 0) {
        m_editableRuns.remove(key);
    }
}

bool FunctionFlowExecutionController::event(QEvent *event)
{
    if (event && event->type() == completionEventType()) {
        FunctionFlowCompletionEvent *completion =
            static_cast<FunctionFlowCompletionEvent *>(event);
        handleCompletion(
            completion->generation,
            completion->runId,
            completion->nodeId,
            completion->result
        );
        return true;
    }
    return QObject::event(event);
}

void FunctionFlowExecutionController::schedulePump()
{
    if (!m_running || m_pumpScheduled) {
        return;
    }
    m_pumpScheduled = true;
    const int generation = m_generation;
    QTimer::singleShot(0, this, [this, generation]() {
        if (generation != m_generation) {
            return;
        }
        m_pumpScheduled = false;
        pump(generation);
    });
}

void FunctionFlowExecutionController::pump(int generation)
{
    if (!m_running
        || generation != m_generation
        || !m_scheduler) {
        return;
    }
    if (m_scheduler->finished()) {
        finalizeRun();
        return;
    }

    const QString nodeId = m_scheduler->nextReadyNode();
    emitChangedNodeStates();
    if (nodeId.isEmpty()) {
        if (m_scheduler->finished()) {
            finalizeRun();
        }
        return;
    }

    OperationError startError;
    if (!m_scheduler->start(nodeId, &startError)) {
        m_scheduler->fail(nodeId, startError);
        emitChangedNodeStates();
        finalizeRun();
        return;
    }
    m_currentNodeId = nodeId;
    m_nodeStartedMs.insert(nodeId, m_runTimer.elapsed());
    emitChangedNodeStates();
    dispatchNode(generation, nodeId);
}

void FunctionFlowExecutionController::dispatchNode(
    int generation,
    const QString &nodeId)
{
    if (!m_running
        || generation != m_generation
        || !m_plan.nodes.contains(nodeId)) {
        return;
    }
    const FunctionFlowCompiledNode node =
        m_plan.nodes.value(nodeId);
    if (node.type == FunctionFlowNodeType::Input
        || node.type == FunctionFlowNodeType::Output) {
        completeInternalNode(nodeId);
        return;
    }

    const FunctionFlowNodeCompletion completion =
        completionFor(
            generation,
            m_context.runId,
            nodeId
        );
    if (node.type == FunctionFlowNodeType::VoiceSource) {
        if (m_access.collectVoice) {
            m_access.collectVoice(m_context, node, completion);
        } else {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = executionError(
                QStringLiteral("flow_runtime_adapter_missing")
            );
            completion(result);
        }
        return;
    }
    if (node.type == FunctionFlowNodeType::SelectionSource) {
        if (m_access.collectSelection) {
            m_access.collectSelection(m_context, node, completion);
        } else {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = executionError(
                QStringLiteral("flow_runtime_adapter_missing")
            );
            completion(result);
        }
        return;
    }
    if (node.type == FunctionFlowNodeType::ScreenshotSource) {
        if (m_access.collectScreenshot) {
            m_access.collectScreenshot(m_context, node, completion);
        } else {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = executionError(
                QStringLiteral("flow_runtime_adapter_missing")
            );
            completion(result);
        }
        return;
    }
    if (node.type == FunctionFlowNodeType::Model) {
        const QList<FunctionFlowValue> inputs =
            m_scheduler->inputValues(nodeId);
        if (m_canonicalInput.isEmpty()) {
            m_canonicalInput = joinedText(inputs);
        }
        if (m_access.runModel) {
            m_access.runModel(
                m_context,
                node,
                inputs,
                completion
            );
        } else {
            FunctionFlowNodeResult result;
            result.state = FunctionFlowNodeState::Failed;
            result.error = executionError(
                QStringLiteral("flow_runtime_adapter_missing")
            );
            completion(result);
        }
        return;
    }

    if (m_access.runResultAction) {
        const QList<FunctionFlowValue> values =
            m_scheduler->inputValues(nodeId);
        FunctionFlowResultActionRequest request;
        if (!values.isEmpty()) {
            request.output = values.first();
        }
        request.canonicalInput = canonicalInput();
        request.collectedSelection = m_collectedSelection;
        m_access.runResultAction(
            m_context,
            node,
            request,
            completion
        );
    } else {
        FunctionFlowNodeResult result;
        result.state = FunctionFlowNodeState::Failed;
        result.error = executionError(
            QStringLiteral("flow_runtime_adapter_missing")
        );
        completion(result);
    }
}

FunctionFlowNodeCompletion
FunctionFlowExecutionController::completionFor(
    int generation,
    const ExecutionId &runId,
    const QString &nodeId)
{
    QPointer<FunctionFlowExecutionController> guard(this);
    return [guard, generation, runId, nodeId](
        const FunctionFlowNodeResult &result) {
        if (!guard) {
            return;
        }
        QCoreApplication::postEvent(
            guard.data(),
            new FunctionFlowCompletionEvent(
                generation,
                runId,
                nodeId,
                result
            )
        );
    };
}

void FunctionFlowExecutionController::handleCompletion(
    int generation,
    const ExecutionId &runId,
    const QString &nodeId,
    const FunctionFlowNodeResult &result)
{
    if (!m_running
        || generation != m_generation
        || runId != m_context.runId
        || !m_scheduler
        || m_currentNodeId != nodeId
        || m_acceptedCompletions.contains(nodeId)) {
        return;
    }
    const FunctionFlowNodeState currentState =
        m_scheduler->state(nodeId);
    if (currentState != FunctionFlowNodeState::Running
        && currentState != FunctionFlowNodeState::Cancelling) {
        return;
    }
    m_acceptedCompletions.insert(nodeId);

    QList<FunctionFlowValue> observations;
    for (FunctionFlowValue observation :
         result.historyObservations) {
        observation.text.clear();
        observation.role.clear();
        observations.append(observation);
    }
    if (!observations.isEmpty()) {
        m_historyObservations.insert(nodeId, observations);
    }

    if (m_context.cancellation.isCancellationRequested()
        || currentState == FunctionFlowNodeState::Cancelling) {
        if (currentState == FunctionFlowNodeState::Running) {
            m_scheduler->cancel();
        }
        if (m_scheduler->state(nodeId)
            == FunctionFlowNodeState::Cancelling) {
            m_scheduler->completeCancelled(nodeId);
        }
        m_currentNodeId.clear();
        emitChangedNodeStates();
        if (m_scheduler->finished()) {
            finalizeRun();
        }
        return;
    }

    if (result.state == FunctionFlowNodeState::Succeeded) {
        m_scheduler->succeed(nodeId, result.values);
        if (m_scheduler->state(nodeId)
            == FunctionFlowNodeState::Succeeded) {
            m_nodeValues.insert(
                nodeId,
                effectiveNodeValues(nodeId, result.values)
            );
            if (m_plan.nodes.value(nodeId).type
                == FunctionFlowNodeType::SelectionSource) {
                for (const FunctionFlowValue &value :
                     result.values) {
                    m_collectedSelection =
                        m_collectedSelection
                        || valueHasText(value);
                }
            }
        }
    } else if (result.state == FunctionFlowNodeState::Skipped) {
        m_scheduler->skip(nodeId);
    } else if (result.state == FunctionFlowNodeState::Cancelled) {
        m_cancellation.cancel();
        m_scheduler->cancel();
        if (m_scheduler->state(nodeId)
            == FunctionFlowNodeState::Cancelling) {
            m_scheduler->completeCancelled(nodeId);
        }
    } else if (result.state == FunctionFlowNodeState::Failed) {
        m_scheduler->fail(nodeId, result.error);
    } else {
        m_scheduler->fail(
            nodeId,
            executionError(
                QStringLiteral("flow_node_result_invalid"),
                QStringLiteral("节点返回了无效的完成状态。")
            )
        );
    }

    m_currentNodeId.clear();
    emitChangedNodeStates();
    if (m_scheduler->finished()) {
        finalizeRun();
    } else {
        schedulePump();
    }
}

void FunctionFlowExecutionController::completeInternalNode(
    const QString &nodeId)
{
    const QList<FunctionFlowValue> values =
        m_scheduler->inputValues(nodeId);
    m_scheduler->succeed(nodeId, values);
    if (m_scheduler->state(nodeId)
        == FunctionFlowNodeState::Succeeded) {
        m_nodeValues.insert(
            nodeId,
            effectiveNodeValues(nodeId, values)
        );
    }
    m_currentNodeId.clear();
    emitChangedNodeStates();
    if (m_scheduler->finished()) {
        finalizeRun();
    } else {
        schedulePump();
    }
}

void FunctionFlowExecutionController::emitChangedNodeStates()
{
    if (!m_scheduler) {
        return;
    }
    QStringList nodeIds = m_plan.topologicalNodeIds;
    for (auto it = m_plan.nodes.constBegin();
         it != m_plan.nodes.constEnd();
         ++it) {
        if (!nodeIds.contains(it.key())) {
            nodeIds.append(it.key());
        }
    }

    for (const QString &nodeId : nodeIds) {
        if (!m_plan.nodes.contains(nodeId)) {
            continue;
        }
        const FunctionFlowNodeState current =
            m_scheduler->state(nodeId);
        if (m_lastObservedStates.value(
                nodeId,
                FunctionFlowNodeState::Pending
            ) == current) {
            continue;
        }
        m_lastObservedStates[nodeId] = current;

        qint64 elapsedMs = -1;
        if (m_nodeStartedMs.contains(nodeId)) {
            elapsedMs = qMax(
                qint64(0),
                m_runTimer.elapsed()
                    - m_nodeStartedMs.value(nodeId)
            );
        }
        QString errorCode;
        if (current == FunctionFlowNodeState::Failed) {
            errorCode = m_scheduler->terminalError().code;
        } else if (current == FunctionFlowNodeState::Cancelled
                   || current
                       == FunctionFlowNodeState::Cancelling) {
            errorCode = QStringLiteral("flow_cancelled");
        }

        const FunctionFlowCompiledNode &node =
            m_plan.nodes.value(nodeId);
        FunctionFlowNodeExecutionEvent event;
        event.runId = m_context.runId;
        event.functionId = m_context.functionId;
        event.publishedRevision = m_context.publishedRevision;
        event.publishedHash = m_context.publishedHash;
        event.trigger = m_context.trigger;
        event.nodeId = nodeId;
        event.nodeType = node.type;
        event.state = current;
        event.elapsedMs = elapsedMs;
        event.errorCode = errorCode;
        if (m_dependencies
            && m_dependencies->byNodeId.contains(nodeId)) {
            const FunctionFlowResolvedNodeSettings settings =
                m_dependencies->byNodeId.value(nodeId);
            event.modelId = settings.modelId;
            event.promptVersion = settings.promptVersion;
        }
        Q_EMIT nodeExecutionChanged(event);

        if (terminalState(current)
            && !m_tracedNodeIds.contains(nodeId)) {
            m_tracedNodeIds.insert(nodeId);
            FunctionFlowNodeTrace trace;
            trace.nodeId = nodeId;
            trace.nodeType = functionFlowNodeTypeId(node.type);
            trace.state = nodeStateId(current);
            trace.elapsedMs = elapsedMs;
            trace.errorCode = errorCode;
            if (m_dependencies
                && m_dependencies->byNodeId.contains(nodeId)) {
                const FunctionFlowResolvedNodeSettings settings =
                    m_dependencies->byNodeId.value(nodeId);
                trace.modelId = settings.modelId;
                trace.promptVersion = settings.promptVersion;
            }
            m_traces.append(trace);
        }
    }
}

void FunctionFlowExecutionController::finalizeRun()
{
    if (!m_running || m_finalized || !m_scheduler) {
        return;
    }
    m_finalized = true;
    OperationError finalError = m_scheduler->terminalError();

    FunctionFlowHistorySaveResult saveResult;
    bool attemptedHistorySave = false;
    if (finalError.isEmpty() || hasHistoryMaterial()) {
        attemptedHistorySave = true;
        if (m_access.saveHistory) {
            saveResult = m_access.saveHistory(historyRequest());
        } else {
            saveResult.error = executionError(
                QStringLiteral("flow_history_save_failed")
            );
        }
        if (saveResult.ok || saveResult.alreadyExists) {
            m_historyDetailPath = saveResult.detailPath;
        } else {
            finalError = executionError(
                QStringLiteral("flow_history_save_failed"),
                QStringLiteral("流程历史保存失败。"),
                saveResult.error.code
            );
        }
    }
    if (attemptedHistorySave) {
        finalizeEditableRunState(saveResult);
    } else {
        FunctionFlowHistorySaveResult emptyResult;
        finalizeEditableRunState(emptyResult);
    }

    m_running = false;
    m_currentNodeId.clear();
    FunctionFlowRunExecutionEvent terminal;
    terminal.runId = m_context.runId;
    terminal.functionId = m_context.functionId;
    terminal.publishedRevision = m_context.publishedRevision;
    terminal.publishedHash = m_context.publishedHash;
    terminal.trigger = m_context.trigger;
    terminal.running = false;
    terminal.cancelled =
        m_context.cancellation.isCancellationRequested()
        || m_scheduler->terminalError().code
            == QStringLiteral("flow_cancelled");
    terminal.terminalError = finalError;
    Q_EMIT runExecutionChanged(terminal);
}

FunctionFlowHistoryRequest
FunctionFlowExecutionController::historyRequest() const
{
    FunctionFlowHistoryRequest request;
    request.runId = m_context.runId;
    request.functionId = m_context.functionId;
    request.functionTitle = m_dependencies
        ? m_dependencies->functionTitle
        : QString();
    request.recordDirectory = m_dependencies
        ? m_dependencies->recordDirectory
        : QString();
    request.publishedRevision = m_context.publishedRevision;
    request.publishedHash = m_context.publishedHash;
    request.trigger = functionFlowTriggerId(m_context.trigger);
    request.canonicalInput = canonicalInput();
    const QString editableKey = runKey(m_context.runId);
    request.pendingEditedText =
        m_editableRuns.value(editableKey).pendingEditedText;
    request.terminalError = m_scheduler
        ? m_scheduler->terminalError()
        : OperationError();
    request.traces = m_traces;
    request.cancelled =
        m_context.cancellation.isCancellationRequested()
        || request.terminalError.code
            == QStringLiteral("flow_cancelled");

    for (const FunctionFlowNodeTrace &trace : m_traces) {
        if (trace.state == QStringLiteral("failed")) {
            request.failedNodeId = trace.nodeId;
            request.failedNodeType = trace.nodeType;
            break;
        }
    }

    QStringList nodeIds = m_plan.topologicalNodeIds;
    for (auto it = m_historyObservations.constBegin();
         it != m_historyObservations.constEnd();
         ++it) {
        if (!nodeIds.contains(it.key())) {
            nodeIds.append(it.key());
        }
    }
    for (const QString &nodeId : nodeIds) {
        QList<FunctionFlowValue> values =
            m_nodeValues.value(nodeId);
        values.append(m_historyObservations.value(nodeId));
        for (const FunctionFlowValue &value : values) {
            if (!value.voice.isNull()
                && request.sourceAudioPath.isEmpty()
                && request.recordingSegments.isEmpty()) {
                request.sourceAudioPath =
                    value.voice->sourceAudioPath;
                request.recordingSegments =
                    value.voice->segments;
                request.speechElapsedMs =
                    value.voice->speechElapsedMs;
                request.recordingTriggerMode =
                    value.voice->recordingTriggerMode;
                request.longRecording =
                    value.voice->longRecording;
            }
            if (!value.screenshot.isNull()
                && request.ocrEngineId.isEmpty()) {
                request.ocrEngineId =
                    ocrEngineId(value.screenshot->engine);
                request.ocrElapsedMs =
                    value.screenshot->elapsedMs;
                request.ocrUsedFallback =
                    value.screenshot->usedFallback;
                request.screenshotRect =
                    value.screenshot->rect;
            }
        }
    }

    for (auto it = m_plan.nodes.constBegin();
         it != m_plan.nodes.constEnd();
         ++it) {
        if (it.value().type != FunctionFlowNodeType::Output) {
            continue;
        }
        const QString text =
            joinedText(m_nodeValues.value(it.key()));
        if (!text.isEmpty()) {
            request.finalOutput = text;
            break;
        }
    }
    return request;
}

bool FunctionFlowExecutionController::hasHistoryMaterial() const
{
    for (auto it = m_nodeValues.constBegin();
         it != m_nodeValues.constEnd();
         ++it) {
        if (!m_plan.nodes.contains(it.key())) {
            continue;
        }
        const FunctionFlowNodeType type =
            m_plan.nodes.value(it.key()).type;
        if (!isSource(type)
            && type != FunctionFlowNodeType::Output) {
            continue;
        }
        for (const FunctionFlowValue &value : it.value()) {
            if (valueHasMaterial(value)) {
                return true;
            }
        }
    }
    for (auto it = m_historyObservations.constBegin();
         it != m_historyObservations.constEnd();
         ++it) {
        for (const FunctionFlowValue &value : it.value()) {
            if (!value.voice.isNull()
                || !value.screenshot.isNull()) {
                return true;
            }
        }
    }
    return false;
}

QString FunctionFlowExecutionController::canonicalInput() const
{
    if (!m_canonicalInput.trimmed().isEmpty()) {
        return m_canonicalInput;
    }
    QStringList parts;
    for (const QString &nodeId : m_plan.topologicalNodeIds) {
        if (!m_plan.nodes.contains(nodeId)
            || !isSource(m_plan.nodes.value(nodeId).type)) {
            continue;
        }
        const QString text =
            joinedText(m_nodeValues.value(nodeId));
        if (!text.isEmpty()) {
            parts.append(text);
        }
    }
    return parts.join(QStringLiteral("\n\n"));
}

QString FunctionFlowExecutionController::runKey(
    const ExecutionId &runId) const
{
    return runId.value.trimmed();
}

void FunctionFlowExecutionController::finalizeEditableRunState(
    const FunctionFlowHistorySaveResult &saveResult)
{
    const QString key = runKey(m_context.runId);
    if (key.isEmpty()) {
        return;
    }
    if (!m_editableRuns.contains(key)) {
        EditableRunState state;
        state.runId = m_context.runId;
        state.recordDirectory = m_dependencies
            ? m_dependencies->recordDirectory
            : QString();
        m_editableRuns.insert(key, state);
    }
    EditableRunState &state = m_editableRuns[key];
    state.finalized = true;
    if (saveResult.ok || saveResult.alreadyExists) {
        state.detailPath = saveResult.detailPath;
    }
    if (state.openSurfaceCount == 0) {
        m_editableRuns.remove(key);
    }
}

QList<FunctionFlowValue>
FunctionFlowExecutionController::effectiveNodeValues(
    const QString &nodeId,
    const QList<FunctionFlowValue> &fallback) const
{
    if (!m_plan.nodes.contains(nodeId) || !m_scheduler) {
        return fallback;
    }
    const QStringList successors =
        m_plan.nodes.value(nodeId).successors;
    for (const QString &successor : successors) {
        const QList<FunctionFlowValue> values =
            m_scheduler->inputValues(successor);
        if (!values.isEmpty()) {
            return values;
        }
    }
    return fallback;
}
