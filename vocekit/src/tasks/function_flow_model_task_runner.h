#ifndef VOCEKIT_FUNCTION_FLOW_MODEL_TASK_RUNNER_H
#define VOCEKIT_FUNCTION_FLOW_MODEL_TASK_RUNNER_H

#include "model_request_task.h"

#include <QAtomicInt>
#include <QObject>
#include <QSharedPointer>

#include <functional>

template <typename T>
class QFutureWatcher;

struct FunctionFlowModelTaskRunnerAccess
{
    std::function<ModelRequestTaskResult(
        const ModelRequestTaskRequest &,
        const ModelDeltaCallback &
    )> runTask;
};

// 流程模型任务在后台线程执行，并把 delta 和最终结果排队送回所属控制器线程。
class FunctionFlowModelTaskRunner : public QObject
{
public:
    explicit FunctionFlowModelTaskRunner(
        const FunctionFlowModelTaskRunnerAccess &access =
            FunctionFlowModelTaskRunnerAccess(),
        QObject *parent = nullptr
    );
    ~FunctionFlowModelTaskRunner() override;

    void start(
        const ExecutionId &runId,
        const QString &nodeId,
        const ModelRequestTaskRequest &request
    );
    void cancel();
    bool isRunning() const;

    std::function<void(
        const ExecutionId &,
        const QString &,
        const QString &
    )> deltaCallback;
    std::function<void(
        const ExecutionId &,
        const QString &,
        const ModelRequestTaskResult &
    )> finishedCallback;

private:
    bool isCurrent(
        quint64 generation,
        const ExecutionId &runId,
        const QString &nodeId,
        const CancellationToken &cancellation
    ) const;
    void deliverFinishedWhenReady(
        quint64 generation,
        const ExecutionId &runId,
        const QString &nodeId,
        const CancellationToken &cancellation,
        const QSharedPointer<QAtomicInt> &pendingDeltas,
        const ModelRequestTaskResult &result
    );

    FunctionFlowModelTaskRunnerAccess m_access;
    quint64 m_generation = 0;
    bool m_running = false;
    bool m_destroying = false;
    ExecutionId m_runId;
    QString m_nodeId;
    CancellationToken m_cancellation;
    QFutureWatcher<ModelRequestTaskResult> *m_watcher = nullptr;
};

#endif // VOCEKIT_FUNCTION_FLOW_MODEL_TASK_RUNNER_H
