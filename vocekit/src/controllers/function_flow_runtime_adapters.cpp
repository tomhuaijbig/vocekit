#include "function_flow_runtime_adapters.h"

#include "../config/app_settings_defaults.h"
#include "../domain/function_flow_model_message.h"
#include "../providers/model_catalog.h"
#include "../result_flow_config.h"

#include <QCryptographicHash>
#include <QSet>

namespace {

OperationError adapterError(
    const QString &code,
    const QString &message = QString())
{
    OperationError error;
    error.code = code;
    error.message = message;
    return error;
}

QString promptVersion(const QString &prompt)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(
            prompt.toUtf8(),
            QCryptographicHash::Sha256
        ).toHex().left(12)
    );
}

QStringList defaultModelIds()
{
    QStringList ids;
    for (const ModelOption &option : modelOptions()) {
        ids.append(option.id);
    }
    return ids;
}

QSet<QString> activeNodeIds(
    const FunctionFlowExecutionPlan &plan,
    FunctionFlowTrigger trigger)
{
    QSet<QString> active;
    QStringList pending =
        plan.triggers.value(trigger).activeSourceNodeIds;
    while (!pending.isEmpty()) {
        const QString nodeId = pending.takeFirst();
        if (active.contains(nodeId)
            || !plan.nodes.contains(nodeId)) {
            continue;
        }
        active.insert(nodeId);
        pending.append(plan.nodes.value(nodeId).successors);
    }
    return active;
}

bool requiresExternalTarget(
    const FunctionFlowExecutionPlan &plan,
    const QSet<QString> &active)
{
    for (const QString &nodeId : active) {
        const FunctionFlowNodeType type =
            plan.nodes.value(nodeId).type;
        if (type == FunctionFlowNodeType::SelectionSource
            || type == FunctionFlowNodeType::AutoWrite) {
            return true;
        }
    }
    return false;
}

QString exactPromptText(
    const PromptRuntimeSnapshot &snapshot,
    const QString &promptId,
    bool *found)
{
    if (found) {
        *found = false;
    }
    for (const PromptTargetInfo &target :
         promptRuntimeTargets(snapshot)) {
        if (target.id != promptId) {
            continue;
        }
        if (found) {
            *found = true;
        }
        return promptRuntimeText(snapshot, target);
    }
    return QString();
}

} // namespace

FunctionFlowRuntimeAdapters::FunctionFlowRuntimeAdapters(
    const FunctionFlowRuntimeAdapterAccess &access,
    FunctionFlowModelTaskRunner *modelRunner,
    QObject *parent)
    : QObject(parent),
      m_access(access),
      m_modelRunner(modelRunner)
{
    if (!m_modelRunner) {
        m_ownedModelRunner.reset(
            new FunctionFlowModelTaskRunner(
                FunctionFlowModelTaskRunnerAccess(),
                this
            )
        );
        m_modelRunner = m_ownedModelRunner.data();
    }
    m_modelRunner->deltaCallback =
        [this](
            const ExecutionId &runId,
            const QString &nodeId,
            const QString &delta) {
            handleModelDelta(runId, nodeId, delta);
        };
    m_modelRunner->finishedCallback =
        [this](
            const ExecutionId &runId,
            const QString &nodeId,
            const ModelRequestTaskResult &result) {
            handleModelFinished(runId, nodeId, result);
        };
}

FunctionFlowRuntimeAdapters::~FunctionFlowRuntimeAdapters()
{
    if (m_modelRunner) {
        m_modelRunner->cancel();
        m_modelRunner->deltaCallback = nullptr;
        m_modelRunner->finishedCallback = nullptr;
    }
}

FunctionFlowRuntimeAccess
FunctionFlowRuntimeAdapters::runtimeAccess()
{
    FunctionFlowRuntimeAccess access;
    access.resolveDependencies =
        [this](
            const FunctionFlowExecutionPlan &plan,
            FunctionFlowTrigger trigger,
            FunctionFlowTargetWindowHandle target,
            QSharedPointer<const FunctionFlowResolvedDependencies> *resolved,
            OperationError *error) {
            return resolveDependencies(
                plan,
                trigger,
                target,
                resolved,
                error
            );
        };
    access.collectVoice = m_access.collectVoice;
    access.collectSelection =
        [this](
            const FunctionFlowRunContext &run,
            const FunctionFlowCompiledNode &node,
            const FunctionFlowNodeCompletion &completion) {
            collectSelection(run, node, completion);
        };
    access.collectScreenshot = m_access.collectScreenshot;
    access.runModel =
        [this](
            const FunctionFlowRunContext &run,
            const FunctionFlowCompiledNode &node,
            const QList<FunctionFlowValue> &inputs,
            const FunctionFlowNodeCompletion &completion) {
            runModel(run, node, inputs, completion);
        };
    access.runResultAction = m_access.runResultAction;
    access.saveHistory = m_access.saveHistory;
    access.updateHistoryEditedText = m_access.updateHistoryEditedText;
    return access;
}

