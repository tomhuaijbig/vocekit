#ifndef VOCEKIT_SELECTION_CONTEXT_MODEL_RUNNER_H
#define VOCEKIT_SELECTION_CONTEXT_MODEL_RUNNER_H

#include "model_request_task.h"

#include <QAtomicInt>
#include <QObject>
#include <QSet>
#include <QSharedPointer>

#include <functional>

template <typename T>
class QFutureWatcher;

struct SelectionContextModelRunnerCallbacks
{
    std::function<void(
        const ExecutionId &executionId,
        const QString &delta
    )> delta;
    std::function<void(
        const ExecutionId &executionId,
        const ModelRequestTaskResult &result
    )> finished;
};

struct SelectionContextModelRunnerAccess
{
    std::function<ModelRequestTaskResult(
        const ModelRequestTaskRequest &,
        const ModelDeltaCallback &
    )> runRequest;
};

class SelectionContextModelRunner : public QObject
{
    Q_OBJECT

public:
    explicit SelectionContextModelRunner(
        const SelectionContextModelRunnerAccess &access =
            SelectionContextModelRunnerAccess(),
        QObject *parent = nullptr
    );
    ~SelectionContextModelRunner() override;

    ExecutionId start(
        const ModelRequestTaskRequest &request,
        const SelectionContextModelRunnerCallbacks &callbacks
    );
    void cancel();
    bool isRunning() const;

private:
    bool isCurrent(
        quint64 generation,
        const ExecutionId &executionId,
        const CancellationToken &cancellation
    ) const;
    void deliverFinishedWhenReady(
        quint64 generation,
        const ExecutionId &executionId,
        const CancellationToken &cancellation,
        const QSharedPointer<QAtomicInt> &pendingDeltas,
        const ModelRequestTaskResult &result
    );

    SelectionContextModelRunnerAccess m_access;
    SelectionContextModelRunnerCallbacks m_callbacks;
    QSharedPointer<CancellationSource> m_cancellationSource;
    QSet<QFutureWatcher<ModelRequestTaskResult> *> m_watchers;
    quint64 m_generation = 0;
    bool m_running = false;
    bool m_destroying = false;
    ExecutionId m_executionId;
};

#endif // VOCEKIT_SELECTION_CONTEXT_MODEL_RUNNER_H
