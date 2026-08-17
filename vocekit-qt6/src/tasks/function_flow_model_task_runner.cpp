#include "function_flow_model_task_runner.h"

#include <QFutureWatcher>
#include <QPointer>
#include <QTimer>
#include <QtConcurrent>

namespace {

ModelRequestTaskResult runDefaultModelTask(
    const ModelRequestTaskRequest &request,
    const ModelDeltaCallback &onDelta)
{
    return runModelProviderRequestTask(request, onDelta);
}

} // namespace

FunctionFlowModelTaskRunner::FunctionFlowModelTaskRunner(
    const FunctionFlowModelTaskRunnerAccess &access,
    QObject *parent)
    : QObject(parent),
      m_access(access)
{
    if (!m_access.runTask) {
        m_access.runTask = runDefaultModelTask;
    }
}

FunctionFlowModelTaskRunner::~FunctionFlowModelTaskRunner()
{
    m_destroying = true;
    cancel();
}

void FunctionFlowModelTaskRunner::start(
    const ExecutionId &runId,
    const QString &nodeId,
    const ModelRequestTaskRequest &request)
{
    cancel();
    if (!m_access.runTask
        || !runId.isValid()
        || nodeId.trimmed().isEmpty()
        || !request.cancellation.isValid()
        || request.cancellation.executionId() != runId) {
        return;
    }

    m_running = true;
    m_runId = runId;
    m_nodeId = nodeId;
    m_cancellation = request.cancellation;
    const quint64 generation = ++m_generation;
    const CancellationToken cancellation = request.cancellation;
    const FunctionFlowModelTaskRunnerAccess access = m_access;
    QPointer<FunctionFlowModelTaskRunner> guard(this);
    const QSharedPointer<QAtomicInt> pendingDeltas(
        new QAtomicInt(0)
    );

    QFutureWatcher<ModelRequestTaskResult> *watcher =
        new QFutureWatcher<ModelRequestTaskResult>(this);
    m_watcher = watcher;
    connect(
        watcher,
        &QFutureWatcher<ModelRequestTaskResult>::finished,
        this,
        [this,
         watcher,
         generation,
         runId,
         nodeId,
         cancellation,
         pendingDeltas]() {
            watcher->deleteLater();
            if (m_watcher == watcher) {
                m_watcher = nullptr;
            }
            if (!isCurrent(
                    generation,
                    runId,
                    nodeId,
                    cancellation)) {
                return;
            }
            const ModelRequestTaskResult result = watcher->result();
            deliverFinishedWhenReady(
                generation,
                runId,
                nodeId,
                cancellation,
                pendingDeltas,
                result
            );
        }
    );

    watcher->setFuture(QtConcurrent::run(
        [access,
         guard,
         generation,
         runId,
         nodeId,
         request,
         cancellation,
         pendingDeltas]() {
            const ModelDeltaCallback onDelta =
                [guard,
                 generation,
                 runId,
                 nodeId,
                 cancellation,
                 pendingDeltas](const QString &delta) {
                    if (!guard
                        || delta.isEmpty()
                        || cancellation
                            .isCancellationRequested()) {
                        return;
                    }
                    pendingDeltas->ref();
                    QTimer::singleShot(
                        0,
                        guard.data(),
                        [guard,
                         generation,
                         runId,
                         nodeId,
                         cancellation,
                         pendingDeltas,
                         delta]() {
                            pendingDeltas->deref();
                            if (!guard
                                || !guard->isCurrent(
                                    generation,
                                    runId,
                                    nodeId,
                                    cancellation)) {
                                return;
                            }
                            if (guard->deltaCallback) {
                                guard->deltaCallback(
                                    runId,
                                    nodeId,
                                    delta
                                );
                            }
                        }
                    );
                };
            return access.runTask(request, onDelta);
        }
    ));
}

void FunctionFlowModelTaskRunner::cancel()
{
    ++m_generation;
    m_running = false;
    m_runId = ExecutionId();
    m_nodeId.clear();
    m_cancellation = CancellationToken();
    if (m_watcher) {
        m_watcher->cancel();
        m_watcher = nullptr;
    }
}

bool FunctionFlowModelTaskRunner::isRunning() const
{
    return m_running;
}

bool FunctionFlowModelTaskRunner::isCurrent(
    quint64 generation,
    const ExecutionId &runId,
    const QString &nodeId,
    const CancellationToken &cancellation) const
{
    return !m_destroying
        && m_running
        && generation == m_generation
        && runId == m_runId
        && nodeId == m_nodeId
        && cancellation.isValid()
        && !cancellation.isCancellationRequested()
        && cancellation.executionId() == runId
        && m_cancellation.executionId() == runId;
}

void FunctionFlowModelTaskRunner::deliverFinishedWhenReady(
    quint64 generation,
    const ExecutionId &runId,
    const QString &nodeId,
    const CancellationToken &cancellation,
    const QSharedPointer<QAtomicInt> &pendingDeltas,
    const ModelRequestTaskResult &result)
{
    if (!isCurrent(
            generation,
            runId,
            nodeId,
            cancellation)
        || result.executionId != runId) {
        return;
    }
    if (pendingDeltas && pendingDeltas->loadAcquire() > 0) {
        QTimer::singleShot(
            0,
            this,
            [this,
             generation,
             runId,
             nodeId,
             cancellation,
             pendingDeltas,
             result]() {
                deliverFinishedWhenReady(
                    generation,
                    runId,
                    nodeId,
                    cancellation,
                    pendingDeltas,
                    result
                );
            }
        );
        return;
    }
    m_running = false;
    if (finishedCallback) {
        finishedCallback(runId, nodeId, result);
    }
}
