#include <QtTest>

#include "../../src/tasks/diagnostic_task_runner.h"
#include "../../src/tasks/interface_self_check_task.h"
#include "../../src/tasks/network_diagnostics_task.h"

#include <atomic>
#include <chrono>
#include <thread>

class InterfaceSelfCheckTaskTests : public QObject
{
    Q_OBJECT

private slots:
    void skipsDeepSeekWhenKeyMissing()
    {
        InterfaceSelfCheckRequest request;
        request.target = QStringLiteral("deepseek");

        const QStringList lines = runInterfaceSelfCheckTask(request);

        QCOMPARE(lines.size(), 1);
        QVERIFY(lines.constFirst().contains(QStringLiteral("DeepSeek")));
        QVERIFY(lines.constFirst().contains(QString::fromUtf8("未填写")));
    }

    void skipsXfyunWhenKeyMissing()
    {
        InterfaceSelfCheckRequest request;
        request.target = QStringLiteral("xfyun");

        const QStringList lines = runInterfaceSelfCheckTask(request);

        QCOMPARE(lines.size(), 1);
        QVERIFY(lines.constFirst().contains(QString::fromUtf8("讯飞语音听写")));
        QVERIFY(lines.constFirst().contains(QString::fromUtf8("未填写")));
    }

    void skipsCustomModelsWhenNoProfilesExist()
    {
        InterfaceSelfCheckRequest request;
        request.target = QStringLiteral("custom:missing");

        const QStringList lines = runInterfaceSelfCheckTask(request);

        QCOMPARE(lines.size(), 1);
        QVERIFY(lines.constFirst().contains(QString::fromUtf8("自定义大模型")));
        QVERIFY(lines.constFirst().contains(QString::fromUtf8("未填写")));
    }

    void cancelsInterfaceCheckBeforeWork()
    {
        CancellationSource cancellation;
        cancellation.cancel();

        InterfaceSelfCheckRequest request;
        request.target = QStringLiteral("deepseek");
        request.cancellation = cancellation.token();

        QVERIFY(runInterfaceSelfCheckTask(request).isEmpty());
    }

    void cancelsNetworkDiagnosticsBeforeWork()
    {
        CancellationSource cancellation;
        cancellation.cancel();

        NetworkDiagnosticsRequest request;
        request.cancellation = cancellation.token();

        QVERIFY(runNetworkDiagnosticsTask(request).isEmpty());
    }

    void diagnosticRunnerCancelsPreviousTaskAndPublishesLatest()
    {
        DiagnosticTaskRunner runner;
        std::atomic<bool> firstStarted(false);
        std::atomic<bool> firstCancelled(false);
        QStringList published;
        runner.finishedCallback = [&published](const QStringList &lines) {
            published = lines;
        };

        runner.start([&firstStarted, &firstCancelled](
            const CancellationToken &cancellation) {
            firstStarted.store(true);
            while (!cancellation.isCancellationRequested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            firstCancelled.store(true);
            return QStringList() << QStringLiteral("old");
        });
        QTRY_VERIFY_WITH_TIMEOUT(firstStarted.load(), 1000);

        runner.start([](const CancellationToken &) {
            return QStringList() << QStringLiteral("new");
        });

        QTRY_COMPARE_WITH_TIMEOUT(
            published,
            QStringList() << QStringLiteral("new"),
            1000
        );
        QTRY_VERIFY_WITH_TIMEOUT(firstCancelled.load(), 1000);
        QVERIFY(!runner.isBusy());
    }

    void diagnosticRunnerCancelSuppressesCompletion()
    {
        DiagnosticTaskRunner runner;
        std::atomic<bool> started(false);
        std::atomic<bool> cancelled(false);
        int completionCount = 0;
        runner.finishedCallback = [&completionCount](const QStringList &) {
            ++completionCount;
        };

        runner.start([&started, &cancelled](
            const CancellationToken &cancellation) {
            started.store(true);
            while (!cancellation.isCancellationRequested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            cancelled.store(true);
            return QStringList() << QStringLiteral("cancelled");
        });
        QTRY_VERIFY_WITH_TIMEOUT(started.load(), 1000);

        runner.cancel();

        QTRY_VERIFY_WITH_TIMEOUT(cancelled.load(), 1000);
        QTest::qWait(50);
        QCOMPARE(completionCount, 0);
        QVERIFY(!runner.isBusy());
    }
};

QTEST_MAIN(InterfaceSelfCheckTaskTests)
#include "interface_self_check_task_tests.moc"
