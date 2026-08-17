#include <QtTest>

#include "../../src/tasks/diagnostic_task_runner.h"
#include "../../src/tasks/interface_self_check_task.h"
#include "../../src/tasks/network_diagnostics_task.h"
#include "../../src/ui/diagnostics_settings_snapshot.h"

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

    void probesWindowsSpeechWithoutSecretsOrNetwork()
    {
        InterfaceSelfCheckRequest request;
        request.target = QStringLiteral("windows_speech");
        request.windowsSpeechLanguage = QStringLiteral(" EN-us ");
        request.applicationDirPath = QStringLiteral("C:/deployed");
        int calls = 0;
        request.windowsSpeechProbe = [&calls](
            const QString &programPath,
            const QString &language,
            const CancellationToken &
        ) -> QStringList {
            ++calls;
            if (programPath != QStringLiteral(
                "C:/deployed/speech/windows/vocekit-windows-speech.exe"
            )) {
                return QStringList() << QStringLiteral("WRONG_PATH");
            }
            if (language != QStringLiteral("en-US")) {
                return QStringList() << QStringLiteral("WRONG_LANGUAGE");
            }
            return QStringList()
                << QStringLiteral("OK")
                << QStringLiteral("resolvedLanguage=en-US")
                << QStringLiteral("installedLanguages=zh-CN,en-US");
        };

        const QStringList lines = runInterfaceSelfCheckTask(request);
        QCOMPARE(calls, 1);
        QCOMPARE(lines.size(), 1);
        QVERIFY(lines.first().contains(QStringLiteral("Windows")));
        QVERIFY(lines.first().contains(QStringLiteral("en-US")));
    }

    void reportsStableMissingWindowsRecognizer()
    {
        InterfaceSelfCheckRequest request;
        request.target = QStringLiteral("windows_speech");
        request.windowsSpeechLanguage = QStringLiteral("zh-CN");
        request.windowsSpeechProbe = [](
            const QString &,
            const QString &language,
            const CancellationToken &
        ) -> QStringList {
            return QStringList()
                << QStringLiteral("RECOGNIZER_MISSING")
                << QStringLiteral("requestedLanguage=") + language;
        };

        const QStringList lines = runInterfaceSelfCheckTask(request);
        QCOMPARE(lines.size(), 1);
        QVERIFY(lines.first().contains(QStringLiteral("RECOGNIZER_MISSING")));
        QVERIFY(lines.first().contains(QStringLiteral("zh-CN")));
    }

    void diagnosticsSnapshotNormalizesWindowsSpeechLanguage()
    {
        AppSettingsData settings;
        settings.windowsSpeechLanguage = QStringLiteral(" EN-us ");
        const DiagnosticsSettingsSnapshot snapshot =
            buildDiagnosticsSettingsSnapshot(settings);
        QCOMPARE(snapshot.windowsSpeechLanguage, QStringLiteral("en-US"));

        settings.windowsSpeechLanguage = QStringLiteral("legacy-value");
        QCOMPARE(
            buildDiagnosticsSettingsSnapshot(settings).windowsSpeechLanguage,
            QStringLiteral("follow-windows")
        );
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
