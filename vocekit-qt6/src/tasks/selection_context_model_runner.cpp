#include "selection_context_model_runner.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QPointer>
#include <QTimer>
#include <QtConcurrent>

namespace {

ModelRequestTaskResult runDefaultRequest(
    const ModelRequestTaskRequest &request,
    const ModelDeltaCallback &delta)
{
    return runModelProviderRequestTask(request, delta);
}

} // namespace

SelectionContextModelRunner::SelectionContextModelRunner(
    const SelectionContextModelRunnerAccess &access,
    QObject *parent)
    : QObject(parent),
      m_access(access)
{
    if (!m_access.runRequest) {
        m_access.runRequest = runDefaultRequest;
    }
}

SelectionContextModelRunner::~SelectionContextModelRunner()
{
    m_destroying = true;
    if (m_cancellationSource) {
        m_cancellationSource->cancel();
    }
    ++m_generation;
    m_running = false;
    m_callbacks = SelectionContextModelRunnerCallbacks();
    const QSet<QFutureWatcher<ModelRequestTaskResult> *> watchers =
        m_watchers;
    for (QFutureWatcher<ModelRequestTaskResult> *watcher : watchers) {
        if (watcher) {
            watcher->future().waitForFinished();
        }
    }
    m_watchers.clear();
    m_cancellationSource.clear();
}

ExecutionId SelectionContextModelRunner::start(
    const ModelRequestTaskRequest &request,
    const SelectionContextModelRunnerCallbacks &callbacks)
{
    cancel();
    if (!m_access.runRequest || m_destroying) {
        return ExecutionId();
    }

    m_cancellationSource.reset(new CancellationSource);
    const CancellationToken cancellation = m_cancellationSource->token();
    const ExecutionId executionId = cancellation.executionId();
    ModelRequestTaskRequest privateRequest = request;
    privateRequest.cancellation = cancellation;

    m_running = true;
    m_executionId = executionId;
    m_callbacks = callbacks;
    const quint64 generation = ++m_generation;
    const SelectionContextModelRunnerAccess access = m_access;
    const QPointer<SelectionContextModelRunner> guard(this);
    const QSharedPointer<QAtomicInt> pendingDeltas(new QAtomicInt(0));

    QFutureWatcher<ModelRequestTaskResult> *watcher =
        new QFutureWatcher<ModelRequestTaskResult>(this);
    m_watchers.insert(watcher);
    connect(
        watcher,
        &QFutureWatcher<ModelRequestTaskResult>::finished,
        this,
        [this,
         watcher,
         generation,
         executionId,
         cancellation,
         pendingDeltas]() {
            m_watchers.remove(watcher);
            const ModelRequestTaskResult result = watcher->result();
            watcher->deleteLater();
            deliverFinishedWhenReady(
                generation,
                executionId,
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
         executionId,
         privateRequest,
         cancellation,
         pendingDeltas]() {
            const ModelDeltaCallback onDelta =
                [guard,
                 generation,
                 executionId,
                 cancellation,
                 pendingDeltas](const QString &delta) {
                    if (!guard
                        || delta.isEmpty()
                        || cancellation.isCancellationRequested()) {
                        return;
                    }
                    pendingDeltas->ref();
                    QTimer::singleShot(
                        0,
                        guard.data(),
                        [guard,
                         generation,
                         executionId,
                         cancellation,
                         pendingDeltas,
                         delta]() {
                            pendingDeltas->deref();
                            if (!guard
                                || !guard->isCurrent(
                                    generation,
                                    executionId,
                                    cancellation)) {
                                return;
                            }
                            const std::function<void(
                                const ExecutionId &,
                                const QString &)> callback =
                                    guard->m_callbacks.delta;
                            if (callback) {
                                callback(executionId, delta);
                            }
                        }
                    );
                };
            ModelRequestTaskResult result =
                access.runRequest(privateRequest, onDelta);
            result.executionId = executionId;
            return result;
        }
    ));
    return executionId;
}

void SelectionContextModelRunner::cancel()
{
    if (m_cancellationSource) {
        m_cancellationSource->cancel();
    }
    ++m_generation;
    m_running = false;
    m_executionId = ExecutionId();
    m_callbacks = SelectionContextModelRunnerCallbacks();
    m_cancellationSource.clear();
}

bool SelectionContextModelRunner::isRunning() const
{
    return m_running;
}

bool SelectionContextModelRunner::isCurrent(
    quint64 generation,
    const ExecutionId &executionId,
    const CancellationToken &cancellation) const
{
    return !m_destroying
        && m_running
        && generation == m_generation
        && executionId == m_executionId
        && cancellation.isValid()
        && !cancellation.isCancellationRequested()
        && cancellation.executionId() == executionId
        && m_cancellationSource
        && m_cancellationSource->executionId() == executionId;
}

void SelectionContextModelRunner::deliverFinishedWhenReady(
    quint64 generation,
    const ExecutionId &executionId,
    const CancellationToken &cancellation,
    const QSharedPointer<QAtomicInt> &pendingDeltas,
    const ModelRequestTaskResult &result)
{
    if (!isCurrent(generation, executionId, cancellation)) {
        return;
    }
    if (pendingDeltas && pendingDeltas->loadAcquire() > 0) {
        QTimer::singleShot(
            0,
            this,
            [this,
             generation,
             executionId,
             cancellation,
             pendingDeltas,
             result]() {
                deliverFinishedWhenReady(
                    generation,
                    executionId,
                    cancellation,
                    pendingDeltas,
                    result
                );
            }
        );
        return;
    }

    const std::function<void(
        const ExecutionId &,
        const ModelRequestTaskResult &)> callback = m_callbacks.finished;
    m_running = false;
    m_executionId = ExecutionId();
    m_callbacks = SelectionContextModelRunnerCallbacks();
    m_cancellationSource.clear();
    if (callback) {
        const QPointer<SelectionContextModelRunner> guard(this);
        callback(executionId, result);
        if (!guard) {
            return;
        }
    }
}
