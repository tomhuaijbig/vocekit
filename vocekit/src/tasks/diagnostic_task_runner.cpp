#include "diagnostic_task_runner.h"

#include <QFutureWatcher>
#include <QtConcurrent>

DiagnosticTaskRunner::DiagnosticTaskRunner(QObject *parent)
    : QObject(parent)
{
}

DiagnosticTaskRunner::~DiagnosticTaskRunner()
{
    cancel();
}

bool DiagnosticTaskRunner::isBusy() const
{
    return m_busy;
}

void DiagnosticTaskRunner::start(const Task &task)
{
    cancel();
    if (!task) {
        return;
    }

    m_busy = true;
    m_cancellation = CancellationSource();
    const CancellationToken cancellation = m_cancellation.token();
    const quint64 generation = ++m_generation;

    QFutureWatcher<QStringList> *watcher =
        new QFutureWatcher<QStringList>(this);
    m_watcher = watcher;
    connect(
        watcher,
        &QFutureWatcher<QStringList>::finished,
        this,
        [this, watcher, generation, cancellation]() {
            const QStringList lines = watcher->result();
            watcher->deleteLater();
            if (m_watcher == watcher) {
                m_watcher = nullptr;
            }
            if (generation != m_generation
                || cancellation.isCancellationRequested()) {
                return;
            }

            m_busy = false;
            if (finishedCallback) {
                finishedCallback(lines);
            }
        }
    );
    watcher->setFuture(QtConcurrent::run([task, cancellation]() {
        if (cancellation.isCancellationRequested()) {
            return QStringList();
        }
        return task(cancellation);
    }));
}

void DiagnosticTaskRunner::cancel()
{
    m_cancellation.cancel();
    ++m_generation;
    m_busy = false;
    m_watcher = nullptr;
}
