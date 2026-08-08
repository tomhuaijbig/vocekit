#ifndef VOCEKIT_FUNCTION_FLOW_RUNTIME_ADAPTERS_H
#define VOCEKIT_FUNCTION_FLOW_RUNTIME_ADAPTERS_H

#include "function_flow_execution_controller.h"
#include "selected_text_workflow_controller.h"
#include "../domain/prompt_runtime_library.h"
#include "../tasks/function_flow_model_task_runner.h"

#include <QObject>
#include <QScopedPointer>

struct FunctionFlowRuntimeAdapterAccess
{
    std::function<PromptRuntimeSnapshot()> runtimeSnapshot;
    std::function<QStringList()> availableModelIds;
    std::function<QString(const QString &)>
        speechConfigurationError;
    std::function<QString(const QString &)> resolveRecordDirectory;
    std::function<bool(FunctionFlowTargetWindowHandle)>
        isUsableExternalTargetWindow;
    std::function<SelectedTextWorkflowResult(
        const SelectedTextWorkflowRequest &
    )> readSelectedText;

    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowNodeCompletion &
    )> collectVoice;
    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowNodeCompletion &
    )> collectScreenshot;
    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowResultActionRequest &,
        const FunctionFlowNodeCompletion &
    )> runResultAction;

    std::function<void(
        const FunctionFlowRunContext &,
        const QString &,
        const QString &
    )> beginStreamingPreview;
    std::function<void(
        const ExecutionId &,
        const QString &,
        const QString &,
        const QString &
    )> appendStreamingDelta;
    std::function<void(
        const ExecutionId &,
        const QString &,
        const QString &
    )> abandonStreamingPreview;

    std::function<FunctionFlowHistorySaveResult(
        const FunctionFlowHistoryRequest &
    )> saveHistory;
    std::function<FunctionFlowHistoryEditResult(
        const FunctionFlowHistoryEditRequest &
    )> updateHistoryEditedText;
};

class FunctionFlowRuntimeAdapters : public QObject
{
public:
    explicit FunctionFlowRuntimeAdapters(
        const FunctionFlowRuntimeAdapterAccess &access,
        FunctionFlowModelTaskRunner *modelRunner = nullptr,
        QObject *parent = nullptr
    );
    ~FunctionFlowRuntimeAdapters() override;

    FunctionFlowRuntimeAccess runtimeAccess();

    bool resolveDependencies(
        const FunctionFlowExecutionPlan &plan,
        FunctionFlowTrigger trigger,
        FunctionFlowTargetWindowHandle targetWindow,
        QSharedPointer<const FunctionFlowResolvedDependencies> *resolved,
        OperationError *error
    ) const;
    void collectSelection(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const FunctionFlowNodeCompletion &completion
    ) const;
    void runModel(
        const FunctionFlowRunContext &run,
        const FunctionFlowCompiledNode &node,
        const QList<FunctionFlowValue> &inputs,
        const FunctionFlowNodeCompletion &completion
    );

private:
    struct PendingModel
    {
        FunctionFlowRunContext run;
        FunctionFlowCompiledNode node;
        FunctionFlowNodeCompletion completion;
        QSharedPointer<const FunctionFlowVoicePayload> voice;
        QSharedPointer<const FunctionFlowScreenshotPayload>
            screenshot;
    };

    void handleModelDelta(
        const ExecutionId &runId,
        const QString &nodeId,
        const QString &delta
    );
    void handleModelFinished(
        const ExecutionId &runId,
        const QString &nodeId,
        const ModelRequestTaskResult &taskResult
    );

    FunctionFlowRuntimeAdapterAccess m_access;
    QScopedPointer<FunctionFlowModelTaskRunner> m_ownedModelRunner;
    FunctionFlowModelTaskRunner *m_modelRunner = nullptr;
    PendingModel m_pendingModel;
    bool m_hasPendingModel = false;
};

#endif // VOCEKIT_FUNCTION_FLOW_RUNTIME_ADAPTERS_H
