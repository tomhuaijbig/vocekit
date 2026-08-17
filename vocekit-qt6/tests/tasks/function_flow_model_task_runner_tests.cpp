#include <QtTest>

#include "../../src/tasks/function_flow_model_task_runner.h"

#include <QAtomicInt>
#include <QCoreApplication>
#include <QSharedPointer>
#include <QThread>

ModelRequestTaskResult runModelProviderRequestTask(
    const ModelRequestTaskRequest &request,
    const ModelDeltaCallback &onDelta)
{
    Q_UNUSED(onDelta);
    ModelRequestTaskResult result;
    result.executionId = request.cancellation.executionId();
    result.errorMessage = QStringLiteral("test default runner");
    return result;
}

namespace {

ModelRequestTaskRequest requestFor(
    const CancellationSource &source)
{
    ModelRequestTaskRequest request;
    request.modelId = QStringLiteral("fake-model");
    request.systemPrompt = QStringLiteral("system");
    request.userPrompt = QStringLiteral("user");
    request.stream = true;
    request.cancellation = source.token();
    return request;
}

ModelRequestTaskResult successResult(
    const ModelRequestTaskRequest &request,
    const QString &text)
{
    ModelRequestTaskResult result;
    result.text = text;
    result.executionId = request.cancellation.executionId();
    return result;
}

} // namespace

class FunctionFlowModelTaskRunnerTests : public QObject
{
    Q_OBJECT

private slots:
    void completesOnOwnerThreadAndForwardsDelta();
    void cancellationSuppressesLateDeltaAndResult();
    void repeatedStartSupersedesOldTask();
    void destructionSuppressesLateCallbacks();
};

void FunctionFlowModelTaskRunnerTests::
completesOnOwnerThreadAndForwardsDelta()
{
    QThread *ownerThread = QThread::currentThread();
    QThread *workerThread = nullptr;
    QStringList deltas;
    QString finalText;

    FunctionFlowModelTaskRunnerAccess access;
    access.runTask = [&](
        const ModelRequestTaskRequest &request,
        const ModelDeltaCallback &onDelta) {
        workerThread = QThread::currentThread();
        onDelta(QStringLiteral("A"));
        onDelta(QStringLiteral("B"));
        return successResult(request, QStringLiteral("AB"));
    };
    FunctionFlowModelTaskRunner runner(access);
    runner.deltaCallback = [&](
        const ExecutionId &,
        const QString &nodeId,
        const QString &delta) {
        QCOMPARE(QThread::currentThread(), ownerThread);
        QCOMPARE(nodeId, QStringLiteral("model"));
        deltas.append(delta);
    };
    runner.finishedCallback = [&](
        const ExecutionId &,
        const QString &,
        const ModelRequestTaskResult &result) {
        QCOMPARE(QThread::currentThread(), ownerThread);
        finalText = result.text;
    };

    CancellationSource source;
    runner.start(
        source.executionId(),
        QStringLiteral("model"),
        requestFor(source)
    );

    QTRY_COMPARE_WITH_TIMEOUT(finalText, QStringLiteral("AB"), 3000);
    QCOMPARE(deltas.join(QString()), QStringLiteral("AB"));
    QVERIFY(workerThread);
    QVERIFY(workerThread != ownerThread);
    QVERIFY(!runner.isRunning());
}

void FunctionFlowModelTaskRunnerTests::
cancellationSuppressesLateDeltaAndResult()
{
    QAtomicInt entered(0);
    int deltaCount = 0;
    int finishedCount = 0;

    FunctionFlowModelTaskRunnerAccess access;
    access.runTask = [&](
        const ModelRequestTaskRequest &request,
        const ModelDeltaCallback &onDelta) {
        entered.storeRelease(1);
        QThread::msleep(80);
        onDelta(QStringLiteral("late"));
        return successResult(request, QStringLiteral("late"));
    };
    FunctionFlowModelTaskRunner runner(access);
    runner.deltaCallback = [&](
        const ExecutionId &,
        const QString &,
        const QString &) {
        ++deltaCount;
    };
    runner.finishedCallback = [&](
        const ExecutionId &,
        const QString &,
        const ModelRequestTaskResult &) {
        ++finishedCount;
    };

    CancellationSource source;
    runner.start(
        source.executionId(),
        QStringLiteral("model"),
        requestFor(source)
    );
    QTRY_COMPARE_WITH_TIMEOUT(entered.loadAcquire(), 1, 1000);
    source.cancel();
    runner.cancel();
    QTest::qWait(180);

    QCOMPARE(deltaCount, 0);
    QCOMPARE(finishedCount, 0);
}