bool FunctionFlowRuntimeAdapters::resolveDependencies(
    const FunctionFlowExecutionPlan &plan,
    FunctionFlowTrigger trigger,
    FunctionFlowTargetWindowHandle targetWindow,
    QSharedPointer<const FunctionFlowResolvedDependencies> *resolved,
    OperationError *error) const
{
    if (resolved) {
        resolved->clear();
    }
    if (error) {
        *error = OperationError();
    }
    if (!resolved || !m_access.runtimeSnapshot) {
        if (error) {
            *error = adapterError(
                QStringLiteral("flow_dependency_resolution_failed")
            );
        }
        return false;
    }

    const QSet<QString> active = activeNodeIds(plan, trigger);
    if (active.isEmpty()) {
        if (error) {
            *error = adapterError(
                QStringLiteral("flow_trigger_unavailable")
            );
        }
        return false;
    }
    if (requiresExternalTarget(plan, active)
        && (!m_access.isUsableExternalTargetWindow
            || !m_access.isUsableExternalTargetWindow(
                targetWindow))) {
        if (error) {
            *error = adapterError(
                QStringLiteral("flow_target_window_unavailable"),
                QString::fromUtf8("请先切换到目标应用。")
            );
        }
        return false;
    }

    const PromptRuntimeSnapshot snapshot =
        m_access.runtimeSnapshot();
    const AppSettingsData &settings = snapshot.settings;
    const QStringList modelIds = m_access.availableModelIds
        ? m_access.availableModelIds()
        : defaultModelIds();

    QSharedPointer<FunctionFlowResolvedDependencies> result(
        new FunctionFlowResolvedDependencies
    );
    const FunctionSettings function =
        settings.function(plan.functionId);
    result->functionTitle =
        function.name.trimmed().isEmpty()
            ? plan.functionId
            : function.name.trimmed();
    result->recordDirectory =
        m_access.resolveRecordDirectory
            ? m_access.resolveRecordDirectory(
                settings.recordDirectory
            )
            : settings.recordDirectory;
    result->inheritedResultPopupOpacity =
        settings.resultPopupOpacity;
    result->inheritedScreenshotPanelOpacity =
        settings.windows.screenshotResultOpacity;
    result->hasResultPopupGeometry =
        settings.windows.hasResultPopupGeometry;
    result->resultPopupGeometry =
        settings.windows.resultPopupGeometry;
    result->hasScreenshotPanelGeometry =
        settings.windows.hasScreenshotResultGeometry;
    result->screenshotPanelGeometry =
        settings.windows.screenshotResultGeometry;

    for (const QString &nodeId : active) {
        const FunctionFlowCompiledNode node =
            plan.nodes.value(nodeId);
        result->nodeConfigs.insert(nodeId, node.config);
        FunctionFlowResolvedNodeSettings nodeSettings;

        if (node.type == FunctionFlowNodeType::Model) {
            const QString configuredModelId =
                node.config.model.modelId;
            const QString normalizedModelId =
                configuredModelId.trimmed().isEmpty()
                    ? configuredModelId
                    : normalizeModelId(
                        configuredModelId,
                        configuredModelId.trimmed()
                    );
            if (!modelIds.contains(normalizedModelId)) {
                if (error) {
                    *error = adapterError(
                        QStringLiteral(
                            "flow_model_reference_missing"
                        )
                    );
                }
                return false;
            }
            bool promptFound = false;
            const QString systemPrompt = exactPromptText(
                snapshot,
                node.config.model.promptId,
                &promptFound
            );
            if (!promptFound) {
                if (error) {
                    *error = adapterError(
                        QStringLiteral(
                            "flow_prompt_reference_missing"
                        )
                    );
                }
                return false;
            }
            nodeSettings.modelId = normalizedModelId;
            nodeSettings.sampling = node.config.model.sampling;
            nodeSettings.systemPrompt = systemPrompt;
            nodeSettings.promptVersion =
                promptVersion(systemPrompt);
            nodeSettings.effectiveNetworkPolicy =
                resolveNetworkPolicy(
                    node.config.model.networkPolicy,
                    settings.useSystemProxy
                );
        } else if (
            node.type == FunctionFlowNodeType::VoiceSource) {
            nodeSettings.speechProviderId =
                node.config.voice.speechProviderId
                    .trimmed().isEmpty()
                    ? normalizeSpeechProvider(
                        settings.speechProvider
                    )
                    : node.config.voice.speechProviderId;
            if (!supportedSpeechProviderIds().contains(
                    nodeSettings.speechProviderId)) {
                if (error) {
                    *error = adapterError(
                        QStringLiteral(
                            "flow_speech_provider_reference_missing"
                        )
                    );
                }
                return false;
            }
            if (m_access.speechConfigurationError
                && !m_access.speechConfigurationError(
                        nodeSettings.speechProviderId
                    ).trimmed().isEmpty()) {
                if (error) {
                    *error = adapterError(
                        QStringLiteral(
                            "flow_speech_provider_reference_missing"
                        )
                    );
                }
                return false;
            }
            nodeSettings.effectiveNetworkPolicy =
                resolveNetworkPolicy(
                    node.config.voice.networkPolicy,
                    settings.useSystemProxy
                );
        } else if (
            node.type == FunctionFlowNodeType::ScreenshotSource) {
            nodeSettings.ocrEngineId =
                node.config.screenshot.ocrEngineId
                    .trimmed().isEmpty()
                    ? normalizeOcrEngine(settings.ocrEngine)
                    : node.config.screenshot.ocrEngineId;
            if (!supportedOcrEngineIds().contains(
                    nodeSettings.ocrEngineId)) {
                if (error) {
                    *error = adapterError(
                        QStringLiteral(
                            "flow_ocr_engine_reference_missing"
                        )
                    );
                }
                return false;
            }
            nodeSettings.effectiveNetworkPolicy =
                resolveNetworkPolicy(
                    node.config.screenshot.networkPolicy,
                    settings.useSystemProxy
                );
        } else if (
            node.type == FunctionFlowNodeType::SelectionSource) {
            nodeSettings.strongSelectionEnabled =
                node.config.selection.inheritStrongSelection
                && settings.strongSelectionEnabled;
        }
        result->byNodeId.insert(nodeId, nodeSettings);
    }

    *resolved = result;
    return true;
}

