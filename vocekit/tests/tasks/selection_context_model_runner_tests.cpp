#include <QtTest>

#include "../../src/tasks/selection_context_model_runner.h"

#include <QAtomicInt>
#include <QElapsedTimer>
#include <QPointer>
#include <QThread>

ModelRequestTaskResult runModelProviderRequestTask(
    const ModelRequestTaskRequest &request,
    const ModelDeltaCallback &)
{
    ModelRequestTaskResult result;
    result.executionId = request.cancellation.executionId();
    result.errorMessage = QStringLiteral("default runner must not be used");
    return result;
}

namespace {

ModelRequestTaskRequest requestWithText(const QString &text)
{
    ModelRequestTaskRequest request;
    request.modelId = QStringLiteral("fake-model");
    request.userPrompt = text;
    request.stream = true;
    return request;
}

ModelRequestTaskResult successfulResult(
    const ModelRequestTaskRequest &request,
    const QString &text = QStringLiteral("done"))
{
    ModelRequestTaskResult result;
    result.executionId = request.cancellation.executionId();
    result.text = text;
    return result;
}

} // namespace

class SelectionContextModelRunnerTests : public QObject
{
    Q_OBJECT

private slots:
    void deltasArriveOnTheOwnerThreadInProviderOrder()
    {
        SelectionContextModelRunnerAccess access;
        access.runRequest = [](
            const ModelRequestTaskRequest &request,
            const ModelDeltaCallback &delta) {
            delta(QStringLiteral("A"));
            delta(QStringLiteral("B"));
            delta(QStringLiteral("C"));
            return successfulResult(request);
        };
        SelectionContextModelRunner runner(access);
        QStringList deltas;
        QList<QThread *> callbackThreads;
        bool finished = false;
        SelectionContextModelRunnerCallbacks callbacks;
        callbacks.delta = [&](const ExecutionId &, const QString &value) {
            deltas.append(value);
            callbackThreads.append(QThread::currentThread());
        };
        callbacks.finished = [&](
            const ExecutionId &,
            const ModelRequestTaskResult &) {
            finished = true;
            callbackThreads.append(QThread::currentThread());
        };

        runner.start(requestWithText(QStringLiteral("private")), callbacks);
        QTRY_VERIFY(finished);
        QCOMPARE(deltas, QStringList() << QStringLiteral("A")
                                      << QStringLiteral("B")
                                      << QStringLiteral("C"));
        for (QThread *thread : callbackThreads) {
            QCOMPARE(thread, runner.thread());
        }
    }

    void terminalCallbackRunsAfterAllQueuedDeltas()
    {
        SelectionContextModelRunnerAccess access;
        access.runRequest = [](
            const ModelRequestTaskRequest &request,
            const ModelDeltaCallback &delta) {
            delta(QStringLiteral("one"));
            delta(QStringLiteral("two"));
            return successfulResult(request);
        };
        SelectionContextModelRunner runner(access);
        QStringList order;
        SelectionContextModelRunnerCallbacks callbacks;
        callbacks.delta = [&](const ExecutionId &, const QString &value) {
            order.append(QStringLiteral("delta:") + value);
        };
        callbacks.finished = [&](
            const ExecutionId &,
            const ModelRequestTaskResult &) {
            order.append(QStringLiteral("finished"));
        };

        runner.start(requestWithText(QStringLiteral("text")), callbacks);
        QTRY_COMPARE(order.size(), 3);
        QCOMPARE(order.constLast(), QStringLiteral("finished"));
        QCOMPARE(order.at(0), QStringLiteral("delta:one"));
        QCOMPARE(order.at(1), QStringLiteral("delta:two"));
    }

    void cancellingDropsLateDeltaAndTerminalCallbacks()
    {
        QAtomicInt started(0);
        QAtomicInt release(0);
        SelectionContextModelRunnerAccess access;
        access.runRequest = [&](
            const ModelRequestTaskRequest &request,
            const ModelDeltaCallback &delta) {
            started.storeRelease(1);
            while (!release.loadAcquire()) {
                QThread::msleep(1);
            }
            delta(QStringLiteral("late"));
            return successfulResult(request);
        };
        SelectionContextModelRunner runner(access);
        int deltaCount = 0;
        int finishedCount = 0;
        SelectionContextModelRunnerCallbacks callbacks;
        callbacks.delta = [&](const ExecutionId &, const QString &) {
            ++deltaCount;
        };
        callbacks.finished = [&](
            const ExecutionId &,
            const ModelRequestTaskResult &) {
            ++finishedCount;
        };

        runner.start(requestWithText(QStringLiteral("text")), callbacks);
        QTRY_COMPARE(started.loadAcquire(), 1);
        runner.cancel();
        release.storeRelease(1);
        QTest::qWait(80);
        QCOMPARE(deltaCount, 0);
        QCOMPARE(finishedCount, 0);
        QVERIFY(!runner.isRunning());
    }

    void cancellingSetsTheExactTokenObservedByTheProvider()
    {
        QAtomicInt started(0);
        QAtomicInt sawCancelled(0);
        QString providerExecutionId;
        SelectionContextModelRunnerAccess access;
        access.runRequest = [&](
            const ModelRequestTaskRequest &request,
            const ModelDeltaCallback &) {
            providerExecutionId = request.cancellation.executionId().value;
            started.storeRelease(1);
            while (!request.cancellation.isCancellationRequested()) {
                QThread::msleep(1);
            }
            sawCancelled.storeRelease(1);
            return successfulResult(request);
        };
        SelectionContextModelRunner runner(access);
        const ExecutionId executionId = runner.start(
            requestWithText(QStringLiteral("text")),
            SelectionContextModelRunnerCallbacks()
        );
        QTRY_COMPARE(started.loadAcquire(), 1);
        runner.cancel();
        QTRY_COMPARE(sawCancelled.loadAcquire(), 1);
        QCOMPARE(providerExecutionId, executionId.value);
    }