void FunctionFlowModelTaskRunnerTests::
repeatedStartSupersedesOldTask()
{
    QStringList deltas;
    QStringList results;
    QAtomicInt oldEntered(0);

    FunctionFlowModelTaskRunnerAccess access;
    access.runTask = [&](
        const ModelRequestTaskRequest &request,
        const ModelDeltaCallback &onDelta) {
        if (request.userPrompt == QStringLiteral("old")) {
            oldEntered.storeRelease(1);
            QThread::msleep(100);
            onDelta(QStringLiteral("old"));
            return successResult(request, QStringLiteral("old"));
        }
        onDelta(QStringLiteral("new"));
        return successResult(request, QStringLiteral("new"));
    };
    FunctionFlowModelTaskRunner runner(access);
    runner.deltaCallback = [&](
        const ExecutionId &,
        const QString &,
        const QString &delta) {
        deltas.append(delta);
    };
    runner.finishedCallback = [&](
        const ExecutionId &,
        const QString &,
        const ModelRequestTaskResult &result) {
        results.append(result.text);
    };

    CancellationSource oldSource;
    ModelRequestTaskRequest oldRequest = requestFor(oldSource);
    oldRequest.userPrompt = QStringLiteral("old");
    runner.start(
        oldSource.executionId(),
        QStringLiteral("model"),
        oldRequest
    );
    QTRY_COMPARE_WITH_TIMEOUT(oldEntered.loadAcquire(), 1, 1000);

    CancellationSource newSource;
    ModelRequestTaskRequest newRequest = requestFor(newSource);
    newRequest.userPrompt = QStringLiteral("new");
    runner.start(
        newSource.executionId(),
        QStringLiteral("model"),
        newRequest
    );

    QTRY_COMPARE_WITH_TIMEOUT(results.size(), 1, 3000);
    QTest::qWait(160);
    QCOMPARE(results, QStringList() << QStringLiteral("new"));
    QCOMPARE(deltas, QStringList() << QStringLiteral("new"));
}

void FunctionFlowModelTaskRunnerTests::
destructionSuppressesLateCallbacks()
{
    int deltaCount = 0;
    int finishedCount = 0;
    QAtomicInt entered(0);

    FunctionFlowModelTaskRunnerAccess access;
    access.runTask = [&](
        const ModelRequestTaskRequest &request,
        const ModelDeltaCallback &onDelta) {
        entered.storeRelease(1);
        QThread::msleep(80);
        onDelta(QStringLiteral("late"));
        return successResult(request, QStringLiteral("late"));
    };
    auto *runner = new FunctionFlowModelTaskRunner(access);
    runner->deltaCallback = [&](
        const ExecutionId &,
        const QString &,
        const QString &) {
        ++deltaCount;
    };
    runner->finishedCallback = [&](
        const ExecutionId &,
        const QString &,
        const ModelRequestTaskResult &) {
        ++finishedCount;
    };

    CancellationSource source;
    runner->start(
        source.executionId(),
        QStringLiteral("model"),
        requestFor(source)
    );
    QTRY_COMPARE_WITH_TIMEOUT(entered.loadAcquire(), 1, 1000);
    delete runner;
    QTest::qWait(180);

    QCOMPARE(deltaCount, 0);
    QCOMPARE(finishedCount, 0);
}

QTEST_MAIN(FunctionFlowModelTaskRunnerTests)

#include "function_flow_model_task_runner_tests.moc"