void FunctionFlowRuntimeAdapters::collectSelection(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const FunctionFlowNodeCompletion &completion) const
{
    FunctionFlowNodeResult result;
    if (!completion) {
        return;
    }
    if (run.cancellation.isCancellationRequested()) {
        result.state = FunctionFlowNodeState::Cancelled;
        completion(result);
        return;
    }
    if (!m_access.isUsableExternalTargetWindow
        || !m_access.isUsableExternalTargetWindow(
            run.targetWindow)) {
        result.state = FunctionFlowNodeState::Failed;
        result.error = adapterError(
            QStringLiteral("flow_target_window_unavailable")
        );
        completion(result);
        return;
    }
    if (!m_access.readSelectedText) {
        result.state = FunctionFlowNodeState::Failed;
        result.error = adapterError(
            QStringLiteral("flow_selection_failed")
        );
        completion(result);
        return;
    }

    SelectedTextWorkflowRequest request;
    request.modeId = run.functionId;
    request.targetWindow = run.targetWindow;
    request.suppressMissingPrompt = true;
    if (run.dependencies) {
        request.strongSelectionEnabled =
            run.dependencies->byNodeId.value(node.nodeId)
                .strongSelectionEnabled;
    }
    const SelectedTextWorkflowResult selected =
        m_access.readSelectedText(request);
    if (run.cancellation.isCancellationRequested()) {
        result.state = FunctionFlowNodeState::Cancelled;
        completion(result);
        return;
    }
    if (selected.blocked) {
        result.state = FunctionFlowNodeState::Failed;
        result.error = adapterError(
            QStringLiteral("flow_selection_failed")
        );
        completion(result);
        return;
    }

    FunctionFlowValue value;
    value.text = selected.text;
    value.sourceNodeId = node.nodeId;
    result.state = FunctionFlowNodeState::Succeeded;
    result.values.append(value);
    completion(result);
}