    void startingAgainCancelsThePreviousExecutionExactlyOnce()
    {
        QAtomicInt callCount(0);
        QAtomicInt firstStarted(0);
        QAtomicInt firstCancellationObserved(0);
        SelectionContextModelRunnerAccess access;
        access.runRequest = [&](
            const ModelRequestTaskRequest &request,
            const ModelDeltaCallback &) {
            const int call = callCount.fetchAndAddOrdered(1);
            if (call == 0) {
                firstStarted.storeRelease(1);
                while (!request.cancellation.isCancellationRequested()) {
                    QThread::msleep(1);
                }
                firstCancellationObserved.ref();
            }
            return successfulResult(request, QString::number(call));
        };
        SelectionContextModelRunner runner(access);
        QList<ExecutionId> finishedIds;
        SelectionContextModelRunnerCallbacks callbacks;
        callbacks.finished = [&finishedIds](
            const ExecutionId &id,
            const ModelRequestTaskResult &) {
            finishedIds.append(id);
        };

        const ExecutionId first = runner.start(
            requestWithText(QStringLiteral("first")),
            callbacks
        );
        QTRY_COMPARE(firstStarted.loadAcquire(), 1);
        const ExecutionId second = runner.start(
            requestWithText(QStringLiteral("second")),
            callbacks
        );
        QTRY_COMPARE(firstCancellationObserved.loadAcquire(), 1);
        QTRY_COMPARE(finishedIds.size(), 1);
        QCOMPARE(finishedIds.constFirst(), second);
        QVERIFY(first != second);
        QCOMPARE(firstCancellationObserved.loadAcquire(), 1);
    }

    void providerErrorPreservesTheExistingErrorMessageWithoutSelectedTextInLogs()
    {
        const QString selectedText = QStringLiteral("secret-selection-body");
        SelectionContextModelRunnerAccess access;
        access.runRequest = [](
            const ModelRequestTaskRequest &request,
            const ModelDeltaCallback &) {
            ModelRequestTaskResult result = successfulResult(request);
            result.errorMessage = QStringLiteral("provider original error");
            return result;
        };
        SelectionContextModelRunner runner(access);
        ModelRequestTaskResult delivered;
        bool finished = false;
        SelectionContextModelRunnerCallbacks callbacks;
        callbacks.finished = [&](
            const ExecutionId &,
            const ModelRequestTaskResult &result) {
            delivered = result;
            finished = true;
        };

        runner.start(requestWithText(selectedText), callbacks);
        QTRY_VERIFY(finished);
        QCOMPARE(
            delivered.errorMessage,
            QStringLiteral("provider original error")
        );
        QVERIFY(!delivered.errorMessage.contains(selectedText));
    }

    void callbackMayDeleteRunnerSynchronously()
    {
        {
            SelectionContextModelRunnerAccess access;
            access.runRequest = [](
                const ModelRequestTaskRequest &request,
                const ModelDeltaCallback &delta) {
                delta(QStringLiteral("delete-now"));
                return successfulResult(request);
            };
            SelectionContextModelRunner *runner =
                new SelectionContextModelRunner(access);
            QPointer<SelectionContextModelRunner> guard(runner);
            SelectionContextModelRunnerCallbacks callbacks;
            callbacks.delta = [&runner](
                const ExecutionId &,
                const QString &) {
                SelectionContextModelRunner *doomed = runner;
                runner = nullptr;
                delete doomed;
            };

            runner->start(requestWithText(QStringLiteral("text")), callbacks);
            QTRY_VERIFY(guard.isNull());
            QCOMPARE(
                runner,
                static_cast<SelectionContextModelRunner *>(nullptr)
            );
        }

        {
            SelectionContextModelRunnerAccess access;
            access.runRequest = [](
                const ModelRequestTaskRequest &request,
                const ModelDeltaCallback &) {
                return successfulResult(request);
            };
            SelectionContextModelRunner *runner =
                new SelectionContextModelRunner(access);
            QPointer<SelectionContextModelRunner> guard(runner);
            SelectionContextModelRunnerCallbacks callbacks;
            callbacks.finished = [&runner](
                const ExecutionId &,
                const ModelRequestTaskResult &) {
                SelectionContextModelRunner *doomed = runner;
                runner = nullptr;
                delete doomed;
            };

            runner->start(requestWithText(QStringLiteral("text")), callbacks);
            QTRY_VERIFY(guard.isNull());
            QCOMPARE(
                runner,
                static_cast<SelectionContextModelRunner *>(nullptr)
            );
        }
    }

    void destructionCancelsAndDrainsWithoutAResidualWorker()
    {
        QAtomicInt started(0);
        QAtomicInt exited(0);
        SelectionContextModelRunnerAccess access;
        access.runRequest = [&](
            const ModelRequestTaskRequest &request,
            const ModelDeltaCallback &) {
            started.storeRelease(1);
            while (!request.cancellation.isCancellationRequested()) {
                QThread::msleep(1);
            }
            exited.storeRelease(1);
            return successfulResult(request);
        };
        SelectionContextModelRunner *runner =
            new SelectionContextModelRunner(access);
        runner->start(
            requestWithText(QStringLiteral("text")),
            SelectionContextModelRunnerCallbacks()
        );
        QTRY_COMPARE(started.loadAcquire(), 1);
        QElapsedTimer elapsed;
        elapsed.start();
        delete runner;
        QCOMPARE(exited.loadAcquire(), 1);
        QVERIFY(elapsed.elapsed() < 1000);
    }
};

QTEST_MAIN(SelectionContextModelRunnerTests)
#include "selection_context_model_runner_tests.moc"
