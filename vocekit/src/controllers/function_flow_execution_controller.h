#ifndef VOCEKIT_FUNCTION_FLOW_EXECUTION_CONTROLLER_H
#define VOCEKIT_FUNCTION_FLOW_EXECUTION_CONTROLLER_H

#include "../domain/function_flow_scheduler.h"

#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QScopedPointer>
#include <QSet>
#include <QSharedPointer>

#include <functional>

struct FunctionFlowRuntimeAccess
{
    std::function<bool(
        const FunctionFlowExecutionPlan &,
        FunctionFlowTrigger,
        FunctionFlowTargetWindowHandle,
        QSharedPointer<const FunctionFlowResolvedDependencies> *,
        OperationError *
    )> resolveDependencies;

    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowNodeCompletion &
    )> collectVoice;

    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowNodeCompletion &
    )> collectSelection;

    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowNodeCompletion &
    )> collectScreenshot;

    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const QList<FunctionFlowValue> &,
        const FunctionFlowNodeCompletion &
    )> runModel;

    std::function<void(
        const FunctionFlowRunContext &,
        const FunctionFlowCompiledNode &,
        const FunctionFlowResultActionRequest &,
        const FunctionFlowNodeCompletion &
    )> runResultAction;

    std::function<FunctionFlowHistorySaveResult(
        const FunctionFlowHistoryRequest &
    )> saveHistory;

    std::function<FunctionFlowHistoryEditResult(
        const FunctionFlowHistoryEditRequest &
    )> updateHistoryEditedText;
};

class FunctionFlowExecutionController : public QObject
{
    Q_OBJECT

public:
    explicit FunctionFlowExecutionController(
        const FunctionFlowRuntimeAccess &access,
        QObject *parent = nullptr
    );
    FunctionFlowExecutionController(
        const FunctionFlowRuntimeAccess &access,
        const FunctionFlowExecutionOptions &options,
        QObject *parent = nullptr
    );
    ~FunctionFlowExecutionController() override;

    FunctionFlowStartOutcome start(
        const QString &functionId,
        const FunctionFlowExecutionPlan &plan,
        FunctionFlowTrigger trigger,
        FunctionFlowTargetWindowHandle targetWindow
    );
    FunctionFlowStartOutcome start(
        const FunctionFlowTriggerRequest &request,
        const QSharedPointer<const FunctionFlowExecutionPlan> &plan
    );
    void cancel();
    bool cancel(const ExecutionId &runId);
    bool isRunning() const;
    OperationError lastStartError() const;
    void editableSurfaceOpened(const ExecutionId &runId);
    void editedTextCommitted(
        const ExecutionId &runId,
        const QString &editedText
    );
    void editableSurfaceClosed(const ExecutionId &runId);

signals:
    void nodeExecutionChanged(FunctionFlowNodeExecutionEvent event);
    void runExecutionChanged(FunctionFlowRunExecutionEvent event);

protected:
    bool event(QEvent *event) override;

private:
    void schedulePump();
    void pump(int generation);
    void dispatchNode(
        int generation,
        const QString &nodeId
    );
    FunctionFlowNodeCompletion completionFor(
        int generation,
        const ExecutionId &runId,
        const QString &nodeId
    );
    void handleCompletion(
        int generation,
        const ExecutionId &runId,
        const QString &nodeId,
        const FunctionFlowNodeResult &result
    );
    void completeInternalNode(const QString &nodeId);
    void emitChangedNodeStates();
    void finalizeRun();
    FunctionFlowHistoryRequest historyRequest() const;
    bool hasHistoryMaterial() const;
    QString canonicalInput() const;
    QString runKey(const ExecutionId &runId) const;
    void finalizeEditableRunState(
        const FunctionFlowHistorySaveResult &saveResult
    );
    QList<FunctionFlowValue> effectiveNodeValues(
        const QString &nodeId,
        const QList<FunctionFlowValue> &fallback
    ) const;

    FunctionFlowRuntimeAccess m_access;
    FunctionFlowExecutionOptions m_options;
    int m_generation = 0;
    bool m_running = false;
    bool m_pumpScheduled = false;
    bool m_finalized = false;
    CancellationSource m_cancellation;
    FunctionFlowExecutionPlan m_plan;
    QSharedPointer<const FunctionFlowResolvedDependencies>
        m_dependencies;
    FunctionFlowRunContext m_context;
    QScopedPointer<FunctionFlowScheduler> m_scheduler;
    QString m_currentNodeId;
    QSet<QString> m_acceptedCompletions;
    QMap<QString, FunctionFlowNodeState> m_lastObservedStates;
    QMap<QString, qint64> m_nodeStartedMs;
    QSet<QString> m_tracedNodeIds;
    QMap<QString, QList<FunctionFlowValue>> m_nodeValues;
    QMap<QString, QList<FunctionFlowValue>> m_historyObservations;
    QVector<FunctionFlowNodeTrace> m_traces;
    QElapsedTimer m_runTimer;
    QString m_canonicalInput;
    bool m_collectedSelection = false;
    QString m_historyDetailPath;
    OperationError m_lastStartError;

    struct EditableRunState
    {
        ExecutionId runId;
        QString recordDirectory;
        QString detailPath;
        QString pendingEditedText;
        int openSurfaceCount = 0;
        bool finalized = false;
    };
    QMap<QString, EditableRunState> m_editableRuns;
};

#endif // VOCEKIT_FUNCTION_FLOW_EXECUTION_CONTROLLER_H