void FunctionFlowRuntimeAdapters::runModel(
    const FunctionFlowRunContext &run,
    const FunctionFlowCompiledNode &node,
    const QList<FunctionFlowValue> &inputs,
    const FunctionFlowNodeCompletion &completion)
{
    if (!completion) {
        return;
    }
    FunctionFlowNodeResult immediate;
    if (run.cancellation.isCancellationRequested()) {
        immediate.state = FunctionFlowNodeState::Cancelled;
        completion(immediate);
        return;
    }
    const QString userPrompt =
        buildFunctionFlowUserPrompt(inputs);
    if (userPrompt.trimmed().isEmpty()) {
        immediate.state = FunctionFlowNodeState::Failed;
        immediate.error = adapterError(
            QStringLiteral("flow_model_input_empty")
        );
        completion(immediate);
        return;
    }
    if (!run.dependencies
        || !run.dependencies->byNodeId.contains(node.nodeId)
        || !m_modelRunner) {
        immediate.state = FunctionFlowNodeState::Failed;
        immediate.error = adapterError(
            QStringLiteral("flow_model_reference_missing")
        );
        completion(immediate);
        return;
    }

    const FunctionFlowResolvedNodeSettings resolved =
        run.dependencies->byNodeId.value(node.nodeId);
    ModelRequestTaskRequest request;
    request.modelId = resolved.modelId;
    request.systemPrompt = resolved.systemPrompt;
    request.userPrompt = userPrompt;
    request.sampling = resolved.sampling;
    request.stream = node.config.model.stream;
    request.networkPolicy =
        resolved.effectiveNetworkPolicy;
    request.useSystemProxy =
        resolved.effectiveNetworkPolicy
            == QStringLiteral("systemProxy");
    request.cancellation = run.cancellation;

    m_pendingModel.run = run;
    m_pendingModel.node = node;
    m_pendingModel.completion = completion;
    m_pendingModel.voice.clear();
    m_pendingModel.screenshot.clear();
    for (const FunctionFlowValue &input : inputs) {
        if (m_pendingModel.voice.isNull()
            && !input.voice.isNull()) {
            m_pendingModel.voice = input.voice;
        }
        if (m_pendingModel.screenshot.isNull()
            && !input.screenshot.isNull()) {
            m_pendingModel.screenshot = input.screenshot;
        }
    }
    m_hasPendingModel = true;
    if (node.config.model.stream
        && !node.streamingResultPopupNodeId.isEmpty()
        && m_access.beginStreamingPreview) {
        m_access.beginStreamingPreview(
            run,
            node.nodeId,
            node.streamingResultPopupNodeId
        );
    }
    m_modelRunner->start(run.runId, node.nodeId, request);
    if (!m_modelRunner->isRunning()) {
        m_hasPendingModel = false;
        immediate.state = FunctionFlowNodeState::Failed;
        immediate.error = adapterError(
            QStringLiteral("flow_model_failed")
        );
        completion(immediate);
    }
}

void FunctionFlowRuntimeAdapters::handleModelDelta(
    const ExecutionId &runId,
    const QString &nodeId,
    const QString &delta)
{
    if (!m_hasPendingModel
        || m_pendingModel.run.runId != runId
        || m_pendingModel.node.nodeId != nodeId
        || m_pendingModel.run.cancellation
            .isCancellationRequested()) {
        return;
    }
    const QString popupNodeId =
        m_pendingModel.node.streamingResultPopupNodeId;
    if (!popupNodeId.isEmpty()
        && m_access.appendStreamingDelta) {
        m_access.appendStreamingDelta(
            runId,
            nodeId,
            popupNodeId,
            delta
        );
    }
}

void FunctionFlowRuntimeAdapters::handleModelFinished(
    const ExecutionId &runId,
    const QString &nodeId,
    const ModelRequestTaskResult &taskResult)
{
    if (!m_hasPendingModel
        || m_pendingModel.run.runId != runId
        || m_pendingModel.node.nodeId != nodeId) {
        return;
    }
    const PendingModel pending = m_pendingModel;
    m_pendingModel = PendingModel();
    m_hasPendingModel = false;

    FunctionFlowNodeResult result;
    const bool cancelled =
        pending.run.cancellation.isCancellationRequested()
        || taskResult.cancelled;
    if (cancelled) {
        result.state = FunctionFlowNodeState::Cancelled;
    } else if (!taskResult.errorMessage.trimmed().isEmpty()) {
        result.state = FunctionFlowNodeState::Failed;
        result.error = adapterError(
            QStringLiteral("flow_model_failed"),
            QString::fromUtf8("模型请求失败。")
        );
    } else {
        FunctionFlowValue value;
        value.text = taskResult.text;
        value.sourceNodeId = nodeId;
        value.voice = pending.voice;
        value.screenshot = pending.screenshot;
        result.values.append(value);
        result.state = FunctionFlowNodeState::Succeeded;
    }

    if (result.state != FunctionFlowNodeState::Succeeded
        && !pending.node.streamingResultPopupNodeId.isEmpty()
        && m_access.abandonStreamingPreview) {
        m_access.abandonStreamingPreview(
            runId,
            nodeId,
            pending.node.streamingResultPopupNodeId
        );
    }
    if (pending.completion) {
        pending.completion(result);
    }
}
